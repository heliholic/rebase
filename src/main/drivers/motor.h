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

void motorPostInit(void);
void motorWriteAll(float *values);
void motorRequestTelemetry(unsigned index);

void motorDevInit(void);
unsigned motorDeviceCount(void);

const motorVTable_t *motorGetVTable(void);
bool checkMotorProtocolEnabled(const motorDevConfig_t *motorConfig);
bool checkMotorProtocolDshot(const motorDevConfig_t *motorConfig);

motorProtocolFamily_e motorGetProtocolFamily(void);

bool isMotorProtocolDshot(void);
bool isMotorProtocolBidirDshot(void);
bool isMotorProtocolDronecan(void);
bool isMotorProtocolEnabled(void);

void motorDisable(void);
void motorEnable(void);
float motorEstimateMaxRpm(void);
bool motorIsEnabled(void);
bool motorIsMotorEnabled(unsigned index);
bool motorIsMotorIdle(unsigned index);
IO_t motorGetIo(unsigned index);

timeMs_t motorGetMotorEnableTimeMs(void);
void motorShutdown(void); // Replaces stopPwmAllMotors
