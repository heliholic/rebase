/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
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

#include "pg/pg_limits.h"

/*
  #defines used for serial port resource access (pin/dma/inversion)
  target/serial_post.h normalizes enabled port definitions (and port counts),
  values are just renamed here
 */

// use _MAX value here, resource command needs linear mapping
//  (UART8 is always at index RESOURCE_UART_OFFSET + 7, no matter which other ports are enabled)
#define RESOURCE_UART_COUNT SERIAL_UART_MAX
#define RESOURCE_LPUART_COUNT SERIAL_LPUART_MAX
#define RESOURCE_SOFTSERIAL_COUNT SERIAL_SOFTSERIAL_MAX
#define RESOURCE_PIOUART_COUNT SERIAL_PIOUART_MAX
#define RESOURCE_SERIAL_COUNT (RESOURCE_UART_COUNT + RESOURCE_LPUART_COUNT + RESOURCE_SOFTSERIAL_COUNT + RESOURCE_PIOUART_COUNT)

// resources are stored in one array, in UART, LPUART, SOFTSERIAL, PIOUART order. Code does assume this ordering,
//  do not change it without adapting relevant code.
//
// The blocks are laid out at the fixed sizes in pg/pg_limits.h rather than at
// this target's counts. The offsets are part of what a stored slot means: the
// layout hash no longer distinguishes targets by port count, so deriving them
// from RESOURCE_*_COUNT would let a record written where SERIAL_UART_MAX is 6
// load somewhere it is 15 and read that target's UART7 pin out of the slot
// holding LPUART1's.
#define RESOURCE_UART_OFFSET 0
#define RESOURCE_LPUART_OFFSET PG_MAX_UART_RESOURCES
#define RESOURCE_SOFTSERIAL_OFFSET (PG_MAX_UART_RESOURCES + PG_MAX_LPUART_RESOURCES)
#define RESOURCE_PIOUART_OFFSET (PG_MAX_UART_RESOURCES + PG_MAX_LPUART_RESOURCES + PG_MAX_SOFTSERIAL_RESOURCES)
