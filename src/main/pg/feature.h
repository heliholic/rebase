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

#include "common/utils.h"

#include "pg/pg.h"

#ifndef DEFAULT_FEATURES
#define DEFAULT_FEATURES 0
#endif

typedef enum {
    FEATURE_BIT_RX_PPM              = 0,
    FEATURE_BIT_RX_UDP              = 1,
    // 2 was INFLIGHT_ACC_CAL (removed)
    FEATURE_BIT_RX_SERIAL           = 3,
    FEATURE_BIT_SOFTSERIAL          = 6,
    FEATURE_BIT_GPS                 = 7,
    FEATURE_BIT_OPTICALFLOW         = 8,
    FEATURE_BIT_RANGEFINDER         = 9,
    FEATURE_BIT_TELEMETRY           = 10,
    // 13 was RX_PARALLEL_PWM (removed)
    FEATURE_BIT_RX_MSP              = 14,
    FEATURE_BIT_RSSI_ADC            = 15,
    FEATURE_BIT_LED_STRIP           = 16,
    FEATURE_BIT_DASHBOARD           = 17,
    // 18 was OSD (removed)
    // 25 was RX_SPI, 26 was SOFTSPI (both removed)
    FEATURE_BIT_ESC_SENSOR          = 27,
    // 29 was DYNAMIC_FILTER (removed)
    FEATURE_BIT_COUNT               = 32
} feature_bit_e;

#define ENTRY(FEA)  FEATURE_ ## FEA = BIT(FEATURE_BIT_ ## FEA)
typedef enum {
    ENTRY(RX_PPM),
    ENTRY(RX_UDP),
    ENTRY(RX_SERIAL),
    ENTRY(SOFTSERIAL),
    ENTRY(GPS),
    ENTRY(OPTICALFLOW),
    ENTRY(RANGEFINDER),
    ENTRY(TELEMETRY),
    ENTRY(RX_MSP),
    ENTRY(RSSI_ADC),
    ENTRY(LED_STRIP),
    ENTRY(DASHBOARD),
    ENTRY(ESC_SENSOR),
} features_e;
#undef ENTRY

typedef struct {
    uint32_t enabledFeatures;
} featureConfig_t;

PG_DECLARE(featureConfig_t, featureConfig);
