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

#include "common/utils.h"

#include "pg/osd.h"

#ifdef USE_OSD

STATIC_ASSERT(OSD_PROFILE_COUNT <= PG_MAX_OSD_PROFILES, osd_profile_count_exceeds_stored_bound);

PG_REGISTER_WITH_RESET_FN(osdConfig_t, osdConfig, PG_OSD_CONFIG);

PG_REGISTER_WITH_RESET_FN(osdElementConfig_t, osdElementConfig, PG_OSD_ELEMENT_CONFIG);

#ifdef USE_OSD_CUSTOM_TEXT
PG_REGISTER_WITH_RESET_FN(osdCustomTextConfig_t, osdCustomTextConfig, PG_OSD_CUSTOM_TEXT_CONFIG);
#endif

#endif
