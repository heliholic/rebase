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

#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "platform.h"

#include "common/maths.h"
#include "common/utils.h"

#include "streambuf.h"

sbuf_t *sbufInit(sbuf_t *sbuf, uint8_t *ptr, uint8_t *end)
{
    sbuf->ptr = ptr;
    sbuf->end = end;
    return sbuf;
}

void sbufReset(sbuf_t *buf, uint8_t *ptr)
{
    buf->ptr = ptr;
}

void sbufAdvance(sbuf_t *buf, size_t len)
{
    const ptrdiff_t remaining = buf->end - buf->ptr;
    if (remaining > 0 && len < (size_t)remaining) {
        buf->ptr += len;
    } else {
        buf->ptr = buf->end;
    }
}

void sbufSwitchToReader(sbuf_t *buf, uint8_t *base)
{
    buf->end = buf->ptr;
    buf->ptr = base;
}

/* True if len more bytes fit between the cursor and end. */
static inline bool sbufFits(const sbuf_t *buf, size_t len)
{
    const ptrdiff_t remaining = buf->end - buf->ptr;
    return remaining >= 0 && (size_t)remaining >= len;
}

/*
 * Writers and readers alike are kept out-of-line: the bounds check would
 * otherwise be inlined at every one of the ~800 write and ~400 read call
 * sites, bloating flash on constrained targets. A single shared copy costs a
 * call instead. Inlining the writers costs a further 4764 bytes of flash on an
 * STM32F722 build, which buys a handful of cycles in code that is only ever
 * assembling a serial frame.
 *
 * A value that does not fit entirely within the buffer is not transferred at
 * all, and the cursor is parked on end: a truncated value is never mistaken
 * for a real one, and a later, narrower value cannot slip into the gap that a
 * wider one could not fill. The width is checked once and the fast path is a
 * single unaligned access.
 */

/*
 * Store len bytes of *val at the write cursor, advancing it by len.
 *
 * The memcpy lets the compiler ignore the alignment restrictions and emit a
 * single store, but only while len is a constant here, which is why this has
 * to be inlined into its callers below. The cursor must be hoisted into a
 * local first: a store through a uint8_t* may alias dst->ptr itself, so
 * reading dst->ptr again afterwards forces the compiler to reload it.
 */
__attribute__((always_inline))
static inline void sbufWrite(sbuf_t *dst, const void *val, size_t len)
{
    if (sbufFits(dst, len)) {
        uint8_t *ptr = dst->ptr;
        __builtin_memcpy(ptr, val, len);
        dst->ptr = ptr + len;
    } else {
        dst->ptr = dst->end;
    }
}

/*
 * The width each writer commits to comes from the type of its own argument,
 * so it cannot drift away from the name.
 */
NOINLINE void sbufWriteU8(sbuf_t *dst, uint8_t val)     { sbufWrite(dst, &val, sizeof(val)); }
NOINLINE void sbufWriteU16(sbuf_t *dst, uint16_t val)   { sbufWrite(dst, &val, sizeof(val)); }
NOINLINE void sbufWriteU32(sbuf_t *dst, uint32_t val)   { sbufWrite(dst, &val, sizeof(val)); }
NOINLINE void sbufWriteU64(sbuf_t *dst, uint64_t val)   { sbufWrite(dst, &val, sizeof(val)); }
NOINLINE void sbufWriteFloat(sbuf_t *dst, float val)    { sbufWrite(dst, &val, sizeof(val)); }

/*
 * U24 is the one width with no type of its own, and copying three bytes out of
 * a four-byte value makes the compiler spill it to the stack and read the
 * bytes back. Store the low half and the top byte separately instead, the same
 * way sbufReadU24 fetches them.
 */
NOINLINE void sbufWriteU24(sbuf_t *dst, uint32_t val)
{
    if (sbufFits(dst, 3)) {
        uint8_t *ptr = dst->ptr;
        const uint16_t lo = val;
        __builtin_memcpy(ptr, &lo, 2);
        ptr[2] = val >> 16;
        dst->ptr = ptr + 3;
    } else {
        dst->ptr = dst->end;
    }
}

NOINLINE void sbufWriteData(sbuf_t *dst, const void *data, size_t len)
{
    if (sbufFits(dst, len)) {
        memcpy(dst->ptr, data, len);
        dst->ptr += len;
    } else {
        dst->ptr = dst->end;
    }
}

NOINLINE void sbufFill(sbuf_t *dst, uint8_t data, size_t len)
{
    if (sbufFits(dst, len)) {
        memset(dst->ptr, data, len);
        dst->ptr += len;
    } else {
        dst->ptr = dst->end;
    }
}

/*
 * The strings go through sbufWriteData so that they obey the same rule as
 * every other value: a string that does not fit is not written at all, rather
 * than being cut off wherever end happens to fall.
 */

void sbufWriteString(sbuf_t *dst, const char *string)
{
    sbufWriteData(dst, string, strlen(string));
}

void sbufWriteStringWithZeroTerminator(sbuf_t *dst, const char *string)
{
    sbufWriteData(dst, string, strlen(string) + 1);
}

void sbufWritePString(sbuf_t *dst, const char *string)
{
    const size_t length = MIN(strlen(string), (size_t)255);
    if (sbufFits(dst, length + 1)) {
        sbufWriteU8(dst, length);
        sbufWriteData(dst, string, length);
    } else {
        dst->ptr = dst->end;
    }
}

/*
 * A read that comes up short consumes whatever remains as well as returning
 * 0, so a following read cannot resynchronise part-way through the value it
 * could not have.
 */

NOINLINE uint8_t sbufReadU8(sbuf_t *src)
{
    if (sbufFits(src, 1)) {
        return *src->ptr++;
    }
    return 0;
}

NOINLINE uint16_t sbufReadU16(sbuf_t *src)
{
    uint8_t *ptr = src->ptr;
    if (sbufFits(src, 2)) {
        uint16_t val;
        __builtin_memcpy(&val, ptr, 2);
        src->ptr = ptr + 2;
        return val;
    }
    src->ptr = src->end;
    return 0;
}

NOINLINE uint32_t sbufReadU24(sbuf_t *src)
{
    uint8_t *ptr = src->ptr;
    if (sbufFits(src, 3)) {
        uint16_t val;
        __builtin_memcpy(&val, ptr, 2);
        src->ptr = ptr + 3;
        return val | ((uint32_t)ptr[2] << 16);
    }
    src->ptr = src->end;
    return 0;
}

NOINLINE uint32_t sbufReadU32(sbuf_t *src)
{
    uint8_t *ptr = src->ptr;
    if (sbufFits(src, 4)) {
        uint32_t val;
        __builtin_memcpy(&val, ptr, 4);
        src->ptr = ptr + 4;
        return val;
    }
    src->ptr = src->end;
    return 0;
}

NOINLINE uint64_t sbufReadU64(sbuf_t *src)
{
    uint8_t *ptr = src->ptr;
    if (sbufFits(src, 8)) {
        uint64_t val;
        __builtin_memcpy(&val, ptr, 8);
        src->ptr = ptr + 8;
        return val;
    }
    src->ptr = src->end;
    return 0;
}

NOINLINE float sbufReadFloat(sbuf_t *src)
{
    uint8_t *ptr = src->ptr;
    if (sbufFits(src, 4)) {
        float val;
        __builtin_memcpy(&val, ptr, 4);
        src->ptr = ptr + 4;
        return val;
    }
    src->ptr = src->end;
    return 0;
}

NOINLINE void sbufReadData(sbuf_t *src, void *data, size_t len)
{
    const ptrdiff_t remaining = src->end - src->ptr;
    const size_t available = (remaining > 0) ? (size_t)remaining : 0;
    const size_t toCopy = MIN(len, available);
    memcpy(data, src->ptr, toCopy);
    if (toCopy < len) {
        memset((uint8_t *)data + toCopy, 0, len - toCopy);
    }
    src->ptr += toCopy;
}
