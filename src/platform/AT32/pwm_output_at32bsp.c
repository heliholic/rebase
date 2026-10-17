/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
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

#include "platform.h"

#ifdef USE_PWM_OUTPUT

#include "drivers/io.h"
#include "drivers/pwm_output.h"
#include "drivers/pwm_output_impl.h"
#include "drivers/time.h"
#include "drivers/timer.h"
#include "platform/timer.h"

static void pwmOCConfig(tmr_type *tim, uint8_t channel, uint16_t value, uint8_t output)
{
    tmr_output_config_type  tmr_OCInitStruct;
    tmr_output_default_para_init(&tmr_OCInitStruct);
    tmr_OCInitStruct.oc_mode= TMR_OUTPUT_CONTROL_PWM_MODE_A;

    if (output & TIMER_OUTPUT_N_CHANNEL) {
        tmr_OCInitStruct.occ_output_state = TRUE;
        tmr_OCInitStruct.occ_idle_state = FALSE;
        tmr_OCInitStruct.occ_polarity =  (output & TIMER_OUTPUT_INVERTED) ? TMR_OUTPUT_ACTIVE_LOW : TMR_OUTPUT_ACTIVE_HIGH;
    } else {
        tmr_OCInitStruct.oc_output_state = TRUE;
        tmr_OCInitStruct.oc_idle_state = TRUE;
        tmr_OCInitStruct.oc_polarity =  (output & TIMER_OUTPUT_INVERTED) ? TMR_OUTPUT_ACTIVE_LOW : TMR_OUTPUT_ACTIVE_HIGH;
    }
    tmr_channel_value_set(tim, (channel-1)*2, value);
    tmr_output_channel_config(tim,(channel-1)*2, &tmr_OCInitStruct);
    tmr_output_channel_buffer_enable(tim, ((channel-1)*2),TRUE);
}

void pwmOutputConfig(timerChannel_t *channel, const timerHardware_t *timerHardware, uint32_t hz, uint16_t period, uint16_t value, uint8_t inversion)
{
    timerReconfigureTimeBase(timerHardware, period, hz);
    pwmOCConfig((tmr_type *)timerHardware->tim,
        timerHardware->channel,
        value,
        inversion ? timerHardware->output ^ TIMER_OUTPUT_INVERTED : timerHardware->output
        );

    tmr_output_enable((tmr_type *)timerHardware->tim, TRUE);
    tmr_counter_enable((tmr_type *)timerHardware->tim, TRUE);

    channel->ccr = timerChCCR(timerHardware);

    channel->tim = timerHardware->tim;

    *channel->ccr = 0;
}

void pwmWriteChannel(timerChannel_t *channel, uint32_t value)
{
    *channel->ccr = value;
}

#endif // USE_PWM_OUTPUT
