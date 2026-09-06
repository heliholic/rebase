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

#include "config/config_reset.h"

#include "sensors/battery.h"

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "pg/battery.h"

#ifndef DEFAULT_CURRENT_METER_SOURCE
#define DEFAULT_CURRENT_METER_SOURCE CURRENT_METER_NONE
#endif

#ifndef DEFAULT_VOLTAGE_METER_SOURCE
#define DEFAULT_VOLTAGE_METER_SOURCE VOLTAGE_METER_NONE
#endif

#ifndef DEFAULT_IBAT_LPF_PERIOD
#define DEFAULT_IBAT_LPF_PERIOD 10
#endif

const batteryProfile_t *currentBatteryProfile;

PG_RESET_FN(batteryProfile_t, batteryProfiles)
{
    for (int i = 0; i < BATTERY_PROFILE_COUNT; i++) {
        batteryProfiles[i].vbatmaxcellvoltage = VBAT_CELL_VOLTAGE_DEFAULT_MAX;
        batteryProfiles[i].vbatmincellvoltage = VBAT_CELL_VOLTAGE_DEFAULT_MIN;
        batteryProfiles[i].vbatwarningcellvoltage = 350;
        batteryProfiles[i].vbatfullcellvoltage = 410;
        batteryProfiles[i].batteryCapacity = 0;
        batteryProfiles[i].forceBatteryCellCount = 0;
        batteryProfiles[i].consumptionWarningPercentage = 10;
        memset(batteryProfiles[i].profileName, 0, sizeof(batteryProfiles[i].profileName));
    }
}

PG_REGISTER_ARRAY_WITH_RESET_FN(batteryProfile_t, BATTERY_PROFILE_COUNT, batteryProfiles, PG_BATTERY_PROFILES, 1);

PG_REGISTER_WITH_RESET_TEMPLATE(batteryConfig_t, batteryConfig, PG_BATTERY_CONFIG, 4);

PG_RESET_TEMPLATE(batteryConfig_t, batteryConfig)
{
    // voltage
    .vbatnotpresentcellvoltage = 300, //A cell below 3 will be ignored
    .voltageMeterSource = DEFAULT_VOLTAGE_METER_SOURCE,
    .lvcPercentage = 100, //Off by default at 100%

    // current
    .currentMeterSource = DEFAULT_CURRENT_METER_SOURCE,

    // warnings / alerts
    .useVBatAlerts = true,
    .useConsumptionAlerts = false,
    .vbathysteresis = 1, // 0.01V

    .vbatDisplayLpfPeriod = 30,
    .ibatLpfPeriod = DEFAULT_IBAT_LPF_PERIOD,
    .vbatDurationForWarning = 0,
    .vbatDurationForCritical = 0,
};
