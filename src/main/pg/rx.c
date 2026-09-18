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

#if defined(USE_RX_PPM) || defined(USE_SERIALRX) || defined(USE_RX_MSP)

#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "rx.h"

#include "common/maths.h"

#include "drivers/io.h"
#include "rx/rx.h"

#ifndef SERIALRX_PROVIDER

#if defined(USE_SERIALRX_CRSF)
#define SERIALRX_PROVIDER SERIALRX_CRSF
#elif defined(USE_SERIALRX_GHST)
#define SERIALRX_PROVIDER SERIALRX_GHST
#elif defined(USE_SERIALRX_IBUS)
#define SERIALRX_PROVIDER SERIALRX_IBUS
#elif defined(USE_SERIALRX_SBUS)
#define SERIALRX_PROVIDER SERIALRX_SBUS
#elif defined(USE_SERIALRX_SPEKTRUM)
#if defined(USE_TELEMETRY_SRXL)
#define SERIALRX_PROVIDER SERIALRX_SRXL
#else
#define SERIALRX_PROVIDER SERIALRX_SPEKTRUM2048
#endif
#elif defined(USE_SERIALRX_FPORT)
#define SERIALRX_PROVIDER SERIALRX_FPORT
#elif defined(USE_SERIALRX_XBUS)
#define SERIALRX_PROVIDER SERIALRX_XBUS_MODE_B
#elif defined(USE_SERIALRX_SRXL2)
#define SERIALRX_PROVIDER SERIALRX_SRXL2
#elif defined(USE_SERIALRX_JETIEXBUS)
#define SERIALRX_PROVIDER SERIALRX_JETIEXBUS
#elif defined(USE_SERIALRX_SUMD)
#define SERIALRX_PROVIDER SERIALRX_SUMD
#elif defined(USE_SERIALRX_SUMH)
#define SERIALRX_PROVIDER SERIALRX_SUMH
#elif defined(USE_SERIALRX_MAVLINK)
#define SERIALRX_PROVIDER SERIALRX_MAVLINK
#else
#define SERIALRX_PROVIDER SERIALRX_NONE
#endif

#endif

#ifndef SERIALRX_HALFDUPLEX
#if (defined(USE_SERIALRX_FPORT) || defined(USE_SERIALRX_SRXL2)) && !(defined(USE_SERIALRX_CRSF) && defined(USE_SERIALRX_GHST) && defined(USE_SERIALRX_IBUS) && defined(USE_SERIALRX_SBUS) && defined(USE_SERIALRX_SPEKTRUM) && defined(USE_SERIALRX_XBUS))
#define SERIALRX_HALFDUPLEX 1
#else
#define SERIALRX_HALFDUPLEX 0
#endif
#endif

PG_REGISTER_WITH_RESET_TEMPLATE(rxConfig_t, rxConfig, PG_RX_CONFIG);

PG_RESET_TEMPLATE(rxConfig_t, rxConfig)
{
    .halfDuplex = SERIALRX_HALFDUPLEX,
    .serialrx_provider = SERIALRX_PROVIDER,
    .serialrx_inverted = 0,
    .spektrum_bind_pin_override_ioTag = IO_TAG(SPEKTRUM_BIND_PIN),
    .spektrum_bind_plug_ioTag = IO_TAG(BINDPLUG_PIN),
    .spektrum_sat_bind = 0,
    .spektrum_sat_bind_autoreset = 1,
    .rx_pulse_min = RX_PWM_PULSE_MIN,   // any control channel below this value will trigger rx loss detection
    .rx_pulse_max = RX_PWM_PULSE_MAX,   // any control channel above this value will trigger rx loss detection
    .rssi_src_frame_errors = false,
    .rssi_channel = 0,
    .rssi_scale = RSSI_SCALE_DEFAULT,
    .rssi_offset = 0,
    .rssi_invert = 0,
    .rssi_src_frame_lpf_period = 30,
    .rssi_smoothing = 125,
    .max_rc_channel = DEFAULT_RC_CHANNEL_COUNT,
    .srxl2_unit_id = 1,
    .srxl2_baud_fast = true,
    .sbus_baud_fast = false,
    .crsf_use_negotiated_baud = false,
    .rc_channel = {
        [RC_ROLL]       = 0,
        [RC_PITCH]      = 1,
        [RC_YAW]        = 3,
        [RC_COLLECTIVE] = 4,
        [RC_THROTTLE]   = 2,
        [RC_ARM]        = 5,
    },
};

PG_REGISTER_ARRAY_WITH_RESET_TEMPLATE(rxChannelRangeConfig_t, RC_CONTROL_COUNT, rxChannelRangeConfigs, PG_RX_CHANNEL_RANGE_CONFIG);

// default calibration is full range and a 1:1 mapping
PG_RESET_TEMPLATE(rxChannelRangeConfig_t, rxChannelRangeConfigs)
{
    .min = PWM_RANGE_MIN,
    .max = PWM_RANGE_MAX,
};

PG_REGISTER_ARRAY_WITH_RESET_FN(rxFailsafeChannelConfig_t, MAX_SUPPORTED_RC_CHANNEL_COUNT, rxFailsafeChannelConfigs, PG_RX_FAILSAFE_CHANNEL_CONFIG);

// AUTO resolves through the control map in getRxfailValue(), and falls through
// to HOLD on a channel that carries no control function, so the mode no longer
// depends on the channel's position in the array.
PG_RESET_FN(rxFailsafeChannelConfig_t, rxFailsafeChannelConfigs)
{
    rxFailsafeChannelConfigs->failsafe_mode = RX_FAILSAFE_MODE_AUTO;
    rxFailsafeChannelConfigs->failsafe_step = CHANNEL_VALUE_TO_RXFAIL_STEP(RX_PWM_PULSE_MID);
}

#endif
