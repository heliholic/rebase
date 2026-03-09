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

extern "C" {
    #include "unittest_platform.h"

    #include "build/debug.h"
    #include "common/maths.h"

    #include "fc/rc_controls.h"
    #include "fc/rc_modes.h"
    #include "fc/runtime_config.h"

    #include "io/vtx.h"

    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/rx.h"

    #include "rx/rx.h"

    vtxSettingsConfig_t vtxGetSettings(void);

    extern float rcCommand[5];
    float rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];
    uint8_t cliMode = 0;
    uint8_t debugMode = 0;
    int32_t debug[DEBUG_VALUE_COUNT];
    rxRuntimeState_t rxRuntimeState = {};
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

TEST(VtxTest, PitMode)
{
    // given
    modeActivationConditionsMutable(0)->auxChannelIndex = 0;
    modeActivationConditionsMutable(0)->modeId = BOXVTXPITMODE;
    modeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(1750);
    modeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(CHANNEL_RANGE_MAX);

    analyzeModeActivationConditions();

    // and
    vtxSettingsConfigMutable()->band = 0;
    vtxSettingsConfigMutable()->freq = 5800;
    vtxSettingsConfigMutable()->pitModeFreq = 5300;

    // expect
    EXPECT_EQ(5800, vtxGetSettings().freq);

    // and
    // enable vtx pit mode
    rcData[AUX1] = 1800;

    // when
    updateActivatedModes();

    // expect
    EXPECT_TRUE(IS_RC_MODE_ACTIVE(BOXVTXPITMODE));
    EXPECT_EQ(5300, vtxGetSettings().freq);
}

// STUBS
extern "C" {
    uint32_t micros(void) { return 0; }
    uint32_t millis(void) { return micros() / 1000; }
    bool featureIsEnabled(uint32_t) { return true; }
    void saveConfigAndNotify(void) {}
    void beeperConfirmationBeeps(uint8_t) {}
    bool failsafeIsActive(void) { return false; }
    void pinioBoxTaskControl(void) {}
    void vtxControlInputPoll(void) {}
    void parseRcChannels(const char *, rxConfig_t *) {}
}
