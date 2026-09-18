/*
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software. If not, see <https://www.gnu.org/licenses/>.
 */

#include "platform.h"
#include "common/utils.h"
#include "fc/motors.h"
#include "flight/mixer.h"

void motorStop(void)
{
}

int getMotorOutput(uint8_t motor)
{
    UNUSED(motor);
    return 0;
}

int getMotorOverride(uint8_t motor)
{
    UNUSED(motor);
    return MOTOR_OVERRIDE_OFF;
}

int setMotorOverride(uint8_t motor, int value, timeDelta_t timeout)
{
    UNUSED(motor);
    UNUSED(timeout);
    return value;
}

bool hasMotorOverride(uint8_t motor)
{
    UNUSED(motor);
    return false;
}

void resetMotorOverride(void)
{
}
