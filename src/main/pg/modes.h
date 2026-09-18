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

#include "pg/pg.h"

// steps are 5 apart
// a value of 0 corresponds to a channel value of 1500
// a value of 100 corresponds to a channel value of 2000
// a value of -100 corresponds to a channel value of 1000

typedef struct {
    int8_t          startStep;
    int8_t          endStep;
} channelRange_t;

typedef struct {
    uint8_t         boxId;
    uint8_t         linkedTo;
    uint8_t         modeLogic;
    uint8_t         rcChannel;
    channelRange_t  range;
} modeActivationCondition_t;


#define MAX_MODE_ACTIVATION_CONDITION_COUNT 20

PG_DECLARE_ARRAY(modeActivationCondition_t, MAX_MODE_ACTIVATION_CONDITION_COUNT, modeActivationConditions);


#define MAX_BOX_USER_NAME_LENGTH 16
#define BOX_USER_NAME_COUNT      4

typedef struct {
    char box_user_names[BOX_USER_NAME_COUNT][MAX_BOX_USER_NAME_LENGTH];
} modeActivationConfig_t;

PG_DECLARE(modeActivationConfig_t, modeActivationConfig);
