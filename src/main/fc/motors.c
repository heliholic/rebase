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

#include "config/config.h"

#include "fc/feature.h"

#include "controls/modes.h"

#include "pg/motor.h"

#include "drivers/pwm_output.h"
#include "drivers/dshot_command.h"
#include "drivers/dshot.h"
#include "drivers/motor.h"
#include "drivers/time.h"
#include "drivers/io.h"

#include "fc/runtime_config.h"
#include "fc/core.h"
#include "controls/rc.h"
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
    return (motorOutput[motor] < 0) ? MOTOR_STOP : lrintf(motorOutput[motor] * 1000);
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

void INIT_CODE validateAndFixMotorConfig(void)
{
    bool proshot = false;

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        motorConfig_t *motor = motorConfigMutable(i);

        if (motor->protocol >= MOTOR_PROTOCOL_COUNT) {
            motor->protocol = MOTOR_PROTOCOL_DISABLED;
        }

        const bool dshot = motorProtocolIsDshot(motor->protocol);

        // Telemetry rides on DShot, and extended telemetry on top of that
        motor->flags &= MOTOR_FLAGS_ALL;
        if (motor->flags & MOTOR_FLAG_EDT_FORCE) {
            motor->flags |= MOTOR_FLAG_EDT;
        }
        // Whether the reply can be captured depends on the pin and driver,
        // which the motor device checks when it sets the motors up
#ifndef USE_DSHOT_TELEMETRY
        motor->flags &= ~MOTOR_FLAG_BIDIR;
#endif
        if (!dshot || !(motor->flags & MOTOR_FLAG_BIDIR)) {
            motor->flags &= ~(MOTOR_FLAG_BIDIR | MOTOR_FLAG_EDT | MOTOR_FLAG_EDT_FORCE);
        }

        const uint16_t pulseMin = motorProtocolMinPulse(motor->protocol);
        const uint16_t pulseMax = motorProtocolMaxPulse(motor->protocol);

        // The stop pulse may sit anywhere, usually at or below zero throttle
        motor->stopPwmPulse = constrain(motor->stopPwmPulse, pulseMin, pulseMax);
        motor->zeroPwmPulse = constrain(motor->zeroPwmPulse, pulseMin, pulseMax);
        motor->fullPwmPulse = constrain(motor->fullPwmPulse, pulseMin, pulseMax);
        if (motor->zeroPwmPulse >= motor->fullPwmPulse) {
            motor->zeroPwmPulse = constrain(DEFAULT_MOTOR_ZERO_PULSE, pulseMin, pulseMax);
            motor->fullPwmPulse = constrain(DEFAULT_MOTOR_FULL_PULSE, pulseMin, pulseMax);
        }

        // 0 leaves the rate to the protocol
        if (motor->rate) {
            const uint16_t rateMax = motorProtocolMaxRate(motor->protocol, motor->flags & MOTOR_FLAG_BIDIR);
            motor->rate = constrain(motor->rate, MIN(MOTOR_RATE_MIN, rateMax), rateMax);
        }

        // Poles come in pairs
        motor->poles &= ~1;

        motor->kv = MIN(motor->kv, MOTOR_KV_MAX);

        if (motor->protocol == MOTOR_PROTOCOL_PROSHOT1000) {
            proshot = true;
        }
    }

#ifdef USE_DSHOT_BITBANG
    // Bitbang has no ProShot timing
    if (proshot && motorDriverConfig()->useDshotBitbang == DSHOT_BITBANG_ON) {
        motorDriverConfigMutable()->useDshotBitbang = DSHOT_BITBANG_AUTO;
    }
#else
    UNUSED(proshot);
#endif
}

void INIT_CODE motorInit(void)
{
    // Every motor starts stopped, with no override
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++)
        motorOutput[i] = MOTOR_STOP;

    resetMotorOverride();

    motorDevInit();
}

void motorStop(void)
{
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++)
        motorOutput[i] = MOTOR_STOP;

    motorWriteAllNow(motorOutput);

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
        // Disarmed, a motor is stopped unless overridden; an override
        // spins it up from zero throttle
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
            if (isfinite(motorOverride[i]))
                motorOutput[i] = slewUpLimit(fmaxf(motorOutput[i], 0),
                    constrainf(motorOverride[i], 0, 1),
                    MOTOR_OVERRIDE_RATE * pidGetDT());
            else
                motorOutput[i] = MOTOR_STOP;
        }
    }

    motorWriteAll(motorOutput);
}
