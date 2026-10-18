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
#include "drivers/timer.h"

#include "drivers/time.h"

#include "sensors/battery.h"

#if ENABLE_DRONECAN_ESC
#include "io/dronecan/dronecan_esc.h"
#endif

#include "motor.h"
#include "motor_resolve.h"

static FAST_DATA_ZERO_INIT motorDevice_t motorDevice;

// Which motors are driven, by what and how fast
static motorResolveOutput_t motorResolved;

static bool motorProtocolEnabled = false;
static bool motorProtocolDshot = false;

static bool motorProtocolIsAnalog(uint8_t protocol)
{
    switch (protocol) {
        case MOTOR_PROTOCOL_PWM:
        case MOTOR_PROTOCOL_ONESHOT125:
        case MOTOR_PROTOCOL_ONESHOT42:
        case MOTOR_PROTOCOL_MULTISHOT:
            return true;
        default:
            return false;
    }
}

void motorShutdown(void)
{
    uint32_t shutdownDelayUs = 1500;
    bool analog = false;

    motorDevice.vTable->shutdown();
    motorDevice.enabled = false;
    motorDevice.motorEnableTimeMs = 0;
    motorDevice.initialized = false;

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if ((motorDevice.configuredMask & BIT(i)) && motorProtocolIsAnalog(motorConfig(i)->protocol)) {
            analog = true;
        }
    }

    if (analog) {
        // Delay 500ms will disarm esc which can prevent motor spin while reboot
        shutdownDelayUs += 500 * 1000;
    }

    delayMicroseconds(shutdownDelayUs);
}

void motorWriteAll(float *values)
{
#ifdef USE_PWM_OUTPUT
    if (motorDevice.enabled) {
#ifdef USE_DSHOT_BITBANG
        if (isDshotBitbangActive()) {
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

motorProtocolFamily_e motorGetProtocolFamily(unsigned index)
{
    return index < MAX_SUPPORTED_MOTORS ? motorResolved.family[index] : MOTOR_PROTOCOL_FAMILY_UNKNOWN;
}

motorState_e motorGetState(unsigned index)
{
    return index < MAX_SUPPORTED_MOTORS ? motorResolved.state[index] : MOTOR_STATE_ABSENT;
}

uint16_t motorGetUpdateRate(unsigned index)
{
    return index < MAX_SUPPORTED_MOTORS ? motorResolved.rate[index] : 0;
}

// What a driver's motors must have in common is set by the lowest of them
const motorConfig_t *motorLeadConfig(uint32_t motorMask)
{
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (motorMask & BIT(i)) {
            return motorConfig(i);
        }
    }
    return motorConfig(0);
}

// Read from the configuration, so it holds before the motors are set up too
bool isMotorProtocolDronecan(void)
{
#if ENABLE_DRONECAN_ESC
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (motorConfig(i)->protocol == MOTOR_PROTOCOL_DRONECAN) {
            return true;
        }
    }
#endif
    return false;
}

#ifdef USE_DSHOT_BITBANG
bool isDshotBitbangActive(void)
{
    return motorResolved.bitbang;
}
#endif

void motorPostInit(void)
{
    if (motorDevice.vTable->postInit) {
        motorDevice.vTable->postInit();
    }

    // Some drivers only set the hardware up here, and a motor can still fail
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if ((motorDevice.motorMask & BIT(i)) && !motorDevice.vTable->isMotorEnabled(i)) {
            motorDevice.motorMask &= ~BIT(i);
            motorResolved.state[i] = MOTOR_STATE_FAILED;
        }
    }
}

// A motor protocol is in use, and every motor the configuration asks for is set up
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

static void motorResolveInputInit(motorResolveInput_t *in)
{
    memset(in, 0, sizeof(*in));

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        const motorConfig_t *param = motorConfig(i);
        const ioTag_t tag = motorDevConfig(i)->ioTag;
        const timerHardware_t *timer = tag ? timerGetConfiguredByTag(tag) : NULL;

        in->motor[i].rate = param->rate;
        in->motor[i].maxPulse = MAX(param->stopPwmPulse, MAX(param->zeroPwmPulse, param->fullPwmPulse));
        in->motor[i].protocol = param->protocol;
        in->motor[i].flags = param->flags;
        in->motor[i].timer = timer ? timerGetTIMNumber(timer) : MOTOR_TIMER_NONE;
        in->motor[i].hasPin = (tag != IO_TAG_NONE);
        in->motor[i].nChannel = timer && (timer->output & TIMER_OUTPUT_N_CHANNEL);
    }

#ifdef SIMULATOR_BUILD
    in->pinless = true;
#endif
#ifdef USE_DSHOT_BITBANG
    in->bitbangMode = motorDriverConfig()->useDshotBitbang;
#if defined(STM32F4) || defined(APM32F4)
    // The DMA2 errata makes bitbang cost the SPI buses their DMA
    in->bitbangNeedsBidir = true;
#endif
#else
    in->bitbangMode = DSHOT_BITBANG_OFF;
#endif
#ifdef USE_DSHOT_DMAR
    in->burstMode = motorDriverConfig()->useBurstDshot;
#else
    in->burstMode = DSHOT_DMAR_OFF;
#endif
}

void motorDevInit(void)
{
    static motorResolveInput_t resolveInput;

    motorResolveInputInit(&resolveInput);
    motorResolve(&resolveInput, &motorResolved);

    uint32_t configuredMask = 0;
    uint32_t resolvedMask = 0;
    motorProtocolFamily_e family = MOTOR_PROTOCOL_FAMILY_UNKNOWN;

    motorProtocolEnabled = false;

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (motorProtocolIsEnabled(motorConfig(i)->protocol)) {
            motorProtocolEnabled = true;
        }
        if (motorResolved.state[i] != MOTOR_STATE_ABSENT) {
            configuredMask |= BIT(i);
        }
        if (motorResolved.state[i] == MOTOR_STATE_OK) {
            resolvedMask |= BIT(i);
            family = motorResolved.family[i];

            // Keep a rate the hardware forced down, as the servos do
            if (motorConfig(i)->rate && motorResolved.rate[i] < motorConfig(i)->rate) {
                motorConfigMutable(i)->rate = motorResolved.rate[i];
            }
        }
    }

    motorProtocolDshot = (family == MOTOR_PROTOCOL_FAMILY_DSHOT);

    const motorVTable_t *vTable = NULL;
    uint32_t motorMask = 0;

    switch (family) {
#if ENABLE_DRONECAN_ESC
        case MOTOR_PROTOCOL_FAMILY_CAN:
            motorMask = dronecanMotorDevInit(resolvedMask, &vTable);
            break;
#endif
#ifdef USE_PWM_OUTPUT
        case MOTOR_PROTOCOL_FAMILY_PWM:
            motorMask = motorPwmDevInit(resolvedMask, &vTable);
            break;
#endif
#ifdef USE_DSHOT
        case MOTOR_PROTOCOL_FAMILY_DSHOT:
#ifdef USE_DSHOT_BITBANG
            if (motorResolved.bitbang) {
                motorMask = dshotBitbangDevInit(resolvedMask, &vTable);
                break;
            }
#endif
            motorMask = dshotPwmDevInit(resolvedMask, &vTable);
            break;
#endif
        default:
            break;
    }

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if ((resolvedMask & BIT(i)) && !(motorMask & BIT(i))) {
            motorResolved.state[i] = MOTOR_STATE_FAILED;
        }
    }

    motorDevice.configuredMask = configuredMask;

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

float motorEstimateMaxRpm(unsigned index)
{
    // Empirical testing found this relationship between estimated max RPM without props attached
    // (unloaded) and measured max RPM with props attached (loaded), independent from prop size
    const uint16_t kv = (index < MAX_SUPPORTED_MOTORS) ? motorConfig(index)->kv : 0;
    float unloadedMaxRpm = 0.01f * getBatteryVoltage() * kv;
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
