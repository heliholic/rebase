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

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "drivers/motor_types.h"

STATIC_ASSERT(MAX_SUPPORTED_MOTORS <= 32, motor_masks_hold_every_motor);
STATIC_ASSERT(MAX_SUPPORTED_SERVOS <= 32, servo_masks_hold_every_servo);

#define MOTOR_TIMER_NONE    (-1)

// Why a motor is, or is not, driven
typedef enum {
    MOTOR_STATE_ABSENT = 0,     // protocol disabled, or no pin
    MOTOR_STATE_OK,             // the driver may set it up
    MOTOR_STATE_UNSUPPORTED,    // protocol not built in, or not possible on its driver
    MOTOR_STATE_NO_TIMER,       // the pin has no timer
    MOTOR_STATE_TIMER_CLASH,    // an earlier motor on the same timer needs another timebase
    MOTOR_STATE_GROUP_CLASH,    // differs from an earlier motor its driver must treat alike
    MOTOR_STATE_NO_BIDIR,       // bidirectional DShot asked for where the reply cannot be captured
    MOTOR_STATE_FAILED,         // the driver could not set it up
    MOTOR_STATE_COUNT
} motorState_e;

typedef struct {
    uint16_t rate;              // requested update rate in Hz, 0 for the protocol default
    uint16_t maxPulse;          // longest PWM pulse in us
    uint16_t flags;             // MOTOR_FLAG_*
    uint8_t  protocol;          // motorProtocol_e
    int8_t   timer;             // timer number of the pin, MOTOR_TIMER_NONE if none
    bool     hasPin;
    bool     nChannel;          // the pin is a complementary (N) timer output
} motorResolveMotor_t;

typedef struct {
    uint16_t rate;              // requested update rate in Hz
    uint16_t maxPulse;          // longest pulse in us
    int8_t   timer;             // timer number of the pin, MOTOR_TIMER_NONE if none
} motorResolveServo_t;

typedef struct {
    motorResolveMotor_t motor[MAX_SUPPORTED_MOTORS];
    motorResolveServo_t servo[MAX_SUPPORTED_SERVOS];
    bool    pinless;            // the simulator: motors need neither pin nor timer
    uint8_t bitbangMode;        // dshotBitbangMode_e; OFF where bitbang is not built in
    uint8_t burstMode;          // dshotDmar_e; OFF where burst DMA is not built in
    bool    bitbangNeedsBidir;  // AUTO picks bitbang only for bidirectional DShot
} motorResolveInput_t;

typedef struct {
    uint8_t  family[MAX_SUPPORTED_MOTORS];  // motorProtocolFamily_e of the driver
    uint8_t  state[MAX_SUPPORTED_MOTORS];   // motorState_e
    uint8_t  group[MAX_SUPPORTED_MOTORS];   // lowest motor sharing its timebase
    uint16_t rate[MAX_SUPPORTED_MOTORS];    // update rate of its group in Hz, 0 if not applicable
    uint16_t servoRate[MAX_SUPPORTED_SERVOS];   // rate of a servo on a motor's timer, 0 if its timer has none
    uint32_t refusedServos;                 // servos on a timer a motor needs another timebase on
    bool     bitbang;                       // DShot motors use the bitbang driver
} motorResolveOutput_t;

const char *motorProtocolName(uint8_t protocol);
int motorProtocolFromName(const char *name);

bool motorProtocolIsEnabled(uint8_t protocol);
bool motorProtocolIsDshot(uint8_t protocol);
bool motorProtocolIsContinuous(uint8_t protocol);
motorProtocolFamily_e motorProtocolFamily(uint8_t protocol);

uint16_t motorProtocolMaxRate(uint8_t protocol, bool bidir);
uint16_t motorProtocolDefaultRate(uint8_t protocol, bool bidir);
uint16_t motorProtocolMinPulse(uint8_t protocol);
uint16_t motorProtocolMaxPulse(uint8_t protocol);

uint16_t motorFrameDivider(uint16_t rate, uint32_t looptimeUs);

bool motorResolveBitbang(const motorResolveInput_t *in);
void motorResolve(const motorResolveInput_t *in, motorResolveOutput_t *out);
