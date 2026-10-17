/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
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

#include <math.h>
#include <string.h>

#include "platform.h"

#include "common/maths.h"
#include "common/utils.h"

#include "drivers/io.h"
#include "drivers/timer.h"
#include "drivers/pwm_output.h"
#include "drivers/pwm_output_impl.h"

#ifdef USE_TIMER_AF
#include "platform/timer.h"
#endif

#include "pg/motor.h"
#include "pg/servo.h"

#ifdef USE_PWM_OUTPUT

#ifdef USE_MOTOR

FAST_DATA_ZERO_INIT pwmOutputPort_t pwmMotors[MAX_SUPPORTED_MOTORS];
FAST_DATA_ZERO_INIT uint32_t pwmMotorMask;

IO_t pwmGetMotorIO(unsigned index)
{
    if (index >= MAX_SUPPORTED_MOTORS) {
        return IO_NONE;
    }
    return pwmMotors[index].io;
}

bool pwmIsMotorEnabled(unsigned index)
{
    return pwmMotors[index].enabled;
}

bool pwmEnableMotors(void)
{
    /* check motors can be enabled */
    return pwmMotorMask != 0;
}

#ifdef USE_TIMER_AF

static bool useContinuousUpdate = true;

static float pwmConvertToInternal(uint8_t index, float throttle)
{
    UNUSED(index);

    float value = (float)motorConfig()->mincommand;

    if (throttle >= 0) {
        value = scaleRangef(throttle, 0, 1, (float)motorConfig()->mincommand, (float)motorConfig()->maxthrottle);
    }

    return value;
}

static void pwmWriteStandard(uint8_t index, float throttle)
{
    float value = pwmConvertToInternal(index, throttle);
    float pulse = value * pwmMotors[index].pulseScale + pwmMotors[index].pulseOffset;

    *pwmMotors[index].channel.ccr = lrintf(pulse);
}

static void pwmShutdownPulsesForAllMotors(void)
{
    for (int index = 0; index < MAX_SUPPORTED_MOTORS; index++) {
        // Set the compare register to 0, which stops the output pulsing if the timer overflows
        if (pwmMotors[index].enabled) {
            *pwmMotors[index].channel.ccr = 0;
        }
    }
}

static void pwmDisableMotors(void)
{
    pwmShutdownPulsesForAllMotors();
}

static void pwmCompleteMotorUpdate(void)
{
    if (useContinuousUpdate) {
        return;
    }

    for (int index = 0; index < MAX_SUPPORTED_MOTORS; index++) {
        if (pwmMotors[index].enabled) {
            if (pwmMotors[index].forceOverflow) {
                timerForceOverflow(pwmMotors[index].channel.tim);
            }
            // Set the compare register to 0, which stops the output pulsing if the timer overflows before the main loop completes again.
            // This compare register will be set to the output value on the next main loop.
            *pwmMotors[index].channel.ccr = 0;
        }
    }
}

static const motorVTable_t motorPwmVTable = {
    .postInit = NULL,
    .enable = pwmEnableMotors,
    .disable = pwmDisableMotors,
    .isMotorEnabled = pwmIsMotorEnabled,
    .shutdown = pwmShutdownPulsesForAllMotors,
    .write = pwmWriteStandard,
    .decodeTelemetry = NULL,
    .updateComplete = pwmCompleteMotorUpdate,
    .requestTelemetry = NULL,
    .isMotorIdle = NULL,
    .getMotorIO = pwmGetMotorIO,
};

uint32_t motorPwmDevInit(uint32_t motorMask, const motorVTable_t **vTable)
{
    const motorDevConfig_t *motorDevConfig = &motorConfig()->dev;

    memset(pwmMotors, 0, sizeof(pwmMotors));
    pwmMotorMask = 0;

    uint16_t idlePulse = motorConfig()->mincommand;
    if (motorDevConfig->motorProtocol == MOTOR_PROTOCOL_BRUSHED) {
        idlePulse = 0;
    }

    *vTable = &motorPwmVTable;
    useContinuousUpdate = motorDevConfig->useContinuousUpdate;

    float sMin = 0;
    float sLen = 0;
    switch (motorDevConfig->motorProtocol) {
    default:
    case MOTOR_PROTOCOL_ONESHOT125:
        sMin = 125e-6f;
        sLen = 125e-6f;
        break;
    case MOTOR_PROTOCOL_ONESHOT42:
        sMin = 42e-6f;
        sLen = 42e-6f;
        break;
    case MOTOR_PROTOCOL_MULTISHOT:
        sMin = 5e-6f;
        sLen = 20e-6f;
        break;
    case MOTOR_PROTOCOL_BRUSHED:
        sMin = 0;
        useContinuousUpdate = true;
        idlePulse = 0;
        break;
    case MOTOR_PROTOCOL_PWM :
        sMin = 1e-3f;
        sLen = 1e-3f;
        useContinuousUpdate = true;
        idlePulse = 0;
        break;
    }

    for (int motorIndex = 0; motorIndex < MAX_SUPPORTED_MOTORS; motorIndex++) {
        if (motorMask & BIT(motorIndex)) {
            const ioTag_t tag = motorDevConfig->ioTags[motorIndex];
            const timerHardware_t *timerHardware = timerAllocate(tag, OWNER_MOTOR, RESOURCE_INDEX(motorIndex));

            // A motor whose pin has no free timer stays down; the others carry on
            if (timerHardware) {
                pwmMotors[motorIndex].io = IOGetByTag(tag);
                IOInit(pwmMotors[motorIndex].io, OWNER_MOTOR, RESOURCE_INDEX(motorIndex));

                IOConfigGPIOAF(pwmMotors[motorIndex].io, IOCFG_AF_PP, timerHardware->alternateFunction);

                /* standard PWM outputs */
                // margin of safety is 4 periods when unsynced
                const unsigned pwmRateHz = useContinuousUpdate ? motorDevConfig->motorPwmRate : ceilf(1 / ((sMin + sLen) * 4));

                const uint32_t clock = timerClock(timerHardware);
                /* used to find the desired timer frequency for max resolution */
                const unsigned prescaler = ((clock / pwmRateHz) + 0xffff) / 0x10000; /* rounding up */
                const uint32_t hz = clock / prescaler;
                const unsigned period = useContinuousUpdate ? hz / pwmRateHz : 0xffff;

                /*
                    if brushed then it is the entire length of the period.
                    TODO: this can be moved back to periodMin and periodLen
                    once mixer outputs a 0..1 float value.
                */
                pwmMotors[motorIndex].pulseScale = ((motorDevConfig->motorProtocol == MOTOR_PROTOCOL_BRUSHED) ? period : (sLen * hz)) / 1000.0f;
                pwmMotors[motorIndex].pulseOffset = (sMin * hz) - (pwmMotors[motorIndex].pulseScale * 1000);

                pwmOutputConfig(&pwmMotors[motorIndex].channel, timerHardware, hz, period, idlePulse, motorDevConfig->motorInversion);

                bool timerAlreadyUsed = false;
                for (int i = 0; i < motorIndex; i++) {
                    if (pwmMotors[i].channel.tim == pwmMotors[motorIndex].channel.tim) {
                        timerAlreadyUsed = true;
                        break;
                    }
                }
                pwmMotors[motorIndex].forceOverflow = !timerAlreadyUsed;
                pwmMotors[motorIndex].enabled = true;
                pwmMotorMask |= BIT(motorIndex);
            }
        }
    }

    return pwmMotorMask;
}

pwmOutputPort_t *pwmGetMotors(void)
{
    return pwmMotors;
}

#endif // USE_TIMER_AF

#endif // USE_MOTOR

#if defined(USE_SERVOS) && defined(USE_TIMER_AF)

static FAST_DATA_ZERO_INIT pwmOutputPort_t servos[MAX_SUPPORTED_SERVOS];

void servoWrite(uint8_t index, float value)
{
    if (index < MAX_SUPPORTED_SERVOS && servos[index].enabled) {
        float pulse = value * servos[index].pulseScale + servos[index].pulseOffset;
        *servos[index].channel.ccr = lrintf(pulse);
    }
}

void servoDevInit(void)
{
    const ioTag_t *ioTags = servoConfig()->dev.ioTags;
    const timerHardware_t *timers[MAX_SUPPORTED_SERVOS];
    int rates[MAX_SUPPORTED_SERVOS];

    for (int index = 0; index < MAX_SUPPORTED_SERVOS; index++) {
        const ioTag_t tag = ioTags[index];
        const IO_t io = IOGetByTag(tag);
        if (tag) {
            const timerHardware_t *timer = timerAllocate(tag, OWNER_SERVO, RESOURCE_INDEX(index));
            if (timer) {
                servos[index].io = io;
                timers[index] = timer;
                IOInit(io, OWNER_SERVO, RESOURCE_INDEX(index));
                IOConfigGPIOAF(io, IOCFG_AF_PP, timer->alternateFunction);
            }
        }
    }

    for (int index = 0; index < MAX_SUPPORTED_SERVOS; index++) {
        if (servos[index].io) {
            uint32_t update_rate = servoParams(index)->rate;
            for (int jndex = 0; jndex < MAX_SUPPORTED_SERVOS; jndex++) {
                if (servos[jndex].io) {
                    if (timers[index]->tim == timers[jndex]->tim) {
                        uint32_t maxpulse = servoParams(jndex)->mid + servoParams(jndex)->max;
                        uint32_t maxrate = MIN(servoParams(jndex)->rate, 950000 / maxpulse);  // 1000000 / (maxpulse +5%)
                        update_rate = MIN(update_rate, maxrate);
                    }
                }
            }

            rates[index] = constrain(update_rate, SERVO_RATE_MIN, SERVO_RATE_MAX);
        }
    }

    for (int index = 0; index < MAX_SUPPORTED_SERVOS; index++) {
        if (servos[index].io) {
            const uint32_t timer_clock = timerClock(timers[index]);
            const uint32_t update_rate = rates[index];
            uint32_t timebase;

            servoParamsMutable(index)->rate = update_rate;

            if (update_rate > 500) {
                const uint32_t timer_rate = update_rate * 64000;
                const uint32_t timer_div = (timer_clock + timer_rate - 1) / timer_rate;
                timebase = timer_clock / timer_div;
            }
            else if (update_rate > 154 && timer_clock % 10000000 == 0) {
                timebase = 10000000;
            }
            else if (update_rate > 124 && timer_clock % 8000000 == 0) {
                timebase = 8000000;
            }
            else if (update_rate > 77 && timer_clock % 5000000 == 0) {
                timebase = 5000000;
            }
            else if (update_rate > 62 && timer_clock % 4000000 == 0) {
                timebase = 4000000;
            }
            else if (update_rate > 31 && timer_clock % 2000000 == 0) {
                timebase = 2000000;
            }
            else {
                timebase = 1000000;
            }

            servos[index].pulseScale = timebase / 1e6f;
            servos[index].pulseOffset = 0;

            pwmOutputConfig(&servos[index].channel, timers[index], timebase, (uint16_t)(timebase / update_rate), 0, 0);

            servos[index].enabled = true;
        }
    }
}

#endif // USE_SERVOS && USE_TIMER_AF

#endif // USE_PWM_OUTPUT

#ifdef USE_SERVOS

int getServoCount(void)
{
    return MAX_SUPPORTED_SERVOS;
}

#endif
