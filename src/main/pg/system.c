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

#include "build/debug.h"

#include "common/utils.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "pg/system.h"

STATIC_ASSERT(sizeof(TARGET_BOARD_IDENTIFIER) <= MAX_BOARD_IDENTIFIER_LENGTH + 1, board_identifier_too_long);

PG_REGISTER_WITH_RESET_TEMPLATE(systemConfig_t, systemConfig, PG_SYSTEM_CONFIG, 4);

PG_RESET_TEMPLATE(systemConfig_t, systemConfig,
    .pidProfileIndex = 0,
    .activeRateProfile = 0,
    .debug_mode = DEBUG_MODE,
    .task_statistics = true,
    .cpu_overclock = DEFAULT_CPU_OVERCLOCK,
    .powerOnArmingGraceTime = 5,
    .boardIdentifier = TARGET_BOARD_IDENTIFIER,
    .hseMhz = SYSTEM_HSE_MHZ,  // Only used for F4 and G4 targets
    .configurationState = CONFIGURATION_STATE_UNCONFIGURED,
    .enableStickArming = false,
    .activeBatteryProfile = 0,
);
