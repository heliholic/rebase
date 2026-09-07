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
#include <string.h>

#include "platform.h"

#include "common/axis.h"
#include "common/utils.h"

#include "config/config_reset.h"

#include "drivers/dshot_command.h"

#include "pg/pid.h"

#include "sensors/gyro.h"

#include "pid.h"


FAST_DATA_ZERO_INIT uint32_t targetPidLooptime;

FAST_DATA_ZERO_INIT pidAxisData_t pidData[XYZ_AXIS_COUNT];


void resetPidProfile(pidProfile_t *pidProfile)
{
    RESET_CONFIG(pidProfile_t, pidProfile,
        .profileName = { 0 },
    );
}

PG_RESET_FN(pidProfile_t, pidProfiles)
{
    for (int i = 0; i < PID_PROFILE_COUNT; i++) {
        resetPidProfile(&pidProfiles[i]);
    }
}

static void pidSetTargetLooptime(uint32_t pidLooptime)
{
    targetPidLooptime = pidLooptime;
#ifdef USE_DSHOT
    dshotSetPidLoopTime(targetPidLooptime);
#endif
}

void pidResetIterm(void)
{
    for (int axis = 0; axis < 3; axis++) {
        pidData[axis].I = 0.0f;
    }
}

void pidController(const pidProfile_t *pidProfile, timeUs_t currentTimeUs)
{
    UNUSED(pidProfile);
    UNUSED(currentTimeUs);
}

float pidGetPreviousSetpoint(int axis)
{
    UNUSED(axis);
    return 0;
}

float pidGetDT(void)
{
    return gyro.targetLooptime / 1000000.0f;
}

float pidGetPidFrequency(void)
{
    return 0;
}

void pidInitFilters(const pidProfile_t *pidProfile)
{
    UNUSED(pidProfile);
}

void pidInitConfig(const pidProfile_t *pidProfile)
{
    UNUSED(pidProfile);
}

void pidInit(const pidProfile_t *pidProfile)
{
    pidSetTargetLooptime(gyro.targetLooptime);
    pidInitFilters(pidProfile);
    pidInitConfig(pidProfile);
}

void pidCopyProfile(uint8_t dstPidProfileIndex, uint8_t srcPidProfileIndex)
{
    if (dstPidProfileIndex < PID_PROFILE_COUNT && srcPidProfileIndex < PID_PROFILE_COUNT
        && dstPidProfileIndex != srcPidProfileIndex) {
        memcpy(pidProfilesMutable(dstPidProfileIndex), pidProfilesMutable(srcPidProfileIndex), sizeof(pidProfile_t));
    }
}
