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

#include <stdbool.h>

#include "platform.h"

#include "pg/displayport_profiles.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"

#if defined(USE_MSP_DISPLAYPORT)

PG_REGISTER_WITH_RESET_FN(displayPortProfile_t, displayPortProfileMsp, PG_DISPLAY_PORT_MSP_CONFIG);

PG_RESET_FN(displayPortProfile_t, displayPortProfileMsp)
{
    for (uint8_t font = 0; font < DISPLAYPORT_SEVERITY_COUNT; font++) {
        displayPortProfileMsp->fontSelection[font] = font;
    }
}

#endif

#if defined(USE_MAX7456)

PG_REGISTER_WITH_RESET_FN(displayPortProfile_t, displayPortProfileMax7456, PG_DISPLAY_PORT_MAX7456_CONFIG);

PG_RESET_FN(displayPortProfile_t, displayPortProfileMax7456)
{
    displayPortProfileMax7456->colAdjust = 0;
    displayPortProfileMax7456->rowAdjust = 0;

    // Set defaults as per MAX7456 datasheet
    displayPortProfileMax7456->invert = false;
    displayPortProfileMax7456->blackBrightness = 0;
    displayPortProfileMax7456->whiteBrightness = 2;
}

#endif

#if ENABLE_FB_OSD

PG_REGISTER_WITH_RESET_FN(displayPortProfile_t, displayPortProfileFbOsd, PG_DISPLAY_PORT_FBOSD_CONFIG);

PG_RESET_FN(displayPortProfile_t, displayPortProfileFbOsd)
{
    // TODO add entries in settings.c, so we can set from Configurator / CLI.
    displayPortProfileFbOsd->colAdjust = 0;
    displayPortProfileFbOsd->rowAdjust = 0;
    displayPortProfileFbOsd->invert = false;
    displayPortProfileFbOsd->blackBrightness = 0;
    displayPortProfileFbOsd->whiteBrightness = 0;
    // font selection
    // use device blink
}

#endif
