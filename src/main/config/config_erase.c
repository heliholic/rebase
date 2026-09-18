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

#include <stdint.h>

#include "platform.h"

/*
 * Erase the stored config when this image is flashed.
 *
 * The linker scripts always have a .flash_config output section, which
 * fills the FLASH_CONFIG region with 0xFF once __config_erase is defined.
 * Without this word the section stays empty, carries no contents, and
 * objcopy leaves it out of the .hex; with it, the section covers the whole
 * region and flashing the image leaves no config behind.
 *
 * Enabled with the Makefile flag FLASH_CONFIG_ERASE=yes, which is meant
 * for snapshot images only. Release images should _NEVER_ use it.
 */

#ifdef USE_CONFIG_ERASE

uint32_t __config_erase __attribute__ ((section(".flash_config"), aligned(4), used)) = 0xFFFFFFFF;

#endif
