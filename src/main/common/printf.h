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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#include "types.h"
#include "utils.h"

#include "io/serial.h"


/*
 * Formatting is nanoprintf (0BSD / Unlicense), vendored at lib/main/nanoprintf
 * from https://github.com/charlesnicholson/nanoprintf (commit 115c9160).
 *
 * Always compiled in: %d %i %u %o %x %X %c %s %p %%, length modifier 'l',
 * and the sign flags '+' / ' '. Disabled conversions are emitted verbatim
 * and their argument is not consumed.
 *
 * NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS
 *     Field width, '*' width, '-' left-justify, '0' pad
 * NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS
 *     Precision '.N' and '.*'
 * NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
 *     %f %F. Required by the other FLOAT_* flags
 * NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER
 *     %a %A
 * NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER
 *     %e %E
 * NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER
 *     %g %G
 * NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS
 *     Length modifiers 'h' (short) and 'hh' (char)
 * NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS
 *     Length modifiers 'll' (long long), 'j' (intmax_t), 'z' (size_t), 't' (ptrdiff_t)
 * NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS
 *     %b %B
 * NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS
 *     %n - store the number of characters written so far through an int *
 * NANOPRINTF_USE_ALT_FORM_FLAG
 *     '#' flag: %#x / %#X -> 0x / 0X, %#o -> leading 0
 * NANOPRINTF_USE_FLOAT_SINGLE_PRECISION
 *     Pull float (not double) from va_list. Requires FLOAT_FORMAT_SPECIFIERS
 *
 * rf_printf() / rf_sprintf() / rf_snprintf() carry the printf format
 * attribute, so the compiler validates arguments against the full ISO set.
 *
 * The tfp_* entry points do not, so that the existing call sites passing a
 * runtime-built format string keep compiling.
 */

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS    1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS      1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS          1
#define NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER       0
#define NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER       0
#define NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER  0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS          0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS          1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS         1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS      0
#define NANOPRINTF_USE_ALT_FORM_FLAG                    0
#define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION           0


//// Character sink. The type matches nanoprintf's npf_putc.

typedef void (*putc_f) (int c, void *ctx);

typedef struct {
    putc_f      putf;
    void *      putp;
} stdSink_t;


//// tinyprintf compatibility formatting

int tfp_format(void *putp, putc_f putf, const char *fmt, va_list va);

int tfp_sprintf(char *s, const char *fmt, ...);


//// Native formatting

__attribute__((format(printf, 1, 2)))
int rf_printf(const char *fmt, ...);

__attribute__((format(printf, 2, 3)))
int rf_sprintf(char *s, const char *fmt, ...);

__attribute__((format(printf, 3, 4)))
int rf_snprintf(char *s, size_t n, const char *fmt, ...);


//// No-op printf sink

__attribute__((format(printf, 1, 2)))
static inline int null_printf(__unused const char *fmt, ...) { return 0; }


//// stdout used by printf()

stdSink_t getStdoutSink(void);
void setStdoutSink(stdSink_t sink);


//// Output backends

void printfITMInit(void);
void printfSerialInit(serialPortIdentifier_e port, uint32_t baudRate, portOptions_e options);

void setPrintfSerialPort(serialPort_t *serialPort);


//// Select libc or local printf() implementation

#if defined(UNIT_TEST) || ENABLE_SIMULATOR || (USBD_DEBUG_LEVEL > 0) || defined(ENABLE_STDIO_PREINCLUDE)

#include <stdio.h>

#else

#define sprintf(...)    rf_sprintf(__VA_ARGS__)
#define snprintf(...)   rf_snprintf(__VA_ARGS__)

#if defined(USE_ITM_PRINTF) || defined(USE_SERIAL_PRINTF)
#define printf(...)     rf_printf(__VA_ARGS__)
#elif defined(USE_NULL_PRINTF)
#define printf(...)     null_printf(__VA_ARGS__)
#else
#define printf(...)     printf_not_implemented()
#endif

#endif
