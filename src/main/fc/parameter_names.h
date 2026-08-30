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

#define PARAM_NAME_GYRO_HARDWARE_LPF "gyro_hardware_lpf"
#define PARAM_NAME_GYRO_LPF1_TYPE "gyro_lpf1_type"
#define PARAM_NAME_GYRO_LPF1_STATIC_HZ "gyro_lpf1_static_hz"
#define PARAM_NAME_GYRO_LPF2_TYPE "gyro_lpf2_type"
#define PARAM_NAME_GYRO_LPF2_STATIC_HZ "gyro_lpf2_static_hz"
#define PARAM_NAME_GYRO_ENABLE_MASK "gyro_enabled_bitmask"
#define PARAM_NAME_ACC_HARDWARE "acc_hardware"
#define PARAM_NAME_ACC_LPF_HZ "acc_lpf_hz"
#define PARAM_NAME_MAG_HARDWARE "mag_hardware"
#define PARAM_NAME_BARO_HARDWARE "baro_hardware"
#define PARAM_NAME_SERIAL_RX_PROVIDER "serialrx_provider"
#define PARAM_NAME_DSHOT_BIDIR "dshot_bidir"
#define PARAM_NAME_USE_UNSYNCED_PWM "use_unsynced_pwm"
#define PARAM_NAME_MOTOR_PWM_PROTOCOL "motor_pwm_protocol"
#define PARAM_NAME_MOTOR_PWM_RATE "motor_pwm_rate"
#define PARAM_NAME_MOTOR_POLES "motor_poles"
#define PARAM_NAME_RATES_TYPE "rates_type"
#define PARAM_NAME_GYRO_CAL_ON_FIRST_ARM "gyro_cal_on_first_arm"
#define PARAM_NAME_PREARM_ALLOW_REARM "prearm_allow_rearm"
#define PARAM_NAME_DEADBAND "deadband"
#define PARAM_NAME_YAW_DEADBAND "yaw_deadband"
#define PARAM_NAME_PID_PROCESS_DENOM "pid_process_denom"
#define PARAM_NAME_DEBUG_MODE "debug_mode"

#define PARAM_NAME_ALTITUDE_SOURCE "altitude_source"
#define PARAM_NAME_ALTITUDE_PREFER_BARO "altitude_prefer_baro"
#define PARAM_NAME_ALTITUDE_LPF "altitude_lpf"
#define PARAM_NAME_ALTITUDE_D_LPF "altitude_d_lpf"


// Velocity-based position control with drag compensation

// Phase 3: Waypoint navigation & yaw control

// Leg-line carrot path tracking and turn-angle cornering

// Phase 5: Velocity buildup

// Turn rate and holding patterns

// Landing sequence

// Flight-plan OSD minimap

// Phase 3: L1 Nonlinear Guidance

#ifdef USE_GPS
#define PARAM_NAME_GPS_PROVIDER "gps_provider"
#define PARAM_NAME_GPS_SBAS_MODE "gps_sbas_mode"
#define PARAM_NAME_GPS_SBAS_INTEGRITY "gps_sbas_integrity"
#define PARAM_NAME_GPS_AUTO_CONFIG "gps_auto_config"
#define PARAM_NAME_GPS_AUTO_BAUD "gps_auto_baud"
#define PARAM_NAME_GPS_UBLOX_USE_GALILEO "gps_ublox_use_galileo"
#define PARAM_NAME_GPS_UBLOX_ACQUIRE_MODEL "gps_ublox_acquire_model"
#define PARAM_NAME_GPS_UBLOX_FLIGHT_MODEL "gps_ublox_flight_model"
#define PARAM_NAME_GPS_UBLOX_UTC_STANDARD "gps_ublox_utc_standard"
#define PARAM_NAME_GPS_UBLOX_ENABLE_ANA "gps_ublox_enable_ana"
#define PARAM_NAME_GPS_SET_HOME_POINT_ONCE "gps_set_home_point_once"
#define PARAM_NAME_GPS_USE_3D_SPEED "gps_use_3d_speed"
#define PARAM_NAME_GPS_NMEA_CUSTOM_COMMANDS "gps_nmea_custom_commands"
#define PARAM_NAME_GPS_UPDATE_RATE_HZ "gps_update_rate_hz"

#endif // USE_GPS

#define PARAM_NAME_IMU_DCM_KP "imu_dcm_kp"
#define PARAM_NAME_IMU_DCM_KI "imu_dcm_ki"
#define PARAM_NAME_IMU_PROCESS_DENOM "imu_process_denom"

#ifdef USE_MAG
#define PARAM_NAME_IMU_MAG_DECLINATION "mag_declination"
#define PARAM_NAME_TRUST_MAG "trust_mag"
#endif

