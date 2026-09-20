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

#ifdef USE_QUADSPI

#include "drivers/io.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "bus_quadspi.h"

PG_REGISTER_ARRAY_WITH_RESET_ARRAY(quadSpiConfig_t, QUADSPIDEV_COUNT, quadSpiConfig, PG_QUADSPI_CONFIG);

PG_RESET_TEMPLATE_ARRAY(quadSpiConfig_t, QUADSPIDEV_COUNT, quadSpiConfig)
{
#ifdef USE_QUADSPI_DEVICE_1
    /* Provide safe defaults when some per-target macros are missing */
    #ifdef QUADSPI1_SCK_PIN
        #define QSPI1_CLK_TAG IO_TAG(QUADSPI1_SCK_PIN)
    #else
        #define QSPI1_CLK_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK1_IO0_PIN
        #define QSPI1_BK1_IO0_TAG IO_TAG(QUADSPI1_BK1_IO0_PIN)
    #else
        #define QSPI1_BK1_IO0_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK1_IO1_PIN
        #define QSPI1_BK1_IO1_TAG IO_TAG(QUADSPI1_BK1_IO1_PIN)
    #else
        #define QSPI1_BK1_IO1_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK1_IO2_PIN
        #define QSPI1_BK1_IO2_TAG IO_TAG(QUADSPI1_BK1_IO2_PIN)
    #else
        #define QSPI1_BK1_IO2_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK1_IO3_PIN
        #define QSPI1_BK1_IO3_TAG IO_TAG(QUADSPI1_BK1_IO3_PIN)
    #else
        #define QSPI1_BK1_IO3_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK1_CS_PIN
        #define QSPI1_BK1_CS_TAG IO_TAG(QUADSPI1_BK1_CS_PIN)
    #else
        #define QSPI1_BK1_CS_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK2_IO0_PIN
        #define QSPI1_BK2_IO0_TAG IO_TAG(QUADSPI1_BK2_IO0_PIN)
    #else
        #define QSPI1_BK2_IO0_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK2_IO1_PIN
        #define QSPI1_BK2_IO1_TAG IO_TAG(QUADSPI1_BK2_IO1_PIN)
    #else
        #define QSPI1_BK2_IO1_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK2_IO2_PIN
        #define QSPI1_BK2_IO2_TAG IO_TAG(QUADSPI1_BK2_IO2_PIN)
    #else
        #define QSPI1_BK2_IO2_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK2_IO3_PIN
        #define QSPI1_BK2_IO3_TAG IO_TAG(QUADSPI1_BK2_IO3_PIN)
    #else
        #define QSPI1_BK2_IO3_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_BK2_CS_PIN
        #define QSPI1_BK2_CS_TAG IO_TAG(QUADSPI1_BK2_CS_PIN)
    #else
        #define QSPI1_BK2_CS_TAG IO_TAG_NONE
    #endif
    #ifdef QUADSPI1_MODE
        #define QSPI1_MODE_VAL QUADSPI1_MODE
    #else
        #define QSPI1_MODE_VAL 0
    #endif
    #ifdef QUADSPI1_CS_FLAGS
        #define QSPI1_CS_FLAGS_VAL QUADSPI1_CS_FLAGS
    #else
        #define QSPI1_CS_FLAGS_VAL 0
    #endif

    [QUADSPIDEV_1] = {
        QSPI1_CLK_TAG,
        QSPI1_BK1_IO0_TAG, QSPI1_BK1_IO1_TAG, QSPI1_BK1_IO2_TAG, QSPI1_BK1_IO3_TAG, QSPI1_BK1_CS_TAG,
        QSPI1_BK2_IO0_TAG, QSPI1_BK2_IO1_TAG, QSPI1_BK2_IO2_TAG, QSPI1_BK2_IO3_TAG, QSPI1_BK2_CS_TAG,
        QSPI1_MODE_VAL,
        QSPI1_CS_FLAGS_VAL
    },
    /* Undefine helper macros to avoid bleeding into other units */
    #undef QSPI1_CLK_TAG
    #undef QSPI1_BK1_IO0_TAG
    #undef QSPI1_BK1_IO1_TAG
    #undef QSPI1_BK1_IO2_TAG
    #undef QSPI1_BK1_IO3_TAG
    #undef QSPI1_BK1_CS_TAG
    #undef QSPI1_BK2_IO0_TAG
    #undef QSPI1_BK2_IO1_TAG
    #undef QSPI1_BK2_IO2_TAG
    #undef QSPI1_BK2_IO3_TAG
    #undef QSPI1_BK2_CS_TAG
    #undef QSPI1_MODE_VAL
    #undef QSPI1_CS_FLAGS_VAL
#endif
};

#endif
