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

#include "fc/runtime_config.h"

#include "rx/rx.h"

#include "fc/servos.h"
#include "flight/mixer.h"
#include "flight/pid.h"

#include "pg/servo.h"


static FAST_DATA_ZERO_INIT float servoInput[MAX_SUPPORTED_SERVOS];
static FAST_DATA_ZERO_INIT float servoOutput[MAX_SUPPORTED_SERVOS];
static FAST_DATA_ZERO_INIT float servoOverride[MAX_SUPPORTED_SERVOS];


int getServoOutput(uint8_t servo)
{
    return lrintf(servoOutput[servo]);
}

bool hasServoOverride(uint8_t servo)
{
    return isfinite(servoOverride[servo]);
}

int getServoOverride(uint8_t servo)
{
    return isfinite(servoOverride[servo]) ? lrintf(servoOverride[servo] * 1000) : SERVO_OVERRIDE_OFF;
}

int setServoOverride(uint8_t servo, int val)
{
    return servoOverride[servo] = (val < SERVO_OVERRIDE_MIN || val > SERVO_OVERRIDE_MAX) ? NAN : val / 1000.0f;
}

void INIT_CODE validateAndFixServoConfig(void)
{
    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        servoParam_t *servo = servoParamsMutable(i);

        servo->mid   = constrain(servo->mid, PWM_SERVO_PULSE_MIN, PWM_SERVO_PULSE_MAX);
        servo->min   = constrain(servo->min, SERVO_LIMIT_MIN, 0);
        servo->max   = constrain(servo->max, 0, SERVO_LIMIT_MAX);
        servo->rneg  = constrain(servo->rneg, SERVO_SCALE_MIN, SERVO_SCALE_MAX);
        servo->rpos  = constrain(servo->rpos, SERVO_SCALE_MIN, SERVO_SCALE_MAX);
        servo->rate  = constrain(servo->rate, SERVO_RATE_MIN, SERVO_RATE_MAX);
        servo->speed = constrain(servo->speed, SERVO_SPEED_MIN, SERVO_SPEED_MAX);

        servo->flags &= SERVO_FLAGS_ALL;

        const int min_allowed = PWM_SERVO_PULSE_MIN - servo->mid;
        const int max_allowed = PWM_SERVO_PULSE_MAX - servo->mid;

        if (servo->min < min_allowed) {
            servo->min = min_allowed;
        }
        if (servo->max > max_allowed) {
            servo->max = max_allowed;
        }
    }
}

void INIT_CODE servoInit(void)
{
    servoDevInit();

    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        servoInput[i] = 0;
        servoOutput[i] = servoParams(i)->mid;
        servoOverride[i] = NAN;
    }
}

void INIT_CODE servoShutdown(void)
{
    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        servoWrite(i, 0);
    }
}

static inline float servoSaturate(uint8_t servo, float pos, float min, float max)
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

static inline float servoLimitSpeed(float old, float new_pos, float speed)
{
    float rate = 1200.0f * pidGetDT() / speed;
    float diff = new_pos - old;

    if (diff > rate)
        return old + rate;
    else if (diff < -rate)
        return old - rate;

    return new_pos;
}

static inline float servoLimitRatio(float old, float new_pos, float ratio)
{
    return old + (new_pos - old) * ratio;
}

static float servoGeometryCorrection(float pos)
{
    // 1.0 == 50° without correction
    float height = constrainf(pos * 0.7660444431f, -1.0f, 1.0f);

    // Scale 50° in rad => 1.0
    float rotation = asin_approx(height) * 1.14591559026f;

    return rotation;
}

void servoUpdate(void)
{
    const int servoCount = getServoCount();
    float input[MAX_SUPPORTED_SERVOS];
    float cyclic_limit = 1.0f;

    for (int i = 0; i < servoCount; i++) {
        const servoParam_t *servo = servoParams(i);

        if (!ARMING_FLAG(ARMED) && hasServoOverride(i))
            input[i] = servoOverride[i];
        else
            input[i] = mixerGetServoOutput(i);

        if (servo->flags & SERVO_FLAG_GEO_CORR)
            input[i] = servoGeometryCorrection(input[i]);

        if (servo->speed > 0 && mixerIsCyclicServo(i)) {
            const float limit = 1200.0f * pidGetDT() / servo->speed;
            const float delta = fabsf(input[i] - servoInput[i]);
            cyclic_limit = fminf(cyclic_limit, limit / delta);
        }
    }

    for (int i = 0; i < servoCount; i++) {
        const servoParam_t *servo = servoParams(i);
        float pos = input[i];

        if (servo->speed > 0) {
            if (mixerIsCyclicServo(i))
                pos = servoLimitRatio(servoInput[i], pos, cyclic_limit);
            else
                pos = servoLimitSpeed(servoInput[i], pos, servo->speed);
        }

        servoInput[i] = pos;

        if (servo->flags & SERVO_FLAG_REVERSED)
            pos = -pos;

        float scale = (pos > 0) ? servo->rpos : servo->rneg;

        pos = servoSaturate(i, scale * pos, servo->min, servo->max);
        pos = servo->mid + pos;

        servoOutput[i] = pos;

        servoWrite(i, pos);
    }
}

#endif
