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

/// sbuf is a wire-format serialiser: MSP, CRSF, GHST, SRXL and the VTX
/// protocols all put bytes on a link through it, so the byte layout it
/// produces is a contract with the other end. These tests pin that layout
/// down explicitly rather than round-tripping through sbuf itself, which
/// would pass happily if both ends were wrong in the same way.
///
/// The invariants worth defending, and why:
///
///   layout      writers emit target-order bytes via memcpy, so an endianness
///               or width slip is silent at compile time
///   alignment   the write cursor lands on arbitrary offsets, so no store may
///               assume alignment (a typed store would let the compiler fold
///               a pair into STRD, which faults on ARMv7-M when unaligned)
///   fusion      adjacent inline writes are deliberately merged by the
///               compiler into wider stores; the merge must be byte-exact
///   width       SBUF_WRITE takes its size from the type of its argument, so
///               a missing cast would promote to int and write four bytes
///   bounds      readers stop at end, return 0 past it, and never dereference
///               beyond the buffer

#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

extern "C" {
    #include "common/streambuf.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

namespace {

using Bytes = std::vector<uint8_t>;

// Anything the code under test must never hand back. Reads past the end are
// specified to yield 0, so poison surfacing in a result proves an over-read,
// and poison surviving in the guard bands proves no write escaped the buffer.
constexpr uint8_t kPoison = 0xA5;

class SbufTest : public ::testing::Test {
protected:
    static constexpr size_t kGuard    = 16;
    static constexpr size_t kCapacity = 512;

    uint8_t storage[kGuard + kCapacity + kGuard];
    uint8_t *buf;
    sbuf_t sb;

    void SetUp() override
    {
        memset(storage, kPoison, sizeof(storage));
        buf = storage + kGuard;
        sbufInit(&sb, buf, buf + kCapacity);
    }

    void TearDown() override
    {
        EXPECT_TRUE(guardsIntact()) << "a write escaped the buffer";
    }

    bool guardsIntact() const
    {
        for (size_t i = 0; i < kGuard; i++) {
            if (storage[i] != kPoison) return false;
            if (storage[kGuard + kCapacity + i] != kPoison) return false;
        }
        return true;
    }

    /* Point the buffer at `data` and cap `end` tightly, leaving poison beyond
       so an over-read is visible rather than reading incidental zeroes. */
    void reader(const Bytes &data)
    {
        memcpy(buf, data.data(), data.size());
        sbufInit(&sb, buf, buf + data.size());
    }

    /* Bytes committed so far. */
    Bytes written() const { return Bytes(buf, sb.ptr); }

    int used() const { return static_cast<int>(sb.ptr - buf); }
};

/* ------------------------------------------------------------------------ */
/* Wire layout - little-endian writers                                       */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, WriteU8Layout)
{
    sbufWriteU8(&sb, 0x12);
    EXPECT_EQ(written(), Bytes({0x12}));
    EXPECT_EQ(used(), 1);
}

TEST_F(SbufTest, WriteU16Layout)
{
    sbufWriteU16(&sb, 0x1234);
    EXPECT_EQ(written(), Bytes({0x34, 0x12}));
}

TEST_F(SbufTest, WriteU24Layout)
{
    sbufWriteU24(&sb, 0x123456);
    EXPECT_EQ(written(), Bytes({0x56, 0x34, 0x12}));
}

TEST_F(SbufTest, WriteU24DiscardsTopByte)
{
    sbufWriteU24(&sb, 0xFF123456);
    EXPECT_EQ(written(), Bytes({0x56, 0x34, 0x12}));
    EXPECT_EQ(used(), 3) << "U24 must emit exactly three bytes";
}

TEST_F(SbufTest, WriteU32Layout)
{
    sbufWriteU32(&sb, 0x12345678);
    EXPECT_EQ(written(), Bytes({0x78, 0x56, 0x34, 0x12}));
}

TEST_F(SbufTest, WriteU64Layout)
{
    sbufWriteU64(&sb, UINT64_C(0x123456789ABCDEF0));
    EXPECT_EQ(written(), Bytes({0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12}));
}

/* ------------------------------------------------------------------------ */
/* Wire layout - big-endian writers                                          */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, WriteU16BELayout)
{
    sbufWriteU16BE(&sb, 0x1234);
    EXPECT_EQ(written(), Bytes({0x12, 0x34}));
}

TEST_F(SbufTest, WriteU24BELayout)
{
    sbufWriteU24BE(&sb, 0x123456);
    EXPECT_EQ(written(), Bytes({0x12, 0x34, 0x56}));
}

TEST_F(SbufTest, WriteU32BELayout)
{
    sbufWriteU32BE(&sb, 0x12345678);
    EXPECT_EQ(written(), Bytes({0x12, 0x34, 0x56, 0x78}));
}

TEST_F(SbufTest, WriteU64BELayout)
{
    sbufWriteU64BE(&sb, UINT64_C(0x123456789ABCDEF0));
    EXPECT_EQ(written(), Bytes({0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}));
}

TEST_F(SbufTest, BigEndianCompatAliasesMatch)
{
    sbufWriteU16BigEndian(&sb, 0xABCD);
    sbufWriteU32BigEndian(&sb, 0x01020304);
    EXPECT_EQ(written(), Bytes({0xAB, 0xCD, 0x01, 0x02, 0x03, 0x04}));
}

/* ------------------------------------------------------------------------ */
/* Signed writers - two's complement, same layout as their unsigned twins    */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, SignedWriteLayout)
{
    sbufWriteS8(&sb,  -2);                       // 0xFE
    sbufWriteS16(&sb, -2);                       // 0xFFFE
    sbufWriteS24(&sb, -2);                       // 0xFFFFFE
    sbufWriteS32(&sb, -2);                       // 0xFFFFFFFE
    EXPECT_EQ(written(), Bytes({
        0xFE,
        0xFE, 0xFF,
        0xFE, 0xFF, 0xFF,
        0xFE, 0xFF, 0xFF, 0xFF,
    }));
}

TEST_F(SbufTest, SignedWriteLayoutBE)
{
    sbufWriteS16BE(&sb, -2);
    sbufWriteS24BE(&sb, -2);
    sbufWriteS32BE(&sb, -2);
    EXPECT_EQ(written(), Bytes({
        0xFF, 0xFE,
        0xFF, 0xFF, 0xFE,
        0xFF, 0xFF, 0xFF, 0xFE,
    }));
}

TEST_F(SbufTest, SignedExtremesRoundTrip)
{
    sbufWriteS8(&sb, INT8_MIN);
    sbufWriteS16(&sb, INT16_MIN);
    sbufWriteS32(&sb, INT32_MIN);
    sbufWriteS64(&sb, INT64_MIN);
    sbufSwitchToReader(&sb, buf);
    EXPECT_EQ(sbufReadS8(&sb),  INT8_MIN);
    EXPECT_EQ(sbufReadS16(&sb), INT16_MIN);
    EXPECT_EQ(sbufReadS32(&sb), INT32_MIN);
    EXPECT_EQ(sbufReadS64(&sb), INT64_MIN);
}

/* S24 has no native type, so its sign extension is hand-rolled and is the
   one signed reader that can plausibly be wrong. */
TEST_F(SbufTest, ReadS24SignExtends)
{
    reader({0xFF, 0xFF, 0x7F});                  // 0x7FFFFF
    EXPECT_EQ(sbufReadS24(&sb), 8388607);

    reader({0x00, 0x00, 0x80});                  // 0x800000
    EXPECT_EQ(sbufReadS24(&sb), -8388608);

    reader({0xFE, 0xFF, 0xFF});                  // -2
    EXPECT_EQ(sbufReadS24(&sb), -2);

    reader({0x00, 0x00, 0x00});
    EXPECT_EQ(sbufReadS24(&sb), 0);
}

TEST_F(SbufTest, ReadS24BESignExtends)
{
    reader({0x7F, 0xFF, 0xFF});
    EXPECT_EQ(sbufReadS24BE(&sb), 8388607);

    reader({0x80, 0x00, 0x00});
    EXPECT_EQ(sbufReadS24BE(&sb), -8388608);

    reader({0xFF, 0xFF, 0xFE});
    EXPECT_EQ(sbufReadS24BE(&sb), -2);
}

/* ------------------------------------------------------------------------ */
/* Float - IEEE-754 bit pattern, not a numeric conversion                    */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, WriteFloatLayout)
{
    sbufWriteFloat(&sb, 1.0f);                   // 0x3F800000
    EXPECT_EQ(written(), Bytes({0x00, 0x00, 0x80, 0x3F}));
}

TEST_F(SbufTest, FloatRoundTripsBitExact)
{
    const float values[] = {
        0.0f, -0.0f, 1.0f, -1.0f, 3.14159265f, 1e-38f, 3.4e38f,
    };
    for (float v : values) {
        SetUp();
        sbufWriteFloat(&sb, v);
        sbufSwitchToReader(&sb, buf);
        const float got = sbufReadFloat(&sb);
        // compare bits so -0.0 and 0.0 stay distinct
        EXPECT_EQ(memcmp(&got, &v, sizeof(v)), 0) << "value " << v;
    }
}

TEST_F(SbufTest, FloatMatchesU32OfSameBits)
{
    const float v = -12345.678f;
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));

    sbufWriteFloat(&sb, v);
    const Bytes asFloat = written();

    SetUp();
    sbufWriteU32(&sb, bits);
    EXPECT_EQ(asFloat, written());
}

/* ------------------------------------------------------------------------ */
/* Alignment - the cursor lands wherever the previous field left it          */
/* ------------------------------------------------------------------------ */

/* Writing each width at every byte offset catches any store that assumes
   alignment. On ARMv7-M a typed store lets the compiler fold an adjacent
   pair into STRD, which requires word alignment and faults without it. */
TEST_F(SbufTest, WritesAreCorrectAtEveryOffset)
{
    for (int offset = 0; offset < 8; offset++) {
        SetUp();
        for (int i = 0; i < offset; i++) {
            sbufWriteU8(&sb, 0x5A);
        }
        sbufWriteU16(&sb, 0x1122);
        sbufWriteU32(&sb, 0x33445566);
        sbufWriteU64(&sb, UINT64_C(0x778899AABBCCDDEE));
        sbufWriteU32BE(&sb, 0x01020304);

        Bytes expected(offset, 0x5A);
        for (uint8_t b : {0x22, 0x11,
                          0x66, 0x55, 0x44, 0x33,
                          0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77,
                          0x01, 0x02, 0x03, 0x04}) {
            expected.push_back(b);
        }
        EXPECT_EQ(written(), expected) << "at offset " << offset;
    }
}

TEST_F(SbufTest, ReadsAreCorrectAtEveryOffset)
{
    for (int offset = 0; offset < 8; offset++) {
        SetUp();
        for (int i = 0; i < offset; i++) {
            sbufWriteU8(&sb, 0);
        }
        sbufWriteU16(&sb, 0x1122);
        sbufWriteU32(&sb, 0x33445566);
        sbufWriteU64(&sb, UINT64_C(0x778899AABBCCDDEE));
        sbufSwitchToReader(&sb, buf);
        sbufAdvance(&sb, offset);

        EXPECT_EQ(sbufReadU16(&sb), 0x1122)                          << "offset " << offset;
        EXPECT_EQ(sbufReadU32(&sb), 0x33445566u)                     << "offset " << offset;
        EXPECT_EQ(sbufReadU64(&sb), UINT64_C(0x778899AABBCCDDEE))    << "offset " << offset;
    }
}

/* ------------------------------------------------------------------------ */
/* Fusion - adjacent inline writes are merged into wider stores              */
/* ------------------------------------------------------------------------ */

/* A long mixed run is where a bad store-merge would show up: each field is
   individually plausible, but the concatenation is what goes on the wire. */
TEST_F(SbufTest, MixedRunIsByteExact)
{
    sbufWriteU8(&sb,  0x01);
    sbufWriteU16(&sb, 0x0302);
    sbufWriteU8(&sb,  0x04);
    sbufWriteU32(&sb, 0x08070605);
    sbufWriteU16(&sb, 0x0A09);
    sbufWriteU8(&sb,  0x0B);
    sbufWriteU24(&sb, 0x0E0D0C);
    sbufWriteU16BE(&sb, 0x0F10);
    sbufWriteU8(&sb,  0x11);

    Bytes expected;
    for (int i = 1; i <= 0x11; i++) {
        expected.push_back(static_cast<uint8_t>(i));
    }
    EXPECT_EQ(written(), expected);
}

/* Each writer must advance the cursor by exactly its own width. A missing
   cast inside SBUF_WRITE would promote to int and silently write four. */
TEST_F(SbufTest, EachWriterAdvancesByItsOwnWidth)
{
    struct { const char *name; int width; void (*write)(sbuf_t *); } cases[] = {
        {"U8",    1, [](sbuf_t *s){ sbufWriteU8(s, 0xFF); }},
        {"U16",   2, [](sbuf_t *s){ sbufWriteU16(s, 0xFFFF); }},
        {"U24",   3, [](sbuf_t *s){ sbufWriteU24(s, 0xFFFFFF); }},
        {"U32",   4, [](sbuf_t *s){ sbufWriteU32(s, 0xFFFFFFFF); }},
        {"U64",   8, [](sbuf_t *s){ sbufWriteU64(s, UINT64_MAX); }},
        {"U16BE", 2, [](sbuf_t *s){ sbufWriteU16BE(s, 0xFFFF); }},
        {"U24BE", 3, [](sbuf_t *s){ sbufWriteU24BE(s, 0xFFFFFF); }},
        {"U32BE", 4, [](sbuf_t *s){ sbufWriteU32BE(s, 0xFFFFFFFF); }},
        {"U64BE", 8, [](sbuf_t *s){ sbufWriteU64BE(s, UINT64_MAX); }},
        {"Float", 4, [](sbuf_t *s){ sbufWriteFloat(s, 1.0f); }},
    };
    for (const auto &c : cases) {
        SetUp();
        c.write(&sb);
        EXPECT_EQ(used(), c.width) << c.name;
        EXPECT_EQ(buf[c.width], kPoison) << c.name << " wrote past its width";
    }
}

/* ------------------------------------------------------------------------ */
/* Round trip                                                                */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, UnsignedExtremesRoundTrip)
{
    sbufWriteU8(&sb,  UINT8_MAX);
    sbufWriteU16(&sb, UINT16_MAX);
    sbufWriteU24(&sb, 0xFFFFFF);
    sbufWriteU32(&sb, UINT32_MAX);
    sbufWriteU64(&sb, UINT64_MAX);
    sbufSwitchToReader(&sb, buf);
    EXPECT_EQ(sbufReadU8(&sb),  UINT8_MAX);
    EXPECT_EQ(sbufReadU16(&sb), UINT16_MAX);
    EXPECT_EQ(sbufReadU24(&sb), 0xFFFFFFu);
    EXPECT_EQ(sbufReadU32(&sb), UINT32_MAX);
    EXPECT_EQ(sbufReadU64(&sb), UINT64_MAX);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
}

TEST_F(SbufTest, BigEndianRoundTrips)
{
    sbufWriteU16BE(&sb, 0xBEEF);
    sbufWriteU24BE(&sb, 0xC0FFEE);
    sbufWriteU32BE(&sb, 0xDEADBEEF);
    sbufWriteU64BE(&sb, UINT64_C(0x0123456789ABCDEF));
    sbufSwitchToReader(&sb, buf);
    EXPECT_EQ(sbufReadU16BE(&sb), 0xBEEF);
    EXPECT_EQ(sbufReadU24BE(&sb), 0xC0FFEEu);
    EXPECT_EQ(sbufReadU32BE(&sb), 0xDEADBEEFu);
    EXPECT_EQ(sbufReadU64BE(&sb), UINT64_C(0x0123456789ABCDEF));
}

/* U8BE exists only so generated code can spell a width uniformly. */
TEST_F(SbufTest, ReadU8BEMatchesReadU8)
{
    reader({0x42});
    EXPECT_EQ(sbufReadU8BE(&sb), 0x42);
}

/* ------------------------------------------------------------------------ */
/* Reader bounds                                                             */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, ReadAtEndReturnsZero)
{
    reader({});
    EXPECT_EQ(sbufReadU8(&sb), 0);
    EXPECT_EQ(sbufReadU16(&sb), 0);
    EXPECT_EQ(sbufReadU32(&sb), 0u);
    EXPECT_EQ(sbufReadU64(&sb), 0u);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0) << "cursor moved past end";
}

TEST_F(SbufTest, ReadPastEndKeepsReturningZero)
{
    reader({0x11});
    EXPECT_EQ(sbufReadU8(&sb), 0x11);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(sbufReadU8(&sb), 0) << "read " << i << " past end";
    }
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
}

/* A multi-byte read that does not fit entirely within the buffer yields 0 -
   never a partial value - and swallows the bytes that were there, so the next
   read cannot resynchronise part-way through a truncated value. */
TEST_F(SbufTest, TruncatedReadsReturnZeroAndConsume)
{
    reader({0x11, 0x22});
    EXPECT_EQ(sbufReadU32(&sb), 0u);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0) << "truncated read must consume";

    reader({0x11, 0x22, 0x33});
    EXPECT_EQ(sbufReadU64(&sb), 0u);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);

    reader({0x11});
    EXPECT_EQ(sbufReadU16(&sb), 0);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);

    reader({0x11, 0x22});
    EXPECT_EQ(sbufReadU24(&sb), 0u);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
}

TEST_F(SbufTest, TruncatedBigEndianReadsReturnZeroAndConsume)
{
    reader({0x11, 0x22});
    EXPECT_EQ(sbufReadU32BE(&sb), 0u);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);

    reader({0x11});
    EXPECT_EQ(sbufReadU16BE(&sb), 0);

    reader({0x11, 0x22});
    EXPECT_EQ(sbufReadU24BE(&sb), 0u);

    reader({0x11, 0x22, 0x33});
    EXPECT_EQ(sbufReadU64BE(&sb), 0u);
}

/* A truncated read must not leave a byte behind for the next reader to find. */
TEST_F(SbufTest, TruncatedReadDoesNotLeaveTail)
{
    reader({0x11, 0x22, 0x33});
    EXPECT_EQ(sbufReadU32(&sb), 0u);
    EXPECT_EQ(sbufReadU8(&sb), 0) << "tail of a truncated read leaked";
}

/* Every truncation length, every width - the exhaustive version of the above. */
TEST_F(SbufTest, ReadsNeverConsumePoison)
{
    for (size_t avail = 0; avail <= 8; avail++) {
        Bytes data;
        for (size_t i = 0; i < avail; i++) {
            data.push_back(0x11);
        }
        reader(data);
        const uint64_t v = sbufReadU64(&sb);
        const uint64_t want = (avail >= 8) ? UINT64_C(0x1111111111111111) : 0;
        EXPECT_EQ(v, want) << "avail=" << avail;
        EXPECT_EQ(sbufBytesRemaining(&sb), 0) << "avail=" << avail;
    }
}

/* ------------------------------------------------------------------------ */
/* sbufReadData                                                              */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, ReadDataCopiesAndAdvances)
{
    reader({1, 2, 3, 4, 5});
    uint8_t out[3] = {};
    sbufReadData(&sb, out, sizeof(out));
    EXPECT_EQ(Bytes(out, out + 3), Bytes({1, 2, 3}));
    EXPECT_EQ(sbufBytesRemaining(&sb), 2);
}

TEST_F(SbufTest, ReadDataZeroFillsShortfall)
{
    reader({1, 2});
    uint8_t out[5];
    memset(out, kPoison, sizeof(out));
    sbufReadData(&sb, out, sizeof(out));
    EXPECT_EQ(Bytes(out, out + 5), Bytes({1, 2, 0, 0, 0}));
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
}

TEST_F(SbufTest, ReadDataAtEndZeroFillsEntirely)
{
    reader({});
    uint8_t out[4];
    memset(out, kPoison, sizeof(out));
    sbufReadData(&sb, out, sizeof(out));
    EXPECT_EQ(Bytes(out, out + 4), Bytes({0, 0, 0, 0}));
}

TEST_F(SbufTest, ReadDataZeroLengthIsNoOp)
{
    reader({1, 2, 3});
    uint8_t out[1] = {kPoison};
    sbufReadData(&sb, out, 0);
    EXPECT_EQ(out[0], kPoison);
    EXPECT_EQ(sbufBytesRemaining(&sb), 3);
}

/* ------------------------------------------------------------------------ */
/* Bulk writers                                                              */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, Fill)
{
    sbufFill(&sb, 0x7E, 4);
    EXPECT_EQ(written(), Bytes({0x7E, 0x7E, 0x7E, 0x7E}));
}

TEST_F(SbufTest, FillZeroLength)
{
    sbufFill(&sb, 0x7E, 0);
    EXPECT_EQ(used(), 0);
}

TEST_F(SbufTest, WriteData)
{
    const uint8_t src[] = {9, 8, 7};
    sbufWriteData(&sb, src, sizeof(src));
    EXPECT_EQ(written(), Bytes({9, 8, 7}));
}

TEST_F(SbufTest, WriteStringOmitsTerminator)
{
    sbufWriteString(&sb, "abc");
    EXPECT_EQ(written(), Bytes({'a', 'b', 'c'}));
}

TEST_F(SbufTest, WriteEmptyStringWritesNothing)
{
    sbufWriteString(&sb, "");
    EXPECT_EQ(used(), 0);
}

TEST_F(SbufTest, WriteStringWithZeroTerminator)
{
    sbufWriteStringWithZeroTerminator(&sb, "ab");
    EXPECT_EQ(written(), Bytes({'a', 'b', 0}));
}

TEST_F(SbufTest, WriteEmptyStringWithZeroTerminatorWritesTerminator)
{
    sbufWriteStringWithZeroTerminator(&sb, "");
    EXPECT_EQ(written(), Bytes({0}));
}

TEST_F(SbufTest, WritePStringPrefixesLength)
{
    sbufWritePString(&sb, "abc");
    EXPECT_EQ(written(), Bytes({3, 'a', 'b', 'c'}));
}

TEST_F(SbufTest, WritePStringEmpty)
{
    sbufWritePString(&sb, "");
    EXPECT_EQ(written(), Bytes({0}));
}

/* The length prefix is one byte, so longer strings are truncated rather than
   wrapping round to a short frame. */
TEST_F(SbufTest, WritePStringTruncatesAt255)
{
    const std::string longString(300, 'x');
    sbufWritePString(&sb, longString.c_str());
    EXPECT_EQ(used(), 256);
    EXPECT_EQ(buf[0], 255);
    EXPECT_EQ(buf[1], 'x');
    EXPECT_EQ(buf[255], 'x');
}

TEST_F(SbufTest, WritePStringExactly255)
{
    const std::string s(255, 'y');
    sbufWritePString(&sb, s.c_str());
    EXPECT_EQ(used(), 256);
    EXPECT_EQ(buf[0], 255);
}

/* ------------------------------------------------------------------------ */
/* Cursor management                                                         */
/* ------------------------------------------------------------------------ */

TEST_F(SbufTest, InitSetsCursorAndEnd)
{
    sbuf_t s;
    EXPECT_EQ(sbufInit(&s, buf, buf + 10), &s);
    EXPECT_EQ(sbufPtr(&s), buf);
    EXPECT_EQ(sbufBytesRemaining(&s), 10);
}

TEST_F(SbufTest, BytesRemainingTracksWrites)
{
    EXPECT_EQ(sbufBytesRemaining(&sb), static_cast<int>(kCapacity));
    sbufWriteU32(&sb, 0);
    EXPECT_EQ(sbufBytesRemaining(&sb), static_cast<int>(kCapacity) - 4);
}

TEST_F(SbufTest, PtrAndConstPtrTrackCursor)
{
    sbufWriteU16(&sb, 0);
    EXPECT_EQ(sbufPtr(&sb), buf + 2);
    EXPECT_EQ(sbufConstPtr(&sb), buf + 2);
}

TEST_F(SbufTest, AdvanceWithinBounds)
{
    reader({1, 2, 3, 4, 5});
    sbufAdvance(&sb, 2);
    EXPECT_EQ(sbufReadU8(&sb), 3);
}

TEST_F(SbufTest, AdvanceToExactEnd)
{
    reader({1, 2, 3});
    sbufAdvance(&sb, 3);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
}

/* Overshooting clamps rather than running the cursor past end, which would
   make bytesRemaining negative and defeat every later bounds check. */
TEST_F(SbufTest, AdvanceBeyondEndClamps)
{
    reader({1, 2, 3});
    sbufAdvance(&sb, 99);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
    EXPECT_EQ(sbufPtr(&sb), buf + 3);
    EXPECT_EQ(sbufReadU8(&sb), 0);
}

TEST_F(SbufTest, AdvanceZeroIsNoOp)
{
    reader({1, 2, 3});
    sbufAdvance(&sb, 0);
    EXPECT_EQ(sbufPtr(&sb), buf);
}

/* len is unsigned, so there is no rewind: a value that looks like a negative
   int clamps to end like any other overshoot. Use sbufReset to go back. */
TEST_F(SbufTest, AdvanceHugeLengthClamps)
{
    reader({1, 2, 3});
    sbufAdvance(&sb, SIZE_MAX);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
    EXPECT_EQ(sbufPtr(&sb), buf + 3);
}

TEST_F(SbufTest, ResetMovesCursor)
{
    sbufWriteU32(&sb, 0x11223344);
    sbufReset(&sb, buf);
    EXPECT_EQ(sbufPtr(&sb), buf);
    sbufWriteU8(&sb, 0xFF);
    EXPECT_EQ(buf[0], 0xFF) << "reset must allow overwriting";
}

TEST_F(SbufTest, SwitchToReaderCapsEndAtWrittenLength)
{
    sbufWriteU8(&sb, 1);
    sbufWriteU8(&sb, 2);
    sbufSwitchToReader(&sb, buf);
    EXPECT_EQ(sbufBytesRemaining(&sb), 2) << "end must cap at what was written";
    EXPECT_EQ(sbufReadU8(&sb), 1);
    EXPECT_EQ(sbufReadU8(&sb), 2);
    EXPECT_EQ(sbufReadU8(&sb), 0) << "must not read uninitialised buffer";
}

TEST_F(SbufTest, SwitchToReaderWithNoDataYieldsEmptyRange)
{
    sbufSwitchToReader(&sb, buf);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
    EXPECT_EQ(sbufReadU8(&sb), 0);
}

/* ------------------------------------------------------------------------ */
/* A realistic frame                                                         */
/* ------------------------------------------------------------------------ */

/* MSP shape: a byte header followed by wider fields, so every subsequent
   field starts at an odd offset. */
TEST_F(SbufTest, MspStyleFrameRoundTrips)
{
    sbufWriteU8(&sb, 0xA1);
    sbufWriteU16(&sb, 0x1234);
    sbufWriteU32(&sb, 0xDEADBEEF);
    sbufWriteString(&sb, "rotorflight");
    sbufWriteU8(&sb, 0);
    sbufWriteFloat(&sb, 2.5f);
    sbufWriteU16BE(&sb, 0x0102);

    const int total = used();
    sbufSwitchToReader(&sb, buf);
    EXPECT_EQ(sbufBytesRemaining(&sb), total);

    EXPECT_EQ(sbufReadU8(&sb), 0xA1);
    EXPECT_EQ(sbufReadU16(&sb), 0x1234);
    EXPECT_EQ(sbufReadU32(&sb), 0xDEADBEEFu);
    char name[12] = {};
    sbufReadData(&sb, name, 11);          // consumes the 11 bytes it copies
    EXPECT_STREQ(name, "rotorflight");
    EXPECT_EQ(sbufReadU8(&sb), 0);
    EXPECT_EQ(sbufReadFloat(&sb), 2.5f);
    EXPECT_EQ(sbufReadU16BE(&sb), 0x0102);
    EXPECT_EQ(sbufBytesRemaining(&sb), 0);
}

} // namespace
