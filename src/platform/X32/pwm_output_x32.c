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

static void pwmOCConfig(TIM_TypeDef *tim, uint8_t channel, uint16_t value, uint8_t output)
{
    TIM_OCInitTypeDef ocInit;
    TIM_OCStructInit(&ocInit);
    ocInit.OCMode = TIM_OCMODE_PWM1;

    if (output & TIMER_OUTPUT_N_CHANNEL) {
        ocInit.OutputNState = TIM_OUTPUT_NSTATE_ENABLE;
        ocInit.OCNIdleState = TIM_OCN_IDLE_STATE_RESET;
        ocInit.OCNPolarity = (output & TIMER_OUTPUT_INVERTED) ? TIM_OCN_POLARITY_LOW : TIM_OCN_POLARITY_HIGH;
    } else {
        ocInit.OutputState = TIM_OUTPUT_STATE_ENABLE;
        ocInit.OCIdleState = TIM_OC_IDLE_STATE_SET;
        ocInit.OCPolarity = (output & TIMER_OUTPUT_INVERTED) ? TIM_OC_POLARITY_LOW : TIM_OC_POLARITY_HIGH;
    }

    ocInit.Pulse = value;
    timerOCInit(tim, channel, &ocInit);
    timerOCPreloadConfig(tim, channel, TIM_OC_PRE_LOAD_ENABLE);
}

void pwmOutputConfig(timerChannel_t *channel, const timerHardware_t *timerHardware, uint32_t hz, uint16_t period, uint16_t value, uint8_t inversion)
{
    timerReconfigureTimeBase(timerHardware, period, hz);
    pwmOCConfig((TIM_TypeDef *)timerHardware->tim,
        timerHardware->channel,
        value,
        inversion ? (timerHardware->output ^ TIMER_OUTPUT_INVERTED) : timerHardware->output);
    timerStart(timerHardware);

    channel->ccr = timerChCCR(timerHardware);
    channel->tim = timerHardware->tim;
}

void pwmWriteChannel(timerChannel_t *channel, uint32_t value)
{
    *channel->ccr = value;
}

#endif
