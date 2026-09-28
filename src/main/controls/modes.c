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
#include <string.h>

#include "platform.h"

#include "build/build_config.h"

#include "common/maths.h"

#include "drivers/time.h"

#include "config/config.h"
#include "pg/arming.h"
#include "fc/feature.h"

#include "io/piniobox.h"

#include "pg/pg.h"
#include "pg/rx.h"

#include "rx/rx.h"

#include "modes.h"

boxBitmap_t rcModeActivationMap;
STATIC_UNIT_TESTED boxBitmap_t stickyModesEverDisabled;

static int activeMacCount = 0;
static uint8_t activeMacArray[MAX_MODE_ACTIVATION_CONDITION_COUNT];
static int activeLinkedMacCount = 0;
static uint8_t activeLinkedMacArray[MAX_MODE_ACTIVATION_CONDITION_COUNT];

bool isModeActive(boxId_e boxId)
{
    return BOXMAP_TEST_ID(&rcModeActivationMap, boxId);
}

void rcModeUpdate(boxBitmap_t newState)
{
    rcModeActivationMap = newState;
}

bool isRangeUsable(const channelRange_t *range)
{
    return
        range->startStep >= MIN_MODE_RANGE_STEP &&
        range->endStep <= MAX_MODE_RANGE_STEP &&
        range->startStep < range->endStep;
}

// The arm switch is a control channel of its own, so BOXARM is not driven by a
// mode activation condition. isArmSwitchConfigured() is what "an arm switch is
// defined" means now.
bool isArmSwitchConfigured(void)
{
    return isRangeUsable(&armingConfig()->arm_range);
}

bool isArmSwitchActive(void)
{
    const channelRange_t *range = &armingConfig()->arm_range;

    if (isArmSwitchConfigured()) {
        const uint16_t channelValue = rcControls[RC_ARM];
        return (channelValue >= STEP_TO_CHANNEL_VALUE(range->startStep) &&
                channelValue < STEP_TO_CHANNEL_VALUE(range->endStep));
    }

    return false;
}

bool isRangeActive(uint8_t channel, const channelRange_t *range)
{
    if (isRangeUsable(range)) {
        const uint16_t channelValue = rcInput[channel];
        return (channelValue >= STEP_TO_CHANNEL_VALUE(range->startStep) &&
                channelValue < STEP_TO_CHANNEL_VALUE(range->endStep));
    }

    return false;
}

/*
 *  updateMapsForMac:
 *
 *  The following are the possible logic states at each MAC update:
 *      AND     NEW
 *      ---     ---
 *       F       F      - no previous AND macs evaluated, no previous active OR macs
 *       F       T      - at least 1 previous active OR mac (***this state is latched True***)
 *       T       F      - all previous AND macs active, no previous active OR macs
 *       T       T      - at least 1 previous inactive AND mac, no previous active OR macs
 */
static void updateMapsForMac(const modeActivationCondition_t *mac, boxBitmap_t *andMap, boxBitmap_t *newMap, bool bActive)
{
    const boxBitmap_t mask = BOXMASK(mac->modeId);

    if (BOXMAP_TEST(andMap, mask) || !BOXMAP_TEST(newMap, mask)) {
        bool bAnd = mac->modeLogic == MODELOGIC_AND;

        if (!bAnd) {    // OR mac
            if (bActive) {
                BOXMAP_CLR(andMap, mask);
                BOXMAP_SET(newMap, mask);
            }
        } else {        // AND mac
            BOXMAP_SET(andMap, mask);
            if (!bActive) {
                BOXMAP_SET(newMap, mask);
            }
        }
    }
}

static void updateMapsForStickyModes(const modeActivationCondition_t *mac, boxBitmap_t *andMap, boxBitmap_t *newMap, bool latchForever)
{
    const boxBitmap_t mask = BOXMASK(mac->modeId);

    if (latchForever && isModeActive(mac->modeId)) {
        BOXMAP_CLR(andMap, mask);
        BOXMAP_SET(newMap, mask);
        return;
    }

    bool bActive = isRangeActive(mac->rcChannel, &mac->range);

    if (BOXMAP_TEST(&stickyModesEverDisabled, mask)) {
        updateMapsForMac(mac, andMap, newMap, bActive);
    } else {
        if (micros() >= STICKY_MODE_BOOT_DELAY_US && !bActive) {
            BOXMAP_SET(&stickyModesEverDisabled, mask);
        }
    }
}

void updateActivatedModes(void)
{
    const boxBitmap_t stickyModes = BOXMASK(BOXPARALYZE) | BOXMASK(BOXBLACKBOXERASE);
    const boxBitmap_t latchModes = BOXMASK(BOXPARALYZE);

    boxBitmap_t newMap = 0;
    boxBitmap_t andMap = 0;

    // determine which conditions set/clear the mode
    for (int i = 0; i < activeMacCount; i++) {
        const modeActivationCondition_t *mac = modeActivationConditions(activeMacArray[i]);
        const boxBitmap_t mask = BOXMASK(mac->modeId);

        if (BOXMAP_TEST(&stickyModes, mask)) {
            updateMapsForStickyModes(mac, &andMap, &newMap, BOXMAP_TEST(&latchModes, mask));
        } else if (mac->modeId < CHECKBOX_ITEM_COUNT) {
            bool bActive = isRangeActive(mac->rcChannel, &mac->range);
            updateMapsForMac(mac, &andMap, &newMap, bActive);
        }
    }

    // Update linked modes
    for (int i = 0; i < activeLinkedMacCount; i++) {
        const modeActivationCondition_t *mac = modeActivationConditions(activeLinkedMacArray[i]);
        const boxBitmap_t linkedMask = BOXMASK(mac->linkedTo);
        bool bActive = BOXMAP_TEST(&andMap, linkedMask) != BOXMAP_TEST(&newMap, linkedMask);

        updateMapsForMac(mac, &andMap, &newMap, bActive);
    }

    boxBitmap_t modes = newMap ^ andMap;

    // The arm switch is not a mode activation condition
    if (isArmSwitchActive()) {
        BOXMAP_SET_ID(&modes, BOXARM);
    } else {
        BOXMAP_CLR_ID(&modes, BOXARM);
    }

    rcModeUpdate(modes);
}

bool isModeActivationConditionPresent(boxId_e modeId)
{
    for (int i = 0; i < MAX_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        const modeActivationCondition_t *mac = modeActivationConditions(i);

        if (mac->modeId == modeId && (isRangeUsable(&mac->range) || mac->linkedTo)) {
            return true;
        }
    }

    return false;
}

bool isModeActivationConditionLinked(boxId_e modeId)
{
    for (int i = 0; i < MAX_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        const modeActivationCondition_t *mac = modeActivationConditions(i);

        if (mac->modeId == modeId && mac->linkedTo != 0) {
            return true;
        }
    }

    return false;
}

void removeModeActivationCondition(const boxId_e modeId)
{
    unsigned offset = 0;
    for (unsigned i = 0; i < MAX_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        modeActivationCondition_t *mac = modeActivationConditionsMutable(i);

        if (mac->modeId == modeId && !offset) {
            offset++;
        }

        if (offset) {
            while (i + offset < MAX_MODE_ACTIVATION_CONDITION_COUNT && modeActivationConditions(i + offset)->modeId == modeId) {
                offset++;
            }

            if (i + offset < MAX_MODE_ACTIVATION_CONDITION_COUNT) {
                memcpy(mac, modeActivationConditions(i + offset), sizeof(modeActivationCondition_t));
            } else {
                memset(mac, 0, sizeof(modeActivationCondition_t));
            }
        }
    }
}

bool isModeActivationConditionConfigured(const modeActivationCondition_t *mac, const modeActivationCondition_t *emptyMac)
{
    if (memcmp(mac, emptyMac, sizeof(*emptyMac))) {
        return true;
    } else {
        return false;
    }
}

// Build the list of used modeActivationConditions indices
// We can then use this to speed up processing by only evaluating used conditions
static void analyzeModeActivationConditions(void)
{
    modeActivationCondition_t emptyMac;
    memset(&emptyMac, 0, sizeof(emptyMac));

    activeMacCount = 0;
    activeLinkedMacCount = 0;

    for (uint8_t i = 0; i < MAX_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        const modeActivationCondition_t *mac = modeActivationConditions(i);
        if (mac->linkedTo) {
            activeLinkedMacArray[activeLinkedMacCount++] = i;
        } else if (isModeActivationConditionConfigured(mac, &emptyMac)) {
            activeMacArray[activeMacCount++] = i;
        }
    }
}

void rcModeInit(void)
{
    analyzeModeActivationConditions();

#if defined(USE_PINIOBOX) && !ENABLE_SIMULATOR_MULTITHREAD
    pinioBoxTaskControl();
#endif
}
