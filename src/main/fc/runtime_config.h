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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "common/utils.h"

#include "fc/arming.h"

// FIXME some of these are flight modes, some of these are general status indicators
typedef enum {
    ARMED                       = BIT(0),
    WAS_EVER_ARMED              = BIT(1),
    WAS_ARMED_WITH_PREARM       = BIT(2),
} armingFlag_e;

extern uint8_t armingFlags;

#define DISABLE_ARMING_FLAG(mask)   (armingFlags &= ~(mask))
#define ENABLE_ARMING_FLAG(mask)    (armingFlags |= (mask))
#define ARMING_FLAG(mask)           (armingFlags & (mask))

typedef enum {
    ARMED_MODE_BIT       = 0,
    ANGLE_MODE_BIT       = 1,
    HORIZON_MODE_BIT     = 2,
    FAILSAFE_MODE_BIT    = 3,
} flightModeBits_e;

typedef enum {
    ARMED_MODE           = BIT(ARMED_MODE_BIT),
    ANGLE_MODE           = BIT(ANGLE_MODE_BIT),
    HORIZON_MODE         = BIT(HORIZON_MODE_BIT),
    FAILSAFE_MODE        = BIT(FAILSAFE_MODE_BIT),
} flightModeFlags_e;

extern uint16_t flightModeFlags;

#define DISABLE_FLIGHT_MODE(mask) disableFlightMode(mask)
#define ENABLE_FLIGHT_MODE(mask) enableFlightMode(mask)
#define FLIGHT_MODE(mask) (flightModeFlags & (mask))

// macro to initialize map from boxId_e flightModeBits. Keep it in sync with flightModeFlags_e enum.
#define BOXID_TO_FLIGHT_MODE_MAP_INITIALIZER {           \
   [BOXANGLE]       = ANGLE_MODE_BIT,                    \
   [BOXHORIZON]     = HORIZON_MODE_BIT,                  \
   [BOXFAILSAFE]    = FAILSAFE_MODE_BIT,                 \
}                                                        \
/**/

typedef enum {
    GPS_FIX_HOME   = BIT(0),
    GPS_FIX        = BIT(1),
    GPS_FIX_EVER   = BIT(2),
} stateFlags_t;

#define DISABLE_STATE(mask) (stateFlags &= ~(mask))
#define ENABLE_STATE(mask) (stateFlags |= (mask))
#define STATE(mask) (stateFlags & (mask))

extern uint8_t stateFlags;

uint16_t enableFlightMode(flightModeFlags_e mask);
uint16_t disableFlightMode(flightModeFlags_e mask);

bool sensors(uint32_t mask);
void sensorsSet(uint32_t mask);
void sensorsClear(uint32_t mask);
uint32_t sensorsMask(void);
