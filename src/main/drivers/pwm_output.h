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

#pragma once

#include "platform.h"

#include "common/rtc.h"

#include "drivers/dma.h"
#include "drivers/io_types.h"
#include "drivers/motor_types.h"
#include "drivers/timer.h"

#include "pg/motor.h"

#define PWM_TIMER_1MHZ        MHZ_TO_HZ(1)

typedef struct {
    volatile uint32_t *ccr;
    timerResource_t   *tim;
} timerChannel_t;

typedef struct {
    timerChannel_t channel;
    float pulseScale;
    float pulseOffset;
    bool forceOverflow;
    bool enabled;
    IO_t io;
} pwmOutputPort_t;

extern FAST_DATA_ZERO_INIT pwmOutputPort_t pwmMotors[MAX_SUPPORTED_MOTORS];
extern FAST_DATA_ZERO_INIT uint8_t pwmMotorCount;

bool motorPwmDevInit(motorDevice_t *device, const motorDevConfig_t *motorDevConfig);

void pwmOutputConfig(timerChannel_t *channel, const timerHardware_t *timerHardware, uint32_t hz, uint16_t period, uint16_t value, uint8_t inversion);
void pwmWriteChannel(timerChannel_t *channel, uint32_t value);

pwmOutputPort_t *pwmGetMotors(void);
bool pwmIsSynced(void);

void servoDevInit(void);

void servoWrite(uint8_t index, float value);

int getServoCount(void);

#define PWM_SERVO_PULSE_MIN   100    // minimum PWM servo output pulse width allowed
#define PWM_SERVO_PULSE_MAX   2500   // maximum PWM servo output pulse width allowed
