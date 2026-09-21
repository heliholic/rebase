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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "common/utils.h"

#include "pg/mixer.h"


/** Mixer constants **/

// Output 0 is the null output a rule writes to when it is disabled, so that
// no rule needs a branch to discard its result.
#define MIXER_OUTPUT_COUNT    (1 + MAX_SUPPORTED_SERVOS + MAX_SUPPORTED_MOTORS)

#define MIXER_SERVO_OFFSET    1
#define MIXER_MOTOR_OFFSET    (MIXER_SERVO_OFFSET + MAX_SUPPORTED_SERVOS)

#define MIXER_RATE_MIN       -10000
#define MIXER_RATE_MAX        10000

#define MIXER_WEIGHT_MIN     -10000
#define MIXER_WEIGHT_MAX      10000

#define MIXER_INPUT_MIN      -2500
#define MIXER_INPUT_MAX       2500

#define MIXER_OVERRIDE_MIN   -2500
#define MIXER_OVERRIDE_MAX    2500
#define MIXER_OVERRIDE_OFF    (MIXER_OVERRIDE_MAX + 1)
#define MIXER_OVERRIDE_PASSTHROUGH  (MIXER_OVERRIDE_MAX + 2)

// How many PID loops an input stays marked saturated after it was clipped.
#define MIXER_SATURATION_TIME 5

// Inputs are tracked per output and outputs per servo, both in a bitmap_t.
STATIC_ASSERT(MIXER_INPUT_COUNT <= 32, mixer_input_count_exceeds_bitmap);
STATIC_ASSERT(MIXER_OUTPUT_COUNT <= 32, mixer_output_count_exceeds_bitmap);


/** Interface functions **/

void mixerInit(void);
void mixerInitConfig(void);
void mixerInitMapping(void);

void validateAndFixMixerConfig(void);

void mixerUpdate(void);

float mixerGetInput(uint8_t index);
float mixerGetOutput(uint8_t index);

bool mixerSaturated(uint8_t index);
void mixerSaturateOutput(uint8_t index);

int16_t mixerGetOverride(uint8_t index);
int16_t mixerSetOverride(uint8_t index, int16_t value);
bool    isMixerOverrideActive(void);

bool mixerIsCyclicServo(uint8_t index);


/** Inline functions **/

// An override holding a value, rather than off or passthrough
static inline bool mixerOverrideIsValue(int value)
{
    return (value >= MIXER_OVERRIDE_MIN && value <= MIXER_OVERRIDE_MAX);
}

static inline float mixerGetServoOutput(uint8_t index)
{
    return mixerGetOutput(MIXER_SERVO_OFFSET + index);
}

static inline float mixerGetMotorOutput(uint8_t index)
{
    return mixerGetOutput(MIXER_MOTOR_OFFSET + index);
}

static inline void mixerSaturateServoOutput(uint8_t index)
{
    mixerSaturateOutput(MIXER_SERVO_OFFSET + index);
}

static inline float mixerGetThrottle(void)
{
    return mixerGetInput(MIXER_IN_COMMAND_THROTTLE);
}

static inline int mixerRotationSign(void)
{
    return (mixerConfig()->main_rotor_dir == DIR_CW) ? -1 : 1;
}

static inline bool mixerMotorizedTail(void)
{
    return (mixerConfig()->tail_rotor_mode == TAIL_MODE_MOTOR);
}
