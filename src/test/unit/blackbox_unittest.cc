/*
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
#include <string.h>

extern "C" {
    #include "platform.h"

    #include "build/debug.h"

    #include "blackbox/blackbox.h"
    #include "common/utils.h"

    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/rx.h"
    #include "pg/motor.h"

    #include "drivers/accgyro/accgyro.h"
    #include "drivers/accgyro/gyro_sync.h"
    #include "drivers/serial.h"

    #include "flight/failsafe.h"
    #include "flight/mixer.h"
    #include "flight/pid.h"

    #include "fc/rc_controls.h"
    #include "fc/rc_modes.h"

    #include "io/gps.h"
    #include "io/serial.h"

    #include "rx/rx.h"

    #include "sensors/battery.h"
    #include "sensors/gyro.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

gyroDev_t gyroDev;

// Set up the gyro rate and the P-frame denominator, then re-run blackboxInit()
static void blackboxInitWith(uint16_t targetRateHz, uint16_t denom)
{
    gyro.targetRateHz = targetRateHz;
    blackboxConfigMutable()->denom = denom;
    blackboxInit();
}

TEST(BlackboxTest, TestInitIntervals)
{
    // 1kHz gyro, logging every iteration. An I-frame is written every 32ms.
    blackboxInitWith(1000, 1);
    EXPECT_EQ(1, blackboxPInterval);
    EXPECT_EQ(32, blackboxIInterval);
    EXPECT_EQ(5000, blackboxSInterval);
    EXPECT_EQ(312, blackboxGInterval);

    // 1kHz gyro, logging every 4th iteration. I-interval stays a multiple of P.
    blackboxInitWith(1000, 4);
    EXPECT_EQ(4, blackboxPInterval);
    EXPECT_EQ(32, blackboxIInterval);
    EXPECT_EQ(1250, blackboxSInterval);
    EXPECT_EQ(312, blackboxGInterval);

    // 8kHz gyro, logging at 1kHz.
    blackboxInitWith(8000, 8);
    EXPECT_EQ(8, blackboxPInterval);
    EXPECT_EQ(256, blackboxIInterval);
    EXPECT_EQ(5000, blackboxSInterval);
    EXPECT_EQ(312, blackboxGInterval);

    // Slow logging: 32ms would need fewer than one P-frame, so I == P.
    blackboxInitWith(1000, 64);
    EXPECT_EQ(64, blackboxPInterval);
    EXPECT_EQ(64, blackboxIInterval);

    // Very fast gyro with a small denom: I-interval is capped at 64 P-frames.
    blackboxInitWith(8000, 1);
    EXPECT_EQ(1, blackboxPInterval);
    EXPECT_EQ(64, blackboxIInterval);

    // The denom is constrained into [1, 8000].
    blackboxInitWith(1000, 0);
    EXPECT_EQ(1, blackboxPInterval);
}

TEST(BlackboxTest, TestFrameScheduling1kHz)
{
    // 1kHz gyro, logging every iteration -> a fast frame every iteration,
    // an I-frame every 32.
    blackboxInitWith(1000, 1);
    EXPECT_EQ(1, blackboxPInterval);
    EXPECT_EQ(32, blackboxIInterval);

    EXPECT_TRUE(blackboxShouldLogFastFrame());
    EXPECT_TRUE(blackboxShouldLogIFrame());

    for (int ii = 0; ii < 31; ii++) {
        blackboxAdvanceIterationTimers();
        EXPECT_TRUE(blackboxShouldLogFastFrame());
        EXPECT_FALSE(blackboxShouldLogIFrame());
    }

    blackboxAdvanceIterationTimers();
    EXPECT_TRUE(blackboxShouldLogFastFrame());
    EXPECT_TRUE(blackboxShouldLogIFrame());
}

TEST(BlackboxTest, TestFrameSchedulingDecimated)
{
    // 8kHz gyro, logging at 1kHz -> a fast frame every 8th iteration,
    // an I-frame every 256.
    blackboxInitWith(8000, 8);
    EXPECT_EQ(8, blackboxPInterval);
    EXPECT_EQ(256, blackboxIInterval);

    EXPECT_TRUE(blackboxShouldLogFastFrame());
    EXPECT_TRUE(blackboxShouldLogIFrame());

    for (int ii = 0; ii < 7; ii++) {
        blackboxAdvanceIterationTimers();
        EXPECT_FALSE(blackboxShouldLogFastFrame());
        EXPECT_FALSE(blackboxShouldLogIFrame());
    }

    // 8th iteration: a P-frame, but not an I-frame
    blackboxAdvanceIterationTimers();
    EXPECT_TRUE(blackboxShouldLogFastFrame());
    EXPECT_FALSE(blackboxShouldLogIFrame());

    // Advance to iteration 256, which is both
    for (int ii = 8; ii < 256; ii++) {
        blackboxAdvanceIterationTimers();
    }
    EXPECT_TRUE(blackboxShouldLogFastFrame());
    EXPECT_TRUE(blackboxShouldLogIFrame());
}

TEST(BlackboxTest, TestGetRateDenom)
{
    blackboxInitWith(1000, 1);
    EXPECT_EQ(1, blackboxGetRateDenom());

    blackboxInitWith(1000, 4);
    EXPECT_EQ(4, blackboxGetRateDenom());

    blackboxInitWith(8000, 16);
    EXPECT_EQ(16, blackboxGetRateDenom());
}

// STUBS
extern "C" {

PG_REGISTER(motorConfig_t, motorConfig, PG_MOTOR_CONFIG, 0);
PG_REGISTER(batteryConfig_t, batteryConfig, PG_BATTERY_CONFIG, 0);
PG_REGISTER(rxConfig_t, rxConfig, PG_RX_CONFIG, 0);
PG_REGISTER_ARRAY(modeActivationCondition_t, MAX_MODE_ACTIVATION_CONDITION_COUNT, modeActivationConditions, PG_MODE_ACTIVATION_PROFILE, 0);

uint8_t armingFlags;
uint8_t stateFlags;
const uint32_t baudRates[] = {0, 9600, 19200, 38400, 57600, 115200, 230400, 250000,
        400000, 460800, 500000, 921600, 1000000, 1500000, 2000000, 2470000}; // see baudRate_e
uint8_t debugMode = 0;
int32_t debug[DEBUG_VALUE_COUNT];
extern int32_t blackboxHeaderBudget;
gpsSolutionData_t gpsSol;
gpsLocation_t GPS_home_llh;

uint32_t gpsDateTimeToEpoch(const gpsDateTime_t *) { return 0; }

gyro_t gyro;

struct pidProfile_s;
struct pidProfile_s *currentPidProfile;

boxBitmask_t rcModeActivationMask;

void mspSerialAllocatePorts(void) {}
uint32_t getArmingBeepTimeMicros(void) {return 0;}
uint16_t getBatteryVoltageLatest(void) {return 0;}
int32_t getAmperageLatest(void) {return 0;}
bool isAmperageConfigured(void) {return false;}
bool isBatteryVoltageConfigured(void) {return false;}
int16_t getCoreTemperatureCelsius(void) {return 0;}
uint8_t getMotorCount(void) {return 4;}
bool areMotorsRunning(void) { return false; }
void motorStop(void) {}
int getMotorOutput(uint8_t) { return 0; }
int getServoCount(void) {return 0;}
int getServoOutput(uint8_t) {return 0;}
bool IS_RC_MODE_ACTIVE(boxId_e) {return false;}
bool isModeActivationConditionPresent(boxId_e) {return false;}
uint32_t millis(void) {return 0;}
bool sensors(uint32_t) {return false;}
void serialWrite(serialPort_t *, uint8_t) {}
uint32_t serialTxBytesFree(const serialPort_t *) {return 0;}
bool isSerialTransmitBufferEmpty(const serialPort_t *) {return false;}
bool featureIsEnabled(uint32_t) {return false;}
void mspSerialReleasePortIfAllocated(serialPort_t *) {}
const serialPortConfig_t *findSerialPortConfig(serialPortFunction_e ) {return NULL;}
serialPort_t *findSharedSerialPort(uint16_t , serialPortFunction_e ) {return NULL;}
serialPort_t *openSerialPort(serialPortIdentifier_e, serialPortFunction_e, serialReceiveCallbackPtr, void *, uint32_t, portMode_e, portOptions_e) {return NULL;}
void closeSerialPort(serialPort_t *) {}
portSharing_e determinePortSharing(const serialPortConfig_t *, serialPortFunction_e ) {return PORTSHARING_UNUSED;}
failsafePhase_e failsafePhase(void) {return FAILSAFE_IDLE;}
bool rxAreFlightChannelsValid(void) {return false;}
bool isRxReceivingSignal(void) {return false;}
bool isRssiConfigured(void) {return false;}
float getMotorOutputLow(void) {return 0.0;}
float getMotorOutputHigh(void) {return 0.0;}
}
