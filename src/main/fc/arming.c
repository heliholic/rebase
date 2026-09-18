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

#include "blackbox/blackbox.h"
#include "blackbox/blackbox_fielddefs.h"

#include "common/maths.h"
#include "common/utils.h"

#include "config/config.h"

#include "drivers/dshot.h"
#include "drivers/dshot_command.h"
#include "drivers/light_led.h"
#include "drivers/motor.h"
#include "drivers/sound_beeper.h"
#include "drivers/time.h"

#include "fc/feature.h"
#include "fc/motors.h"
#include "fc/runtime_config.h"
#include "fc/stats.h"

#include "flight/failsafe.h"
#include "flight/imu.h"

#include "io/beeper.h"
#include "io/gps.h"
#include "io/statusindicator.h"

#include "pg/arming.h"
#include "pg/motor.h"
#include "pg/rx.h"
#include "pg/system.h"

#include "rx/rc_stats.h"
#include "rx/rx.h"

#include "scheduler/scheduler.h"

#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/compass.h"
#include "sensors/gyro.h"

#include "controls/modes.h"
#include "controls/rc.h"

#include "arming.h"


enum {
    ARMING_DELAYED_DISARMED = 0,
    ARMING_DELAYED_NORMAL = 1,
};

static int lastArmingDisabledReason = 0;
static timeUs_t lastDisarmTimeUs;
static int tryingToArm = ARMING_DELAYED_DISARMED;

static armingDisableFlags_e armingDisableFlags = 0;

#define ENTRY(_NAME)   [ARMING_DISABLED_ ## _NAME ## _BIT] = #_NAME
static const char * const armingDisableFlagNames[ARMING_DISABLE_FLAGS_COUNT] = {
    ENTRY(NO_GYRO),
    ENTRY(FAILSAFE),
    ENTRY(RX_FAILSAFE),
    ENTRY(NOT_DISARMED),
    ENTRY(BOXFAILSAFE),
    ENTRY(NO_ARM_SWITCH),
    ENTRY(UNUSED_6),
    ENTRY(THROTTLE),
    ENTRY(ANGLE),
    ENTRY(BOOT_GRACE_TIME),
    ENTRY(NOPREARM),
    ENTRY(LOAD),
    ENTRY(CALIBRATING),
    ENTRY(CLI),
    ENTRY(UNUSED_14),
    ENTRY(BST),
    ENTRY(MSP),
    ENTRY(PARALYZE),
    ENTRY(GPS),
    ENTRY(UNUSED_19),
    ENTRY(DSHOT_TELEM),
    ENTRY(REBOOT_REQUIRED),
    ENTRY(DSHOT_BITBANG),
    ENTRY(ACC_CALIBRATION),
    ENTRY(MOTOR_PROTOCOL),
    ENTRY(UNUSED_25),
    ENTRY(UNUSED_26),
    ENTRY(UNUSED_27),
    ENTRY(UNUSED_28),
    ENTRY(ARM_SWITCH),
};
#undef ENTRY


/*
 * Arming disable flags
 */

void setArmingDisabled(armingDisableFlags_e flag)
{
    armingDisableFlags = armingDisableFlags | flag;
}

void unsetArmingDisabled(armingDisableFlags_e flag)
{
    armingDisableFlags = armingDisableFlags & ~flag;
}

bool isArmingDisabled(void)
{
    return armingDisableFlags;
}

armingDisableFlags_e getArmingDisableFlags(void)
{
    return armingDisableFlags;
}

// return name for given flag
// will return first name (LSB) if multiple bits are passed
const char *getArmingDisableFlagName(armingDisableFlags_e flag)
{
    if (!flag) {
        return "NONE";
    }
    unsigned idx = ffs(flag & -flag) - 1;   // use LSB if there are multiple bits set
    if (idx >= ARRAYLEN(armingDisableFlagNames) || !armingDisableFlagNames[idx]) {
        return "UNKNOWN";                   // the bit list has holes; every caller prints this
    }
    return armingDisableFlagNames[idx];
}

void resetArmingDisabled(void)
{
    lastArmingDisabledReason = 0;
}


/*
 * Arming status
 */

static bool isCalibrating(void)
{
    return (sensors(SENSOR_GYRO) && !gyroIsCalibrationComplete())
#ifdef USE_ACC
        || (sensors(SENSOR_ACC) && !accIsCalibrationComplete())
#endif
#ifdef USE_BARO
        || (sensors(SENSOR_BARO) && !baroIsCalibrated())
#endif
#ifdef USE_MAG
        || (sensors(SENSOR_MAG) && !compassIsCalibrationComplete())
#endif
        ;
}

#ifdef USE_ACC
static bool accNeedsCalibration(void)
{
    if (sensors(SENSOR_ACC)) {

        // Check to see if the ACC has already been calibrated
        if (accHasBeenCalibrated()) {
            return false;
        }

        // We've determined that there's a detected ACC that has not
        // yet been calibrated. Check to see if anything is using the
        // ACC that would be affected by the lack of calibration.

        // Check for any configured modes that use the ACC
        if (isModeActivationConditionPresent(BOXANGLE) ||
            isModeActivationConditionPresent(BOXHORIZON)) {
            return true;
        }
    }

    return false;
}
#endif

#ifdef USE_DSHOT
static void setMotorSpinDirection(uint8_t spinDirection)
{
    if (isMotorProtocolDshot()) {
        dshotCommandWrite(ALL_MOTORS, getMotorCount(), spinDirection, DSHOT_CMD_TYPE_INLINE);
    }
}
#endif


/*
 * Arming status
 */

void updateArmingStatus(void)
{

    if (ARMING_FLAG(ARMED)) {
        LED0_ON;
    } else {
        // arming switch on, but not yet armed; currently DISARMED
        // identify things that should delay, or prevent, arming, then arm

        // Check if the power-on arming grace time has elapsed
        bool graceTimeElapsed =
            (getArmingDisableFlags() & ARMING_DISABLED_BOOT_GRACE_TIME)
            && (millis() >= systemConfig()->powerOnArmingGraceTime * 1000);

#ifdef USE_DSHOT
        // With DSHOT, we also require DSHOT to be ready
        graceTimeElapsed = graceTimeElapsed && (!isMotorProtocolDshot() || dshotStreamingCommandsAreEnabled());
#endif

        if (graceTimeElapsed) {
            unsetArmingDisabled(ARMING_DISABLED_BOOT_GRACE_TIME);
        }

        // The arm switch is the only way to arm
        if (isArmSwitchConfigured()) {
            unsetArmingDisabled(ARMING_DISABLED_NO_ARM_SWITCH);
        } else {
            setArmingDisabled(ARMING_DISABLED_NO_ARM_SWITCH);
        }

        // Check the arm switch is not defaulting to on when the RX link recovers from a fault
        static bool hadRx = false;
        const bool haveRx = isRxReceivingSignal();

        const bool justGotRxBack = !hadRx && haveRx;

        if (justGotRxBack && isArmSwitchActive()) {
            // If the RX has just started to receive a signal again and the arm switch is on, apply arming restriction
            setArmingDisabled(ARMING_DISABLED_NOT_DISARMED);
        } else if (haveRx && !isArmSwitchActive()) {
            // If RX signal is OK and the arm switch is off, remove arming restriction
            unsetArmingDisabled(ARMING_DISABLED_NOT_DISARMED);
        }
        hadRx = haveRx;

        if (IS_RC_MODE_ACTIVE(BOXFAILSAFE)) {
            setArmingDisabled(ARMING_DISABLED_BOXFAILSAFE);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_BOXFAILSAFE);
        }

        if (!isThrottleOff()) {
            setArmingDisabled(ARMING_DISABLED_THROTTLE);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_THROTTLE);
        }

        if (!isUpright()) {
            setArmingDisabled(ARMING_DISABLED_ANGLE);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_ANGLE);
        }

#if defined(USE_LATE_TASK_STATISTICS)
        if ((getCpuPercentageLate() > schedulerConfig()->cpuLatePercentageLimit)) {
            setArmingDisabled(ARMING_DISABLED_LOAD);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_LOAD);
        }
#endif // USE_LATE_TASK_STATISTICS

        if (isCalibrating()) {
            setArmingDisabled(ARMING_DISABLED_CALIBRATING);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_CALIBRATING);
        }

        if (isModeActivationConditionPresent(BOXPREARM)) {
            if (IS_RC_MODE_ACTIVE(BOXPREARM) && (!ARMING_FLAG(WAS_ARMED_WITH_PREARM) || armingConfig()->prearm_allow_rearm) ) {
                unsetArmingDisabled(ARMING_DISABLED_NOPREARM);
            } else {
                setArmingDisabled(ARMING_DISABLED_NOPREARM);
            }
        }

#ifdef USE_DSHOT_TELEMETRY
        // If Dshot Telemetry is enabled and any motor isn't providing telemetry, then disable arming
        if (useDshotTelemetry && !isDshotTelemetryActive()) {
            setArmingDisabled(ARMING_DISABLED_DSHOT_TELEM);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_DSHOT_TELEM);
        }
#endif

#ifdef USE_DSHOT_BITBANG
        if (isDshotBitbangActive(&motorConfig()->dev) && dshotBitbangGetStatus() != DSHOT_BITBANG_STATUS_OK) {
            setArmingDisabled(ARMING_DISABLED_DSHOT_BITBANG);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_DSHOT_BITBANG);
        }
#endif

        if (IS_RC_MODE_ACTIVE(BOXPARALYZE)) {
            setArmingDisabled(ARMING_DISABLED_PARALYZE);
        }

#ifdef USE_ACC
        if (accNeedsCalibration()) {
            setArmingDisabled(ARMING_DISABLED_ACC_CALIBRATION);
        } else {
            unsetArmingDisabled(ARMING_DISABLED_ACC_CALIBRATION);
        }
#endif

        if (!isMotorProtocolEnabled()) {
            setArmingDisabled(ARMING_DISABLED_MOTOR_PROTOCOL);
        }

        /* Ignore ARMING_DISABLED_CALIBRATING if we are going to calibrate gyro on first arm */
        bool ignoreGyro = armingConfig()->gyro_cal_on_first_arm
            && !(getArmingDisableFlags() & ~(ARMING_DISABLED_ARM_SWITCH | ARMING_DISABLED_CALIBRATING));

        // If arming is disabled and the ARM switch is on
        if (isArmingDisabled()
            && !ignoreGyro
            && isArmSwitchActive()) {
            setArmingDisabled(ARMING_DISABLED_ARM_SWITCH);
        } else if (!isArmSwitchActive()) {
            unsetArmingDisabled(ARMING_DISABLED_ARM_SWITCH);
        }

        if (isArmingDisabled()) {
            warningLedFlash();
        } else {
            warningLedDisable();
        }

        warningLedUpdate();
    }
}

void disarm(flightLogDisarmReason_e reason)
{
    if (ARMING_FLAG(ARMED)) {
        ENABLE_ARMING_FLAG(WAS_EVER_ARMED);
        DISABLE_ARMING_FLAG(ARMED); // disarm now
        lastDisarmTimeUs = micros();

#ifdef USE_BLACKBOX
        flightLogEvent_disarm_t eventData;
        eventData.reason = reason;
        blackboxLogEvent(FLIGHT_LOG_EVENT_DISARM, (flightLogEventData_t*)&eventData);
#else
        UNUSED(reason);
#endif

        BEEP_OFF;
#ifdef USE_PERSISTENT_STATS
        statsOnDisarm();
#endif

        // always set motor direction to normal on disarming
#ifdef USE_DSHOT
        setMotorSpinDirection(DSHOT_CMD_SPIN_DIRECTION_NORMAL);
#endif

        // make disarming beeps
        beeper(BEEPER_DISARMING);
    }
}

void tryArm(void)
{
    if (armingConfig()->gyro_cal_on_first_arm) {
        gyroStartCalibration(true);
    }


    updateArmingStatus();
    // set or clear armingDisabled flags, while arming is requested, whether armed or disarmed,


    if (!isArmingDisabled()) {
        if (ARMING_FLAG(ARMED)) {
            return;
            // don't allow arming if there are active armingDisabled flags or already armed
        }

#ifdef USE_DSHOT
        // Handle DShot factors that should delay arming
        timeDelta_t beaconTimeDiff = cmpTimeUs(micros(), getLastDshotBeaconCommandTimeUs());
        if (beaconTimeDiff < DSHOT_BEACON_GUARD_DELAY_US && beaconTimeDiff >= 0) {
            if (tryingToArm == ARMING_DELAYED_DISARMED) {
                tryingToArm = ARMING_DELAYED_NORMAL;
            }
            return;
        }

        if (isMotorProtocolDshot()) {
#if defined(USE_ESC_SENSOR) && defined(USE_DSHOT_TELEMETRY)
            // Try to activate extended DSHOT telemetry only if no esc sensor exists and dshot telemetry is active
            if (!featureIsEnabled(FEATURE_ESC_SENSOR) && useDshotTelemetry) {
                dshotCleanTelemetryData();
                if (motorConfig()->dev.useDshotEdt) {
                    dshotCommandWrite(ALL_MOTORS, getMotorCount(), DSHOT_CMD_EXTENDED_TELEMETRY_ENABLE, DSHOT_CMD_TYPE_INLINE);
                }
            }
#endif

            setMotorSpinDirection(DSHOT_CMD_SPIN_DIRECTION_NORMAL);
        }
#endif // USE_DSHOT


        ENABLE_ARMING_FLAG(ARMED);  // ***ARM NOW ***

#ifdef USE_RC_STATS
        NotifyRcStatsArming();
#endif

        resetTryingToArm();

        if (isModeActivationConditionPresent(BOXPREARM)) {
            ENABLE_ARMING_FLAG(WAS_ARMED_WITH_PREARM);
        }

        lastArmingDisabledReason = 0;

#ifdef USE_GPS
        //beep to indicate arming
        if (featureIsEnabled(FEATURE_GPS)) {
            GPS_reset_home_position();
            canUseGPSHeading = false; // reset GPS heading after each arm until IMU can align to GPS COG
            if (STATE(GPS_FIX) && gpsSol.numSat >= GPS_HOME_MIN_SATS) {
                beeper(BEEPER_ARMING_GPS_FIX);
            } else {
                beeper(BEEPER_ARMING_GPS_NO_FIX);
            }
        } else {
            beeper(BEEPER_ARMING);
        }
#else
        beeper(BEEPER_ARMING);
#endif

#ifdef USE_PERSISTENT_STATS
        statsOnArm();
#endif

    } else {
        resetTryingToArm();
        if (!isFirstArmingGyroCalibrationRunning()) {
            int armingDisabledReason = ffs(getArmingDisableFlags());
            if (lastArmingDisabledReason != armingDisabledReason) {
                lastArmingDisabledReason = armingDisabledReason;

                beeperWarningBeeps(armingDisabledReason);
            }
        }
    }
}

/*
 * Arming and disarming with the arm switch
 */

void processArmSwitch(void)
{
    // an extra guard for disarming through switch to prevent that one frame can disarm it
    static uint8_t rcDisarmTicks;

    if (isArmSwitchActive()) {
        rcDisarmTicks = 0;
        tryArm();
    } else {
        resetTryingToArm();
        resetArmingDisabled();
        const bool boxFailsafeSwitchIsOn = IS_RC_MODE_ACTIVE(BOXFAILSAFE);
        if (ARMING_FLAG(ARMED) && (failsafeIsReceivingRxData() || boxFailsafeSwitchIsOn)) {
            // in a true signal loss situation, allow disarm only once we regain validated RxData (failsafeIsReceivingRxData = true),
            // to avoid potentially false disarm signals soon after link recover
            // Note that BOXFAILSAFE will also drive failsafeIsReceivingRxData false (immediately at start or end)
            // That's why we explicitly allow disarm here if BOXFAILSAFE switch is active
            rcDisarmTicks++;
            if (rcDisarmTicks > 3) {
                // require three duplicate disarm values in a row before we disarm
                disarm(DISARM_REASON_SWITCH);
            }
        }
    }
}

/*
 * The armed beeper
 */

void updateArmedBeeper(void)
{
    static bool armedBeeperOn = false;

    // While armed with the throttle off, beep to warn that the machine is live
    if (ARMING_FLAG(ARMED)) {
        if (isThrottleOff()) {
            beeper(BEEPER_ARMED);
            armedBeeperOn = true;
        } else if (armedBeeperOn) {
            beeperSilence();
            armedBeeperOn = false;
        }
    }
}

timeUs_t getLastDisarmTimeUs(void)
{
    return lastDisarmTimeUs;
}

bool isTryingToArm(void)
{
    return (tryingToArm != ARMING_DELAYED_DISARMED);
}

void resetTryingToArm(void)
{
    tryingToArm = ARMING_DELAYED_DISARMED;
}
