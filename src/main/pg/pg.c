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
#include "common/utils.h"

#include "pg.h"

// PG_REGISTER_ATTRIBUTES and the linker SUBALIGN pack entries on a 4-byte
// stride. PG_FOREACH walks by sizeof(pgRegistry_t), so both the size and the
// alignment of the struct must be multiples of 4 (the alignment comes out as 4
// on MCUs and 8 on 64-bit hosts). Neither is forced by an attribute -- they
// follow from the members -- so a member change could break them silently.
STATIC_ASSERT(sizeof(pgRegistry_t) % 4 == 0, pg_registry_size_not_multiple_of_4);
STATIC_ASSERT(_Alignof(pgRegistry_t) % 4 == 0, pg_registry_alignment_not_multiple_of_4);

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

    if (reg->reset.data) {
        if (reg->reset.data >= (void*)__pg_resetdata_start && reg->reset.data < (void*)__pg_resetdata_end) {
            // pointer points into .pg_resetdata, so it is a data template
            memcpy(base, reg->reset.data, pgSize(reg));
        }
        else if (reg->reset.data >= (void*)__pg_resetfunc_start && reg->reset.data < (void*)__pg_resetfunc_end) {
            // pointer points into .pg_resetfunc (or is otherwise a reset function)
            reg->reset.func(base);
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
