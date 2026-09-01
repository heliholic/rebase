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

#include "platform.h"

#ifdef USE_RX_PPM

#include "drivers/io.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "rx_pwm.h"

PG_REGISTER_WITH_RESET_FN(ppmConfig_t, ppmConfig, PG_PPM_CONFIG);

void pgResetFn_ppmConfig(ppmConfig_t *ppmConfig)
{
#ifdef RX_PPM_PIN
    ppmConfig->ioTag = IO_TAG(RX_PPM_PIN);
#else
    ppmConfig->ioTag = IO_TAG_NONE;
#endif
}

#endif
