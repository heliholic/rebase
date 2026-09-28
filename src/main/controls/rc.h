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
#include <math.h>

#include "common/maths.h"
#include "common/types.h"

#include "drivers/time.h"

#include "pg/rc_controls.h"
#include "pg/rx.h"

// Legacy RC command; -500..500 for RPYC and THROTTLE
extern float rcCommand[RC_CONTROL_COUNT];

void initRcProcessing(void);
void updateRcCommands(void);

float getRcDeflection(int axis);

float getThrottle(void);

static inline float getThrottleCommand(void) { return getThrottle() * 1000; }
static inline uint8_t getThrottlePercent(void) { return lrintf(getThrottle() * 100); }

bool isThrottleOff(void);

uint16_t getFailsafeThrottle(void);
uint16_t getFailsafeCenter(void);

uint getCurrentRxRefreshRate(void);
float getAverageRxRefreshRate(void);

void updateRcRefreshRate(timeUs_t currentTimeUs);
