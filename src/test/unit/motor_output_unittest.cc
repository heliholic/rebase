/*
 * This file is part of Betaflight.
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

#include <stdint.h>
#include <iostream>

extern "C" {
#include "platform.h"
#include "drivers/dshot.h"
#include "common/atomic.h"
#include "common/filter.h"
#include "build/debug.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/motor.h"

PG_REGISTER_ARRAY(motorConfig_t, MAX_SUPPORTED_MOTORS, motorConfig, PG_MOTOR_CONFIG);
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

extern "C" {

bool featureIsEnabled(uint8_t f);

// Mocking functions

bool featureIsEnabled(uint8_t f)
{
    UNUSED(f);
    return true;
}

bool useDshotTelemetry = false;
uint8_t debugMode = 0;
int32_t debug[DEBUG_VALUE_COUNT];

float pt1FilterApply(pt1Filter_t *filter, float input)
{
    UNUSED(filter);
    return input;
}

}

// Tests

// Bidirectional DShot inverts the checksum, and only that
TEST(MotorOutputUnittest, TestDshotChecksum)
{
    dshotProtocolControl_t pcb = { .value = 0, .requestTelemetry = false, .bidir = false };
    EXPECT_EQ(0x0000, prepareDshotPacket(&pcb));
    pcb.bidir = true;
    EXPECT_EQ(0x000F, prepareDshotPacket(&pcb));

    for (uint16_t value = DSHOT_MIN_THROTTLE; value <= DSHOT_MAX_THROTTLE; value += 7) {
        dshotProtocolControl_t normal = { .value = value, .requestTelemetry = false, .bidir = false };
        dshotProtocolControl_t bidir = { .value = value, .requestTelemetry = false, .bidir = true };
        const uint16_t a = prepareDshotPacket(&normal);
        const uint16_t b = prepareDshotPacket(&bidir);
        EXPECT_EQ(a >> 4, b >> 4);
        EXPECT_EQ(0xF, (a ^ b) & 0xF);
    }
}

// Each motor converts eRPM with its own pole count
TEST(MotorOutputUnittest, TestErpmToRpm)
{
    motorConfigMutable(0)->poles = 14;
    motorConfigMutable(1)->poles = 2;
    motorConfigMutable(2)->poles = 0;

    // eRPM arrives in units of 100
    EXPECT_FLOAT_EQ(100 * 100 / 7.0f, erpmToRpm(0, 100));
    EXPECT_FLOAT_EQ(100 * 100, erpmToRpm(1, 100));
    // No pole count, no RPM
    EXPECT_FLOAT_EQ(0, erpmToRpm(2, 100));
    EXPECT_FLOAT_EQ(0, erpmToRpm(MAX_SUPPORTED_MOTORS, 100));
}
