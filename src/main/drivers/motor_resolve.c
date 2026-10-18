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

/*
 * Decides which motors can be driven, by which driver and at what rate,
 * from the per-motor configuration and the timer behind each pin.
 *
 * Motors on one hardware timer share its timebase, so they must use
 * protocols with the same timebase, and they run at one update rate: the
 * slowest any of them asks for. Every bitbang motor shares the one pacer
 * timer, and the same port DMA, so those must agree on protocol, telemetry
 * and polarity too. When two motors cannot coexist, the lower-numbered one
 * wins and the other is left undriven.
 *
 * This file has no hardware access, so the rules can be unit tested.
 */

#include <string.h>
#include <strings.h>

#include "platform.h"

#include "common/maths.h"
#include "common/utils.h"

#include "pg/motor.h"

#include "drivers/motor_resolve.h"

// OneShot and MultiShot map the pulse range linearly onto a short window,
// and MultiShot would go negative below this
#define MOTOR_SYNCED_PULSE_MIN      750
#define MOTOR_SYNCED_PULSE_MAX      2250

// A brushed motor takes the pulse range as 0-100% duty
#define MOTOR_BRUSHED_PULSE_MIN     1000
#define MOTOR_BRUSHED_PULSE_MAX     2000

static const char * const motorProtocolNames[MOTOR_PROTOCOL_COUNT] = {
    [MOTOR_PROTOCOL_DISABLED]       = "DISABLED",
    [MOTOR_PROTOCOL_PWM]            = "PWM",
    [MOTOR_PROTOCOL_ONESHOT125]     = "ONESHOT125",
    [MOTOR_PROTOCOL_ONESHOT42]      = "ONESHOT42",
    [MOTOR_PROTOCOL_MULTISHOT]      = "MULTISHOT",
    [MOTOR_PROTOCOL_BRUSHED]        = "BRUSHED",
    [MOTOR_PROTOCOL_DSHOT150]       = "DSHOT150",
    [MOTOR_PROTOCOL_DSHOT300]       = "DSHOT300",
    [MOTOR_PROTOCOL_DSHOT600]       = "DSHOT600",
    [MOTOR_PROTOCOL_PROSHOT1000]    = "PROSHOT1000",
    [MOTOR_PROTOCOL_DRONECAN]       = "DRONECAN",
};

const char *motorProtocolName(uint8_t protocol)
{
    return (protocol < MOTOR_PROTOCOL_COUNT) ? motorProtocolNames[protocol] : "?";
}

// The protocol with this name, ignoring case; -1 if there is none
int motorProtocolFromName(const char *name)
{
    for (int i = 0; i < MOTOR_PROTOCOL_COUNT; i++) {
        if (strcasecmp(name, motorProtocolNames[i]) == 0) {
            return i;
        }
    }
    return -1;
}

bool motorProtocolIsEnabled(uint8_t protocol)
{
    switch (protocol) {
        case MOTOR_PROTOCOL_PWM:
        case MOTOR_PROTOCOL_ONESHOT125:
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_MULTISHOT:
        case MOTOR_PROTOCOL_BRUSHED:
#ifdef USE_DSHOT
        case MOTOR_PROTOCOL_DSHOT150:
        case MOTOR_PROTOCOL_DSHOT300:
        case MOTOR_PROTOCOL_DSHOT600:
        case MOTOR_PROTOCOL_PROSHOT1000:
#endif
#if ENABLE_DRONECAN_ESC
        case MOTOR_PROTOCOL_DRONECAN:
#endif
            return true;
        default:
            break;
    }
    return false;
}

bool motorProtocolIsDshot(uint8_t protocol)
{
#ifdef USE_DSHOT
    switch (protocol) {
        case MOTOR_PROTOCOL_DSHOT150:
        case MOTOR_PROTOCOL_DSHOT300:
        case MOTOR_PROTOCOL_DSHOT600:
        case MOTOR_PROTOCOL_PROSHOT1000:
            return true;
        default:
            break;
    }
#else
    UNUSED(protocol);
#endif
    return false;
}

// Driven by a free-running timer rather than one pulse per update
bool motorProtocolIsContinuous(uint8_t protocol)
{
    return protocol == MOTOR_PROTOCOL_PWM || protocol == MOTOR_PROTOCOL_BRUSHED;
}

motorProtocolFamily_e motorProtocolFamily(uint8_t protocol)
{
    if (motorProtocolIsEnabled(protocol)) {
        if (motorProtocolIsDshot(protocol)) {
            return MOTOR_PROTOCOL_FAMILY_DSHOT;
        }
        if (protocol == MOTOR_PROTOCOL_DRONECAN) {
            return MOTOR_PROTOCOL_FAMILY_CAN;
        }
        return MOTOR_PROTOCOL_FAMILY_PWM;
    }
    return MOTOR_PROTOCOL_FAMILY_UNKNOWN;
}

// The fastest update rate the protocol can carry. A bidirectional DShot
// frame also waits for the ESC's reply, so it can only go half as often.
// DroneCAN has its own broadcast rate, so the motor has none.
uint16_t motorProtocolMaxRate(uint8_t protocol, bool bidir)
{
    uint16_t rate;

    switch (protocol) {
        case MOTOR_PROTOCOL_DISABLED:
        case MOTOR_PROTOCOL_DRONECAN:
            rate = 0;
            break;
        case MOTOR_PROTOCOL_PWM:
            rate = BRUSHLESS_MOTORS_PWM_RATE;
            break;
        case MOTOR_PROTOCOL_ONESHOT125:
            rate = 2000;
            break;
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_DSHOT300:
            rate = 10000;
            break;
        case MOTOR_PROTOCOL_DSHOT150:
            rate = 4000;
            break;
        default:
            rate = MOTOR_RATE_MAX;
            break;
    }

    return bidir ? rate / 2 : rate;
}

// The rate a motor runs at when it does not ask for one
uint16_t motorProtocolDefaultRate(uint8_t protocol, bool bidir)
{
    switch (protocol) {
        case MOTOR_PROTOCOL_PWM:
            return BRUSHLESS_MOTORS_PWM_RATE;
        case MOTOR_PROTOCOL_BRUSHED:
            return BRUSHED_MOTORS_PWM_RATE;
        default:
            return motorProtocolMaxRate(protocol, bidir);
    }
}

uint16_t motorProtocolMinPulse(uint8_t protocol)
{
    switch (protocol) {
        case MOTOR_PROTOCOL_ONESHOT125:
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_MULTISHOT:
            return MOTOR_SYNCED_PULSE_MIN;
        case MOTOR_PROTOCOL_BRUSHED:
            return MOTOR_BRUSHED_PULSE_MIN;
        default:
            return MOTOR_PULSE_MIN;
    }
}

uint16_t motorProtocolMaxPulse(uint8_t protocol)
{
    switch (protocol) {
        case MOTOR_PROTOCOL_ONESHOT125:
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_MULTISHOT:
            return MOTOR_SYNCED_PULSE_MAX;
        case MOTOR_PROTOCOL_BRUSHED:
            return MOTOR_BRUSHED_PULSE_MAX;
        default:
            return MOTOR_PULSE_MAX;
    }
}

static bool motorIsBidir(const motorResolveMotor_t *motor)
{
    return motorProtocolIsDshot(motor->protocol) && (motor->flags & MOTOR_FLAG_BIDIR);
}

static bool motorIsConfigured(const motorResolveInput_t *in, const motorResolveMotor_t *motor)
{
    // DroneCAN addresses ESCs by number, so those need no pin
    return motor->protocol != MOTOR_PROTOCOL_DISABLED &&
        (motor->hasPin || in->pinless || motor->protocol == MOTOR_PROTOCOL_DRONECAN);
}

// Bitbang motors share the pacer timer, the port DMA and the line idle
// level, so they must agree on protocol, telemetry and polarity
static bool motorBitbangCompatible(const motorResolveMotor_t *a, const motorResolveMotor_t *b)
{
    return a->protocol == b->protocol &&
        motorIsBidir(a) == motorIsBidir(b) &&
        (a->flags & MOTOR_FLAG_INVERTED) == (b->flags & MOTOR_FLAG_INVERTED);
}

// Bitbang drives every DShot motor or none of them. AUTO picks it only when
// all of them can use it: alike, none on ProShot, which bitbang has no timing
// for, and on some MCUs all bidirectional.
bool motorResolveBitbang(const motorResolveInput_t *in)
{
    bool bitbang = false;

    if (in->bitbangMode == DSHOT_BITBANG_ON) {
        bitbang = true;
    }
    else if (in->bitbangMode == DSHOT_BITBANG_AUTO) {
        const motorResolveMotor_t *first = NULL;
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
            const motorResolveMotor_t *motor = &in->motor[i];
            if (motorIsConfigured(in, motor) && motorProtocolIsEnabled(motor->protocol) && motorProtocolIsDshot(motor->protocol)) {
                if (motor->protocol == MOTOR_PROTOCOL_PROSHOT1000 || (in->bitbangNeedsBidir && !motorIsBidir(motor)) ||
                    (first && !motorBitbangCompatible(motor, first))) {
                    return false;
                }
                if (!first) {
                    first = motor;
                }
                bitbang = true;
            }
        }
    }

    return bitbang;
}

// Two protocols can share a timer only if they need the same timebase.
// PWM and brushed both free-run at the configured rate.
static bool motorTimebaseCompatible(uint8_t a, uint8_t b)
{
    return a == b || (motorProtocolIsContinuous(a) && motorProtocolIsContinuous(b));
}

static bool motorUsesTimer(const motorResolveInput_t *in, const motorResolveOutput_t *out, int index)
{
    const uint8_t family = out->family[index];

    return !in->pinless && (family == MOTOR_PROTOCOL_FAMILY_PWM || (family == MOTOR_PROTOCOL_FAMILY_DSHOT && !out->bitbang));
}

// Whether a motor's driver makes it share a timebase with an earlier motor
static bool motorSharesTimebase(const motorResolveInput_t *in, const motorResolveOutput_t *out, int index, int other)
{
    const uint8_t family = out->family[index];

    if (family != out->family[other]) {
        // Different drivers only meet on a timer
        return motorUsesTimer(in, out, index) && motorUsesTimer(in, out, other) &&
            in->motor[index].timer == in->motor[other].timer;
    }
    if (family == MOTOR_PROTOCOL_FAMILY_CAN || (family == MOTOR_PROTOCOL_FAMILY_DSHOT && out->bitbang)) {
        // One bus, or one pacer timer, for all of them
        return true;
    }
    return motorUsesTimer(in, out, index) && in->motor[index].timer == in->motor[other].timer;
}

// Whether a motor can share that timebase
static bool motorCanShareTimebase(const motorResolveInput_t *in, const motorResolveOutput_t *out, int index, int other)
{
    const motorResolveMotor_t *motor = &in->motor[index];
    const motorResolveMotor_t *first = &in->motor[other];

    if (out->family[index] != out->family[other]) {
        return false;
    }
    if (out->family[index] == MOTOR_PROTOCOL_FAMILY_CAN) {
        return true;
    }
    if (out->family[index] == MOTOR_PROTOCOL_FAMILY_DSHOT && out->bitbang) {
        return motorBitbangCompatible(motor, first);
    }
    return motorTimebaseCompatible(motor->protocol, first->protocol);
}

static uint16_t motorRequestedRate(const motorResolveMotor_t *motor)
{
    const bool bidir = motorIsBidir(motor);
    const uint16_t maxRate = motorProtocolMaxRate(motor->protocol, bidir);
    uint16_t rate = motor->rate ? MIN(motor->rate, maxRate) : motorProtocolDefaultRate(motor->protocol, bidir);

    if (motor->protocol == MOTOR_PROTOCOL_PWM && motor->maxPulse) {
        // The period must be longer than the longest pulse
        rate = MIN(rate, 1000000 / motor->maxPulse);
    }

    return rate;
}

void motorResolve(const motorResolveInput_t *in, motorResolveOutput_t *out)
{
    memset(out, 0, sizeof(*out));

    out->bitbang = motorResolveBitbang(in);

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        const motorResolveMotor_t *motor = &in->motor[i];

        out->group[i] = i;

        if (!motorIsConfigured(in, motor)) {
            out->state[i] = MOTOR_STATE_ABSENT;
        }
        else if (!motorProtocolIsEnabled(motor->protocol)) {
            out->state[i] = MOTOR_STATE_UNSUPPORTED;
        }
        else {
            out->family[i] = motorProtocolFamily(motor->protocol);

            if (out->family[i] == MOTOR_PROTOCOL_FAMILY_DSHOT && out->bitbang && motor->protocol == MOTOR_PROTOCOL_PROSHOT1000) {
                out->state[i] = MOTOR_STATE_UNSUPPORTED;
            }
            else if (out->family[i] != MOTOR_PROTOCOL_FAMILY_CAN && !in->pinless && motor->timer == MOTOR_TIMER_NONE) {
                out->state[i] = MOTOR_STATE_NO_TIMER;
            }
            else {
                out->state[i] = MOTOR_STATE_OK;
                out->rate[i] = motorRequestedRate(motor);
            }
        }
    }

    // Bidirectional DShot over timer DMA captures the reply on the motor's own
    // channel: an N-channel output cannot, and nor can burst DMA, which AUTO
    // only picks when no motor asks for telemetry
    if (!out->bitbang) {
        bool bidir = false;
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
            if (out->state[i] == MOTOR_STATE_OK && out->family[i] == MOTOR_PROTOCOL_FAMILY_DSHOT && motorIsBidir(&in->motor[i])) {
                if (in->motor[i].nChannel) {
                    out->state[i] = MOTOR_STATE_NO_BIDIR;
                }
                else {
                    bidir = true;
                }
            }
        }
        if (bidir && in->burstMode == DSHOT_DMAR_ON) {
            for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
                if (out->state[i] == MOTOR_STATE_OK && out->family[i] == MOTOR_PROTOCOL_FAMILY_DSHOT && motorIsBidir(&in->motor[i])) {
                    out->state[i] = MOTOR_STATE_NO_BIDIR;
                }
            }
        }
    }

    // Until the DShot drivers take settings per motor, all DShot motors share
    // one protocol and telemetry setting.
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (out->state[i] == MOTOR_STATE_OK && out->family[i] == MOTOR_PROTOCOL_FAMILY_DSHOT) {
            for (int j = 0; j < i; j++) {
                if (out->state[j] == MOTOR_STATE_OK && out->family[j] == MOTOR_PROTOCOL_FAMILY_DSHOT) {
                    if (in->motor[i].protocol != in->motor[j].protocol || motorIsBidir(&in->motor[i]) != motorIsBidir(&in->motor[j])) {
                        out->state[i] = MOTOR_STATE_GROUP_CLASH;
                    }
                    break;
                }
            }
        }
    }

    // Join each motor to the first earlier one it shares a timebase with
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (out->state[i] == MOTOR_STATE_OK) {
            for (int j = 0; j < i; j++) {
                if (out->state[j] == MOTOR_STATE_OK && motorSharesTimebase(in, out, i, j)) {
                    if (!motorCanShareTimebase(in, out, i, j)) {
                        out->state[i] = (motorUsesTimer(in, out, i) && motorUsesTimer(in, out, j)) ?
                            MOTOR_STATE_TIMER_CLASH : MOTOR_STATE_GROUP_CLASH;
                    }
                    else {
                        out->group[i] = out->group[j];
                    }
                    break;
                }
            }
        }
    }

    // A group runs at the slowest rate any of its motors asks for
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (out->state[i] == MOTOR_STATE_OK) {
            const int group = out->group[i];
            out->rate[group] = MIN(out->rate[group], out->rate[i]);
        }
    }
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (out->state[i] == MOTOR_STATE_OK) {
            out->rate[i] = out->rate[out->group[i]];
        }
        else {
            out->family[i] = MOTOR_PROTOCOL_FAMILY_UNKNOWN;
            out->rate[i] = 0;
        }
    }
}
