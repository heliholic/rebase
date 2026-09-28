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

#include <stdint.h>
#include <stdbool.h>

#include <limits.h>

extern "C" {
    #include "platform.h"

    #include "controls/modes.h"

    #include "pg/rx.h"
    #include "build/debug.h"
    #include "drivers/io.h"
    #include "fc/rc_controls.h"
    #include "fc/runtime_config.h"
    #include "flight/failsafe.h"
    #include "rx/rx.h"
    #include "common/maths.h"
    #include "common/utils.h"
    #include "fc/feature.h"
    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "io/beeper.h"

    extern boxBitmap_t rcModeActivationMap;
    int32_t debug[DEBUG_VALUE_COUNT];
    uint8_t debugMode = 0;
    uint8_t armingFlags = 0;

    bool isPulseValid(uint16_t pulseDuration);

    PG_RESET_TEMPLATE(featureConfig_t, featureConfig)
    {
        .enabledFeatures = 0
    };

    PG_REGISTER(failsafeConfig_t, failsafeConfig, PG_FAILSAFE_CONFIG);
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

#define DE_ACTIVATE_ALL_BOXES   0

typedef struct testData_s {
    bool isPPMDataBeingReceived;
} testData_t;

static testData_t testData;

#if 0 //!! valid pulse handling has changed so these test now test removed functions
TEST(RxTest, TestValidFlightChannels)
{
    // given
    memset(&testData, 0, sizeof(testData));
    memset(&rcModeActivationMap, 0, sizeof(rcModeActivationMap)); // BOXFAILSAFE must be OFF

    // and
    rxConfigMutable()->rx_pulse_min = 1000;
    rxConfigMutable()->rx_pulse_max = 2000;

    // and
    armingConfigMutable()->arm_range.startStep = MIN_MODE_RANGE_STEP;
    armingConfigMutable()->arm_range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // when
    rxInit();

    // then (ARM channel should be positioned just 1 step above active range to init to OFF)
    EXPECT_EQ(1625, rcControls[RC_ARM]);

    // given
    rxResetFlightChannelStatus();

    // and
    for (uint8_t channelIndex = 0; channelIndex < MAX_SUPPORTED_RC_CHANNEL_COUNT; channelIndex++) {
        bool validPulse = isPulseValid(1500);
        rxUpdateFlightChannelStatus(channelIndex, validPulse);
    }

    // then
    EXPECT_TRUE(rxHaveValidFlightChannels());
}

TEST(RxTest, TestValidFlightChannelsHighArm)
{
    // given
    memset(&testData, 0, sizeof(testData));
    memset(&rcModeActivationMap, 0, sizeof(rcModeActivationMap)); // BOXFAILSAFE must be OFF

    // and
    rxConfigMutable()->rx_pulse_min = 1000;
    rxConfigMutable()->rx_pulse_max = 2000;

    // and
    armingConfigMutable()->arm_range.startStep = CHANNEL_VALUE_TO_STEP(1400);
    armingConfigMutable()->arm_range.endStep = MAX_MODE_RANGE_STEP;

    // when
    rxInit();

    // then (ARM channel should be positioned just 1 step below active range to init to OFF)
    EXPECT_EQ(1375, rcControls[RC_ARM]);

    // given
    rxResetFlightChannelStatus();

    // and
    for (uint8_t channelIndex = 0; channelIndex < MAX_SUPPORTED_RC_CHANNEL_COUNT; channelIndex++) {
        bool validPulse = isPulseValid(1500);
        rxUpdateFlightChannelStatus(channelIndex, validPulse);
    }

    // then
    EXPECT_TRUE(rxHaveValidFlightChannels());
}

TEST(RxTest, TestInvalidFlightChannels)
{
    // given
    memset(&testData, 0, sizeof(testData));

    // and
    rxConfigMutable()->rx_pulse_min = 1000;
    rxConfigMutable()->rx_pulse_max = 2000;

    // and
    armingConfigMutable()->arm_range.startStep = CHANNEL_VALUE_TO_STEP(1400);
    armingConfigMutable()->arm_range.endStep = MAX_MODE_RANGE_STEP;

    // and
    uint16_t channelPulses[MAX_SUPPORTED_RC_CHANNEL_COUNT];
    memset(&channelPulses, 1500, sizeof(channelPulses));

    // and
    rxInit();

    // then (ARM channel should be positioned just 1 step below active range to init to OFF)
    EXPECT_EQ(1375, rcControls[RC_ARM]);

    // and
    for (uint8_t stickChannelIndex = 0; stickChannelIndex < STICK_CHANNEL_COUNT; stickChannelIndex++) {
        // given
        rxResetFlightChannelStatus();

        for (uint8_t otherStickChannelIndex = 0; otherStickChannelIndex < STICK_CHANNEL_COUNT; otherStickChannelIndex++) {
            channelPulses[otherStickChannelIndex] = rxConfig()->rx_pulse_min;
        }
        channelPulses[stickChannelIndex] = rxConfig()->rx_pulse_min - 1;

        // when
        for (uint8_t channelIndex = 0; channelIndex < MAX_SUPPORTED_RC_CHANNEL_COUNT; channelIndex++) {
            bool validPulse = isPulseValid(channelPulses[channelIndex]);
            rxUpdateFlightChannelStatus(channelIndex, validPulse);
        }

        // then
        EXPECT_FALSE(rxHaveValidFlightChannels());

        // given
        rxResetFlightChannelStatus();

        for (uint8_t otherStickChannelIndex = 0; otherStickChannelIndex < STICK_CHANNEL_COUNT; otherStickChannelIndex++) {
            channelPulses[otherStickChannelIndex] = rxConfig()->rx_pulse_max;
        }
        channelPulses[stickChannelIndex] = rxConfig()->rx_pulse_max + 1;

        // when
        for (uint8_t channelIndex = 0; channelIndex < MAX_SUPPORTED_RC_CHANNEL_COUNT; channelIndex++) {
            bool validPulse = isPulseValid(channelPulses[channelIndex]);
            rxUpdateFlightChannelStatus(channelIndex, validPulse);
        }

        // then
        EXPECT_FALSE(rxHaveValidFlightChannels());
    }
}
#endif

// STUBS

extern "C" {
    void failsafeOnValidDataFailed() {}
    void failsafeOnValidDataReceived() {}
    uint16_t getFailsafeCenter(void) { return 1500; }
    uint16_t getFailsafeThrottle(void) { return 1036; }

    void failsafeOnRxSuspend(uint32_t ) {}
    void failsafeOnRxResume(void) {}
    bool failsafeIsActive(void) { return false; }
    bool failsafeIsReceivingRxData(void) { return true; }
    uint32_t failsafeFailurePeriodMs(void) { return 400; }
    uint16_t flightModeFlags = 0;

    uint32_t micros(void) { return 0; }
    uint32_t millis(void) { return 0; }

    bool isPPMDataBeingReceived(void) {
        return testData.isPPMDataBeingReceived;
    }

    void resetPPMDataReceivedState(void) {}
    bool rxMspFrameComplete(void) { return false; }

    void crsfRxInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void ibusInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void jetiExBusInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void sbusInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void spektrumInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void sumdInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void sumhInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void xBusInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void rxMspInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void rxPwmInit(const rxConfig_t *, rxRuntimeState_t *) {}
    void setArmingDisabled(armingDisableFlags_e flag) { UNUSED(flag); }
    void unsetArmingDisabled(armingDisableFlags_e flag) { UNUSED(flag); }
    bool taskUpdateRxMainInProgress(void) { return true; }
    float pt1FilterGain(float f_cut, float dT)
    {
        UNUSED(f_cut);
        UNUSED(dT);
        return 0.0;
    }

    void pt1FilterInit(pt1Filter_t *filter, float k)
    {
        UNUSED(filter);
        UNUSED(k);
    }

    void pt1FilterUpdateCutoff(pt1Filter_t *filter, float k)
    {
        UNUSED(filter);
        UNUSED(k);
    }

    float pt1FilterApply(pt1Filter_t *filter, float input)
    {
        UNUSED(filter);
        UNUSED(input);
        return 0.0;
    }

    void pinioBoxTaskControl(void) {}

}
