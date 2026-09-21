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

#include <cmath>
#include <cstring>

extern "C" {
    #include "platform.h"

    #include "common/axis.h"
    #include "common/maths.h"

    #include "fc/runtime_config.h"

    #include "flight/mixer.h"

    #include "pg/mixer.h"
    #include "pg/rx.h"

    #include "rx/rx.h"

    float rcInput[MAX_SUPPORTED_RC_CHANNEL_COUNT];

    uint8_t armingFlags = 0;

    // What the PID would be asking of the swash, set per test.
    float mockPidOutput[3];
    float mockPidCollective;
    float mockRcDeflection[4];
    float mockThrottle;

    float pidGetOutput(int axis) { return mockPidOutput[axis]; }
    float pidGetCollective(void) { return mockPidCollective; }
    float getRcDeflection(int axis) { return mockRcDeflection[axis]; }
    float getThrottle(void) { return mockThrottle; }
    uint16_t getFailsafeCenter(void) { return 1500; }
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

// The mixer works in units of full servo travel; a milli-unit is plenty.
#define MIXER_EPS 1e-4f

// The default rate of the stabilized inputs, as a fraction
#define STAB_RATE 1.0f

class MixerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        PG_RESET(mixerConfig);
        PG_RESET(mixerInputs);
        PG_RESET(mixerRules);

        armingFlags = 0;

        for (int i = 0; i < 3; i++)
            mockPidOutput[i] = 0;
        mockPidCollective = 0;
        mockThrottle = 0;

        for (int i = 0; i < 4; i++)
            mockRcDeflection[i] = 0;

        for (int i = 0; i < MAX_SUPPORTED_RC_CHANNEL_COUNT; i++)
            rcInput[i] = 1500;
    }

    // mixerInit() reads the configuration, so every test sets the config it
    // wants first and then starts the mixer.
    void start()
    {
        mixerInit();
    }

    float servo(int index)
    {
        return mixerGetServoOutput(index);
    }

    float motor(int index)
    {
        return mixerGetMotorOutput(index);
    }
};


// A machine which is asked for nothing must hold every output at zero, both
// before and after the first mixer pass. A swash that jumps on boot is the
// failure this rules out.
TEST_F(MixerTest, NeutralStartup)
{
    start();

    for (int i = 0; i < MIXER_OUTPUT_COUNT; i++)
        EXPECT_FLOAT_EQ(0, mixerGetOutput(i));

    mixerUpdate();

    for (int i = 0; i < MIXER_OUTPUT_COUNT; i++)
        EXPECT_FLOAT_EQ(0, mixerGetOutput(i));
}

// Collective alone lifts all three swash servos together, at half the
// commanded value, and leaves no cyclic behind.
TEST_F(MixerTest, Swash120Collective)
{
    start();

    mockPidCollective = 1.0f;
    mixerUpdate();

    // 1.0 of collective is STAB_RATE of travel, and the swash takes half.
    EXPECT_NEAR(0.5f * STAB_RATE, servo(0), MIXER_EPS);
    EXPECT_NEAR(0.5f * STAB_RATE, servo(1), MIXER_EPS);
    EXPECT_NEAR(0.5f * STAB_RATE, servo(2), MIXER_EPS);
}

// Pitch tilts the swash fore and aft: the front servo against the two rear
// ones, at half their throw on a 120 head.
TEST_F(MixerTest, Swash120Pitch)
{
    start();

    mockPidOutput[FD_PITCH] = 1.0f;
    mixerUpdate();

    EXPECT_NEAR(-1.0f * STAB_RATE, servo(0), MIXER_EPS);
    EXPECT_NEAR( 0.5f * STAB_RATE, servo(1), MIXER_EPS);
    EXPECT_NEAR( 0.5f * STAB_RATE, servo(2), MIXER_EPS);
}

// Roll tilts it left and right: the front servo stays, the rear pair moves
// in opposition by sqrt(3)/2 of the throw.
TEST_F(MixerTest, Swash120Roll)
{
    start();

    mockPidOutput[FD_ROLL] = 1.0f;
    mixerUpdate();

    EXPECT_NEAR(0, servo(0), MIXER_EPS);
    EXPECT_NEAR( 0.86602540f * STAB_RATE, servo(1), MIXER_EPS);
    EXPECT_NEAR(-0.86602540f * STAB_RATE, servo(2), MIXER_EPS);
}

// A 90 degree linear head puts pitch and roll on their own servos and has no
// collective servo of its own.
TEST_F(MixerTest, Swash90Linear)
{
    mixerConfigMutable()->swash_type = SWASH_TYPE_90L;
    start();

    mockPidOutput[FD_PITCH] = 1.0f;
    mockPidOutput[FD_ROLL] = 0.5f;
    mixerUpdate();

    EXPECT_NEAR(1.0f * STAB_RATE, servo(0), MIXER_EPS);
    EXPECT_NEAR(0.5f * STAB_RATE, servo(1), MIXER_EPS);
}

// Swash phasing rotates the cyclic plane. At 90 degrees, pitch comes out as
// roll and roll as negative pitch.
TEST_F(MixerTest, SwashPhasing)
{
    mixerConfigMutable()->swash_type = SWASH_TYPE_THRU;
    mixerConfigMutable()->swash_phase = 900;  // decidegrees
    start();

    mockPidOutput[FD_PITCH] = 1.0f;
    mixerUpdate();

    // Passthrough head: servo 0 is pitch, servo 1 is roll.
    EXPECT_NEAR(0, servo(0), MIXER_EPS);
    EXPECT_NEAR(1.0f * STAB_RATE, servo(1), MIXER_EPS);
}

// swash_phase is stored in decidegrees, so half a degree of phase has to
// reach the mixer as half a degree rather than being truncated to zero.
TEST_F(MixerTest, SwashPhasingKeepsDecidegrees)
{
    mixerConfigMutable()->swash_type = SWASH_TYPE_THRU;
    mixerConfigMutable()->swash_phase = 5;  // 0.5 degrees
    start();

    mockPidOutput[FD_ROLL] = 1.0f;
    mixerUpdate();

    EXPECT_NEAR(-STAB_RATE * sinf(0.5f * M_PIf / 180.0f), servo(0), MIXER_EPS);
}

// The swash ring holds the cyclic inside a circle: full pitch and full roll
// together may not reach further than either one alone.
TEST_F(MixerTest, CyclicRingLimit)
{
    mixerConfigMutable()->swash_type = SWASH_TYPE_THRU;
    mixerConfigMutable()->swash_ring = 100;
    start();

    mockPidOutput[FD_ROLL] = 5.0f;   // beyond the input limit either way
    mockPidOutput[FD_PITCH] = 5.0f;
    mixerUpdate();

    const float pitch = servo(0);
    const float roll = servo(1);
    const float total = sqrtf(pitch * pitch + roll * roll);

    // The input limit alone would allow 1.25 on each axis, so 1.7678 of
    // total travel. The ring pulls the pair back onto the circle.
    EXPECT_NEAR(1.25f * STAB_RATE, total, 1e-3f);
    EXPECT_NEAR(pitch, roll, MIXER_EPS);
}

// The total pitch limit is shared: collective takes its share first and the
// cyclic gets whatever blade pitch is left.
TEST_F(MixerTest, TotalPitchLimit)
{
    mixerConfigMutable()->swash_type = SWASH_TYPE_THRU;
    mixerConfigMutable()->swash_ring = 0;
    mixerConfigMutable()->swash_pitch_limit = 1000;
    validateAndFixMixerConfig();
    start();

    // Validation raised the limit to the collective range plus 6 degrees.
    ASSERT_EQ(1750, mixerConfig()->swash_pitch_limit);

    mockPidCollective = 2.0f;        // clipped to the 1.25 input limit
    mockPidOutput[FD_PITCH] = 5.0f;  // asks for more than is left
    mixerUpdate();

    // 1.75 of total pitch, 1.25 of it spent on collective, so 0.5 of cyclic
    // remains. Both come out scaled by the stabilized rate.
    EXPECT_NEAR(1.25f * STAB_RATE, servo(2), MIXER_EPS);
    EXPECT_NEAR(0.5f * STAB_RATE, servo(0), 1e-3f);
}

// A rule multiplies its input by the weight, adds the offset, and SET, ADD
// and MUL differ only in what they do with the result.
TEST_F(MixerTest, RuleOperations)
{
    mixerConfigMutable()->swash_type = SWASH_TYPE_NONE;
    mixerRulesMutable(0)->oper = MIXER_OP_SET;
    mixerRulesMutable(0)->input = MIXER_IN_STABILIZED_YAW;
    mixerRulesMutable(0)->output = MIXER_SERVO_OFFSET + 4;
    mixerRulesMutable(0)->weight = 1000;
    mixerRulesMutable(0)->offset = 100;
    mixerRulesMutable(1)->oper = MIXER_OP_ADD;
    mixerRulesMutable(1)->input = MIXER_IN_STABILIZED_YAW;
    mixerRulesMutable(1)->output = MIXER_SERVO_OFFSET + 4;
    mixerRulesMutable(1)->weight = 500;
    mixerRulesMutable(1)->offset = 0;
    mixerRulesMutable(2)->oper = MIXER_OP_MUL;
    mixerRulesMutable(2)->input = MIXER_IN_NONE;
    mixerRulesMutable(2)->output = MIXER_SERVO_OFFSET + 4;
    mixerRulesMutable(2)->weight = 0;
    mixerRulesMutable(2)->offset = 2000;
    start();

    mockPidOutput[FD_YAW] = 1.0f;
    mixerUpdate();

    // SET: (100 + 1000 * rate) / 1000, ADD: 500 * rate / 1000, MUL: by 2.
    EXPECT_NEAR((0.1f + 1.0f * STAB_RATE + 0.5f * STAB_RATE) * 2.0f, servo(4), MIXER_EPS);
}

// RC channel inputs are receiver channels by number, whatever the channel map
// does with them. The control functions are the COMMAND_* inputs.
TEST_F(MixerTest, RcChannelInputsByNumber)
{
    start();

    rcInput[0] = 2000;
    rcInput[5] = 1250;
    rcInput[17] = 1000;
    mixerUpdate();

    EXPECT_NEAR(1.0f, mixerGetInput(MIXER_IN_CHANNEL_1), MIXER_EPS);
    EXPECT_NEAR(0, mixerGetInput(MIXER_IN_CHANNEL_2), MIXER_EPS);
    EXPECT_NEAR(-0.5f, mixerGetInput(MIXER_IN_CHANNEL_6), MIXER_EPS);
    EXPECT_NEAR(-1.0f, mixerGetInput(MIXER_IN_CHANNEL_18), MIXER_EPS);
}

// A rule naming the null output must not disturb anything, which is what
// makes output zero safe as the destination of a disabled rule.
TEST_F(MixerTest, NullOutputRuleIsInert)
{
    mixerRulesMutable(0)->oper = MIXER_OP_SET;
    mixerRulesMutable(0)->input = MIXER_IN_STABILIZED_COLLECTIVE;
    mixerRulesMutable(0)->output = 0;
    mixerRulesMutable(0)->weight = 10000;
    mixerRulesMutable(0)->offset = 2500;
    start();

    mockPidCollective = 1.0f;
    mixerUpdate();

    EXPECT_NEAR(0.5f * STAB_RATE, servo(0), MIXER_EPS);
    EXPECT_NEAR(0.5f * STAB_RATE, servo(1), MIXER_EPS);
    EXPECT_NEAR(0.5f * STAB_RATE, servo(2), MIXER_EPS);
}

// An input which asks for more than its configured limit is clipped and left
// marked saturated, so the term that could not act can be told from one that
// asked for nothing.
TEST_F(MixerTest, InputLimitAndSaturation)
{
    start();

    mockPidCollective = 10.0f;
    mixerUpdate();

    // The default collective limit is 1250, scaled by the stabilized rate.
    EXPECT_NEAR(1.25f * STAB_RATE * 0.5f, servo(0), MIXER_EPS);
    EXPECT_TRUE(mixerSaturated(MIXER_IN_STABILIZED_COLLECTIVE));
    EXPECT_FALSE(mixerSaturated(MIXER_IN_STABILIZED_YAW));
}

// A saturated output reports back through the inputs which reach it, which
// is how the servo endpoints reach the axes that drove them.
TEST_F(MixerTest, OutputSaturationReachesItsInputs)
{
    start();
    mixerUpdate();

    EXPECT_FALSE(mixerSaturated(MIXER_IN_STABILIZED_ROLL));

    // Servo 1 is driven by collective, pitch and roll on a 120 head.
    mixerSaturateServoOutput(1);

    EXPECT_TRUE(mixerSaturated(MIXER_IN_STABILIZED_ROLL));
    EXPECT_TRUE(mixerSaturated(MIXER_IN_STABILIZED_PITCH));
    EXPECT_TRUE(mixerSaturated(MIXER_IN_STABILIZED_COLLECTIVE));
    EXPECT_FALSE(mixerSaturated(MIXER_IN_STABILIZED_YAW));

    // Servo 0 has no roll in it.
    mixerUpdate();
    EXPECT_TRUE(mixerSaturated(MIXER_IN_STABILIZED_ROLL));
}

// An override replaces an input while disarmed, and is ignored the moment
// the machine is armed.
TEST_F(MixerTest, OverrideOnlyWhileDisarmed)
{
    start();

    mixerSetOverride(MIXER_IN_STABILIZED_COLLECTIVE, 1000);
    EXPECT_TRUE(isMixerOverrideActive());

    mixerUpdate();
    EXPECT_NEAR(0.5f * STAB_RATE, servo(0), MIXER_EPS);

    ENABLE_ARMING_FLAG(ARMED);
    mixerUpdate();
    EXPECT_NEAR(0, servo(0), MIXER_EPS);

    DISABLE_ARMING_FLAG(ARMED);
    mixerUpdate();
    EXPECT_NEAR(0.5f * STAB_RATE, servo(0), MIXER_EPS);

    mixerSetOverride(MIXER_IN_STABILIZED_COLLECTIVE, MIXER_OVERRIDE_OFF);
    EXPECT_FALSE(isMixerOverrideActive());
}

// A passthrough override follows the stick rather than the flight
// controller, scaled by 120% so the endpoints can be reached.
TEST_F(MixerTest, OverridePassthrough)
{
    start();

    mixerSetOverride(MIXER_IN_STABILIZED_COLLECTIVE, MIXER_OVERRIDE_PASSTHROUGH);
    EXPECT_TRUE(isMixerOverrideActive());

    mockPidCollective = 1.0f;   // must not be what comes out
    mockRcDeflection[FD_COLL] = 0.5f;
    mixerUpdate();

    // 0.5 of stick, 120% of it, mapped onto the 1250 input maximum.
    EXPECT_NEAR(0.5f * 1.2f * 1.25f * STAB_RATE * 0.5f, servo(0), MIXER_EPS);
}

// The yaw servo follows the tail centre trim, and only a variable-pitch tail
// has one at all.
TEST_F(MixerTest, ServoTail)
{
    mixerConfigMutable()->tail_center_trim = 100;
    start();

    mockPidOutput[FD_YAW] = 1.0f;
    mixerUpdate();

    EXPECT_NEAR(STAB_RATE + 0.1f, servo(3), MIXER_EPS);
}

// A motorized tail turns yaw into throttle on the second motor, against the
// main rotor torque, so the sign follows the rotor direction.
TEST_F(MixerTest, MotorizedTailFollowsRotorDirection)
{
    mixerConfigMutable()->tail_rotor_mode = TAIL_MODE_MOTOR;
    mixerConfigMutable()->main_rotor_dir = DIR_CW;
    start();

    mockThrottle = 0.5f;
    mockPidOutput[FD_YAW] = -1.0f;
    mixerUpdate();

    EXPECT_NEAR(1.0f * STAB_RATE, motor(1), MIXER_EPS);
    // The yaw servo is not driven when the tail is a motor.
    EXPECT_NEAR(0, servo(3), MIXER_EPS);

    mixerConfigMutable()->main_rotor_dir = DIR_CCW;
    start();

    mixerUpdate();
    EXPECT_NEAR(0, motor(1), MIXER_EPS);  // idle, the demand is now negative
}

// A motorized tail must not spin while the main rotor is being asked for
// nothing, whatever the tail idle is set to.
TEST_F(MixerTest, MotorizedTailStopsWithTheRotor)
{
    mixerConfigMutable()->tail_rotor_mode = TAIL_MODE_MOTOR;
    mixerConfigMutable()->tail_motor_idle = 100;
    start();

    mockThrottle = 0;
    mixerUpdate();
    EXPECT_NEAR(0, motor(1), MIXER_EPS);

    mockThrottle = 0.5f;
    mixerUpdate();
    EXPECT_NEAR(0.1f * STAB_RATE, motor(1), MIXER_EPS);
}

// Only the servos the swash actually tilts count as cyclic, because those
// are the ones whose speed limit has to be applied together.
TEST_F(MixerTest, CyclicServoMapping)
{
    start();

    EXPECT_TRUE(mixerIsCyclicServo(0));
    EXPECT_TRUE(mixerIsCyclicServo(1));
    EXPECT_TRUE(mixerIsCyclicServo(2));
    EXPECT_FALSE(mixerIsCyclicServo(3));  // yaw
}

// A SET rule replaces what drove its output, so a swash servo taken over by
// a non-cyclic input stops counting as cyclic, and one taken over by a
// cyclic input starts.
TEST_F(MixerTest, SetRuleReplacesCyclicMapping)
{
    mixerRulesMutable(0)->oper = MIXER_OP_SET;
    mixerRulesMutable(0)->input = MIXER_IN_CHANNEL_6;
    mixerRulesMutable(0)->output = MIXER_SERVO_OFFSET + 0;
    mixerRulesMutable(0)->weight = 1000;
    mixerRulesMutable(1)->oper = MIXER_OP_SET;
    mixerRulesMutable(1)->input = MIXER_IN_STABILIZED_PITCH;
    mixerRulesMutable(1)->output = MIXER_SERVO_OFFSET + 3;
    mixerRulesMutable(1)->weight = 1000;
    start();

    EXPECT_FALSE(mixerIsCyclicServo(0));
    EXPECT_TRUE(mixerIsCyclicServo(1));
    EXPECT_TRUE(mixerIsCyclicServo(3));
}

// The rules are live configuration. Rebuilding the mapping after an edit
// brings saturation and the cyclic servo test in step with them.
TEST_F(MixerTest, MappingFollowsRuleEdits)
{
    start();
    EXPECT_FALSE(mixerIsCyclicServo(4));

    mixerRulesMutable(0)->oper = MIXER_OP_ADD;
    mixerRulesMutable(0)->input = MIXER_IN_STABILIZED_ROLL;
    mixerRulesMutable(0)->output = MIXER_SERVO_OFFSET + 4;
    mixerRulesMutable(0)->weight = 1000;
    mixerInitMapping();

    EXPECT_TRUE(mixerIsCyclicServo(4));
    mixerSaturateServoOutput(4);
    EXPECT_TRUE(mixerSaturated(MIXER_IN_STABILIZED_ROLL));

    PG_RESET(mixerRules);
    mixerInitMapping();

    EXPECT_FALSE(mixerIsCyclicServo(4));
    EXPECT_TRUE(mixerIsCyclicServo(0));  // the swash mapping is rebuilt too
}

// A stored rule with any field outside its range is disabled, not clamped:
// clamping would turn a corrupt record into a live rule on another output.
TEST_F(MixerTest, MalformedRulesAreDisabled)
{
    const mixerRule_t good = { MIXER_OP_ADD, MIXER_IN_STABILIZED_YAW, MIXER_SERVO_OFFSET + 4, 0, 1000 };

    for (int i = 0; i < 6; i++)
        *mixerRulesMutable(i) = good;

    mixerRulesMutable(0)->oper = MIXER_OP_COUNT;
    mixerRulesMutable(1)->input = MIXER_INPUT_COUNT;
    mixerRulesMutable(2)->output = MIXER_OUTPUT_COUNT;
    mixerRulesMutable(3)->weight = MIXER_WEIGHT_MAX + 1;
    mixerRulesMutable(4)->offset = MIXER_INPUT_MIN - 1;

    validateAndFixMixerConfig();

    const mixerRule_t null = { 0, 0, 0, 0, 0 };
    for (int i = 0; i < 5; i++)
        EXPECT_EQ(0, memcmp(mixerRules(i), &null, sizeof(null))) << "rule " << i;

    EXPECT_EQ(0, memcmp(mixerRules(5), &good, sizeof(good)));

    start();
    mockPidOutput[FD_YAW] = 1.0f;
    mixerUpdate();

    // Only the one valid rule reaches the servo: 1000 * rate / 1000.
    EXPECT_NEAR(STAB_RATE, servo(4), MIXER_EPS);
    for (int i = 0; i < MIXER_OUTPUT_COUNT; i++)
        EXPECT_TRUE(std::isfinite(mixerGetOutput(i)));
}

// A stored tail mode the firmware does not know falls back to a servo tail,
// rather than sending raw yaw to a motor output.
TEST_F(MixerTest, UnknownTailModeIsServo)
{
    mixerConfigMutable()->tail_rotor_mode = TAIL_MODE_COUNT;
    validateAndFixMixerConfig();

    EXPECT_EQ(TAIL_MODE_SERVO, mixerConfig()->tail_rotor_mode);
    EXPECT_FALSE(mixerMotorizedTail());
}

// A pitch limit which leaves no room for cyclic is raised until it does,
// so that collective at its limit cannot lock the cyclic out entirely.
TEST_F(MixerTest, PitchLimitKeepsCyclicHeadroom)
{
    mixerConfigMutable()->swash_pitch_limit = 100;
    validateAndFixMixerConfig();

    EXPECT_EQ(mixerInputs(MIXER_IN_STABILIZED_COLLECTIVE)->max + 500,
              mixerConfig()->swash_pitch_limit);
}
