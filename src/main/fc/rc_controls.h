/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>

#include "common/axis.h"
#include "common/filter.h"

#include "pg/pg.h"
#include "pg/arming.h"
#include "pg/rc_controls.h"

#include "rx/rx.h"

typedef enum {
    THROTTLE_LOW = 0,
    THROTTLE_HIGH
} throttleStatus_e;

typedef enum {
    NOT_CENTERED = 0,
    CENTERED
} rollPitchStatus_e;

// The sticks are roll, pitch, yaw and collective; throttle is not on a stick.
#define ROL_LO (1 << (2 * RC_ROLL))
#define ROL_CE (3 << (2 * RC_ROLL))
#define ROL_HI (2 << (2 * RC_ROLL))
#define PIT_LO (1 << (2 * RC_PITCH))
#define PIT_CE (3 << (2 * RC_PITCH))
#define PIT_HI (2 << (2 * RC_PITCH))
#define YAW_LO (1 << (2 * RC_YAW))
#define YAW_CE (3 << (2 * RC_YAW))
#define YAW_HI (2 << (2 * RC_YAW))
#define COL_LO (1 << (2 * RC_COLLECTIVE))
#define COL_CE (3 << (2 * RC_COLLECTIVE))
#define COL_HI (2 << (2 * RC_COLLECTIVE))
#define COL_MASK (3 << (2 * RC_COLLECTIVE))

#define CONTROL_RATE_CONFIG_RC_EXPO_MAX  100

#define CONTROL_RATE_CONFIG_RC_RATES_MAX  255

#define CONTROL_RATE_CONFIG_RATE_LIMIT_MIN  200
#define CONTROL_RATE_CONFIG_RATE_LIMIT_MAX  1998

// (Super) rates are constrained to [0, 100] for Betaflight rates, so values higher than 100 won't make a difference. Range extended for RaceFlight rates.
#define CONTROL_RATE_CONFIG_RATE_MAX  255

bool areUsingSticksToArm(void);

throttleStatus_e calculateThrottleStatus(void);
void processRcStickPositions(void);

bool isUsingSticksForArming(void);

void rcControlsInit(void);

bool wasLastDisarmUserRequested(void); // Check if the user has requested a disarm since the last cleared

void clearWasLastDisarmUserRequested(void); // Clear the user disarm request flag
