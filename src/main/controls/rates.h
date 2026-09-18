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

#include "pg/rates.h"

#define RATE_MAX        1000    // maximum rate in deg/s
#define EXPO_MAX         100    // maximum expo
#define CURVE_MAX        100    // maximum expo curve order

#define SETPOINT_RATE_LIMIT     2000

extern rateProfile_t * currentRateProfile;

float applyRatesCurve(const int axis, const float rcCommandf);

void loadRateProfile(void);

void changeRateProfile(uint8_t rateProfileIndex);
void copyRateProfile(uint8_t dstRateProfileIndex, uint8_t srcRateProfileIndex);
