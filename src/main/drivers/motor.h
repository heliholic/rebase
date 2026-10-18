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
 *
 * Author: jflyper
 */

#pragma once

#include "common/rtc.h"

#include "pg/motor.h"

#include "drivers/motor_types.h"
#include "drivers/motor_resolve.h"

void motorPostInit(void);
void motorWriteAll(float *values);
void motorWriteAllNow(float *values);
void motorSetLoopTime(timeUs_t looptimeUs);
void motorRequestTelemetry(unsigned index);

void motorDevInit(void);
uint32_t motorDeviceMask(void);

uint32_t motorGetDriverMask(motorProtocolFamily_e family);
const motorVTable_t *motorGetDriverVTable(motorProtocolFamily_e family);

motorProtocolFamily_e motorGetProtocolFamily(unsigned index);
motorState_e motorGetState(unsigned index);
uint16_t motorGetUpdateRate(unsigned index);
const motorConfig_t *motorLeadConfig(uint32_t motorMask);

bool motorIsServoRefused(unsigned servoIndex);
uint16_t motorGetServoRate(unsigned servoIndex);

bool isMotorProtocolDshot(void);
bool isMotorProtocolBidirDshot(void);
bool isMotorProtocolDronecan(void);
bool isMotorProtocolEnabled(void);

void motorDisable(void);
void motorEnable(void);
float motorEstimateMaxRpm(unsigned index);
bool motorIsEnabled(void);
bool motorIsMotorEnabled(unsigned index);
bool motorIsMotorIdle(unsigned index);
IO_t motorGetIo(unsigned index);

timeMs_t motorGetMotorEnableTimeMs(void);
void motorShutdown(void); // Replaces stopPwmAllMotors
