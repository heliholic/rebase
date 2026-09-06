/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <limits.h>

extern "C" {
    #include <platform.h>
    #include "build/debug.h"
    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/motor.h"

    #include "flight/mixer.h"

//PG_DECLARE(motorConfig_t, motorConfig);

PG_REGISTER_WITH_RESET_TEMPLATE(motorConfig_t, motorConfig, PG_MOTOR_CONFIG);

PG_RESET_TEMPLATE(motorConfig_t, motorConfig)
{
    .dev = {
        .motorPwmRate = 400,
        .motorProtocol = 0,
        .motorInversion = 0,
        .useContinuousUpdate = 0,
        .useBurstDshot = DSHOT_DMAR_OFF,
        .useDshotTelemetry = DSHOT_TELEMETRY_OFF,
        .useDshotEdt = DSHOT_EDT_OFF,
        .ioTags = {IO_TAG_NONE, IO_TAG_NONE, IO_TAG_NONE, IO_TAG_NONE},
        .motorTransportProtocol = 0,
        .useDshotBitbang = DSHOT_BITBANG_OFF,
        .useDshotBitbangedTimer = DSHOT_BITBANGED_TIMER_AUTO,
    },
    .maxthrottle = 1850,
    .mincommand = 1000,
    .kv = 1000,
    .motorPoleCount = 14,
};

// A scalar PG with no reset at all: pgResetInstance() must still zero it.
typedef struct pgTestZeroed_s {
    uint32_t a;
    uint16_t b;
    uint8_t  c;
} pgTestZeroed_t;

PG_DECLARE(pgTestZeroed_t, pgTestZeroed);
PG_REGISTER(pgTestZeroed_t, pgTestZeroed, PG_RESERVED_FOR_TESTING_1);

// An array PG reset by function, so every element can be given a distinct
// value and a truncated reset is visible.
#define PG_TEST_ARRAY_LEN 8

typedef struct pgTestElem_s {
    uint16_t index;
    uint16_t value;
} pgTestElem_t;

PG_DECLARE_ARRAY(pgTestElem_t, PG_TEST_ARRAY_LEN, pgTestArray);
PG_REGISTER_ARRAY_WITH_RESET_FN(pgTestElem_t, PG_TEST_ARRAY_LEN, pgTestArray,
                                PG_RESERVED_FOR_TESTING_2);

extern "C" PG_RESET_FN(pgTestElem_t, pgTestArray)
{
    for (int i = 0; i < PG_TEST_ARRAY_LEN; i++) {
        pgTestArray[i].index = i;
        pgTestArray[i].value = 100 + i;
    }
}
}


#include "unittest_macros.h"
#include "gtest/gtest.h"

TEST(ParameterGroupsfTest, Test_pgResetAll)
{
    memset(motorConfigMutable(), 0, sizeof(motorConfig_t));
    pgResetAll();
    EXPECT_EQ(1850, motorConfig()->maxthrottle);
    EXPECT_EQ(1000, motorConfig()->mincommand);
    EXPECT_EQ(400, motorConfig()->dev.motorPwmRate);
}

TEST(ParameterGroupsfTest, Test_pgFind)
{
    memset(motorConfigMutable(), 0, sizeof(motorConfig_t));
    const pgRegistry_t *pgRegistry = pgFind(PG_MOTOR_CONFIG);
    pgReset(pgRegistry);
    EXPECT_EQ(1850, motorConfig()->maxthrottle);
    EXPECT_EQ(1000, motorConfig()->mincommand);
    EXPECT_EQ(400, motorConfig()->dev.motorPwmRate);

    motorConfig_t motorConfig2;
    memset(&motorConfig2, 0, sizeof(motorConfig_t));
    motorConfigMutable()->dev.motorPwmRate = 500;
    pgStore(pgRegistry, &motorConfig2, sizeof(motorConfig_t));
    EXPECT_EQ(1850, motorConfig2.maxthrottle);
    EXPECT_EQ(1000, motorConfig2.mincommand);
    EXPECT_EQ(500, motorConfig2.dev.motorPwmRate);

    motorConfig_t motorConfig3;
    memset(&motorConfig3, 0, sizeof(motorConfig_t));
    pgResetCopy(&motorConfig3, PG_MOTOR_CONFIG);
    EXPECT_EQ(1850, motorConfig3.maxthrottle);
    EXPECT_EQ(1000, motorConfig3.mincommand);
    EXPECT_EQ(400, motorConfig3.dev.motorPwmRate);
}

// An array PG must register the size of the whole array, not of one element.
// Getting this wrong truncates every array group to its first element in
// pgReset(), loadEEPROM() and writeSettingsToEEPROM().
TEST(ParameterGroupsfTest, Test_pgRegistryArrayGeometry)
{
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_2);
    ASSERT_NE(nullptr, reg);

    EXPECT_EQ(PG_TEST_ARRAY_LEN, reg->length);
    EXPECT_EQ(sizeof(pgTestElem_t) * PG_TEST_ARRAY_LEN, pgSize(reg));
    EXPECT_EQ(sizeof(pgTestElem_t), pgElementSize(reg));

    // Every registered PG must agree with itself, not just this one.
    PG_FOREACH(r) {
        ASSERT_GT(r->length, 0);
        EXPECT_EQ(0, pgSize(r) % r->length)
            << "PG " << pgN(r) << " size " << pgSize(r)
            << " is not a multiple of length " << (int)r->length;
        EXPECT_EQ(pgSize(r), pgElementSize(r) * r->length)
            << "PG " << pgN(r) << " element size does not span the group";
    }
}

// The registry is not an array the compiler laid out; it is whatever the
// linker concatenated into .pg_registry, walked by PG_FOREACH as if it were
// one. That only holds while every object file's contribution abuts the last
// and lands on the struct's own alignment, and neither is visible in the C.
// The pg.c static asserts cover the struct side; these cover the linked image.
TEST(ParameterGroupsfTest, Test_pgRegistryIsAContiguousArray)
{
    const ptrdiff_t span = (const char *)__pg_registry_end - (const char *)__pg_registry_start;

    ASSERT_GT(span, 0) << "no parameter groups linked into .pg_registry";

    // Padding between the object files that contribute entries would leave a
    // remainder here, and PG_FOREACH would then walk into a straddled entry.
    EXPECT_EQ(0, span % (ptrdiff_t)sizeof(pgRegistry_t))
        << "span " << span << " is not a whole number of "
        << sizeof(pgRegistry_t) << "-byte entries";

    EXPECT_EQ(span / (ptrdiff_t)sizeof(pgRegistry_t), PG_REGISTRY_SIZE);

    PG_FOREACH(r) {
        // A literal alignment below alignof() is honoured as a lowering by
        // some compilers, and a linker script can place the section below it
        // too; either leaves the pointer members on a boundary the compiler
        // has already assumed away.
        EXPECT_EQ(0u, (uintptr_t)r % alignof(pgRegistry_t))
            << "PG " << pgN(r) << " at " << (const void *)r
            << " is not " << alignof(pgRegistry_t) << "-byte aligned";

        // An entry straddling padding reads as a group that never registered.
        EXPECT_NE(nullptr, pgData(r)) << "PG " << pgN(r) << " has no data";
        EXPECT_NE(nullptr, pgCopy(r)) << "PG " << pgN(r) << " has no copy";
        EXPECT_NE(nullptr, pgChecksum(r)) << "PG " << pgN(r) << " has no checksum";
        EXPECT_GT(pgSize(r), 0u) << "PG " << pgN(r) << " is empty";

        // The walk and the lookup must see the same table.
        EXPECT_EQ(r, pgFind(pgN(r))) << "PG " << pgN(r) << " is not findable";
    }
}

// pgResetInstance() tells a reset template from a reset function by the
// address alone, so a group whose reset pointer escaped both sections is
// silently zeroed instead of taking its defaults. Nothing in the C says the
// attributes placed it there.
TEST(ParameterGroupsfTest, Test_pgResetPointersLieInTheirSections)
{
    PG_FOREACH(r) {
        if (r->reset.data == nullptr) {
            continue;
        }
        const uint8_t *p = (const uint8_t *)r->reset.data;
        const bool isTemplate = (p >= __pg_resetdata_start && p < __pg_resetdata_end);
        const bool isFunction = (p >= __pg_resetfunc_start && p < __pg_resetfunc_end);

        EXPECT_TRUE(isTemplate || isFunction)
            << "PG " << pgN(r) << " reset pointer " << (const void *)p
            << " is in neither .pg_resetdata nor .pg_resetfunc, so its"
               " defaults would be dropped";
        EXPECT_FALSE(isTemplate && isFunction)
            << "PG " << pgN(r) << ": .pg_resetdata and .pg_resetfunc overlap";
    }
}

// The reset must reach the last element, not just the first.
TEST(ParameterGroupsfTest, Test_pgResetArray)
{
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_2);
    ASSERT_NE(nullptr, reg);

    memset(pgTestArray_array(), 0xA5, sizeof(*pgTestArray_array()));
    pgReset(reg);

    for (int i = 0; i < PG_TEST_ARRAY_LEN; i++) {
        EXPECT_EQ(i, pgTestArray(i)->index) << "element " << i;
        EXPECT_EQ(100 + i, pgTestArray(i)->value) << "element " << i;
    }
}

// A PG with no reset template and no reset function is zeroed.
TEST(ParameterGroupsfTest, Test_pgResetNoResetIsZeroed)
{
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_1);
    ASSERT_NE(nullptr, reg);

    memset(pgTestZeroedMutable(), 0x5A, sizeof(pgTestZeroed_t));
    pgReset(reg);

    EXPECT_EQ(0u, pgTestZeroed()->a);
    EXPECT_EQ(0u, pgTestZeroed()->b);
    EXPECT_EQ(0u, pgTestZeroed()->c);
}

TEST(ParameterGroupsfTest, Test_pgFindUnknownPgn)
{
    EXPECT_EQ(nullptr, pgFind(PG_RESERVED_FOR_TESTING_3));
    EXPECT_FALSE(pgResetCopy(nullptr, PG_RESERVED_FOR_TESTING_3));
}

// pgLoad() restores stored bytes only when the layout hash matches; on a
// mismatch the group keeps its defaults rather than adopting a stale layout.
TEST(ParameterGroupsfTest, Test_pgLoadHashMatch)
{
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_2);
    ASSERT_NE(nullptr, reg);

    pgTestElem_t stored[PG_TEST_ARRAY_LEN];
    for (int i = 0; i < PG_TEST_ARRAY_LEN; i++) {
        stored[i].index = 900 + i;
        stored[i].value = 800 + i;
    }

    EXPECT_TRUE(pgLoad(reg, stored, sizeof(stored), pgHash(reg)));
    for (int i = 0; i < PG_TEST_ARRAY_LEN; i++) {
        EXPECT_EQ(900 + i, pgTestArray(i)->index) << "element " << i;
        EXPECT_EQ(800 + i, pgTestArray(i)->value) << "element " << i;
    }
}

TEST(ParameterGroupsfTest, Test_pgLoadHashMismatchKeepsDefaults)
{
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_2);
    ASSERT_NE(nullptr, reg);

    pgTestElem_t stored[PG_TEST_ARRAY_LEN];
    for (int i = 0; i < PG_TEST_ARRAY_LEN; i++) {
        stored[i].index = 900 + i;
        stored[i].value = 800 + i;
    }

    EXPECT_FALSE(pgLoad(reg, stored, sizeof(stored), pgHash(reg) ^ 1u));
    for (int i = 0; i < PG_TEST_ARRAY_LEN; i++) {
        EXPECT_EQ(i, pgTestArray(i)->index) << "element " << i;
        EXPECT_EQ(100 + i, pgTestArray(i)->value) << "element " << i;
    }
}

// A record shorter than the group (an older, smaller layout) fills what it can
// and leaves the remaining elements at their defaults.
TEST(ParameterGroupsfTest, Test_pgLoadShortRecord)
{
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_2);
    ASSERT_NE(nullptr, reg);

    pgTestElem_t stored[2];
    for (int i = 0; i < 2; i++) {
        stored[i].index = 900 + i;
        stored[i].value = 800 + i;
    }

    // Dirty a trailing element so the defaults below are known to come from
    // the reset inside pgLoad(), not from whatever a previous test left.
    pgTestArrayMutable(PG_TEST_ARRAY_LEN - 1)->value = 0xFFFF;

    EXPECT_TRUE(pgLoad(reg, stored, sizeof(stored), pgHash(reg)));

    for (int i = 0; i < 2; i++) {
        EXPECT_EQ(900 + i, pgTestArray(i)->index) << "element " << i;
        EXPECT_EQ(800 + i, pgTestArray(i)->value) << "element " << i;
    }
    for (int i = 2; i < PG_TEST_ARRAY_LEN; i++) {
        EXPECT_EQ(i, pgTestArray(i)->index) << "element " << i;
        EXPECT_EQ(100 + i, pgTestArray(i)->value) << "element " << i;
    }
}

// pgStore() must copy the whole array and never write past the destination.
TEST(ParameterGroupsfTest, Test_pgStoreArray)
{
    const pgRegistry_t *reg = pgFind(PG_RESERVED_FOR_TESTING_2);
    ASSERT_NE(nullptr, reg);
    pgReset(reg);

    pgTestElem_t dest[PG_TEST_ARRAY_LEN];
    memset(dest, 0, sizeof(dest));
    EXPECT_EQ((int)sizeof(dest), pgStore(reg, dest, sizeof(dest)));
    for (int i = 0; i < PG_TEST_ARRAY_LEN; i++) {
        EXPECT_EQ(i, dest[i].index) << "element " << i;
        EXPECT_EQ(100 + i, dest[i].value) << "element " << i;
    }

    // A destination smaller than the group is clamped, not overrun.
    struct {
        pgTestElem_t head[2];
        uint32_t guard;
    } small;
    memset(&small, 0, sizeof(small));
    small.guard = 0xDEADBEEF;

    EXPECT_EQ((int)sizeof(small.head), pgStore(reg, small.head, sizeof(small.head)));
    EXPECT_EQ(0xDEADBEEFu, small.guard);
    EXPECT_EQ(0, small.head[0].index);
    EXPECT_EQ(101, small.head[1].value);
}

// STUBS

extern "C" {
}
