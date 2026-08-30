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
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#ifdef USE_RX_PPM

#include "build/build_config.h"
#include "build/debug.h"

#include "common/utils.h"

#include "drivers/io.h"
#include "drivers/nvic.h"
#include "drivers/pwm_output.h"
#include "drivers/timer.h"
#include "drivers/timer_impl.h"

#include "pg/rx_pwm.h"

#include "rx_pwm.h"

#define DEBUG_PPM_ISR

#define PPM_CAPTURE_COUNT 12

#if PPM_CAPTURE_COUNT > PWM_INPUT_PORT_COUNT
#define PWM_PORTS_OR_PPM_CAPTURE_COUNT PPM_CAPTURE_COUNT
#else
#define PWM_PORTS_OR_PPM_CAPTURE_COUNT PWM_INPUT_PORT_COUNT
#endif

// TODO - change to timer clocks ticks
#define INPUT_FILTER_TO_HELP_WITH_NOISE_FROM_OPENLRS_TELEMETRY_RX 0x03

static inputFilteringMode_e inputFilteringMode;

void pwmICConfig(timerResource_t *tim, uint8_t channel, uint16_t polarity, uint8_t filter);

typedef enum {
    INPUT_MODE_PPM,
} pwmInputMode_e;

typedef struct {
    pwmInputMode_e mode;
    uint8_t channel; // only used for pwm, ignored by ppm

    uint8_t state;
    uint8_t missedEvents;

    captureCompare_t rise;
    captureCompare_t fall;
    captureCompare_t capture;

    const timerHardware_t *timerHardware;
    timerEdgeHandlerRec_t edgeCb;
    timerOvrHandlerRec_t overflowCb;
} pwmInputPort_t;

static pwmInputPort_t pwmInputPorts[PWM_INPUT_PORT_COUNT];

static uint16_t captures[PWM_PORTS_OR_PPM_CAPTURE_COUNT];

#define PPM_TIMER_PERIOD 0x10000

static uint8_t ppmFrameCount = 0;
static uint8_t lastPPMFrameCount = 0;
static uint8_t ppmCountDivisor = 1;

typedef struct ppmDevice_s {
    //uint32_t previousTime;
    uint32_t currentCapture;
    uint32_t currentTime;
    uint32_t deltaTime;
    uint32_t captures[PWM_PORTS_OR_PPM_CAPTURE_COUNT];
    uint32_t largeCounter;
    uint8_t  pulseIndex;
    int8_t   numChannels;
    int8_t   numChannelsPrevFrame;
    uint8_t  stableFramesSeenCount;

    bool     tracking;
    bool     overflowed;
} ppmDevice_t;

static ppmDevice_t ppmDev;

#define PPM_IN_MIN_SYNC_PULSE_US    2700    // microseconds
#define PPM_IN_MIN_CHANNEL_PULSE_US 750     // microseconds
#define PPM_IN_MAX_CHANNEL_PULSE_US 2250    // microseconds
#define PPM_STABLE_FRAMES_REQUIRED_COUNT    25
#define PPM_IN_MIN_NUM_CHANNELS     4
#define PPM_IN_MAX_NUM_CHANNELS     PWM_PORTS_OR_PPM_CAPTURE_COUNT

bool isPPMDataBeingReceived(void)
{
    return (ppmFrameCount != lastPPMFrameCount);
}

void resetPPMDataReceivedState(void)
{
    lastPPMFrameCount = ppmFrameCount;
}

#define MIN_CHANNELS_BEFORE_PPM_FRAME_CONSIDERED_VALID 4

typedef enum {
    SOURCE_OVERFLOW = 0,
    SOURCE_EDGE = 1
} eventSource_e;

static void ppmISREvent(eventSource_e source, uint32_t capture);

#ifdef DEBUG_PPM_ISR

typedef struct ppmISREvent_s {
    uint32_t capture;
    eventSource_e source;
} ppmISREvent_t;

static ppmISREvent_t ppmEvents[20];
static uint8_t ppmEventIndex = 0;

static void ppmISREvent(eventSource_e source, uint32_t capture)
{
    ppmEventIndex = (ppmEventIndex + 1) % ARRAYLEN(ppmEvents);

    ppmEvents[ppmEventIndex].source = source;
    ppmEvents[ppmEventIndex].capture = capture;
}
#else
static void ppmISREvent(eventSource_e source, uint32_t capture) {}
#endif

static void ppmResetDevice(void)
{
    ppmDev.pulseIndex   = 0;
    ppmDev.currentCapture = 0;
    ppmDev.currentTime  = 0;
    ppmDev.deltaTime    = 0;
    ppmDev.largeCounter = 0;
    ppmDev.numChannels  = -1;
    ppmDev.numChannelsPrevFrame = -1;
    ppmDev.stableFramesSeenCount = 0;
    ppmDev.tracking     = false;
    ppmDev.overflowed   = false;
}

static void ppmOverflowCallback(timerOvrHandlerRec_t* cbRec, captureCompare_t capture)
{
    UNUSED(cbRec);
    ppmISREvent(SOURCE_OVERFLOW, capture);

    ppmDev.largeCounter += capture + 1;
    if (capture == PPM_TIMER_PERIOD - 1) {
        ppmDev.overflowed = true;
    }
}

static void ppmEdgeCallback(timerEdgeHandlerRec_t* cbRec, captureCompare_t capture)
{
    UNUSED(cbRec);
    ppmISREvent(SOURCE_EDGE, capture);

    int32_t i;

    uint32_t previousTime = ppmDev.currentTime;
    uint32_t previousCapture = ppmDev.currentCapture;

    /* Grab the new count */
    uint32_t currentTime = capture;

    /* Convert to 32-bit timer result */
    currentTime += ppmDev.largeCounter;

    if (capture < previousCapture) {
        if (ppmDev.overflowed) {
            currentTime += PPM_TIMER_PERIOD;
        }
    }

    // Divide value if Oneshot, Multishot or brushed motors are active and the timer is shared
    currentTime = currentTime / ppmCountDivisor;

    /* Capture computation */
    if (currentTime > previousTime) {
        ppmDev.deltaTime    = currentTime - (previousTime + (ppmDev.overflowed ? (PPM_TIMER_PERIOD / ppmCountDivisor) : 0));
    } else {
        ppmDev.deltaTime    = (PPM_TIMER_PERIOD / ppmCountDivisor) + currentTime - previousTime;
    }

    ppmDev.overflowed = false;

    /* Store the current measurement */
    ppmDev.currentTime = currentTime;
    ppmDev.currentCapture = capture;

    /* Sync pulse detection */
    if (ppmDev.deltaTime > PPM_IN_MIN_SYNC_PULSE_US) {
        if (ppmDev.pulseIndex == ppmDev.numChannelsPrevFrame
            && ppmDev.pulseIndex >= PPM_IN_MIN_NUM_CHANNELS
            && ppmDev.pulseIndex <= PPM_IN_MAX_NUM_CHANNELS) {
            /* If we see n simultaneous frames of the same
               number of channels we save it as our frame size */
            if (ppmDev.stableFramesSeenCount < PPM_STABLE_FRAMES_REQUIRED_COUNT) {
                ppmDev.stableFramesSeenCount++;
            } else {
                ppmDev.numChannels = ppmDev.pulseIndex;
            }
        } else {
            ppmDev.stableFramesSeenCount = 0;
        }

        /* Check if the last frame was well formed */
        if (ppmDev.pulseIndex == ppmDev.numChannels && ppmDev.tracking) {
            /* The last frame was well formed */
            for (i = 0; i < ppmDev.numChannels; i++) {
                captures[i] = ppmDev.captures[i];
            }
            for (i = ppmDev.numChannels; i < PPM_IN_MAX_NUM_CHANNELS; i++) {
                captures[i] = PPM_RCVR_TIMEOUT;
            }
            ppmFrameCount++;
        }

        ppmDev.tracking   = true;
        ppmDev.numChannelsPrevFrame = ppmDev.pulseIndex;
        ppmDev.pulseIndex = 0;

        /* We rely on the supervisor to set captureValue to invalid
           if no valid frame is found otherwise we ride over it */
    } else if (ppmDev.tracking) {
        /* Valid pulse duration 0.75 to 2.5 ms*/
        if (ppmDev.deltaTime > PPM_IN_MIN_CHANNEL_PULSE_US
            && ppmDev.deltaTime < PPM_IN_MAX_CHANNEL_PULSE_US
            && ppmDev.pulseIndex < PPM_IN_MAX_NUM_CHANNELS) {
            ppmDev.captures[ppmDev.pulseIndex] = ppmDev.deltaTime;
            ppmDev.pulseIndex++;
        } else {
            /* Not a valid pulse duration */
            ppmDev.tracking = false;
            for (i = 0; i < PWM_PORTS_OR_PPM_CAPTURE_COUNT; i++) {
                ppmDev.captures[i] = PPM_RCVR_TIMEOUT;
            }
        }
    }
}

#define FIRST_PWM_PORT 0

#ifdef USE_PWM_OUTPUT
static void ppmAvoidPWMTimerClash(const timerHardware_t *timHw)
{
    pwmOutputPort_t *motors = pwmGetMotors();
    for (int motorIndex = 0; motorIndex < MAX_SUPPORTED_MOTORS; motorIndex++) {
        if (!motors[motorIndex].enabled || motors[motorIndex].channel.tim != timHw->tim) {
            continue;
        }

        ppmCountDivisor = timerClock(timHw) / (timerGetPrescaler(timHw) + 1);
        return;
    }
}
#endif

void ppmRxInit(const ppmConfig_t *ppmConfig)
{
    ppmResetDevice();

    pwmInputPort_t *port = &pwmInputPorts[FIRST_PWM_PORT];

    const timerHardware_t *timer = timerAllocate(ppmConfig->ioTag, OWNER_PPMINPUT, 0);
    if (!timer) {
        /* TODO: fail here? */
        return;
    }

#ifdef USE_PWM_OUTPUT
    ppmAvoidPWMTimerClash(timer);
#endif

    port->mode = INPUT_MODE_PPM;
    port->timerHardware = timer;

    IO_t io = IOGetByTag(ppmConfig->ioTag);
    IOInit(io, OWNER_PPMINPUT, 0);
    IOConfigGPIOAF(io, IOCFG_AF_PP, timer->alternateFunction);

    timerConfigure(timer, (uint16_t)PPM_TIMER_PERIOD, PWM_TIMER_1MHZ);
    timerChannelEdgeHandlerInit(&port->edgeCb, ppmEdgeCallback);
    timerChannelOverflowHandlerInit(&port->overflowCb, ppmOverflowCallback);
    timerChannelConfigCallbacks(timer, &port->edgeCb, &port->overflowCb);

    const uint8_t filter = (inputFilteringMode == INPUT_FILTERING_ENABLED) ? INPUT_FILTER_TO_HELP_WITH_NOISE_FROM_OPENLRS_TELEMETRY_RX : 0;
    pwmICConfig(timer->tim, timer->channel, TIMER_POLARITY_RISING, filter);
}

uint16_t ppmRead(uint8_t channel)
{
    return captures[channel];
}
#endif
