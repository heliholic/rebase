/*
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "platform.h"

#include "common/utils.h"

#include "pg/pg.h"
#include "pg/pg_limits.h"

#include "drivers/io.h"

#if !defined(BRUSHED_MOTORS_PWM_RATE)
#define BRUSHED_MOTORS_PWM_RATE 16000
#endif

#if !defined(BRUSHLESS_MOTORS_PWM_RATE)
#define BRUSHLESS_MOTORS_PWM_RATE 480
#endif

#define DEFAULT_MOTOR_STOP_PULSE    1000
#define DEFAULT_MOTOR_ZERO_PULSE    1100
#define DEFAULT_MOTOR_FULL_PULSE    1900
#define DEFAULT_MOTOR_POLES           14
#define DEFAULT_MOTOR_KV            1200

#define MOTOR_RATE_MIN                50
#define MOTOR_RATE_MAX             32000
#define MOTOR_PULSE_MIN              100
#define MOTOR_PULSE_MAX             5000
#define MOTOR_POLES_MAX              254
#define MOTOR_KV_MAX               50000

typedef enum {
    MOTOR_PROTOCOL_DISABLED = 0,
    MOTOR_PROTOCOL_PWM,
    MOTOR_PROTOCOL_ONESHOT125,
    MOTOR_PROTOCOL_ONESHOT42,
    MOTOR_PROTOCOL_MULTISHOT,
    MOTOR_PROTOCOL_BRUSHED,
    MOTOR_PROTOCOL_DSHOT150,
    MOTOR_PROTOCOL_DSHOT300,
    MOTOR_PROTOCOL_DSHOT600,
    MOTOR_PROTOCOL_DRONECAN,
    MOTOR_PROTOCOL_COUNT
} PG_ENUM motorProtocol_e;

// Legacy aliases kept for board configs that still use PWM_TYPE_* names
#define PWM_TYPE_PWM              MOTOR_PROTOCOL_PWM
#define PWM_TYPE_ONESHOT125       MOTOR_PROTOCOL_ONESHOT125
#define PWM_TYPE_ONESHOT42        MOTOR_PROTOCOL_ONESHOT42
#define PWM_TYPE_MULTISHOT        MOTOR_PROTOCOL_MULTISHOT
#define PWM_TYPE_BRUSHED          MOTOR_PROTOCOL_BRUSHED
#define PWM_TYPE_DSHOT150         MOTOR_PROTOCOL_DSHOT150
#define PWM_TYPE_DSHOT300         MOTOR_PROTOCOL_DSHOT300
#define PWM_TYPE_DSHOT600         MOTOR_PROTOCOL_DSHOT600
#define PWM_TYPE_DISABLED         MOTOR_PROTOCOL_DISABLED

enum {
    MOTOR_FLAG_INVERTED     = BIT(0),   // output signal is active-low
    MOTOR_FLAG_BIDIR        = BIT(1),   // bidirectional DShot telemetry
    MOTOR_FLAG_EDT          = BIT(2),   // extended DShot telemetry
    MOTOR_FLAG_EDT_FORCE    = BIT(3),   // decode extended telemetry even if the ESC does not announce it
    MOTOR_FLAGS_ALL         = BIT(4) - 1,
};

// The bitbang pacer timer is set by its timer number, or this to let the
// driver pick one of its platform's pacer timers
#define DSHOT_BITBANGED_TIMER_AUTO  0

//TODO: DMAR is platform specific. This should be moved to platform specific code.
typedef enum {
    DSHOT_DMAR_OFF,
    DSHOT_DMAR_ON,
    DSHOT_DMAR_AUTO
} PG_ENUM dshotDmar_e;

typedef enum {
    DSHOT_BITBANG_OFF,
    DSHOT_BITBANG_ON,
    DSHOT_BITBANG_AUTO,
} PG_ENUM dshotBitbangMode_e;

// Values for the DEFAULT_DSHOT_TELEMETRY and DEFAULT_DSHOT_EDT board defaults
typedef enum {
    DSHOT_TELEMETRY_OFF,
    DSHOT_TELEMETRY_ON,
} dshotTelemetry_e;

typedef enum {
    DSHOT_EDT_OFF = 0,
    DSHOT_EDT_ON = 1,
    DSHOT_EDT_FORCE = 2,
} dshotEdt_e;

// Board wiring of a motor output
typedef struct motorDevConfig_s {
    ioTag_t     ioTag;      // output pin
} motorDevConfig_t;

PG_DECLARE_ARRAY(motorDevConfig_t, MAX_SUPPORTED_MOTORS, motorDevConfig);

// What the user sets up for each motor and its ESC
typedef struct motorConfig_s {
    motorProtocol_e         protocol;       // motor control protocol
    uint8_t                 poles;          // motor pole count, 0 for no RPM
    uint16_t                kv;             // motor velocity constant (KV)
    uint16_t                rate;           // update rate in Hz, 0 for the protocol default
    uint16_t                stopPwmPulse;   // PWM pulse while stopped (disarmed) in us
    uint16_t                zeroPwmPulse;   // PWM pulse at zero throttle in us
    uint16_t                fullPwmPulse;   // PWM pulse at full throttle in us
    uint16_t                flags;          // MOTOR_FLAG_*
} motorConfig_t;

PG_DECLARE_ARRAY(motorConfig_t, MAX_SUPPORTED_MOTORS, motorConfig);

// Settings shared by every motor of a driver
typedef struct motorDriverConfig_s {
    dshotDmar_e           useBurstDshot;
    dshotBitbangMode_e    useDshotBitbang;
    uint8_t               useDshotBitbangTimer;     // pacer timer number, DSHOT_BITBANGED_TIMER_AUTO to pick one
} motorDriverConfig_t;

PG_DECLARE(motorDriverConfig_t, motorDriverConfig);
