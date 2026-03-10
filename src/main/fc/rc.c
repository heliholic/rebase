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
#include <math.h>

#include "platform.h"

#include "build/debug.h"

#include "common/utils.h"
#include "common/vector.h"

#include "config/config.h"
#include "config/feature.h"

#include "fc/rc_rates.h"
#include "fc/core.h"
#include "fc/rc.h"
#include "fc/rc_controls.h"
#include "fc/rc_modes.h"
#include "fc/runtime_config.h"

#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/gps_rescue.h"
#include "flight/pid.h"

#include "pg/rx.h"
#include "rx/rx.h"

#include "sensors/battery.h"
#include "sensors/gyro.h"

#include "rc.h"

#define RX_INTERVAL_MIN_US     800 // 0.800ms to fit 1kHz without an issue often 1khz rc comes in at 880us or so
#define RX_INTERVAL_MAX_US   65500 // 65.5ms or 15.26hz

static float rawSetpoint[4];
static float setpointRate[4];
static float rcDeflection[4], rcDeflectionAbs[4];
static float maxRcDeflectionAbs;

static uint16_t currentRxIntervalUs;  // packet interval in microseconds, constrained to above range
static uint16_t previousRxIntervalUs; // previous packet interval in microseconds
static float currentRxRateHz;         // packet interval in Hz, constrained as above
static float smoothedRxRateHz = 100.0f;

static bool isRxDataNew = false;
static bool isRxRateValid = false;
static float rcCommandDivider = 500.0f;
static float rcCommandYawDivider = 500.0f;

float getSetpointRate(int axis)
{
    return rawSetpoint[axis];
}

static float maxRcRate[3];
float getMaxRcRate(int axis)
{
    return maxRcRate[axis];
}

float getRcDeflection(int axis)
{
    return rcDeflection[axis];
}

float getRcDeflectionRaw(int axis)
{
    return rcDeflection[axis];
}

float getRcDeflectionAbs(int axis)
{
    return rcDeflectionAbs[axis];
}

float getMaxRcDeflectionAbs(void)
{
    return maxRcDeflectionAbs;
}

void updateRcRefreshRate(timeUs_t currentTimeUs, bool rxReceivingSignal)
{
    // this function runs from processRx in core.c
    // rxReceivingSignal is true:
    // - every time a new frame is detected,
    // - if we stop getting data, at the expiry of RXLOSS_TRIGGER_INTERVAL since the last good frame
    // - if that interval is exceeded and still no data, every RX_FRAME_RECHECK_INTERVAL, until a new frame is detected
    static timeUs_t lastRxTimeUs = 0;
    timeDelta_t delta = 0;

    if (rxReceivingSignal) { // true while receiving data and until RXLOSS_TRIGGER_INTERVAL expires, otherwise false
        previousRxIntervalUs = currentRxIntervalUs;
        // use driver rx time if available, current time otherwise
        const timeUs_t rxTime = rxRuntimeState.lastRcFrameTimeUs ? rxRuntimeState.lastRcFrameTimeUs : currentTimeUs;

        if (lastRxTimeUs) {  // report delta only if previous time is available
            delta = cmpTimeUs(rxTime, lastRxTimeUs);
        }
        lastRxTimeUs = rxTime;
        DEBUG_SET(DEBUG_RX_TIMING, 1, rxTime / 100);  // packet time stamp in tenths of ms
    } else {
        if (lastRxTimeUs) {
            // no packet received, use current time for delta
            delta = cmpTimeUs(currentTimeUs, lastRxTimeUs);
        }
    }

    // constrain to a frequency range no lower than about 15Hz and up to about 1000Hz
    // these intervals and rates will be used for RCSmoothing, Feedforward, etc.
    currentRxIntervalUs = constrain(delta, RX_INTERVAL_MIN_US, RX_INTERVAL_MAX_US);
    currentRxRateHz = 1e6f / currentRxIntervalUs;
    isRxRateValid = delta == currentRxIntervalUs; // delta is not constrained, therefore not outside limits

    DEBUG_SET(DEBUG_RX_TIMING, 0, MIN(delta / 10, INT16_MAX));   // packet interval in hundredths of ms
    DEBUG_SET(DEBUG_RX_TIMING, 2, isRxRateValid);
    DEBUG_SET(DEBUG_RX_TIMING, 3, MIN(currentRxIntervalUs / 10, INT16_MAX));  // constrained packet interval, tenths of ms
    DEBUG_SET(DEBUG_RX_TIMING, 4, lrintf(currentRxRateHz));
    // temporary debugs
#ifdef USE_RX_LINK_QUALITY_INFO
    DEBUG_SET(DEBUG_RX_TIMING, 6, rxGetLinkQualityPercent());    // raw link quality value
#endif
    DEBUG_SET(DEBUG_RX_TIMING, 7, isRxReceivingSignal());        // flag to initiate RXLOSS signal and Stage 1 values
}

// currently only used in the CLI
float getCurrentRxRateHz(void)
{
    return smoothedRxRateHz;
}

bool getRxRateValid(void)
{
    return isRxRateValid;
}

bool shouldUpdateSmoothing(void)
{
    static int validCount = 0;
    static int outlierCount = 0;
    static const float smoothingFactor = 0.1f;       // Low pass smoothing factor to smooth valid RxRate values
    static int8_t prevOutlierSign = 0;               // -1 for negative, +1 for positive

    if (isRxReceivingSignal() && isRxRateValid) {
        float deltaRateHz = currentRxRateHz - smoothedRxRateHz;
        bool isOutlier = fabsf(deltaRateHz) > (smoothedRxRateHz * 0.2f);

        if (isOutlier) {
            const int8_t currentSign = (deltaRateHz < 0.0f) ? -1 : 1;
            if (outlierCount == 0) {
                prevOutlierSign = currentSign;
                outlierCount++;
            } else {
                if (currentSign != prevOutlierSign) {
                    // Reset outlier count if outlier sign reverses, as often happens at 1000Hz
                    // with a true change, all new packet delta will have the same sign.
                    outlierCount = 0;
                    prevOutlierSign = currentSign;
                } else {
                    outlierCount++;
                }
            }
            validCount = 0;
        } else {
            // First-order smoothing toward new value for non-outliers
            smoothedRxRateHz += smoothingFactor * (currentRxRateHz - smoothedRxRateHz);
            validCount++;
            outlierCount = 0;
        }

        if (validCount >= 3) {
            validCount = 0;
            // indicate that filter cutoffs should be updated to smoothedRxRateHz
            return true;
        }
        if (outlierCount >= 3) {
            // Link rate likely changed — snap smoothing accumulator to current value
            smoothedRxRateHz = currentRxRateHz;
            outlierCount = 0;
        }
    } else {
        // Signal lost or invalid widths, reset counts, but hold last stable and smoothed values
        validCount = 0;
        outlierCount = 0;
    }
    return false;
}

FAST_CODE void processRcCommand(void)
{
    if (isRxDataNew) {
        maxRcDeflectionAbs = 0.0f;

        for (int axis = FD_ROLL; axis <= FD_YAW; axis++) {

            float angleRate;

#ifdef USE_GPS_RESCUE
            if ((axis == FD_YAW) && FLIGHT_MODE(GPS_RESCUE_MODE)) {
                // If GPS Rescue is active then override the setpointRate used in the
                // pid controller with the value calculated from the desired heading logic.
                angleRate = gpsRescueGetYawRate();
                angleRate = constrainf(angleRate, -SETPOINT_RATE_LIMIT, SETPOINT_RATE_LIMIT);
                // Treat the stick input as centered to avoid any stick deflection base modifications (like acceleration limit)
                rcDeflection[axis] = 0;
                rcDeflectionAbs[axis] = 0;
            } else
#endif
            {
                // scale rcCommandf to range [-1.0, 1.0]
                float rcCommandf;
                if (axis == FD_YAW) {
                    rcCommandf = rcCommand[axis] / rcCommandYawDivider;
                } else {
                    rcCommandf = rcCommand[axis] / rcCommandDivider;
                }
                rcDeflection[axis] = rcCommandf;
                rcDeflectionAbs[axis] = fabsf(rcCommandf);
                maxRcDeflectionAbs = fmaxf(maxRcDeflectionAbs, rcDeflectionAbs[axis]);

                angleRate = applyRatesCurve(axis, rcCommandf);
            }

            rawSetpoint[axis] = angleRate;
            DEBUG_SET(DEBUG_ANGLERATE, axis, angleRate);

            // log the smoothed Rx Rate from non-outliers, this will not show the steps every three valid packets
            DEBUG_SET(DEBUG_RX_TIMING, 5, lrintf(smoothedRxRateHz));
        }
    }

    isRxDataNew = false;
}

FAST_CODE_NOINLINE void updateRcCommands(void)
{
    isRxDataNew = true;

    for (int axis = 0; axis < 4; axis++) {
        float rc = constrainf(rcData[axis] - rxConfig()->midrc, -500.0f, 500.0f); // -500 to 500
        float deadband = 0;
        if (axis == ROLL || axis == PITCH) {
            deadband = rcControlsConfig()->deadband;
        } else if (axis == YAW) {
            deadband  = rcControlsConfig()->yaw_deadband;
            rc = -rc;  // Yaw direction reversed
        }
        rcCommand[axis] = fapplyDeadband(rc, deadband);
    }

    rcCommand[THROTTLE] = constrain(rcData[THROTTLE], PWM_RANGE_MIN, PWM_RANGE_MAX) - PWM_RANGE_MIN;
}

void resetYawAxis(void)
{
    rcCommand[YAW] = 0;
    setpointRate[YAW] = 0;
}

void initRcProcessing(void)
{
    rcCommandDivider = 500.0f - rcControlsConfig()->deadband;
    rcCommandYawDivider = 500.0f - rcControlsConfig()->yaw_deadband;

    loadControlRateProfile();

    for (int i = 0; i < XYZ_AXIS_COUNT; i++) {
        maxRcRate[i] = applyRatesCurve(i, 1.0f);
    }
}
