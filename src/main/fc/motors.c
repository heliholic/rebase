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

#include "build/build_config.h"
#include "build/debug.h"

#include "common/maths.h"
#include "common/utils.h"

#include "config/feature.h"
#include "config/config.h"

#include "pg/motor.h"

#include "drivers/pwm_output.h"
#include "drivers/dshot_command.h"
#include "drivers/dshot.h"
#include "drivers/motor.h"
#include "drivers/time.h"
#include "drivers/io.h"

#include "fc/runtime_config.h"
#include "fc/rc_controls.h"
#include "fc/rc_modes.h"
#include "fc/core.h"
#include "fc/rc.h"
#include "fc/motors.h"

#include "flight/mixer.h"
#include "flight/pid.h"


static FAST_DATA_ZERO_INIT timeUs_t motorOverrideTimeout;

static FAST_DATA_ZERO_INIT float    motorOutput[MAX_SUPPORTED_MOTORS];
static FAST_DATA_ZERO_INIT float    motorOverride[MAX_SUPPORTED_MOTORS];


/*** Access functions ***/

uint8_t getMotorCount(void)
{
    return MAX_SUPPORTED_MOTORS;
}

int getMotorOutput(uint8_t motor)
{
    return lrintf(motorOutput[motor] * 1000);
}

int getMotorOverride(uint8_t motor)
{
    return isfinite(motorOverride[motor]) ? lrintf(motorOverride[motor] * 1000) : MOTOR_OVERRIDE_OFF;
}

int setMotorOverride(uint8_t motor, int value, timeDelta_t timeout)
{
    if (!ARMING_FLAG(ARMED) && motor < MAX_SUPPORTED_MOTORS &&
        value >= MOTOR_OVERRIDE_MIN && value <= MOTOR_OVERRIDE_MAX) {
        motorOverride[motor] = value / 1000.0f;
        motorOverrideTimeout = timeout ? (micros() + timeout) | BIT(0) : 0;
        return value;
    }
    else {
        motorOverride[motor] = NAN;
    }
    return MOTOR_OVERRIDE_OFF;
}

bool hasMotorOverride(uint8_t motor)
{
    return isfinite(motorOverride[motor]);
}

void resetMotorOverride(void)
{
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++)
        motorOverride[i] = NAN;

    motorOverrideTimeout = 0;
}

bool areMotorsRunning(void)
{
    if (ARMING_FLAG(ARMED))
        return true;

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++)
        if (motorOutput[i] >= 0)
            return true;

    return false;
}


/*** Init functions ***/

void INIT_CODE motorInit(void)
{
    motorDevInit();
}

void motorStop(void)
{
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++)
        motorOutput[i] = 0;

    motorWriteAll(motorOutput);

    delay(50);
}

void motorUpdate(timeUs_t currentTimeUs)
{
    if (ARMING_FLAG(ARMED)) {
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++)
            motorOutput[i] = constrainf(mixerGetMotorOutput(i), 0, 1);
    }
    else {
        if (motorOverrideTimeout) {
            if (cmp32(currentTimeUs, motorOverrideTimeout) > 0)
                resetMotorOverride();
        }
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++)
            motorOutput[i] = slewUpLimit(motorOutput[i],
                constrain(motorOverride[i], MOTOR_OVERRIDE_MIN, MOTOR_OVERRIDE_MAX) / 1000.0f,
                MOTOR_OVERRIDE_RATE * pidGetDT());
    }

    motorWriteAll(motorOutput);
}
