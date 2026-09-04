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

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

extern "C" {
    #include "platform.h"

    #include "common/crc.h"

    #include "drivers/system.h"

    #include "config/config_eeprom.h"
    #include "config/config_streamer.h"

    #include "pg/pg.h"
    #include "pg/pg_ids.h"

    // Two groups to store: a scalar with a reset template, and an array with
    // a reset function, so a short record and a multi-element record are both
    // exercised by a real write/read cycle.
    typedef struct eeTestScalar_s {
        uint32_t a;
        uint16_t b;
        uint8_t  c;
    } eeTestScalar_t;

    PG_DECLARE(eeTestScalar_t, eeTestScalar);
    PG_REGISTER_WITH_RESET_TEMPLATE(eeTestScalar_t, eeTestScalar, PG_RESERVED_FOR_TESTING_1);
    PG_RESET_TEMPLATE(eeTestScalar_t, eeTestScalar)
    {
        .a = 0xA5A5A5A5,
        .b = 0x1234,
        .c = 0x5A,
    };

    #define EE_TEST_ARRAY_LEN 4

    typedef struct eeTestElem_s {
        uint16_t index;
        uint16_t value;
    } eeTestElem_t;

    PG_DECLARE_ARRAY(eeTestElem_t, EE_TEST_ARRAY_LEN, eeTestArray);
    PG_REGISTER_ARRAY_WITH_RESET_FN(eeTestElem_t, EE_TEST_ARRAY_LEN, eeTestArray,
                                    PG_RESERVED_FOR_TESTING_2);

    PG_RESET_FN(eeTestElem_t, eeTestArray)
    {
        for (int i = 0; i < EE_TEST_ARRAY_LEN; i++) {
            eeTestArray[i].index = i;
            eeTestArray[i].value = 100 + i;
        }
    }
}

// The store is plain RAM under CONFIG_IN_RAM, so a round trip here is the
// same code path the FC runs, minus the flash driver.
extern "C" uint8_t eepromData[EEPROM_SIZE];

class ConfigEepromTest : public ::testing::Test
{
protected:
    void SetUp() override {
        memset(eepromData, 0, sizeof(eepromData));
        pgResetAll();
    }
};

static void dirtyAllGroups(void)
{
    eeTestScalarMutable()->a = 0xDEADBEEF;
    eeTestScalarMutable()->b = 0xBEEF;
    eeTestScalarMutable()->c = 0x42;
    for (int i = 0; i < EE_TEST_ARRAY_LEN; i++) {
        eeTestArrayMutable(i)->index = 900 + i;
        eeTestArrayMutable(i)->value = 800 + i;
    }
}

// A freshly written store must satisfy both checks. This is the pair that
// silently disagreed when the record header was reordered: the writer was
// correct and the scanner walked straight past the terminator.
TEST_F(ConfigEepromTest, WrittenConfigIsValid)
{
    dirtyAllGroups();
    writeConfigToEEPROM();

    EXPECT_TRUE(isEEPROMVersionValid());
    EXPECT_TRUE(isEEPROMStructureValid());
}

// Values must survive the write/read cycle, not merely leave a valid image.
TEST_F(ConfigEepromTest, RoundTripPreservesValues)
{
    dirtyAllGroups();
    writeConfigToEEPROM();

    // Scribble over RAM so anything read back has to come from the store.
    memset(eeTestScalarMutable(), 0, sizeof(eeTestScalar_t));
    memset(eeTestArray_array(), 0, sizeof(*eeTestArray_array()));

    ASSERT_TRUE(isEEPROMStructureValid());
    EXPECT_TRUE(loadEEPROM());

    EXPECT_EQ(0xDEADBEEF, eeTestScalar()->a);
    EXPECT_EQ(0xBEEF, eeTestScalar()->b);
    EXPECT_EQ(0x42, eeTestScalar()->c);
    for (int i = 0; i < EE_TEST_ARRAY_LEN; i++) {
        EXPECT_EQ(900 + i, eeTestArray(i)->index) << "element " << i;
        EXPECT_EQ(800 + i, eeTestArray(i)->value) << "element " << i;
    }
}

// getEEPROMConfigSize() is derived by the same scan, so it is a cheap probe
// that the scan stopped exactly on the footer rather than running past it.
TEST_F(ConfigEepromTest, ReportedSizeMatchesWhatWasWritten)
{
    writeConfigToEEPROM();
    ASSERT_TRUE(isEEPROMStructureValid());

    size_t expected = 8;                      // configHeader_t
    PG_FOREACH(reg) {
        expected += 6 + pgSize(reg);          // configRecord_t + payload
    }
    expected += 4;                            // configFooter_t
    expected += 2;                            // stored CRC

    EXPECT_EQ(expected, getEEPROMConfigSize());
    EXPECT_LT(getEEPROMConfigSize(), getEEPROMStorageSize());
}

// An empty store must be rejected rather than parsed as zero records.
TEST_F(ConfigEepromTest, ErasedStoreIsInvalid)
{
    memset(eepromData, 0, sizeof(eepromData));
    EXPECT_FALSE(isEEPROMStructureValid());

    memset(eepromData, 0xFF, sizeof(eepromData));
    EXPECT_FALSE(isEEPROMStructureValid());
}

// Any single flipped bit in the payload must fail the CRC.
TEST_F(ConfigEepromTest, CorruptedPayloadIsRejected)
{
    dirtyAllGroups();
    writeConfigToEEPROM();
    ASSERT_TRUE(isEEPROMStructureValid());

    eepromData[16] ^= 0x01;
    EXPECT_FALSE(isEEPROMStructureValid());
}

// A record whose layout hash no longer matches is discarded and the group
// falls back to its defaults, rather than adopting the stale bytes.
TEST_F(ConfigEepromTest, StaleLayoutHashFallsBackToDefaults)
{
    dirtyAllGroups();
    writeConfigToEEPROM();
    ASSERT_TRUE(isEEPROMStructureValid());

    // Rewrite the scalar group's stored hash, then repair the image CRC by
    // writing it out again is not possible - so drive loadEEPROM() directly
    // and only assert the group that lost its record.
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_1);
    ASSERT_NE(nullptr, reg);

    uint32_t stored = pgHash(reg);
    uint8_t *found = nullptr;
    for (size_t i = 8; i + 4 < sizeof(eepromData); i++) {
        if (memcmp(&eepromData[i], &stored, sizeof(stored)) == 0) {
            found = &eepromData[i];
            break;
        }
    }
    ASSERT_NE(nullptr, found) << "scalar group's record not found in the store";

    uint32_t bogus = stored ^ 0xFFFFFFFFu;
    memcpy(found, &bogus, sizeof(bogus));

    // The record no longer matches any group, so loadEEPROM() reports failure
    // and resets the group it could not find.
    EXPECT_FALSE(loadEEPROM());
    EXPECT_EQ(0xA5A5A5A5, eeTestScalar()->a);
    EXPECT_EQ(0x1234, eeTestScalar()->b);
    EXPECT_EQ(0x5A, eeTestScalar()->c);

    // The untouched group still loads its stored values.
    for (int i = 0; i < EE_TEST_ARRAY_LEN; i++) {
        EXPECT_EQ(900 + i, eeTestArray(i)->index) << "element " << i;
    }
}

// STUBS

extern "C" {
    void failureMode(failureMode_e mode) { FAIL() << "failureMode(" << mode << ") called"; }
}
