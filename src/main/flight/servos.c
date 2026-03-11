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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "platform.h"

#ifdef USE_SERVOS

#include "build/build_config.h"

#include "common/maths.h"

#include "config/config.h"
#include "config/config_reset.h"

#include "drivers/pwm_output.h"

#include "sensors/gyro.h"

#include "fc/runtime_config.h"

#include "rx/rx.h"

#include "flight/servos.h"
#include "flight/mixer.h"

#include "pg/servos.h"


static FAST_DATA_ZERO_INIT float    servoOutput[MAX_SUPPORTED_SERVOS];
static FAST_DATA_ZERO_INIT int16_t  servoOverride[MAX_SUPPORTED_SERVOS];


uint16_t getServoOutput(uint8_t servo)
{
    return (uint16_t)lrintf(servoOutput[servo]);
}

bool hasServoOverride(uint8_t servo)
{
    return (servoOverride[servo] >= SERVO_OVERRIDE_MIN && servoOverride[servo] <= SERVO_OVERRIDE_MAX);
}

int16_t getServoOverride(uint8_t servo)
{
    return servoOverride[servo];
}

int16_t setServoOverride(uint8_t servo, int16_t val)
{
    return servoOverride[servo] = val;
}

void validateAndFixServoConfig(void)
{
    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        servoParam_t *servo = servoParamsMutable(i);

        servo->mid = constrain(servo->mid, PWM_SERVO_PULSE_MIN, PWM_SERVO_PULSE_MAX);
        servo->min = constrain(servo->min, SERVO_LIMIT_MIN, 0);
        servo->max = constrain(servo->max, 0, SERVO_LIMIT_MAX);
        servo->rneg = constrain(servo->rneg, SERVO_SCALE_MIN, SERVO_SCALE_MAX);
        servo->rpos = constrain(servo->rpos, SERVO_SCALE_MIN, SERVO_SCALE_MAX);
        servo->rate = constrain(servo->rate, SERVO_RATE_MIN, SERVO_RATE_MAX);
        servo->speed = constrain(servo->speed, SERVO_SPEED_MIN, SERVO_SPEED_MAX);
        servo->flags &= SERVO_FLAGS_ALL;

        const int16_t min_allowed = (int16_t)PWM_SERVO_PULSE_MIN - (int16_t)servo->mid;
        const int16_t max_allowed = (int16_t)PWM_SERVO_PULSE_MAX - (int16_t)servo->mid;

        if (servo->min < min_allowed) {
            servo->min = min_allowed;
        }
        if (servo->max > max_allowed) {
            servo->max = max_allowed;
        }
    }
}

void servoInit(void)
{
    servoDevInit();

    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        servoOutput[i] = servoParams(i)->mid;
        servoOverride[i] = SERVO_OVERRIDE_OFF;
    }
}

void servoShutdown(void)
{
    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        servoWrite(i, 0);
    }
}

static inline float limitTravel(uint8_t servo, float pos, float min, float max)
{
    if (pos > max) {
        mixerSaturateServoOutput(servo);
        return max;
    } else if (pos < min) {
        mixerSaturateServoOutput(servo);
        return min;
    }
    return pos;
}

static inline float limitSpeed(float old, float new_pos, float speed)
{
    float rate = 1200.0f * gyro.targetLooptime / (speed * 1000000.0f);
    float diff = new_pos - old;

    if (diff > rate)
        return old + rate;
    else if (diff < -rate)
        return old - rate;

    return new_pos;
}

void servoUpdate(void)
{
    const int servoCount = getServoCount();

    for (int i = 0; i < servoCount; i++)
    {
        const servoParam_t *servo = servoParams(i);
        float pos;

        if (!ARMING_FLAG(ARMED) && hasServoOverride(i))
            pos = servoOverride[i] / 1000.0f;
        else
            pos = mixerGetServoOutput(i);

        if (servo->flags & SERVO_FLAG_REVERSED)
            pos = -pos;

        {
            float scale = (pos >= 0) ? (float)servo->rpos : (float)servo->rneg;
            pos = limitTravel(i, scale * pos, servo->min, servo->max);
        }
        pos = servo->mid + pos;

        if (servo->speed > 0)
            pos = limitSpeed(servoOutput[i], pos, servo->speed);

        servoOutput[i] = pos;

        servoWrite(i, pos);
    }
}

#endif
