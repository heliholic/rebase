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
