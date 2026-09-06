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

#define PG_PACKED __attribute__((packed))

typedef uint16_t pgn_t;
typedef uint16_t pgLen_t;
typedef uint32_t pgSize_t;
typedef uint32_t pgHash_t;

typedef enum {
    PGR_PGN_MASK =          0x0fff,
    PGR_PGN_VERSION_MASK =  0xf000,
} pgRegistryInternal_e;

// function to reset the Parameter Group to default values
typedef void (pgResetFunc)(void * /* base */);

// Function to load the PG data into the active parameters.
typedef bool (pgLoadFunc)(void * /* base */);

// The PG registry is a table of all parameter groups, their sizes, etc.
typedef struct pgRegistry_s {
    pgn_t pgn;             // The parameter group number, the top 4 bits are reserved for version
    pgLen_t length;        // The number of elements in the group
    pgSize_t size;         // Size of the group in RAM
    uint8_t *data;         // Pointer to the group in RAM.
    uint8_t *copy;         // Pointer to the copy in RAM.
    union {
        void *data;        // Pointer to init template
        pgResetFunc *func; // Pointer to pgResetFunc
    } reset;
    union {
        void *ptr;         // Raw pointer
        pgLoadFunc *func;  // Pointer to pgLoadFunc
    } load;
    pgHash_t *checksum;    // Used to detect if config has changed
} pgRegistry_t;

static inline pgn_t pgN(const pgRegistry_t* reg) { return reg->pgn & PGR_PGN_MASK; }
static inline uint8_t pgVersion(const pgRegistry_t* reg) { return (uint8_t)(reg->pgn >> 12); }
static inline pgSize_t pgSize(const pgRegistry_t* reg) { return reg->size; }
static inline pgSize_t pgElementSize(const pgRegistry_t* reg) { return reg->size / reg->length; }
static inline uint8_t* pgData(const pgRegistry_t* reg) { return reg->data; }
static inline uint8_t* pgCopy(const pgRegistry_t* reg) { return reg->copy; }
static inline pgHash_t *pgChecksum(const pgRegistry_t* reg) { return reg->checksum; }

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

extern const uint8_t __pg_resetfunc_start[];
extern const uint8_t __pg_resetfunc_end[];
#define PG_RESETFUNC_ATTRIBUTES __attribute__ ((section(".pg_resetfunc"), used))

// Helper to iterate over the PG register.  Cheaper than a visitor style callback.
#define PG_FOREACH(_name) \
    for (const pgRegistry_t *(_name) = __pg_registry_start; (_name) < __pg_registry_end; (_name)++)

// Reset configuration to default (by name)
#define PG_RESET(_name)                                                 \
    do {                                                                \
        extern const pgRegistry_t __pgRegistry_ ## _name;               \
        pgReset(&__pgRegistry_ ## _name);                               \
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
#define PG_REGISTER_I(_type, _name, _pgn, _version, _reset, _load)      \
    _type _name ## _Data;                                               \
    _type _name ## _Copy;                                               \
    uint32_t _name ## _Checksum;                                        \
    extern const pgRegistry_t __pgRegistry_ ## _name;                   \
    const pgRegistry_t __pgRegistry_ ## _name PG_REGISTER_ATTRIBUTES = { \
        .pgn = _pgn | (_version << 12),                                 \
        .length = 1,                                                    \
        .size = sizeof(_type),                                          \
        .data = (uint8_t*)&_name ## _Data,                              \
        .copy = (uint8_t*)&_name ## _Copy,                              \
        .reset = _reset,                                                \
        .load = _load,                                                  \
        .checksum = &_name ## _Checksum,                                \
    }                                                                   \
    /**/

#define PG_REGISTER(_type, _name, _pgn, _version)                       \
    PG_REGISTER_I(_type, _name, _pgn, _version, {.data = 0}, {.ptr = 0}) \
    /**/

#define PG_REGISTER_WITH_RESET_FN(_type, _name, _pgn, _version)         \
    extern PG_RESET_FN(_type, _name);                                   \
    PG_REGISTER_I(_type, _name, _pgn, _version, {.func = (pgResetFunc*)&__pgResetFn_ ## _name}, {.ptr = 0}) \
    /**/

#define PG_REGISTER_WITH_RESET_TEMPLATE(_type, _name, _pgn, _version)   \
    extern const _type pgResetTemplate_ ## _name;                       \
    PG_REGISTER_I(_type, _name, _pgn, _version, {.data = (void*)&pgResetTemplate_ ## _name}, {.ptr = 0}) \
    /**/

#define PG_REGISTER_ARRAY_I(_type, _length, _name, _pgn, _version, _reset, _load) \
    _type _name ## _DataArray[_length];                                 \
    _type _name ## _CopyArray[_length];                                 \
    uint32_t _name ## _Checksum;                                        \
    extern const pgRegistry_t __pgRegistry_ ## _name;                   \
    const pgRegistry_t __pgRegistry_ ## _name PG_REGISTER_ATTRIBUTES = { \
        .pgn = _pgn | (_version << 12),                                 \
        .length = _length,                                              \
        .size = sizeof(_type) * _length,                                \
        .data = (uint8_t*)&_name ## _DataArray,                         \
        .copy = (uint8_t*)&_name ## _CopyArray,                         \
        .reset = _reset,                                                \
        .load = _load,                                                  \
        .checksum = &_name ## _Checksum,                                \
    }                                                                   \
    /**/

#define PG_REGISTER_ARRAY(_type, _length, _name, _pgn, _version)        \
    PG_REGISTER_ARRAY_I(_type, _length, _name, _pgn, _version, {.data = 0}, {.ptr = 0}) \
    /**/

#define PG_REGISTER_ARRAY_WITH_RESET_FN(_type, _length, _name, _pgn, _version) \
    extern PG_RESET_FN(_type, _name);                                   \
    PG_REGISTER_ARRAY_I(_type, _length, _name, _pgn, _version, {.func = (pgResetFunc*)&__pgResetFn_ ## _name}, {.ptr = 0}) \
    /**/

#define PG_ARRAY_ELEMENT_OFFSET(type, index, member) (index * sizeof(type) + offsetof(type, member))

#define PG_RESET_TEMPLATE(_type, _name, ...)                            \
    const _type pgResetTemplate_ ## _name PG_RESETDATA_ATTRIBUTES = {   \
        __VA_ARGS__                                                     \
    }                                                                   \
    /**/

#define PG_RESET_FN(_type, _name)                                       \
    void PG_RESETFUNC_ATTRIBUTES __pgResetFn_ ## _name(_type *_name)    \
    /**/

const pgRegistry_t* pgFind(pgn_t pgn);

bool pgLoad(const pgRegistry_t* reg, const void *from, pgSize_t size, int version);
int pgStore(const pgRegistry_t* reg, void *to, pgSize_t size);

void pgResetAll(void);
void pgResetInstance(const pgRegistry_t *reg, uint8_t *base);
bool pgResetCopy(void *copy, pgn_t pgn);
void pgReset(const pgRegistry_t* reg);
