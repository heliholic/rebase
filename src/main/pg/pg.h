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

#define PG_UNUSED __attribute__((unused))

// Applied to an enum whose values are stored in a parameter group.
//
// The width has to be the same in every build. arm-none-eabi compiles with
// -fshort-enums and the host toolchain does not, so an unannotated enum member
// is one byte in the firmware and four in SITL and the unit tests. That is a
// layout difference: it moves every field behind it, changes the group's hash,
// and leaves the two builds unable to read each other's stored config.
//
// Packing rather than storing the value as a plain integer keeps the enum type
// in the struct, so its enumerator names and values stay part of the layout
// hash. Renumbering or reordering an enum changes what a stored byte means, and
// the hash has to catch that.
#define PG_ENUM __attribute__((packed))

typedef uint16_t pgn_t;
typedef uint16_t pgLen_t;
typedef uint32_t pgSize_t;
typedef uint32_t pgHash_t;

// Function to reset the Parameter Group element to default values.
typedef void (pgResetFunc)(void * /* base */, int index);

// Function to load the PG data into the active parameters.
typedef bool (pgLoadFunc)(void * /* base */);

// The PG registry is a table of all parameter groups, their sizes, etc.
typedef struct pgRegistry_s {
    pgn_t pgn;             // The parameter group number
    pgLen_t length;        // The number of elements in the group
    pgSize_t size;         // Size of the element
    pgHash_t hash;         // The parameter group layout hash
    uint8_t *data;         // Pointer to the group in RAM.
    uint8_t *copy;         // Pointer to the copy in RAM.
    union {
        uint8_t *data;     // Pointer to reset template
        pgResetFunc *func; // Pointer to pgResetFunc
    } reset;
    pgLoadFunc *load;      // Pointer to pgLoadFunc
    pgHash_t *checksum;    // Used to detect if config has changed
} pgRegistry_t;

static inline pgn_t pgN(const pgRegistry_t* reg) { return reg->pgn; }
static inline pgSize_t pgSize(const pgRegistry_t* reg) { return reg->size * reg->length; }
static inline pgHash_t pgHash(const pgRegistry_t* reg) { return reg->hash; }

static inline uint8_t* pgElement(const pgRegistry_t* reg, int index) { return reg->data + reg->size * index; }
static inline uint8_t* pgElementPtr(const pgRegistry_t* reg, uint8_t *base, int index) { return base + reg->size * index; }
static inline pgSize_t pgElementSize(const pgRegistry_t* reg) { return reg->size; }
static inline pgSize_t pgElementCount(const pgRegistry_t* reg) { return reg->length; }

static inline uint8_t* pgData(const pgRegistry_t* reg) { return reg->data; }
static inline uint8_t* pgCopy(const pgRegistry_t* reg) { return reg->copy; }

static inline pgHash_t pgGetChecksum(const pgRegistry_t* reg) { return *reg->checksum; }
static inline void pgSetChecksum(const pgRegistry_t* reg, pgHash_t hash) { *reg->checksum = hash; }

extern const pgRegistry_t __pg_registry_start[];
extern const pgRegistry_t __pg_registry_end[];
// aligned() must track _Alignof(pgRegistry_t), not a constant. A literal
// below the natural alignment does not under-align the object, but it does
// lower the alignment recorded on the input section - gcc and clang both emit
// .pg_registry as sh_addralign 4 for aligned(4) - and the linker is then free
// to place the section on a 4-byte boundary, which under-aligns every entry on
// a 64-bit host. Stating it explicitly also keeps the toolchain from boosting
// the objects to an alignment of its own, which would pad the gaps between
// object files and break the array walk.
#define PG_REGISTER_ATTRIBUTES __attribute__ ((section(".pg_registry"), used, aligned(__alignof__(pgRegistry_t))))
#define PG_REGISTRY_SIZE (__pg_registry_end - __pg_registry_start)

extern const uint8_t __pg_resetdata_start[];
extern const uint8_t __pg_resetdata_end[];
#define PG_RESETDATA_ATTRIBUTES __attribute__ ((section(".pg_resetdata"), used, aligned(2)))

extern const uint8_t __pg_resetarray_start[];
extern const uint8_t __pg_resetarray_end[];
#define PG_RESETARRAY_ATTRIBUTES __attribute__ ((section(".pg_resetarray"), used, aligned(2)))

extern const uint8_t __pg_resetfunc_start[];
extern const uint8_t __pg_resetfunc_end[];
#define PG_RESETFUNC_ATTRIBUTES __attribute__ ((section(".pg_resetfunc"), used))

// Helper to iterate over the PG register.  Cheaper than a visitor style callback.
#define PG_FOREACH(_name) \
    for (const pgRegistry_t *(_name) = __pg_registry_start; (_name) < __pg_registry_end; (_name)++)

// Reset configuration to default (by name)
#define PG_RESET(_name)                                                 \
    do {                                                                \
        extern const pgRegistry_t _pgRegistry_ ## _name;                \
        pgReset(&_pgRegistry_ ## _name);                                \
    } while (0)                                                         \
    /**/

// Declare system config
#define PG_DECLARE(_type, _name)                                        \
    extern _type _pgData_ ## _name;                                     \
    extern _type _pgCopy_ ## _name;                                     \
    static inline const _type * _name(void) { return &_pgData_ ## _name; }  \
    static inline _type * _name ## Mutable(void) { return &_pgData_ ## _name; }  \
    static inline _type * _name ## Copy(void) { return &_pgCopy_ ## _name; }  \
    struct _dummy                                                       \
    /**/

// Declare system config array
#define PG_DECLARE_ARRAY(_type, _length, _name)                         \
    extern _type _pgData_ ## _name[_length];                            \
    extern _type _pgCopy_ ## _name[_length];                            \
    static inline const _type * _name(int _index) { return &_pgData_ ## _name[_index]; }  \
    static inline _type * _name ## Mutable(int _index) { return &_pgData_ ## _name[_index]; }  \
    static inline _type * _name ## Copy(int _index) { return &_pgCopy_ ## _name[_index]; }  \
    struct _dummy                                                       \
    /**/

// Register system config
#define PG_REGISTER_CONFIG(_name, _type, _pgn, _reset, _load)           \
    _type _pgData_ ## _name;                                            \
    _type _pgCopy_ ## _name;                                            \
    pgHash_t _name ## _Checksum;                                        \
    extern const pgRegistry_t _pgRegistry_ ## _name;                    \
    const pgRegistry_t _pgRegistry_ ## _name PG_REGISTER_ATTRIBUTES = { \
        .pgn = _pgn,                                                    \
        .length = 1,                                                    \
        .size = sizeof(_type),                                          \
        .hash = _pgn ## _HASH,                                          \
        .data = (uint8_t*)&_pgData_ ## _name,                           \
        .copy = (uint8_t*)&_pgCopy_ ## _name,                           \
        .reset = {_reset},                                              \
        .load = _load,                                                  \
        .checksum = &_name ## _Checksum,                                \
    }                                                                   \
    /**/

// Register system config array
#define PG_REGISTER_CONFIG_ARRAY(_name, _type, _length, _pgn, _reset, _load) \
    _type _pgData_ ## _name[_length];                                   \
    _type _pgCopy_ ## _name[_length];                                   \
    pgHash_t _name ## _Checksum;                                        \
    extern const pgRegistry_t _pgRegistry_ ## _name;                    \
    const pgRegistry_t _pgRegistry_ ## _name PG_REGISTER_ATTRIBUTES = { \
        .pgn = _pgn,                                                    \
        .length = _length,                                              \
        .size = sizeof(_type),                                          \
        .hash = _pgn ## _HASH,                                          \
        .data = (uint8_t*)&_pgData_ ## _name,                           \
        .copy = (uint8_t*)&_pgCopy_ ## _name,                           \
        .reset = {_reset},                                              \
        .load = _load,                                                  \
        .checksum = &_name ## _Checksum,                                \
    }                                                                   \
    /**/

#define PG_RESET_TEMPLATE(_type, _name)                                 \
    const _type _pgResetTemplate_ ## _name PG_RESETDATA_ATTRIBUTES =    \
    /**/

#define PG_RESET_TEMPLATE_ARRAY(_type, _length, _name)                  \
    const _type _pgResetTemplate_ ## _name[_length] PG_RESETARRAY_ATTRIBUTES =  \
    /**/

#define PG_RESET_FN(_type, _name)                                       \
    void PG_RESETFUNC_ATTRIBUTES _pgResetFn_ ## _name(_type *_name, int PG_UNUSED index)  \
    /**/

#define PG_REGISTER(_type, _name, _pgn)                                 \
    PG_REGISTER_CONFIG(_name, _type, _pgn, .data = 0, 0)                \
    /**/

#define PG_REGISTER_WITH_RESET_FN(_type, _name, _pgn)                   \
    extern PG_RESET_FN(_type, _name);                                   \
    PG_REGISTER_CONFIG(_name, _type, _pgn, .func = (pgResetFunc*)&_pgResetFn_ ## _name, 0)  \
    /**/

#define PG_REGISTER_WITH_RESET_TEMPLATE(_type, _name, _pgn)             \
    extern const _type _pgResetTemplate_ ## _name;                      \
    PG_REGISTER_CONFIG(_name, _type, _pgn, .data = (uint8_t*)&_pgResetTemplate_ ## _name, 0)  \
    /**/

#define PG_REGISTER_ARRAY(_type, _length, _name, _pgn)                  \
    PG_REGISTER_CONFIG_ARRAY(_name, _type, _length, _pgn, .data = 0, 0) \
    /**/

#define PG_REGISTER_ARRAY_WITH_RESET_FN(_type, _length, _name, _pgn)    \
    extern PG_RESET_FN(_type, _name);                                   \
    PG_REGISTER_CONFIG_ARRAY(_name, _type, _length, _pgn, .func = (pgResetFunc*)&_pgResetFn_ ## _name, 0)  \
    /**/

#define PG_REGISTER_ARRAY_WITH_RESET_TEMPLATE(_type, _length, _name, _pgn) \
    extern const _type _pgResetTemplate_ ## _name;                      \
    PG_REGISTER_CONFIG_ARRAY(_name, _type, _length, _pgn, .data = (uint8_t*)&_pgResetTemplate_ ## _name, 0)  \
    /**/

#define PG_REGISTER_ARRAY_WITH_RESET_ARRAY(_type, _length, _name, _pgn) \
    extern const _type _pgResetTemplate_ ## _name[_length];             \
    PG_REGISTER_CONFIG_ARRAY(_name, _type, _length, _pgn, .data = (uint8_t*)&_pgResetTemplate_ ## _name, 0)  \
    /**/

#define PG_ARRAY_ELEMENT_OFFSET(type, index, member) (index * sizeof(type) + offsetof(type, member))

const pgRegistry_t* pgFind(pgn_t pgn);

void pgLoad(const pgRegistry_t* reg, const void *from, pgSize_t size);
int pgStore(const pgRegistry_t* reg, void *to, pgSize_t size);

void pgReset(const pgRegistry_t* reg);
void pgResetAll(void);

bool pgResetBuf(void *buf, pgn_t pgn);
