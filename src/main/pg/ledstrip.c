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

#include "platform.h"

#ifdef USE_LED_STRIP

#include "config/config_reset.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "common/utils.h"
#include "pg/ledstrip.h"

STATIC_ASSERT(LED_STRIP_MAX_LENGTH <= PG_MAX_LED_STRIP_LENGTH, led_strip_length_exceeds_stored_bound);

PG_REGISTER_WITH_RESET_FN(ledStripConfig_t, ledStripConfig, PG_LED_STRIP_CONFIG);

#ifdef USE_LED_STRIP_STATUS_MODE
PG_REGISTER_WITH_RESET_FN(ledStripStatusModeConfig_t, ledStripStatusModeConfig, PG_LED_STRIP_STATUS_MODE_CONFIG);
#endif

#endif
