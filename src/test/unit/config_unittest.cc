/*
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rotorflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>

extern "C" {
    #include "platform.h"

    #include "config/config.h"

    #include "common/sensor_alignment.h"

    #include "drivers/motor_types.h"

    #include "fc/feature.h"
    #include "fc/runtime_config.h"

    #include "controls/modes.h"

    #include "io/serial.h"

    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/acceleration.h"
    #include "pg/battery.h"
    #include "pg/beeper.h"
    #include "pg/blackbox.h"
    #include "pg/compass.h"
    #include "pg/failsafe.h"
    #include "pg/gps.h"
    #include "pg/gyro.h"
    #include "pg/gyrodev.h"
    #include "pg/modes.h"
    #include "pg/motor.h"
    #include "pg/pid.h"
    #include "pg/rates.h"
    #include "pg/rx.h"
    #include "pg/serial.h"
    #include "pg/serial_port.h"
    #include "pg/system.h"

    #include "sensors/gyro.h"

    void validateAndFixConfig(void);

    // The configs validateAndFixConfig() walks that this test has no interest
    // in. Their own reset functions live in the subsystems, which are not
    // linked here, so they take the zeroed defaults.
    PG_REGISTER(accelerometerConfig_t, accelerometerConfig, PG_ACCELEROMETER_CONFIG);
    PG_REGISTER(beeperConfig_t, beeperConfig, PG_BEEPER_CONFIG);
    PG_REGISTER(blackboxConfig_t, blackboxConfig, PG_BLACKBOX_CONFIG);
    PG_REGISTER(compassConfig_t, compassConfig, PG_COMPASS_CONFIG);
    PG_REGISTER(failsafeConfig_t, failsafeConfig, PG_FAILSAFE_CONFIG);
    PG_REGISTER(gpsConfig_t, gpsConfig, PG_GPS_CONFIG);
    PG_REGISTER(gyroConfig_t, gyroConfig, PG_GYRO_CONFIG);
    PG_REGISTER_ARRAY(motorConfig_t, MAX_SUPPORTED_MOTORS, motorConfig, PG_MOTOR_CONFIG);
    PG_REGISTER_ARRAY(motorDevConfig_t, MAX_SUPPORTED_MOTORS, motorDevConfig, PG_MOTOR_DEV_CONFIG);
    PG_REGISTER(pidConfig_t, pidConfig, PG_PID_CONFIG);
    PG_REGISTER(serialConfig_t, serialConfig, PG_SERIAL_CONFIG);
    PG_REGISTER(serialPinConfig_t, serialPinConfig, PG_SERIAL_PIN_CONFIG);
    PG_REGISTER(systemConfig_t, systemConfig, PG_SYSTEM_CONFIG);

    PG_REGISTER_ARRAY(batteryProfile_t, BATTERY_PROFILE_COUNT, batteryProfiles, PG_BATTERY_PROFILES);
    PG_REGISTER_ARRAY(rateProfile_t, RATE_PROFILE_COUNT, rateProfiles, PG_RATE_PROFILES);
    PG_REGISTER_ARRAY(gyroDeviceConfig_t, MAX_GYRODEV_COUNT, gyroDeviceConfig, PG_GYRO_DEVICE_CONFIG);
    PG_REGISTER_ARRAY(modeActivationCondition_t, MAX_MODE_ACTIVATION_CONDITION_COUNT, modeActivationConditions, PG_MODE_ACTIVATION_PROFILE);
    PG_REGISTER_ARRAY(pidProfile_t, PID_PROFILE_COUNT, pidProfiles, PG_PID_PROFILE);

    gyro_t gyro;
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

// The map as PG_RESET_FN(rxConfig_t, rxConfig) leaves it.
static const uint8_t defaultControlMap[RC_CONTROL_COUNT] = { 0, 1, 3, 4, 2, 5 };

static void setControlMap(const uint8_t map[RC_CONTROL_COUNT])
{
    for (int function = 0; function < RC_CONTROL_COUNT; function++) {
        rxConfigMutable()->rc_channel[function] = map[function];
    }
}

// Every control function must end up on a receiver channel of its own, within
// the range the Rx layer can index. Both properties are what the Rx layer -
// which reverses rc_channel[] into a channel -> function map - relies on.
static void expectUsableControlMap(void)
{
    for (int function = 0; function < RC_CONTROL_COUNT; function++) {
        const uint8_t channel = rxConfig()->rc_channel[function];

        EXPECT_LT(channel, MAX_SUPPORTED_RC_CHANNEL_COUNT)
            << "control function " << function << " is off the end of rcInput[]";

        for (int other = function + 1; other < RC_CONTROL_COUNT; other++) {
            EXPECT_NE(channel, rxConfig()->rc_channel[other])
                << "control functions " << function << " and " << other
                << " share input channel " << (int)channel;
        }
    }
}

static void expectControlMap(const uint8_t expected[RC_CONTROL_COUNT])
{
    for (int function = 0; function < RC_CONTROL_COUNT; function++) {
        EXPECT_EQ(expected[function], rxConfig()->rc_channel[function])
            << "control function " << function;
    }
}

class ControlMapTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pgResetAll();
    }
};

// The shipped default map is already usable, and validation must leave it be.
TEST_F(ControlMapTest, DefaultMapIsKept)
{
    validateAndFixConfig();

    expectControlMap(defaultControlMap);
    expectUsableControlMap();
}

// Any unique in-range map is the pilot's business, however unlike the default
// it looks. Validation must not tidy it up.
TEST_F(ControlMapTest, UnusualButUsableMapIsKept)
{
    const uint8_t map[RC_CONTROL_COUNT] = { 17, 16, 15, 14, 13, 12 };
    setControlMap(map);

    validateAndFixConfig();

    expectControlMap(map);
}

// MSP_SET_RX_CONFIG bounds what it writes, but a record restored from an older
// layout need not be in range. An out of range entry would be read past the end
// of rcInput[] in the Rx layer.
TEST_F(ControlMapTest, OutOfRangeChannelIsReplaced)
{
    uint8_t map[RC_CONTROL_COUNT] = { 0, 1, 3, 4, 2, 5 };
    map[RC_ARM] = MAX_SUPPORTED_RC_CHANNEL_COUNT;
    setControlMap(map);

    validateAndFixConfig();

    expectUsableControlMap();
    // The five functions ahead of it keep what they were given
    EXPECT_EQ(0, rxConfig()->rc_channel[RC_ROLL]);
    EXPECT_EQ(1, rxConfig()->rc_channel[RC_PITCH]);
    EXPECT_EQ(3, rxConfig()->rc_channel[RC_YAW]);
    EXPECT_EQ(4, rxConfig()->rc_channel[RC_COLLECTIVE]);
    EXPECT_EQ(2, rxConfig()->rc_channel[RC_THROTTLE]);
}

// Two functions on one channel would leave one of them flying on the other's
// Rx calibration and failsafe treatment, and both would move with the one stick.
TEST_F(ControlMapTest, DuplicateChannelIsReplaced)
{
    uint8_t map[RC_CONTROL_COUNT] = { 0, 1, 3, 4, 2, 5 };
    map[RC_ARM] = map[RC_THROTTLE];
    setControlMap(map);

    validateAndFixConfig();

    expectUsableControlMap();
    // The earlier function of the pair keeps the channel
    EXPECT_EQ(2, rxConfig()->rc_channel[RC_THROTTLE]);
    EXPECT_NE(2, rxConfig()->rc_channel[RC_ARM]);
}

// A duplicate on a stick is the dangerous one: arm on the collective channel
// would arm the aircraft on collective travel.
TEST_F(ControlMapTest, ArmIsNotLeftOnAStickChannel)
{
    uint8_t map[RC_CONTROL_COUNT] = { 0, 1, 3, 4, 2, 5 };
    map[RC_ARM] = map[RC_COLLECTIVE];
    setControlMap(map);

    validateAndFixConfig();

    expectUsableControlMap();
    EXPECT_EQ(4, rxConfig()->rc_channel[RC_COLLECTIVE]);
    EXPECT_NE(rxConfig()->rc_channel[RC_COLLECTIVE], rxConfig()->rc_channel[RC_ARM]);
}

// The worst a stored record can hand over: every function on one channel.
TEST_F(ControlMapTest, EveryFunctionOnOneChannelIsSpreadOut)
{
    const uint8_t map[RC_CONTROL_COUNT] = { 0, 0, 0, 0, 0, 0 };
    setControlMap(map);

    validateAndFixConfig();

    expectUsableControlMap();
    const uint8_t expected[RC_CONTROL_COUNT] = { 0, 1, 2, 3, 4, 5 };
    expectControlMap(expected);
}

// The repair for one entry must not create a duplicate of its own: RC_ROLL
// lands on the lowest free channel, which RC_ARM is holding.
TEST_F(ControlMapTest, RepairDoesNotCreateAnotherDuplicate)
{
    const uint8_t map[RC_CONTROL_COUNT] = { 200, 1, 3, 4, 2, 0 };
    setControlMap(map);

    validateAndFixConfig();

    expectUsableControlMap();
    EXPECT_EQ(0, rxConfig()->rc_channel[RC_ROLL]);
    EXPECT_NE(0, rxConfig()->rc_channel[RC_ARM]);
}

// Duplicates and out of range entries in the one map.
TEST_F(ControlMapTest, DuplicatesAndOutOfRangeTogether)
{
    const uint8_t map[RC_CONTROL_COUNT] = { 5, 5, 200, 5, 200, 5 };
    setControlMap(map);

    validateAndFixConfig();

    expectUsableControlMap();
    EXPECT_EQ(5, rxConfig()->rc_channel[RC_ROLL]);
}

// Validation runs on every load and every save, so it has to be idempotent -
// a second pass must not move a map the first pass has already repaired.
TEST_F(ControlMapTest, RepairIsIdempotent)
{
    const uint8_t map[RC_CONTROL_COUNT] = { 9, 9, 200, 9, 255, 9 };
    setControlMap(map);

    validateAndFixConfig();

    uint8_t repaired[RC_CONTROL_COUNT];
    for (int function = 0; function < RC_CONTROL_COUNT; function++) {
        repaired[function] = rxConfig()->rc_channel[function];
    }

    validateAndFixConfig();

    expectControlMap(repaired);
    expectUsableControlMap();
}

// Whatever a stored record or an MSP write holds, validation must produce a
// map the Rx layer can use - it must never leave a duplicate or an out of
// range entry behind, and must never run off the end looking for a free
// channel.
TEST_F(ControlMapTest, AnyMapIsMadeUsable)
{
    // A deterministic sweep of the awkward values: in range, at the bound,
    // and well past it
    static const uint8_t values[] = { 0, 1, 2, 5, 17, 18, 100, 255 };
    const int valueCount = ARRAYLEN(values);

    for (int i = 0; i < valueCount; i++) {
        for (int j = 0; j < valueCount; j++) {
            for (int k = 0; k < valueCount; k++) {
                const uint8_t map[RC_CONTROL_COUNT] = {
                    values[i], values[j], values[k],
                    values[k], values[j], values[i],
                };
                SCOPED_TRACE(::testing::Message()
                    << "map { " << (int)map[0] << ", " << (int)map[1] << ", "
                    << (int)map[2] << ", " << (int)map[3] << ", "
                    << (int)map[4] << ", " << (int)map[5] << " }");
                setControlMap(map);

                validateAndFixConfig();

                expectUsableControlMap();
            }
        }
    }
}

// STUBS

extern "C" {

bool isSerialConfigValid(serialConfig_t *) { return true; }
const serialPortConfig_t *findSerialPortConfig(serialPortFunction_e) { return NULL; }
void serialRemovePort(serialPortIdentifier_e) { }

void buildAlignmentFromStandardAlignment(alignment_t *, sensor_align_e) { }
void validateAndFixServoConfig(void) { }
void validateAndFixMixerConfig(void) { }

bool isModeActivationConditionLinked(boxId_e) { return false; }
void removeModeActivationCondition(boxId_e) { }
void initActiveBoxIds(void) { }

bool loadEEPROM(void) { return true; }
void writeConfigToEEPROM(void) { }
bool isEEPROMStructureValid(void) { return true; }

void loadRateProfile(void) { }
void loadBatteryProfile(void) { }
void initRcProcessing(void) { }
void adjustmentRangeInit(void) { }
void rcModeInit(void) { }
void stickCommandsInit(void) { }
void pidInit(const pidProfile_t *) { }
void failsafeReset(void) { }
void setAccelerationTrims(flightDynamicsTrims_t *) { }
void accInitFilters(void) { }
void imuConfigure(void) { }
void reevaluateLedConfig(void) { }

void suspendRxSignal(void) { }
void resumeRxSignal(void) { }

void beeperConfirmationBeeps(uint8_t) { }
void schedulerIgnoreTaskExecTime(void) { }
void setArmingDisabled(armingDisableFlags_e) { }

}
