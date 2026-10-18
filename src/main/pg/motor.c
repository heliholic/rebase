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

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#ifdef USE_MOTOR

#include "common/utils.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/motor.h"

#if !defined(DEFAULT_DSHOT_BITBANG)
#define DEFAULT_DSHOT_BITBANG DSHOT_BITBANG_AUTO
#endif

#if !defined(DSHOT_BITBANGED_TIMER_DEFAULT)
#define DSHOT_BITBANGED_TIMER_DEFAULT DSHOT_BITBANGED_TIMER_AUTO
#endif

#if !defined(DEFAULT_DSHOT_BURST)
#define DEFAULT_DSHOT_BURST DSHOT_DMAR_OFF
#endif

#if !defined(DEFAULT_DSHOT_TELEMETRY)
#define DEFAULT_DSHOT_TELEMETRY DSHOT_TELEMETRY_OFF
#endif

#if !defined(DEFAULT_DSHOT_EDT)
#define DEFAULT_DSHOT_EDT DSHOT_EDT_OFF
#endif

// Default motor protocol: a target/config may force one via DEFAULT_MOTOR_PROTOCOL
// (e.g. BRUSHED, or PWM on a DShot-capable build); otherwise pick by capability.
#if defined(DEFAULT_MOTOR_PROTOCOL)
#define MOTOR_PROTOCOL_DEFAULT      DEFAULT_MOTOR_PROTOCOL
#elif !defined(USE_DSHOT) && defined(USE_PWM_OUTPUT)
#define MOTOR_PROTOCOL_DEFAULT      MOTOR_PROTOCOL_PWM
#elif defined(USE_DSHOT) && defined(DEFAULT_MOTOR_DSHOT_SPEED)
#define MOTOR_PROTOCOL_DEFAULT      DEFAULT_MOTOR_DSHOT_SPEED
#elif defined(USE_DSHOT)
#define MOTOR_PROTOCOL_DEFAULT      MOTOR_PROTOCOL_DSHOT600
#else
#define MOTOR_PROTOCOL_DEFAULT      MOTOR_PROTOCOL_DISABLED
#endif

// Board override for the motor update rate (Hz); 0 leaves it to the protocol,
// e.g. an external ESC that reads a duty-cycle PWM throttle at a specific rate.
#if defined(DEFAULT_MOTOR_PWM_RATE)
#define MOTOR_RATE_DEFAULT          DEFAULT_MOTOR_PWM_RATE
#else
#define MOTOR_RATE_DEFAULT          0
#endif

#if defined(USE_DSHOT_TELEMETRY)
#define MOTOR_FLAGS_DEFAULT \
    ((DEFAULT_DSHOT_TELEMETRY == DSHOT_TELEMETRY_ON ? MOTOR_FLAG_BIDIR : 0) | \
     (DEFAULT_DSHOT_EDT >= DSHOT_EDT_ON ? MOTOR_FLAG_EDT : 0) | \
     (DEFAULT_DSHOT_EDT == DSHOT_EDT_FORCE ? MOTOR_FLAG_EDT_FORCE : 0))
#else
#define MOTOR_FLAGS_DEFAULT         0
#endif

PG_REGISTER_ARRAY_WITH_RESET_FN(motorDevConfig_t, MAX_SUPPORTED_MOTORS, motorDevConfig, PG_MOTOR_DEV_CONFIG);

PG_REGISTER_ARRAY_WITH_RESET_TEMPLATE(motorConfig_t, MAX_SUPPORTED_MOTORS, motorConfig, PG_MOTOR_CONFIG);

PG_REGISTER_WITH_RESET_FN(motorDriverConfig_t, motorDriverConfig, PG_MOTOR_DRIVER_CONFIG);

PG_RESET_FN(motorDevConfig_t, motorDevConfig)
{
    UNUSED(motorDevConfig);

    switch (index) {
#if defined(MOTOR1_PIN)
    case 0:
        motorDevConfig->ioTag = IO_TAG(MOTOR1_PIN);
        break;
#endif
#if defined(MOTOR2_PIN)
    case 1:
        motorDevConfig->ioTag = IO_TAG(MOTOR2_PIN);
        break;
#endif
#if defined(MOTOR3_PIN)
    case 2:
        motorDevConfig->ioTag = IO_TAG(MOTOR3_PIN);
        break;
#endif
#if defined(MOTOR4_PIN)
    case 3:
        motorDevConfig->ioTag = IO_TAG(MOTOR4_PIN);
        break;
#endif
#if defined(MOTOR5_PIN)
    case 4:
        motorDevConfig->ioTag = IO_TAG(MOTOR5_PIN);
        break;
#endif
#if defined(MOTOR6_PIN)
    case 5:
        motorDevConfig->ioTag = IO_TAG(MOTOR6_PIN);
        break;
#endif
#if defined(MOTOR7_PIN)
    case 6:
        motorDevConfig->ioTag = IO_TAG(MOTOR7_PIN);
        break;
#endif
#if defined(MOTOR8_PIN)
    case 7:
        motorDevConfig->ioTag = IO_TAG(MOTOR8_PIN);
        break;
#endif
    default:
        break;
    }
}

PG_RESET_TEMPLATE(motorConfig_t, motorConfig)
{
    .protocol     = MOTOR_PROTOCOL_DEFAULT,
    .poles        = DEFAULT_MOTOR_POLES,
    .kv           = DEFAULT_MOTOR_KV,
    .rate         = MOTOR_RATE_DEFAULT,
    .stopPwmPulse = DEFAULT_MOTOR_STOP_PULSE,
    .zeroPwmPulse = DEFAULT_MOTOR_ZERO_PULSE,
    .fullPwmPulse = DEFAULT_MOTOR_FULL_PULSE,
    .flags        = MOTOR_FLAGS_DEFAULT,
};

PG_RESET_FN(motorDriverConfig_t, motorDriverConfig)
{
    UNUSED(motorDriverConfig);

#ifdef USE_DSHOT_DMAR
    motorDriverConfig->useBurstDshot = DEFAULT_DSHOT_BURST;
#endif

#ifdef USE_DSHOT_BITBANG
    motorDriverConfig->useDshotBitbang = DEFAULT_DSHOT_BITBANG;
    motorDriverConfig->useDshotBitbangTimer = DSHOT_BITBANGED_TIMER_DEFAULT;
#endif
}

#endif // USE_MOTOR
