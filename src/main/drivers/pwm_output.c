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

#include "platform.h"

#include "common/types.h"
#include "common/maths.h"

#include "config/feature.h"

#include "drivers/io.h"
#include "drivers/timer.h"
#include "drivers/pwm_output.h"

#include "pg/motor.h"
#include "pg/servo.h"

#ifdef USE_PWM_OUTPUT

#ifdef USE_MOTOR

FAST_DATA_ZERO_INIT pwmOutputPort_t pwmMotors[MAX_SUPPORTED_MOTORS];
FAST_DATA_ZERO_INIT uint8_t pwmMotorCount;

IO_t pwmGetMotorIO(unsigned index)
{
    if (index >= pwmMotorCount) {
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
    return pwmMotorCount > 0;
}

#endif // USE_MOTOR

#ifdef USE_SERVOS

static FAST_DATA_ZERO_INIT pwmOutputPort_t servos[MAX_SUPPORTED_SERVOS];

int getServoCount(void)
{
    return MAX_SUPPORTED_SERVOS;
}

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
            const uint32_t timer_clock = timerClock(timers[index]->tim);
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

            pwmOutConfig(&servos[index].channel, timers[index], timebase, timebase / update_rate, 0, 0);

            servos[index].enabled = true;
        }
    }
}

#endif // USE_SERVOS

#endif // USE_PWM_OUTPUT
