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
#include <math.h>

#include "platform.h"

#include "common/axis.h"
#include "common/maths.h"
#include "common/utils.h"

#include "config/config.h"

#include "pg/rates.h"

#include "rx/rx.h"

#include "rates.h"


FAST_DATA_ZERO_INIT rateProfile_t * currentRateProfile;


/*
 * The rate curve is a variable order expo, blending a linear response with a
 * curve of the configured order. The rate is the deflection limit in deg/s.
 *
 *      expof = |x| * (1 - expo) + |x|^curve * expo
 *      rate  = maxrate * expof
 */

float applyRatesCurve(const int axis, const float rcCommandf)
{
    const float rcCommandAbs = fabsf(rcCommandf);

    const float maxRate = currentRateProfile->rates[axis];
    const float expo = currentRateProfile->expo[axis] / 100.0f;
    const float curve = currentRateProfile->curve[axis] / 16.0f + 2.0f;

    // Variable order expo
    const float expof = rcCommandAbs * (1.0f - expo) + pow_approx(rcCommandAbs, curve) * expo;

    float rate = fminf(maxRate * expof, SETPOINT_RATE_LIMIT);

    rate = copysignf(rate, rcCommandf);

    // Collective is an angle - scale it here so that 480deg/s => 12deg
    if (axis == FD_COLL)
        rate *= 2.083333333f;

    return rate;
}


INIT_CODE void loadRateProfile(void)
{
    currentRateProfile = rateProfilesMutable(systemConfig()->activeRateProfile);
}

INIT_CODE void changeRateProfile(uint8_t rateProfileIndex)
{
    if (rateProfileIndex < RATE_PROFILE_COUNT) {
        systemConfigMutable()->activeRateProfile = rateProfileIndex;
    }

    loadRateProfile();
}

INIT_CODE void copyRateProfile(uint8_t dstRateProfileIndex, uint8_t srcRateProfileIndex)
{
    if (dstRateProfileIndex < RATE_PROFILE_COUNT &&
        srcRateProfileIndex < RATE_PROFILE_COUNT &&
        dstRateProfileIndex != srcRateProfileIndex)
    {
        memcpy(rateProfilesMutable(dstRateProfileIndex),
               rateProfiles(srcRateProfileIndex), sizeof(rateProfile_t));
    }
}


/*
 * Adjustment functions. The rate profile index is one-based on the wire, so
 * that zero can mean "no change" for the configurator.
 */

ADJUSTMENT_GET(RATE_PROFILE)
{
    return getCurrentRateProfileIndex() + 1;
}

ADJUSTMENT_SET(RATE_PROFILE)
{
    changeRateProfile(value - 1);
}

ADJUSTMENT_GET(PITCH_RATE)
{
    return currentRateProfile->rates[FD_PITCH];
}

ADJUSTMENT_SET(PITCH_RATE)
{
    currentRateProfile->rates[FD_PITCH] = constrain(value, 0, RATE_MAX);
}

ADJUSTMENT_GET(PITCH_CURVE)
{
    return currentRateProfile->curve[FD_PITCH];
}

ADJUSTMENT_SET(PITCH_CURVE)
{
    currentRateProfile->curve[FD_PITCH] = constrain(value, 0, CURVE_MAX);
}

ADJUSTMENT_GET(PITCH_EXPO)
{
    return currentRateProfile->expo[FD_PITCH];
}

ADJUSTMENT_SET(PITCH_EXPO)
{
    currentRateProfile->expo[FD_PITCH] = constrain(value, 0, EXPO_MAX);
}

ADJUSTMENT_GET(ROLL_RATE)
{
    return currentRateProfile->rates[FD_ROLL];
}

ADJUSTMENT_SET(ROLL_RATE)
{
    currentRateProfile->rates[FD_ROLL] = constrain(value, 0, RATE_MAX);
}

ADJUSTMENT_GET(ROLL_CURVE)
{
    return currentRateProfile->curve[FD_ROLL];
}

ADJUSTMENT_SET(ROLL_CURVE)
{
    currentRateProfile->curve[FD_ROLL] = constrain(value, 0, CURVE_MAX);
}

ADJUSTMENT_GET(ROLL_EXPO)
{
    return currentRateProfile->expo[FD_ROLL];
}

ADJUSTMENT_SET(ROLL_EXPO)
{
    currentRateProfile->expo[FD_ROLL] = constrain(value, 0, EXPO_MAX);
}

ADJUSTMENT_GET(YAW_RATE)
{
    return currentRateProfile->rates[FD_YAW];
}

ADJUSTMENT_SET(YAW_RATE)
{
    currentRateProfile->rates[FD_YAW] = constrain(value, 0, RATE_MAX);
}

ADJUSTMENT_GET(YAW_CURVE)
{
    return currentRateProfile->curve[FD_YAW];
}

ADJUSTMENT_SET(YAW_CURVE)
{
    currentRateProfile->curve[FD_YAW] = constrain(value, 0, CURVE_MAX);
}

ADJUSTMENT_GET(YAW_EXPO)
{
    return currentRateProfile->expo[FD_YAW];
}

ADJUSTMENT_SET(YAW_EXPO)
{
    currentRateProfile->expo[FD_YAW] = constrain(value, 0, EXPO_MAX);
}

ADJUSTMENT_GET(COLL_RATE)
{
    return currentRateProfile->rates[FD_COLL];
}

ADJUSTMENT_SET(COLL_RATE)
{
    currentRateProfile->rates[FD_COLL] = constrain(value, 0, RATE_MAX);
}

ADJUSTMENT_GET(COLL_CURVE)
{
    return currentRateProfile->curve[FD_COLL];
}

ADJUSTMENT_SET(COLL_CURVE)
{
    currentRateProfile->curve[FD_COLL] = constrain(value, 0, CURVE_MAX);
}

ADJUSTMENT_GET(COLL_EXPO)
{
    return currentRateProfile->expo[FD_COLL];
}

ADJUSTMENT_SET(COLL_EXPO)
{
    currentRateProfile->expo[FD_COLL] = constrain(value, 0, EXPO_MAX);
}
