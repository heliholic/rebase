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
#include "pg/pg_limits.h"
#include "pg/color.h"

#include "drivers/io_types.h"

#define LED_CONFIGURABLE_COLOR_COUNT   16
#define LED_MODE_COUNT                  6
#define LED_DIRECTION_COUNT             6
#define LED_BASEFUNCTION_COUNT          10
#define LED_OVERLAY_COUNT               7
#define LED_SPECIAL_COLOR_COUNT        11

typedef enum {
    COLOR_BLACK = 0,
    COLOR_WHITE,
    COLOR_RED,
    COLOR_ORANGE,
    COLOR_YELLOW,
    COLOR_LIME_GREEN,
    COLOR_GREEN,
    COLOR_MINT_GREEN,
    COLOR_CYAN,
    COLOR_LIGHT_BLUE,
    COLOR_BLUE,
    COLOR_DARK_VIOLET,
    COLOR_MAGENTA,
    COLOR_DEEP_PINK,
    COLOR_COUNT
} PG_ENUM colorId_e;

// LED_PROFILE_STATUS is listed whether or not USE_LED_STRIP_STATUS_MODE is set.
// The enumerators are part of ledStripConfig's layout hash - that is the point
// of storing the member as an enum - so dropping one would make the group's
// hash depend on a build option. Only the code that renders the profile is
// conditional.
typedef enum {
    LED_PROFILE_RACE = 0,
    LED_PROFILE_BEACON,
    LED_PROFILE_STATUS,
    LED_PROFILE_COUNT
} PG_ENUM ledProfile_e;

typedef struct {
    ioTag_t      ioTag;
    bool         ledstrip_visual_beeper;
    uint8_t      ledstrip_grb_rgb;
    ledProfile_e ledstrip_profile;
    colorId_e    ledstrip_race_color;
    colorId_e    ledstrip_beacon_color;
    uint16_t     ledstrip_beacon_period_ms;
    uint8_t      ledstrip_beacon_percent;
    bool         ledstrip_beacon_armed_only;
    colorId_e    ledstrip_visual_beeper_color;
    uint8_t      ledstrip_brightness;
    uint16_t     ledstrip_rainbow_delta;
    uint16_t     ledstrip_rainbow_freq;
} ledStripConfig_t;

PG_DECLARE(ledStripConfig_t, ledStripConfig);

typedef uint32_t ledConfig_t;

typedef struct {
    uint8_t color[LED_DIRECTION_COUNT];
} modeColorIndexes_t;

typedef struct {
    uint8_t color[LED_SPECIAL_COLOR_COUNT];
} specialColorIndexes_t;

typedef struct {
    ledConfig_t ledConfigs[PG_MAX_LED_STRIP_LENGTH]; // LED_STRIP_MAX_LENGTH of these are used
    hsvColor_t colors[LED_CONFIGURABLE_COLOR_COUNT];
    modeColorIndexes_t modeColors[LED_MODE_COUNT];
    specialColorIndexes_t specialColors;
    uint8_t ledstrip_aux_channel;
} ledStripStatusModeConfig_t;

PG_DECLARE(ledStripStatusModeConfig_t, ledStripStatusModeConfig);
