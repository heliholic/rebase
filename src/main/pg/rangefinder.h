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

#include <stdint.h>

#include "pg/pg.h"

#include "drivers/io_types.h"

typedef enum {
    RANGEFINDER_NONE        = 0,
    RANGEFINDER_HCSR04      = 1,
    RANGEFINDER_TFMINI      = 2,
    RANGEFINDER_TF02        = 3,
    RANGEFINDER_MTF01       = 4,
    RANGEFINDER_MTF02       = 5,
    RANGEFINDER_MTF01P      = 6,
    RANGEFINDER_MTF02P      = 7,
    RANGEFINDER_TFNOVA        = 8,
    RANGEFINDER_NOOPLOOP_F2   = 9,
    RANGEFINDER_NOOPLOOP_F2P  = 10,
    RANGEFINDER_NOOPLOOP_F2PH = 11,
    RANGEFINDER_NOOPLOOP_F    = 12,
    RANGEFINDER_NOOPLOOP_FP   = 13,
    RANGEFINDER_NOOPLOOP_F2MINI = 14,
    RANGEFINDER_UPT1          = 15,
    RANGEFINDER_HARDWARE_COUNT,
} PG_ENUM rangefinderType_e;

typedef struct {
    rangefinderType_e rangefinder_hardware;
} rangefinderConfig_t;

PG_DECLARE(rangefinderConfig_t, rangefinderConfig);

typedef struct {
    ioTag_t triggerTag;
    ioTag_t echoTag;
} sonarConfig_t;

PG_DECLARE(sonarConfig_t, sonarConfig);
