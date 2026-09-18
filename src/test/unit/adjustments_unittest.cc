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
#include <stdbool.h>
#include <string.h>

extern "C" {
    #include "platform.h"

    #include "blackbox/blackbox.h"
    #include "blackbox/blackbox_fielddefs.h"

    #include "common/axis.h"
    #include "common/maths.h"
    #include "common/utils.h"

    #include "config/config.h"

    #include "controls/adjustments.h"
    #include "controls/modes.h"
    #include "controls/rates.h"

    #include "drivers/time.h"

    #include "io/beeper.h"

    #include "pg/adjustments.h"
    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/rates.h"
    #include "pg/rx.h"
    #include "pg/system.h"

    #include "rx/rx.h"

    PG_REGISTER(systemConfig_t, systemConfig, PG_SYSTEM_CONFIG);
    PG_REGISTER(blackboxConfig_t, blackboxConfig, PG_BLACKBOX_CONFIG);
}

#include "unittest_macros.h"
#include "gtest/gtest.h"


//// Stubs

extern "C" {

float rcInput[MAX_SUPPORTED_RC_CHANNEL_COUNT];
float rcControls[RC_CONTROL_COUNT];

static timeMs_t  stubMillis;
static bool      stubRxReceiving;
static int       stubBeeps;
static int       stubConfigDirty;
static int       stubLogCount;
static int       stubLogFunc;
static int       stubLogValue;

uint32_t millis(void) { return stubMillis; }
uint32_t micros(void) { return stubMillis * 1000; }

bool isRxReceivingSignal(void) { return stubRxReceiving; }

void beeperConfirmationBeeps(uint8_t beepCount) { stubBeeps += beepCount; }

void setConfigDirty(void) { stubConfigDirty++; }

void blackboxLogEvent(FlightLogEvent event, flightLogEventData_t *data)
{
    if (event == FLIGHT_LOG_EVENT_INFLIGHT_ADJUSTMENT) {
        stubLogCount++;
        stubLogFunc  = data->inflightAdjustment.adjustmentFunction;
        stubLogValue = data->inflightAdjustment.newValue;
    }
}

uint8_t getCurrentRateProfileIndex(void)
{
    return systemConfig()->activeRateProfile;
}

void pinioBoxTaskControl(void) {}

/*
 * Test doubles for the adjustment functions whose owning module is not linked
 * into this test. They keep the one-based profile numbering of the real ones.
 */

static int stubPidProfile;
static int stubLedProfile;
static int stubBatteryProfile;
static int stubAccTrimPitch;
static int stubAccTrimRoll;

ADJUSTMENT_GET(PID_PROFILE)     { return stubPidProfile + 1; }
ADJUSTMENT_SET(PID_PROFILE)     { stubPidProfile = value - 1; }

ADJUSTMENT_GET(LED_PROFILE)     { return stubLedProfile + 1; }
ADJUSTMENT_SET(LED_PROFILE)     { stubLedProfile = value - 1; }

ADJUSTMENT_GET(BATTERY_PROFILE) { return stubBatteryProfile + 1; }
ADJUSTMENT_SET(BATTERY_PROFILE) { stubBatteryProfile = value - 1; }

ADJUSTMENT_GET(ACC_TRIM_PITCH)  { return stubAccTrimPitch; }
ADJUSTMENT_SET(ACC_TRIM_PITCH)  { stubAccTrimPitch = value; }

ADJUSTMENT_GET(ACC_TRIM_ROLL)   { return stubAccTrimRoll; }
ADJUSTMENT_SET(ACC_TRIM_ROLL)   { stubAccTrimRoll = value; }

}


//// Test fixture

// The enable switch and the adjustment switch, as raw input channels
#define ENA_CHANNEL  5
#define ADJ_CHANNEL  7

// Always-enabled marker for the enable channel
#define ENA_ALWAYS   0xff

class AdjustmentsTest : public ::testing::Test
{
protected:

    virtual void SetUp()
    {
        pgResetAll();
        loadRateProfile();

        blackboxConfigMutable()->device = (BlackboxDevice_e)1;

        for (int index = 0; index < MAX_SUPPORTED_RC_CHANNEL_COUNT; index++) {
            rcInput[index] = PWM_RANGE_MIDDLE;
        }

        // The reported adjustment is module state with no reset of its own.
        // One quiet pass, far enough in the future, expires whatever the
        // previous test left behind.
        stubMillis = 1000000;
        stubRxReceiving = true;
        processRcAdjustments();

        stubMillis = 0;
        stubBeeps = 0;
        stubConfigDirty = 0;
        stubLogCount = 0;
        stubLogFunc = 0;
        stubLogValue = 0;

        stubPidProfile = 0;
        stubLedProfile = 0;
        stubBatteryProfile = 0;
        stubAccTrimPitch = 0;
        stubAccTrimRoll = 0;

        adjustmentRangeInit();
    }

    // A stepped range: 900..1300 decrements, 1700..2100 increments
    adjustmentRange_t *steppedRange(int index, adjustmentFunc_e func, int step, int min, int max)
    {
        adjustmentRange_t *range = adjustmentRangesMutable(index);

        memset(range, 0, sizeof(*range));

        range->function = func;
        range->enaChannel = ENA_ALWAYS;
        range->adjChannel = ADJ_CHANNEL;
        range->adjRange1.startStep = CHANNEL_VALUE_TO_STEP(900);
        range->adjRange1.endStep   = CHANNEL_VALUE_TO_STEP(1300);
        range->adjRange2.startStep = CHANNEL_VALUE_TO_STEP(1700);
        range->adjRange2.endStep   = CHANNEL_VALUE_TO_STEP(2100);
        range->adjMin = min;
        range->adjMax = max;
        range->adjStep = step;

        return range;
    }

    // A continuous range: 1000..2000 maps onto min..max
    adjustmentRange_t *continuousRange(int index, adjustmentFunc_e func, int min, int max)
    {
        adjustmentRange_t *range = adjustmentRangesMutable(index);

        memset(range, 0, sizeof(*range));

        range->function = func;
        range->enaChannel = ENA_ALWAYS;
        range->adjChannel = ADJ_CHANNEL;
        range->adjRange1.startStep = CHANNEL_VALUE_TO_STEP(1000);
        range->adjRange1.endStep   = CHANNEL_VALUE_TO_STEP(2000);
        range->adjMin = min;
        range->adjMax = max;
        range->adjStep = 0;

        return range;
    }

    void runAt(timeMs_t time)
    {
        stubMillis = time;
        processRcAdjustments();
    }

    // Move the adjustment switch and let the trigger delay expire
    void settleAt(timeMs_t time, int channelValue)
    {
        rcInput[ADJ_CHANNEL] = channelValue;
        runAt(time);
        runAt(time + TRIGGER_DELAY);
    }

    // Times are relative to the fixture, so name the engine's constants here
    static const timeMs_t TRIGGER_DELAY = 100;
    static const timeMs_t REPEAT_DELAY = 200;
    static const timeMs_t LATENCY_MS = 3000;
};


//// Tests

TEST_F(AdjustmentsTest, noAdjustmentWithoutRxSignal)
{
    // given
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    adjustmentRangeInit();

    // and
    stubRxReceiving = false;

    // when
    settleAt(1000, 2000);
    runAt(1000 + TRIGGER_DELAY);

    // then
    EXPECT_EQ(250, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(0, stubBeeps);
    EXPECT_EQ(0, stubConfigDirty);
}

TEST_F(AdjustmentsTest, steppedIncrementWaitsForTheStickToSettle)
{
    // given
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    adjustmentRangeInit();

    // when: the switch is thrown to the increment range
    rcInput[ADJ_CHANNEL] = 2000;
    runAt(1000);

    // then: the move itself only starts the trigger delay
    EXPECT_EQ(250, currentRateProfile->rates[FD_PITCH]);

    // when: the delay has not quite expired
    runAt(1000 + TRIGGER_DELAY - 1);

    // then
    EXPECT_EQ(250, currentRateProfile->rates[FD_PITCH]);

    // when: it has
    runAt(1000 + TRIGGER_DELAY);

    // then
    EXPECT_EQ(260, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(1, stubBeeps);
    EXPECT_EQ(1, stubConfigDirty);
}

TEST_F(AdjustmentsTest, steppedIncrementRepeatsAtTheRepeatDelay)
{
    // given
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then
    EXPECT_EQ(260, currentRateProfile->rates[FD_PITCH]);

    // when: the switch is held, but the repeat delay has not expired
    runAt(1100 + REPEAT_DELAY - 1);

    // then
    EXPECT_EQ(260, currentRateProfile->rates[FD_PITCH]);

    // when: it has
    runAt(1100 + REPEAT_DELAY);

    // then
    EXPECT_EQ(270, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(2, stubBeeps);
}

TEST_F(AdjustmentsTest, steppedDecrementUsesTheFirstRange)
{
    // given
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    adjustmentRangeInit();

    // when
    settleAt(1000, 1000);

    // then
    EXPECT_EQ(240, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(1, stubBeeps);
}

TEST_F(AdjustmentsTest, steppedCentreDoesNothing)
{
    // given
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    adjustmentRangeInit();

    // when: the switch sits between the two ranges
    settleAt(1000, 1500);
    runAt(1000 + TRIGGER_DELAY + REPEAT_DELAY);

    // then
    EXPECT_EQ(250, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(0, stubBeeps);
    EXPECT_EQ(0, stubConfigDirty);
}

TEST_F(AdjustmentsTest, steppedStopsAtTheConfiguredLimit)
{
    // given: a range that allows two steps up
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, 270);
    adjustmentRangeInit();

    // when: the switch is held for four repeats
    settleAt(1000, 2000);
    runAt(1100 + 1 * REPEAT_DELAY);
    runAt(1100 + 2 * REPEAT_DELAY);
    runAt(1100 + 3 * REPEAT_DELAY);

    // then: only the two steps within the limit are taken
    EXPECT_EQ(270, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(2, stubBeeps);
}

TEST_F(AdjustmentsTest, steppedStopsAtTheParameterLimit)
{
    // given: a range asking for more than the parameter allows
    steppedRange(0, ADJUSTMENT_PITCH_EXPO, 40, 0, 500);
    adjustmentRangeInit();

    // when: the switch is held for three repeats
    settleAt(1000, 2000);
    runAt(1100 + 1 * REPEAT_DELAY);
    runAt(1100 + 2 * REPEAT_DELAY);

    // then: EXPO_MAX caps it, whatever the range says
    EXPECT_EQ(EXPO_MAX, currentRateProfile->expo[FD_PITCH]);
}

TEST_F(AdjustmentsTest, enableChannelGatesTheAdjustment)
{
    // given
    adjustmentRange_t *range = steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    range->enaChannel = ENA_CHANNEL;
    range->enaRange.startStep = CHANNEL_VALUE_TO_STEP(1700);
    range->enaRange.endStep   = CHANNEL_VALUE_TO_STEP(2100);
    adjustmentRangeInit();

    // when: the enable switch is off
    rcInput[ENA_CHANNEL] = 1500;
    settleAt(1000, 2000);

    // then
    EXPECT_EQ(250, currentRateProfile->rates[FD_PITCH]);

    // when: the enable switch comes on
    rcInput[ENA_CHANNEL] = 2000;
    runAt(2000);

    // then: the trigger delay runs from here, not from when the switch moved
    EXPECT_EQ(250, currentRateProfile->rates[FD_PITCH]);

    // when: the delay expires
    runAt(2000 + TRIGGER_DELAY);

    // then
    EXPECT_EQ(260, currentRateProfile->rates[FD_PITCH]);
}

TEST_F(AdjustmentsTest, continuousMapsTheChannelOntoTheValueRange)
{
    // given
    continuousRange(0, ADJUSTMENT_PITCH_RATE, 200, 400);
    adjustmentRangeInit();

    // when: the stick is at the bottom of the range
    rcInput[ADJ_CHANNEL] = 1000;
    runAt(1000);

    // then
    EXPECT_EQ(200, currentRateProfile->rates[FD_PITCH]);

    // when: at the middle
    rcInput[ADJ_CHANNEL] = 1500;
    runAt(1000 + REPEAT_DELAY);

    // then
    EXPECT_EQ(300, currentRateProfile->rates[FD_PITCH]);

    // when: at the top
    rcInput[ADJ_CHANNEL] = 2000;
    runAt(1000 + 2 * REPEAT_DELAY);

    // then
    EXPECT_EQ(400, currentRateProfile->rates[FD_PITCH]);
}

TEST_F(AdjustmentsTest, continuousIsUnaffectedByAChannelOutsideTheRange)
{
    // given
    continuousRange(0, ADJUSTMENT_PITCH_RATE, 200, 400);
    adjustmentRangeInit();

    // when: the stick is well below the range
    rcInput[ADJ_CHANNEL] = 900;
    runAt(1000);

    // then: the value is left alone rather than pinned to the bottom
    EXPECT_EQ(250, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(0, stubBeeps);
    EXPECT_EQ(0, stubConfigDirty);
}

TEST_F(AdjustmentsTest, continuousStopsAtTheParameterLimit)
{
    // given: a range asking for more than the parameter allows
    continuousRange(0, ADJUSTMENT_PITCH_EXPO, 0, 200);
    adjustmentRangeInit();

    // when
    rcInput[ADJ_CHANNEL] = 2000;
    runAt(1000);

    // then
    EXPECT_EQ(EXPO_MAX, currentRateProfile->expo[FD_PITCH]);
}

TEST_F(AdjustmentsTest, rangeLimitsApplyWithoutTheMapping)
{
    // given: adjMin equal to adjMax leaves nothing to map onto
    continuousRange(0, ADJUSTMENT_PITCH_RATE, 300, 300);
    adjustmentRangeInit();

    // when
    rcInput[ADJ_CHANNEL] = 2000;
    runAt(1000);

    // then: the mapping is skipped, but adjMin..adjMax still clamps the value
    EXPECT_EQ(300, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(1, stubConfigDirty);
}

TEST_F(AdjustmentsTest, rangeLimitsApplyOutsideTheMappedWindow)
{
    // given: a range whose limits sit below the current value
    continuousRange(0, ADJUSTMENT_PITCH_RATE, 100, 200);
    adjustmentRangeInit();

    // when: the switch is outside the mapped window, so nothing is mapped
    rcInput[ADJ_CHANNEL] = 900;
    runAt(1000);

    // then: the value is pulled into the range all the same
    EXPECT_EQ(200, currentRateProfile->rates[FD_PITCH]);
}

TEST_F(AdjustmentsTest, initPrimesTheStateFromTheCurrentValue)
{
    // given: the parameter already at the value the stick maps to
    continuousRange(0, ADJUSTMENT_PITCH_RATE, 200, 400);
    currentRateProfile->rates[FD_PITCH] = 300;
    rcInput[ADJ_CHANNEL] = 1500;
    adjustmentRangeInit();

    // when
    runAt(1000);

    // then: the switch position at boot is not reported as a change
    EXPECT_EQ(300, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(0, stubBeeps);
    EXPECT_EQ(0, stubConfigDirty);
    EXPECT_EQ(0, stubLogCount);
    EXPECT_EQ(NULL, getAdjustmentsRangeName());
}

TEST_F(AdjustmentsTest, unimplementedFunctionCodeIsInert)
{
    // given: a code with no entry in the table - 18 and 19 are a gap
    adjustmentRange_t *range = steppedRange(0, ADJUSTMENT_COLL_EXPO, 10, 0, 100);
    range->function = 18;
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then
    EXPECT_EQ(0, stubBeeps);
    EXPECT_EQ(0, stubConfigDirty);
}

TEST_F(AdjustmentsTest, outOfRangeFunctionCodeIsInert)
{
    // given
    adjustmentRange_t *range = steppedRange(0, ADJUSTMENT_COLL_EXPO, 10, 0, 100);
    range->function = 200;
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then
    EXPECT_EQ(0, stubBeeps);
    EXPECT_EQ(0, stubConfigDirty);
}

TEST_F(AdjustmentsTest, changeIsReportedForTelemetry)
{
    // given
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then
    EXPECT_STREQ("PITCH_RATE", getAdjustmentsRangeName());
    EXPECT_EQ(ADJUSTMENT_PITCH_RATE, getAdjustmentsRangeFunc());
    EXPECT_EQ(260, getAdjustmentsRangeValue());
}

TEST_F(AdjustmentsTest, reportExpiresAfterTheLatency)
{
    // given
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    adjustmentRangeInit();

    // and
    settleAt(1000, 2000);
    EXPECT_STREQ("PITCH_RATE", getAdjustmentsRangeName());

    // when: the switch is centred and the latency has not expired
    rcInput[ADJ_CHANNEL] = 1500;
    runAt(1100 + LATENCY_MS);

    // then
    EXPECT_STREQ("PITCH_RATE", getAdjustmentsRangeName());

    // when: it has
    runAt(1100 + LATENCY_MS + 1);

    // then
    EXPECT_EQ(NULL, getAdjustmentsRangeName());
}

TEST_F(AdjustmentsTest, profileChangeIsNotReported)
{
    // given
    steppedRange(0, ADJUSTMENT_RATE_PROFILE, 1, 1, RATE_PROFILE_COUNT);
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then: the profile changed, one-based on the wire
    EXPECT_EQ(1, systemConfig()->activeRateProfile);
    EXPECT_EQ(rateProfilesMutable(1), currentRateProfile);
    EXPECT_EQ(1, stubBeeps);

    // and: but it is not what telemetry reports
    EXPECT_EQ(NULL, getAdjustmentsRangeName());
}

TEST_F(AdjustmentsTest, pidProfileChangeDoesNotBeep)
{
    // given: the PID profile confirms itself by beeping the profile number
    steppedRange(0, ADJUSTMENT_PID_PROFILE, 1, 1, PID_PROFILE_COUNT);
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then
    EXPECT_EQ(1, stubPidProfile);
    EXPECT_EQ(0, stubBeeps);
    EXPECT_EQ(1, stubConfigDirty);
}

TEST_F(AdjustmentsTest, changeIsLoggedToBlackbox)
{
    // given
    steppedRange(0, ADJUSTMENT_ACC_TRIM_PITCH, 5, -300, 300);
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then
    EXPECT_EQ(5, stubAccTrimPitch);
    EXPECT_EQ(1, stubLogCount);
    EXPECT_EQ(ADJUSTMENT_ACC_TRIM_PITCH, stubLogFunc);
    EXPECT_EQ(5, stubLogValue);
}

TEST_F(AdjustmentsTest, eachRangeKeepsItsOwnState)
{
    // given: two ranges on the same switch, adjusting different parameters
    steppedRange(0, ADJUSTMENT_PITCH_RATE, 10, 0, RATE_MAX);
    steppedRange(1, ADJUSTMENT_ROLL_RATE, 20, 0, RATE_MAX);
    adjustmentRangeInit();

    // when
    settleAt(1000, 2000);

    // then
    EXPECT_EQ(260, currentRateProfile->rates[FD_PITCH]);
    EXPECT_EQ(270, currentRateProfile->rates[FD_ROLL]);
    EXPECT_EQ(2, stubBeeps);
}
