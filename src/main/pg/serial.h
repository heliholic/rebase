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

#include "platform.h"

#include "pg/pg.h"
#include "pg/pg_limits.h"

#define SERIAL_PORT_COUNT (SERIAL_UART_COUNT + SERIAL_LPUART_COUNT + SERIAL_SOFTSERIAL_COUNT + SERIAL_VCP_COUNT + SERIAL_PIOUART_COUNT)

typedef struct {
    uint32_t    functionMask;
    int8_t      identifier;
    uint8_t     msp_baudrateIndex;
    uint8_t     gps_baudrateIndex;
    uint8_t     blackbox_baudrateIndex;
    uint8_t     telemetry_baudrateIndex;
} serialPortConfig_t;

typedef struct {
    // Stored at the fixed bound, not SERIAL_PORT_COUNT, so the group keeps one
    // layout across targets. Slots from SERIAL_PORT_COUNT up are reset to
    // SERIAL_PORT_NONE and skipped by everything that walks the array.
    serialPortConfig_t portConfigs[PG_MAX_SERIAL_PORTS];
    uint16_t    serial_update_rate_hz;
    uint8_t     reboot_character;
} serialConfig_t;

PG_DECLARE(serialConfig_t, serialConfig);
