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

#include <string.h>

#include "platform.h"

#include "common/axis.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/rates.h"

PG_REGISTER_ARRAY_WITH_RESET_TEMPLATE(rateProfile_t, RATE_PROFILE_COUNT, rateProfiles, PG_RATE_PROFILES);

PG_RESET_TEMPLATE(rateProfile_t, rateProfiles)
{
    .profileName = { 0 },
    .rates[FD_ROLL] = 250,
    .rates[FD_PITCH] = 250,
    .rates[FD_YAW] = 400,
    .rates[FD_COLL] = 500,
    .expo[FD_ROLL] = 40,
    .expo[FD_PITCH] = 40,
    .expo[FD_YAW] = 50,
    .expo[FD_COLL] = 0,
    .curve[FD_ROLL] = 12,
    .curve[FD_PITCH] = 12,
    .curve[FD_YAW] = 12,
    .curve[FD_COLL] = 12,
    .levelExpo[FD_ROLL] = 0,
    .levelExpo[FD_PITCH] = 0,
    .response_time[FD_ROLL] = 0,
    .response_time[FD_PITCH] = 0,
    .response_time[FD_YAW] = 0,
    .response_time[FD_COLL] = 0,
    .accel_limit[FD_ROLL] = 0,
    .accel_limit[FD_PITCH] = 0,
    .accel_limit[FD_YAW] = 0,
    .accel_limit[FD_COLL] = 0,
    .cyclic_ring = 150,
    .cyclic_polar = 0,
    .setpoint_boost_gain[FD_ROLL] = 0,
    .setpoint_boost_gain[FD_PITCH] = 0,
    .setpoint_boost_gain[FD_YAW] = 0,
    .setpoint_boost_gain[FD_COLL] = 0,
    .setpoint_boost_cutoff[FD_ROLL] = 15,
    .setpoint_boost_cutoff[FD_PITCH] = 15,
    .setpoint_boost_cutoff[FD_YAW] = 90,
    .setpoint_boost_cutoff[FD_COLL] = 15,
    .yaw_dynamic_ceiling_gain = 0,
    .yaw_dynamic_deadband_gain = 10,
    .yaw_dynamic_deadband_cutoff = 50,
    .yaw_dynamic_deadband_filter = 60,
};
