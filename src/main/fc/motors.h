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

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "drivers/motor.h"


#define MOTOR_STOP                    -1

#define MOTOR_OVERRIDE_OFF            -1
#define MOTOR_OVERRIDE_MIN             0
#define MOTOR_OVERRIDE_MAX          1000

#define MOTOR_OVERRIDE_RATE        0.25f
#define MOTOR_OVERRIDE_TIMEOUT   1000000


uint8_t getMotorCount(void);

int getMotorOutput(uint8_t motor);

int getMotorOverride(uint8_t motor);
int setMotorOverride(uint8_t motor, int value, timeDelta_t timeout);
bool hasMotorOverride(uint8_t motor);
void resetMotorOverride(void);

bool areMotorsRunning(void);

void motorInit(void);
void motorStop(void);
void motorUpdate(timeUs_t currentTimeUs);

// Compat
static inline void stopMotors(void) { motorStop(); }
