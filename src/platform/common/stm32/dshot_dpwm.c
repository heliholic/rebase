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
#include <math.h>
#include <string.h>

#include "platform.h"

#ifdef USE_DSHOT

#include "common/utils.h"

#include "drivers/pwm_output.h"
#include "pwm_output_dshot_shared.h"
#include "drivers/dshot.h"
#include "dshot_dpwm.h"
#include "drivers/motor_impl.h"

#include "pg/motor.h"

// XXX TODO: Share a single region among dshotDmaBuffer and dshotBurstDmaBuffer

DSHOT_DMA_BUFFER_ATTRIBUTE DSHOT_DMA_BUFFER_UNIT dshotDmaBuffer[MAX_SUPPORTED_MOTORS][DSHOT_DMA_BUFFER_ALLOC_SIZE];

#ifdef USE_DSHOT_DMAR
DSHOT_DMA_BUFFER_ATTRIBUTE DSHOT_DMA_BUFFER_UNIT dshotBurstDmaBuffer[MAX_DMA_TIMERS][DSHOT_DMA_BUFFER_SIZE * 4];
#endif

#ifdef USE_DSHOT_DMAR
FAST_DATA_ZERO_INIT bool useBurstDshot = false;
#endif
#ifdef USE_DSHOT_TELEMETRY
FAST_DATA_ZERO_INIT bool useDshotTelemetry = false;
#endif

FAST_DATA_ZERO_INIT loadDmaBufferFn *loadDmaBuffer;

FAST_CODE_NOINLINE uint8_t loadDmaBufferDshot(uint32_t *dmaBuffer, int stride, uint16_t packet)
{
    int i;
    for (i = 0; i < 16; i++) {
        dmaBuffer[i * stride] = (packet & 0x8000) ? MOTOR_BIT_1 : MOTOR_BIT_0;  // MSB first
        packet <<= 1;
    }
    dmaBuffer[i++ * stride] = 0;
    dmaBuffer[i++ * stride] = 0;

    return DSHOT_DMA_BUFFER_SIZE;
}

FAST_CODE_NOINLINE uint8_t loadDmaBufferProshot(uint32_t *dmaBuffer, int stride, uint16_t packet)
{
    int i;
    for (i = 0; i < 4; i++) {
        dmaBuffer[i * stride] = PROSHOT_BASE_SYMBOL + ((packet & 0xF000) >> 12) * PROSHOT_BIT_WIDTH;  // Most significant nibble first
        packet <<= 4;   // Shift 4 bits
    }
    dmaBuffer[i++ * stride] = 0;
    dmaBuffer[i++ * stride] = 0;

    return PROSHOT_DMA_BUFFER_SIZE;
}

uint32_t getDshotHz(motorProtocolTypes_e pwmProtocolType)
{
    switch (pwmProtocolType) {
    case(MOTOR_PROTOCOL_PROSHOT1000):
        return MOTOR_PROSHOT1000_HZ;
    case(MOTOR_PROTOCOL_DSHOT600):
        return MOTOR_DSHOT600_HZ;
    case(MOTOR_PROTOCOL_DSHOT300):
        return MOTOR_DSHOT300_HZ;
    default:
    case(MOTOR_PROTOCOL_DSHOT150):
        return MOTOR_DSHOT150_HZ;
    }
}

static void dshotPwmShutdown(void)
{
    // DShot signal is only generated if write to motors happen,
    // and that is prevented by enabled checking at write.
    // Hence there's no special processing required here.
    return;
}

static void dshotPwmDisableMotors(void)
{
    // No special processing required
    return;
}

static bool dshotPwmEnableMotors(void)
{
    for (int i = 0; i < MAX_SUPPORTED_MOTORS; i++) {
        const motorDmaOutput_t *motor = getMotorDmaOutput(i);
        if (motor->configured) {
            const IO_t motorIO = IOGetByTag(motor->timerHardware->tag);
            IOConfigGPIOAF(motorIO, motor->iocfg, motor->timerHardware->alternateFunction);
        }
    }

    // No special processing required
    return true;
}

static bool dshotPwmIsMotorEnabled(unsigned index)
{
    const motorDmaOutput_t *motor = getMotorDmaOutput(index);
    return motor && motor->configured;
}

static IO_t pwmDshotGetMotorIO(unsigned index)
{
    if (!dshotPwmIsMotorEnabled(index)) {
        return IO_NONE;
    }
    return IOGetByTag(getMotorDmaOutput(index)->timerHardware->tag);
}

static FAST_CODE void dshotWriteInt(uint8_t index, uint16_t value)
{
    pwmWriteDshotInt(index, value);
}

static FAST_CODE void dshotWrite(uint8_t index, float throttle)
{
    pwmWriteDshotInt(index, dshotConvertToInternal(index, throttle));
}

static const motorVTable_t dshotPwmVTable = {
    .postInit = motorPostInitNull,
    .enable = dshotPwmEnableMotors,
    .disable = dshotPwmDisableMotors,
    .isMotorEnabled = dshotPwmIsMotorEnabled,
    .decodeTelemetry = pwmTelemetryDecode,
    .write = dshotWrite,
    .writeInt = dshotWriteInt,
    .updateComplete = pwmCompleteDshotMotorUpdate,
    .shutdown = dshotPwmShutdown,
    .requestTelemetry = pwmDshotRequestTelemetry,
    .isMotorIdle = pwmDshotIsMotorIdle,
    .getMotorIO = pwmDshotGetMotorIO,
};

uint32_t dshotPwmDevInit(uint32_t motorMask, const motorVTable_t **vTable)
{
    const motorDevConfig_t *motorDevConfig = &motorConfig()->dev;

    *vTable = &dshotPwmVTable;
    dshotMotorMask = 0;
#ifdef USE_DSHOT_TELEMETRY
    useDshotTelemetry = motorDevConfig->useDshotTelemetry;
#endif

    switch (motorDevConfig->motorProtocol) {
    case MOTOR_PROTOCOL_PROSHOT1000:
        loadDmaBuffer = loadDmaBufferProshot;
        break;
    case MOTOR_PROTOCOL_DSHOT600:
    case MOTOR_PROTOCOL_DSHOT300:
    case MOTOR_PROTOCOL_DSHOT150:
        loadDmaBuffer = loadDmaBufferDshot;
#ifdef USE_DSHOT_DMAR
        useBurstDshot = motorDevConfig->useBurstDshot == DSHOT_DMAR_ON ||
            (motorDevConfig->useBurstDshot == DSHOT_DMAR_AUTO && !motorDevConfig->useDshotTelemetry);
#endif
        break;
    }

    for (int motorIndex = 0; motorIndex < MAX_SUPPORTED_MOTORS; motorIndex++) {
        if (motorMask & BIT(motorIndex)) {
            const ioTag_t tag = motorDevConfig->ioTags[motorIndex];
            const timerHardware_t *timerHardware = timerAllocate(tag, OWNER_MOTOR, RESOURCE_INDEX(motorIndex));

            // A motor whose pin has no free timer or DMA stays down; the others carry on
            if (timerHardware) {
                IOInit(IOGetByTag(tag), OWNER_MOTOR, RESOURCE_INDEX(motorIndex));

                if (pwmDshotMotorHardwareConfig(timerHardware,
                    motorIndex,
                    motorDevConfig->motorProtocol,
                    motorDevConfig->motorInversion ? timerHardware->output ^ TIMER_OUTPUT_INVERTED : timerHardware->output)) {
                    dshotMotorMask |= BIT(motorIndex);
                }
            }
        }
    }

    return dshotMotorMask;
}

#endif // USE_DSHOT
