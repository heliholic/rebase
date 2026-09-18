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
#include "pg/modes.h"

#define MAX_ADJUSTMENT_RANGE_COUNT 42

/*
 * Defines the get/set pair an adjustment function is wired to. The pair is
 * defined by the module that owns the parameter, and picked up by the
 * ADJ_ENTRY table in controls/adjustments.c.
 */
#define ADJUSTMENT_GET(id)      int _adjustment_get_##id(void)
#define ADJUSTMENT_SET(id)      void _adjustment_set_##id(int value)

/*
 * Declares the get/set pair, for the header of the owning module.
 */
#define ADJUSTMENT_DECLARE(id) \
    extern ADJUSTMENT_GET(id); \
    extern ADJUSTMENT_SET(id);

typedef struct {
    uint8_t function;
    uint8_t enaChannel;
    channelRange_t enaRange;
    uint8_t adjChannel;
    channelRange_t adjRange1;
    channelRange_t adjRange2;
    int16_t adjMin;
    int16_t adjMax;
    uint8_t adjStep;
} adjustmentRange_t;

PG_DECLARE_ARRAY(adjustmentRange_t, MAX_ADJUSTMENT_RANGE_COUNT, adjustmentRanges);
