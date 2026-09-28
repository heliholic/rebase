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

typedef struct {
    uint16_t rc_center;                 // Stick center. Usually 1500 or 1520, depending on the RC system
    uint16_t rc_deflection;             // Max stick deflection in us, applies to RPYC
    uint16_t rc_min_throttle;           // Throttle channel value for 0%
    uint16_t rc_max_throttle;           // Throttle channel value for 100%
    uint8_t  rc_deadband;               // A deadband around the stick center for pitch and roll axis
    uint8_t  rc_yaw_deadband;           // A deadband around the stick center for yaw axis
    uint8_t  rc_smoothness;             // Minimum RPYC smoothing level
    uint8_t  rc_threshold[4];           // Threshold for stick activity
} rcControlsConfig_t;

PG_DECLARE(rcControlsConfig_t, rcControlsConfig);
