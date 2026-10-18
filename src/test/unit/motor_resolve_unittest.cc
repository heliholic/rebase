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

#include <stdint.h>
#include <string.h>

extern "C" {
    #include "platform.h"

    #include "pg/motor.h"

    #include "drivers/motor_types.h"
    #include "drivers/motor_resolve.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

class MotorResolveTest : public ::testing::Test
{
protected:
    motorResolveInput_t in;
    motorResolveOutput_t out;

    void SetUp() override
    {
        memset(&in, 0, sizeof(in));
        memset(&out, 0xA5, sizeof(out));
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
            in.motor[i].protocol = MOTOR_PROTOCOL_DISABLED;
            in.motor[i].timer = MOTOR_TIMER_NONE;
        }
        for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
            in.servo[i].timer = MOTOR_TIMER_NONE;
        }
        in.bitbangMode = DSHOT_BITBANG_OFF;
    }

    void servo(int index, int8_t timer, uint16_t rate, uint16_t maxPulse = 2200)
    {
        in.servo[index].timer = timer;
        in.servo[index].rate = rate;
        in.servo[index].maxPulse = maxPulse;
    }

    void motor(int index, uint8_t protocol, int8_t timer, uint16_t rate = 0, uint16_t flags = 0)
    {
        in.motor[index].protocol = protocol;
        in.motor[index].timer = timer;
        in.motor[index].hasPin = true;
        in.motor[index].rate = rate;
        in.motor[index].maxPulse = DEFAULT_MOTOR_FULL_PULSE;
        in.motor[index].flags = flags;
    }
};

TEST_F(MotorResolveTest, ProtocolNames)
{
    EXPECT_STREQ("DSHOT300", motorProtocolName(MOTOR_PROTOCOL_DSHOT300));
    EXPECT_STREQ("?", motorProtocolName(MOTOR_PROTOCOL_COUNT));
    EXPECT_EQ(MOTOR_PROTOCOL_ONESHOT125, motorProtocolFromName("oneshot125"));
    EXPECT_EQ(MOTOR_PROTOCOL_DISABLED, motorProtocolFromName("DISABLED"));
    EXPECT_EQ(-1, motorProtocolFromName("DSHOT1200"));

    for (int i = 0; i < MOTOR_PROTOCOL_COUNT; i++) {
        EXPECT_EQ(i, motorProtocolFromName(motorProtocolName(i)));
    }
}

TEST_F(MotorResolveTest, ProtocolRates)
{
    EXPECT_EQ(BRUSHLESS_MOTORS_PWM_RATE, motorProtocolDefaultRate(MOTOR_PROTOCOL_PWM, false));
    EXPECT_EQ(BRUSHED_MOTORS_PWM_RATE, motorProtocolDefaultRate(MOTOR_PROTOCOL_BRUSHED, false));
    EXPECT_EQ(2000, motorProtocolMaxRate(MOTOR_PROTOCOL_ONESHOT125, false));
    EXPECT_EQ(4000, motorProtocolMaxRate(MOTOR_PROTOCOL_DSHOT150, false));
    EXPECT_EQ(2000, motorProtocolMaxRate(MOTOR_PROTOCOL_DSHOT150, true));
    EXPECT_EQ(MOTOR_RATE_MAX / 2, motorProtocolDefaultRate(MOTOR_PROTOCOL_DSHOT600, true));
    EXPECT_EQ(0, motorProtocolMaxRate(MOTOR_PROTOCOL_DRONECAN, false));
    EXPECT_EQ(0, motorProtocolMaxRate(MOTOR_PROTOCOL_DISABLED, false));
}

TEST_F(MotorResolveTest, ProtocolPulses)
{
    EXPECT_EQ(MOTOR_PULSE_MIN, motorProtocolMinPulse(MOTOR_PROTOCOL_PWM));
    EXPECT_EQ(MOTOR_PULSE_MAX, motorProtocolMaxPulse(MOTOR_PROTOCOL_PWM));
    // MultiShot would go negative below 750
    EXPECT_EQ(750, motorProtocolMinPulse(MOTOR_PROTOCOL_MULTISHOT));
    EXPECT_EQ(1000, motorProtocolMinPulse(MOTOR_PROTOCOL_BRUSHED));
    EXPECT_EQ(2000, motorProtocolMaxPulse(MOTOR_PROTOCOL_BRUSHED));
}

TEST_F(MotorResolveTest, ProtocolFamilies)
{
    EXPECT_EQ(MOTOR_PROTOCOL_FAMILY_PWM, motorProtocolFamily(MOTOR_PROTOCOL_ONESHOT42));
    EXPECT_EQ(MOTOR_PROTOCOL_FAMILY_DSHOT, motorProtocolFamily(MOTOR_PROTOCOL_PROSHOT1000));
    EXPECT_EQ(MOTOR_PROTOCOL_FAMILY_UNKNOWN, motorProtocolFamily(MOTOR_PROTOCOL_DISABLED));
    EXPECT_TRUE(motorProtocolIsContinuous(MOTOR_PROTOCOL_BRUSHED));
    EXPECT_FALSE(motorProtocolIsContinuous(MOTOR_PROTOCOL_ONESHOT125));
}

TEST_F(MotorResolveTest, AbsentMotors)
{
    // No pin, or protocol disabled
    in.motor[0].protocol = MOTOR_PROTOCOL_PWM;
    motor(1, MOTOR_PROTOCOL_DISABLED, 3);
    motorResolve(&in, &out);

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        EXPECT_EQ(MOTOR_STATE_ABSENT, out.state[i]) << "motor " << i;
        EXPECT_EQ(MOTOR_PROTOCOL_FAMILY_UNKNOWN, out.family[i]) << "motor " << i;
        EXPECT_EQ(0, out.rate[i]) << "motor " << i;
    }
}

TEST_F(MotorResolveTest, PinWithoutTimer)
{
    motor(0, MOTOR_PROTOCOL_PWM, MOTOR_TIMER_NONE);
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_NO_TIMER, out.state[0]);

    // The simulator needs no pins or timers
    in.pinless = true;
    in.motor[0].hasPin = false;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(BRUSHLESS_MOTORS_PWM_RATE, out.rate[0]);
}

TEST_F(MotorResolveTest, UnsupportedProtocol)
{
    // DroneCAN is not built into the test, and needs no pin when it is
    in.motor[0].protocol = MOTOR_PROTOCOL_DRONECAN;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_UNSUPPORTED, out.state[0]);
}

TEST_F(MotorResolveTest, RequestedRates)
{
    motor(0, MOTOR_PROTOCOL_PWM, 1, 50);
    motor(1, MOTOR_PROTOCOL_PWM, 2, 0);
    motor(2, MOTOR_PROTOCOL_PWM, 3, 0);
    in.motor[2].maxPulse = 2500;
    motorResolve(&in, &out);

    EXPECT_EQ(50, out.rate[0]);
    EXPECT_EQ(BRUSHLESS_MOTORS_PWM_RATE, out.rate[1]);
    // The period must hold the longest pulse
    EXPECT_EQ(400, out.rate[2]);
}

TEST_F(MotorResolveTest, SharedTimerTakesSlowestRate)
{
    motor(0, MOTOR_PROTOCOL_PWM, 3, 333);
    motor(1, MOTOR_PROTOCOL_BRUSHED, 3, 50);
    motor(2, MOTOR_PROTOCOL_PWM, 4, 200);
    motorResolve(&in, &out);

    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[1]);
    EXPECT_EQ(0, out.group[1]);
    EXPECT_EQ(50, out.rate[0]);
    EXPECT_EQ(50, out.rate[1]);
    // A motor on another timer keeps its own
    EXPECT_EQ(2, out.group[2]);
    EXPECT_EQ(200, out.rate[2]);
}

TEST_F(MotorResolveTest, SharedTimerNeedsOneTimebase)
{
    motor(0, MOTOR_PROTOCOL_ONESHOT125, 3);
    motor(1, MOTOR_PROTOCOL_ONESHOT42, 3);
    motor(2, MOTOR_PROTOCOL_ONESHOT125, 3);
    motorResolve(&in, &out);

    // The lowest motor wins
    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(MOTOR_STATE_TIMER_CLASH, out.state[1]);
    EXPECT_EQ(0, out.rate[1]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[2]);
    EXPECT_EQ(0, out.group[2]);
}

TEST_F(MotorResolveTest, SharedTimerContinuousAndSynced)
{
    motor(0, MOTOR_PROTOCOL_PWM, 3);
    motor(1, MOTOR_PROTOCOL_MULTISHOT, 3);
    motorResolve(&in, &out);

    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(MOTOR_STATE_TIMER_CLASH, out.state[1]);
}

TEST_F(MotorResolveTest, BitbangAuto)
{
    in.bitbangMode = DSHOT_BITBANG_AUTO;
    motor(0, MOTOR_PROTOCOL_DSHOT600, 1);
    motor(1, MOTOR_PROTOCOL_DSHOT600, 2);
    EXPECT_TRUE(motorResolveBitbang(&in));

    // Some MCUs only want it for bidirectional DShot
    in.bitbangNeedsBidir = true;
    EXPECT_FALSE(motorResolveBitbang(&in));
    in.motor[0].flags = MOTOR_FLAG_BIDIR;
    in.motor[1].flags = MOTOR_FLAG_BIDIR;
    EXPECT_TRUE(motorResolveBitbang(&in));

    // Bitbang has no ProShot
    in.motor[1].protocol = MOTOR_PROTOCOL_PROSHOT1000;
    EXPECT_FALSE(motorResolveBitbang(&in));

    // Motors that differ fall back to DMA rather than being refused
    in.bitbangNeedsBidir = false;
    in.motor[1].protocol = MOTOR_PROTOCOL_DSHOT600;
    in.motor[1].flags = 0;
    EXPECT_FALSE(motorResolveBitbang(&in));
    in.motor[1].flags = MOTOR_FLAG_BIDIR | MOTOR_FLAG_INVERTED;
    EXPECT_FALSE(motorResolveBitbang(&in));
    in.motor[1].flags = MOTOR_FLAG_BIDIR;
    in.motor[1].protocol = MOTOR_PROTOCOL_DSHOT300;
    EXPECT_FALSE(motorResolveBitbang(&in));
    in.motor[1].protocol = MOTOR_PROTOCOL_DSHOT600;
    EXPECT_TRUE(motorResolveBitbang(&in));

    // Nothing to drive
    SetUp();
    in.bitbangMode = DSHOT_BITBANG_AUTO;
    motor(0, MOTOR_PROTOCOL_PWM, 1);
    EXPECT_FALSE(motorResolveBitbang(&in));
}

TEST_F(MotorResolveTest, BitbangOnRefusesProshot)
{
    in.bitbangMode = DSHOT_BITBANG_ON;
    motor(0, MOTOR_PROTOCOL_PROSHOT1000, 1);
    motorResolve(&in, &out);

    EXPECT_TRUE(out.bitbang);
    EXPECT_EQ(MOTOR_STATE_UNSUPPORTED, out.state[0]);
}

TEST_F(MotorResolveTest, BitbangIsOneGroup)
{
    in.bitbangMode = DSHOT_BITBANG_ON;
    motor(0, MOTOR_PROTOCOL_DSHOT300, 1, 1000);
    motor(1, MOTOR_PROTOCOL_DSHOT300, 2, 500);
    motor(2, MOTOR_PROTOCOL_DSHOT300, 3, 0, MOTOR_FLAG_INVERTED);
    motorResolve(&in, &out);

    // One pacer timer, whatever timers the pins have
    EXPECT_EQ(MOTOR_STATE_OK, out.state[1]);
    EXPECT_EQ(0, out.group[1]);
    EXPECT_EQ(500, out.rate[0]);
    EXPECT_EQ(500, out.rate[1]);
    // and one line idle level
    EXPECT_EQ(MOTOR_STATE_GROUP_CLASH, out.state[2]);
}

TEST_F(MotorResolveTest, BidirNeedsCapture)
{
    // An N-channel output cannot capture the reply over DMA
    motor(0, MOTOR_PROTOCOL_DSHOT600, 1, 0, MOTOR_FLAG_BIDIR);
    in.motor[0].nChannel = true;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_NO_BIDIR, out.state[0]);

    // Bitbang reads the pin directly
    in.bitbangMode = DSHOT_BITBANG_ON;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);

    // Nor can forced burst DMA; AUTO gives burst up instead
    SetUp();
    motor(0, MOTOR_PROTOCOL_DSHOT600, 1, 0, MOTOR_FLAG_BIDIR);
    in.burstMode = DSHOT_DMAR_ON;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_NO_BIDIR, out.state[0]);
    in.burstMode = DSHOT_DMAR_AUTO;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);

    // A motor without telemetry is fine either way
    in.burstMode = DSHOT_DMAR_ON;
    in.motor[0].flags = 0;
    in.motor[0].nChannel = true;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
}

TEST_F(MotorResolveTest, DriversMix)
{
    motor(0, MOTOR_PROTOCOL_PWM, 1, 50);
    motor(1, MOTOR_PROTOCOL_DSHOT300, 2, 2000, MOTOR_FLAG_BIDIR);
    motor(2, MOTOR_PROTOCOL_ONESHOT125, 3);
    motorResolve(&in, &out);

    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(MOTOR_PROTOCOL_FAMILY_PWM, out.family[0]);
    EXPECT_EQ(50, out.rate[0]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[1]);
    EXPECT_EQ(MOTOR_PROTOCOL_FAMILY_DSHOT, out.family[1]);
    EXPECT_EQ(2000, out.rate[1]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[2]);
    EXPECT_EQ(2000, out.rate[2]);
}

TEST_F(MotorResolveTest, DriversShareNoTimer)
{
    motor(0, MOTOR_PROTOCOL_DSHOT600, 3);
    motor(1, MOTOR_PROTOCOL_PWM, 3);
    motorResolve(&in, &out);

    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(MOTOR_STATE_TIMER_CLASH, out.state[1]);

    // A bitbang motor leaves its pin's timer free
    in.bitbangMode = DSHOT_BITBANG_ON;
    motorResolve(&in, &out);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[1]);
}

TEST_F(MotorResolveTest, DshotMotorsDiffer)
{
    motor(0, MOTOR_PROTOCOL_DSHOT300, 1, 0, MOTOR_FLAG_BIDIR);
    motor(1, MOTOR_PROTOCOL_DSHOT600, 2);
    motor(2, MOTOR_PROTOCOL_PROSHOT1000, 3, 0, MOTOR_FLAG_INVERTED);
    // Telemetry may differ on one timer, the protocol may not
    motor(3, MOTOR_PROTOCOL_DSHOT300, 1);
    motor(4, MOTOR_PROTOCOL_DSHOT150, 1);
    motorResolve(&in, &out);

    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[1]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[2]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[3]);
    EXPECT_EQ(0, out.group[3]);
    EXPECT_EQ(MOTOR_STATE_TIMER_CLASH, out.state[4]);
    // The bidirectional motor holds its timer to half the frame rate
    EXPECT_EQ(5000, out.rate[0]);
    EXPECT_EQ(5000, out.rate[3]);
    EXPECT_EQ(MOTOR_RATE_MAX, out.rate[1]);
}

TEST_F(MotorResolveTest, BitbangAutoMixedTelemetryUsesDma)
{
    in.bitbangMode = DSHOT_BITBANG_AUTO;
    motor(0, MOTOR_PROTOCOL_DSHOT600, 1, 0, MOTOR_FLAG_BIDIR);
    motor(1, MOTOR_PROTOCOL_DSHOT600, 2);
    motorResolve(&in, &out);

    EXPECT_FALSE(out.bitbang);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[0]);
    EXPECT_EQ(MOTOR_STATE_OK, out.state[1]);
}

TEST_F(MotorResolveTest, ServoJoinsFreeRunningMotor)
{
    motor(0, MOTOR_PROTOCOL_PWM, 3, 200);
    servo(0, 3, 333);
    servo(1, 3, 150);
    servo(2, 4, 333);
    motorResolve(&in, &out);

    // Motor and servos on timer 3 run at the slowest of them
    EXPECT_EQ(150, out.rate[0]);
    EXPECT_EQ(150, out.servoRate[0]);
    EXPECT_EQ(150, out.servoRate[1]);
    // A servo on a timer without motors is left to itself
    EXPECT_EQ(0, out.servoRate[2]);
    EXPECT_EQ(0u, out.refusedServos);
}

TEST_F(MotorResolveTest, ServoPeriodHoldsLongestPulse)
{
    motor(0, MOTOR_PROTOCOL_PWM, 3);
    servo(0, 3, 5000, 2500);
    motorResolve(&in, &out);

    // 5% above the longest servo pulse
    EXPECT_EQ(380, out.rate[0]);
    EXPECT_EQ(380, out.servoRate[0]);
}

TEST_F(MotorResolveTest, ServoRefusedBySyncedMotor)
{
    motor(0, MOTOR_PROTOCOL_DSHOT600, 3);
    motor(1, MOTOR_PROTOCOL_ONESHOT125, 4);
    servo(0, 3, 333);
    servo(1, 4, 333);
    servo(2, 5, 333);
    motorResolve(&in, &out);

    EXPECT_EQ(BIT(0) | BIT(1), out.refusedServos);
    EXPECT_EQ(0, out.servoRate[0]);
    // The motors keep their own rates
    EXPECT_EQ(MOTOR_RATE_MAX, out.rate[0]);

    // A bitbang motor leaves its pin's timer to the servo
    in.bitbangMode = DSHOT_BITBANG_ON;
    motorResolve(&in, &out);
    EXPECT_EQ(BIT(1), out.refusedServos);
}
