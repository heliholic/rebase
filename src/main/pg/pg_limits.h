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

/*
 * Fixed bounds for arrays stored inside parameter group structs.
 *
 * A member array sized by a target-derived count - SERIAL_PORT_COUNT,
 * RESOURCE_SERIAL_COUNT, ADCDEV_COUNT and the like - changes sizeof() and so
 * changes the group's layout hash, which is what identifies the record in
 * EEPROM. Two targets would then be unable to read each other's config, and
 * more importantly the same board built with different options could not read
 * back its own.
 *
 * So the stored array gets a fixed bound from here while the target keeps its
 * real count for everything else, and each user asserts that its count fits.
 * The trailing slots are unused storage; the reset functions are responsible
 * for leaving them in a state the readers ignore.
 *
 * These are a layout contract. Raising one is a deliberate breaking change: it
 * resizes the stored record, moves every hash, and needs an EEPROM version
 * bump. They are set above the largest target rather than at the theoretical
 * maximum, so there is room to grow without paying for ports no MCU has.
 *
 * Whole-group array lengths - PG_DECLARE_ARRAY(type, LENGTH, name) - are not
 * covered here and stay target-derived. The layout hash ignores them, and
 * pgLoad() reconciles a record whose element count differs.
 */

// vtxTableConfig. VTX_TABLE_MAX_* is two different things at once: the size of
// the stored table when USE_VTX_TABLE is set, and the size of the built-in
// frequency table when it is not - 8/8/8 against 5/8/5. The group is declared
// either way, so it cannot take its shape from that; it gets the USE_VTX_TABLE
// maxima always, and vtx_table.h asserts the two agree where both exist.
#define PG_VTX_TABLE_MAX_BANDS 8
#define PG_VTX_TABLE_MAX_CHANNELS 8
#define PG_VTX_TABLE_MAX_POWER_LEVELS 8

// serialConfig.portConfigs. Largest today is X32M7B at 16.
#define PG_MAX_SERIAL_PORTS 20

// serialPinConfig.ioTagTx / ioTagRx and serialUartConfig, which are addressed by
// a linear resource index built from fixed per-kind blocks. The block sizes are
// the most ports the normalisation in target/serial_post.h can enumerate, not
// the most any target has: the block offsets are part of the stored meaning of
// a slot, so unlike a bound they cannot be raised later at all.
#define PG_MAX_UART_RESOURCES 16
#define PG_MAX_LPUART_RESOURCES 1
#define PG_MAX_SOFTSERIAL_RESOURCES 2
#define PG_MAX_PIOUART_RESOURCES 10
#define PG_MAX_SERIAL_RESOURCES (PG_MAX_UART_RESOURCES + PG_MAX_LPUART_RESOURCES + \
                                 PG_MAX_SOFTSERIAL_RESOURCES + PG_MAX_PIOUART_RESOURCES)

// adcConfig.dmaopt, one per ADC peripheral. Largest today is STM32G474 at 5.
#define PG_MAX_ADC_DEVICES 6

// motorConfig.dev.ioTags. MAX_SUPPORTED_MOTORS is overridable by a target or a
// board config - src/config/configs/STMI/NUCLEOF446 asks for 12.
#define PG_MAX_MOTORS 12

// servoConfig.dev.ioTags. MAX_SUPPORTED_SERVOS is overridable the same way,
// though nothing in tree raises it above the default 8 today.
#define PG_MAX_SERVOS 12

// ledStripStatusModeConfig.ledConfigs. LED_STRIP_MAX_LENGTH is 32 or 64
// depending on USE_LED_STRIP_64.
#define PG_MAX_LED_STRIP_LENGTH 64

// osdConfig.profile. OSD_PROFILE_COUNT is 3 or 1 depending on USE_OSD_PROFILES.
#define PG_MAX_OSD_PROFILES 3
