/*
 * This file is part of Cleanflight and Betaflight.
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
 *
 * Author: jflyper
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#ifdef USE_MOTOR

#include "common/maths.h"
#include "common/utils.h"

#include "fc/feature.h"

#include "drivers/dshot.h"
#include "drivers/dshot_bitbang.h" // TODO: bitbang should be behind the veil of dshot (it is an implementation)
#include "drivers/pwm_output.h"

#include "drivers/time.h"

#include "sensors/battery.h"

#if ENABLE_DRONECAN_ESC
#include "io/dronecan/dronecan_esc.h"
#endif

#include "motor.h"

static FAST_DATA_ZERO_INIT motorDevice_t motorDevice;

static bool motorProtocolEnabled = false;
static bool motorProtocolDshot = false;

void motorShutdown(void)
{
    uint32_t shutdownDelayUs = 1500;

    motorDevice.vTable->shutdown();
    motorDevice.enabled = false;
    motorDevice.motorEnableTimeMs = 0;
    motorDevice.initialized = false;

    switch (motorConfig()->dev.motorProtocol) {
        case MOTOR_PROTOCOL_PWM:
        case MOTOR_PROTOCOL_ONESHOT125:
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_MULTISHOT:
            // Delay 500ms will disarm esc which can prevent motor spin while reboot
            shutdownDelayUs += 500 * 1000;
            break;
        default:
            break;
    }

    delayMicroseconds(shutdownDelayUs);
}

void motorWriteAll(float *values)
{
#ifdef USE_PWM_OUTPUT
    if (motorDevice.enabled) {
#ifdef USE_DSHOT_BITBANG
        if (isDshotBitbangActive(&motorConfig()->dev)) {
            // Initialise the output buffers
            if (motorDevice.vTable->updateInit) {
                motorDevice.vTable->updateInit();
            }

            // Update the motor data
            for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
                if (motorDevice.motorMask & BIT(i)) {
                    motorDevice.vTable->write(i, values[i]);
                }
            }

            // Don't attempt to write commands to the motors if telemetry is still being received
            if (motorDevice.vTable->telemetryWait) {
                (void)motorDevice.vTable->telemetryWait();
            }

            // Trigger the transmission of the motor data
            motorDevice.vTable->updateComplete();

            // Perform the decode of the last data received
            // New data will be received once the send of motor data, triggered above, completes
#if defined(USE_DSHOT) && defined(USE_DSHOT_TELEMETRY)
            if (motorDevice.vTable->decodeTelemetry) {
                motorDevice.vTable->decodeTelemetry();
            }
#endif
        } else
#endif
        {
            // Perform the decode of the last data received
            // New data will be received once the send of motor data, triggered above, completes
#if defined(USE_DSHOT) && defined(USE_DSHOT_TELEMETRY)
            if (motorDevice.vTable->decodeTelemetry) {
                motorDevice.vTable->decodeTelemetry();
            }
#endif

            // Update the motor data
            for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
                if (motorDevice.motorMask & BIT(i)) {
                    motorDevice.vTable->write(i, values[i]);
                }
            }

            // Trigger the transmission of the motor data
            motorDevice.vTable->updateComplete();
        }
    }
#else
    UNUSED(values);
#endif
}

static bool motorIsPresent(unsigned index)
{
    return index < MAX_SUPPORTED_MOTORS && (motorDevice.motorMask & BIT(index));
}

void motorRequestTelemetry(unsigned index)
{
    if (motorIsPresent(index) && motorDevice.vTable->requestTelemetry) {
        motorDevice.vTable->requestTelemetry(index);
    }
}

uint32_t motorDeviceMask(void)
{
    return motorDevice.motorMask;
}

const motorVTable_t *motorGetVTable(void)
{
    return motorDevice.vTable;
}

bool checkMotorProtocolEnabled(const motorDevConfig_t *motorConfig)
{
    switch (motorConfig->motorProtocol) {
        case MOTOR_PROTOCOL_PWM:
        case MOTOR_PROTOCOL_ONESHOT125:
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_MULTISHOT:
        case MOTOR_PROTOCOL_BRUSHED:
#ifdef USE_DSHOT
        case MOTOR_PROTOCOL_DSHOT150:
        case MOTOR_PROTOCOL_DSHOT300:
        case MOTOR_PROTOCOL_DSHOT600:
        case MOTOR_PROTOCOL_PROSHOT1000:
#endif
#if ENABLE_DRONECAN_ESC
        case MOTOR_PROTOCOL_DRONECAN:
#endif
            return true;
        default:
            break;
    }
    return false;
}

bool checkMotorProtocolDshot(const motorDevConfig_t *motorConfig)
{
#ifdef USE_DSHOT
    switch (motorConfig->motorProtocol) {
        case MOTOR_PROTOCOL_DSHOT150:
        case MOTOR_PROTOCOL_DSHOT300:
        case MOTOR_PROTOCOL_DSHOT600:
        case MOTOR_PROTOCOL_PROSHOT1000:
            return true;
        default:
            break;
    }
#else
    UNUSED(motorConfig);
#endif
    return false;
}

motorProtocolFamily_e motorGetProtocolFamily(void)
{
    switch (motorConfig()->dev.motorProtocol) {
#ifdef USE_PWM_OUTPUT
        case MOTOR_PROTOCOL_PWM:
        case MOTOR_PROTOCOL_ONESHOT125:
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_MULTISHOT:
        case MOTOR_PROTOCOL_BRUSHED:
            return MOTOR_PROTOCOL_FAMILY_PWM;
#endif
#ifdef USE_DSHOT
        case MOTOR_PROTOCOL_DSHOT150:
        case MOTOR_PROTOCOL_DSHOT300:
        case MOTOR_PROTOCOL_DSHOT600:
        case MOTOR_PROTOCOL_PROSHOT1000:
            return MOTOR_PROTOCOL_FAMILY_DSHOT;
#endif
#if ENABLE_DRONECAN_ESC
        case MOTOR_PROTOCOL_DRONECAN:
            return MOTOR_PROTOCOL_FAMILY_CAN;
#endif
        default:
            break;
    }
    return MOTOR_PROTOCOL_FAMILY_UNKNOWN;
}

bool isMotorProtocolDronecan(void)
{
#if ENABLE_DRONECAN_ESC
    return motorConfig()->dev.motorProtocol == MOTOR_PROTOCOL_DRONECAN;
#else
    return false;
#endif
}

void motorPostInit(void)
{
    if (motorDevice.vTable->postInit) {
        motorDevice.vTable->postInit();
    }

    // Some drivers only set the hardware up here, and a motor can still fail
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if ((motorDevice.motorMask & BIT(i)) && !motorDevice.vTable->isMotorEnabled(i)) {
            motorDevice.motorMask &= ~BIT(i);
        }
    }
}

// The protocol is usable, and every motor the configuration asks for is set up
bool isMotorProtocolEnabled(void)
{
    return motorProtocolEnabled && motorDevice.motorMask == motorDevice.configuredMask;
}

bool isMotorProtocolDshot(void)
{
    return motorProtocolDshot;
}

bool isMotorProtocolBidirDshot(void)
{
    return isMotorProtocolDshot() && useDshotTelemetry;
}

void motorNullDevInit(motorDevice_t *device);

// A motor is configured when it has a pin. DroneCAN and the simulator
// address motors by number instead, so there every motor is configured.
static uint32_t motorConfiguredMask(void)
{
#ifdef SIMULATOR_BUILD
    const bool pinless = true;
#else
    const bool pinless = isMotorProtocolDronecan();
#endif
    uint32_t mask = 0;

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (pinless || motorConfig()->dev.ioTags[i]) {
            mask |= BIT(i);
        }
    }

    return mask;
}

void motorDevInit(void)
{
    motorProtocolEnabled = checkMotorProtocolEnabled(&motorConfig()->dev);
    motorProtocolDshot = checkMotorProtocolDshot(&motorConfig()->dev);

    const motorVTable_t *vTable = NULL;
    uint32_t motorMask = 0;

    motorDevice.configuredMask = 0;

    if (motorProtocolEnabled) {
        const uint32_t configuredMask = motorConfiguredMask();

        motorDevice.configuredMask = configuredMask;

        do {
#if ENABLE_DRONECAN_ESC
            if (isMotorProtocolDronecan()) {
                motorMask = dronecanMotorDevInit(configuredMask, &vTable);
                break;
            }
#endif
            if (!isMotorProtocolDshot()) {
#ifdef USE_PWM_OUTPUT
                motorMask = motorPwmDevInit(configuredMask, &vTable);
#endif
                break;
            }
#ifdef USE_DSHOT
#ifdef USE_DSHOT_BITBANG
            if (isDshotBitbangActive(&motorConfig()->dev)) {
                motorMask = dshotBitbangDevInit(configuredMask, &vTable);
                break;
            }
#endif
            motorMask = dshotPwmDevInit(configuredMask, &vTable);
#endif
        } while (0);
    }

    // if the VTable has been populated, the device is initialized.
    if (vTable) {
        motorDevice.vTable = vTable;
        motorDevice.motorMask = motorMask;
        motorDevice.initialized = true;
        motorDevice.motorEnableTimeMs = 0;
        motorDevice.enabled = false;
    } else {
        motorNullDevInit(&motorDevice);
    }
}

void motorDisable(void)
{
    motorDevice.vTable->disable();
    motorDevice.enabled = false;
    motorDevice.motorEnableTimeMs = 0;
}

void motorEnable(void)
{
    if (motorDevice.initialized && motorDevice.vTable->enable()) {
        motorDevice.enabled = true;
        motorDevice.motorEnableTimeMs = millis();
    }
}

float motorEstimateMaxRpm(void)
{
    // Empirical testing found this relationship between estimated max RPM without props attached
    // (unloaded) and measured max RPM with props attached (loaded), independent from prop size
    float unloadedMaxRpm = 0.01f * getBatteryVoltage() * motorConfig()->kv;
    float loadDerating = -5.44e-6f * unloadedMaxRpm + 0.944f;

    return unloadedMaxRpm * loadDerating;
}

bool motorIsEnabled(void)
{
    return motorDevice.enabled;
}

bool motorIsMotorEnabled(unsigned index)
{
    return motorIsPresent(index) && motorDevice.vTable->isMotorEnabled(index);
}

bool motorIsMotorIdle(unsigned index)
{
    return (motorIsPresent(index) && motorDevice.vTable->isMotorIdle) ? motorDevice.vTable->isMotorIdle(index) : false;
}

#ifdef USE_DSHOT
timeMs_t motorGetMotorEnableTimeMs(void)
{
    return motorDevice.motorEnableTimeMs;
}
#endif

IO_t motorGetIo(unsigned index)
{
    if (motorIsPresent(index) && motorDevice.vTable->getMotorIO) {
        return motorDevice.vTable->getMotorIO(index);
    }
    return IO_NONE;
}

/* functions below for empty methods and no active motors */
void motorPostInitNull(void)
{
}

static bool motorEnableNull(void)
{
    return false;
}

static void motorDisableNull(void)
{
}

static bool motorIsEnabledNull(unsigned index)
{
    UNUSED(index);
    return false;
}

bool motorDecodeTelemetryNull(void)
{
    return true;
}

void motorWriteNull(uint8_t index, float throttle)
{
    UNUSED(index);
    UNUSED(throttle);
}

static void motorWriteIntNull(uint8_t index, uint16_t value)
{
    UNUSED(index);
    UNUSED(value);
}

static void motorUpdateCompleteNull(void)
{
}

static void motorShutdownNull(void)
{
}

static const motorVTable_t motorNullVTable = {
    .postInit = motorPostInitNull,
    .enable = motorEnableNull,
    .disable = motorDisableNull,
    .isMotorEnabled = motorIsEnabledNull,
    .decodeTelemetry = motorDecodeTelemetryNull,
    .write = motorWriteNull,
    .writeInt = motorWriteIntNull,
    .updateComplete = motorUpdateCompleteNull,
    .shutdown = motorShutdownNull,
    .requestTelemetry = NULL,
    .isMotorIdle = NULL,
    .getMotorIO = NULL,
};

void motorNullDevInit(motorDevice_t *device)
{
    device->vTable = &motorNullVTable;
    device->motorMask = 0;
}


#endif // USE_MOTOR
