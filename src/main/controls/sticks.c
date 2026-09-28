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
#include <string.h>

#include "platform.h"

#ifdef USE_STICK_COMMANDS

#include "config/config.h"

#include "controls/modes.h"
#include "controls/rc.h"

#include "fc/feature.h"
#include "fc/runtime_config.h"

#include "io/beeper.h"
#include "io/dashboard.h"
#include "io/gps.h"
#include "io/usb_cdc_hid.h"

#include "pg/arming.h"
#include "pg/pg.h"

#include "scheduler/scheduler.h"

#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/compass.h"
#include "sensors/gyro.h"

#include "sticks.h"

// The sticks are roll, pitch, yaw and collective; throttle is not on a stick.
#define ROL_LO (1 << (2 * RC_ROLL))
#define ROL_CE (3 << (2 * RC_ROLL))
#define ROL_HI (2 << (2 * RC_ROLL))
#define PIT_LO (1 << (2 * RC_PITCH))
#define PIT_CE (3 << (2 * RC_PITCH))
#define PIT_HI (2 << (2 * RC_PITCH))
#define YAW_LO (1 << (2 * RC_YAW))
#define YAW_CE (3 << (2 * RC_YAW))
#define YAW_HI (2 << (2 * RC_YAW))
#define COL_LO (1 << (2 * RC_COLLECTIVE))
#define COL_CE (3 << (2 * RC_COLLECTIVE))
#define COL_HI (2 << (2 * RC_COLLECTIVE))
#define COL_MASK (3 << (2 * RC_COLLECTIVE))

#define STICK_COMMAND_MIN  -450
#define STICK_COMMAND_MAX   450

#define STICK_DELAY_MS      50
#define STICK_AUTOREPEAT_MS 250
#define repeatAfter(t) { \
    rcDelayMs -= (t); \
    doNotRepeat = false; \
}

// time the sticks have been held in the current position
static int16_t rcDelayMs;
// the current stick position, as a command combo
static uint8_t rcSticks;
// a command has been executed; the sticks must move before the next one
static bool doNotRepeat;
// accelerometer trims changed; save them once the collective leaves the top
static bool pendingApplyRollAndPitchTrimDeltaSave;

void stickCommandsInit(void)
{
    // Whatever the sticks are doing, it is not a command in progress. The position
    // itself is left alone, so a combo that is still held does not execute again.
    rcDelayMs = 0;
    doNotRepeat = true;
    pendingApplyRollAndPitchTrimDeltaSave = false;
}

void processStickCommands(void)
{
    // checking sticks positions
    uint8_t stTmp = 0;
    for (int i = 0; i < 4; i++) {
        stTmp >>= 2;
        if (rcCommand[i] > STICK_COMMAND_MIN) {
            stTmp |= 0x80;  // check for MIN
        }
        if (rcCommand[i] < STICK_COMMAND_MAX) {
            stTmp |= 0x40;  // check for MAX
        }
    }
    if (stTmp == rcSticks) {
        if (rcDelayMs <= INT16_MAX - (getTaskDeltaTimeUs(TASK_SELF) / 1000)) {
            rcDelayMs += getTaskDeltaTimeUs(TASK_SELF) / 1000;
        }
    } else {
        rcDelayMs = 0;
        doNotRepeat = false;
    }
    rcSticks = stTmp;

    if (ARMING_FLAG(ARMED) || doNotRepeat || rcDelayMs <= STICK_DELAY_MS) {
        return;
    }
    doNotRepeat = true;

    // Stick commands must be enabled, and not disabled by the switch
    if (!armingConfig()->enable_stick_commands || IS_RC_MODE_ACTIVE(BOXSTICKCOMMANDDISABLE)) {
        return;
    }

#ifdef USE_USB_CDC_HID
    // If this target is used as a joystick, we should leave here.
    if (cdcDeviceIsMayBeActive()) {
        return;
    }
#endif

    // actions during not armed

    if (rcSticks == COL_LO + YAW_LO + PIT_LO + ROL_CE) {
        // GYRO calibration
        gyroStartCalibration(false);

#ifdef USE_GPS
        if (featureIsEnabled(FEATURE_GPS)) {
            GPS_reset_home_position();
        }
#endif

#ifdef USE_BARO
        if (sensors(SENSOR_BARO)) {
            baroSetGroundLevel();
        }
#endif

        return;
    }

    // Change PID profile
    switch (rcSticks) {
    case COL_LO + YAW_LO + PIT_CE + ROL_LO:
        // ROLL left -> PID profile 1
        changePidProfile(0);
        return;
    case COL_LO + YAW_LO + PIT_HI + ROL_CE:
        // PITCH up -> PID profile 2
        changePidProfile(1);
        return;
    case COL_LO + YAW_LO + PIT_CE + ROL_HI:
        // ROLL right -> PID profile 3
        changePidProfile(2);
        return;
    }

    if (rcSticks == COL_LO + YAW_LO + PIT_LO + ROL_HI) {
        saveConfigAndNotify();
    }

#ifdef USE_ACC
    if (rcSticks == COL_HI + YAW_LO + PIT_LO + ROL_CE) {
        // Calibrating Acc
        accStartCalibration();
        return;
    }
#endif

#if defined(USE_MAG)
    if (rcSticks == COL_HI + YAW_HI + PIT_LO + ROL_CE) {
        // Calibrating Mag
        compassStartCalibration();

        return;
    }
#endif

    if (FLIGHT_MODE(ANGLE_MODE | HORIZON_MODE)) {
        // in ANGLE or HORIZON mode, so use sticks to apply accelerometer trims
        rollAndPitchTrims_t accelerometerTrimsDelta;
        memset(&accelerometerTrimsDelta, 0, sizeof(accelerometerTrimsDelta));

        if (pendingApplyRollAndPitchTrimDeltaSave && ((rcSticks & COL_MASK) != COL_HI)) {
            saveConfigAndNotify();
            pendingApplyRollAndPitchTrimDeltaSave = false;
            return;
        }

        bool shouldApplyRollAndPitchTrimDelta = false;
        switch (rcSticks) {
        case COL_HI + YAW_CE + PIT_HI + ROL_CE:
            accelerometerTrimsDelta.values.pitch = 1;
            shouldApplyRollAndPitchTrimDelta = true;
            break;
        case COL_HI + YAW_CE + PIT_LO + ROL_CE:
            accelerometerTrimsDelta.values.pitch = -1;
            shouldApplyRollAndPitchTrimDelta = true;
            break;
        case COL_HI + YAW_CE + PIT_CE + ROL_HI:
            accelerometerTrimsDelta.values.roll = 1;
            shouldApplyRollAndPitchTrimDelta = true;
            break;
        case COL_HI + YAW_CE + PIT_CE + ROL_LO:
            accelerometerTrimsDelta.values.roll = -1;
            shouldApplyRollAndPitchTrimDelta = true;
            break;
        }
        if (shouldApplyRollAndPitchTrimDelta) {
#if defined(USE_ACC)
            applyAccelerometerTrimsDelta(&accelerometerTrimsDelta);
#endif
            pendingApplyRollAndPitchTrimDeltaSave = true;

            beeperConfirmationBeeps(1);

            repeatAfter(STICK_AUTOREPEAT_MS);

            return;
        }
    } else {
        // in ACRO mode, so use sticks to change RATE profile
        switch (rcSticks) {
        case COL_HI + YAW_CE + PIT_HI + ROL_CE:
            changeRateProfile(0);
            return;
        case COL_HI + YAW_CE + PIT_LO + ROL_CE:
            changeRateProfile(1);
            return;
        case COL_HI + YAW_CE + PIT_CE + ROL_HI:
            changeRateProfile(2);
            return;
        case COL_HI + YAW_CE + PIT_CE + ROL_LO:
            changeRateProfile(3);
            return;
        }
    }

#ifdef USE_DASHBOARD
    if (rcSticks == COL_LO + YAW_CE + PIT_HI + ROL_LO) {
        dashboardDisablePageCycling();
    }

    if (rcSticks == COL_LO + YAW_CE + PIT_HI + ROL_HI) {
        dashboardEnablePageCycling();
    }
#endif
}

#endif // USE_STICK_COMMANDS
