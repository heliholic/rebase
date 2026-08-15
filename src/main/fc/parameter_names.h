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
#define PARAM_NAME_MIXER_TYPE "mixer_type"
#define PARAM_NAME_LANDING_DISARM_THRESHOLD "landing_disarm_threshold"
#define PARAM_NAME_GYRO_CAL_ON_FIRST_ARM "gyro_cal_on_first_arm"
#define PARAM_NAME_PREARM_ALLOW_REARM "prearm_allow_rearm"
#define PARAM_NAME_DEADBAND "deadband"
#define PARAM_NAME_YAW_DEADBAND "yaw_deadband"
#define PARAM_NAME_PID_PROCESS_DENOM "pid_process_denom"
#define PARAM_NAME_DTERM_LPF1_TYPE "dterm_lpf1_type"
#define PARAM_NAME_DTERM_LPF1_STATIC_HZ "dterm_lpf1_static_hz"
#define PARAM_NAME_DTERM_LPF2_TYPE "dterm_lpf2_type"
#define PARAM_NAME_DTERM_LPF2_STATIC_HZ "dterm_lpf2_static_hz"
#define PARAM_NAME_DTERM_NOTCH_HZ "dterm_notch_hz"
#define PARAM_NAME_DTERM_NOTCH_CUTOFF "dterm_notch_cutoff"
#define PARAM_NAME_PID_AT_MIN_THROTTLE "pid_at_min_throttle"
#define PARAM_NAME_ACC_LIMIT_YAW "acc_limit_yaw"
#define PARAM_NAME_ACC_LIMIT "acc_limit"
#define PARAM_NAME_ITERM_WINDUP "iterm_windup"
#define PARAM_NAME_PIDSUM_LIMIT "pidsum_limit"
#define PARAM_NAME_PIDSUM_LIMIT_YAW "pidsum_limit_yaw"
#define PARAM_NAME_DEBUG_MODE "debug_mode"

#define PARAM_NAME_ALTITUDE_SOURCE "altitude_source"
#define PARAM_NAME_ALTITUDE_PREFER_BARO "altitude_prefer_baro"
#define PARAM_NAME_ALTITUDE_LPF "altitude_lpf"
#define PARAM_NAME_ALTITUDE_D_LPF "altitude_d_lpf"

#define PARAM_NAME_AP_LANDING_ALTITUDE_M "ap_landing_altitude_m"
#define PARAM_NAME_AP_HOVER_THROTTLE "ap_hover_throttle"
#define PARAM_NAME_AP_THROTTLE_MIN "ap_throttle_min"
#define PARAM_NAME_AP_THROTTLE_MAX "ap_throttle_max"
#define PARAM_NAME_AP_ALTITUDE_P "ap_altitude_p"
#define PARAM_NAME_AP_ALTITUDE_I "ap_altitude_i"
#define PARAM_NAME_AP_ALTITUDE_D "ap_altitude_d"
#define PARAM_NAME_AP_ALTITUDE_F "ap_altitude_f"
#define PARAM_NAME_AP_POSITION_P "ap_position_p"
#define PARAM_NAME_AP_POSITION_I "ap_position_i"
#define PARAM_NAME_AP_POSITION_D "ap_position_d"
#define PARAM_NAME_AP_POSITION_A "ap_position_a"
#define PARAM_NAME_AP_POSITION_F "ap_position_f"
#define PARAM_NAME_AP_POSITION_CUTOFF "ap_position_cutoff"
#define PARAM_NAME_AP_STOP_THRESHOLD "ap_stop_threshold"
#define PARAM_NAME_AP_MAX_ANGLE "ap_max_angle"

// Velocity-based position control with drag compensation
#define PARAM_NAME_AP_VELOCITY_DRAG_COEFF "ap_velocity_drag_coeff"
#define PARAM_NAME_AP_MAX_VELOCITY "ap_max_velocity"

// Phase 3: Waypoint navigation & yaw control
#define PARAM_NAME_AP_WAYPOINT_ARRIVAL_RADIUS "ap_waypoint_arrival_radius"
#define PARAM_NAME_AP_WAYPOINT_HOLD_RADIUS "ap_waypoint_hold_radius"
#define PARAM_NAME_AP_STICK_DEADBAND "ap_stick_deadband"
#define PARAM_NAME_AP_THROTTLE_DEADBAND "ap_throttle_deadband"
#define PARAM_NAME_AP_YAW_MODE "ap_yaw_mode"
#define PARAM_NAME_AP_YAW_P "ap_yaw_p"
#define PARAM_NAME_AP_YAW_D "ap_yaw_d"
#define PARAM_NAME_AP_MAX_YAW_RATE "ap_max_yaw_rate"
#define PARAM_NAME_AP_MIN_FORWARD_VELOCITY "ap_min_forward_velocity"

// Leg-line carrot path tracking and turn-angle cornering
#define PARAM_NAME_AP_NAV_CORNER_SPEED "ap_nav_corner_speed"
#define PARAM_NAME_AP_NAV_CORNER_DELTA_V "ap_nav_corner_delta_v"
#define PARAM_NAME_AP_NAV_DECEL "ap_nav_decel"
#define PARAM_NAME_AP_NAV_ACCEL "ap_nav_accel"
#define PARAM_NAME_AP_NAV_CARROT_LEAD_TIME "ap_nav_carrot_lead_time"
#define PARAM_NAME_AP_NAV_CARROT_LEAD_MAX "ap_nav_carrot_lead_max"
#define PARAM_NAME_AP_NAV_PRETURN_DIST "ap_nav_preturn_dist"

// Phase 5: Velocity buildup
#define PARAM_NAME_AP_VELOCITY_BUILDUP_MAX_PITCH "ap_velocity_buildup_max_pitch"

// Turn rate and holding patterns
#define PARAM_NAME_AP_MAX_TURN_RATE "ap_max_turn_rate"
#define PARAM_NAME_AP_HOLD_ORBIT_RADIUS "ap_hold_orbit_radius"
#define PARAM_NAME_AP_HOLD_FIGURE8_WIDTH "ap_hold_figure8_width"

// Landing sequence
#define PARAM_NAME_AP_LANDING_DESCENT_RATE "ap_landing_descent_rate"
#define PARAM_NAME_AP_LANDING_DETECTION_TIME "ap_landing_detection_time"
#define PARAM_NAME_AP_LANDING_SPIRAL_ENABLE "ap_landing_spiral_enable"
#define PARAM_NAME_AP_LANDING_SPIRAL_RADIUS "ap_landing_spiral_radius"
#define PARAM_NAME_AP_LANDING_SPIRAL_RATE "ap_landing_spiral_rate"
#define PARAM_NAME_AP_LANDING_VELOCITY_THRESHOLD "ap_landing_velocity_threshold"
#define PARAM_NAME_AP_LANDING_THROTTLE_THRESHOLD "ap_landing_throttle_threshold"
#define PARAM_NAME_AP_MIN_NAV_ALTITUDE_M "ap_min_nav_altitude_m"
#define PARAM_NAME_AP_RX_LOSS_POLICY "ap_rx_loss_policy"
#define PARAM_NAME_AP_MAX_DISTANCE_FROM_HOME "ap_max_distance_from_home"
#define PARAM_NAME_AP_GEOFENCE_ACTION "ap_geofence_action"

// Flight-plan OSD minimap
#define PARAM_NAME_OSD_NAV_MAP_MODE "osd_nav_map_mode"
#define PARAM_NAME_OSD_NAV_MAP_CENTRE "osd_nav_map_centre"
#define PARAM_NAME_OSD_NAV_MAP_MIN_SCALE_M "osd_nav_map_min_scale_m"

// Phase 3: L1 Nonlinear Guidance
#define PARAM_NAME_AP_L1_ENABLE "ap_l1_enable"
#define PARAM_NAME_AP_L1_PERIOD "ap_l1_period"
#define PARAM_NAME_AP_L1_MIN_LOOKAHEAD "ap_l1_min_lookahead"
#define PARAM_NAME_AP_L1_MAX_LOOKAHEAD "ap_l1_max_lookahead"
#define PARAM_NAME_AP_L1_MAX_CROSS_TRACK_ERROR "ap_l1_max_cross_track_error"
#define PARAM_NAME_AP_L1_TURN_RATE "ap_l1_turn_rate"

#define PARAM_NAME_ANGLE_LIMIT "angle_limit"
#define PARAM_NAME_S_PITCH "s_pitch"
#define PARAM_NAME_S_ROLL "s_roll"
#define PARAM_NAME_S_YAW "s_yaw"
#define PARAM_NAME_ANGLE_P_GAIN "angle_p_gain"
#define PARAM_NAME_ANGLE_EARTH_REF "angle_earth_ref"
#define PARAM_NAME_ANGLE_PITCH_OFFSET "angle_pitch_offset"

#define PARAM_NAME_HORIZON_LEVEL_STRENGTH "horizon_level_strength"
#define PARAM_NAME_HORIZON_LIMIT_DEGREES "horizon_limit_degrees"
#define PARAM_NAME_HORIZON_LIMIT_STICKS "horizon_limit_sticks"
#define PARAM_NAME_HORIZON_IGNORE_STICKS "horizon_ignore_sticks"
#define PARAM_NAME_HORIZON_DELAY_MS "horizon_delay_ms"

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

#ifdef USE_GPS_RESCUE
#define PARAM_NAME_GPS_RESCUE_MIN_START_DIST "gps_rescue_min_start_dist"
#define PARAM_NAME_GPS_RESCUE_ALT_MODE "gps_rescue_alt_mode"
#define PARAM_NAME_GPS_RESCUE_INITIAL_CLIMB "gps_rescue_initial_climb"
#define PARAM_NAME_GPS_RESCUE_ASCEND_RATE "gps_rescue_ascend_rate"

#define PARAM_NAME_GPS_RESCUE_RETURN_ALT "gps_rescue_return_alt"
#define PARAM_NAME_GPS_RESCUE_GROUND_SPEED "gps_rescue_ground_speed"

#define PARAM_NAME_GPS_RESCUE_DESCENT_DIST "gps_rescue_descent_dist"
#define PARAM_NAME_GPS_RESCUE_DESCEND_RATE "gps_rescue_descend_rate"
#define PARAM_NAME_GPS_RESCUE_DISARM_THRESHOLD "gps_rescue_disarm_threshold"

#define PARAM_NAME_GPS_RESCUE_SANITY_CHECKS "gps_rescue_sanity_checks"
#define PARAM_NAME_GPS_RESCUE_MIN_SATS "gps_rescue_min_sats"
#define PARAM_NAME_GPS_RESCUE_ALLOW_ARMING_WITHOUT_FIX "gps_rescue_allow_arming_without_fix"

#define PARAM_NAME_GPS_RESCUE_YAW_P "gps_rescue_yaw_p"
#endif // USE_GPS_RESCUE
#endif // USE_GPS

#ifdef USE_ALTITUDE_HOLD
#define PARAM_NAME_ALT_HOLD_DEADBAND "alt_hold_deadband"
#define PARAM_NAME_ALT_HOLD_CLIMB_RATE "alt_hold_climb_rate"
#endif

#ifdef USE_POSITION_HOLD
#define PARAM_NAME_POS_HOLD_DEADBAND "pos_hold_deadband"
#endif

#define PARAM_NAME_IMU_DCM_KP "imu_dcm_kp"
#define PARAM_NAME_IMU_DCM_KI "imu_dcm_ki"
#define PARAM_NAME_IMU_PROCESS_DENOM "imu_process_denom"

#ifdef USE_MAG
#define PARAM_NAME_IMU_MAG_DECLINATION "mag_declination"
#define PARAM_NAME_TRUST_MAG "trust_mag"
#endif

