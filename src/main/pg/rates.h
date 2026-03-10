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
#include <stdlib.h>
#include <string.h>

#include "pg/pg.h"

typedef enum {
    RATES_TYPE_BETAFLIGHT = 0,
    RATES_TYPE_RACEFLIGHT,
    RATES_TYPE_KISS,
    RATES_TYPE_ACTUAL,
    RATES_TYPE_QUICK,
    RATES_TYPE_ROTORFLIGHT,
    RATES_TYPE_COUNT
} ratesType_e;

#define MAX_RATE_PROFILE_NAME_LENGTH 8u

typedef struct {
    char profileName[MAX_RATE_PROFILE_NAME_LENGTH + 1];
    uint8_t rates_type;
    uint8_t rcRates[4];
    uint8_t sRates[4];
    uint8_t rcExpo[4];
} controlRateConfig_t;

PG_DECLARE_ARRAY(controlRateConfig_t, CONTROL_RATE_PROFILE_COUNT, controlRateProfiles);

