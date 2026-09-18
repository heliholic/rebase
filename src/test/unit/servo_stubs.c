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

#include "platform.h"

#include "common/utils.h"

#include "fc/servos.h"
#include "pg/servo.h"

void motorStop(void)
{
}

int getServoCount(void)
{
    return MAX_SUPPORTED_SERVOS;
}

void servoShutdown(void)
{
}

int getServoOutput(uint8_t servo)
{
    UNUSED(servo);
    return DEFAULT_SERVO_CENTER;
}

int getServoOverride(uint8_t servo)
{
    UNUSED(servo);
    return SERVO_OVERRIDE_OFF;
}

int setServoOverride(uint8_t servo, int val)
{
    UNUSED(servo);
    return val;
}

bool hasServoOverride(uint8_t servo)
{
    UNUSED(servo);
    return false;
}
