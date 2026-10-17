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

static void pwmOCConfig(timerResource_t *tim, uint8_t channel, uint16_t value, uint8_t output)
{
    TMR_HandleTypeDef* Handle = timerFindTimerHandle(tim);
    if (Handle == NULL) {
        return;
    }

    TMR_OC_InitTypeDef TMR_OCInitStructure;

    TMR_OCInitStructure.OCMode = TMR_OCMODE_PWM1;
    TMR_OCInitStructure.OCIdleState = TMR_OCIDLESTATE_SET;
    TMR_OCInitStructure.OCPolarity = (output & TIMER_OUTPUT_INVERTED) ? TMR_OCPOLARITY_LOW : TMR_OCPOLARITY_HIGH;
    TMR_OCInitStructure.OCNIdleState = TMR_OCNIDLESTATE_SET;
    TMR_OCInitStructure.OCNPolarity = (output & TIMER_OUTPUT_INVERTED) ? TMR_OCNPOLARITY_LOW : TMR_OCNPOLARITY_HIGH;
    TMR_OCInitStructure.Pulse = value;
    TMR_OCInitStructure.OCFastMode = TMR_OCFAST_DISABLE;

    DAL_TMR_PWM_ConfigChannel(Handle, &TMR_OCInitStructure, channel);
}

void pwmOutputConfig(timerChannel_t *channel, const timerHardware_t *timerHardware, uint32_t hz, uint16_t period, uint16_t value, uint8_t inversion)
{
    TMR_HandleTypeDef* Handle = timerFindTimerHandle(timerHardware->tim);
    if (Handle == NULL) {
        return;
    }

    timerReconfigureTimeBase(timerHardware, period, hz);
    pwmOCConfig(timerHardware->tim,
        timerHardware->channel,
        value,
        inversion ? timerHardware->output ^ TIMER_OUTPUT_INVERTED : timerHardware->output
        );

    if (timerHardware->output & TIMER_OUTPUT_N_CHANNEL) {
        DAL_TMREx_PWMN_Start(Handle, timerHardware->channel);
    } else {
        DAL_TMR_PWM_Start(Handle, timerHardware->channel);
    }
    DAL_TMR_Base_Start(Handle);

    channel->ccr = timerChCCR(timerHardware);

    channel->tim = timerHardware->tim;

    *channel->ccr = 0;
}

void pwmWriteChannel(timerChannel_t *channel, uint32_t value)
{
    *channel->ccr = value;
}

#endif // USE_PWM_OUTPUT
