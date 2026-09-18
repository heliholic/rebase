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

#define MAX_RATE_PROFILE_NAME_LENGTH 8

typedef struct rateProfile_s {
    char profileName[MAX_RATE_PROFILE_NAME_LENGTH + 1];

    uint16_t rates[4];                      // maximum rate in deg/s
    uint8_t expo[4];                        // expo applied to the rate curve
    uint8_t curve[4];                       // order of the expo curve

    uint8_t levelExpo[2];                   // roll/pitch level mode expo

    uint8_t response_time[4];
    uint16_t accel_limit[4];

    uint8_t cyclic_ring;
    uint8_t cyclic_polar;

    uint8_t setpoint_boost_gain[4];
    uint8_t setpoint_boost_cutoff[4];

    uint8_t yaw_dynamic_ceiling_gain;
    uint8_t yaw_dynamic_deadband_gain;
    uint8_t yaw_dynamic_deadband_cutoff;
    uint8_t yaw_dynamic_deadband_filter;

} rateProfile_t;

PG_DECLARE_ARRAY(rateProfile_t, RATE_PROFILE_COUNT, rateProfiles);

