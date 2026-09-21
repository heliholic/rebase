/*
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "platform.h"

#include "build/build_config.h"

#include "common/axis.h"
#include "common/maths.h"
#include "common/trig.h"
#include "common/types.h"
#include "common/utils.h"

#include "controls/rc.h"

#include "fc/runtime_config.h"

#include "flight/pid.h"

#include "pg/mixer.h"
#include "pg/rx.h"

#include "rx/rx.h"

#include "mixer.h"


/** Internal data **/

typedef struct {

    float           input[MIXER_INPUT_COUNT];
    float           output[MIXER_OUTPUT_COUNT];

    bitmap_t        mapping[MIXER_OUTPUT_COUNT];
    int16_t         override[MIXER_INPUT_COUNT];
    uint16_t        saturation[MIXER_INPUT_COUNT];

    float           tailCenterTrim;
    float           tailMotorIdle;

    float           swashTrim[3];

    float           collGeoCorrection;

    float           totalPitchLimit;
    float           cyclicRingLimit;

    sincosf_t       cyclicPhase;

} mixerData_t;

static FAST_DATA_ZERO_INIT mixerData_t mixer;

// An output driven by any of these tilts the swash
#define MIXER_CYCLIC_INPUTS  (BIT(MIXER_IN_STABILIZED_ROLL) | BIT(MIXER_IN_STABILIZED_PITCH) | \
                              BIT(MIXER_IN_COMMAND_ROLL) | BIT(MIXER_IN_COMMAND_PITCH))

// Every receiver channel has a CHANNEL_* input
STATIC_ASSERT(MIXER_IN_CHANNEL_18 - MIXER_IN_CHANNEL_1 + 1 == MAX_SUPPORTED_RC_CHANNEL_COUNT,
              mixer_rc_channel_inputs_mismatch);


/** Interface functions **/

float mixerGetInput(uint8_t index)
{
    return mixer.input[index];
}

float mixerGetOutput(uint8_t index)
{
    return mixer.output[index];
}

bool mixerSaturated(uint8_t index)
{
    return (mixer.saturation[index] > 0);
}

static inline void mixerSaturateInput(uint8_t index)
{
    mixer.saturation[index] = MIXER_SATURATION_TIME;
}

/*
 * An output which could not reach the value asked of it saturates every input
 * that reaches it, so that a term which cannot move the machine any further
 * can be told apart from one which simply asked for nothing.
 */
void mixerSaturateOutput(uint8_t index)
{
    for (int i = 1; i < MIXER_INPUT_COUNT; i++) {
        if (mixer.mapping[index] & BIT(i)) {
            mixerSaturateInput(i);
        }
    }
}

int16_t mixerGetOverride(uint8_t index)
{
    return mixer.override[index];
}

int16_t mixerSetOverride(uint8_t index, int16_t value)
{
    return mixer.override[index] = value;
}

bool isMixerOverrideActive(void)
{
    for (int i = 1; i < MIXER_INPUT_COUNT; i++) {
        const int16_t ovr = mixer.override[i];
        if (mixerOverrideIsValue(ovr) || ovr == MIXER_OVERRIDE_PASSTHROUGH)
            return true;
    }
    return false;
}

bool mixerIsCyclicServo(uint8_t index)
{
    return (mixer.mapping[MIXER_SERVO_OFFSET + index] & MIXER_CYCLIC_INPUTS);
}


/** Internal functions **/

static void mixerApplyInputLimit(int index, float value)
{
    const mixerInput_t *in = mixerInputs(index);

    // Input limits
    const float in_min = in->min / 1000.0f;
    const float in_max = in->max / 1000.0f;

    // Constrain and saturate
    if (value > in_max) {
        mixer.input[index] = in_max;
        mixerSaturateInput(index);
    }
    else if (value < in_min) {
        mixer.input[index] = in_min;
        mixerSaturateInput(index);
    }
    else {
        mixer.input[index] = value;
    }
}

/*
 * Passthrough override: a stabilized axis follows the stick directly, so that
 * the endpoints of a swash can be set up without the flight controller in the
 * way. Any other input keeps the value it was given.
 */
static float mixerGetPassthroughInput(const int index, const float original_value)
{
    float rc;

    switch (index) {
    case MIXER_IN_STABILIZED_ROLL:
        rc = getRcDeflection(FD_ROLL);
        break;
    case MIXER_IN_STABILIZED_PITCH:
        rc = getRcDeflection(FD_PITCH);
        break;
    case MIXER_IN_STABILIZED_YAW:
        // Normally, yaw command is reversed in the setpoint (unlike other axes).
        // As we passthrough RC commands we want to keep the same reversal.
        rc = -getRcDeflection(FD_YAW);
        break;
    case MIXER_IN_STABILIZED_COLLECTIVE:
        rc = getRcDeflection(FD_COLL);
        break;
    default:
        return original_value;
    }

    // Scale rc by 120% for easier observing endpoints.
    rc *= 1.2f;

    if (rc > 0) {
        return scaleRangef(rc, 0, 1.0f, 0, mixerInputs(index)->max / 1000.0f);
    }
    return scaleRangef(rc, 0, -1.0f, 0, mixerInputs(index)->min / 1000.0f);
}

static void mixerSetInput(int index, float value)
{
    // An override replaces the value, but only while disarmed
    if (!ARMING_FLAG(ARMED)) {
        if (mixerOverrideIsValue(mixer.override[index])) {
            value = mixer.override[index] / 1000.0f;
        }
        else if (mixer.override[index] == MIXER_OVERRIDE_PASSTHROUGH) {
            value = mixerGetPassthroughInput(index, value);
        }
    }

    mixerApplyInputLimit(index, value);
}

static void mixerUpdateCyclic(void)
{
    float SR = mixer.input[MIXER_IN_STABILIZED_ROLL];
    float SP = mixer.input[MIXER_IN_STABILIZED_PITCH];

    // Limit action active
    if (mixer.cyclicRingLimit > 0 || mixer.totalPitchLimit > 0) {
        float factor = 1.0f;

        // The blade pitch collective has not already spent
        const float cyclic_limit = fmaxf(mixer.totalPitchLimit -
            fabsf(mixer.input[MIXER_IN_STABILIZED_COLLECTIVE]), 0);

        // Apply cyclic ring limit
        if (mixer.cyclicRingLimit > 0) {
            // Individual limits on SP and SR
            const mixerInput_t *in_roll = mixerInputs(MIXER_IN_STABILIZED_ROLL);
            const mixerInput_t *in_pitch = mixerInputs(MIXER_IN_STABILIZED_PITCH);

            // Assume min<0 and max>0
            const float max_roll = MAX(abs((SR < 0) ? in_roll->min : in_roll->max), 10) / 1000.0f;
            const float max_pitch = MAX(abs((SP < 0) ? in_pitch->min : in_pitch->max), 10) / 1000.0f;

            // Stretch the values to a unit circle limit
            const float SSR = SR / (max_roll * mixer.cyclicRingLimit);
            const float SSP = SP / (max_pitch * mixer.cyclicRingLimit);

            // Stretched cyclic deflection
            const float cyclic = sqrtf(sq(SSR) + sq(SSP));

            // Cyclic limits reached - scale back
            if (cyclic > 1.0f) {
                factor = 1.0f / cyclic;
            }
        }

        // Apply dynamic cyclic limit
        if (mixer.totalPitchLimit > 0) {
            // Total cyclic after ring limit
            const float cyclic = sqrtf(sq(SR) + sq(SP)) * factor;

            // Cyclic limits reached - scale back
            if (cyclic > cyclic_limit) {
                factor *= cyclic_limit / cyclic;
            }
        }

        // Apply limit factor
        if (factor < 1.0f) {
            SP *= factor;
            SR *= factor;
            mixerSaturateInput(MIXER_IN_STABILIZED_ROLL);
            mixerSaturateInput(MIXER_IN_STABILIZED_PITCH);
        }
    }

    // Swash phasing
    if (mixer.cyclicPhase.sin != 0) {
        const float P = SP;
        const float R = SR;
        SP = P * mixer.cyclicPhase.cos - R * mixer.cyclicPhase.sin;
        SR = P * mixer.cyclicPhase.sin + R * mixer.cyclicPhase.cos;
    }

    // Apply new values
    mixer.input[MIXER_IN_STABILIZED_ROLL]  = SR;
    mixer.input[MIXER_IN_STABILIZED_PITCH] = SP;
}

static float mixerCollectiveCorrection(float SC)
{
    if (mixer.collGeoCorrection != 0) {
        if (SC > 0)
            SC += SC * mixer.collGeoCorrection;
        else
            SC -= SC * mixer.collGeoCorrection;
    }

    return SC;
}

static float mixerCollectiveScale(float SC, float SR, float SP)
{
    float beta;

    if (SC > 0) {
        beta = mixerConfig()->collective_tilt_correction_pos / 100.0f;
    } else {
        beta = -mixerConfig()->collective_tilt_correction_neg / 100.0f;
    }

    const float scale = constrainf(1 + beta * (SR * SR + SP * SP), 0.0f, 2.0f);

    return SC * scale;
}

static void mixerUpdateMotorizedTail(void)
{
    // Motorized tail control
    if (mixerMotorizedTail()) {
        // Yaw input value - positive is against torque
        const float yaw = mixer.input[MIXER_IN_STABILIZED_YAW] * mixerRotationSign();

        // Add center trim
        float throttle = yaw + mixer.tailCenterTrim;

        // Apply minimum throttle
        throttle = fmaxf(throttle, mixer.tailMotorIdle);

        // Hold the tail motor until the main rotor is asked for throttle.
        // The governor relaxes this once the rotor is spooled up.
        if (mixer.input[MIXER_IN_STABILIZED_THROTTLE] < 0.001f)
            throttle = 0;

        // Yaw is now tail motor throttle
        mixer.input[MIXER_IN_STABILIZED_YAW] = throttle;
    }
}

#define inputValue(NAME)                (mixer.input[MIXER_IN_STABILIZED_##NAME] * mixerInputs(MIXER_IN_STABILIZED_##NAME)->rate / 1000.0f)
#define setServoOutput(SERVO,VAL)       (mixer.output[MIXER_SERVO_OFFSET + (SERVO)] = (VAL))
#define setMotorOutput(MOTOR,VAL)       (mixer.output[MIXER_MOTOR_OFFSET + (MOTOR)] = (VAL))

static void mixerUpdateSwash(void)
{
    if (mixerConfig()->swash_type) {
        float SR = inputValue(ROLL);
        float SP = inputValue(PITCH);
        float SY = inputValue(YAW);
        float SC = inputValue(COLLECTIVE);
        float ST = inputValue(THROTTLE);

        const float TC = mixer.tailCenterTrim;

        SC = mixerCollectiveCorrection(SC);
        SC = mixerCollectiveScale(SC, SR, SP);

        SR += mixer.swashTrim[0];
        SP += mixer.swashTrim[1];
        SC += mixer.swashTrim[2];

        switch (mixerConfig()->swash_type) {
            case SWASH_TYPE_120:
                setServoOutput(0, 0.5f * SC - SP);
                setServoOutput(1, 0.5f * SC + 0.86602540f * SR + 0.5f * SP);
                setServoOutput(2, 0.5f * SC - 0.86602540f * SR + 0.5f * SP);
                break;

            case SWASH_TYPE_135:
                setServoOutput(0, 0.5f * SC - SP);
                setServoOutput(1, 0.5f * SC + SR + SP);
                setServoOutput(2, 0.5f * SC - SR + SP);
                break;

            case SWASH_TYPE_140:
                setServoOutput(0, 0.5f * SC - SP);
                setServoOutput(1, 0.5f * SC + 0.86602540f * SR + SP);
                setServoOutput(2, 0.5f * SC - 0.86602540f * SR + SP);
                break;

            case SWASH_TYPE_90L:
                setServoOutput(0, SP);
                setServoOutput(1, SR);
                break;

            case SWASH_TYPE_90V:
                setServoOutput(0,  0.70710678f * SR + 0.70710678f * SP);
                setServoOutput(1, -0.70710678f * SR + 0.70710678f * SP);
                break;

            case SWASH_TYPE_THRU:
                setServoOutput(0, SP);
                setServoOutput(1, SR);
                setServoOutput(2, SC);
                break;
        }

        setMotorOutput(0, ST);

        if (mixerMotorizedTail())
            setMotorOutput(1, SY);
        else
            setServoOutput(3, SY + TC);
    }
}

static void mixerUpdateRules(void)
{
    for (int i = 0; i < MIXER_RULE_COUNT; i++) {
        const mixerRule_t *rule = mixerRules(i);

        if (rule->oper) {
            const uint8_t src = rule->input;
            const uint8_t dst = rule->output;
            const float   val = mixer.input[src] * mixerInputs(src)->rate / 1000.0f;
            const float   out = (rule->offset + rule->weight * val) / 1000.0f;

            switch (rule->oper) {
                case MIXER_OP_SET:
                    mixer.output[dst] = out;
                    break;
                case MIXER_OP_ADD:
                    mixer.output[dst] += out;
                    break;
                case MIXER_OP_MUL:
                    mixer.output[dst] *= out;
                    break;
            }
        }
    }
}

static void mixerUpdateInputs(void)
{
    // Flight Dynamics
    mixerSetInput(MIXER_IN_COMMAND_ROLL, getRcDeflection(FD_ROLL));
    mixerSetInput(MIXER_IN_COMMAND_PITCH, getRcDeflection(FD_PITCH));
    mixerSetInput(MIXER_IN_COMMAND_YAW, getRcDeflection(FD_YAW));
    mixerSetInput(MIXER_IN_COMMAND_COLLECTIVE, getRcDeflection(FD_COLL));

    // Throttle input
    mixerSetInput(MIXER_IN_COMMAND_THROTTLE, getThrottle());

    // Receiver channels, by channel number
    const float center = getFailsafeCenter();
    for (int i = MIXER_IN_CHANNEL_1; i <= MIXER_IN_CHANNEL_18; i++) {
        mixerSetInput(i, (rcInput[i - MIXER_IN_CHANNEL_1] - center) / 500);
    }

    // Stabilised inputs
    mixerSetInput(MIXER_IN_STABILIZED_ROLL, pidGetOutput(PID_ROLL));
    mixerSetInput(MIXER_IN_STABILIZED_PITCH, pidGetOutput(PID_PITCH));
    mixerSetInput(MIXER_IN_STABILIZED_YAW, pidGetOutput(PID_YAW));
    mixerSetInput(MIXER_IN_STABILIZED_COLLECTIVE, pidGetCollective());

    // Calculate cyclic
    mixerUpdateCyclic();

    // Main rotor throttle. The governor takes this input over once it exists.
    mixerSetInput(MIXER_IN_STABILIZED_THROTTLE, getThrottle());

    // Update motorized tail (must be done after the rotor throttle)
    mixerUpdateMotorizedTail();
}

void mixerUpdate(void)
{
    // Reset saturation
    for (int i = 0; i < MIXER_INPUT_COUNT; i++) {
        if (mixer.saturation[i])
            mixer.saturation[i]--;
    }

    // Reset mixer outputs
    for (int i = 0; i < MIXER_OUTPUT_COUNT; i++) {
        mixer.output[i] = 0;
    }

    // Fetch input values
    mixerUpdateInputs();

    // Evaluate hard-coded mixer
    mixerUpdateSwash();

    // Evaluate rule-based mixer
    mixerUpdateRules();
}

static bool mixerRuleValid(const mixerRule_t *rule)
{
    return rule->oper > MIXER_OP_NUL && rule->oper < MIXER_OP_COUNT &&
           rule->input < MIXER_INPUT_COUNT &&
           rule->output < MIXER_OUTPUT_COUNT &&
           rule->offset >= MIXER_INPUT_MIN && rule->offset <= MIXER_INPUT_MAX &&
           rule->weight >= MIXER_WEIGHT_MIN && rule->weight <= MIXER_WEIGHT_MAX;
}

void INIT_CODE validateAndFixMixerConfig(void)
{
    // A rule with any field out of range is disabled, not clamped. Clamping
    // would turn a corrupt record into a live rule on some other output.
    for (int i = 0; i < MIXER_RULE_COUNT; i++) {
        if (!mixerRuleValid(mixerRules(i))) {
            memset(mixerRulesMutable(i), 0, sizeof(mixerRule_t));
        }
    }

    // An unknown tail mode falls back to a servo-driven tail
    if (mixerConfig()->tail_rotor_mode >= TAIL_MODE_COUNT) {
        mixerConfigMutable()->tail_rotor_mode = TAIL_MODE_SERVO;
    }

    // A total pitch limit below the collective range alone would leave no
    // cyclic at full collective, so keep at least 6 degrees of headroom.
    if (mixerConfig()->swash_pitch_limit > 0) {
        const uint16_t min_cyclic = 500;
        uint16_t limit = mixerConfig()->swash_pitch_limit;
        limit = MAX(limit, ABS(mixerInputs(MIXER_IN_STABILIZED_COLLECTIVE)->max) + min_cyclic);
        limit = MAX(limit, ABS(mixerInputs(MIXER_IN_STABILIZED_COLLECTIVE)->min) + min_cyclic);
        mixerConfigMutable()->swash_pitch_limit = limit;
    }
}

void INIT_CODE mixerInitConfig(void)
{
    if (mixerConfig()->swash_pitch_limit)
        mixer.totalPitchLimit = mixerConfig()->swash_pitch_limit / 1000.0f;
    else
        mixer.totalPitchLimit = 0;

    if (mixerConfig()->swash_ring)
        mixer.cyclicRingLimit = 1.4142135623f - mixerConfig()->swash_ring * 0.004142135623f;
    else
        mixer.cyclicRingLimit = 0;

    if (mixerConfig()->swash_phase) {
        const float angle = DEGREES_TO_RADIANS(mixerConfig()->swash_phase / 10.0f);
        mixer.cyclicPhase = sinfcosf(angle);
    }
    else {
        mixer.cyclicPhase.sin = 0.0f;
        mixer.cyclicPhase.cos = 1.0f;
    }

    for (int i = 0; i < 3; i++)
        mixer.swashTrim[i] = mixerConfig()->swash_trim[i] / 1000.0f;

    mixer.collGeoCorrection = mixerConfig()->swash_geo_correction / 1000.0f;

    mixer.tailMotorIdle = mixerConfig()->tail_motor_idle / 1000.0f;
    mixer.tailCenterTrim = mixerConfig()->tail_center_trim / 1000.0f;
}

#define addServoMapping(INDEX,SERVO)    (mixer.mapping[MIXER_SERVO_OFFSET + (SERVO)] |= BIT(INDEX))
#define addMotorMapping(INDEX,MOTOR)    (mixer.mapping[MIXER_MOTOR_OFFSET + (MOTOR)] |= BIT(INDEX))

/*
 * Which inputs reach each output, for saturation and the cyclic servo test.
 * The rules are live configuration, so anything that edits them calls this
 * again to keep the mapping in step with what mixerUpdateRules() evaluates.
 */
void mixerInitMapping(void)
{
    // Every mapping below is accumulated into what is already there
    memset(mixer.mapping, 0, sizeof(mixer.mapping));

    if (mixerConfig()->swash_type) {
        switch (mixerConfig()->swash_type) {
            case SWASH_TYPE_120:
            case SWASH_TYPE_135:
            case SWASH_TYPE_140:
                addServoMapping(MIXER_IN_STABILIZED_COLLECTIVE, 0);
                addServoMapping(MIXER_IN_STABILIZED_COLLECTIVE, 1);
                addServoMapping(MIXER_IN_STABILIZED_COLLECTIVE, 2);
                addServoMapping(MIXER_IN_STABILIZED_PITCH, 0);
                addServoMapping(MIXER_IN_STABILIZED_PITCH, 1);
                addServoMapping(MIXER_IN_STABILIZED_PITCH, 2);
                addServoMapping(MIXER_IN_STABILIZED_ROLL, 1);
                addServoMapping(MIXER_IN_STABILIZED_ROLL, 2);
                break;

            case SWASH_TYPE_90L:
                addServoMapping(MIXER_IN_STABILIZED_PITCH, 0);
                addServoMapping(MIXER_IN_STABILIZED_ROLL, 1);
                break;

            case SWASH_TYPE_90V:
                addServoMapping(MIXER_IN_STABILIZED_PITCH, 0);
                addServoMapping(MIXER_IN_STABILIZED_PITCH, 1);
                addServoMapping(MIXER_IN_STABILIZED_ROLL, 0);
                addServoMapping(MIXER_IN_STABILIZED_ROLL, 1);
                break;

            case SWASH_TYPE_THRU:
                addServoMapping(MIXER_IN_STABILIZED_PITCH, 0);
                addServoMapping(MIXER_IN_STABILIZED_ROLL, 1);
                addServoMapping(MIXER_IN_STABILIZED_COLLECTIVE, 2);
                break;
        }

        addMotorMapping(MIXER_IN_STABILIZED_THROTTLE, 0);

        if (mixerMotorizedTail())
            addMotorMapping(MIXER_IN_STABILIZED_YAW, 1);
        else
            addServoMapping(MIXER_IN_STABILIZED_YAW, 3);
    }

    for (int i = 0; i < MIXER_RULE_COUNT; i++) {
        const mixerRule_t *rule = mixerRules(i);

        switch (rule->oper) {
            case MIXER_OP_SET:
                mixer.mapping[rule->output] = BIT(rule->input);
                break;
            case MIXER_OP_ADD:
            case MIXER_OP_MUL:
                mixer.mapping[rule->output] |= BIT(rule->input);
                break;
        }
    }
}

void INIT_CODE mixerInit(void)
{
    // The whole runtime starts from nothing rather than from a previous config
    memset(&mixer, 0, sizeof(mixer));

    for (int i = 1; i < MIXER_INPUT_COUNT; i++) {
        mixer.override[i] = MIXER_OVERRIDE_OFF;
    }

    mixerInitMapping();
    mixerInitConfig();
}
