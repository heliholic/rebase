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

#include "pg/adjustments.h"

/*
 * Adjustment Function codes
 *
 * Use explicit values here, so that
 *    - Each value is visible for reference
 *    - Any change to all values will show up in a diff
 *
 * The codes are the Rotorflight 4.6 set, so that they are final. Only the
 * functions whose target exists in this tree are wired in the ADJ_ENTRY table
 * in adjustments.c; the rest arrive with the features that implement them.
 *
 * Codes 5..13 held 4.6's per-axis srate / rc_rate / rc_expo. This tree has one
 * rate curve, described by rate, curve and expo, so the slots carry those
 * instead. Collective, which 4.6 could not adjust, is at 83..85.
 */

typedef enum {

    ADJUSTMENT_NONE                     = 0,

    // Profile change
    ADJUSTMENT_RATE_PROFILE             = 1,
    ADJUSTMENT_PID_PROFILE              = 2,
    ADJUSTMENT_LED_PROFILE              = 3,
    ADJUSTMENT_BATTERY_PROFILE          = 4,
    ADJUSTMENT_OSD_PROFILE              = 5,

    // Rates
    ADJUSTMENT_PITCH_RATE               = 6,
    ADJUSTMENT_ROLL_RATE                = 7,
    ADJUSTMENT_YAW_RATE                 = 8,
    ADJUSTMENT_COLL_RATE                = 9,

    ADJUSTMENT_PITCH_CURVE              = 10,
    ADJUSTMENT_ROLL_CURVE               = 11,
    ADJUSTMENT_YAW_CURVE                = 12,
    ADJUSTMENT_COLL_CURVE               = 13,

    ADJUSTMENT_PITCH_EXPO               = 14,
    ADJUSTMENT_ROLL_EXPO                = 15,
    ADJUSTMENT_YAW_EXPO                 = 16,
    ADJUSTMENT_COLL_EXPO                = 17,

    // Accelerometer
    ADJUSTMENT_ACC_TRIM_PITCH           = 20,
    ADJUSTMENT_ACC_TRIM_ROLL            = 21,

    ADJUSTMENT_FUNCTION_COUNT
} adjustmentFunc_e;


void adjustmentRangeInit(void);
void adjustmentRangeReset(int index);

void processRcAdjustments(void);

const char *getAdjustmentsRangeName(void);
int getAdjustmentsRangeFunc(void);
int getAdjustmentsRangeValue(void);
