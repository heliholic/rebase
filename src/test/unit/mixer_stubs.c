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

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "common/utils.h"

#include "flight/mixer.h"

// The mixer runtime as seen by a test which links the mixer configuration but
// not the mixer itself. pg/mixer.c supplies the groups these leave alone.

float mixerGetOutput(uint8_t index)
{
    UNUSED(index);
    return 0;
}

int16_t mixerGetOverride(uint8_t index)
{
    UNUSED(index);
    return MIXER_OVERRIDE_OFF;
}

int16_t mixerSetOverride(uint8_t index, int16_t value)
{
    UNUSED(index);
    return value;
}

void mixerInitMapping(void)
{
}
