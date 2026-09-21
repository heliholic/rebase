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

#include <stdint.h>

#include "platform.h"

#include "pg/pg.h"

// Swashplate geometry. The servo angles a cyclic-pitch head presents to the
// swash; NONE disables the hard-coded mixer and leaves only the rules.
typedef enum {
    SWASH_TYPE_NONE = 0,
    SWASH_TYPE_THRU,
    SWASH_TYPE_120,
    SWASH_TYPE_135,
    SWASH_TYPE_140,
    SWASH_TYPE_90L,
    SWASH_TYPE_90V,
} swashType_e;

typedef enum {
    TAIL_MODE_SERVO = 0,
    TAIL_MODE_MOTOR,
    TAIL_MODE_COUNT
} tailMode_e;

typedef enum {
    DIR_CW = 0,
    DIR_CCW,
} rotationDir_e;

// Mixer input sources.
//
// STABILIZED_* are the flight controller's own outputs, COMMAND_* the
// pilot's processed stick deflections, and CHANNEL_* the receiver.
//
// The CHANNEL_* slots are receiver channels by number, in receiver order.
// A control function is reached through its COMMAND_* slot, wherever the
// channel map put it. RF 4.6 reordered the whole channel block through the
// channel map, which this tree no longer keeps for anything but the control
// functions.
typedef enum {
    MIXER_IN_NONE = 0,
    MIXER_IN_STABILIZED_ROLL,
    MIXER_IN_STABILIZED_PITCH,
    MIXER_IN_STABILIZED_YAW,
    MIXER_IN_STABILIZED_COLLECTIVE,
    MIXER_IN_STABILIZED_THROTTLE,
    MIXER_IN_COMMAND_ROLL,
    MIXER_IN_COMMAND_PITCH,
    MIXER_IN_COMMAND_YAW,
    MIXER_IN_COMMAND_COLLECTIVE,
    MIXER_IN_COMMAND_THROTTLE,
    MIXER_IN_CHANNEL_1,
    MIXER_IN_CHANNEL_2,
    MIXER_IN_CHANNEL_3,
    MIXER_IN_CHANNEL_4,
    MIXER_IN_CHANNEL_5,
    MIXER_IN_CHANNEL_6,
    MIXER_IN_CHANNEL_7,
    MIXER_IN_CHANNEL_8,
    MIXER_IN_CHANNEL_9,
    MIXER_IN_CHANNEL_10,
    MIXER_IN_CHANNEL_11,
    MIXER_IN_CHANNEL_12,
    MIXER_IN_CHANNEL_13,
    MIXER_IN_CHANNEL_14,
    MIXER_IN_CHANNEL_15,
    MIXER_IN_CHANNEL_16,
    MIXER_IN_CHANNEL_17,
    MIXER_IN_CHANNEL_18,
    MIXER_IN_COUNT
} mixerInputSource_e;

typedef enum {
    MIXER_OP_NUL = 0,
    MIXER_OP_SET,
    MIXER_OP_ADD,
    MIXER_OP_MUL,
    MIXER_OP_COUNT
} mixerOperation_e;

#define MIXER_RULE_COUNT      32
#define MIXER_INPUT_COUNT     MIXER_IN_COUNT

typedef struct {
    uint8_t   main_rotor_dir;       // main rotor direction: CW/CCW
    uint8_t   tail_rotor_mode;      // tail motor vs. variable pitch tail
    uint8_t   tail_motor_idle;      // idle throttle for tail motor
    uint8_t   swash_type;           // swashplate type
    uint8_t   swash_ring;           // swash ring size
    int8_t    swash_geo_correction; // head geometry correction (collective asymmetry)
    int8_t    collective_tilt_correction_pos;
    int8_t    collective_tilt_correction_neg;
    int16_t   tail_center_trim;     // tail center position offset
    int16_t   swash_phase;          // swashplate phasing angle in decidegrees
    uint16_t  swash_pitch_limit;    // maximum main rotor blade pitch
    int16_t   swash_trim[3];        // swash center position trims
} mixerConfig_t;

PG_DECLARE(mixerConfig_t, mixerConfig);

typedef struct {
    int16_t   rate;             // multiplier
    int16_t   min;              // minimum input value
    int16_t   max;              // maximum input value
} mixerInput_t;

PG_DECLARE_ARRAY(mixerInput_t, MIXER_INPUT_COUNT, mixerInputs);

typedef struct {
    uint8_t   oper;             // rule operation
    uint8_t   input;            // input channel
    uint8_t   output;           // output channel
    int16_t   offset;           // addition
    int16_t   weight;           // multiplier (weight and direction)
} mixerRule_t;

PG_DECLARE_ARRAY(mixerRule_t, MIXER_RULE_COUNT, mixerRules);
