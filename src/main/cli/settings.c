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

#include "build/debug.h"

#include "blackbox/blackbox.h"
#include "blackbox/blackbox_fielddefs.h"

#include "common/utils.h"
#include "common/rtc.h"

#include "drivers/adc.h"
#include "drivers/bus_i2c.h"
#include "drivers/bus_spi.h"
#include "drivers/dshot_command.h"
#include "drivers/light_led.h"
#include "drivers/mco.h"
#include "drivers/pinio.h"
#include "drivers/sdio.h"

#include "config/config.h"
#include "fc/controlrate_profile.h"
#include "fc/core.h"
#include "fc/parameter_names.h"
#include "controls/rc.h"
#include "fc/rc_adjustments.h"
#include "fc/rc_controls.h"

#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/mixer.h"
#include "flight/pid.h"
#include "flight/position.h"
#include "fc/servos.h"

#include "io/beeper.h"
#include "io/dashboard.h"
#include "io/gps.h"
#include "io/ledstrip.h"
#include "io/serial.h"

#include "pg/adc.h"
#include "pg/beeper.h"
#include "pg/beeper_dev.h"
#include "pg/bus_i2c.h"
#include "pg/can.h"
#include "pg/dronecan.h"
#include "pg/dashboard.h"
#include "pg/flash.h"
#include "pg/gyrodev.h"
#include "pg/mco.h"
#include "pg/motor.h"
#include "pg/msp.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/pilot.h"
#include "pg/pinio.h"
#include "pg/piniobox.h"
#include "pg/rx.h"
#include "pg/sdcard.h"
#include "pg/usb.h"
#include "pg/scheduler.h"
#include "pg/sdio.h"
#include "pg/stats.h"
#include "pg/board.h"

#include "rx/crsf.h"
#include "rx/rx.h"
#include "rx/spektrum.h"

#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/battery.h"
#include "sensors/boardalignment.h"
#include "sensors/compass.h"
#include "sensors/esc_sensor.h"
#include "sensors/gyro.h"
#include "sensors/rangefinder.h"
#include "sensors/opticalflow.h"
#include "sensors/sensors.h"

#include "scheduler/scheduler.h"

#include "telemetry/frsky_hub.h"
#include "telemetry/telemetry.h"

#include "settings.h"

const char * const lookupTableOffOn[] = {
    "OFF", "ON"
};

#ifdef USE_DSHOT_TELEMETRY
static const char * const lookupTableDshotEdt[] = {
    "OFF", "ON", "FORCE"
};
#endif

static const char * const lookupTableUnit[] = {
    "IMPERIAL", "METRIC", "BRITISH"
};

static const char * const lookupTableAlignment[] = {
    "DEFAULT",
    "CW0",
    "CW90",
    "CW180",
    "CW270",
    "CW0FLIP",
    "CW90FLIP",
    "CW180FLIP",
    "CW270FLIP",
    "CUSTOM",
};

#ifdef USE_GPS
static const char * const lookupTableGpsProvider[] = {
    "NMEA", "UBLOX", "MSP", "VIRTUAL",
#if ENABLE_DRONECAN
    "DRONECAN",
#endif
};

static const char * const lookupTableGpsSbasMode[] = {
    "AUTO", "EGNOS", "WAAS", "MSAS", "GAGAN", "NONE"
};

static const char * const lookupTableGpsUbloxModels[] = {
    "PORTABLE", "STATIONARY", "PEDESTRIAN", "AUTOMOTIVE", "AT_SEA", "AIRBORNE_1G", "AIRBORNE_2G", "AIRBORNE_4G"
};

static const char * const lookupTableGpsUbloxUtcStandard[] = {
    "AUTO", "USNO", "EU", "SU", "NTSC"
};
#endif

#ifdef USE_BLACKBOX
static const char * const lookupTableBlackboxDevice[] = {
    "NONE",
    "SPIFLASH",
    "SDCARD",
    "SERIAL",
#ifdef USE_BLACKBOX_VIRTUAL
    "VIRTUAL",
#endif
};

static const char * const lookupTableBlackboxMode[] = {
    "OFF",
    "NORMAL",
    "ARMED",
    "SWITCH",
};
#endif

#ifdef USE_SERIALRX
static const char * const lookupTableSerialRX[] = {
    "NONE",
    "SPEK2048",
    "SBUS",
    "SUMD",
    "SUMH",
    "XB-B",
    "XB-B-RJ01",
    "IBUS",
    "JETIEXBUS",
    "CRSF",
    "SRXL",
    NULL, // CUSTOM (removed)
    "FPORT",
    "SRXL2",
    NULL, // GHST (removed)
    "SPEK1024",
    "MAVLINK",
};
#endif

static const char * const lookupTableGyroHardwareLpf[] = {
    "NORMAL",
    "OPTION_1",
    "OPTION_2",
#ifdef USE_GYRO_DLPF_EXPERIMENTAL
    "EXPERIMENTAL",
#endif
};

static const char * const lookupTablePwmProtocol[] = {
    "DISABLED",
    "PWM", "ONESHOT125", "ONESHOT42", "MULTISHOT", "BRUSHED",
    "DSHOT150", "DSHOT300", "DSHOT600", "PROSHOT1000",
    "DRONECAN"
};

static const char * const lookupTableLowpassType[] = {
    "PT1",
    "BIQUAD",
    "PT2",
    "PT3",
};

static const char * const lookupTableDtermLowpassType[] = {
    "PT1",
    "BIQUAD",
    "PT2",
    "PT3",
};

static const char * const lookupTableFailsafe[] = {
    "AUTO-LAND", "DROP", "GPS-RESCUE"
};

static const char * const lookupTableFailsafeSwitchMode[] = {
    "STAGE1", "KILL", "STAGE2"
};

static const char * const lookupTableBusType[] = {
    "NONE", "I2C", "SPI", "SLAVE",
#if defined(USE_SPI_GYRO) && defined(USE_I2C_GYRO)
    "GYROAUTO"
#endif
};


#ifdef USE_GYRO_OVERFLOW_CHECK
static const char * const lookupTableGyroOverflowCheck[] = {
    "OFF", "YAW", "ALL"
};
#endif

static const char * const lookupTableRatesType[] = {
    "BETAFLIGHT", "RACEFLIGHT", "KISS", "ACTUAL", "QUICK"
};

#ifdef USE_OVERCLOCK
static const char * const lookupOverclock[] = {
    "OFF",
#if ENABLE_OVERCLOCK_108_MHZ
    "108MHZ",
#endif
#if ENABLE_OVERCLOCK_120_MHZ
    "120MHZ",
#endif
#if ENABLE_OVERCLOCK_192_MHZ
    "192MHZ",
#endif
#if ENABLE_OVERCLOCK_216_MHZ
    "216MHZ",
#endif
#if ENABLE_OVERCLOCK_240_MHZ
    "240MHZ",
#endif
};
#endif

#ifdef USE_LED_STRIP
    static const char * const lookupLedStripFormatRGB[] = {
        "GRB", "RGB", "GRBW"
    };
#endif

#ifdef USE_SDCARD
static const char * const lookupTableSdcardMode[] = {
    "OFF", "SPI", "SDIO"
};
#endif

#ifdef USE_LED_STRIP
#ifdef USE_LED_STRIP_STATUS_MODE
static const char * const lookupTableLEDProfile[] = {
    "RACE", "BEACON", "STATUS"
};
#else
static const char * const lookupTableLEDProfile[] = {
    "RACE", "BEACON"
};
#endif
#endif

const char * const lookupTableLedstripColors[COLOR_COUNT] = {
    "BLACK",
    "WHITE",
    "RED",
    "ORANGE",
    "YELLOW",
    "LIME_GREEN",
    "GREEN",
    "MINT_GREEN",
    "CYAN",
    "LIGHT_BLUE",
    "BLUE",
    "DARK_VIOLET",
    "MAGENTA",
    "DEEP_PINK"
};

static const char * const lookupTablePositionAltitudeSource[] = {
    "DEFAULT", "BARO_ONLY", "GPS_ONLY", "RANGEFINDER_PREFER", "RANGEFINDER_ONLY"
};

static const char * const lookupTableOffOnAuto[] = {
    "OFF", "ON", "AUTO"
};

static const char* const lookupTableDshotBitbangedTimer[] = {
    "AUTO", "TIM1", "TIM8"
};

#define LOOKUP_TABLE_ENTRY(name) { name, ARRAYLEN(name) }

const lookupTableEntry_t lookupTables[] = {
    LOOKUP_TABLE_ENTRY(lookupTableOffOn),
    LOOKUP_TABLE_ENTRY(lookupTableUnit),
    LOOKUP_TABLE_ENTRY(lookupTableAlignment),
#ifdef USE_GPS
    LOOKUP_TABLE_ENTRY(lookupTableGpsProvider),
    LOOKUP_TABLE_ENTRY(lookupTableGpsSbasMode),
    LOOKUP_TABLE_ENTRY(lookupTableGpsUbloxModels),
    LOOKUP_TABLE_ENTRY(lookupTableGpsUbloxUtcStandard),
#endif
#ifdef USE_BLACKBOX
    LOOKUP_TABLE_ENTRY(lookupTableBlackboxDevice),
    LOOKUP_TABLE_ENTRY(lookupTableBlackboxMode),
#endif
    LOOKUP_TABLE_ENTRY(currentMeterSourceNames),
    LOOKUP_TABLE_ENTRY(voltageMeterSourceNames),
#ifdef USE_SERIALRX
    LOOKUP_TABLE_ENTRY(lookupTableSerialRX),
#endif
    LOOKUP_TABLE_ENTRY(lookupTableGyroHardwareLpf),
    { lookupTableAccHardware, ACC_HARDWARE_COUNT },
#ifdef USE_BARO
    { lookupTableBaroHardware, BARO_HARDWARE_COUNT },
#endif
#ifdef USE_MAG
    { lookupTableMagHardware, MAG_HARDWARE_COUNT },
#endif
    LOOKUP_TABLE_ENTRY(debugModeNames),
    LOOKUP_TABLE_ENTRY(lookupTablePwmProtocol),
#ifdef USE_DSHOT_TELEMETRY
    LOOKUP_TABLE_ENTRY(lookupTableDshotEdt),
#endif
    LOOKUP_TABLE_ENTRY(lookupTableLowpassType),
    LOOKUP_TABLE_ENTRY(lookupTableDtermLowpassType),
    LOOKUP_TABLE_ENTRY(lookupTableFailsafe),
    LOOKUP_TABLE_ENTRY(lookupTableFailsafeSwitchMode),
    LOOKUP_TABLE_ENTRY(lookupTableBusType),
#ifdef USE_RANGEFINDER
    { lookupTableRangefinderHardware, RANGEFINDER_HARDWARE_COUNT },
#endif
#ifdef USE_OPTICALFLOW
    { lookupTableOpticalflowHardware, OPTICALFLOW_HARDWARE_COUNT },
#endif
#ifdef USE_GYRO_OVERFLOW_CHECK
    LOOKUP_TABLE_ENTRY(lookupTableGyroOverflowCheck),
#endif
    LOOKUP_TABLE_ENTRY(lookupTableRatesType),
#ifdef USE_OVERCLOCK
    LOOKUP_TABLE_ENTRY(lookupOverclock),
#endif
#ifdef USE_LED_STRIP
    LOOKUP_TABLE_ENTRY(lookupLedStripFormatRGB),
#endif
#ifdef USE_SDCARD
    LOOKUP_TABLE_ENTRY(lookupTableSdcardMode),
#endif
#ifdef USE_LED_STRIP
    LOOKUP_TABLE_ENTRY(lookupTableLEDProfile),
    LOOKUP_TABLE_ENTRY(lookupTableLedstripColors),
#endif

    LOOKUP_TABLE_ENTRY(lookupTablePositionAltitudeSource),
    LOOKUP_TABLE_ENTRY(lookupTableOffOnAuto),
    LOOKUP_TABLE_ENTRY(lookupTableDshotBitbangedTimer),

};

#undef LOOKUP_TABLE_ENTRY

const clivalue_t valueTable[] = {
// PG_GYRO_CONFIG
    { PARAM_NAME_GYRO_HARDWARE_LPF, VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GYRO_HARDWARE_LPF }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_hardware_lpf) },

#if defined(USE_GYRO_SPI_ICM20649)
    { "gyro_high_range",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_high_fsr) },
#endif

    { PARAM_NAME_GYRO_LPF1_TYPE,      VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GYRO_LPF_TYPE }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_lpf1_type) },
    { PARAM_NAME_GYRO_LPF1_STATIC_HZ, VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, LPF_MAX_HZ }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_lpf1_static_hz) },

    { PARAM_NAME_GYRO_LPF2_TYPE,      VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GYRO_LPF_TYPE }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_lpf2_type) },
    { PARAM_NAME_GYRO_LPF2_STATIC_HZ, VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0,  LPF_MAX_HZ }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_lpf2_static_hz) },

    { "gyro_notch1_hz",             VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, LPF_MAX_HZ }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_hz_1) },
    { "gyro_notch1_cutoff",         VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, LPF_MAX_HZ }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_cutoff_1) },
    { "gyro_notch2_hz",             VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, LPF_MAX_HZ }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_hz_2) },
    { "gyro_notch2_cutoff",         VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, LPF_MAX_HZ }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_soft_notch_cutoff_2) },

    { "gyro_calib_duration",        VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 50,  3000 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyroCalibrationDuration) },
    { "gyro_calib_noise_limit",     VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0,  200 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyroMovementCalibrationThreshold) },
    { "gyro_offset_yaw",            VAR_INT16  | MASTER_VALUE, .config.minmax = { -1000, 1000 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_offset_yaw) },
#ifdef USE_GYRO_OVERFLOW_CHECK
    { "gyro_overflow_detect",       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GYRO_OVERFLOW_CHECK }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, checkOverflow) },
#endif

#if GYRO_COUNT > 1
    { PARAM_NAME_GYRO_ENABLE_MASK,  VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, (1 << GYRO_COUNT) - 1 }, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
    { "gyro_1_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 0, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
    { "gyro_2_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 1, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
#endif
#if GYRO_COUNT > 2
    { "gyro_3_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 2, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
#endif
#if GYRO_COUNT > 3
    { "gyro_4_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 3, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
#endif
#if GYRO_COUNT > 4
    { "gyro_5_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 4, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
#endif
#if GYRO_COUNT > 5
    { "gyro_6_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 5, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
#endif
#if GYRO_COUNT > 6
    { "gyro_7_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 6, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
#endif
#if GYRO_COUNT > 7
    { "gyro_8_enabled",             VAR_UINT8  | HARDWARE_VALUE | MODE_BITSET, .config.bitpos = 7, PG_GYRO_CONFIG, offsetof(gyroConfig_t, gyro_enabled_bitmask) },
#endif // GYRO_COUNT
// PG_ACCELEROMETER_CONFIG
#if defined(USE_ACC)
    { PARAM_NAME_ACC_HARDWARE,      VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_ACC_HARDWARE }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, acc_hardware) },
#if defined(USE_GYRO_SPI_ICM20649)
    { "acc_high_range",             VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, acc_high_fsr) },
#endif
    { PARAM_NAME_ACC_LPF_HZ,        VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 500 }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, acc_lpf_hz) },
    { "acc_trim_pitch",             VAR_INT16  | MASTER_VALUE, .config.minmax = { -300, 300 }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, accelerometerTrims.values.pitch) },
    { "acc_trim_roll",              VAR_INT16  | MASTER_VALUE, .config.minmax = { -300, 300 }, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, accelerometerTrims.values.roll) },

    // 4 elements are output for the ACC calibration - The 3 axis values and the 4th representing whether calibration has been performed
    { "acc_calibration",            VAR_INT16  | MASTER_VALUE | MODE_ARRAY, .config.array.length = 4, PG_ACCELEROMETER_CONFIG, offsetof(accelerometerConfig_t, accZero.raw) },
#endif

// PG_COMPASS_CONFIG
#ifdef USE_MAG
    { "align_mag",                  VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_ALIGNMENT }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_alignment) },
    { "mag_align_roll",             VAR_INT16  | HARDWARE_VALUE, .config.minmax = { -3600, 3600 }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_customAlignment.roll) },
    { "mag_align_pitch",            VAR_INT16  | HARDWARE_VALUE, .config.minmax = { -3600, 3600 }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_customAlignment.pitch) },
    { "mag_align_yaw",              VAR_INT16  | HARDWARE_VALUE, .config.minmax = { -3600, 3600 }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_customAlignment.yaw) },
    { "mag_bustype",                VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BUS_TYPE }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_busType) },
    { "mag_i2c_device",             VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, I2CDEV_COUNT }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_i2c_device) },
    { "mag_i2c_address",            VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, I2C_ADDR7_MAX }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_i2c_address) },
    { "mag_spi_device",             VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, SPIDEV_COUNT }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_spi_device) },
    { PARAM_NAME_MAG_HARDWARE,      VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_MAG_HARDWARE }, PG_COMPASS_CONFIG, offsetof(compassConfig_t, mag_hardware) },
    { "mag_calibration",            VAR_INT16  | MASTER_VALUE | MODE_ARRAY, .config.array.length = XYZ_AXIS_COUNT, PG_COMPASS_CONFIG, offsetof(compassConfig_t, magZero.raw) },
#endif

// PG_BAROMETER_CONFIG
#ifdef USE_BARO
    { "baro_bustype",               VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BUS_TYPE }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_busType) },
    { "baro_spi_device",            VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, 5 }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_spi_device) },
    { "baro_i2c_device",            VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, 5 }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_i2c_device) },
    { "baro_i2c_address",           VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, I2C_ADDR7_MAX }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_i2c_address) },
    { PARAM_NAME_BARO_HARDWARE,     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BARO_HARDWARE }, PG_BAROMETER_CONFIG, offsetof(barometerConfig_t, baro_hardware) },
#endif

// PG_RX_CONFIG
    { "rc_channel_map",             VAR_UINT8  | MASTER_VALUE | MODE_ARRAY, .config.array.length = RC_CONTROL_COUNT, PG_RX_CONFIG, offsetof(rxConfig_t, rc_channel) },
    { "rssi_channel",               VAR_INT8   | MASTER_VALUE, .config.minmax = { 0, MAX_SUPPORTED_RC_CHANNEL_COUNT }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_channel) },
    { "rssi_src_frame_errors",      VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_src_frame_errors) },
    { "rssi_scale",                 VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { RSSI_SCALE_MIN, RSSI_SCALE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_scale) },
    { "rssi_offset",                VAR_INT8   | MASTER_VALUE, .config.minmax = { -100, 100 }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_offset) },
    { "rssi_invert",                VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_invert) },
    { "rssi_src_frame_lpf_period",  VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, UINT8_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_src_frame_lpf_period) },
    { "rssi_smoothing",             VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, UINT8_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rssi_smoothing) },

    { "max_rc_channels",             VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, MAX_SUPPORTED_RC_CHANNEL_COUNT }, PG_RX_CONFIG, offsetof(rxConfig_t, max_rc_channel) },
#ifdef USE_SERIALRX
    { PARAM_NAME_SERIAL_RX_PROVIDER, VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_SERIAL_RX }, PG_RX_CONFIG, offsetof(rxConfig_t, serialrx_provider) },
    { "serialrx_inverted",           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, serialrx_inverted) },
#endif
#ifdef USE_SPEKTRUM_BIND
    { "spektrum_sat_bind",           VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { SPEKTRUM_SAT_BIND_DISABLED, SPEKTRUM_SAT_BIND_MAX}, PG_RX_CONFIG, offsetof(rxConfig_t, spektrum_sat_bind) },
    { "spektrum_sat_bind_autoreset", VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, spektrum_sat_bind_autoreset) },
#endif
#ifdef USE_SERIALRX_SRXL2
    { "srxl2_unit_id",               VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 0xf }, PG_RX_CONFIG, offsetof(rxConfig_t, srxl2_unit_id) },
    { "srxl2_baud_fast",             VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, srxl2_baud_fast) },
#endif
#if defined(USE_SERIALRX_SBUS)
    { "sbus_baud_fast",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, sbus_baud_fast) },
#endif
#if defined(USE_CRSF_V3)
    { "crsf_use_negotiated_baud",    VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, crsf_use_negotiated_baud) },
#endif
    { "rx_pulse_min",               VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { PWM_PULSE_MIN, PWM_PULSE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rx_pulse_min) },
    { "rx_pulse_max",               VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { PWM_PULSE_MIN, PWM_PULSE_MAX }, PG_RX_CONFIG, offsetof(rxConfig_t, rx_pulse_max) },
    { "serialrx_halfduplex",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_RX_CONFIG, offsetof(rxConfig_t, halfDuplex) },

// PG_ADC_CONFIG
#if defined(USE_ADC)
#if PLATFORM_TRAIT_ADC_DEVICE
    { "adc_device",                 VAR_INT8 | HARDWARE_VALUE, .config.minmax = { 0, ADCDEV_COUNT }, PG_ADC_CONFIG, offsetof(adcConfig_t, device) },
#endif
    { "adc_vrefint_calibration",    VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 2000 }, PG_ADC_CONFIG, offsetof(adcConfig_t, vrefIntCalibration) },
    { "adc_tempsensor_calibration30", VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 2000 }, PG_ADC_CONFIG, offsetof(adcConfig_t, tempSensorCalibration1) },
    { "adc_tempsensor_calibration110", VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 2000 }, PG_ADC_CONFIG, offsetof(adcConfig_t, tempSensorCalibration2) },
#endif

// PG_BLACKBOX_CONFIG
#ifdef USE_BLACKBOX
    { "blackbox_mode",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BLACKBOX_MODE }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, mode) },
    { "blackbox_device",            VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BLACKBOX_DEVICE }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, device) },
    { "blackbox_rate_denom",        VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 1, 8000 }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, denom) },
    { "blackbox_log_command",       VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_COMMAND, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_setpoint",      VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_SETPOINT, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_pid",           VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_PID, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_attitude",      VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_ATTITUDE, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_gyro_raw",      VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_GYRAW, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_gyro",          VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_GYRO, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
#if defined(USE_ACC)
    { "blackbox_log_acc",           VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_ACC, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
#endif
#ifdef USE_MAG
    { "blackbox_log_mag",           VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_MAG, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
#endif
#ifdef USE_BARO
    { "blackbox_log_alt",           VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_ALT, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
#endif
#ifdef USE_GPS
    { "blackbox_log_gps",           VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_GPS, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
#endif
    { "blackbox_log_battery",       VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_BATTERY, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_motors",        VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_MOTOR, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_servos",        VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_SERVO, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_rpm",           VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_RPM, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_rssi",          VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_RSSI, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_temp",          VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_TEMP, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
#ifdef USE_ESC_SENSOR
    { "blackbox_log_esc",           VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_ESC, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
    { "blackbox_log_esc2",          VAR_UINT32 | MASTER_VALUE | MODE_BITSET, .config.bitpos = FLIGHT_LOG_FIELD_SELECT_ESC2, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, fields) },
#endif

#ifdef USE_FLASHFS_LOOP
    { "blackbox_initial_erase_kb",  VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, UINT16_MAX }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, initialErase) },
    { "blackbox_rolling_erase",     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, rollingErase) },
#endif
    { "blackbox_grace_period",      VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 60 }, PG_BLACKBOX_CONFIG, offsetof(blackboxConfig_t, gracePeriod) },
#endif

// PG_MOTOR_CONFIG
    { "max_throttle",               VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { PWM_PULSE_MIN, PWM_PULSE_MAX }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, maxthrottle) },
    { "min_command",                VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { PWM_PULSE_MIN, PWM_PULSE_MAX }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, mincommand) },
    { "motor_kv",                   VAR_UINT16 | HARDWARE_VALUE, .config.minmaxUnsigned = { 1, 40000 },                   PG_MOTOR_CONFIG, offsetof(motorConfig_t, kv) },
#ifdef USE_DSHOT
#ifdef USE_DSHOT_DMAR
    { "dshot_burst",                VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON_AUTO }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.useBurstDshot) },
#endif
#ifdef USE_DSHOT_TELEMETRY
    { PARAM_NAME_DSHOT_BIDIR,       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.useDshotTelemetry) },
    { "dshot_edt",                  VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_DSHOT_EDT }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.useDshotEdt) },
#endif
#ifdef USE_DSHOT_BITBANG
    { "dshot_bitbang",               VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON_AUTO }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.useDshotBitbang) },
    { "dshot_bitbang_timer",         VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_DSHOT_BITBANGED_TIMER }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.useDshotBitbangedTimer) },
#endif
#endif // USE_DSHOT
    { PARAM_NAME_USE_UNSYNCED_PWM,  VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.useContinuousUpdate) },
    { PARAM_NAME_MOTOR_PWM_PROTOCOL, VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_MOTOR_PWM_PROTOCOL }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.motorProtocol) },
    { PARAM_NAME_MOTOR_PWM_RATE,    VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 200, 32000 }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.motorPwmRate) },
    { "motor_pwm_inversion",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, dev.motorInversion) },
    { PARAM_NAME_MOTOR_POLES,       VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 4, UINT8_MAX }, PG_MOTOR_CONFIG, offsetof(motorConfig_t, motorPoleCount) },

// PG_FAILSAFE_CONFIG
    { "failsafe_delay",             VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { PERIOD_RXDATA_RECOVERY / MILLIS_PER_TENTH_SECOND, 200 }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_delay) },
    { "failsafe_landing_time",      VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 250 }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_landing_time) },
    { "failsafe_throttle",          VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { PWM_PULSE_MIN, PWM_PULSE_MAX }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_throttle) },
    { "failsafe_switch_mode",       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_FAILSAFE_SWITCH_MODE }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_switch_mode) },
    { "failsafe_throttle_low_delay",VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 300 }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_throttle_low_delay) },
    { "failsafe_procedure",         VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_FAILSAFE }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_procedure) },
    { "failsafe_recovery_delay",    VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 1, 200 }, PG_FAILSAFE_CONFIG, offsetof(failsafeConfig_t, failsafe_recovery_delay) },

// PG_BOARDALIGNMENT_CONFIG
    { "align_board_roll",           VAR_INT16  | MASTER_VALUE, .config.minmax = { -3600, 3600 }, PG_BOARD_ALIGNMENT, offsetof(alignment_t, roll) },
    { "align_board_pitch",          VAR_INT16  | MASTER_VALUE, .config.minmax = { -3600, 3600 }, PG_BOARD_ALIGNMENT, offsetof(alignment_t, pitch) },
    { "align_board_yaw",            VAR_INT16  | MASTER_VALUE, .config.minmax = { -3600, 3600 }, PG_BOARD_ALIGNMENT, offsetof(alignment_t, yaw) },

// PG_BATTERY_PROFILES (per-profile settings)
#ifdef USE_PROFILE_NAMES
    { "battery_profile_name",       VAR_UINT8  | PROFILE_BATTERY_VALUE | MODE_STRING, .config.string = { 1, MAX_BATTERY_PROFILE_NAME_LENGTH, STRING_FLAGS_NONE }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, profileName) },
#endif
    { "bat_capacity",               VAR_UINT16 | PROFILE_BATTERY_VALUE, .config.minmaxUnsigned = { 0, 20000 }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, batteryCapacity) },
    { "vbat_max_cell_voltage",      VAR_UINT16  | PROFILE_BATTERY_VALUE, .config.minmaxUnsigned = { VBAT_CELL_VOTAGE_RANGE_MIN, VBAT_CELL_VOTAGE_RANGE_MAX }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, vbatmaxcellvoltage) },
    { "vbat_full_cell_voltage",     VAR_UINT16  | PROFILE_BATTERY_VALUE, .config.minmaxUnsigned = { VBAT_CELL_VOTAGE_RANGE_MIN, VBAT_CELL_VOTAGE_RANGE_MAX }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, vbatfullcellvoltage) },
    { "vbat_min_cell_voltage",      VAR_UINT16  | PROFILE_BATTERY_VALUE, .config.minmaxUnsigned = { VBAT_CELL_VOTAGE_RANGE_MIN, VBAT_CELL_VOTAGE_RANGE_MAX }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, vbatmincellvoltage) },
    { "vbat_warning_cell_voltage",  VAR_UINT16  | PROFILE_BATTERY_VALUE, .config.minmaxUnsigned = { VBAT_CELL_VOTAGE_RANGE_MIN, VBAT_CELL_VOTAGE_RANGE_MAX }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, vbatwarningcellvoltage) },
    { "cbat_alert_percent",         VAR_UINT8  | PROFILE_BATTERY_VALUE, .config.minmaxUnsigned = { 0, 100 }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, consumptionWarningPercentage) },
    { "force_battery_cell_count",   VAR_UINT8  | PROFILE_BATTERY_VALUE, .config.minmaxUnsigned = { 0, 24 }, PG_BATTERY_PROFILES, offsetof(batteryProfile_t, forceBatteryCellCount) },

// PG_BATTERY_CONFIG (global/hardware settings)
    { "vbat_hysteresis",            VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 250 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbathysteresis) },
    { "current_meter",              VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_CURRENT_METER }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, currentMeterSource) },
    { "battery_meter",              VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_VOLTAGE_METER }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, voltageMeterSource) },
    { "vbat_detect_cell_voltage",   VAR_UINT16  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 2000 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatnotpresentcellvoltage) },
    { "use_vbat_alerts",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, useVBatAlerts) },
    { "use_cbat_alerts",            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, useConsumptionAlerts) },
    { "vbat_cutoff_percent",        VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 100 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, lvcPercentage) },
    { "vbat_display_lpf_period",    VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 1, UINT8_MAX }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatDisplayLpfPeriod) },
    { "ibat_lpf_period",            VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, UINT8_MAX }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, ibatLpfPeriod) },
    { "vbat_duration_for_warning",  VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 150 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatDurationForWarning) },
    { "vbat_duration_for_critical", VAR_UINT8  | MASTER_VALUE, .config.minmax = { 0, 150 }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, vbatDurationForCritical) },

//  PG_VOLTAGE_SENSOR_ADC_CONFIG
    { "vbat_scale",                 VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { VBAT_SCALE_MIN, VBAT_SCALE_MAX }, PG_VOLTAGE_SENSOR_ADC_CONFIG, offsetof(voltageSensorADCConfig_t, vbatscale) },
    { "vbat_divider",               VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { VBAT_DIVIDER_MIN, VBAT_DIVIDER_MAX }, PG_VOLTAGE_SENSOR_ADC_CONFIG, offsetof(voltageSensorADCConfig_t, vbatresdivval) },
    { "vbat_multiplier",            VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { VBAT_MULTIPLIER_MIN, VBAT_MULTIPLIER_MAX }, PG_VOLTAGE_SENSOR_ADC_CONFIG, offsetof(voltageSensorADCConfig_t, vbatresdivmultiplier) },

// PG_CURRENT_SENSOR_ADC_CONFIG
    { "ibata_scale",                VAR_INT16  | HARDWARE_VALUE, .config.minmax = { -16000, 16000 }, PG_CURRENT_SENSOR_ADC_CONFIG, offsetof(currentSensorADCConfig_t, scale) },
    { "ibata_offset",               VAR_INT16  | MASTER_VALUE, .config.minmax = { -32000, 32000 }, PG_CURRENT_SENSOR_ADC_CONFIG, offsetof(currentSensorADCConfig_t, offset) },
// PG_CURRENT_SENSOR_ADC_CONFIG
#ifdef USE_BATTERY_CONTINUE
    { "battery_continue",           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BATTERY_CONFIG, offsetof(batteryConfig_t, isBatteryContinueEnabled) },
#endif

#ifdef USE_BEEPER
// PG_BEEPER_DEV_CONFIG
    { "beeper_inversion",           VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BEEPER_DEV_CONFIG, offsetof(beeperDevConfig_t, isInverted) },
    { "beeper_od",                  VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_BEEPER_DEV_CONFIG, offsetof(beeperDevConfig_t, isOpenDrain) },
    { "beeper_frequency",           VAR_INT16  | HARDWARE_VALUE, .config.minmax = { 0, 16000 }, PG_BEEPER_DEV_CONFIG, offsetof(beeperDevConfig_t, frequency) },

// PG_BEEPER_CONFIG
#ifdef USE_DSHOT
    { "beeper_dshot_beacon_tone",   VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = {1, DSHOT_CMD_BEACON5 }, PG_BEEPER_CONFIG, offsetof(beeperConfig_t, dshotBeaconTone) },
#endif
#endif // USE_BEEPER

// PG_CONTROLRATE_PROFILES
#ifdef USE_PROFILE_NAMES
    { "rateprofile_name",           VAR_UINT8  | PROFILE_RATE_VALUE | MODE_STRING, .config.string = { 1, MAX_RATE_PROFILE_NAME_LENGTH, STRING_FLAGS_NONE }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, profileName) },
#endif
    { PARAM_NAME_RATES_TYPE,        VAR_UINT8  | PROFILE_RATE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_RATES_TYPE }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rates_type) },
    { "quickrates_rc_expo",         VAR_UINT8  | PROFILE_RATE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, quickRatesRcExpo) },
    { "roll_rc_rate",               VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 1, CONTROL_RATE_CONFIG_RC_RATES_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcRates[FD_ROLL]) },
    { "pitch_rc_rate",              VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 1, CONTROL_RATE_CONFIG_RC_RATES_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcRates[FD_PITCH]) },
    { "yaw_rc_rate",                VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 1, CONTROL_RATE_CONFIG_RC_RATES_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcRates[FD_YAW]) },
    { "roll_expo",                  VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 0, CONTROL_RATE_CONFIG_RC_EXPO_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcExpo[FD_ROLL]) },
    { "pitch_expo",                 VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 0, CONTROL_RATE_CONFIG_RC_EXPO_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcExpo[FD_PITCH]) },
    { "yaw_expo",                   VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 0, CONTROL_RATE_CONFIG_RC_EXPO_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rcExpo[FD_YAW]) },
    { "roll_srate",                 VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 0, CONTROL_RATE_CONFIG_RATE_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rates[FD_ROLL]) },
    { "pitch_srate",                VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 0, CONTROL_RATE_CONFIG_RATE_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rates[FD_PITCH]) },
    { "yaw_srate",                  VAR_UINT8  | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { 0, CONTROL_RATE_CONFIG_RATE_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rates[FD_YAW]) },
    { "roll_rate_limit",            VAR_UINT16 | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { CONTROL_RATE_CONFIG_RATE_LIMIT_MIN, CONTROL_RATE_CONFIG_RATE_LIMIT_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rate_limit[FD_ROLL]) },
    { "pitch_rate_limit",           VAR_UINT16 | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { CONTROL_RATE_CONFIG_RATE_LIMIT_MIN, CONTROL_RATE_CONFIG_RATE_LIMIT_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rate_limit[FD_PITCH]) },
    { "yaw_rate_limit",             VAR_UINT16 | PROFILE_RATE_VALUE, .config.minmaxUnsigned = { CONTROL_RATE_CONFIG_RATE_LIMIT_MIN, CONTROL_RATE_CONFIG_RATE_LIMIT_MAX }, PG_CONTROL_RATE_PROFILES, offsetof(controlRateConfig_t, rate_limit[FD_YAW]) },

// PG_SERIAL_CONFIG
    { "reboot_character",           VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 48, 126 }, PG_SERIAL_CONFIG, offsetof(serialConfig_t, reboot_character) },
    { "serial_update_rate_hz",      VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 100, 2000 }, PG_SERIAL_CONFIG, offsetof(serialConfig_t, serial_update_rate_hz) },

// PG_IMU_CONFIG
    { PARAM_NAME_IMU_DCM_KP,          VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 32000 }, PG_IMU_CONFIG, offsetof(imuConfig_t, imu_dcm_kp) },
    { PARAM_NAME_IMU_DCM_KI,          VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 32000 }, PG_IMU_CONFIG, offsetof(imuConfig_t, imu_dcm_ki) },
    { PARAM_NAME_IMU_PROCESS_DENOM,   VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 1,     4 }, PG_IMU_CONFIG, offsetof(imuConfig_t, imu_process_denom) },
#ifdef USE_MAG
    { PARAM_NAME_IMU_MAG_DECLINATION, VAR_INT16 | MASTER_VALUE, .config.minmax = { -300, 300 }, PG_IMU_CONFIG, offsetof(imuConfig_t, mag_declination) },
    { PARAM_NAME_TRUST_MAG,           VAR_UINT8 | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },  PG_IMU_CONFIG, offsetof(imuConfig_t, trust_mag) },

#endif

// PG_ARMING_CONFIG
    { "auto_disarm_delay",          VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 60 }, PG_ARMING_CONFIG, offsetof(armingConfig_t, auto_disarm_delay) },
    { PARAM_NAME_GYRO_CAL_ON_FIRST_ARM, VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_ARMING_CONFIG, offsetof(armingConfig_t, gyro_cal_on_first_arm) },
    { PARAM_NAME_PREARM_ALLOW_REARM, VAR_UINT8 | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_ARMING_CONFIG, offsetof(armingConfig_t, prearm_allow_rearm) },
// PG_GPS_CONFIG
#ifdef USE_GPS
    { PARAM_NAME_GPS_PROVIDER,               VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GPS_PROVIDER },   PG_GPS_CONFIG, offsetof(gpsConfig_t, provider) },
    { PARAM_NAME_GPS_SBAS_MODE,              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GPS_SBAS_MODE },  PG_GPS_CONFIG, offsetof(gpsConfig_t, sbasMode) },
    { PARAM_NAME_GPS_AUTO_CONFIG,            VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },         PG_GPS_CONFIG, offsetof(gpsConfig_t, autoConfig) },
    { PARAM_NAME_GPS_AUTO_BAUD,              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },         PG_GPS_CONFIG, offsetof(gpsConfig_t, autoBaud) },
    { PARAM_NAME_GPS_UBLOX_ACQUIRE_MODEL,    VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GPS_UBLOX_MODELS }, PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_ublox_acquire_model) },
    { PARAM_NAME_GPS_UBLOX_FLIGHT_MODEL,     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GPS_UBLOX_MODELS }, PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_ublox_flight_model) },
    { PARAM_NAME_GPS_UPDATE_RATE_HZ,         VAR_UINT8  | MASTER_VALUE,               .config.minmaxUnsigned = {1, 20},          PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_update_rate_hz) },
    { PARAM_NAME_GPS_UBLOX_UTC_STANDARD,     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_GPS_UBLOX_UTC_STANDARD }, PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_ublox_utc_standard) },
    { PARAM_NAME_GPS_UBLOX_USE_GALILEO,      VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },         PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_ublox_use_galileo) },
    { PARAM_NAME_GPS_UBLOX_ENABLE_ANA,       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },         PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_ublox_enable_ana) },
    { PARAM_NAME_GPS_SET_HOME_POINT_ONCE,    VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },         PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_set_home_point_once) },
    { PARAM_NAME_GPS_USE_3D_SPEED,           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },         PG_GPS_CONFIG, offsetof(gpsConfig_t, gps_use_3d_speed) },
    { PARAM_NAME_GPS_SBAS_INTEGRITY,         VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },         PG_GPS_CONFIG, offsetof(gpsConfig_t, sbas_integrity) },
    { PARAM_NAME_GPS_NMEA_CUSTOM_COMMANDS,   VAR_UINT8  | MASTER_VALUE | MODE_STRING, .config.string = { 1, NMEA_CUSTOM_COMMANDS_MAX_LENGTH, STRING_FLAGS_NONE }, PG_GPS_CONFIG, offsetof(gpsConfig_t, nmeaCustomCommands) },

#endif // USE_GPS
    { "rc_center",                  VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 1200, 1700 }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, rc_center) },
    { "rc_deflection",              VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 250, 750 }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, rc_deflection) },
    { "rc_min_throttle",            VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, PWM_PULSE_MAX }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, rc_min_throttle) },
    { "rc_max_throttle",            VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, PWM_PULSE_MAX }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, rc_max_throttle) },
    { PARAM_NAME_DEADBAND,          VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 100 }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, rc_deadband) },
    { PARAM_NAME_YAW_DEADBAND,      VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 100 }, PG_RC_CONTROLS_CONFIG, offsetof(rcControlsConfig_t, rc_yaw_deadband) },



// PG_PID_CONFIG
    { PARAM_NAME_PID_PROCESS_DENOM,    VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 1, MAX_PID_PROCESS_DENOM }, PG_PID_CONFIG, offsetof(pidConfig_t, pid_process_denom) },
    { PARAM_NAME_FILTER_PROCESS_DENOM, VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, MAX_PID_PROCESS_DENOM }, PG_PID_CONFIG, offsetof(pidConfig_t, filter_process_denom) },

// PG_PID_PROFILE
#ifdef USE_PROFILE_NAMES
    { "profile_name",               VAR_UINT8  | PROFILE_VALUE | MODE_STRING, .config.string = { 1, MAX_PROFILE_NAME_LENGTH, STRING_FLAGS_NONE }, PG_PID_PROFILE, offsetof(pidProfile_t, profileName) },
#endif

// PG_TELEMETRY_CONFIG
#ifdef USE_TELEMETRY
    { "tlm_inverted",               VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, telemetry_inverted) },
    { "tlm_halfduplex",             VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, halfDuplex) },
#if defined(USE_CRSF_ACCGYRO_TELEMETRY)
    { "crsf_tlm_accgyro",           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, crsf_tlm_accgyro) },
#endif
#if defined(USE_TELEMETRY_FRSKY_HUB)
#if defined(USE_GPS)
    { "frsky_default_lat",          VAR_INT16  | MASTER_VALUE, .config.minmax = { -9000, 9000 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, gpsNoFixLatitude) },
    { "frsky_default_long",         VAR_INT16  | MASTER_VALUE, .config.minmax = { -18000, 18000 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, gpsNoFixLongitude) },
    { "frsky_gps_format",           VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, FRSKY_FORMAT_NMEA }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, frsky_coordinate_format) },
    { "frsky_unit",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_UNIT }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, frsky_unit) },
#endif
    { "frsky_vfas_precision",       VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { FRSKY_VFAS_PRECISION_LOW,  FRSKY_VFAS_PRECISION_HIGH }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, frsky_vfas_precision) },
#endif // USE_TELEMETRY_FRSKY_HUB
    { "hott_alarm_int",             VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 120 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, hottAlarmSoundInterval) },
    { "pid_in_tlm",                 VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, pidValuesAsTelemetry) },
    { "report_cell_voltage",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, report_cell_voltage) },
#if defined(USE_TELEMETRY_IBUS)
    { "ibus_sensor",                VAR_UINT8  | MASTER_VALUE | MODE_ARRAY, .config.array.length = IBUS_SENSOR_COUNT, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, flysky_sensors)},
#endif
#ifdef USE_TELEMETRY_MAVLINK
    // Support for misusing the heading field in MAVlink to indicate mAh drawn for Connex Prosight OSD
    // Set to 10 to show a tenth of your capacity drawn.
    // Set to $size_of_battery to get a percentage of battery used.
    { "mavlink_mah_as_heading_divisor", VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 30000 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_mah_as_heading_divisor) },
    // mavlink telemetry flow control to prevent TX buffers overflow
    { "mavlink_min_txbuff", VAR_UINT8 | MASTER_VALUE, .config.minmaxUnsigned = { 1, 100 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_min_txbuff) },
    { "mavlink_ext_status_rate", VAR_UINT8 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 50 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_extended_status_rate) },
    { "mavlink_rc_chan_rate", VAR_UINT8 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 50 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_rc_channels_rate) },
    { "mavlink_pos_rate", VAR_UINT8 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 50 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_position_rate) },
    { "mavlink_extra1_rate", VAR_UINT8 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 50 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_extra1_rate) },
    { "mavlink_extra2_rate", VAR_UINT8 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 50 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_extra2_rate) },
    { "mavlink_extra3_rate", VAR_UINT8 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 50 }, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, mavlink_extra3_rate) },
#endif
#ifdef USE_TELEMETRY_SENSORS_DISABLED_DETAILS
    { "telemetry_disabled_voltage",         VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_VOLTAGE),         PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_current",         VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_CURRENT),         PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_fuel",            VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_FUEL),            PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_mode",            VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_MODE),            PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_acc_x",           VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_ACC_X),           PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_acc_y",           VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_ACC_Y),           PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_acc_z",           VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_ACC_Z),           PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_pitch",           VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_PITCH),           PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_roll",            VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_ROLL),            PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_heading",         VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_HEADING),         PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_altitude",        VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_ALTITUDE),        PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_vario",           VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_VARIO),           PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_lat_long",        VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_LAT_LONG),        PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_ground_speed",    VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_GROUND_SPEED),    PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_distance",        VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_DISTANCE),        PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_esc_current",     VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(ESC_SENSOR_CURRENT),     PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_esc_voltage",     VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(ESC_SENSOR_VOLTAGE),     PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_esc_rpm",         VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(ESC_SENSOR_RPM),         PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_esc_temperature", VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(ESC_SENSOR_TEMPERATURE), PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_temperature",     VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_TEMPERATURE),     PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
    { "telemetry_disabled_cap_used",        VAR_UINT32  | MASTER_VALUE | MODE_BITSET, .config.bitpos = LOG2(SENSOR_CAP_USED),        PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
#else
    { "telemetry_disabled_sensors", VAR_UINT32 | MASTER_VALUE, .config.u32Max = SENSOR_ALL, PG_TELEMETRY_CONFIG, offsetof(telemetryConfig_t, disabledSensors)},
#endif
#endif // USE_TELEMETRY

// PG_LED_STRIP_CONFIG
#ifdef USE_LED_STRIP
    { "ledstrip_visual_beeper",     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_visual_beeper) },
    { "ledstrip_visual_beeper_color",VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_LEDSTRIP_COLOR }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_visual_beeper_color) },
    { "ledstrip_grb_rgb",           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_RGB_GRB }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_grb_rgb) },
    { "ledstrip_profile",           VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_LED_PROFILE }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_profile) },
    { "ledstrip_race_color",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_LEDSTRIP_COLOR }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_race_color) },
    { "ledstrip_beacon_color",      VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_LEDSTRIP_COLOR }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_beacon_color) },
    { "ledstrip_beacon_period_ms",  VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 50, 10000 }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_beacon_period_ms) },
    { "ledstrip_beacon_percent",    VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 100 }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_beacon_percent) },
    { "ledstrip_beacon_armed_only", VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_beacon_armed_only) },
    { "ledstrip_brightness",        VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 5, 100 }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_brightness) },
    { "ledstrip_rainbow_delta",     VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, HSV_HUE_MAX }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_rainbow_delta) },
    { "ledstrip_rainbow_freq",      VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 1, 2000 }, PG_LED_STRIP_CONFIG, offsetof(ledStripConfig_t, ledstrip_rainbow_freq) },
#endif

// PG_SDCARD_CONFIG
#ifdef USE_SDCARD
    { "sdcard_detect_inverted",     VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SDCARD_CONFIG, offsetof(sdcardConfig_t, cardDetectInverted) },
    { "sdcard_mode",                VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_SDCARD_MODE }, PG_SDCARD_CONFIG, offsetof(sdcardConfig_t, mode) },
#endif
#ifdef USE_SDCARD_SPI
    { "sdcard_spi_bus",             VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, SPIDEV_COUNT }, PG_SDCARD_CONFIG, offsetof(sdcardConfig_t, device) },
#endif
#ifdef USE_SDCARD_SDIO
    { "sdio_clk_bypass",            VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SDIO_CONFIG, offsetof(sdioConfig_t, clockBypass) },
    { "sdio_use_cache",             VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SDIO_CONFIG, offsetof(sdioConfig_t, useCache) },
    { "sdio_use_4bit_width",        VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SDIO_CONFIG, offsetof(sdioConfig_t, use4BitWidth) },
#if ENABLE_SDIO_PIN_CONFIG
    { "sdio_device",                VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, SDIODEV_COUNT }, PG_SDIO_CONFIG, offsetof(sdioConfig_t, device) },
#endif
#endif

// PG_SYSTEM_CONFIG
#if PLATFORM_TRAIT_CONFIG_HSE
    { "system_hse_mhz",             VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, 30 }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, hseMhz) },
#endif
    { "task_statistics",            VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, task_statistics) },
    { PARAM_NAME_DEBUG_MODE,        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_DEBUG }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, debug_mode) },
    { PARAM_NAME_DEBUG_AXIS,        VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 255 }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, debug_axis) },
#ifdef USE_OVERCLOCK
    { "cpu_overclock",              VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OVERCLOCK }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, cpu_overclock) },
#endif
    { "pwr_on_arm_grace",           VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 30 }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, powerOnArmingGraceTime) },
    { "enable_stick_arming",        VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_SYSTEM_CONFIG, offsetof(systemConfig_t, enableStickArming) },

#ifdef USE_ESC_SENSOR
    { "esc_sensor_halfduplex",          VAR_UINT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_ESC_SENSOR_CONFIG, offsetof(escSensorConfig_t, halfDuplex) },
    { "esc_sensor_current_offset",      VAR_UINT16  | MASTER_VALUE, .config.minmaxUnsigned = { 0, 16000 }, PG_ESC_SENSOR_CONFIG, offsetof(escSensorConfig_t, offset) },
#endif

#if !defined(USE_VIRTUAL_LED)
    { "led_inversion",                  VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, ((1 << STATUS_LED_COUNT) - 1) }, PG_STATUS_LED_CONFIG, offsetof(statusLedConfig_t, inversion) },
#endif
#ifdef USE_DASHBOARD
    { "dashboard_i2c_bus",           VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, I2CDEV_COUNT }, PG_DASHBOARD_CONFIG, offsetof(dashboardConfig_t, device) },
    { "dashboard_i2c_addr",          VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { I2C_ADDR7_MIN, I2C_ADDR7_MAX }, PG_DASHBOARD_CONFIG, offsetof(dashboardConfig_t, address) },
#endif

// PG_RANGEFINDER_CONFIG
#ifdef USE_RANGEFINDER
    { "rangefinder_hardware", VAR_UINT8 | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_RANGEFINDER_HARDWARE }, PG_RANGEFINDER_CONFIG, offsetof(rangefinderConfig_t, rangefinder_hardware) },
#endif

// PG_OPTICALFLOW_CONFIG
#ifdef USE_OPTICALFLOW
    { "opticalflow_hardware",     VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OPTICALFLOW_HARDWARE }, PG_OPTICALFLOW_CONFIG, offsetof(opticalflowConfig_t, opticalflow_hardware) },
    { "opticalflow_rotation",     VAR_INT16  | MASTER_VALUE ,              .config.minmaxUnsigned = {0, 359},               PG_OPTICALFLOW_CONFIG, offsetof(opticalflowConfig_t, rotation) },
    { "opticalflow_lpf",          VAR_UINT16 | MASTER_VALUE ,              .config.minmaxUnsigned = {0, 10000},             PG_OPTICALFLOW_CONFIG, offsetof(opticalflowConfig_t, flow_lpf) },
    { "opticalflow_flip_x",       VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON },               PG_OPTICALFLOW_CONFIG, offsetof(opticalflowConfig_t, flip_x) },
#endif

// PG_PINIO_CONFIG
#ifdef USE_PINIO
    { "pinio_config", VAR_UINT8 | HARDWARE_VALUE | MODE_ARRAY, .config.array.length = PINIO_COUNT, PG_PINIO_CONFIG, offsetof(pinioConfig_t, config) },
#ifdef USE_PINIOBOX
    { "pinio_box", VAR_UINT8 | HARDWARE_VALUE | MODE_ARRAY, .config.array.length = PINIO_COUNT, PG_PINIOBOX_CONFIG, offsetof(pinioBoxConfig_t, permanentId) },
#endif
#endif

//PG USB
#ifdef USE_USB_CDC_HID
    { "usb_hid_cdc", VAR_UINT8 | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_USB_CONFIG, offsetof(usbDev_t, type) },
#endif
#ifdef USE_USB_MSC
    { "usb_msc_pin_pullup", VAR_UINT8 | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_USB_CONFIG, offsetof(usbDev_t, mscButtonUsePullup) },
#endif
// PG_FLASH_CONFIG
#ifdef USE_FLASH_SPI
    { "flash_spi_bus", VAR_UINT8 | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, SPIDEV_COUNT }, PG_FLASH_CONFIG, offsetof(flashConfig_t, spiDevice) },
#endif
// PG_GYRO_DEVICE_CONFIG
#define GYRO_DEVICE_RECORDS(N, IDX)                                                \
    /* bustype  */                                                                 \
    { "gyro_" STR(N) "_bustype",       VAR_UINT8 | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_BUS_TYPE }, PG_GYRO_DEVICE_CONFIG, PG_ARRAY_ELEMENT_OFFSET(gyroDeviceConfig_t, IDX, busType) },\
    /* spibus   */                                                                 \
    { "gyro_" STR(N) "_spibus",        VAR_UINT8 | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, SPIDEV_COUNT }, PG_GYRO_DEVICE_CONFIG, PG_ARRAY_ELEMENT_OFFSET(gyroDeviceConfig_t, IDX, spiBus) },\
    /* i2cBus   */                                                                 \
    { "gyro_" STR(N) "_i2cBus",        VAR_UINT8 | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, I2CDEV_COUNT }, PG_GYRO_DEVICE_CONFIG, PG_ARRAY_ELEMENT_OFFSET(gyroDeviceConfig_t, IDX, i2cBus) },\
    /* i2c addr */                                                                 \
    { "gyro_" STR(N) "_i2c_address",   VAR_UINT8 | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, I2C_ADDR7_MAX }, PG_GYRO_DEVICE_CONFIG, PG_ARRAY_ELEMENT_OFFSET(gyroDeviceConfig_t, IDX, i2cAddress) }\
    /* end      */

    GYRO_DEVICE_RECORDS(1, 0),
#if GYRO_COUNT > 1
    GYRO_DEVICE_RECORDS(2, 1),
#endif
#if GYRO_COUNT > 2
    GYRO_DEVICE_RECORDS(3, 2),
#endif
#if GYRO_COUNT > 3
    GYRO_DEVICE_RECORDS(4, 3),
#endif
#if GYRO_COUNT > 4
    GYRO_DEVICE_RECORDS(5, 4),
#endif
#if GYRO_COUNT > 5
    GYRO_DEVICE_RECORDS(6, 5),
#endif
#if GYRO_COUNT > 6
    GYRO_DEVICE_RECORDS(7, 6),
#endif
#if GYRO_COUNT > 7
    GYRO_DEVICE_RECORDS(8, 7),
#endif
#ifdef I2C_FULL_RECONFIGURABILITY
#ifdef USE_I2C_DEVICE_0
    { "i2c0_pullup",    VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_0, pullUp) },
    { "i2c0_clockspeed_khz", VAR_UINT16 | HARDWARE_VALUE, .config.minmax = { I2C_CLOCKSPEED_MIN_KHZ, I2C_CLOCKSPEED_MAX_KHZ }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_0, clockSpeed) },
#endif
#ifdef USE_I2C_DEVICE_1
    { "i2c1_pullup",    VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_1, pullUp) },
    { "i2c1_clockspeed_khz", VAR_UINT16 | HARDWARE_VALUE, .config.minmax = { I2C_CLOCKSPEED_MIN_KHZ, I2C_CLOCKSPEED_MAX_KHZ }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_1, clockSpeed) },
#endif
#ifdef USE_I2C_DEVICE_2
    { "i2c2_pullup",    VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_2, pullUp) },
    { "i2c2_clockspeed_khz", VAR_UINT16 | HARDWARE_VALUE, .config.minmax = { I2C_CLOCKSPEED_MIN_KHZ, I2C_CLOCKSPEED_MAX_KHZ }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_2, clockSpeed) },
#endif
#ifdef USE_I2C_DEVICE_3
    { "i2c3_pullup",    VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_3, pullUp) },
    { "i2c3_clockspeed_khz", VAR_UINT16 | HARDWARE_VALUE, .config.minmax = { I2C_CLOCKSPEED_MIN_KHZ, I2C_CLOCKSPEED_MAX_KHZ }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_3, clockSpeed) },
#endif
#ifdef USE_I2C_DEVICE_4
    { "i2c4_pullup",    VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_4, pullUp) },
    { "i2c4_clockspeed_khz", VAR_UINT16 | HARDWARE_VALUE, .config.minmax = { I2C_CLOCKSPEED_MIN_KHZ, I2C_CLOCKSPEED_MAX_KHZ }, PG_I2C_CONFIG, PG_ARRAY_ELEMENT_OFFSET(i2cConfig_t, I2CDEV_4, clockSpeed) },
#endif
#endif
#if ENABLE_CAN
// PG_CAN_CONFIG
    { "can_bitrate_khz", VAR_UINT16 | HARDWARE_VALUE, .config.minmaxUnsigned = { 125, 1000 }, PG_CAN_CONFIG, offsetof(canConfig_t, bitrate_khz) },
#endif
#if ENABLE_DRONECAN
// PG_DRONECAN_CONFIG
    { "dronecan_enabled", VAR_UINT8 | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_DRONECAN_CONFIG, offsetof(dronecanConfig_t, enabled) },
    { "dronecan_node_id", VAR_UINT8 | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, 127 }, PG_DRONECAN_CONFIG, offsetof(dronecanConfig_t, node_id) },
    { "dronecan_device", VAR_UINT8 | HARDWARE_VALUE, .config.minmaxUnsigned = { 1, CANDEV_COUNT }, PG_DRONECAN_CONFIG, offsetof(dronecanConfig_t, device) },
    { "dronecan_esc_rate_hz", VAR_UINT16 | HARDWARE_VALUE, .config.minmaxUnsigned = { 50, 500 }, PG_DRONECAN_CONFIG, offsetof(dronecanConfig_t, esc_rate_hz) },
    { "dronecan_dna_enabled", VAR_UINT8 | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_DRONECAN_CONFIG, offsetof(dronecanConfig_t, dna_enabled) },
#endif
#ifdef USE_MCO
#if defined(USE_MCO_DEVICE1)
// TODO: remove pin specific names
    { "mco_on_pa8",     VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MCO_CONFIG, PG_ARRAY_ELEMENT_OFFSET(mcoConfig_t, 0, enabled) },
    { "mco_source",     VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, MCO_SOURCE_COUNT - 1 }, PG_MCO_CONFIG, PG_ARRAY_ELEMENT_OFFSET(mcoConfig_t, 0, source) },
    { "mco_divider",    VAR_UINT8  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, MCO_DIVIDER_COUNT - 1 }, PG_MCO_CONFIG, PG_ARRAY_ELEMENT_OFFSET(mcoConfig_t, 0, divider) },
#elif defined(USE_MCO_DEVICE2)
    { "mco2_on_pc9",    VAR_UINT8  | HARDWARE_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MCO_CONFIG, PG_ARRAY_ELEMENT_OFFSET(mcoConfig_t, 1, enabled) },
#endif
#endif

    { "scheduler_relax_rx",  VAR_UINT16  | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, 500 }, PG_SCHEDULER_CONFIG, PG_ARRAY_ELEMENT_OFFSET(schedulerConfig_t, 0, rxRelaxDeterminism) },

    { "scheduler_debug_task", VAR_UINT8   | HARDWARE_VALUE, .config.minmaxUnsigned = { 0, TASK_COUNT }, PG_SCHEDULER_CONFIG, PG_ARRAY_ELEMENT_OFFSET(schedulerConfig_t, 0, debugTask) },

#ifdef USE_LATE_TASK_STATISTICS
    { "cpu_late_limit_permille", VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 0, 100 }, PG_SCHEDULER_CONFIG, offsetof(schedulerConfig_t, cpuLatePercentageLimit) },
#endif

    { "serialmsp_halfduplex", VAR_UINT8  | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_OFF_ON }, PG_MSP_CONFIG, offsetof(mspConfig_t, halfDuplex) },

// PG_TIMECONFIG
#ifdef USE_RTC_TIME
    { "timezone_offset_minutes",  VAR_INT16 | MASTER_VALUE, .config.minmax = { TIMEZONE_OFFSET_MINUTES_MIN, TIMEZONE_OFFSET_MINUTES_MAX }, PG_TIME_CONFIG, offsetof(timeConfig_t, tz_offsetMinutes) },
#endif


#ifdef USE_PERSISTENT_STATS
    { "stats_min_armed_time_s",   VAR_INT8   | MASTER_VALUE, .config.minmax = { STATS_OFF, INT8_MAX }, PG_STATS_CONFIG, offsetof(statsConfig_t, stats_min_armed_time_s) },
    { "stats_total_flights",    VAR_UINT32 | MASTER_VALUE, .config.u32Max = UINT32_MAX, PG_STATS_CONFIG, offsetof(statsConfig_t, stats_total_flights) },

    { "stats_total_time_s",     VAR_UINT32 | MASTER_VALUE, .config.u32Max = UINT32_MAX, PG_STATS_CONFIG, offsetof(statsConfig_t, stats_total_time_s) },
    { "stats_total_dist_m",     VAR_UINT32 | MASTER_VALUE, .config.u32Max = UINT32_MAX, PG_STATS_CONFIG, offsetof(statsConfig_t, stats_total_dist_m) },
    { "stats_save_move_limit",  VAR_UINT8  | MASTER_VALUE, .config.minmaxUnsigned = { 0, UINT8_MAX }, PG_STATS_CONFIG, offsetof(statsConfig_t, statsSaveMoveLimit) },
#ifdef USE_BATTERY_CONTINUE
    { "stats_mah_used",     VAR_UINT32 | MASTER_VALUE, .config.u32Max = UINT32_MAX, PG_STATS_CONFIG, offsetof(statsConfig_t, stats_mah_used) },
#endif

#endif // USE_PERSISTENT_STATS

    { "craft_name",   VAR_UINT8  | MASTER_VALUE | MODE_STRING, .config.string = { 1, MAX_NAME_LENGTH, STRING_FLAGS_NONE }, PG_PILOT_CONFIG, offsetof(pilotConfig_t, craftName) },

// PG_POSITION
    { PARAM_NAME_ALTITUDE_SOURCE,       VAR_INT8   | MASTER_VALUE | MODE_LOOKUP, .config.lookup = { TABLE_POSITION_ALT_SOURCE }, PG_POSITION, offsetof(positionConfig_t, altitude_source) },
    { PARAM_NAME_ALTITUDE_PREFER_BARO,  VAR_INT8   | MASTER_VALUE, .config.minmaxUnsigned = { 0, 100 }, PG_POSITION, offsetof(positionConfig_t, altitude_prefer_baro) },
    { PARAM_NAME_ALTITUDE_LPF,          VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 10, 5000 }, PG_POSITION, offsetof(positionConfig_t, altitude_lpf) },
    { "rangefinder_max_range_cm",       VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 50, 1000 }, PG_POSITION, offsetof(positionConfig_t, rangefinder_max_range_cm) },
    { PARAM_NAME_ALTITUDE_D_LPF,        VAR_UINT16 | MASTER_VALUE, .config.minmaxUnsigned = { 10, 5000 }, PG_POSITION, offsetof(positionConfig_t, altitude_d_lpf) },


// PG_MODE_ACTIVATION_CONFIG
#if defined(USE_CUSTOM_BOX_NAMES)
    { "box_user_1_name", VAR_UINT8 | HARDWARE_VALUE | MODE_STRING, .config.string = { 1, MAX_BOX_USER_NAME_LENGTH, STRING_FLAGS_NONE }, PG_MODE_ACTIVATION_CONFIG, offsetof(modeActivationConfig_t, box_user_names[0]) },
    { "box_user_2_name", VAR_UINT8 | HARDWARE_VALUE | MODE_STRING, .config.string = { 1, MAX_BOX_USER_NAME_LENGTH, STRING_FLAGS_NONE }, PG_MODE_ACTIVATION_CONFIG, offsetof(modeActivationConfig_t, box_user_names[1]) },
    { "box_user_3_name", VAR_UINT8 | HARDWARE_VALUE | MODE_STRING, .config.string = { 1, MAX_BOX_USER_NAME_LENGTH, STRING_FLAGS_NONE }, PG_MODE_ACTIVATION_CONFIG, offsetof(modeActivationConfig_t, box_user_names[2]) },
    { "box_user_4_name", VAR_UINT8 | HARDWARE_VALUE | MODE_STRING, .config.string = { 1, MAX_BOX_USER_NAME_LENGTH, STRING_FLAGS_NONE }, PG_MODE_ACTIVATION_CONFIG, offsetof(modeActivationConfig_t, box_user_names[3]) },
#endif
};

const uint16_t valueTableEntryCount = ARRAYLEN(valueTable);

STATIC_ASSERT(LOOKUP_TABLE_COUNT == ARRAYLEN(lookupTables), LOOKUP_TABLE_COUNT_incorrect);
