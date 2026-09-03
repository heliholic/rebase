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

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "platform.h"

#include "common/crc.h"
#include "common/maths.h"

#include "pg.h"

const pgRegistry_t* pgFind(pgn_t pgn)
{
    PG_FOREACH(reg) {
        if (pgN(reg) == pgn) {
            return reg;
        }
    }
    return NULL;
}

void pgResetInstance(const pgRegistry_t *reg, uint8_t *base)
{
    memset(base, 0, pgSize(reg));

    if (reg->reset.ptr) {
        if (reg->reset.ptr >= (void*)__pg_resetdata_start && reg->reset.ptr < (void*)__pg_resetdata_end) {
            // pointer points to resetdata section, to it is data template
            memcpy(base, reg->reset.ptr, pgSize(reg));
        } else {
            // reset function, call it
            reg->reset.fn(base);
        }
    }
}

void pgReset(const pgRegistry_t* reg)
{
    pgResetInstance(reg, pgAddress(reg));
}

bool pgResetCopy(void *copy, pgn_t pgn)
{
    const pgRegistry_t *reg = pgFind(pgn);
    if (reg) {
        pgResetInstance(reg, copy);
        return true;
    }
    return false;
}

void pgResetAll(void)
{
    PG_FOREACH(reg) {
        pgReset(reg);
    }
}

bool pgLoad(const pgRegistry_t* reg, const void *from, pgSize_t size, pgHash_t hash)
{
    pgResetInstance(reg, pgAddress(reg));

    // restore only matching version hash, keep defaults otherwise
    if (hash == pgHash(reg)) {
        const int take = MIN(size, pgSize(reg));
        memcpy(pgAddress(reg), from, take);

        *pgChecksum(reg) = fnv_update(FNV_OFFSET_BASIS, from, take);

        return true;
    }

    return false;
}

int pgStore(const pgRegistry_t* reg, void *to, pgSize_t size)
{
    const size_t take = MIN(size, pgSize(reg));
    memcpy(to, pgAddress(reg), take);
    return take;
}
