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

#include "common/axis.h"

#include "config/config.h"
#include "config/config_reset.h"

#include "fc/rc_controls.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "pg/rates.h"

PG_REGISTER_ARRAY_WITH_RESET_FN(controlRateConfig_t, CONTROL_RATE_PROFILE_COUNT, controlRateProfiles, PG_CONTROL_RATE_PROFILES, 8);

void pgResetFn_controlRateProfiles(controlRateConfig_t *controlRateConfig)
{
    for (int i = 0; i < CONTROL_RATE_PROFILE_COUNT; i++) {
        RESET_CONFIG(controlRateConfig_t, &controlRateConfig[i],
            .profileName = "",
            .rates_type = RATES_TYPE_ROTORFLIGHT,
            .rcRates[FD_ROLL] = 50,
            .rcRates[FD_PITCH] = 50,
            .rcRates[FD_YAW] = 80,
            .rcRates[FD_COLL] = 100,
            .sRates[FD_ROLL] = 12,
            .sRates[FD_PITCH] = 12,
            .sRates[FD_YAW] = 12,
            .sRates[FD_COLL] = 12,
            .rcExpo[FD_ROLL] = 40,
            .rcExpo[FD_PITCH] = 40,
            .rcExpo[FD_YAW] = 50,
            .rcExpo[FD_COLL] = 0,
        );
    }
}
