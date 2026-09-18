/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "platform.h"

#include "blackbox/blackbox.h"
#include "blackbox/blackbox_fielddefs.h"

#include "build/debug.h"

#include "cli/cli.h"

#include "cms/cms.h"

#include "common/axis.h"
#include "common/filter.h"
#include "common/maths.h"
#include "common/utils.h"

#include "config/config.h"
#include "fc/feature.h"

#include "drivers/dshot.h"
#include "drivers/dshot_command.h"
#include "drivers/light_led.h"
#include "drivers/motor.h"
#include "drivers/sound_beeper.h"
#include "drivers/system.h"
#include "drivers/time.h"

#include "controls/rates.h"
#include "controls/rc.h"
#include "controls/adjustments.h"
#include "controls/sticks.h"
#include "fc/runtime_config.h"
#include "fc/stats.h"

#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/mixer.h"
#include "flight/pid.h"
#include "flight/position.h"
#include "fc/motors.h"
#include "fc/servos.h"

#include "io/beeper.h"
#include "io/gps.h"
#include "io/serial.h"
#include "io/statusindicator.h"
#include "io/vtx_control.h"
#include "io/vtx_rtc6705.h"

#include "msp/msp_serial.h"

#include "osd/osd.h"

#include "pg/motor.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/rx.h"

#include "rx/rc_stats.h"
#include "rx/rx.h"

#include "scheduler/scheduler.h"

#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/battery.h"
#include "sensors/boardalignment.h"
#include "sensors/compass.h"
#include "sensors/gyro.h"

#include "telemetry/telemetry.h"

#include "core.h"

enum {
    ALIGN_GYRO = 0,
    ALIGN_ACCEL = 1,
    ALIGN_MAG = 2
};

#define GYRO_WATCHDOG_DELAY 80 //  delay for gyro sync

static FAST_DATA_ZERO_INIT uint16_t pidUpdateCounter;

#ifdef USE_VTX_CONTROL
static bool canUpdateVTX(void)
{
#ifdef USE_VTX_RTC6705
    return vtxRTC6705CanUpdate();
#endif
    return true;
}
#endif



// Throttle percentage above which throttleRaised latches for the rest of the arm.
// Was the airmode_start_throttle_percent setting; kept at its former default.
#define THROTTLE_RAISED_PERCENT 25

static bool throttleRaised = false;
bool wasThrottleRaised(void)
{
    return throttleRaised;
}

/*
 * processRx called from taskUpdateRxMain
 */
bool processRx(timeUs_t currentTimeUs)
{
    if (!calculateRxChannelsAndUpdateFailsafe(currentTimeUs)) {
        return false;
    }

    updateRcRefreshRate(currentTimeUs);

    updateRSSI(currentTimeUs);

    if (currentTimeUs > FAILSAFE_POWER_ON_DELAY_US && !failsafeIsMonitoring()) {
        failsafeStartMonitoring();
    }

    const uint8_t throttlePercent = getThrottlePercent();

    if (ARMING_FLAG(ARMED)) {
        if (throttlePercent >= THROTTLE_RAISED_PERCENT) {
            throttleRaised = true; // Latch true until disarm
        }
    } else {
        throttleRaised = false;
    }

    return true;
}

void processRxModes(void)
{
#ifdef USE_TELEMETRY
    static bool sharedPortTelemetryEnabled = false;
#endif

    updateArmedBeeper();

    if (!(IS_RC_MODE_ACTIVE(BOXPARALYZE) && !ARMING_FLAG(ARMED))
#ifdef USE_CMS
        && !cmsInMenu
#endif
        ) {
        processArmSwitch();

#ifdef USE_STICK_COMMANDS
        processStickCommands();
#endif
    }

    updateActivatedModes();

    if (!cliMode && !(IS_RC_MODE_ACTIVE(BOXPARALYZE) && !ARMING_FLAG(ARMED))) {
        processRcAdjustments();
    }

    bool canUseHorizonMode = true;
    if ((IS_RC_MODE_ACTIVE(BOXANGLE)
        || failsafeIsActive()
        ) && (sensors(SENSOR_ACC))) {
        // bumpless transfer to Level mode
        canUseHorizonMode = false;

        if (!FLIGHT_MODE(ANGLE_MODE)) {
            ENABLE_FLIGHT_MODE(ANGLE_MODE);
        }
    } else {
        DISABLE_FLIGHT_MODE(ANGLE_MODE); // failsafe support
    }



    if (IS_RC_MODE_ACTIVE(BOXHORIZON) && canUseHorizonMode && sensors(SENSOR_ACC)) {
        DISABLE_FLIGHT_MODE(ANGLE_MODE);
        if (!FLIGHT_MODE(HORIZON_MODE)) {
            ENABLE_FLIGHT_MODE(HORIZON_MODE);
        }
    } else {
        DISABLE_FLIGHT_MODE(HORIZON_MODE);
    }

    if (FLIGHT_MODE(ANGLE_MODE | HORIZON_MODE)) {
        LED1_ON;
        // increase frequency of attitude task to reduce drift when in angle or horizon mode
        rescheduleTask(TASK_ATTITUDE, TASK_PERIOD_HZ(acc.sampleRateHz / (float)imuConfig()->imu_process_denom));
    } else {
        LED1_OFF;
        rescheduleTask(TASK_ATTITUDE, TASK_PERIOD_HZ(100));
    }

    if (!IS_RC_MODE_ACTIVE(BOXPREARM) && ARMING_FLAG(WAS_ARMED_WITH_PREARM)) {
        DISABLE_ARMING_FLAG(WAS_ARMED_WITH_PREARM);
    }

#ifdef USE_TELEMETRY
    if (featureIsEnabled(FEATURE_TELEMETRY)) {
        bool enableSharedPortTelemetry = (!isModeActivationConditionPresent(BOXTELEMETRY) && ARMING_FLAG(ARMED)) || (isModeActivationConditionPresent(BOXTELEMETRY) && IS_RC_MODE_ACTIVE(BOXTELEMETRY));
        if (enableSharedPortTelemetry && !sharedPortTelemetryEnabled) {
            mspSerialReleaseSharedTelemetryPorts();
            telemetryCheckState();

            sharedPortTelemetryEnabled = true;
        } else if (!enableSharedPortTelemetry && sharedPortTelemetryEnabled) {
            // the telemetry state must be checked immediately so that shared serial ports are released.
            telemetryCheckState();
            mspSerialAllocatePorts();

            sharedPortTelemetryEnabled = false;
        }
    }
#endif

#ifdef USE_VTX_CONTROL
    vtxUpdateActivatedChannel();

    if (canUpdateVTX()) {
        handleVTXControlButton();
    }
#endif
}

static FAST_CODE_NOINLINE void subTaskPidController(timeUs_t currentTimeUs)
{
    DEBUG_TIME_START(PIDLOOP, 1);

    pidController(currentPidProfile, currentTimeUs);

    DEBUG_TIME_END(PIDLOOP, 1);
}

static FAST_CODE_NOINLINE void subTaskPidSubprocesses(timeUs_t currentTimeUs)
{
    DEBUG_TIME_START(PIDLOOP, 3);

#ifdef USE_BLACKBOX
    if (!cliMode && blackboxConfig()->device) {
        blackboxUpdate(currentTimeUs);
    }
#else
    UNUSED(currentTimeUs);
#endif

    DEBUG_TIME_END(PIDLOOP, 3);
}

static FAST_CODE_NOINLINE void subTaskBlackboxFlush(timeUs_t currentTimeUs)
{
#ifdef USE_BLACKBOX
    if (blackboxConfig()->device) {
        blackboxFlush(currentTimeUs);
    }
#else
    UNUSED(currentTimeUs);
#endif
}

#ifdef USE_TELEMETRY
#define GYRO_TEMP_READ_DELAY_US (3 * 1000 * 1000)    // Only read the gyro temp every 3 seconds
void subTaskTelemetryPollSensors(timeUs_t currentTimeUs)
{
    static timeUs_t lastGyroTempTimeUs = 0;

    if (cmpTimeUs(currentTimeUs, lastGyroTempTimeUs) >= GYRO_TEMP_READ_DELAY_US) {
        // Read out gyro temperature if used for telemmetry
        gyroReadTemperature();
        lastGyroTempTimeUs = currentTimeUs;
    }
}
#endif

static FAST_CODE void subTaskMotorUpdate(timeUs_t currentTimeUs)
{
    DEBUG_TIME_START(PIDLOOP, 2);

    if (debugMode == DEBUG_CYCLETIME) {
        uint32_t startTime = micros();
        static uint32_t previousMotorUpdateTime;
        const uint32_t currentDeltaTime = startTime - previousMotorUpdateTime;
        debug[2] = currentDeltaTime;
        debug[3] = currentDeltaTime - gyro.targetLooptime;
        previousMotorUpdateTime = startTime;
    }

#ifdef USE_SERVOS
    servoUpdate();
#endif

    motorUpdate(currentTimeUs);

#ifdef USE_DSHOT_TELEMETRY_STATS
    if (debugMode == DEBUG_DSHOT_RPM_ERRORS && useDshotTelemetry) {
        const uint8_t motorCount = MIN(getMotorCount(), 4);
        for (uint8_t i = 0; i < motorCount; i++) {
            debug[i] = getDshotTelemetryMotorInvalidPercent(i);
        }
    }
#endif

    DEBUG_TIME_END(PIDLOOP, 2);
}

FAST_CODE void taskGyroSample(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);
    gyroUpdate();

    pidUpdateCounter = (pidUpdateCounter + 1) % activePidLoopDenom;
}

FAST_CODE bool gyroFilterReady(void)
{
    return ((pidUpdateCounter + 1) % activeFilterLoopDenom == 0);
}

bool pidLoopReady(void)
{
    return (pidUpdateCounter == 0);
}

FAST_CODE void taskFiltering(timeUs_t currentTimeUs)
{
#ifdef USE_DSHOT_TELEMETRY
    updateDshotTelemetry();  // decode and update Dshot telemetry
#endif
    gyroFiltering(currentTimeUs);
}

// Function for loop trigger
FAST_CODE void taskMainPidLoop(timeUs_t currentTimeUs)
{

#if ENABLE_SIMULATOR_GYROPID_SYNC
    if (lockMainPID() != 0) return;
#endif

    // DEBUG_PIDLOOP, timings for:
    // 0 - gyroUpdate()
    // 1 - subTaskPidController()
    // 2 - subTaskMotorUpdate()
    // 3 - subTaskPidSubprocesses()
    DEBUG_SET(DEBUG_PIDLOOP, 0, micros() - currentTimeUs);

    subTaskPidController(currentTimeUs);
    subTaskMotorUpdate(currentTimeUs);
    subTaskPidSubprocesses(currentTimeUs);
    subTaskBlackboxFlush(currentTimeUs);

    DEBUG_SET(DEBUG_CYCLETIME, 0, getTaskDeltaTimeUs(TASK_SELF));
    DEBUG_SET(DEBUG_CYCLETIME, 1, getAverageSystemLoadPercent());
}
