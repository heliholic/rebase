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

/// CRC-8/CRC-16 here are MSB-first, no reflect, xorout 0.
/// Catalogue check values for "123456789" are the independent oracle:
///   CRC-8/DVB-S2  poly 0xD5 init 0x00     -> 0xBC
///   CRC-8/SMBUS   poly 0x07 init 0x00     -> 0xF4   (crc8_kiss_update)
///   CRC-16/XMODEM poly 0x1021 init 0x0000 -> 0x31C3 (crc16_ccitt)
///   CRC-16/CCITT-FALSE init 0xFFFF        -> 0x29B1
///   CRC-16/BUYPASS poly 0x8005 init 0x0000 -> 0xFEE8  (bit15 poly)

#include <stdint.h>

extern "C" {
    #include "common/crc.h"
    #include "common/streambuf.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

static const uint8_t kCheck[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
static const uint8_t kHello[] = { 'h', 'e', 'l', 'l', 'o' };

namespace {

void writePayload(sbuf_t *dst, const uint8_t *data, size_t length)
{
    sbufWriteData(dst, data, length);
}

} // namespace

/*
 * crc8
 */

TEST(CrcUnittest, Crc8CalcKnownBytes)
{
    EXPECT_EQ(crc8_calc(0, 0x00, 0xD5), 0x00);
    EXPECT_EQ(crc8_calc(0, 0x01, 0xD5), 0xD5);
    EXPECT_EQ(crc8_calc(0, 0x80, 0xD5), 0xEF);
    EXPECT_EQ(crc8_calc(0, 0xFF, 0xD5), 0xF9);
    EXPECT_EQ(crc8_calc(0xFF, 0x01, 0xD5), 0x2C);

    EXPECT_EQ(crc8_calc(0, 0x01, 0x07), 0x07);
    EXPECT_EQ(crc8_calc(0, 0x01, 0xBA), 0xBA);
    EXPECT_EQ(crc8_calc(0, 0x80, 0xBA), 0xE6);
}

TEST(CrcUnittest, Crc8UpdateMatchesCatalogueAndIsIncremental)
{
    EXPECT_EQ(crc8_update(0, kCheck, 0, 0xD5), 0);
    EXPECT_EQ(crc8_update(0xA5, kCheck, 0, 0xD5), 0xA5);

    EXPECT_EQ(crc8_update(0, kCheck, sizeof(kCheck), 0xD5), 0xBC);
    EXPECT_EQ(crc8_update(0, kHello, sizeof(kHello), 0xD5), 0x9D);

    const uint8_t *const split = kCheck + 5;
    const uint8_t first = crc8_update(0, kCheck, 5, 0xD5);
    EXPECT_EQ(crc8_update(first, split, 4, 0xD5), 0xBC);
}

TEST(CrcUnittest, Crc8NamedMacrosUseTheDocumentedPolynomials)
{
    EXPECT_EQ(crc8_dvb_s2(0, 0x01), crc8_calc(0, 0x01, 0xD5));
    EXPECT_EQ(crc8_dvb_s2_update(0, kCheck, sizeof(kCheck)), 0xBC);

    EXPECT_EQ(crc8_kiss_update(0, kCheck, sizeof(kCheck)), 0xF4);
    EXPECT_EQ(crc8_kiss_update(0, kHello, sizeof(kHello)), 0x92);

    EXPECT_EQ(crc8_poly_0xba(0, 0x01), crc8_calc(0, 0x01, 0xBA));
    EXPECT_EQ(crc8_update(0, kCheck, sizeof(kCheck), 0xBA), 0x20);
}

TEST(CrcUnittest, Crc8SbufAppendWritesCrcOverThePayloadRange)
{
    uint8_t buf[16];
    sbuf_t sbuf;

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    writePayload(&sbuf, kCheck, sizeof(kCheck));
    crc8_dvb_s2_sbuf_append(&sbuf, buf);
    EXPECT_EQ(buf[sizeof(kCheck)], 0xBC);
    EXPECT_EQ(sbufPtr(&sbuf), buf + sizeof(kCheck) + 1);

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    sbufWriteU8(&sbuf, 0xAA);
    writePayload(&sbuf, kCheck, sizeof(kCheck));
    crc8_poly_0xba_sbuf_append(&sbuf, buf + 1);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[1 + sizeof(kCheck)], 0x20);
}

/*
 * crc16
 */

TEST(CrcUnittest, Crc16CalcKnownBytes)
{
    EXPECT_EQ(crc16_calc(0, 0x00, 0x1021), 0x0000);
    EXPECT_EQ(crc16_calc(0, 0x01, 0x1021), 0x1021);
    EXPECT_EQ(crc16_calc(0, 0x80, 0x1021), 0x9188);
    EXPECT_EQ(crc16_calc(0, 0xFF, 0x1021), 0x1EF0);

    // A polynomial with bit 15 set must survive the shift-out unmasked.
    EXPECT_EQ(crc16_calc(0, 0x01, 0x8005), 0x8005);
    EXPECT_EQ(crc16_calc(0, 0x80, 0x8005), 0x8303);
    EXPECT_EQ(crc16_calc(0, 0xFF, 0x8005), 0x0202);
}

TEST(CrcUnittest, Crc16UpdateMatchesCatalogueAndIsIncremental)
{
    EXPECT_EQ(crc16_update(0, kCheck, 0, 0x1021), 0);
    EXPECT_EQ(crc16_update(0xBEEF, kCheck, 0, 0x1021), 0xBEEF);

    EXPECT_EQ(crc16_ccitt_update(0, kCheck, sizeof(kCheck)), 0x31C3);
    EXPECT_EQ(crc16_ccitt_update(0xFFFF, kCheck, sizeof(kCheck)), 0x29B1);
    EXPECT_EQ(crc16_ccitt_update(0, kHello, sizeof(kHello)), 0xC362);

    const uint8_t *const split = kCheck + 5;
    const uint16_t first = crc16_ccitt_update(0, kCheck, 5);
    EXPECT_EQ(crc16_ccitt_update(first, split, 4), 0x31C3);

    EXPECT_EQ(crc16_update(0, kCheck, sizeof(kCheck), 0x8005), 0xFEE8);
    EXPECT_EQ(crc16_update(0, kHello, sizeof(kHello), 0x8005), 0x38C5);
    EXPECT_EQ(crc16_update(0xFFFF, kCheck, sizeof(kCheck), 0x8005), 0xAEE7);
}

TEST(CrcUnittest, Crc16NamedMacrosUseCcittPolynomial)
{
    EXPECT_EQ(crc16_ccitt(0, 0x01), crc16_calc(0, 0x01, 0x1021));
    EXPECT_EQ(crc16_ccitt_update(0, kCheck, sizeof(kCheck)),
              crc16_update(0, kCheck, sizeof(kCheck), 0x1021));
}

TEST(CrcUnittest, Crc16SbufAppendWritesLittleEndianCrc)
{
    uint8_t buf[16];
    sbuf_t sbuf;

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    writePayload(&sbuf, kCheck, sizeof(kCheck));
    crc16_ccitt_sbuf_append(&sbuf, buf);
    EXPECT_EQ(buf[sizeof(kCheck)], 0xC3);
    EXPECT_EQ(buf[sizeof(kCheck) + 1], 0x31);
    EXPECT_EQ(sbufPtr(&sbuf), buf + sizeof(kCheck) + 2);

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    sbufWriteU8(&sbuf, 0xAA);
    writePayload(&sbuf, kHello, sizeof(kHello));
    crc16_ccitt_sbuf_append(&sbuf, buf + 1);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[1 + sizeof(kHello)], 0x62);
    EXPECT_EQ(buf[1 + sizeof(kHello) + 1], 0xC3);
}

/*
 * xor / FNV
 */

TEST(CrcUnittest, Crc8XorUpdateIsRunningXor)
{
    EXPECT_EQ(crc8_xor_update(0, kCheck, 0), 0);
    EXPECT_EQ(crc8_xor_update(0x5A, kCheck, 0), 0x5A);
    EXPECT_EQ(crc8_xor_update(0, kCheck, 1), kCheck[0]);
    EXPECT_EQ(crc8_xor_update(0, kCheck, sizeof(kCheck)), 0x31);
    EXPECT_EQ(crc8_xor_update(0, kHello, sizeof(kHello)), 0x62);

    const uint8_t first = crc8_xor_update(0, kCheck, 5);
    EXPECT_EQ(crc8_xor_update(first, kCheck + 5, 4), 0x31);
}

TEST(CrcUnittest, Crc8XorSbufAppendWritesXorOverThePayloadRange)
{
    uint8_t buf[16];
    sbuf_t sbuf;

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    writePayload(&sbuf, kHello, sizeof(kHello));
    crc8_xor_sbuf_append(&sbuf, buf);
    EXPECT_EQ(buf[sizeof(kHello)], 0x62);

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    sbufWriteU8(&sbuf, 0xAA);
    writePayload(&sbuf, kHello, sizeof(kHello));
    crc8_xor_sbuf_append(&sbuf, buf + 1);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[1 + sizeof(kHello)], 0x62);
}

/*
 * Empty payload range: start == sbufPtr(dst). Every appender must add the
 * CRC of zero bytes rather than walking past the end of the buffer.
 */

TEST(CrcUnittest, SbufAppendOverEmptyRangeWritesTheInitialCrc)
{
    uint8_t buf[16];
    sbuf_t sbuf;

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    crc8_dvb_s2_sbuf_append(&sbuf, sbufPtr(&sbuf));
    EXPECT_EQ(buf[0], 0x00);
    EXPECT_EQ(sbufPtr(&sbuf), buf + 1);

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    crc16_ccitt_sbuf_append(&sbuf, sbufPtr(&sbuf));
    EXPECT_EQ(buf[0], 0x00);
    EXPECT_EQ(buf[1], 0x00);
    EXPECT_EQ(sbufPtr(&sbuf), buf + 2);

    sbufInit(&sbuf, buf, buf + sizeof(buf));
    crc8_xor_sbuf_append(&sbuf, sbufPtr(&sbuf));
    EXPECT_EQ(buf[0], 0x00);
    EXPECT_EQ(sbufPtr(&sbuf), buf + 1);

    // Same, but with a non-empty prefix already written: the appended CRC
    // must cover the empty range only, not the preceding bytes.
    sbufInit(&sbuf, buf, buf + sizeof(buf));
    writePayload(&sbuf, kHello, sizeof(kHello));
    crc8_dvb_s2_sbuf_append(&sbuf, sbufPtr(&sbuf));
    EXPECT_EQ(buf[sizeof(kHello)], 0x00);
}

TEST(CrcUnittest, FnvUpdateIsFnv1)
{
    EXPECT_EQ(fnv_update(FNV_OFFSET_BASIS, kHello, 0), FNV_OFFSET_BASIS);
    EXPECT_EQ(fnv_update(FNV_OFFSET_BASIS, kHello, sizeof(kHello)), 0xB6FA7167u);
    EXPECT_EQ(fnv_update(FNV_OFFSET_BASIS, kCheck, sizeof(kCheck)), 0x24148816u);

    const uint32_t first = fnv_update(FNV_OFFSET_BASIS, kHello, 2);
    EXPECT_EQ(fnv_update(first, kHello + 2, 3), 0xB6FA7167u);
}
