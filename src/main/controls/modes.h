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

#include "pg/modes.h"

#define BOXID_NONE 255

typedef enum {
    // ARM flag
    BOXARM = 0,

    // Flight modes
    BOXANGLE,
    BOXHORIZON,
    BOXFAILSAFE,
    BOXID_FLIGHTMODE_LAST = BOXFAILSAFE,

    // RC modes
    BOXBEEPERON,
    BOXLEDLOW,
    BOXTELEMETRY,
    BOXSERVO1,
    BOXSERVO2,
    BOXSERVO3,
    BOXBLACKBOX,
    BOXBLACKBOXERASE,
    BOXPREARM,
    BOXBEEPGPSCOUNT,
    BOXPARALYZE,
    BOXUSER1,
    BOXUSER2,
    BOXUSER3,
    BOXUSER4,
    BOXSTICKCOMMANDDISABLE,
    BOXBEEPERMUTE,
    BOXREADY,
    CHECKBOX_ITEM_COUNT
} boxId_e;

typedef enum {
    MODELOGIC_OR = 0,
    MODELOGIC_AND
} modeLogic_e;

// One bit per mode defined in boxId_e
#if CHECKBOX_ITEM_COUNT <= 32
typedef uint32_t boxBitmap_t;
#elif CHECKBOX_ITEM_COUNT <= 64
typedef uint64_t boxBitmap_t;
#else
#error "CHECKBOX_ITEM_COUNT does not fit in boxBitmap_t"
#endif

#define BOXMASK(boxId)                  ((boxBitmap_t)1 << (boxId))

#define BOXMAP_SET(pmap, mask)          (*(pmap) |= (mask))
#define BOXMAP_CLR(pmap, mask)          (*(pmap) &= ~(mask))
#define BOXMAP_TEST(pmap, mask)         ((*(pmap) & (mask)) != 0)

#define BOXMAP_SET_ID(pmap, boxId)      BOXMAP_SET(pmap, BOXMASK(boxId))
#define BOXMAP_CLR_ID(pmap, boxId)      BOXMAP_CLR(pmap, BOXMASK(boxId))
#define BOXMAP_TEST_ID(pmap, boxId)     BOXMAP_TEST(pmap, BOXMASK(boxId))

#define STICKY_MODE_BOOT_DELAY_US       (5 * 1000 * 1000)

// steps are 5 apart
// a value of 0 corresponds to a channel value of 1500
// a value of 100 corresponds to a channel value of 2000
// a value of -100 corresponds to a channel value of 1000

#define MIN_MODE_RANGE_STEP -125
#define MAX_MODE_RANGE_STEP  125

#define STEP_TO_CHANNEL_VALUE(step)   (1500 + 5 * (step))
#define CHANNEL_VALUE_TO_STEP(value)  (((value) - 1500) / 5)

#define IS_RC_MODE_ACTIVE(boxId)    isModeActive(boxId)

bool isModeActive(boxId_e boxId);

void rcModeUpdate(boxBitmap_t newState);

bool isArmSwitchConfigured(void);
bool isArmSwitchActive(void);

bool isRangeUsable(const channelRange_t *range);
bool isRangeActive(uint8_t channel, const channelRange_t *range);

void updateActivatedModes(void);
bool isModeActivationConditionPresent(boxId_e modeId);
bool isModeActivationConditionLinked(boxId_e modeId);
void removeModeActivationCondition(boxId_e modeId);
bool isModeActivationConditionConfigured(const modeActivationCondition_t *mac, const modeActivationCondition_t *emptyMac);

void rcModeInit(void);
