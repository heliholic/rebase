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


//// The sink used by printf()

stdSink_t getStdoutSink(void);
void setStdoutSink(stdSink_t sink);


//// Output backends

void printfITMInit(void);
void printfSerialInit(serialPortIdentifier_e port, uint32_t baudRate, portOptions_e options);

void setPrintfSerialPort(serialPort_t *serialPort);
