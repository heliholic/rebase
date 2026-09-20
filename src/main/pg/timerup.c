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

#include "platform.h"

#if defined(USE_TIMER_MGMT) && defined(USE_TIMER_UP_CONFIG)

#include "drivers/dma_reqmap.h"
#include "drivers/timer.h"

#include "timerup.h"

PG_REGISTER_ARRAY_WITH_RESET_FN(timerUpConfig_t, HARDWARE_TIMER_DEFINITION_COUNT, timerUpConfig, PG_TIMER_UP_CONFIG);

PG_RESET_FN(timerUpConfig_t, timerUpConfig)
{
    timerUpConfig->dmaopt = DMA_OPT_UNUSED;

#if defined(TIMUP1_DMA_OPT) && (TIMUP_TIMERS & TIM_N(1))
    if (index == TIMER_INDEX(1)) {
        timerUpConfig->dmaopt = TIMUP1_DMA_OPT;
    }
#endif
#if defined(TIMUP2_DMA_OPT) && (TIMUP_TIMERS & TIM_N(2))
    if (index == TIMER_INDEX(2)) {
        timerUpConfig->dmaopt = TIMUP2_DMA_OPT;
    }
#endif
#if defined(TIMUP3_DMA_OPT) && (TIMUP_TIMERS & TIM_N(3))
    if (index == TIMER_INDEX(3)) {
        timerUpConfig->dmaopt = TIMUP3_DMA_OPT;
    }
#endif
#if defined(TIMUP4_DMA_OPT) && (TIMUP_TIMERS & TIM_N(4))
    if (index == TIMER_INDEX(4)) {
        timerUpConfig->dmaopt = TIMUP4_DMA_OPT;
    }
#endif
#if defined(TIMUP5_DMA_OPT) && (TIMUP_TIMERS & TIM_N(5))
    if (index == TIMER_INDEX(5)) {
        timerUpConfig->dmaopt = TIMUP5_DMA_OPT;
    }
#endif
#if defined(TIMUP6_DMA_OPT) && (TIMUP_TIMERS & TIM_N(6))
    if (index == TIMER_INDEX(6)) {
        timerUpConfig->dmaopt = TIMUP6_DMA_OPT;
    }
#endif
#if defined(TIMUP7_DMA_OPT) && (TIMUP_TIMERS & TIM_N(7))
    if (index == TIMER_INDEX(7)) {
        timerUpConfig->dmaopt = TIMUP7_DMA_OPT;
    }
#endif
#if defined(TIMUP8_DMA_OPT) && (TIMUP_TIMERS & TIM_N(8))
    if (index == TIMER_INDEX(8)) {
        timerUpConfig->dmaopt = TIMUP8_DMA_OPT;
    }
#endif
#if defined(TIMUP15_DMA_OPT) && (TIMUP_TIMERS & TIM_N(15))
    if (index == TIMER_INDEX(15)) {
        timerUpConfig->dmaopt = TIMUP15_DMA_OPT;
    }
#endif
#if defined(TIMUP16_DMA_OPT) && (TIMUP_TIMERS & TIM_N(16))
    if (index == TIMER_INDEX(16)) {
        timerUpConfig->dmaopt = TIMUP16_DMA_OPT;
    }
#endif
#if defined(TIMUP17_DMA_OPT) && (TIMUP_TIMERS & TIM_N(17))
    if (index == TIMER_INDEX(17)) {
        timerUpConfig->dmaopt = TIMUP17_DMA_OPT;
    }
#endif
#if defined(TIMUP20_DMA_OPT) && (TIMUP_TIMERS & TIM_N(20))
    if (index == TIMER_INDEX(20)) {
        timerUpConfig->dmaopt = TIMUP20_DMA_OPT;
    }
#endif
}
#endif
