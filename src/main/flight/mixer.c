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

#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "platform.h"

#include "common/utils.h"

#include "drivers/motor.h"

#include "mixer.h"


float FAST_DATA_ZERO_INIT motor[MAX_SUPPORTED_MOTORS];

float motor_disarmed[MAX_SUPPORTED_MOTORS];


void writeMotors(void)
{
}

void stopMotors(void)
{
}

float getMotorOutputLow(void)
{
    return 0;
}

float getMotorOutputHigh(void)
{
    return 1;
}

bool areMotorsRunning(void)
{
    return true;
}

uint8_t getMotorCount(void)
{
    return MAX_SUPPORTED_MOTORS;
}

float mixerGetThrottle(void)
{
    return 0;
}

float mixerGetServoOutput(uint8_t servo)
{
    UNUSED(servo);
    return 0;
}

void mixerSaturateServoOutput(uint8_t servo)
{
    UNUSED(servo);
}

bool mixerIsCyclicServo(uint8_t index)
{
    UNUSED(index);
    return false;
}

void mixerInit(void)
{
}

