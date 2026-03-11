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

#include "types.h"
#include "platform.h"

#ifdef USE_SERVOS

#include "pg/pg_ids.h"
#include "pg/servo.h"

#include "config/config_reset.h"

#include "drivers/io.h"

PG_REGISTER_WITH_RESET_FN(servoConfig_t, servoConfig, PG_SERVO_CONFIG, 1);

void pgResetFn_servoConfig(servoConfig_t *servoConfig)
{
    UNUSED(servoConfig);
#ifdef SERVO1_PIN
    servoConfig->ioTags[0] = IO_TAG(SERVO1_PIN);
#endif
#ifdef SERVO2_PIN
    servoConfig->ioTags[1] = IO_TAG(SERVO2_PIN);
#endif
#ifdef SERVO3_PIN
    servoConfig->ioTags[2] = IO_TAG(SERVO3_PIN);
#endif
#ifdef SERVO4_PIN
    servoConfig->ioTags[3] = IO_TAG(SERVO4_PIN);
#endif
#ifdef SERVO5_PIN
    servoConfig->ioTags[4] = IO_TAG(SERVO5_PIN);
#endif
#ifdef SERVO6_PIN
    servoConfig->ioTags[5] = IO_TAG(SERVO6_PIN);
#endif
#ifdef SERVO7_PIN
    servoConfig->ioTags[6] = IO_TAG(SERVO7_PIN);
#endif
#ifdef SERVO8_PIN
    servoConfig->ioTags[7] = IO_TAG(SERVO8_PIN);
#endif
}

PG_REGISTER_ARRAY_WITH_RESET_FN(servoParam_t, MAX_SUPPORTED_SERVOS, servoParams, PG_SERVO_PARAMS, 1);

void pgResetFn_servoParams(servoParam_t *instance)
{
    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        RESET_CONFIG(servoParam_t, &instance[i],
                     .mid   = DEFAULT_SERVO_CENTER,
                     .min   = DEFAULT_SERVO_MIN,
                     .max   = DEFAULT_SERVO_MAX,
                     .rneg  = DEFAULT_SERVO_SCALE,
                     .rpos  = DEFAULT_SERVO_SCALE,
                     .rate  = DEFAULT_SERVO_RATE,
                     .speed = DEFAULT_SERVO_SPEED,
                     .flags = DEFAULT_SERVO_FLAGS,
        );
    }
}

#endif
