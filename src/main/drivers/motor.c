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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#ifdef USE_MOTOR

#include "common/maths.h"
#include "common/utils.h"

#include "fc/feature.h"

#include "drivers/dshot.h"
#include "drivers/dshot_command.h"
#include "drivers/dshot_bitbang.h" // TODO: bitbang should be behind the veil of dshot (it is an implementation)
#include "drivers/pwm_output.h"
#include "drivers/timer.h"

#include "drivers/time.h"

#include "pg/servo.h"

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

// Frame scheduling. The motors of a timebase group send together, once every
// `divider` motor updates, indexed by the group's lowest motor.
static FAST_DATA_ZERO_INIT uint32_t motorGroupLeaders;
static FAST_DATA_ZERO_INIT uint32_t motorGroupMask[MAX_SUPPORTED_MOTORS];
static FAST_DATA_ZERO_INIT uint16_t motorGroupDivider[MAX_SUPPORTED_MOTORS];
static FAST_DATA_ZERO_INIT uint16_t motorGroupCountdown[MAX_SUPPORTED_MOTORS];

// While DShot commands run, all DShot motors send together, as often as the
// slowest of them may: the commands count frames
static FAST_DATA_ZERO_INIT uint16_t motorCommandDivider;
static FAST_DATA_ZERO_INIT uint16_t motorCommandCountdown;

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

// The order the drivers are updated in. DShot goes first: its frame timing
// matters most.
static const motorProtocolFamily_e motorDriverOrder[] = {
    MOTOR_PROTOCOL_FAMILY_DSHOT,
    MOTOR_PROTOCOL_FAMILY_PWM,
    MOTOR_PROTOCOL_FAMILY_CAN,
};

void motorShutdown(void)
{
    uint32_t shutdownDelayUs = 1500;
    bool analog = false;

    for (unsigned d = 0; d < ARRAYLEN(motorDriverOrder); d++) {
        const motorVTable_t *vTable = motorDevice.vTable[motorDriverOrder[d]];
        if (vTable) {
            vTable->shutdown();
        }
    }
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

static void motorDriverWrite(const motorVTable_t *vTable, uint32_t motorMask, float *values, bool bitbang)
{
    if (bitbang) {
        // Initialise the output buffers
        if (vTable->updateInit) {
            vTable->updateInit();
        }

        // Update the motor data
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
            if (motorMask & BIT(i)) {
                vTable->write(i, values[i]);
            }
        }

        // Don't attempt to write commands to the motors if telemetry is still being received
        if (vTable->telemetryWait) {
            (void)vTable->telemetryWait();
        }

        // Trigger the transmission of the motor data
        vTable->updateComplete();

        // Perform the decode of the last data received
        // New data will be received once the send of motor data, triggered above, completes
#if defined(USE_DSHOT) && defined(USE_DSHOT_TELEMETRY)
        if (vTable->decodeTelemetry) {
            vTable->decodeTelemetry();
        }
#endif
    }
    else {
        // Perform the decode of the last data received
        // New data will be received once the send of motor data, triggered above, completes
#if defined(USE_DSHOT) && defined(USE_DSHOT_TELEMETRY)
        if (vTable->decodeTelemetry) {
            vTable->decodeTelemetry();
        }
#endif

        // Update the motor data
        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
            if (motorMask & BIT(i)) {
                vTable->write(i, values[i]);
            }
        }

        // Trigger the transmission of the motor data
        vTable->updateComplete();
    }
}

static void motorWriteMask(float *values, uint32_t motorMask)
{
    for (unsigned d = 0; d < ARRAYLEN(motorDriverOrder); d++) {
        const motorProtocolFamily_e family = motorDriverOrder[d];
        const motorVTable_t *vTable = motorDevice.vTable[family];
        const uint32_t driverMask = motorDevice.driverMask[family] & motorMask;
        if (vTable && driverMask) {
#ifdef USE_DSHOT_BITBANG
            const bool bitbang = (family == MOTOR_PROTOCOL_FAMILY_DSHOT) && isDshotBitbangActive();
#else
            const bool bitbang = false;
#endif
            motorDriverWrite(vTable, driverMask, values, bitbang);
        }
    }
}

// Write the motors whose group is due this update
void motorWriteAll(float *values)
{
    if (motorDevice.enabled) {
        uint32_t dueMask = 0;

        for (int g = 0; g < MAX_SUPPORTED_MOTORS; g++) {
            if (motorGroupLeaders & BIT(g)) {
                if (--motorGroupCountdown[g] == 0) {
                    motorGroupCountdown[g] = motorGroupDivider[g];
                    dueMask |= motorGroupMask[g];
                }
            }
        }

#ifdef USE_DSHOT
        if (!dshotCommandQueueEmpty()) {
            const uint32_t dshotMask = motorDevice.driverMask[MOTOR_PROTOCOL_FAMILY_DSHOT];
            dueMask &= ~dshotMask;
            if (--motorCommandCountdown == 0) {
                motorCommandCountdown = motorCommandDivider;
                dueMask |= dshotMask;
            }
        }
#endif

        motorWriteMask(values, dueMask);
    }
}

// Write every motor at once, whatever its schedule: for stopping them
void motorWriteAllNow(float *values)
{
    if (motorDevice.enabled) {
        motorWriteMask(values, motorDevice.motorMask);
    }
}

// Set how often each group sends, from the motor update period. A group
// sending one frame per update is held to its rate by skipping updates;
// free-running PWM and DroneCAN keep their own pace and take every update.
void motorSetLoopTime(timeUs_t looptimeUs)
{
    motorGroupLeaders = 0;
    motorCommandDivider = 1;
    motorCommandCountdown = 1;
    memset(motorGroupMask, 0, sizeof(motorGroupMask));

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (motorDevice.motorMask & BIT(i)) {
            const int group = motorResolved.group[i];
            motorGroupMask[group] |= BIT(i);
            motorGroupLeaders |= BIT(group);
        }
    }

    for (int g = 0; g < MAX_SUPPORTED_MOTORS; g++) {
        if (motorGroupLeaders & BIT(g)) {
            const uint32_t rate = motorResolved.rate[g];
            const bool paced = !motorProtocolIsContinuous(motorConfig(g)->protocol) &&
                motorResolved.family[g] != MOTOR_PROTOCOL_FAMILY_CAN;
            uint32_t divider = 1;

            if (paced && rate && looptimeUs) {
                divider = motorFrameDivider(rate, looptimeUs);

                // Report the rate actually reached
                for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
                    if (motorGroupMask[g] & BIT(i)) {
                        motorResolved.rate[i] = 1000000 / (looptimeUs * divider);
                    }
                }
            }

            motorGroupDivider[g] = divider;
            motorGroupCountdown[g] = 1;

            if (motorResolved.family[g] == MOTOR_PROTOCOL_FAMILY_DSHOT) {
                motorCommandDivider = MAX(motorCommandDivider, divider);
            }
        }
    }

#ifdef USE_DSHOT
    // Command delays count command frames
    dshotSetCommandFrameDivider(motorCommandDivider);
#endif
}

// The driver of a motor that is set up, or NULL
static const motorVTable_t *motorDriverOf(unsigned index)
{
    if (index < MAX_SUPPORTED_MOTORS && (motorDevice.motorMask & BIT(index))) {
        return motorDevice.vTable[motorResolved.family[index]];
    }
    return NULL;
}

void motorRequestTelemetry(unsigned index)
{
    const motorVTable_t *vTable = motorDriverOf(index);

    if (vTable && vTable->requestTelemetry) {
        vTable->requestTelemetry(index);
    }
}

uint32_t motorDeviceMask(void)
{
    return motorDevice.motorMask;
}

uint32_t motorGetDriverMask(motorProtocolFamily_e family)
{
    return family < MOTOR_PROTOCOL_FAMILY_COUNT ? motorDevice.driverMask[family] : 0;
}

const motorVTable_t *motorGetDriverVTable(motorProtocolFamily_e family)
{
    return family < MOTOR_PROTOCOL_FAMILY_COUNT ? motorDevice.vTable[family] : NULL;
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

// A servo whose timer a motor needs for another timebase is not driven
bool motorIsServoRefused(unsigned servoIndex)
{
    return servoIndex < MAX_SUPPORTED_SERVOS && (motorResolved.refusedServos & BIT(servoIndex));
}

// The rate a servo on a motor's timer must run at, 0 if its timer has no motor
uint16_t motorGetServoRate(unsigned servoIndex)
{
    return servoIndex < MAX_SUPPORTED_SERVOS ? motorResolved.servoRate[servoIndex] : 0;
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
    for (unsigned d = 0; d < ARRAYLEN(motorDriverOrder); d++) {
        const motorVTable_t *vTable = motorDevice.vTable[motorDriverOrder[d]];
        if (vTable && vTable->postInit) {
            vTable->postInit();
        }
    }

    // Some drivers only set the hardware up here, and a motor can still fail
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        const motorVTable_t *vTable = motorDriverOf(i);
        if (vTable && !vTable->isMotorEnabled(i)) {
            motorDevice.motorMask &= ~BIT(i);
            motorDevice.driverMask[motorResolved.family[i]] &= ~BIT(i);
            motorResolved.state[i] = MOTOR_STATE_FAILED;
        }
    }
}

// A motor protocol is in use, every motor the configuration asks for is set
// up, and no servo had to give way to a motor
bool isMotorProtocolEnabled(void)
{
    return motorProtocolEnabled && motorDevice.motorMask == motorDevice.configuredMask &&
        motorResolved.refusedServos == 0;
}

bool isMotorProtocolDshot(void)
{
    return motorProtocolDshot;
}

bool isMotorProtocolBidirDshot(void)
{
    return isMotorProtocolDshot() && useDshotTelemetry;
}

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

    for (int i = 0; i < MAX_SUPPORTED_SERVOS; i++) {
        in->servo[i].timer = MOTOR_TIMER_NONE;
#ifdef USE_SERVOS
        const servoParam_t *param = servoParams(i);
        const ioTag_t tag = servoConfig()->dev.ioTags[i];
        const timerHardware_t *timer = tag ? timerGetConfiguredByTag(tag) : NULL;

        in->servo[i].rate = param->rate;
        in->servo[i].maxPulse = param->mid + param->max;
        if (timer) {
            in->servo[i].timer = timerGetTIMNumber(timer);
        }
#endif
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
    uint32_t resolvedMask[MOTOR_PROTOCOL_FAMILY_COUNT] = { 0 };

    motorProtocolEnabled = false;

    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        if (motorProtocolIsEnabled(motorConfig(i)->protocol)) {
            motorProtocolEnabled = true;
        }
        if (motorResolved.state[i] != MOTOR_STATE_ABSENT) {
            configuredMask |= BIT(i);
        }
        if (motorResolved.state[i] == MOTOR_STATE_OK) {
            resolvedMask[motorResolved.family[i]] |= BIT(i);

            // Keep a rate the hardware forced down, as the servos do
            if (motorConfig(i)->rate && motorResolved.rate[i] < motorConfig(i)->rate) {
                motorConfigMutable(i)->rate = motorResolved.rate[i];
            }
        }
    }

    motorProtocolDshot = (resolvedMask[MOTOR_PROTOCOL_FAMILY_DSHOT] != 0);

    memset(&motorDevice, 0, sizeof(motorDevice));

    for (unsigned d = 0; d < ARRAYLEN(motorDriverOrder); d++) {
        const motorProtocolFamily_e family = motorDriverOrder[d];
        const uint32_t motorMask = resolvedMask[family];
        const motorVTable_t *vTable = NULL;
        uint32_t driverMask = 0;

        if (motorMask) {
            switch (family) {
#if ENABLE_DRONECAN_ESC
                case MOTOR_PROTOCOL_FAMILY_CAN:
                    driverMask = dronecanMotorDevInit(motorMask, &vTable);
                    break;
#endif
#ifdef USE_PWM_OUTPUT
                case MOTOR_PROTOCOL_FAMILY_PWM:
                    driverMask = motorPwmDevInit(motorMask, &vTable);
                    break;
#endif
#ifdef USE_DSHOT
                case MOTOR_PROTOCOL_FAMILY_DSHOT:
#ifdef USE_DSHOT_BITBANG
                    if (motorResolved.bitbang) {
                        driverMask = dshotBitbangDevInit(motorMask, &vTable);
                        break;
                    }
#endif
                    driverMask = dshotPwmDevInit(motorMask, &vTable);
                    break;
#endif
                default:
                    break;
            }
        }

        if (vTable) {
            motorDevice.vTable[family] = vTable;
            motorDevice.driverMask[family] = driverMask;
            motorDevice.motorMask |= driverMask;
        }
        else {
            driverMask = 0;
        }

        for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
            if ((motorMask & BIT(i)) && !(driverMask & BIT(i))) {
                motorResolved.state[i] = MOTOR_STATE_FAILED;
            }
        }
    }

    motorDevice.configuredMask = configuredMask;
    motorDevice.initialized = true;
    motorDevice.motorEnableTimeMs = 0;
    motorDevice.enabled = false;

    // Every update until the loop time is known
    motorSetLoopTime(0);
}

void motorDisable(void)
{
    for (unsigned d = 0; d < ARRAYLEN(motorDriverOrder); d++) {
        const motorVTable_t *vTable = motorDevice.vTable[motorDriverOrder[d]];
        if (vTable) {
            vTable->disable();
        }
    }
    motorDevice.enabled = false;
    motorDevice.motorEnableTimeMs = 0;
}

void motorEnable(void)
{
    bool enabled = motorDevice.initialized && motorDevice.motorMask;

    for (unsigned d = 0; d < ARRAYLEN(motorDriverOrder); d++) {
        const motorVTable_t *vTable = motorDevice.vTable[motorDriverOrder[d]];
        if (vTable && !vTable->enable()) {
            enabled = false;
        }
    }

    if (enabled) {
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
    const motorVTable_t *vTable = motorDriverOf(index);

    return vTable && vTable->isMotorEnabled(index);
}

bool motorIsMotorIdle(unsigned index)
{
    const motorVTable_t *vTable = motorDriverOf(index);

    return (vTable && vTable->isMotorIdle) ? vTable->isMotorIdle(index) : false;
}

#ifdef USE_DSHOT
timeMs_t motorGetMotorEnableTimeMs(void)
{
    return motorDevice.motorEnableTimeMs;
}
#endif

IO_t motorGetIo(unsigned index)
{
    const motorVTable_t *vTable = motorDriverOf(index);

    if (vTable && vTable->getMotorIO) {
        return vTable->getMotorIO(index);
    }
    return IO_NONE;
}

/* empty methods for the drivers to use */
void motorPostInitNull(void)
{
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


#endif // USE_MOTOR
