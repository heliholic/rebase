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

#pragma once

#include "drivers/io_types.h"

#include "pg/pg.h"

#define MAX_SUPPORTED_RC_CHANNEL_COUNT      18

// Control functions. rcControls[] is indexed by these, and rxConfig.rc_channel[]
// names the receiver channel that carries each one.
typedef enum {
    RC_ROLL = 0,
    RC_PITCH,
    RC_YAW,
    RC_COLLECTIVE,
    RC_THROTTLE,
    RC_ARM,
    RC_CONTROL_COUNT,
} rcControl_e;


#define GET_FRAME_ERR_LPF_FREQUENCY(period) (10.0f / period)  // period in deciseconds (0.1s)
#define FRAME_ERR_RESAMPLE_US 100000

typedef struct rxConfig_s {
    uint8_t rc_channel[RC_CONTROL_COUNT];      // receiver channel carrying each control function
    uint8_t serialrx_provider;                 // type of UART-based receiver (0 = spek 10, 1 = spek 11, 2 = sbus). Must be enabled by FEATURE_RX_SERIAL first.
    bool serialrx_inverted;                    // invert the serial RX protocol compared to it's default setting
    bool halfDuplex;                           // allow rx to operate in half duplex mode on F4, ignored for F1 and F3.
    ioTag_t spektrum_bind_pin_override_ioTag;
    ioTag_t spektrum_bind_plug_ioTag;
    uint8_t spektrum_sat_bind;                 // number of bind pulses for Spektrum satellite receivers
    bool spektrum_sat_bind_autoreset;          // whenever we will reset (exit) binding mode after hard reboot
    uint8_t rssi_channel;
    uint8_t rssi_scale;
    bool rssi_invert;
    uint16_t rx_pulse_min;                     // Absolute minimum pulse accepted
    uint16_t rx_pulse_max;                     // Absolute maximum pulse accepted
    uint8_t max_rc_channel;                    // number of receiver channels to process
    bool rssi_src_frame_errors;                // true to use frame drop flags in the rx protocol
    int8_t rssi_offset;                        // offset applied to the RSSI value before it is returned
    uint8_t rssi_src_frame_lpf_period;         // Period of the cutoff frequency for the source frame RSSI filter (in 0.1 s)
    uint8_t rssi_smoothing;                    // Smoothing factor to reduce jumpiness of rssi, rssiDbm and rsnr
    uint8_t srxl2_unit_id;                     // Spektrum SRXL2 RX unit id
    bool srxl2_baud_fast;                      // Select Spektrum SRXL2 fast baud rate
    bool sbus_baud_fast;                       // Select SBus fast baud rate
    bool crsf_use_negotiated_baud;             // Use negotiated baud rate for CRSF V3
} rxConfig_t;

PG_DECLARE(rxConfig_t, rxConfig);

typedef struct {
    uint8_t failsafe_mode;  // See rxFailsafeChannelMode_e
    uint8_t failsafe_step;  // Fixed value, in RXFAIL steps
} rxFailsafeChannelConfig_t;

PG_DECLARE_ARRAY(rxFailsafeChannelConfig_t, MAX_SUPPORTED_RC_CHANNEL_COUNT, rxFailsafeChannelConfigs);

typedef struct {
    uint16_t min;
    uint16_t max;
} rxChannelRangeConfig_t;

PG_DECLARE_ARRAY(rxChannelRangeConfig_t, RC_CONTROL_COUNT, rxChannelRangeConfigs);
