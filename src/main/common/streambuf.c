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

void sbufWriteString(sbuf_t *dst, const char *string)
{
    uint8_t *ptr = dst->ptr;
    while (*string) {
        *ptr++ = *string++;
    }
    dst->ptr = ptr;
}

void sbufWritePString(sbuf_t *dst, const char *string)
{
    const int length = MIN((int)strlen(string), 255);
    sbufWriteU8(dst, length);
    sbufWriteData(dst, string, length);
}

void sbufWriteStringWithZeroTerminator(sbuf_t *dst, const char *string)
{
    uint8_t *ptr = dst->ptr;
    do {
        *ptr++ = *string;
    } while (*string++);
    dst->ptr = ptr;
}

// The readers are kept out-of-line: under LTO the end-bounds check would
// otherwise be inlined at every one of the ~hundreds of call sites, bloating
// flash on constrained targets. A single shared copy costs a call instead.
//
// A read that does not fit entirely within the buffer returns 0 and consumes
// whatever remains, so a truncated value is never mistaken for a real one and
// a following read cannot resynchronise part-way through it. The width is
// checked once and the fast path is a single unaligned load.

NOINLINE uint8_t sbufReadU8(sbuf_t *src)
{
    if (src->ptr < src->end) {
        return *src->ptr++;
    }
    return 0;
}

NOINLINE uint16_t sbufReadU16(sbuf_t *src)
{
    uint8_t *ptr = src->ptr;
    if (src->end - ptr >= 2) {
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
    if (src->end - ptr >= 3) {
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
    if (src->end - ptr >= 4) {
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
    if (src->end - ptr >= 8) {
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
    if (src->end - ptr >= 4) {
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
