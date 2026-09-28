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

typedef struct {
    bool gyro_cal_on_first_arm;             // calibrate the gyro right before the first arm
    bool prearm_allow_rearm;
    bool enable_stick_commands;             // boolean that determines whether stick commands can be used
    channelRange_t arm_range;               // the arm channel range that means armed; unusable means no arm switch
} armingConfig_t;

PG_DECLARE(armingConfig_t, armingConfig);
