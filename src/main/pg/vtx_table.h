/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef USE_VTX_TABLE
#define VTX_TABLE_MAX_BANDS             8 // Maximum number of bands
#define VTX_TABLE_MAX_CHANNELS          8 // Maximum number of channels per band
#define VTX_TABLE_MAX_POWER_LEVELS      8 // Maximum number of power levels
#else
#define VTX_TABLE_MAX_BANDS             5 // default freq table has 5 bands
#define VTX_TABLE_MAX_CHANNELS          8 // and eight channels
#define VTX_TABLE_MAX_POWER_LEVELS      5 // max of VTX_TRAMP_POWER_COUNT, VTX_SMARTAUDIO_POWER_COUNT and VTX_RTC6705_POWER_COUNT
#endif

#define VTX_TABLE_CHANNEL_NAME_LENGTH   1
#define VTX_TABLE_BAND_NAME_LENGTH      8
#define VTX_TABLE_POWER_LABEL_LENGTH    3

#include <stdint.h>
#include <stdbool.h>

#include "build/build_config.h"

#include "pg/pg.h"
#include "pg/pg_limits.h"

// The stored table is always the USE_VTX_TABLE size. Sizing it from
// VTX_TABLE_MAX_* would give the group one layout with USE_VTX_TABLE and
// another without, and a group's layout may not depend on a build flag.
#ifdef USE_VTX_TABLE
STATIC_ASSERT(VTX_TABLE_MAX_BANDS == PG_VTX_TABLE_MAX_BANDS, "VTX table bands changed");
STATIC_ASSERT(VTX_TABLE_MAX_CHANNELS == PG_VTX_TABLE_MAX_CHANNELS, "VTX table channels changed");
STATIC_ASSERT(VTX_TABLE_MAX_POWER_LEVELS == PG_VTX_TABLE_MAX_POWER_LEVELS, "VTX table power levels changed");
#endif

typedef struct vtxTableConfig_s {
    uint8_t  bands;
    uint8_t  channels;
    uint16_t frequency[PG_VTX_TABLE_MAX_BANDS][PG_VTX_TABLE_MAX_CHANNELS];
    char     bandNames[PG_VTX_TABLE_MAX_BANDS][VTX_TABLE_BAND_NAME_LENGTH + 1];
    char     bandLetters[PG_VTX_TABLE_MAX_BANDS];
    char     channelNames[PG_VTX_TABLE_MAX_CHANNELS][VTX_TABLE_CHANNEL_NAME_LENGTH + 1];
    bool     isFactoryBand[PG_VTX_TABLE_MAX_BANDS];

    uint8_t  powerLevels;
    uint16_t powerValues[PG_VTX_TABLE_MAX_POWER_LEVELS];
    char     powerLabels[PG_VTX_TABLE_MAX_POWER_LEVELS][VTX_TABLE_POWER_LABEL_LENGTH + 1];
} vtxTableConfig_t;

struct vtxTableConfig_s;
PG_DECLARE(struct vtxTableConfig_s, vtxTableConfig);
