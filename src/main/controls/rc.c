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
#include <math.h>

#include "platform.h"

#include "build/debug.h"

#include "common/axis.h"
#include "common/maths.h"
#include "common/utils.h"

#include "pg/rc_controls.h"
#include "pg/rx.h"

#include "rx/rx.h"

#include "rc.h"


#define RX_REFRESH_RATE_MIN_US          490
#define RX_REFRESH_RATE_MAX_US        40000

#define RX_REFRESH_RATE_AVERAGING        64

#define RX_RANGE_COUNT                    4

#define RC_THROTTLE_DEADBAND              5


// Legacy rcCommand for direct access
FAST_DATA_ZERO_INIT float rcCommand[RC_CONTROL_COUNT];

// Internal data
typedef struct {

    float       deflection[4];          // RPYC
    float       deadband[4];
    float       center[4];
    float       range[4];

    float       throttle;
    float       offThrottle;
    float       minThrottle;
    float       maxThrottle;

    uint        currentRxRefreshMult;
    uint        currentRxRefreshRate;
    float       averageRxRefreshRate;

    uint        lastRcValue;
    timeUs_t    lastRxTimeUs;

    uint        changeCount;
    uint        repeatCount;
    uint        repeatRange[RX_RANGE_COUNT];

} rcData_t;

static FAST_DATA_ZERO_INIT rcData_t rc;


float getRcDeflection(int axis)
{
    return rc.deflection[axis];
}

float getThrottle(void)
{
    return rc.throttle;
}

bool isThrottleOff(void)
{
    return (rcControls[RC_THROTTLE] < rc.offThrottle);
}

uint16_t getFailsafeThrottle(void)
{
    return rc.offThrottle - RC_THROTTLE_DEADBAND;
}

uint16_t getFailsafeCenter(void)
{
    return rcControlsConfig()->rc_center;
}

uint getCurrentRxRefreshRate(void)
{
    return rc.currentRxRefreshRate;
}

float getAverageRxRefreshRate(void)
{
    return rc.averageRxRefreshRate;
}


/*
 * Detect the Rx frames that carry repeated data, so that the true rate at
 * which the transmitter updates the channels can be told apart from the
 * rate at which the link delivers frames.
 */
static void updateRcChange(void)
{
    const uint rcValue = lrintf(rcCommand[FD_ROLL]);

    if (rcValue == rc.lastRcValue) {
        rc.repeatCount++;
    }
    else {
        if (rc.repeatCount < RX_RANGE_COUNT) {
            rc.repeatRange[rc.repeatCount]++;
            rc.changeCount++;
        }
        rc.repeatCount = 0;
        rc.lastRcValue = rcValue;
    }

    const uint rangeLimit = constrain(rc.changeCount / RX_RANGE_COUNT, 10, 1000);
    for (int i = 0; i < RX_RANGE_COUNT; i++) {
        if (rc.repeatRange[i] > rangeLimit) {
            rc.currentRxRefreshMult = i + 1;
            break;
        }
    }

    DEBUG_VAL(RX_TIMING, 7, rc.currentRxRefreshMult);
}

void updateRcRefreshRate(timeUs_t currentTimeUs)
{
    timeDelta_t frameAgeUs = 0;
    timeDelta_t frameDeltaUs = rxGetFrameDelta(&frameAgeUs);
    timeDelta_t localDeltaUs = cmpTimeUs(currentTimeUs, rc.lastRxTimeUs);

    rc.lastRxTimeUs = currentTimeUs;

    DEBUG_VAL(RX_TIMING, 4, frameDeltaUs);
    DEBUG_VAL(RX_TIMING, 5, localDeltaUs);
    DEBUG_VAL(RX_TIMING, 6, frameAgeUs);

    if (frameDeltaUs == 0 || localDeltaUs <= frameAgeUs) {
        frameDeltaUs = localDeltaUs;
    }

    rc.currentRxRefreshRate = frameDeltaUs;

    if (isRxReceivingSignal()) {
        if (rc.currentRxRefreshRate > RX_REFRESH_RATE_MIN_US && rc.currentRxRefreshRate < RX_REFRESH_RATE_MAX_US) {
            rc.averageRxRefreshRate += (frameDeltaUs - rc.averageRxRefreshRate) / RX_REFRESH_RATE_AVERAGING;
            updateRcChange();
        }
    }

    DEBUG_VAL(RX_TIMING, 0, rc.averageRxRefreshRate);
    DEBUG_VAL(RX_TIMING, 1, rc.averageRxRefreshRate * rc.currentRxRefreshMult);
    DEBUG_VAL(RX_TIMING, 2, rc.currentRxRefreshRate);
}


void updateRcCommands(void)
{
    // rcControls => rc.deflection => rcCommand
    for (int axis = 0; axis < 4; axis++) {
        // Center point
        float data = rcControls[axis] - rc.center[axis];

        // Apply deadband
        data = fapplyDeadband(data, rc.deadband[axis]);

        // Deflection range is -1..1
        rc.deflection[axis] = constrainf(data / rc.range[axis], -1, 1);

        // Legacy rcCommand range is -500..500
        rcCommand[axis] = rc.deflection[axis] * 500;

        DEBUG_VAL(RC_COMMAND, axis, rcCommand[axis]);
    }

    // Throttle range is 0..1
    rc.throttle = transition(rcControls[RC_THROTTLE], rc.minThrottle, rc.maxThrottle, 0, 1);

    // Legacy throttle command range is -500..500 for compatibility with other channels
    rcCommand[RC_THROTTLE] = rc.throttle * 1000 - 500;

    DEBUG_VAL(RC_COMMAND, RC_THROTTLE, lrintf(rc.throttle * 1000));
}

INIT_CODE void initRcProcessing(void)
{
    rc.deadband[FD_ROLL] = rcControlsConfig()->rc_deadband;
    rc.deadband[FD_PITCH] = rcControlsConfig()->rc_deadband;
    rc.deadband[FD_YAW] = rcControlsConfig()->rc_yaw_deadband;
    rc.deadband[FD_COLL] = 0;

    for (int axis = 0; axis < 4; axis++) {
        rc.center[axis] = rcControlsConfig()->rc_center;
        rc.range[axis] = rcControlsConfig()->rc_deflection - rc.deadband[axis];
    }

    if (rcControlsConfig()->rc_min_throttle)
        rc.minThrottle = constrain(rcControlsConfig()->rc_min_throttle, RX_PWM_PULSE_MIN, RX_PWM_PULSE_MAX);
    else
        rc.minThrottle = rcControlsConfig()->rc_center - rcControlsConfig()->rc_deflection * 0.9f;

    if (rcControlsConfig()->rc_max_throttle)
        rc.maxThrottle = constrain(rcControlsConfig()->rc_max_throttle, RX_PWM_PULSE_MIN, RX_PWM_PULSE_MAX);
    else
        rc.maxThrottle = rcControlsConfig()->rc_center + rcControlsConfig()->rc_deflection * 0.9f;

    rc.maxThrottle = fmaxf(rc.maxThrottle, rc.minThrottle + 10);

    rc.offThrottle = rc.minThrottle - RC_THROTTLE_DEADBAND;

    // Start with 100Hz rate
    rc.averageRxRefreshRate = 10000;
    rc.currentRxRefreshMult = 1;
}
