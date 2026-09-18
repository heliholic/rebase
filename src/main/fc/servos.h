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

#pragma once

#include "platform.h"

#include "drivers/pwm_output.h"

#include "pg/servo.h"

#define SERVO_OVERRIDE_MIN   -2000
#define SERVO_OVERRIDE_MAX    2000
#define SERVO_OVERRIDE_OFF  123456

void servoInit(void);
void servoUpdate(void);
void servoShutdown(void);

void validateAndFixServoConfig(void);

int  getServoOutput(uint8_t servo);

bool  hasServoOverride(uint8_t servo);
int   getServoOverride(uint8_t servo);
int   setServoOverride(uint8_t servo, int val);
