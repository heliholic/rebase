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
#include <stdbool.h>

#include "build/build_config.h"

#ifndef PG_LAYOUT_PROBE
#include "pg_hash.h"
#endif

#define PG_PACKED __attribute__((packed))
#define PG_ALIGNED __attribute__((aligned(4)))

typedef uint16_t pgn_t;
typedef uint16_t pgLen_t;
typedef uint32_t pgSize_t;
typedef uint32_t pgHash_t;

// function that resets a single parameter group instance
typedef void (pgResetFunc)(void * /* base */);

// Must not be packed: PG_REGISTER_ATTRIBUTES aligns every entry to 4, so a
// packed layout makes sizeof() smaller than the stride PG_FOREACH walks.
typedef struct pgRegistry_s {
    pgn_t pgn;             // The parameter group number
    pgLen_t length;        // The number of elements in the group
    pgSize_t size;         // Size of the group in RAM
    pgHash_t hash;         // The parameter group version hash
    uint8_t *addr;         // Address of the group in RAM.
    uint8_t *copy;         // Address of the copy in RAM.
    uint8_t **ptr;         // The pointer to update after loading the record into ram.
    union {
        void *data;        // Pointer to init template
        pgResetFunc *func; // Pointer to pgResetFunc
    } reset;
    pgHash_t *checksum;    // Used to detect if config has changed prior to write
} PG_ALIGNED pgRegistry_t;

static inline pgn_t pgN(const pgRegistry_t* reg) { return reg->pgn; }
static inline pgHash_t pgHash(const pgRegistry_t* reg) { return reg->hash; }
static inline pgSize_t pgSize(const pgRegistry_t* reg) { return reg->size; }
static inline pgSize_t pgElementSize(const pgRegistry_t* reg) { return reg->size / reg->length; }
static inline uint8_t* pgAddress(const pgRegistry_t* reg) { return reg->addr; }
static inline uint8_t* pgCopy(const pgRegistry_t* reg) { return reg->copy; }
static inline pgHash_t *pgChecksum(const pgRegistry_t* reg) { return reg->checksum; }

extern const pgRegistry_t __pg_registry_start[];
extern const pgRegistry_t __pg_registry_end[];
#define PG_REGISTER_ATTRIBUTES __attribute__ ((section(".pg_registry"), used, aligned(4)))
#define PG_REGISTRY_SIZE (__pg_registry_end - __pg_registry_start)

extern const uint8_t __pg_resetdata_start[];
extern const uint8_t __pg_resetdata_end[];
#define PG_RESETDATA_ATTRIBUTES __attribute__ ((section(".pg_resetdata"), used, aligned(2)))

extern const uint8_t __pg_resetfunc_start[];
extern const uint8_t __pg_resetfunc_end[];
#define PG_RESETFN_ATTRIBUTES __attribute__ ((section(".pg_resetfunc"), used))

extern const uint8_t __pg_data_start[];
extern const uint8_t __pg_data_end[];
#define PG_DATA_ATTRIBUTES __attribute__ ((section(".pg_data"), aligned(4)))

// Helper to iterate over the PG register.  Cheaper than a visitor style callback.
#define PG_FOREACH(_name) \
    for (const pgRegistry_t *(_name) = __pg_registry_start; (_name) < __pg_registry_end; (_name)++)

// Reset configuration to default (by name)
#define PG_RESET(_name)                                                 \
    do {                                                                \
        extern const pgRegistry_t _name ##_Registry;                    \
        pgReset(&_name ## _Registry);                                   \
    } while (0)                                                         \
    /**/

// Declare system config
#define PG_DECLARE(_type, _name)                                        \
    extern _type _name ## _Data;                                        \
    extern _type _name ## _Copy;                                        \
    static inline const _type * _name(void) { return &_name ## _Data; } \
    static inline _type * _name ## Mutable(void) { return &_name ## _Data; } \
    struct _dummy                                                       \
    /**/

// Declare system config array
#define PG_DECLARE_ARRAY(_type, _length, _name)                         \
    extern _type _name ## _DataArray[_length];                          \
    extern _type _name ## _CopyArray[_length];                          \
    static inline const _type * _name(int _index) { return &_name ## _DataArray[_index]; } \
    static inline _type * _name ## Mutable(int _index) { return &_name ## _DataArray[_index]; } \
    static inline _type (* _name ## _array(void))[_length] { return &_name ## _DataArray; } \
    struct _dummy                                                       \
    /**/

// Register system config
#define PG_REGISTER_I(_type, _name, _pgn, _reset)                       \
    _type _name ## _Data PG_DATA_ATTRIBUTES;                            \
    _type _name ## _Copy PG_DATA_ATTRIBUTES;                            \
    uint32_t _name ## _checksum PG_DATA_ATTRIBUTES;                     \
    extern const pgRegistry_t _name ## _Registry;                       \
    const pgRegistry_t _name ##_Registry PG_REGISTER_ATTRIBUTES = {     \
        .pgn = _pgn,                                                    \
        .length = 1,                                                    \
        .size = sizeof(_type),                                          \
        .hash = _pgn ## _HASH,                                          \
        .addr = (uint8_t*)&_name ## _Data,                              \
        .copy = (uint8_t*)&_name ## _Copy,                              \
        .ptr = 0,                                                       \
        .reset = _reset,                                                \
        .checksum = &_name ## _checksum,                                \
    }                                                                   \
    /**/

#define PG_REGISTER(_type, _name, _pgn)                                 \
    PG_REGISTER_I(_type, _name, _pgn, {.data = 0})                      \
    /**/

#define PG_REGISTER_WITH_RESET_FN(_type, _name, _pgn)                   \
    extern PG_RESET_FN(_type, _name);                                   \
    PG_REGISTER_I(_type, _name, _pgn, {.func = (pgResetFunc*)&pgResetFn_ ## _name}) \
    /**/

#define PG_REGISTER_WITH_RESET_TEMPLATE(_type, _name, _pgn)             \
    extern const _type pgResetTemplate_ ## _name;                       \
    PG_REGISTER_I(_type, _name, _pgn, {.data = (void*)&pgResetTemplate_ ## _name}) \
    /**/

#define PG_REGISTER_ARRAY_I(_type, _length, _name, _pgn, _reset)        \
    _type _name ## _DataArray[_length] PG_DATA_ATTRIBUTES;              \
    _type _name ## _CopyArray[_length] PG_DATA_ATTRIBUTES;              \
    uint32_t _name ## _checksum PG_DATA_ATTRIBUTES;                     \
    extern const pgRegistry_t _name ##_Registry;                        \
    const pgRegistry_t _name ## _Registry PG_REGISTER_ATTRIBUTES = {    \
        .pgn = _pgn,                                                    \
        .length = _length,                                              \
        .size = sizeof(_type) * _length,                                \
        .hash = _pgn ## _HASH,                                          \
        .addr = (uint8_t*)&_name ## _DataArray,                         \
        .copy = (uint8_t*)&_name ## _CopyArray,                         \
        .ptr = 0,                                                       \
        .reset = _reset,                                                \
        .checksum = &_name ## _checksum,                                \
    }                                                                   \
    /**/

#define PG_REGISTER_ARRAY(_type, _length, _name, _pgn)                  \
    PG_REGISTER_ARRAY_I(_type, _length, _name, _pgn, {.data = 0})       \
    /**/

#define PG_REGISTER_ARRAY_WITH_RESET_FN(_type, _length, _name, _pgn)    \
    extern PG_RESET_FN(_type, _name);                                   \
    PG_REGISTER_ARRAY_I(_type, _length, _name, _pgn, {.func = (pgResetFunc*)&pgResetFn_ ## _name}) \
    /**/

#define PG_ARRAY_ELEMENT_OFFSET(type, index, member) (index * sizeof(type) + offsetof(type, member))

#define PG_RESET_TEMPLATE(_type, _name)                                 \
    const _type pgResetTemplate_ ## _name PG_RESETDATA_ATTRIBUTES =     \
    /**/

#define PG_RESET_FN(_type, _name)                                       \
    void PG_RESETFN_ATTRIBUTES pgResetFn_ ## _name(_type *_name)        \
    /**/

const pgRegistry_t* pgFind(pgn_t pgn);

bool pgLoad(const pgRegistry_t* reg, const void *from, pgSize_t size, pgHash_t hash);
int pgStore(const pgRegistry_t* reg, void *to, pgSize_t size);

void pgResetAll(void);
void pgResetInstance(const pgRegistry_t *reg, uint8_t *base);
bool pgResetCopy(void *copy, pgn_t pgn);
void pgReset(const pgRegistry_t* reg);
