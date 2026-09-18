/*
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "sbuf stores multi-byte values in target byte order; only little-endian targets are supported"
#endif

/*
 * simple buffer-based serializer/deserializer.
 *
 * Writers do not check available space. Readers are bounded by end and never
 * dereference past the buffer: a read that does not fit entirely within the
 * remaining bytes returns 0 and consumes the remainder, so a truncated value is
 * never mistaken for a real one. sbufReadData is the exception - it copies what
 * is available and zero-fills the shortfall.
 *
 * Lengths are size_t byte counts - a negative int wraps to an enormous length.
 * sbufAdvance clamps that to end; the data functions do not and will overrun.
 */

typedef struct sbuf_s {
    uint8_t *ptr;    // data pointer must be first (sbuf_t* is equivalent to uint8_t **)
    uint8_t *end;
} sbuf_t;

/* Init function */
sbuf_t *sbufInit(sbuf_t *sbuf, uint8_t *ptr, uint8_t *end);

/* Set to another position */
void sbufReset(sbuf_t *buf, uint8_t *ptr);

/* Move the cursor forward by len, clamped to end. Use sbufReset to go back. */
void sbufAdvance(sbuf_t *buf, size_t len);


/*
 * Store val at the write cursor, advancing it by the width of val.
 *
 * The memcpy lets the compiler ignore the alignment restrictions and emit a
 * single store. The cursor must be hoisted into a local first: a store through
 * a uint8_t* may alias dst->ptr itself, so reading dst->ptr again afterwards
 * forces the compiler to reload it.
 */

/* Write Unsigned Integers */
static inline void sbufWriteU8(sbuf_t *dst, uint8_t val)
{
    uint8_t *ptr = dst->ptr;
    __builtin_memcpy(ptr, &val, 1);
    dst->ptr = ptr + 1;
}

static inline void sbufWriteU16(sbuf_t *dst, uint16_t val)
{
    uint8_t *ptr = dst->ptr;
    __builtin_memcpy(ptr, &val, 2);
    dst->ptr = ptr + 2;
}

static inline void sbufWriteU24(sbuf_t *dst, uint32_t val)
{
    sbufWriteU16(dst, val);
    sbufWriteU8(dst, val >> 16);
}

static inline void sbufWriteU32(sbuf_t *dst, uint32_t val)
{
    uint8_t *ptr = dst->ptr;
    __builtin_memcpy(ptr, &val, 4);
    dst->ptr = ptr + 4;
}

static inline void sbufWriteU64(sbuf_t *dst, uint64_t val)
{
    uint8_t *ptr = dst->ptr;
    __builtin_memcpy(ptr, &val, 8);
    dst->ptr = ptr + 8;
}

static inline void sbufWriteFloat(sbuf_t *dst, float val)
{
    uint8_t *ptr = dst->ptr;
    __builtin_memcpy(ptr, &val, 4);
    dst->ptr = ptr + 4;
}

static inline void sbufWriteData(sbuf_t *dst, const void *data, size_t len)
{
    uint8_t *ptr = dst->ptr;
    __builtin_memcpy(ptr, data, len);
    dst->ptr = ptr + len;
}

static inline void sbufFill(sbuf_t *dst, uint8_t data, size_t len)
{
    uint8_t *ptr = dst->ptr;
    __builtin_memset(ptr, data, len);
    dst->ptr = ptr + len;
}


/* Write Unsigned Integers in Big-Endian */
static inline void sbufWriteU16BE(sbuf_t *dst, uint16_t val)    { sbufWriteU16(dst, __builtin_bswap16(val)); }
static inline void sbufWriteU24BE(sbuf_t *dst, uint32_t val)    { sbufWriteU8(dst, val >> 16); sbufWriteU16(dst, __builtin_bswap16(val)); }
static inline void sbufWriteU32BE(sbuf_t *dst, uint32_t val)    { sbufWriteU32(dst, __builtin_bswap32(val)); }
static inline void sbufWriteU64BE(sbuf_t *dst, uint64_t val)    { sbufWriteU64(dst, __builtin_bswap64(val)); }

/* Compat */
static inline void sbufWriteU16BigEndian(sbuf_t *dst, uint16_t val) { sbufWriteU16BE(dst, val); }
static inline void sbufWriteU32BigEndian(sbuf_t *dst, uint32_t val) { sbufWriteU32BE(dst, val); }

/* Write Signed Integers */
static inline void sbufWriteS8(sbuf_t *dst, int8_t val)         { sbufWriteU8(dst, val); }
static inline void sbufWriteS16(sbuf_t *dst, int16_t val)       { sbufWriteU16(dst, val); }
static inline void sbufWriteS24(sbuf_t *dst, int32_t val)       { sbufWriteU24(dst, val); }
static inline void sbufWriteS32(sbuf_t *dst, int32_t val)       { sbufWriteU32(dst, val); }
static inline void sbufWriteS64(sbuf_t *dst, int64_t val)       { sbufWriteU64(dst, val); }

/* Write Signed Integers in Big-Endian */
static inline void sbufWriteS16BE(sbuf_t *dst, int16_t val)     { sbufWriteU16BE(dst, val); }
static inline void sbufWriteS24BE(sbuf_t *dst, int32_t val)     { sbufWriteU24BE(dst, val); }
static inline void sbufWriteS32BE(sbuf_t *dst, int32_t val)     { sbufWriteU32BE(dst, val); }
static inline void sbufWriteS64BE(sbuf_t *dst, int64_t val)     { sbufWriteU64BE(dst, val); }


/* Write Data and strings */
void sbufWriteString(sbuf_t *dst, const char *string);
void sbufWritePString(sbuf_t *dst, const char *string);
void sbufWriteStringWithZeroTerminator(sbuf_t *dst, const char *string);

/* Read Unsigned Integers */
uint8_t sbufReadU8(sbuf_t *src);
uint16_t sbufReadU16(sbuf_t *src);
uint32_t sbufReadU24(sbuf_t *src);
uint32_t sbufReadU32(sbuf_t *src);
uint64_t sbufReadU64(sbuf_t *src);

/* Read Big-Endian Unsigned Integers */
static inline uint8_t sbufReadU8BE(sbuf_t *src)     { return sbufReadU8(src); }
static inline uint16_t sbufReadU16BE(sbuf_t *src)   { return __builtin_bswap16(sbufReadU16(src)); }
static inline uint32_t sbufReadU24BE(sbuf_t *src)   { return __builtin_bswap32(sbufReadU24(src)) >> 8; }
static inline uint32_t sbufReadU32BE(sbuf_t *src)   { return __builtin_bswap32(sbufReadU32(src)); }
static inline uint64_t sbufReadU64BE(sbuf_t *src)   { return __builtin_bswap64(sbufReadU64(src)); }

/* Read Signed Integers */
static inline int8_t sbufReadS8(sbuf_t *src)        { return sbufReadU8(src); }
static inline int16_t sbufReadS16(sbuf_t *src)      { return sbufReadU16(src); }
static inline int32_t sbufReadS24(sbuf_t *src)      { return (int32_t)(sbufReadU24(src) << 8) >> 8; }
static inline int32_t sbufReadS32(sbuf_t *src)      { return sbufReadU32(src); }
static inline int64_t sbufReadS64(sbuf_t *src)      { return sbufReadU64(src); }

/* Read Big-Endian Signed Integers */
static inline int16_t sbufReadS16BE(sbuf_t *src)    { return sbufReadU16BE(src); }
static inline int32_t sbufReadS24BE(sbuf_t *src)    { return (int32_t)(sbufReadU24BE(src) << 8) >> 8; }
static inline int32_t sbufReadS32BE(sbuf_t *src)    { return sbufReadU32BE(src); }
static inline int64_t sbufReadS64BE(sbuf_t *src)    { return sbufReadU64BE(src); }

/* Read float */
float sbufReadFloat(sbuf_t *src);

/* Read Data and Strings */
void sbufReadData(sbuf_t *dst, void *data, size_t len);

/* Space left in the buffer */
static inline int sbufBytesRemaining(sbuf_t *buf) { return (buf->end - buf->ptr); }

/* Get buffer pointer */
static inline uint8_t *sbufPtr(sbuf_t *buf) { return buf->ptr; }
static inline const uint8_t *sbufConstPtr(const sbuf_t *buf) { return buf->ptr; }

/* Prepare buffer for reading */
void sbufSwitchToReader(sbuf_t *buf, uint8_t *base);
