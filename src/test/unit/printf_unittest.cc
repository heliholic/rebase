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

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" {
    #include "common/printf.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

// Format and compare against the expected result, checking the returned
// length as well as the text.
#define EXPECT_FORMAT(expected, ...)                                    \
    do {                                                                \
        char buf_[128];                                                 \
        const int n_ = tfp_sprintf(buf_, __VA_ARGS__);                  \
        EXPECT_STREQ(expected, buf_);                                   \
        EXPECT_EQ((int)strlen(expected), n_);                           \
    } while (0)


//
// Conversions always compiled in
//

TEST(PrintfTest, PlainText)
{
    EXPECT_FORMAT("", "");
    EXPECT_FORMAT("hello", "hello");
    EXPECT_FORMAT("a\r\nb", "a\r\nb");
}

TEST(PrintfTest, SignedDecimal)
{
    EXPECT_FORMAT("0", "%d", 0);
    EXPECT_FORMAT("1", "%d", 1);
    EXPECT_FORMAT("-1", "%d", -1);
    EXPECT_FORMAT("12345", "%d", 12345);
    EXPECT_FORMAT("-12345", "%d", -12345);
    EXPECT_FORMAT("2147483647", "%d", INT_MAX);
    EXPECT_FORMAT("-2147483648", "%d", INT_MIN);

    EXPECT_FORMAT("42", "%i", 42);
    EXPECT_FORMAT("-42", "%i", -42);
}

TEST(PrintfTest, UnsignedDecimal)
{
    EXPECT_FORMAT("0", "%u", 0u);
    EXPECT_FORMAT("42", "%u", 42u);
    EXPECT_FORMAT("4294967295", "%u", UINT_MAX);
}

TEST(PrintfTest, Hex)
{
    EXPECT_FORMAT("0", "%x", 0u);
    EXPECT_FORMAT("ff", "%x", 0xffu);
    EXPECT_FORMAT("FF", "%X", 0xffu);
    EXPECT_FORMAT("deadbeef", "%x", 0xdeadbeefu);
    EXPECT_FORMAT("DEADBEEF", "%X", 0xdeadbeefu);
    EXPECT_FORMAT("ffffffff", "%x", UINT_MAX);
}

TEST(PrintfTest, Octal)
{
    EXPECT_FORMAT("0", "%o", 0u);
    EXPECT_FORMAT("755", "%o", 0755u);
    EXPECT_FORMAT("37777777777", "%o", UINT_MAX);
}

TEST(PrintfTest, Char)
{
    EXPECT_FORMAT("A", "%c", 'A');
    EXPECT_FORMAT("[ ]", "[%c]", ' ');
    EXPECT_FORMAT("abc", "%c%c%c", 'a', 'b', 'c');
}

TEST(PrintfTest, String)
{
    EXPECT_FORMAT("", "%s", "");
    EXPECT_FORMAT("str", "%s", "str");
    EXPECT_FORMAT("a-b", "%s-%s", "a", "b");

    // A null pointer must not fault; nanoprintf emits nothing for %s NULL
    EXPECT_FORMAT("", "%s", (char *)NULL);
}

TEST(PrintfTest, Percent)
{
    EXPECT_FORMAT("%", "%%");
    EXPECT_FORMAT("100%", "100%%");
    EXPECT_FORMAT("50% off", "%d%% off", 50);
}

TEST(PrintfTest, Pointer)
{
    char buf[32];

    // nanoprintf %p is zero-padded hex of the pointer width, with no 0x prefix.
    const int digits = (int)(sizeof(void *) * CHAR_BIT / 4);

    tfp_sprintf(buf, "%p", (void *)0);
    EXPECT_EQ(digits, (int)strlen(buf));
    for (int i = 0; i < digits; i++) {
        EXPECT_EQ('0', buf[i]);
    }

    tfp_sprintf(buf, "%p", (void *)(uintptr_t)0x1234);
    EXPECT_EQ(digits, (int)strlen(buf));
    EXPECT_STREQ("1234", buf + digits - 4);
}

TEST(PrintfTest, SignFlags)
{
    EXPECT_FORMAT("+42", "%+d", 42);
    EXPECT_FORMAT("-42", "%+d", -42);
    EXPECT_FORMAT(" 42", "% d", 42);
}

TEST(PrintfTest, LongModifier)
{
    EXPECT_FORMAT("0", "%ld", 0L);
    EXPECT_FORMAT("123456", "%ld", 123456L);
    EXPECT_FORMAT("-123456", "%ld", -123456L);
    EXPECT_FORMAT("123456", "%lu", 123456UL);
    EXPECT_FORMAT("1e240", "%lx", 123456UL);
    EXPECT_FORMAT("1E240", "%lX", 123456UL);

    EXPECT_FORMAT("2147483647", "%ld", 2147483647L);
    EXPECT_FORMAT("-2147483647", "%ld", -2147483647L);
    EXPECT_FORMAT("4294967295", "%lu", 4294967295UL);
}


//
// NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS  -  %b %B
//

#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS
TEST(PrintfTest, Binary)
{
    EXPECT_FORMAT("0", "%b", 0u);
    EXPECT_FORMAT("1010", "%b", 10u);
    EXPECT_FORMAT("11111111", "%b", 0xffu);
    EXPECT_FORMAT("1010", "%B", 10u);
}
#else
TEST(PrintfTest, BinaryIsLiteral)
{
    EXPECT_FORMAT("%b", "%b", 10u);
    EXPECT_FORMAT("%B", "%B", 10u);
}
#endif


//
// NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS  -  %n
//

#if NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS
TEST(PrintfTest, Writeback)
{
    int written = -1;
    char buf[64];

    tfp_sprintf(buf, "abcd%n", &written);
    EXPECT_STREQ("abcd", buf);
    EXPECT_EQ(4, written);

    tfp_sprintf(buf, "%d%n", 12345, &written);
    EXPECT_STREQ("12345", buf);
    EXPECT_EQ(5, written);
}
#else
TEST(PrintfTest, WritebackIsLiteral)
{
    int written = -1;
    char buf[64];

    tfp_sprintf(buf, "abcd%n", &written);
    EXPECT_STREQ("abcd%n", buf);
    EXPECT_EQ(-1, written);

    tfp_sprintf(buf, "%d%n", 12345, &written);
    EXPECT_STREQ("12345%n", buf);
    EXPECT_EQ(-1, written);
}
#endif


//
// NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS
//     Field width, '*' width, '-' left-justify, '0' pad
//

#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS
TEST(PrintfTest, FieldWidth)
{
    EXPECT_FORMAT("|    42|", "|%6d|", 42);
    EXPECT_FORMAT("|42|", "|%1d|", 42);
    EXPECT_FORMAT("|   abc|", "|%6s|", "abc");
    EXPECT_FORMAT("|     A|", "|%6c|", 'A');
    EXPECT_FORMAT("|    ff|", "|%6x|", 0xffu);
}

TEST(PrintfTest, ZeroPadding)
{
    EXPECT_FORMAT("|000042|", "|%06d|", 42);
    EXPECT_FORMAT("|0000ff|", "|%06x|", 0xffu);
    EXPECT_FORMAT("|-00042|", "|%06d|", -42);
    EXPECT_FORMAT("|-42|", "|%03d|", -42);
}

TEST(PrintfTest, LeftJustify)
{
    EXPECT_FORMAT("|42    |", "|%-6d|", 42);
    EXPECT_FORMAT("|abc   |", "|%-6s|", "abc");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
    EXPECT_FORMAT("|42    |", "|%-06d|", 42);
#pragma GCC diagnostic pop
}

TEST(PrintfTest, StarWidth)
{
    EXPECT_FORMAT("|    42|", "|%*d|", 6, 42);
    EXPECT_FORMAT("|42|", "|%*d|", 0, 42);
    EXPECT_FORMAT("|42    |", "|%*d|", -6, 42);
}
#else
TEST(PrintfTest, FieldWidthIsLiteral)
{
    EXPECT_FORMAT("%6d", "%6d", 42);
    EXPECT_FORMAT("%06d", "%06d", 42);
    EXPECT_FORMAT("%-6d", "%-6d", 42);
}
#endif


//
// NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS  -  .N and .*
//

#if NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS
TEST(PrintfTest, StringPrecision)
{
    EXPECT_FORMAT("|abc|", "|%.3s|", "abcdef");
    EXPECT_FORMAT("||", "|%.0s|", "abcdef");
    EXPECT_FORMAT("|abcdef|", "|%.20s|", "abcdef");
    EXPECT_FORMAT("|abc|", "|%.*s|", 3, "abcdef");
#if NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS
    EXPECT_FORMAT("|   abc|", "|%6.3s|", "abcdef");
    EXPECT_FORMAT("|abc   |", "|%-6.3s|", "abcdef");
#endif
}

TEST(PrintfTest, NumericPrecision)
{
    EXPECT_FORMAT("00000042", "%.8d", 42);
    EXPECT_FORMAT("42", "%.1d", 42);
    EXPECT_FORMAT("", "%.0d", 0);
}
#else
TEST(PrintfTest, PrecisionIsLiteral)
{
    EXPECT_FORMAT("%.3s", "%.3s", "abcdef");
    EXPECT_FORMAT("%.8d", "%.8d", 42);
}
#endif


//
// NANOPRINTF_USE_ALT_FORM_FLAG  -  '#'
//

#if NANOPRINTF_USE_ALT_FORM_FLAG
TEST(PrintfTest, AltFormFlag)
{
    EXPECT_FORMAT("0xff", "%#x", 0xffu);
    EXPECT_FORMAT("0XFF", "%#X", 0xffu);
    EXPECT_FORMAT("0", "%#x", 0u);
    EXPECT_FORMAT("0755", "%#o", 0755u);
    EXPECT_FORMAT("0", "%#o", 0u);
#if NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS
    EXPECT_FORMAT("0b1010", "%#b", 10u);
    EXPECT_FORMAT("0B1010", "%#B", 10u);
#endif
}
#else
TEST(PrintfTest, AltFormFlagIsLiteral)
{
    EXPECT_FORMAT("%#x", "%#x", 0xffu);
    EXPECT_FORMAT("%#X", "%#X", 0xffu);
}
#endif


//
// NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS  -  h, hh
//

#if NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS
TEST(PrintfTest, ShortModifiers)
{
    // Promoted to int, so these only need to be skipped
    EXPECT_FORMAT("7", "%hhd", (signed char)7);
    EXPECT_FORMAT("-7", "%hhd", (signed char)-7);
    EXPECT_FORMAT("255", "%hhu", (unsigned char)255);
    EXPECT_FORMAT("300", "%hd", (short)300);
    EXPECT_FORMAT("-300", "%hd", (short)-300);
    EXPECT_FORMAT("65535", "%hu", (unsigned short)65535);
}
#else
TEST(PrintfTest, ShortModifiersAreLiteral)
{
    EXPECT_FORMAT("%hhd", "%hhd", (signed char)7);
    EXPECT_FORMAT("%hd", "%hd", (short)300);
    EXPECT_FORMAT("%hu", "%hu", (unsigned short)65535);
}
#endif


//
// NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS  -  ll, j, z, t
//

#if NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS
TEST(PrintfTest, LongLong)
{
    EXPECT_FORMAT("1", "%lld", 1LL);
    EXPECT_FORMAT("-1", "%lld", -1LL);
    EXPECT_FORMAT("1", "%llu", 1ULL);
    EXPECT_FORMAT("1", "%llx", 1ULL);
    EXPECT_FORMAT("1", "%jd", (intmax_t)1);

    EXPECT_FORMAT("1 42", "%lld %d", 1LL, 42);
    EXPECT_FORMAT("42 1 43", "%d %lld %d", 42, 1LL, 43);
}

TEST(PrintfTest, SizeModifiers)
{
    EXPECT_FORMAT("77", "%zu", (size_t)77);
    EXPECT_FORMAT("77", "%zd", (size_t)77);
    EXPECT_FORMAT("77", "%td", (ptrdiff_t)77);
    EXPECT_FORMAT("-77", "%td", (ptrdiff_t)-77);
}
#else
TEST(PrintfTest, LargeModifiersAreLiteral)
{
    EXPECT_FORMAT("%lld", "%lld", 1LL);
    EXPECT_FORMAT("%zu", "%zu", (size_t)77);
    EXPECT_FORMAT("%td", "%td", (ptrdiff_t)77);
}
#endif


//
// NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS  -  %f %F
// NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER  -  %e %E
// NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER  -  %g %G
// NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER  -  %a %A
//
// FLOAT_SINGLE_PRECISION is a calling-convention flag for the npf_* macros.
// tfp_sprintf / rf_sprintf are real variadic functions, so float is always
// promoted to double and that flag cannot be exercised through this API.
//

#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
TEST(PrintfTest, Float)
{
    EXPECT_FORMAT("1.500000", "%f", 1.5);
    EXPECT_FORMAT("1.500000", "%F", 1.5);
    EXPECT_FORMAT("-1.250000", "%f", -1.25);
    EXPECT_FORMAT("0.000000", "%f", 0.0);
    EXPECT_FORMAT("1.50", "%.2f", 1.5);
    EXPECT_FORMAT("2", "%.0f", 1.5);     // round half to even
    EXPECT_FORMAT("0", "%.0f", 0.5);

    EXPECT_FORMAT("1.500000 42", "%f %d", 1.5, 42);
    EXPECT_FORMAT("42 1.500000 43", "%d %f %d", 42, 1.5, 43);
    EXPECT_FORMAT("1.0000002.0000007", "%f%f%d", 1.0, 2.0, 7);
}

#if NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER
TEST(PrintfTest, ScientificFloat)
{
    EXPECT_FORMAT("1.500000e+00", "%e", 1.5);
    EXPECT_FORMAT("1.500000E+00", "%E", 1.5);
    EXPECT_FORMAT("0.000000e+00", "%e", 0.0);
    EXPECT_FORMAT("-1.250000e+00", "%e", -1.25);
    EXPECT_FORMAT("1.23e+04", "%.2e", 12345.6789);
    EXPECT_FORMAT("1.000000E-09", "%E", 1e-9);

    // %f cannot reach these, so %e is the way to print them. Conversion is
    // good for about 9 significant digits, so this is 9.999999e+299 rather
    // than the 1.000000e+300 libc gives.
    EXPECT_FORMAT("9.999999e+299", "%e", 1e300);
}
#else
TEST(PrintfTest, ScientificFloatIsLiteral)
{
    EXPECT_FORMAT("%e", "%e", 1.5);
    EXPECT_FORMAT("%E", "%E", 1.5);
}
#endif

#if NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER
TEST(PrintfTest, ShortestFloat)
{
    EXPECT_FORMAT("1.5", "%g", 1.5);
    EXPECT_FORMAT("1.5", "%G", 1.5);
    EXPECT_FORMAT("0", "%g", 0.0);
    EXPECT_FORMAT("-1.25", "%g", -1.25);
}
#else
TEST(PrintfTest, ShortestFloatIsLiteral)
{
    EXPECT_FORMAT("%g", "%g", 1.5);
    EXPECT_FORMAT("%G", "%G", 1.5);
}
#endif

#if NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER
TEST(PrintfTest, HexFloat)
{
    // Explicit precision keeps the mantissa short.
    EXPECT_FORMAT("0x1.8p+0", "%.1a", 1.5);
    EXPECT_FORMAT("0X1.8P+0", "%.1A", 1.5);
    EXPECT_FORMAT("0x0p+0", "%.0a", 0.0);
}
#else
TEST(PrintfTest, HexFloatIsLiteral)
{
    EXPECT_FORMAT("%a", "%a", 1.5);
    EXPECT_FORMAT("%A", "%A", 1.5);
}
#endif

#else // !NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
TEST(PrintfTest, FloatIsLiteral)
{
    EXPECT_FORMAT("%f", "%f", 1.5);
    EXPECT_FORMAT("%F", "%F", 1.5);
    EXPECT_FORMAT("%e", "%e", 1.5);
    EXPECT_FORMAT("%g", "%g", 1.5);
    EXPECT_FORMAT("%a", "%a", 1.5);
}
#endif


//
// Unknown / truncated specifiers
//

TEST(PrintfTest, UnknownConversionIsEchoed)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
    // Nothing can be assumed about the argument, so it is not consumed - the
    // conversion is echoed instead of being silently swallowed
    EXPECT_FORMAT("%q", "%q");
    EXPECT_FORMAT("a%qb", "a%qb");
#pragma GCC diagnostic pop
}

TEST(PrintfTest, TruncatedFormat)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
    EXPECT_FORMAT("x%", "x%");
    EXPECT_FORMAT("x%-08", "x%-08");
#pragma GCC diagnostic pop
}


//
// API
//

TEST(PrintfTest, SprintfTerminatesAndCountsCorrectly)
{
    char buf[16];

    memset(buf, 'x', sizeof(buf));
    const int n = tfp_sprintf(buf, "ab%d", 1);

    EXPECT_EQ(3, n);
    EXPECT_EQ('\0', buf[3]);        // terminated, and not counted
    EXPECT_EQ('x', buf[4]);         // nothing written past the terminator
}

TEST(PrintfTest, TfpSprintfRejectsNullArguments)
{
    // The tinyprintf-compatible entry point guards its arguments and formats
    // nothing rather than faulting the way the original tinyprintf did.
    EXPECT_EQ(0, tfp_sprintf(NULL, "%d", 42));
    EXPECT_EQ(0, tfp_sprintf(NULL, NULL));

    char buf[8];
    memset(buf, 'x', sizeof(buf));
    EXPECT_EQ(0, tfp_sprintf(buf, NULL));
    EXPECT_EQ('x', buf[0]);         // the buffer is left untouched, not emptied
}

TEST(PrintfTest, SprintfToNullCountsWithoutWriting)
{
    // No buffer is count-only mode, the same as snprintf(NULL, 0, ...)
    EXPECT_EQ(2, rf_sprintf(NULL, "%d", 42));
}

TEST(PrintfTest, SnprintfTruncatesAndReportsUntruncatedLength)
{
    char buf[8];

    memset(buf, 'x', sizeof(buf));
    const int n = rf_snprintf(buf, 4, "abcdef");

    EXPECT_EQ(6, n);                // would-have-been length, not counting NUL
    EXPECT_STREQ("abc", buf);       // 3 chars + terminator inside the 4-byte cap
    EXPECT_EQ('x', buf[4]);         // nothing written past the cap
}


static char sinkBuffer[128];
static size_t sinkLength;

static void sinkPutc(int c, void *p)
{
    EXPECT_EQ(sinkBuffer, p);       // the sink pointer is passed through

    if (sinkLength < sizeof(sinkBuffer) - 1) {
        sinkBuffer[sinkLength++] = (char)c;
        sinkBuffer[sinkLength] = '\0';
    }
}

static void sinkReset(void)
{
    sinkLength = 0;
    sinkBuffer[0] = '\0';
}

TEST(PrintfTest, FormatWritesThroughTheSuppliedSink)
{
    sinkReset();

    va_list va;
    memset(&va, 0, sizeof(va));     // no arguments are consumed below
    const int n = tfp_format(sinkBuffer, sinkPutc, "plain", va);

    EXPECT_STREQ("plain", sinkBuffer);
    EXPECT_EQ(5, n);
}

TEST(PrintfTest, PrintfWritesToTheCurrentSink)
{
    const stdSink_t saved = getStdoutSink();
    const stdSink_t sink = { sinkPutc, sinkBuffer };

    sinkReset();
    setStdoutSink(sink);

    const int n = rf_printf("x=%d", 42);

    setStdoutSink(saved);

    EXPECT_STREQ("x=42", sinkBuffer);
    EXPECT_EQ(4, n);
}

TEST(PrintfTest, PrintfWithoutASinkIsDiscarded)
{
    const stdSink_t saved = getStdoutSink();
    const stdSink_t none = { NULL, NULL };

    setStdoutSink(none);

    const int n = rf_printf("anything %d", 42);

    setStdoutSink(saved);

    EXPECT_EQ(0, n);
}

TEST(PrintfTest, SinkRoundTrips)
{
    const stdSink_t saved = getStdoutSink();
    const stdSink_t sink = { sinkPutc, sinkBuffer };

    setStdoutSink(sink);
    const stdSink_t read = getStdoutSink();

    setStdoutSink(saved);

    EXPECT_EQ(sink.putf, read.putf);
    EXPECT_EQ(sink.putp, read.putp);

    const stdSink_t restored = getStdoutSink();
    EXPECT_EQ(saved.putf, restored.putf);
    EXPECT_EQ(saved.putp, restored.putp);
}
