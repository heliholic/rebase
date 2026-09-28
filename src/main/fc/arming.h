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

#include "common/utils.h"

#include "drivers/time.h"

// Reasons logged with a disarm event. The numbering is stored in blackbox logs,
// so a retired reason keeps its slot.
typedef enum {
    DISARM_REASON_ARMING_DISABLED   = 0,
    DISARM_REASON_FAILSAFE          = 1,
    DISARM_REASON_UNUSED_2          = 2,
    DISARM_REASON_UNUSED_3          = 3,
    DISARM_REASON_SWITCH            = 4,
    DISARM_REASON_UNUSED_5          = 5,
    DISARM_REASON_UNUSED_6          = 6,
    DISARM_REASON_UNUSED_7          = 7,
    DISARM_REASON_SERIAL_COMMAND    = 8,
    DISARM_REASON_LANDING           = 9,
#ifdef UNIT_TEST
    DISARM_REASON_SYSTEM            = 255,
#endif
} flightLogDisarmReason_e;

/*
 * Arming disable flags are listed in the order of criticalness.
 * ARM_SWITCH is last, since it is always set when any other flag is.
 */
typedef enum {
    ARMING_DISABLED_NO_GYRO_BIT         = 0,
    ARMING_DISABLED_FAILSAFE_BIT        = 1,
    ARMING_DISABLED_RX_FAILSAFE_BIT     = 2,
    ARMING_DISABLED_NOT_DISARMED_BIT    = 3,
    ARMING_DISABLED_BOXFAILSAFE_BIT     = 4,
    ARMING_DISABLED_NO_ARM_SWITCH_BIT   = 5,
    ARMING_DISABLED_UNUSED_6_BIT        = 6,
    ARMING_DISABLED_THROTTLE_BIT        = 7,
    ARMING_DISABLED_ANGLE_BIT           = 8,
    ARMING_DISABLED_BOOT_GRACE_TIME_BIT = 9,
    ARMING_DISABLED_NOPREARM_BIT        = 10,
    ARMING_DISABLED_LOAD_BIT            = 11,
    ARMING_DISABLED_CALIBRATING_BIT     = 12,
    ARMING_DISABLED_CLI_BIT             = 13,
    ARMING_DISABLED_UNUSED_14_BIT       = 14,
    ARMING_DISABLED_BST_BIT             = 15,
    ARMING_DISABLED_MSP_BIT             = 16,
    ARMING_DISABLED_PARALYZE_BIT        = 17,
    ARMING_DISABLED_GPS_BIT             = 18,
    ARMING_DISABLED_UNUSED_19_BIT       = 19,
    ARMING_DISABLED_DSHOT_TELEM_BIT     = 20,
    ARMING_DISABLED_REBOOT_REQUIRED_BIT = 21,
    ARMING_DISABLED_DSHOT_BITBANG_BIT   = 22,
    ARMING_DISABLED_ACC_CALIBRATION_BIT = 23,
    ARMING_DISABLED_MOTOR_PROTOCOL_BIT  = 24,
    ARMING_DISABLED_UNUSED_25_BIT       = 25,
    ARMING_DISABLED_UNUSED_26_BIT       = 26,
    ARMING_DISABLED_UNUSED_27_BIT       = 27,
    ARMING_DISABLED_UNUSED_28_BIT       = 28,

    // Needs to be the last element, since it's always activated if one of the others is active when arming
    ARMING_DISABLED_ARM_SWITCH_BIT      = 31,

    ARMING_DISABLE_FLAGS_COUNT
} armingDisableBits_e;

#define ENTRY(NAME)   ARMING_DISABLED_##NAME = BIT(ARMING_DISABLED_##NAME##_BIT)

typedef enum {
    ENTRY(NO_GYRO),
    ENTRY(FAILSAFE),
    ENTRY(RX_FAILSAFE),
    ENTRY(NOT_DISARMED),
    ENTRY(BOXFAILSAFE),
    ENTRY(NO_ARM_SWITCH),
    ENTRY(UNUSED_6),
    ENTRY(THROTTLE),
    ENTRY(ANGLE),
    ENTRY(BOOT_GRACE_TIME),
    ENTRY(NOPREARM),
    ENTRY(LOAD),
    ENTRY(CALIBRATING),
    ENTRY(CLI),
    ENTRY(UNUSED_14),
    ENTRY(BST),
    ENTRY(MSP),
    ENTRY(PARALYZE),
    ENTRY(GPS),
    ENTRY(UNUSED_19),
    ENTRY(DSHOT_TELEM),
    ENTRY(REBOOT_REQUIRED),
    ENTRY(DSHOT_BITBANG),
    ENTRY(ACC_CALIBRATION),
    ENTRY(MOTOR_PROTOCOL),
    ENTRY(UNUSED_25),
    ENTRY(UNUSED_26),
    ENTRY(UNUSED_27),
    ENTRY(UNUSED_28),
    ENTRY(ARM_SWITCH),
} armingDisableFlags_e;

#undef ENTRY

void setArmingDisabled(armingDisableFlags_e flag);
void unsetArmingDisabled(armingDisableFlags_e flag);
bool isArmingDisabled(void);
armingDisableFlags_e getArmingDisableFlags(void);
const char *getArmingDisableFlagName(armingDisableFlags_e flag);

void resetArmingDisabled(void);

void updateArmingStatus(void);
void processArmSwitch(void);
void updateArmedBeeper(void);

void tryArm(void);
void disarm(flightLogDisarmReason_e reason);

timeUs_t getLastDisarmTimeUs(void);
bool isTryingToArm(void);
void resetTryingToArm(void);
