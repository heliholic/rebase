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
 * Both directions are bounded by end and never touch a byte outside the
 * buffer. A value that does not fit entirely in the space that is left is not
 * transferred at all: a reader returns 0, a writer stores nothing, and either
 * way the cursor is parked on end. A truncated value is therefore never
 * mistaken for a real one, and once a buffer has filled up every later access
 * fails too, rather than letting a narrow field slip into the gap that a wider
 * one could not fill. sbufReadData is the exception - it copies what is
 * available and zero-fills the shortfall.
 *
 * sbufBytesRemaining() == 0 after a run of writes therefore means the buffer
 * is full and may have overflowed; the bytes between the last value stored and
 * end, if any, still hold whatever was there before.
 *
 * Lengths are size_t byte counts, so a negative int wraps to an enormous
 * length; that fails the bounds check like any other oversized value.
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


/* Write Unsigned Integers */
void sbufWriteU8(sbuf_t *dst, uint8_t val);
void sbufWriteU16(sbuf_t *dst, uint16_t val);
void sbufWriteU24(sbuf_t *dst, uint32_t val);
void sbufWriteU32(sbuf_t *dst, uint32_t val);
void sbufWriteU64(sbuf_t *dst, uint64_t val);

/* Write float */
void sbufWriteFloat(sbuf_t *dst, float val);

/* Write a block, or a run of the same byte */
void sbufWriteData(sbuf_t *dst, const void *data, size_t len);
void sbufFill(sbuf_t *dst, uint8_t data, size_t len);

/* Write Unsigned Integers in Big-Endian */
static inline void sbufWriteU16BE(sbuf_t *dst, uint16_t val)    { sbufWriteU16(dst, __builtin_bswap16(val)); }
static inline void sbufWriteU24BE(sbuf_t *dst, uint32_t val)    { sbufWriteU24(dst, __builtin_bswap32(val) >> 8); }
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
