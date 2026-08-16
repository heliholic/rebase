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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#include "utils.h"

#include "scheduler/scheduler.h"

#include "printf.h"


/*
 * Formatting is nanoprintf (0BSD / Unlicense), vendored at lib/main/nanoprintf
 * from https://github.com/charlesnicholson/nanoprintf (commit 115c9160).
 *
 * Supported: %d %i %u %o %x %X %c %s %p %n %% %f %F %e %E
 *            flags '-', '+', ' ', '0', '#',
 *            field width (including '*'), precision ('.N' and '.*'),
 *            length modifiers 'hh' 'h' 'l' 'll' 'z' 't' 'j'
 *
 * Not compiled in: %g %G %a %A and %b %B. Those specifiers are emitted
 * verbatim and their argument is not consumed.
 *
 * Float conversion carries about 9 significant digits, not the full precision
 * of a double - %.10f and beyond start to drift from the true value. %f of a
 * magnitude of ~1e64 or more does not fit the fixed-point path and prints
 * "err"; use %e for those.
 *
 * rf_printf() / rf_sprintf() / rf_snprintf() carry the printf format
 * attribute, so the compiler validates arguments against the full ISO set.
 * The tfp_* entry points do not, so that the existing call sites passing a
 * runtime-built format string keep compiling.
 */

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS    1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS      1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS          1
#define NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER       0
#define NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER       1
#define NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER  0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS          1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS          1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS         0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS      1
#define NANOPRINTF_USE_ALT_FORM_FLAG                    1
#define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION           0

#define NANOPRINTF_VISIBILITY_STATIC
#define NANOPRINTF_IMPLEMENTATION

#include "nanoprintf.h"


//// The sink used by printf()

static stdSink_t stdoutSink = { NULL, NULL };

stdSink_t getStdoutSink(void)
{
    return stdoutSink;
}

void setStdoutSink(stdSink_t sink)
{
    stdoutSink = sink;
}


//// Formatting compatible with tinyprintf

int tfp_format(void *putp, putc_f putf, const char *fmt, va_list va)
{
    return npf_vpprintf(putf, putp, fmt, va);
}

int tfp_sprintf(char *s, const char *fmt, ...)
{
    int written = 0;

    if (s && fmt) {
        va_list va;
        va_start(va, fmt);
        written = npf_vsnprintf(s, SIZE_MAX, fmt, va);
        va_end(va);
    }

    return written;
}


//// Native formatting

int rf_printf(const char *fmt, ...)
{
    const stdSink_t sink = stdoutSink;
    int written = 0;

    if (sink.putf) {
        va_list va;
        va_start(va, fmt);
        written = npf_vpprintf(sink.putf, sink.putp, fmt, va);
        va_end(va);
    }

    return written;
}

int rf_sprintf(char *s, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    const int written = npf_vsnprintf(s, SIZE_MAX, fmt, va);
    va_end(va);

    return written;
}

int rf_snprintf(char *s, size_t n, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    const int written = npf_vsnprintf(s, n, fmt, va);
    va_end(va);

    return written;
}


//// Output backends

#ifndef UNIT_TEST
static void serialPutc(int c, void *ctx)
{
    serialPort_t *port = ctx;

    while (serialTxBytesFree(port) == 0) {
        // Waiting on TX backpressure is not task execution time.
        schedulerIgnoreTaskExecTime();
    }

    serialWrite(port, (uint8_t)c);
}

void setPrintfSerialPort(serialPort_t *serialPort)
{
    stdoutSink.putf = serialPort ? serialPutc : NULL;
    stdoutSink.putp = serialPort;
}

#ifdef USE_SERIAL_PRINTF
void printfSerialInit(serialPortIdentifier_e port, uint32_t baudRate, portOptions_e options)
{
    setPrintfSerialPort(openSerialPort(port, FUNCTION_PRINTF, NULL, NULL, baudRate, MODE_TX, options));
}
#endif // USE_SERIAL_PRINTF
#endif // UNIT_TEST

#ifdef USE_ITM_PRINTF
static void itmPutc(int c, void *ctx)
{
    UNUSED(ctx);
    ITM_SendChar(c);
}

void printfITMInit(void)
{
    stdoutSink.putf = itmPutc;
    stdoutSink.putp = NULL;
}
#endif // USE_ITM_PRINTF
