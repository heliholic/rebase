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

#if !defined(USE_VIRTUAL_LED)

#include "config/config_reset.h"

#include "drivers/io.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "pg/leds.h"

PG_REGISTER_WITH_RESET_TEMPLATE(statusLedConfig_t, statusLedConfig, PG_STATUS_LED_CONFIG, 0);

PG_RESET_TEMPLATE(statusLedConfig_t, statusLedConfig)
{
    .ioTags = {
#if STATUS_LED_COUNT > 0 && defined(LED0_PIN)
        [0] = IO_TAG(LED0_PIN),
#endif
#if STATUS_LED_COUNT > 1 && defined(LED1_PIN)
        [1] = IO_TAG(LED1_PIN),
#endif
#if STATUS_LED_COUNT > 2 && defined(LED2_PIN)
        [2] = IO_TAG(LED2_PIN),
#endif
    },
    .inversion = 0
#ifdef LED0_INVERTED
    | BIT(0)
#endif
#ifdef LED1_INVERTED
    | BIT(1)
#endif
#ifdef LED2_INVERTED
    | BIT(2)
#endif
    ,
};

#endif // !USE_VIRTUAL_LED
