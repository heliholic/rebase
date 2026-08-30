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
#include <string.h>
#include <math.h>

#include "platform.h"

#include "blackbox/blackbox.h"

#include "build/debug.h"

#include "cli/cli.h"

#include "common/sensor_alignment.h"

#include "config/config_eeprom.h"
#include "fc/feature.h"

#include "drivers/dshot_command.h"
#include "drivers/motor.h"
#include "drivers/system.h"

#include "fc/controlrate_profile.h"
#include "fc/core.h"
#include "fc/rc.h"
#include "fc/rc_adjustments.h"
#include "fc/rc_controls.h"
#include "fc/runtime_config.h"

#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/mixer.h"
#include "flight/pid.h"
#include "flight/servos.h"
#include "flight/position.h"

#include "io/beeper.h"
#include "io/displayport_msp.h"
#include "io/gps.h"
#include "io/ledstrip.h"
#include "io/serial.h"
#include "io/vtx.h"

#include "msp/msp_box.h"

#include "osd/osd.h"

#include "pg/adc.h"
#include "pg/beeper.h"
#include "pg/beeper_dev.h"
#include "pg/displayport_profiles.h"
#include "pg/gyrodev.h"
#include "pg/motor.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/rx.h"
#include "pg/sdcard.h"
#include "pg/vtx_table.h"

#include "rx/rx.h"

#include "scheduler/scheduler.h"

#include "sensors/acceleration.h"
#include "sensors/battery.h"
#include "sensors/compass.h"
#include "sensors/gyro.h"

#include "config.h"

static bool configIsDirty; /* someone indicated that the config is modified and it is not yet saved */

static bool rebootRequired = false;  // set if a config change requires a reboot to take effect

static bool eepromWriteInProgress = false;

pidProfile_t *currentPidProfile;

bool isEepromWriteInProgress(void)
{
    return eepromWriteInProgress;
}

uint8_t getCurrentPidProfileIndex(void)
{
    return systemConfig()->pidProfileIndex;
}

static void loadPidProfile(void)
{
    currentPidProfile = pidProfilesMutable(systemConfig()->pidProfileIndex);
}

uint8_t getCurrentControlRateProfileIndex(void)
{
    return systemConfig()->activeRateProfile;
}

// Returns the active battery profile index.
uint8_t getCurrentBatteryProfileIndex(void)
{
    return systemConfig()->activeBatteryProfile;
}

void resetConfig(void)
{
    pgResetAll();

#if defined(USE_TARGET_CONFIG)
    targetConfiguration();
#endif
}

static void activateConfig(void)
{
    loadPidProfile();
    loadControlRateProfile();
    loadBatteryProfile();

    initRcProcessing();

    activeAdjustmentRangeReset();

    pidInit(currentPidProfile);

    rcControlsInit();

    failsafeReset();
#ifdef USE_ACC
    setAccelerationTrims(&accelerometerConfigMutable()->accZero);
    accInitFilters();
#endif

    imuConfigure();

#if defined(USE_LED_STRIP_STATUS_MODE)
    reevaluateLedConfig();
#endif

    initActiveBoxIds();
}

static void adjustFilterLimit(uint16_t *parm, uint16_t resetValue)
{
    if (*parm > LPF_MAX_HZ) {
        *parm = resetValue;
    }
}

static void validateAndFixRatesSettings(void)
{
    for (unsigned profileIndex = 0; profileIndex < CONTROL_RATE_PROFILE_COUNT; profileIndex++) {
        const ratesType_e ratesType = controlRateProfilesMutable(profileIndex)->rates_type;
        for (unsigned axis = FD_ROLL; axis <= FD_YAW; axis++) {
            controlRateProfilesMutable(profileIndex)->rcRates[axis] = constrain(controlRateProfilesMutable(profileIndex)->rcRates[axis], 0, ratesSettingLimits[ratesType].rc_rate_limit);
            controlRateProfilesMutable(profileIndex)->rates[axis] = constrain(controlRateProfilesMutable(profileIndex)->rates[axis], 0, ratesSettingLimits[ratesType].srate_limit);
            controlRateProfilesMutable(profileIndex)->rcExpo[axis] = constrain(controlRateProfilesMutable(profileIndex)->rcExpo[axis], 0, ratesSettingLimits[ratesType].expo_limit);
        }
    }
}

static void validateAndFixConfig(void)
{
    if (!isSerialConfigValid(serialConfigMutable())) {
        PG_RESET(serialConfig);
    }

#if defined(USE_GPS)
    const serialPortConfig_t *gpsSerial = findSerialPortConfig(FUNCTION_GPS);
    if (GPS_PROVIDER_REQUIRES_NO_SERIAL_PORT(gpsConfig()->provider) && gpsSerial) {
        serialRemovePort(gpsSerial->identifier);
    }

    if (!GPS_PROVIDER_REQUIRES_NO_SERIAL_PORT(gpsConfig()->provider) && !gpsSerial) {
        featureDisableImmediate(FEATURE_GPS);
    }
#endif

    if (motorConfig()->dev.motorProtocol == MOTOR_PROTOCOL_BRUSHED) {
        if (motorConfig()->mincommand < 1000) {
            motorConfigMutable()->mincommand = 1000;
        }
    }

    if ((motorConfig()->dev.motorProtocol == MOTOR_PROTOCOL_PWM ) && (motorConfig()->dev.motorPwmRate > BRUSHLESS_MOTORS_PWM_RATE)) {
        motorConfigMutable()->dev.motorPwmRate = BRUSHLESS_MOTORS_PWM_RATE;
    }

    validateAndFixGyroConfig();

#if defined(USE_MAG)
    buildAlignmentFromStandardAlignment(&compassConfigMutable()->mag_customAlignment, compassConfig()->mag_alignment);
#endif

    for (int i = 0; i < GYRO_COUNT; i++) {
        buildAlignmentFromStandardAlignment(&gyroDeviceConfigMutable(i)->customAlignment, gyroDeviceConfig(i)->alignment);
    }

#ifdef USE_ACC
    if (accelerometerConfig()->accZero.values.roll != 0 ||
        accelerometerConfig()->accZero.values.pitch != 0 ||
        accelerometerConfig()->accZero.values.yaw != 0) {
        accelerometerConfigMutable()->accZero.values.calibrationCompleted = 1;
    }
#endif // USE_ACC

    bool hasConfiguredRxFeature =
        featureIsConfigured(FEATURE_RX_PPM) ||
        featureIsConfigured(FEATURE_RX_SERIAL) ||
        featureIsConfigured(FEATURE_RX_MSP);
#if ENABLE_RX_UDP
    hasConfiguredRxFeature = hasConfiguredRxFeature || featureIsConfigured(FEATURE_RX_UDP);
#endif
    if (!hasConfiguredRxFeature) {
        featureEnableImmediate(DEFAULT_RX_FEATURE);
    }

    if (featureIsConfigured(FEATURE_RX_PPM)) {
        featureDisableImmediate(FEATURE_RX_SERIAL | FEATURE_RX_MSP | FEATURE_RX_UDP);
    }

    if (featureIsConfigured(FEATURE_RX_MSP)) {
        featureDisableImmediate(FEATURE_RX_SERIAL | FEATURE_RX_PPM | FEATURE_RX_UDP);
    }

    if (featureIsConfigured(FEATURE_RX_SERIAL)) {
        featureDisableImmediate(FEATURE_RX_MSP | FEATURE_RX_PPM | FEATURE_RX_UDP);
    }

#if ENABLE_RX_UDP
    if (featureIsConfigured(FEATURE_RX_UDP)) {
        featureDisableImmediate(FEATURE_RX_SERIAL | FEATURE_RX_PPM | FEATURE_RX_MSP);
    }
#endif // ENABLE_RX_UDP

#if defined(USE_ADC)
    if (featureIsConfigured(FEATURE_RSSI_ADC)) {
        rxConfigMutable()->rssi_channel = 0;
        rxConfigMutable()->rssi_src_frame_errors = false;
    } else
#endif
    if (rxConfigMutable()->rssi_channel
#if defined(USE_RX_PPM)
        || featureIsConfigured(FEATURE_RX_PPM)
#endif
        ) {
        rxConfigMutable()->rssi_src_frame_errors = false;
    }

    if (failsafeConfig()->failsafe_procedure >= FAILSAFE_PROCEDURE_COUNT) {
        failsafeConfigMutable()->failsafe_procedure = FAILSAFE_PROCEDURE_DROP_IT;
    }

#if defined(USE_ESC_SENSOR)
    // DroneCAN ESC telemetry feeds escSensorData[] without a serial port, so
    // only a serial-sourced sensor requires one.
    if (!findSerialPortConfig(FUNCTION_ESC_SENSOR) && !isMotorProtocolDronecan()) {
        featureDisableImmediate(FEATURE_ESC_SENSOR);
    }
#endif

    for (int i = 0; i < MAX_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        const modeActivationCondition_t *mac = modeActivationConditions(i);

        if (mac->linkedTo) {
            if (mac->modeId == BOXARM || isModeActivationConditionLinked(mac->linkedTo)) {
                removeModeActivationCondition(mac->modeId);
            }
        }
    }

#if defined(USE_DSHOT_TELEMETRY) && defined(USE_DSHOT_BITBANG)
    if (motorConfig()->dev.motorProtocol == MOTOR_PROTOCOL_PROSHOT1000 && motorConfig()->dev.useDshotTelemetry &&
        motorConfig()->dev.useDshotBitbang == DSHOT_BITBANG_ON) {
        motorConfigMutable()->dev.useDshotBitbang = DSHOT_BITBANG_AUTO;
    }
#endif

#ifdef USE_ADC
    adcConfigMutable()->vbat.enabled = (batteryConfig()->voltageMeterSource == VOLTAGE_METER_ADC);
    adcConfigMutable()->current.enabled = (batteryConfig()->currentMeterSource == CURRENT_METER_ADC);

    adcConfigMutable()->rssi.enabled = featureIsEnabled(FEATURE_RSSI_ADC);
#endif // USE_ADC

    // Bounds check gyro filter selection in case prior build had USE_GYRO_DLPF_EXPERIMENTAL defined
    if (gyroConfig()->gyro_hardware_lpf >= GYRO_HARDWARE_LPF_COUNT) {
        gyroConfigMutable()->gyro_hardware_lpf = GYRO_HARDWARE_LPF_NORMAL;
    }

    // clear features that are not supported.
    featureDisableImmediate(~featuresSupportedByBuild);

    if (systemConfig()->configurationState == CONFIGURATION_STATE_UNCONFIGURED) {
        // enable some compiled-in features by default
        uint32_t autoFeatures =
            FEATURE_TELEMETRY | FEATURE_OSD | FEATURE_LED_STRIP;
#if defined(USE_SOFTSERIAL)
        // enable softserial if at least one pin is configured
        for (unsigned i = RESOURCE_SOFTSERIAL_OFFSET; i < RESOURCE_SOFTSERIAL_OFFSET + RESOURCE_SOFTSERIAL_COUNT; i++) {
            if (serialPinConfig()->ioTagTx[i] || serialPinConfig()->ioTagRx[i]) {
                autoFeatures |= FEATURE_SOFTSERIAL;
                break;
            }
        }
#endif
        featureEnableImmediate(autoFeatures & featuresSupportedByBuild);
    }

#if defined(USE_BEEPER)
#ifdef USE_TIMER
    if (beeperDevConfig()->frequency && !timerGetConfiguredByTag(beeperDevConfig()->ioTag)) {
        beeperDevConfigMutable()->frequency = 0;
    }
#endif

    if (beeperConfig()->beeper_off_flags & ~BEEPER_ALLOWED_MODES) {
        beeperConfigMutable()->beeper_off_flags = 0;
    }

#ifdef USE_DSHOT
    if (beeperConfig()->dshotBeaconOffFlags & ~DSHOT_BEACON_ALLOWED_MODES) {
        beeperConfigMutable()->dshotBeaconOffFlags = DEFAULT_DSHOT_BEACON_OFF_FLAGS;
    }

    if (beeperConfig()->dshotBeaconTone < DSHOT_CMD_BEACON1
        || beeperConfig()->dshotBeaconTone > DSHOT_CMD_BEACON5) {
        beeperConfigMutable()->dshotBeaconTone = DSHOT_CMD_BEACON1;
    }
#endif
#endif

    bool configuredMotorProtocolDshot = false;
    checkMotorProtocolEnabled(&motorConfig()->dev, &configuredMotorProtocolDshot);
#if defined(USE_DSHOT)
    // If using DSHOT protocol disable unsynched PWM as it's meaningless
    if (configuredMotorProtocolDshot) {
        motorConfigMutable()->dev.useContinuousUpdate = false;
    }

#if defined(USE_DSHOT_TELEMETRY) && defined(USE_TIMER)
    bool nChannelTimerUsed = false;
    for (unsigned i = 0; i < getMotorCount(); i++) {
        const ioTag_t tag = motorConfig()->dev.ioTags[i];
        if (tag) {
            const timerHardware_t *timer = timerGetConfiguredByTag(tag);
            if (timer && timer->output & TIMER_OUTPUT_N_CHANNEL) {
                nChannelTimerUsed = true;

                break;
            }
        }
    }

    if ((!configuredMotorProtocolDshot || (motorConfig()->dev.useDshotBitbang == DSHOT_BITBANG_OFF && (motorConfig()->dev.useBurstDshot == DSHOT_DMAR_ON || nChannelTimerUsed))) && motorConfig()->dev.useDshotTelemetry) {
        motorConfigMutable()->dev.useDshotTelemetry = false;
    }
#endif // USE_DSHOT_TELEMETRY
#endif // USE_DSHOT

#if defined(USE_OSD)
    for (int i = 0; i < OSD_TIMER_COUNT; i++) {
         const uint16_t t = osdConfig()->timers[i];
         if (OSD_TIMER_SRC(t) >= OSD_TIMER_SRC_COUNT ||
                 OSD_TIMER_PRECISION(t) >= OSD_TIMER_PREC_COUNT) {
             osdConfigMutable()->timers[i] = osdTimerDefault[i];
         }
     }
#endif

#if defined(USE_VTX_COMMON) && defined(USE_VTX_TABLE)
    // reset vtx band, channel, power if outside range specified by vtxtable
    if (vtxSettingsConfig()->channel > vtxTableConfig()->channels) {
        vtxSettingsConfigMutable()->channel = 0;
        if (vtxSettingsConfig()->band > 0) {
            vtxSettingsConfigMutable()->freq = 0; // band/channel determined frequency can't be valid anymore
        }
    }
    if (vtxSettingsConfig()->band > vtxTableConfig()->bands) {
        vtxSettingsConfigMutable()->band = 0;
        vtxSettingsConfigMutable()->freq = 0; // band/channel determined frequency can't be valid anymore
    }
    if (vtxSettingsConfig()->power > vtxTableConfig()->powerLevels) {
        vtxSettingsConfigMutable()->power = 0;
    }
#endif

    validateAndFixRatesSettings();  // constrain the various rates settings to limits imposed by the rates type

    // validate battery profile voltages and field bounds, reset to defaults if invalid
    for (unsigned i = 0; i < BATTERY_PROFILE_COUNT; i++) {
        const batteryProfile_t *profile = batteryProfiles(i);
        if (profile->vbatmincellvoltage >= profile->vbatmaxcellvoltage
            || profile->vbatwarningcellvoltage < profile->vbatmincellvoltage
            || profile->vbatwarningcellvoltage > profile->vbatmaxcellvoltage
            || profile->vbatfullcellvoltage < profile->vbatwarningcellvoltage
            || profile->vbatfullcellvoltage > profile->vbatmaxcellvoltage) {
            batteryProfilesMutable(i)->vbatmincellvoltage = VBAT_CELL_VOLTAGE_DEFAULT_MIN;
            batteryProfilesMutable(i)->vbatmaxcellvoltage = VBAT_CELL_VOLTAGE_DEFAULT_MAX;
            batteryProfilesMutable(i)->vbatwarningcellvoltage = 350;
            batteryProfilesMutable(i)->vbatfullcellvoltage = 410;
        }
        if (profile->forceBatteryCellCount > 24) {
            batteryProfilesMutable(i)->forceBatteryCellCount = 0;
        }
        if (profile->consumptionWarningPercentage > 100) {
            batteryProfilesMutable(i)->consumptionWarningPercentage = 10;
        }
    }

#ifdef USE_MSP_DISPLAYPORT
    // Find the first serial port on which MSP Displayport is enabled
    displayPortMspSetSerial(SERIAL_PORT_NONE);

    for (const serialPortConfig_t *portConfig = serialConfig()->portConfigs;
         portConfig < ARRAYEND(serialConfig()->portConfigs);
         portConfig++) {
        if ((portConfig->identifier != SERIAL_PORT_USB_VCP)
            && ((portConfig->functionMask & (FUNCTION_VTX_MSP | FUNCTION_MSP)) == (FUNCTION_VTX_MSP | FUNCTION_MSP))) {
            displayPortMspSetSerial(portConfig->identifier);
            break;
        }
    }
#endif

#ifdef USE_BLACKBOX
    validateAndFixBlackBox();
#endif // USE_BLACKBOX

#if defined(TARGET_VALIDATECONFIG)
    // This should be done at the end of the validation
    targetValidateConfiguration();
#endif
}

void validateAndFixGyroConfig(void)
{
    // Fix gyro filter settings to handle cases where an older configurator was used that
    // allowed higher cutoff limits from previous firmware versions.
    adjustFilterLimit(&gyroConfigMutable()->gyro_lpf1_static_hz, LPF_MAX_HZ);
    adjustFilterLimit(&gyroConfigMutable()->gyro_lpf2_static_hz, LPF_MAX_HZ);
    adjustFilterLimit(&gyroConfigMutable()->gyro_soft_notch_hz_1, LPF_MAX_HZ);
    adjustFilterLimit(&gyroConfigMutable()->gyro_soft_notch_cutoff_1, 0);
    adjustFilterLimit(&gyroConfigMutable()->gyro_soft_notch_hz_2, LPF_MAX_HZ);
    adjustFilterLimit(&gyroConfigMutable()->gyro_soft_notch_cutoff_2, 0);

    // Prevent invalid notch cutoff
    if (gyroConfig()->gyro_soft_notch_cutoff_1 >= gyroConfig()->gyro_soft_notch_hz_1) {
        gyroConfigMutable()->gyro_soft_notch_hz_1 = 0;
    }
    if (gyroConfig()->gyro_soft_notch_cutoff_2 >= gyroConfig()->gyro_soft_notch_hz_2) {
        gyroConfigMutable()->gyro_soft_notch_hz_2 = 0;
    }

    if (gyro.sampleRateHz > 0) {
        float samplingTime = 1.0f / gyro.sampleRateHz;

        // check for looptime restrictions based on motor protocol. Motor times have safety margin
        float motorUpdateRestriction;

#if defined(USE_DSHOT) && defined(USE_PID_DENOM_CHECK)
        /* If bidirectional DSHOT is being used on an F4 or G4 then force DSHOT300. The motor update restrictions then applied
         * will automatically consider the loop time and adjust pid_process_denom appropriately
         */
        if (true
#ifdef USE_PID_DENOM_OVERCLOCK_LEVEL
        && (systemConfig()->cpu_overclock < USE_PID_DENOM_OVERCLOCK_LEVEL)
#endif
        && motorConfig()->dev.useDshotTelemetry
        ) {
            if (motorConfig()->dev.motorProtocol == MOTOR_PROTOCOL_DSHOT600) {
                motorConfigMutable()->dev.motorProtocol = MOTOR_PROTOCOL_DSHOT300;
            }
            if (gyro.sampleRateHz > 4000) {
                pidConfigMutable()->pid_process_denom = MAX(2, pidConfig()->pid_process_denom);
            }
        }
#endif // USE_DSHOT && USE_PID_DENOM_CHECK
        switch (motorConfig()->dev.motorProtocol) {
        case MOTOR_PROTOCOL_PWM :
                motorUpdateRestriction = 1.0f / BRUSHLESS_MOTORS_PWM_RATE;
                break;
        case MOTOR_PROTOCOL_ONESHOT125:
                motorUpdateRestriction = 0.0005f;
                break;
        case MOTOR_PROTOCOL_ONESHOT42:
                motorUpdateRestriction = 0.0001f;
                break;
#ifdef USE_DSHOT
        case MOTOR_PROTOCOL_DSHOT150:
                motorUpdateRestriction = 0.000250f;
                break;
        case MOTOR_PROTOCOL_DSHOT300:
                motorUpdateRestriction = 0.0001f;
                break;
#endif
        default:
            motorUpdateRestriction = 0.00003125f;
            break;
        }

        if (motorConfig()->dev.useContinuousUpdate) {
            bool configuredMotorProtocolDshot = false;
            checkMotorProtocolEnabled(&motorConfig()->dev, &configuredMotorProtocolDshot);
            // Prevent overriding the max rate of motors
            if (!configuredMotorProtocolDshot && motorConfig()->dev.motorProtocol != MOTOR_PROTOCOL_PWM ) {
                const uint32_t maxEscRate = lrintf(1.0f / motorUpdateRestriction);
                motorConfigMutable()->dev.motorPwmRate = MIN(motorConfig()->dev.motorPwmRate, maxEscRate);
            }
        } else {
            const float pidLooptime = samplingTime * pidConfig()->pid_process_denom;
            if (motorConfig()->dev.useDshotTelemetry) {
                motorUpdateRestriction *= 2;
            }
            if (pidLooptime < motorUpdateRestriction) {
                uint8_t minPidProcessDenom = motorUpdateRestriction / samplingTime;
                if (motorUpdateRestriction / samplingTime > minPidProcessDenom) {
                    // if any fractional part then round up
                    minPidProcessDenom++;
                }
                minPidProcessDenom = constrain(minPidProcessDenom, 1, MAX_PID_PROCESS_DENOM);
                pidConfigMutable()->pid_process_denom = MAX(pidConfigMutable()->pid_process_denom, minPidProcessDenom);
            }
        }
    }

    if (systemConfig()->activeRateProfile >= CONTROL_RATE_PROFILE_COUNT) {
        systemConfigMutable()->activeRateProfile = 0;
    }
    loadControlRateProfile();

    if (systemConfig()->pidProfileIndex >= PID_PROFILE_COUNT) {
        systemConfigMutable()->pidProfileIndex = 0;
    }
    loadPidProfile();

    if (systemConfig()->activeBatteryProfile >= BATTERY_PROFILE_COUNT) {
        systemConfigMutable()->activeBatteryProfile = 0;
    }
    loadBatteryProfile();

}

#ifdef USE_BLACKBOX
void validateAndFixBlackBox(void) {
#ifndef USE_FLASHFS
    if (blackboxConfig()->device == BLACKBOX_DEVICE_FLASH) {
        blackboxConfigMutable()->device = BLACKBOX_DEVICE_NONE;
    }
#endif // USE_FLASHFS

    if (blackboxConfig()->device == BLACKBOX_DEVICE_SDCARD) {
#if defined(USE_SDCARD)
        if (!sdcardConfig()->mode)
#endif
        {
            blackboxConfigMutable()->device = BLACKBOX_DEVICE_NONE;
        }
    }
}
#endif // USE_BLACKBOX

bool readEEPROM(void)
{
    suspendRxSignal();

    // Sanity check, read flash
    bool success = loadEEPROM();

    featureInit();

    validateAndFixConfig();

    activateConfig();

    resumeRxSignal();

    return success;
}

void writeUnmodifiedConfigToEEPROM(void)
{
    validateAndFixConfig();

    suspendRxSignal();
    eepromWriteInProgress = true;
    writeConfigToEEPROM();
    eepromWriteInProgress = false;
    resumeRxSignal();
    configIsDirty = false;
}

void writeEEPROM(void)
{
    systemConfigMutable()->configurationState = CONFIGURATION_STATE_CONFIGURED;

    writeUnmodifiedConfigToEEPROM();
}

bool resetEEPROM(void)
{
    resetConfig();

    writeUnmodifiedConfigToEEPROM();

    return true;
}

void ensureEEPROMStructureIsValid(void)
{
    if (isEEPROMStructureValid()) {
        return;
    }
    resetEEPROM();
}

void saveConfigAndNotify(void)
{
    // The write to EEPROM will cause a big delay in the current task, so ignore
    schedulerIgnoreTaskExecTime();

    writeEEPROM();
    readEEPROM();
    beeperConfirmationBeeps(1);
}

void setConfigDirty(void)
{
    configIsDirty = true;
}

bool isConfigDirty(void)
{
    return configIsDirty;
}

void changePidProfile(uint8_t pidProfileIndex)
{
    // The config switch will cause a big enough delay in the current task to upset the scheduler
    schedulerIgnoreTaskExecTime();

    if (pidProfileIndex < PID_PROFILE_COUNT) {
        systemConfigMutable()->pidProfileIndex = pidProfileIndex;
        loadPidProfile();

        pidInit(currentPidProfile);
    }

    beeperConfirmationBeeps(pidProfileIndex + 1);
}

bool isSystemConfigured(void)
{
    return systemConfig()->configurationState == CONFIGURATION_STATE_CONFIGURED;
}

void setRebootRequired(void)
{
    rebootRequired = true;
    setArmingDisabled(ARMING_DISABLED_REBOOT_REQUIRED);
}

bool getRebootRequired(void)
{
    return rebootRequired;
}
