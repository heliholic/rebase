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

#include "platform.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/mixer.h"


PG_REGISTER_WITH_RESET_TEMPLATE(mixerConfig_t, mixerConfig, PG_MIXER_CONFIG);

PG_RESET_TEMPLATE(mixerConfig_t, mixerConfig)
{
    .main_rotor_dir = DIR_CW,
    .tail_rotor_mode = TAIL_MODE_SERVO,
    .tail_motor_idle = 0,
    .swash_type = SWASH_TYPE_120,
    .swash_ring = 100,
    .swash_geo_correction = 0,
    .collective_tilt_correction_pos = 0,
    .collective_tilt_correction_neg = 10,
    .tail_center_trim = 0,
    .swash_phase = 0,
    .swash_pitch_limit = 0,
    .swash_trim = { 0, 0, 0 },
};

// An empty rule table. The hard-coded swash mixer drives a conventional
// helicopter on its own; rules are what a pilot adds on top of it.
PG_REGISTER_ARRAY(mixerRule_t, MIXER_RULE_COUNT, mixerRules, PG_MIXER_RULES);

PG_REGISTER_ARRAY_WITH_RESET_FN(mixerInput_t, MIXER_INPUT_COUNT, mixerInputs, PG_MIXER_INPUTS);

PG_RESET_FN(mixerInput_t, mixerInputs)
{
    // The four stabilized axes pass at 100% and are allowed 125%, so a swash
    // can reach its mechanical limits before the mixer clips.
    if (index >= MIXER_IN_STABILIZED_ROLL && index <= MIXER_IN_STABILIZED_COLLECTIVE) {
        mixerInputs->rate =  1000;
        mixerInputs->min  = -1250;
        mixerInputs->max  =  1250;
    }
    else if (index == MIXER_IN_STABILIZED_THROTTLE) {
        mixerInputs->rate =  1000;
        mixerInputs->min  =  0;
        mixerInputs->max  =  1000;
    }
    else if (index >= MIXER_IN_COMMAND_ROLL && index <= MIXER_IN_CHANNEL_18) {
        mixerInputs->rate =  1000;
        mixerInputs->min  = -1000;
        mixerInputs->max  =  1000;
    }
    else {
        mixerInputs->rate =  0;
        mixerInputs->min  =  0;
        mixerInputs->max  =  0;
    }
}
