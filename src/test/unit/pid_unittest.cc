/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <cmath>

#include "unittest_macros.h"
#include "gtest/gtest.h"
#include "build/debug.h"

bool simulatedThrottleRaised = true;
float simulatedSetpointRate[3] = { 0,0,0 };
float simulatedPrevSetpointRate[3] = { 0,0,0 };
float simulatedRcDeflection[3] = { 0,0,0 };
float simulatedMaxRcDeflectionAbs = 0;
float simulatedMixerGetRcThrottle = 0;
float simulatedRcCommandDelta[3] = { 1,1,1 };
float simulatedRawSetpoint[3] = { 0,0,0 };
float simulatedMaxRate[3] = { 670,670,670 };
float simulatedFeedforward[3] = { 0,0,0 };

int16_t debug[DEBUG16_VALUE_COUNT];
uint8_t debugMode;

extern "C" {
    #include "platform.h"

    #include "build/debug.h"

    #include "common/axis.h"
    #include "common/maths.h"
    #include "common/filter.h"

    #include "config/config.h"
    #include "config/config_reset.h"

    #include "drivers/sound_beeper.h"
    #include "drivers/time.h"

    #include "fc/controlrate_profile.h"
    #include "fc/core.h"
    #include "fc/rc.h"

    #include "fc/rc_controls.h"
    #include "fc/runtime_config.h"

    #include "flight/imu.h"
    #include "flight/mixer.h"
    #include "flight/pid.h"
    #include "flight/position.h"

    #include "io/gps.h"

    #include "pg/pg.h"
    #include "pg/pg_ids.h"

    #include "pg/rx.h"
    #include "rx/rx.h"

    #include "sensors/gyro.h"
    #include "sensors/acceleration.h"

    acc_t acc;
    gyro_t gyro;
    attitudeEulerAngles_t attitude;

    rxRuntimeState_t rxRuntimeState = {};

    PG_REGISTER(accelerometerConfig_t, accelerometerConfig, PG_ACCELEROMETER_CONFIG);
    PG_REGISTER(systemConfig_t, systemConfig, PG_SYSTEM_CONFIG);
    PG_REGISTER(positionConfig_t, positionConfig, PG_SYSTEM_CONFIG);

    float getSetpointRate(int axis) { return simulatedSetpointRate[axis]; }
    bool wasThrottleRaised(void) { return simulatedThrottleRaised; }
    float getRcDeflectionAbs(int axis) { return fabsf(simulatedRcDeflection[axis]); }

    // used by auto-disarm code
    float getMaxRcDeflectionAbs() { return fabsf(simulatedMaxRcDeflectionAbs); }


    bool isBelowLandingAltitude(void) { return false; }

    void systemBeep(bool) { }
    bool gyroOverflowDetected(void) { return false; }
    float getRcDeflection(int axis) { return simulatedRcDeflection[axis]; }
    float getRcDeflectionRaw(int axis) { return simulatedRcDeflection[axis]; }
    float getRawSetpoint(int axis) { return simulatedRawSetpoint[axis]; }
    void beeperConfirmationBeeps(uint8_t) { }
    void disarm(flightLogDisarmReason_e) { }
    float getMaxRcRate(int axis)
    {
        UNUSED(axis);
        float maxRate = simulatedMaxRate[axis];
        return maxRate;
    }
    void initRcProcessing(void) { }
}

pidProfile_t *pidProfile;

int loopIter = 0;

// Always use same defaults for testing in future releases even when defaults change
void setDefaultTestSettings(void)
{
    pgResetAll();
    pidProfile = pidProfilesMutable(1);
    pidProfile->pid[PID_ROLL]  =  { 40, 40, 30, 65 };
    pidProfile->pid[PID_PITCH] =  { 58, 50, 35, 60 };
    pidProfile->pid[PID_YAW]   =  { 70, 45, 20, 60 };
    pidProfile->pid[PID_LEVEL] =  { 50, 50, 75, 50 };

    gyro.targetLooptime = 8000;
}

timeUs_t currentTestTime(void)
{
    return targetPidLooptime * loopIter++;
}

void resetTest(void)
{
    loopIter = 0;

    DISABLE_ARMING_FLAG(ARMED);

    setDefaultTestSettings();
    for (int axis = FD_ROLL; axis <= FD_YAW; axis++) {
        pidData[axis].P = 0;
        pidData[axis].I = 0;
        pidData[axis].D = 0;
        pidData[axis].F = 0;
        pidData[axis].Sum = 0;
        simulatedSetpointRate[axis] = 0;
        simulatedRcDeflection[axis] = 0;
        simulatedRawSetpoint[axis] = 0;
        gyro.gyroADCf[axis] = 0;
    }
    attitude.values.roll = 0;
    attitude.values.pitch = 0;
    attitude.values.yaw = 0;

    flightModeFlags = 0;
    pidInit(pidProfile);
    loadControlRateProfile();

    for (int loop = 0; loop < 20; loop++) {
        pidController(pidProfile, currentTestTime());
    }
    for (int axis = FD_ROLL; axis <= FD_YAW; axis++) {
        pidData[axis].P = 0;
        pidData[axis].I = 0;
        pidData[axis].D = 0;
        pidData[axis].F = 0;
        pidData[axis].Sum = 0;
    }
}

TEST(pidControllerTest, testInitialisation)
{
    resetTest();

    // In initial state PIDsums should be 0
    for (int axis = 0; axis <= FD_YAW; axis++) {
        EXPECT_FLOAT_EQ(0, pidData[axis].P);
        EXPECT_FLOAT_EQ(0, pidData[axis].I);
        EXPECT_FLOAT_EQ(0, pidData[axis].D);
    }
}
