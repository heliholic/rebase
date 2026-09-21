# Parameter group (PG) format

Rotorflight stores its configuration as a sequence of *parameter groups*. A
group is a plain C struct; the bytes the flight controller writes to EEPROM are
that struct's in-memory image, so the layout below **is** the storage format.

This document is generated from the compiled firmware's debug info by
`src/utils/pg_dump_structs.py`. It covers every group, every struct nested
inside one, and every enum whose values are stored in one.

## How a group is stored

The saved config is a header, a run of records, a terminator, and a checksum:

| Part | Fields |
| --- | --- |
| Header | `uint32_t magic`, `uint32_t version` |
| Record (repeated) | `uint32_t hash`, `uint16_t size`, `uint8_t pg[]` |
| Footer | `uint32_t terminator` (zero) |
| Checksum | CRC-16 over everything above |

Every part is `__attribute__((packed))`: there is no padding *between* the
record header and the group payload, and none between records. Padding *within*
a group is real, is stored, and is listed in the tables below.

## How a group is identified

`hash` is the only identity a record carries. On load, the firmware walks the
records and matches each one against the registered groups by hash alone
(`findEEPROM()` in `src/main/config/config_eeprom.c`); a record whose hash
matches nothing is skipped, and a group with no matching record keeps its
defaults.

The hash is a 32-bit FNV-1 over a canonical fingerprint of the group: its PGN
plus its fully resolved layout - every member's offset, size, name and type,
recursively, including enumerator names and values. So **any** change to a
group's layout changes its hash, and an old record is then ignored rather than
misread. A hash of zero is rejected at build time, because zero is the
terminator.

Two properties are deliberately *not* in the hash:

- **The element count of a `PG_DECLARE_ARRAY` group.** Those counts are
  target-derived (`SPIDEV_COUNT`, `I2CDEV_COUNT`, ...) and differ per MCU.
  `pgLoad()` reconciles a count change by copying `MIN(stored, current)`
  elements and leaving the rest at their defaults. The tables below therefore
  name the length macro instead of a number.
- **The group's `size`.** It is stored in the record and used to bound the
  copy, but the hash already pins the layout.

## Reading the tables

`Offset` and `Size` are bytes from the start of the group (for an array group,
from the start of one element). Padding rows are the bytes the compiler
inserts; they are stored as-is and their contents are undefined. Members of an
anonymous `struct`/`union` are indented under it and share its offsets.

## Parameter groups

76 groups, 14 nested structures, 19 enumerations.

| Group | PGN | Hash | Type | Size | Header |
| --- | ---: | --- | --- | ---: | --- |
| `accelerometerConfig` | 35 | `0x3071B171` | [`accelerometerConfig_t`](#accelerometerconfig_t) | 16 | `pg/acceleration.h` |
| `adcConfig` | 510 | `0x278550EC` | [`adcConfig_t`](#adcconfig_t) | 26 | `pg/adc.h` |
| `adjustmentRanges` | 37 | `0xDE2A216C` | [`adjustmentRange_t`](#adjustmentrange_t) | 16 &times; `MAX_ADJUSTMENT_RANGE_COUNT` | `pg/adjustments.h` |
| `armingConfig` | 16 | `0x3D004D91` | [`armingConfig_t`](#armingconfig_t) | 5 | `pg/arming.h` |
| `barometerConfig` | 38 | `0x358757A4` | [`barometerConfig_t`](#barometerconfig_t) | 8 | `pg/barometer.h` |
| `batteryConfig` | 11 | `0x7E75E919` | [`batteryConfig_t`](#batteryconfig_t) | 14 | `pg/battery.h` |
| `batteryProfiles` | 561 | `0x561E7785` | [`batteryProfile_t`](#batteryprofile_t) | 22 &times; `BATTERY_PROFILE_COUNT` | `pg/battery.h` |
| `beeperConfig` | 502 | `0x937634D6` | [`beeperConfig_t`](#beeperconfig_t) | 12 | `pg/beeper.h` |
| `beeperDevConfig` | 503 | `0x8A1FED2B` | [`beeperDevConfig_t`](#beeperdevconfig_t) | 6 | `pg/beeper_dev.h` |
| `blackboxConfig` | 5 | `0xCA44FF83` | [`blackboxConfig_t`](#blackboxconfig_t) | 12 | `pg/blackbox.h` |
| `boardAlignment` | 2 | `0xAC3A314A` | [`alignment_u`](#alignment_u) | 6 | `pg/alignment.h` |
| `boardConfig` | 538 | `0xE3C054A1` | [`boardConfig_t`](#boardconfig_t) | 27 | `pg/board.h` |
| `canConfig` | 564 | `0x5FA345A7` | [`canConfig_t`](#canconfig_t) | 2 | `pg/can.h` |
| `canPinConfig` | 563 | `0x21890503` | [`canPinConfig_t`](#canpinconfig_t) | 3 &times; `CANDEV_COUNT` | `pg/can.h` |
| `compassConfig` | 40 | `0xF9FD2DDC` | [`compassConfig_t`](#compassconfig_t) | 22 | `pg/compass.h` |
| `currentSensorADCConfig` | 256 | `0xFAFE3B75` | [`currentSensorADCConfig_t`](#currentsensoradcconfig_t) | 4 | `pg/current.h` |
| `customBoxConfig` | 553 | `0x7C447B48` | [`customBoxConfig_t`](#customboxconfig_t) | 16 &times; `BOX_USER_COUNT` | `pg/modes.h` |
| `dashboardConfig` | 519 | `0x8765A83F` | [`dashboardConfig_t`](#dashboardconfig_t) | 2 | `pg/dashboard.h` |
| `dronecanConfig` | 565 | `0x0A37AC83` | [`dronecanConfig_t`](#dronecanconfig_t) | 8 | `pg/dronecan.h` |
| `dronecanDnaConfig` | 567 | `0x255EBADA` | [`dronecanDnaConfig_t`](#dronecandnaconfig_t) | 272 | `pg/dronecan_dna.h` |
| `escSensorConfig` | 517 | `0x9FB60FDD` | [`escSensorConfig_t`](#escsensorconfig_t) | 4 | `pg/esc_sensor.h` |
| `escSerialConfig` | 521 | `0x859F7320` | [`escSerialConfig_t`](#escserialconfig_t) | 1 | `pg/esc_serial.h` |
| `failsafeConfig` | 1 | `0x802081D7` | [`failsafeConfig_t`](#failsafeconfig_t) | 10 | `pg/failsafe.h` |
| `featureConfig` | 19 | `0x9C1E8EBD` | [`featureConfig_t`](#featureconfig_t) | 4 | `pg/feature.h` |
| `flashConfig` | 506 | `0x0FCA0D96` | [`flashConfig_t`](#flashconfig_t) | 4 | `pg/flash.h` |
| `gpsConfig` | 30 | `0x74F0A839` | [`gpsConfig_t`](#gpsconfig_t) | 78 | `pg/gps.h` |
| `gyroConfig` | 10 | `0x3512C78A` | [`gyroConfig_t`](#gyroconfig_t) | 26 | `pg/gyro.h` |
| `gyroDeviceConfig` | 540 | `0x6DD87CDA` | [`gyroDeviceConfig_t`](#gyrodeviceconfig_t) | 16 &times; `MAX_GYRODEV_COUNT` | `pg/gyrodev.h` |
| `i2cConfig` | 518 | `0xB026B875` | [`i2cConfig_t`](#i2cconfig_t) | 6 &times; `I2CDEV_COUNT` | `pg/bus_i2c.h` |
| `imuConfig` | 22 | `0xBE4FB408` | [`imuConfig_t`](#imuconfig_t) | 10 | `pg/imu.h` |
| `ledStripConfig` | 27 | `0xC62B80FD` | [`ledStripConfig_t`](#ledstripconfig_t) | 16 | `pg/ledstrip.h` |
| `ledStripStatusModeConfig` | 545 | `0x34ECEA5E` | [`ledStripStatusModeConfig_t`](#ledstripstatusmodeconfig_t) | 368 | `pg/ledstrip.h` |
| `mcoConfig` | 541 | `0x010C2C94` | [`mcoConfig_t`](#mcoconfig_t) | 3 &times; `2` | `pg/mco.h` |
| `mixerConfig` | 20 | `0x80E59A46` | [`mixerConfig_t`](#mixerconfig_t) | 20 | `pg/mixer.h` |
| `mixerInputs` | 15 | `0x7838A8CF` | [`mixerInput_t`](#mixerinput_t) | 6 &times; `MIXER_INPUT_COUNT` | `pg/mixer.h` |
| `mixerRules` | 21 | `0x2C09DFA2` | [`mixerRule_t`](#mixerrule_t) | 8 &times; `MIXER_RULE_COUNT` | `pg/mixer.h` |
| `modeActivationConditions` | 41 | `0x55454DC6` | [`modeActivationCondition_t`](#modeactivationcondition_t) | 6 &times; `MAX_MODE_ACTIVATION_CONDITION_COUNT` | `pg/modes.h` |
| `motorConfig` | 6 | `0xF83A5DC8` | [`motorConfig_t`](#motorconfig_t) | 32 | `pg/motor.h` |
| `mspConfig` | 557 | `0xAC040DC9` | [`mspConfig_t`](#mspconfig_t) | 1 | `pg/msp.h` |
| `opticalflowConfig` | 560 | `0x258240D2` | [`opticalflowConfig_t`](#opticalflowconfig_t) | 8 | `pg/opticalflow.h` |
| `pidConfig` | 504 | `0x77A9E9E6` | [`pidConfig_t`](#pidconfig_t) | 2 | `pg/pid.h` |
| `pidProfiles` | 14 | `0x97F727C6` | [`pidProfile_t`](#pidprofile_t) | 40 &times; `PID_PROFILE_COUNT` | `pg/pid.h` |
| `pilotConfig` | 47 | `0xDFA805D5` | [`pilotConfig_t`](#pilotconfig_t) | 34 | `pg/pilot.h` |
| `pinPulldownConfig` | 552 | `0x8EA458C5` | [`pinPullUpDownConfig_t`](#pinpullupdownconfig_t) | 1 &times; `PIN_PULL_UP_DOWN_COUNT` | `pg/pin_pull_up_down.h` |
| `pinPullupConfig` | 551 | `0x6CC75258` | [`pinPullUpDownConfig_t`](#pinpullupdownconfig_t) | 1 &times; `PIN_PULL_UP_DOWN_COUNT` | `pg/pin_pull_up_down.h` |
| `pinioBoxConfig` | 530 | `0xAA658C54` | [`pinioBoxConfig_t`](#pinioboxconfig_t) | 4 | `pg/piniobox.h` |
| `pinioConfig` | 529 | `0x7945F9DD` | [`pinioConfig_t`](#pinioconfig_t) | 8 | `pg/pinio.h` |
| `positionConfig` | 56 | `0x13BEC47A` | [`positionConfig_t`](#positionconfig_t) | 8 | `pg/position.h` |
| `ppmConfig` | 507 | `0xF9CF641C` | [`ppmConfig_t`](#ppmconfig_t) | 1 | `pg/rx_pwm.h` |
| `quadSpiConfig` | 548 | `0x7AB0CAD9` | [`quadSpiConfig_t`](#quadspiconfig_t) | 13 &times; `QUADSPIDEV_COUNT` | `pg/bus_quadspi.h` |
| `rangefinderConfig` | 527 | `0x32917E98` | [`rangefinderConfig_t`](#rangefinderconfig_t) | 1 | `pg/rangefinder.h` |
| `rateProfiles` | 12 | `0x57F65935` | [`rateProfile_t`](#rateprofile_t) | 54 &times; `RATE_PROFILE_COUNT` | `pg/rates.h` |
| `rcControlsConfig` | 25 | `0x079977AB` | [`rcControlsConfig_t`](#rccontrolsconfig_t) | 16 | `pg/rc_controls.h` |
| `rxChannelRangeConfigs` | 44 | `0x198E9D36` | [`rxChannelRangeConfig_t`](#rxchannelrangeconfig_t) | 4 &times; `RC_CONTROL_COUNT` | `pg/rx.h` |
| `rxConfig` | 24 | `0xDDF454AE` | [`rxConfig_t`](#rxconfig_t) | 30 | `pg/rx.h` |
| `rxFailsafeChannelConfigs` | 43 | `0xF440899F` | [`rxFailsafeChannelConfig_t`](#rxfailsafechannelconfig_t) | 2 &times; `MAX_SUPPORTED_RC_CHANNEL_COUNT` | `pg/rx.h` |
| `schedulerConfig` | 556 | `0xDE846B81` | [`schedulerConfig_t`](#schedulerconfig_t) | 6 | `pg/scheduler.h` |
| `sdcardConfig` | 511 | `0x20EF4804` | [`sdcardConfig_t`](#sdcardconfig_t) | 5 | `pg/sdcard.h` |
| `sdioConfig` | 532 | `0x9742A850` | [`sdioConfig_t`](#sdioconfig_t) | 5 | `pg/sdio.h` |
| `sdioPinConfig` | 550 | `0xD7289D0C` | [`sdioPinConfig_t`](#sdiopinconfig_t) | 6 | `pg/sdio.h` |
| `serialConfig` | 13 | `0x3D257838` | [`serialConfig_t`](#serialconfig_t) | 244 | `pg/serial.h` |
| `serialPinConfig` | 509 | `0xFBA6E927` | [`serialPinConfig_t`](#serialpinconfig_t) | 74 | `pg/serial_port.h` |
| `serialUartConfig` | 543 | `0x9B5CDD0F` | [`serialUartConfig_t`](#serialuartconfig_t) | 2 &times; `UARTDEV_CONFIG_MAX` | `pg/serial_uart.h` |
| `servoConfig` | 52 | `0xB12C2058` | [`servoConfig_t`](#servoconfig_t) | 12 | `pg/servo.h` |
| `servoParams` | 42 | `0x1704EAF5` | [`servoParam_t`](#servoparam_t) | 16 &times; `MAX_SUPPORTED_SERVOS` | `pg/servo.h` |
| `sonarConfig` | 516 | `0xA17AFF25` | [`sonarConfig_t`](#sonarconfig_t) | 2 | `pg/rangefinder.h` |
| `spiPinConfig` | 520 | `0x81954E3F` | [`spiPinConfig_t`](#spipinconfig_t) | 5 &times; `SPIDEV_COUNT` | `pg/bus_spi.h` |
| `statsConfig` | 547 | `0x87526EDD` | [`statsConfig_t`](#statsconfig_t) | 24 | `pg/stats.h` |
| `statusLedConfig` | 505 | `0x81FFF062` | [`statusLedConfig_t`](#statusledconfig_t) | 4 | `pg/leds.h` |
| `systemConfig` | 18 | `0xA315597B` | [`systemConfig_t`](#systemconfig_t) | 18 | `pg/system.h` |
| `telemetryConfig` | 31 | `0xBD6D2B1A` | [`telemetryConfig_t`](#telemetryconfig_t) | 44 | `pg/telemetry.h` |
| `timeConfig` | 526 | `0x4733E1A6` | [`timeConfig_t`](#timeconfig_t) | 2 | `pg/time.h` |
| `timerIOConfig` | 534 | `0x09953C82` | [`timerIOConfig_t`](#timerioconfig_t) | 3 &times; `MAX_TIMER_PINMAP_COUNT` | `pg/timerio.h` |
| `timerUpConfig` | 549 | `0x838D1137` | [`timerUpConfig_t`](#timerupconfig_t) | 1 &times; `HARDWARE_TIMER_DEFINITION_COUNT` | `pg/timerup.h` |
| `usbDevConfig` | 531 | `0x6CDB14DB` | [`usbDev_t`](#usbdev_t) | 4 | `pg/usb.h` |
| `voltageSensorADCConfig` | 258 | `0xBB15461C` | [`voltageSensorADCConfig_t`](#voltagesensoradcconfig_t) | 3 &times; `MAX_VOLTAGE_SENSOR_ADC` | `pg/voltage.h` |

## Group layouts

### accelerometerConfig_t

- `accelerometerConfig` &mdash; PGN 35 (`PG_ACCELEROMETER_CONFIG`), hash `0x3071B171`

`sizeof` = 16 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `acc_lpf_hz` |
| 2 | 1 | `uint8_t` | `acc_hardware` |
| 3 | 1 | `bool` | `acc_high_fsr` |
| 4 | 8 | [`flightDynamicsTrims_u`](#flightdynamicstrims_u) | `accZero` |
| 12 | 4 | [`rollAndPitchTrims_t`](#rollandpitchtrims_t) | `accelerometerTrims` |

### adcConfig_t

- `adcConfig` &mdash; PGN 510 (`PG_ADC_CONFIG`), hash `0x278550EC`

`sizeof` = 26 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 3 | [`adcChannelConfig_t`](#adcchannelconfig_t) | `vbat` |
| 3 | 3 | [`adcChannelConfig_t`](#adcchannelconfig_t) | `rssi` |
| 6 | 3 | [`adcChannelConfig_t`](#adcchannelconfig_t) | `current` |
| 9 | 3 | [`adcChannelConfig_t`](#adcchannelconfig_t) | `external1` |
| 12 | 1 | `int8_t` | `device` |
| 13 | 1 | &mdash; | *(padding)* |
| 14 | 2 | `uint16_t` | `vrefIntCalibration` |
| 16 | 2 | `uint16_t` | `tempSensorCalibration1` |
| 18 | 2 | `uint16_t` | `tempSensorCalibration2` |
| 20 | 6 | `int8_t` | `dmaopt[6]` |

### adjustmentRange_t

- `adjustmentRanges` &mdash; PGN 37 (`PG_ADJUSTMENT_RANGE_CONFIG`), hash `0xDE2A216C` &mdash; array of `MAX_ADJUSTMENT_RANGE_COUNT` elements

`sizeof` = 16 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `function` |
| 1 | 1 | `uint8_t` | `enaChannel` |
| 2 | 2 | [`channelRange_t`](#channelrange_t) | `enaRange` |
| 4 | 1 | `uint8_t` | `adjChannel` |
| 5 | 2 | [`channelRange_t`](#channelrange_t) | `adjRange1` |
| 7 | 2 | [`channelRange_t`](#channelrange_t) | `adjRange2` |
| 9 | 1 | &mdash; | *(padding)* |
| 10 | 2 | `int16_t` | `adjMin` |
| 12 | 2 | `int16_t` | `adjMax` |
| 14 | 1 | `uint8_t` | `adjStep` |
| 15 | 1 | &mdash; | *(tail padding)* |

### alignment_u

- `boardAlignment` &mdash; PGN 2 (`PG_BOARD_ALIGNMENT`), hash `0xAC3A314A`

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 6 | `int16_t` | `raw[3]` |
| 0 | 6 | anonymous `struct` | *(unnamed)* |
| 0 | 2 | `int16_t` | &nbsp;&nbsp;&nbsp;&nbsp;&#8627; `roll` |
| 2 | 2 | `int16_t` | &nbsp;&nbsp;&nbsp;&nbsp;&#8627; `pitch` |
| 4 | 2 | `int16_t` | &nbsp;&nbsp;&nbsp;&nbsp;&#8627; `yaw` |

### armingConfig_t

- `armingConfig` &mdash; PGN 16 (`PG_ARMING_CONFIG`), hash `0x3D004D91`

`sizeof` = 5 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `bool` | `gyro_cal_on_first_arm` |
| 1 | 1 | `bool` | `prearm_allow_rearm` |
| 2 | 1 | `bool` | `enable_stick_commands` |
| 3 | 2 | [`channelRange_t`](#channelrange_t) | `arm_range` |

### barometerConfig_t

- `barometerConfig` &mdash; PGN 38 (`PG_BAROMETER_CONFIG`), hash `0x358757A4`

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `baro_busType` |
| 1 | 1 | `uint8_t` | `baro_spi_device` |
| 2 | 1 | `ioTag_t` | `baro_spi_csn` |
| 3 | 1 | `uint8_t` | `baro_i2c_device` |
| 4 | 1 | `uint8_t` | `baro_i2c_address` |
| 5 | 1 | `uint8_t` | `baro_hardware` |
| 6 | 1 | `ioTag_t` | `baro_eoc_tag` |
| 7 | 1 | `ioTag_t` | `baro_xclr_tag` |

### batteryConfig_t

- `batteryConfig` &mdash; PGN 11 (`PG_BATTERY_CONFIG`), hash `0x7E75E919`

`sizeof` = 14 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `vbatnotpresentcellvoltage` |
| 2 | 1 | `uint8_t` | `lvcPercentage` |
| 3 | 1 | [`voltageMeterSource_e`](#voltagemetersource_e) | `voltageMeterSource` |
| 4 | 1 | [`currentMeterSource_e`](#currentmetersource_e) | `currentMeterSource` |
| 5 | 1 | `bool` | `useVBatAlerts` |
| 6 | 1 | `bool` | `useConsumptionAlerts` |
| 7 | 1 | `uint8_t` | `vbathysteresis` |
| 8 | 1 | `uint8_t` | `vbatDisplayLpfPeriod` |
| 9 | 1 | `uint8_t` | `ibatLpfPeriod` |
| 10 | 1 | `uint8_t` | `vbatDurationForWarning` |
| 11 | 1 | `uint8_t` | `vbatDurationForCritical` |
| 12 | 1 | `bool` | `isBatteryContinueEnabled` |
| 13 | 1 | &mdash; | *(tail padding)* |

### batteryProfile_t

- `batteryProfiles` &mdash; PGN 561 (`PG_BATTERY_PROFILES`), hash `0x561E7785` &mdash; array of `BATTERY_PROFILE_COUNT` elements

`sizeof` = 22 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `vbatmaxcellvoltage` |
| 2 | 2 | `uint16_t` | `vbatmincellvoltage` |
| 4 | 2 | `uint16_t` | `vbatwarningcellvoltage` |
| 6 | 2 | `uint16_t` | `vbatfullcellvoltage` |
| 8 | 2 | `uint16_t` | `batteryCapacity` |
| 10 | 1 | `uint8_t` | `forceBatteryCellCount` |
| 11 | 1 | `uint8_t` | `consumptionWarningPercentage` |
| 12 | 9 | `char` | `profileName[9]` |
| 21 | 1 | &mdash; | *(tail padding)* |

### beeperConfig_t

- `beeperConfig` &mdash; PGN 502 (`PG_BEEPER_CONFIG`), hash `0x937634D6`

`sizeof` = 12 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 4 | `uint32_t` | `beeper_off_flags` |
| 4 | 1 | `uint8_t` | `dshotBeaconTone` |
| 5 | 3 | &mdash; | *(padding)* |
| 8 | 4 | `uint32_t` | `dshotBeaconOffFlags` |

### beeperDevConfig_t

- `beeperDevConfig` &mdash; PGN 503 (`PG_BEEPER_DEV_CONFIG`), hash `0x8A1FED2B`

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTag` |
| 1 | 1 | `bool` | `isInverted` |
| 2 | 1 | `bool` | `isOpenDrain` |
| 3 | 1 | &mdash; | *(padding)* |
| 4 | 2 | `uint16_t` | `frequency` |

### blackboxConfig_t

- `blackboxConfig` &mdash; PGN 5 (`PG_BLACKBOX_CONFIG`), hash `0xCA44FF83`

`sizeof` = 12 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | [`BlackboxDevice_e`](#blackboxdevice_e) | `device` |
| 1 | 1 | [`BlackboxMode_e`](#blackboxmode_e) | `mode` |
| 2 | 2 | `uint16_t` | `denom` |
| 4 | 4 | `uint32_t` | `fields` |
| 8 | 2 | `uint16_t` | `initialErase` |
| 10 | 1 | `bool` | `rollingErase` |
| 11 | 1 | `uint8_t` | `gracePeriod` |

### boardConfig_t

- `boardConfig` &mdash; PGN 538 (`PG_BOARD_CONFIG`), hash `0xE3C054A1`

`sizeof` = 27 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 5 | `char` | `manufacturerId[5]` |
| 5 | 21 | `char` | `boardName[21]` |
| 26 | 1 | `bool` | `boardInformationSet` |

### canConfig_t

- `canConfig` &mdash; PGN 564 (`PG_CAN_CONFIG`), hash `0x5FA345A7`

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `bitrate_khz` |

### canPinConfig_t

- `canPinConfig` &mdash; PGN 563 (`PG_CAN_PIN_CONFIG`), hash `0x21890503` &mdash; array of `CANDEV_COUNT` elements

`sizeof` = 3 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTagTx` |
| 1 | 1 | `ioTag_t` | `ioTagRx` |
| 2 | 1 | `ioTag_t` | `ioTagSilent` |

### compassConfig_t

- `compassConfig` &mdash; PGN 40 (`PG_COMPASS_CONFIG`), hash `0xF9FD2DDC`

`sizeof` = 22 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `mag_alignment` |
| 1 | 1 | `uint8_t` | `mag_hardware` |
| 2 | 1 | `uint8_t` | `mag_busType` |
| 3 | 1 | `uint8_t` | `mag_i2c_device` |
| 4 | 1 | `uint8_t` | `mag_i2c_address` |
| 5 | 1 | `uint8_t` | `mag_spi_device` |
| 6 | 1 | `ioTag_t` | `mag_spi_csn` |
| 7 | 1 | `ioTag_t` | `interruptTag` |
| 8 | 8 | [`flightDynamicsTrims_u`](#flightdynamicstrims_u) | `magZero` |
| 16 | 6 | [`alignment_u`](#alignment_u) | `mag_customAlignment` |

### currentSensorADCConfig_t

- `currentSensorADCConfig` &mdash; PGN 256 (`PG_CURRENT_SENSOR_ADC_CONFIG`), hash `0xFAFE3B75`

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `int16_t` | `scale` |
| 2 | 2 | `int16_t` | `offset` |

### customBoxConfig_t

- `customBoxConfig` &mdash; PGN 553 (`PG_CUSTOM_BOX_CONFIG`), hash `0x7C447B48` &mdash; array of `BOX_USER_COUNT` elements

`sizeof` = 16 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 16 | `char` | `name[16]` |

### dashboardConfig_t

- `dashboardConfig` &mdash; PGN 519 (`PG_DASHBOARD_CONFIG`), hash `0x8765A83F`

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `device` |
| 1 | 1 | `uint8_t` | `address` |

### dronecanConfig_t

- `dronecanConfig` &mdash; PGN 565 (`PG_DRONECAN_CONFIG`), hash `0x0A37AC83`

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `bool` | `enabled` |
| 1 | 1 | `uint8_t` | `node_id` |
| 2 | 1 | `uint8_t` | `device` |
| 3 | 1 | &mdash; | *(padding)* |
| 4 | 2 | `uint16_t` | `esc_rate_hz` |
| 6 | 1 | `bool` | `dna_enabled` |
| 7 | 1 | &mdash; | *(tail padding)* |

### dronecanDnaConfig_t

- `dronecanDnaConfig` &mdash; PGN 567 (`PG_DRONECAN_DNA_CONFIG`), hash `0x255EBADA`

`sizeof` = 272 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 272 | [`dronecanDnaEntry_t`](#dronecandnaentry_t) | `entry[16]` |

### escSensorConfig_t

- `escSensorConfig` &mdash; PGN 517 (`PG_ESC_SENSOR_CONFIG`), hash `0x9FB60FDD`

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `bool` | `halfDuplex` |
| 1 | 1 | &mdash; | *(padding)* |
| 2 | 2 | `uint16_t` | `offset` |

### escSerialConfig_t

- `escSerialConfig` &mdash; PGN 521 (`PG_ESCSERIAL_CONFIG`), hash `0x859F7320`

`sizeof` = 1 byte.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTag` |

### failsafeConfig_t

- `failsafeConfig` &mdash; PGN 1 (`PG_FAILSAFE_CONFIG`), hash `0x802081D7`

`sizeof` = 10 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `failsafe_throttle` |
| 2 | 2 | `uint16_t` | `failsafe_throttle_low_delay` |
| 4 | 1 | `uint8_t` | `failsafe_delay` |
| 5 | 1 | `uint8_t` | `failsafe_landing_time` |
| 6 | 1 | [`failsafeSwitchMode_e`](#failsafeswitchmode_e) | `failsafe_switch_mode` |
| 7 | 1 | [`failsafeProcedure_e`](#failsafeprocedure_e) | `failsafe_procedure` |
| 8 | 2 | `uint16_t` | `failsafe_recovery_delay` |

### featureConfig_t

- `featureConfig` &mdash; PGN 19 (`PG_FEATURE_CONFIG`), hash `0x9C1E8EBD`

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 4 | `uint32_t` | `enabledFeatures` |

### flashConfig_t

- `flashConfig` &mdash; PGN 506 (`PG_FLASH_CONFIG`), hash `0x0FCA0D96`

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `csTag` |
| 1 | 1 | `uint8_t` | `spiDevice` |
| 2 | 1 | `uint8_t` | `quadSpiDevice` |
| 3 | 1 | `uint8_t` | `octoSpiDevice` |

### gpsConfig_t

- `gpsConfig` &mdash; PGN 30 (`PG_GPS_CONFIG`), hash `0x74F0A839`

`sizeof` = 78 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `provider` |
| 1 | 1 | `uint8_t` | `sbasMode` |
| 2 | 1 | `bool` | `autoConfig` |
| 3 | 1 | `bool` | `autoBaud` |
| 4 | 1 | `uint8_t` | `gps_ublox_acquire_model` |
| 5 | 1 | `uint8_t` | `gps_ublox_flight_model` |
| 6 | 1 | `uint8_t` | `gps_update_rate_hz` |
| 7 | 1 | `bool` | `gps_ublox_use_galileo` |
| 8 | 1 | `bool` | `gps_set_home_point_once` |
| 9 | 1 | `bool` | `gps_use_3d_speed` |
| 10 | 1 | `bool` | `sbas_integrity` |
| 11 | 1 | `uint8_t` | `gps_ublox_utc_standard` |
| 12 | 1 | `bool` | `gps_ublox_enable_ana` |
| 13 | 65 | `char` | `nmeaCustomCommands[65]` |

### gyroConfig_t

- `gyroConfig` &mdash; PGN 10 (`PG_GYRO_CONFIG`), hash `0x3512C78A`

`sizeof` = 26 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `gyroMovementCalibrationThreshold` |
| 1 | 1 | `uint8_t` | `gyro_hardware_lpf` |
| 2 | 1 | `bool` | `gyro_high_fsr` |
| 3 | 1 | &mdash; | *(padding)* |
| 4 | 2 | `uint16_t` | `gyro_lpf1_static_hz` |
| 6 | 2 | `uint16_t` | `gyro_lpf2_static_hz` |
| 8 | 2 | `uint16_t` | `gyro_soft_notch_hz_1` |
| 10 | 2 | `uint16_t` | `gyro_soft_notch_cutoff_1` |
| 12 | 2 | `uint16_t` | `gyro_soft_notch_hz_2` |
| 14 | 2 | `uint16_t` | `gyro_soft_notch_cutoff_2` |
| 16 | 2 | `int16_t` | `gyro_offset_yaw` |
| 18 | 1 | `uint8_t` | `checkOverflow` |
| 19 | 1 | `uint8_t` | `gyro_lpf1_type` |
| 20 | 1 | `uint8_t` | `gyro_lpf2_type` |
| 21 | 1 | &mdash; | *(padding)* |
| 22 | 2 | `uint16_t` | `gyroCalibrationDuration` |
| 24 | 1 | `uint8_t` | `gyrosDetected` |
| 25 | 1 | `uint8_t` | `gyro_enabled_bitmask` |

### gyroDeviceConfig_t

- `gyroDeviceConfig` &mdash; PGN 540 (`PG_GYRO_DEVICE_CONFIG`), hash `0x6DD87CDA` &mdash; array of `MAX_GYRODEV_COUNT` elements

`sizeof` = 16 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `int8_t` | `index` |
| 1 | 1 | `uint8_t` | `busType` |
| 2 | 1 | `uint8_t` | `spiBus` |
| 3 | 1 | `ioTag_t` | `csnTag` |
| 4 | 1 | `uint8_t` | `i2cBus` |
| 5 | 1 | `uint8_t` | `i2cAddress` |
| 6 | 1 | `ioTag_t` | `extiTag` |
| 7 | 1 | `uint8_t` | `alignment` |
| 8 | 6 | [`alignment_u`](#alignment_u) | `customAlignment` |
| 14 | 1 | `ioTag_t` | `clkIn` |
| 15 | 1 | &mdash; | *(tail padding)* |

### i2cConfig_t

- `i2cConfig` &mdash; PGN 518 (`PG_I2C_CONFIG`), hash `0xB026B875` &mdash; array of `I2CDEV_COUNT` elements

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTagScl` |
| 1 | 1 | `ioTag_t` | `ioTagSda` |
| 2 | 1 | `bool` | `pullUp` |
| 3 | 1 | &mdash; | *(padding)* |
| 4 | 2 | `uint16_t` | `clockSpeed` |

### imuConfig_t

- `imuConfig` &mdash; PGN 22 (`PG_IMU_CONFIG`), hash `0xBE4FB408`

`sizeof` = 10 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `imu_dcm_kp` |
| 2 | 2 | `uint16_t` | `imu_dcm_ki` |
| 4 | 1 | `uint8_t` | `imu_process_denom` |
| 5 | 1 | &mdash; | *(padding)* |
| 6 | 2 | `int16_t` | `mag_declination` |
| 8 | 1 | `bool` | `trust_mag` |
| 9 | 1 | &mdash; | *(tail padding)* |

### ledStripConfig_t

- `ledStripConfig` &mdash; PGN 27 (`PG_LED_STRIP_CONFIG`), hash `0xC62B80FD`

`sizeof` = 16 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTag` |
| 1 | 1 | `bool` | `ledstrip_visual_beeper` |
| 2 | 1 | `uint8_t` | `ledstrip_grb_rgb` |
| 3 | 1 | [`ledProfile_e`](#ledprofile_e) | `ledstrip_profile` |
| 4 | 1 | [`colorId_e`](#colorid_e) | `ledstrip_race_color` |
| 5 | 1 | [`colorId_e`](#colorid_e) | `ledstrip_beacon_color` |
| 6 | 2 | `uint16_t` | `ledstrip_beacon_period_ms` |
| 8 | 1 | `uint8_t` | `ledstrip_beacon_percent` |
| 9 | 1 | `bool` | `ledstrip_beacon_armed_only` |
| 10 | 1 | [`colorId_e`](#colorid_e) | `ledstrip_visual_beeper_color` |
| 11 | 1 | `uint8_t` | `ledstrip_brightness` |
| 12 | 2 | `uint16_t` | `ledstrip_rainbow_delta` |
| 14 | 2 | `uint16_t` | `ledstrip_rainbow_freq` |

### ledStripStatusModeConfig_t

- `ledStripStatusModeConfig` &mdash; PGN 545 (`PG_LED_STRIP_STATUS_MODE_CONFIG`), hash `0x34ECEA5E`

`sizeof` = 368 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 256 | `ledConfig_t` | `ledConfigs[64]` |
| 256 | 64 | [`hsvColor_t`](#hsvcolor_t) | `colors[16]` |
| 320 | 36 | [`modeColorIndexes_t`](#modecolorindexes_t) | `modeColors[6]` |
| 356 | 11 | [`specialColorIndexes_t`](#specialcolorindexes_t) | `specialColors` |
| 367 | 1 | `uint8_t` | `ledstrip_rc_channel` |

### mcoConfig_t

- `mcoConfig` &mdash; PGN 541 (`PG_MCO_CONFIG`), hash `0x010C2C94` &mdash; array of `2` elements

`sizeof` = 3 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `bool` | `enabled` |
| 1 | 1 | `uint8_t` | `source` |
| 2 | 1 | `uint8_t` | `divider` |

### mixerConfig_t

- `mixerConfig` &mdash; PGN 20 (`PG_MIXER_CONFIG`), hash `0x80E59A46`

`sizeof` = 20 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `main_rotor_dir` |
| 1 | 1 | `uint8_t` | `tail_rotor_mode` |
| 2 | 1 | `uint8_t` | `tail_motor_idle` |
| 3 | 1 | `uint8_t` | `swash_type` |
| 4 | 1 | `uint8_t` | `swash_ring` |
| 5 | 1 | `int8_t` | `swash_geo_correction` |
| 6 | 1 | `int8_t` | `collective_tilt_correction_pos` |
| 7 | 1 | `int8_t` | `collective_tilt_correction_neg` |
| 8 | 2 | `int16_t` | `tail_center_trim` |
| 10 | 2 | `int16_t` | `swash_phase` |
| 12 | 2 | `uint16_t` | `swash_pitch_limit` |
| 14 | 6 | `int16_t` | `swash_trim[3]` |

### mixerInput_t

- `mixerInputs` &mdash; PGN 15 (`PG_MIXER_INPUTS`), hash `0x7838A8CF` &mdash; array of `MIXER_INPUT_COUNT` elements

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `int16_t` | `rate` |
| 2 | 2 | `int16_t` | `min` |
| 4 | 2 | `int16_t` | `max` |

### mixerRule_t

- `mixerRules` &mdash; PGN 21 (`PG_MIXER_RULES`), hash `0x2C09DFA2` &mdash; array of `MIXER_RULE_COUNT` elements

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `oper` |
| 1 | 1 | `uint8_t` | `input` |
| 2 | 1 | `uint8_t` | `output` |
| 3 | 1 | &mdash; | *(padding)* |
| 4 | 2 | `int16_t` | `offset` |
| 6 | 2 | `int16_t` | `weight` |

### modeActivationCondition_t

- `modeActivationConditions` &mdash; PGN 41 (`PG_MODE_ACTIVATION_PROFILE`), hash `0x55454DC6` &mdash; array of `MAX_MODE_ACTIVATION_CONDITION_COUNT` elements

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `boxId` |
| 1 | 1 | `uint8_t` | `linkedTo` |
| 2 | 1 | `uint8_t` | `modeLogic` |
| 3 | 1 | `uint8_t` | `rcChannel` |
| 4 | 2 | [`channelRange_t`](#channelrange_t) | `range` |

### motorConfig_t

- `motorConfig` &mdash; PGN 6 (`PG_MOTOR_CONFIG`), hash `0xF83A5DC8`

`sizeof` = 32 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 24 | [`motorDevConfig_t`](#motordevconfig_t) | `dev` |
| 24 | 2 | `uint16_t` | `maxthrottle` |
| 26 | 2 | `uint16_t` | `mincommand` |
| 28 | 2 | `uint16_t` | `kv` |
| 30 | 1 | `uint8_t` | `motorPoleCount` |
| 31 | 1 | &mdash; | *(tail padding)* |

### mspConfig_t

- `mspConfig` &mdash; PGN 557 (`PG_MSP_CONFIG`), hash `0xAC040DC9`

`sizeof` = 1 byte.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `bool` | `halfDuplex` |

### opticalflowConfig_t

- `opticalflowConfig` &mdash; PGN 560 (`PG_OPTICALFLOW_CONFIG`), hash `0x258240D2`

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | [`opticalflowType_e`](#opticalflowtype_e) | `opticalflow_hardware` |
| 1 | 1 | &mdash; | *(padding)* |
| 2 | 2 | `uint16_t` | `rotation` |
| 4 | 1 | `bool` | `flip_x` |
| 5 | 1 | &mdash; | *(padding)* |
| 6 | 2 | `uint16_t` | `flow_lpf` |

### pidConfig_t

- `pidConfig` &mdash; PGN 504 (`PG_PID_CONFIG`), hash `0x77A9E9E6`

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `pid_process_denom` |
| 1 | 1 | `uint8_t` | `filter_process_denom` |

### pidProfile_t

- `pidProfiles` &mdash; PGN 14 (`PG_PID_PROFILE`), hash `0x97F727C6` &mdash; array of `PID_PROFILE_COUNT` elements

`sizeof` = 40 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 9 | `char` | `profileName[9]` |
| 9 | 1 | &mdash; | *(padding)* |
| 10 | 30 | [`pidf_t`](#pidf_t) | `pid[5]` |

### pilotConfig_t

- `pilotConfig` &mdash; PGN 47 (`PG_PILOT_CONFIG`), hash `0xDFA805D5`

`sizeof` = 34 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 17 | `char` | `craftName[17]` |
| 17 | 17 | `char` | `pilotName[17]` |

### pinPullUpDownConfig_t

- `pinPulldownConfig` &mdash; PGN 552 (`PG_PULLDOWN_CONFIG`), hash `0x8EA458C5` &mdash; array of `PIN_PULL_UP_DOWN_COUNT` elements
- `pinPullupConfig` &mdash; PGN 551 (`PG_PULLUP_CONFIG`), hash `0x6CC75258` &mdash; array of `PIN_PULL_UP_DOWN_COUNT` elements

`sizeof` = 1 byte.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTag` |

### pinioBoxConfig_t

- `pinioBoxConfig` &mdash; PGN 530 (`PG_PINIOBOX_CONFIG`), hash `0xAA658C54`

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 4 | `uint8_t` | `permanentId[4]` |

### pinioConfig_t

- `pinioConfig` &mdash; PGN 529 (`PG_PINIO_CONFIG`), hash `0x7945F9DD`

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 4 | `ioTag_t` | `ioTag[4]` |
| 4 | 4 | `uint8_t` | `config[4]` |

### positionConfig_t

- `positionConfig` &mdash; PGN 56 (`PG_POSITION`), hash `0x13BEC47A`

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | [`altitudeSource_e`](#altitudesource_e) | `altitude_source` |
| 1 | 1 | `uint8_t` | `altitude_prefer_baro` |
| 2 | 2 | `uint16_t` | `altitude_lpf` |
| 4 | 2 | `uint16_t` | `altitude_d_lpf` |
| 6 | 2 | `uint16_t` | `rangefinder_max_range_cm` |

### ppmConfig_t

- `ppmConfig` &mdash; PGN 507 (`PG_PPM_CONFIG`), hash `0xF9CF641C`

`sizeof` = 1 byte.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTag` |

### quadSpiConfig_t

- `quadSpiConfig` &mdash; PGN 548 (`PG_QUADSPI_CONFIG`), hash `0x7AB0CAD9` &mdash; array of `QUADSPIDEV_COUNT` elements

`sizeof` = 13 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTagClk` |
| 1 | 1 | `ioTag_t` | `ioTagBK1IO0` |
| 2 | 1 | `ioTag_t` | `ioTagBK1IO1` |
| 3 | 1 | `ioTag_t` | `ioTagBK1IO2` |
| 4 | 1 | `ioTag_t` | `ioTagBK1IO3` |
| 5 | 1 | `ioTag_t` | `ioTagBK1CS` |
| 6 | 1 | `ioTag_t` | `ioTagBK2IO0` |
| 7 | 1 | `ioTag_t` | `ioTagBK2IO1` |
| 8 | 1 | `ioTag_t` | `ioTagBK2IO2` |
| 9 | 1 | `ioTag_t` | `ioTagBK2IO3` |
| 10 | 1 | `ioTag_t` | `ioTagBK2CS` |
| 11 | 1 | `uint8_t` | `mode` |
| 12 | 1 | `uint8_t` | `csFlags` |

### rangefinderConfig_t

- `rangefinderConfig` &mdash; PGN 527 (`PG_RANGEFINDER_CONFIG`), hash `0x32917E98`

`sizeof` = 1 byte.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | [`rangefinderType_e`](#rangefindertype_e) | `rangefinder_hardware` |

### rateProfile_t

- `rateProfiles` &mdash; PGN 12 (`PG_RATE_PROFILES`), hash `0x57F65935` &mdash; array of `RATE_PROFILE_COUNT` elements

`sizeof` = 54 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 9 | `char` | `profileName[9]` |
| 9 | 1 | &mdash; | *(padding)* |
| 10 | 8 | `uint16_t` | `rates[4]` |
| 18 | 4 | `uint8_t` | `expo[4]` |
| 22 | 4 | `uint8_t` | `curve[4]` |
| 26 | 2 | `uint8_t` | `levelExpo[2]` |
| 28 | 4 | `uint8_t` | `response_time[4]` |
| 32 | 8 | `uint16_t` | `accel_limit[4]` |
| 40 | 1 | `uint8_t` | `cyclic_ring` |
| 41 | 1 | `uint8_t` | `cyclic_polar` |
| 42 | 4 | `uint8_t` | `setpoint_boost_gain[4]` |
| 46 | 4 | `uint8_t` | `setpoint_boost_cutoff[4]` |
| 50 | 1 | `uint8_t` | `yaw_dynamic_ceiling_gain` |
| 51 | 1 | `uint8_t` | `yaw_dynamic_deadband_gain` |
| 52 | 1 | `uint8_t` | `yaw_dynamic_deadband_cutoff` |
| 53 | 1 | `uint8_t` | `yaw_dynamic_deadband_filter` |

### rcControlsConfig_t

- `rcControlsConfig` &mdash; PGN 25 (`PG_RC_CONTROLS_CONFIG`), hash `0x079977AB`

`sizeof` = 16 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `rc_center` |
| 2 | 2 | `uint16_t` | `rc_deflection` |
| 4 | 2 | `uint16_t` | `rc_min_throttle` |
| 6 | 2 | `uint16_t` | `rc_max_throttle` |
| 8 | 1 | `uint8_t` | `rc_deadband` |
| 9 | 1 | `uint8_t` | `rc_yaw_deadband` |
| 10 | 1 | `uint8_t` | `rc_smoothness` |
| 11 | 4 | `uint8_t` | `rc_threshold[4]` |
| 15 | 1 | &mdash; | *(tail padding)* |

### rxChannelRangeConfig_t

- `rxChannelRangeConfigs` &mdash; PGN 44 (`PG_RX_CHANNEL_RANGE_CONFIG`), hash `0x198E9D36` &mdash; array of `RC_CONTROL_COUNT` elements

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `min` |
| 2 | 2 | `uint16_t` | `max` |

### rxConfig_t

- `rxConfig` &mdash; PGN 24 (`PG_RX_CONFIG`), hash `0xDDF454AE`

`sizeof` = 30 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 6 | `uint8_t` | `rc_channel[6]` |
| 6 | 1 | `uint8_t` | `serialrx_provider` |
| 7 | 1 | `bool` | `serialrx_inverted` |
| 8 | 1 | `bool` | `halfDuplex` |
| 9 | 1 | `ioTag_t` | `spektrum_bind_pin_override_ioTag` |
| 10 | 1 | `ioTag_t` | `spektrum_bind_plug_ioTag` |
| 11 | 1 | `uint8_t` | `spektrum_sat_bind` |
| 12 | 1 | `bool` | `spektrum_sat_bind_autoreset` |
| 13 | 1 | `uint8_t` | `rssi_channel` |
| 14 | 1 | `uint8_t` | `rssi_scale` |
| 15 | 1 | `bool` | `rssi_invert` |
| 16 | 2 | `uint16_t` | `rx_pulse_min` |
| 18 | 2 | `uint16_t` | `rx_pulse_max` |
| 20 | 1 | `uint8_t` | `max_rc_channel` |
| 21 | 1 | `bool` | `rssi_src_frame_errors` |
| 22 | 1 | `int8_t` | `rssi_offset` |
| 23 | 1 | `uint8_t` | `rssi_src_frame_lpf_period` |
| 24 | 1 | `uint8_t` | `rssi_smoothing` |
| 25 | 1 | `uint8_t` | `srxl2_unit_id` |
| 26 | 1 | `bool` | `srxl2_baud_fast` |
| 27 | 1 | `bool` | `sbus_baud_fast` |
| 28 | 1 | `bool` | `crsf_use_negotiated_baud` |
| 29 | 1 | &mdash; | *(tail padding)* |

### rxFailsafeChannelConfig_t

- `rxFailsafeChannelConfigs` &mdash; PGN 43 (`PG_RX_FAILSAFE_CHANNEL_CONFIG`), hash `0xF440899F` &mdash; array of `MAX_SUPPORTED_RC_CHANNEL_COUNT` elements

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `failsafe_mode` |
| 1 | 1 | `uint8_t` | `failsafe_step` |

### schedulerConfig_t

- `schedulerConfig` &mdash; PGN 556 (`PG_SCHEDULER_CONFIG`), hash `0xDE846B81`

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `rxRelaxDeterminism` |
| 2 | 2 | `uint16_t` | `cpuLatePercentageLimit` |
| 4 | 1 | `uint8_t` | `debugTask` |
| 5 | 1 | &mdash; | *(tail padding)* |

### sdcardConfig_t

- `sdcardConfig` &mdash; PGN 511 (`PG_SDCARD_CONFIG`), hash `0x20EF4804`

`sizeof` = 5 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `int8_t` | `device` |
| 1 | 1 | `ioTag_t` | `cardDetectTag` |
| 2 | 1 | `ioTag_t` | `chipSelectTag` |
| 3 | 1 | `bool` | `cardDetectInverted` |
| 4 | 1 | [`sdcardMode_e`](#sdcardmode_e) | `mode` |

### sdioConfig_t

- `sdioConfig` &mdash; PGN 532 (`PG_SDIO_CONFIG`), hash `0x9742A850`

`sizeof` = 5 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `bool` | `clockBypass` |
| 1 | 1 | `bool` | `useCache` |
| 2 | 1 | `bool` | `use4BitWidth` |
| 3 | 1 | `int8_t` | `dmaopt` |
| 4 | 1 | `uint8_t` | `device` |

### sdioPinConfig_t

- `sdioPinConfig` &mdash; PGN 550 (`PG_SDIO_PIN_CONFIG`), hash `0xD7289D0C`

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `CKPin` |
| 1 | 1 | `ioTag_t` | `CMDPin` |
| 2 | 1 | `ioTag_t` | `D0Pin` |
| 3 | 1 | `ioTag_t` | `D1Pin` |
| 4 | 1 | `ioTag_t` | `D2Pin` |
| 5 | 1 | `ioTag_t` | `D3Pin` |

### serialConfig_t

- `serialConfig` &mdash; PGN 13 (`PG_SERIAL_CONFIG`), hash `0x3D257838`

`sizeof` = 244 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 240 | [`serialPortConfig_t`](#serialportconfig_t) | `portConfigs[20]` |
| 240 | 2 | `uint16_t` | `serial_update_rate_hz` |
| 242 | 1 | `uint8_t` | `reboot_character` |
| 243 | 1 | &mdash; | *(tail padding)* |

### serialPinConfig_t

- `serialPinConfig` &mdash; PGN 509 (`PG_SERIAL_PIN_CONFIG`), hash `0xFBA6E927`

`sizeof` = 74 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 29 | `ioTag_t` | `ioTagTx[29]` |
| 29 | 29 | `ioTag_t` | `ioTagRx[29]` |
| 58 | 16 | `ioTag_t` | `ioTagInverter[16]` |

### serialUartConfig_t

- `serialUartConfig` &mdash; PGN 543 (`PG_SERIAL_UART_CONFIG`), hash `0x9B5CDD0F` &mdash; array of `UARTDEV_CONFIG_MAX` elements

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `int8_t` | `txDmaopt` |
| 1 | 1 | `int8_t` | `rxDmaopt` |

### servoConfig_t

- `servoConfig` &mdash; PGN 52 (`PG_SERVO_CONFIG`), hash `0xB12C2058`

`sizeof` = 12 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 12 | [`servoDevConfig_t`](#servodevconfig_t) | `dev` |

### servoParam_t

- `servoParams` &mdash; PGN 42 (`PG_SERVO_PARAMS`), hash `0x1704EAF5` &mdash; array of `MAX_SUPPORTED_SERVOS` elements

`sizeof` = 16 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `int16_t` | `mid` |
| 2 | 2 | `int16_t` | `min` |
| 4 | 2 | `int16_t` | `max` |
| 6 | 2 | `uint16_t` | `rpos` |
| 8 | 2 | `uint16_t` | `rneg` |
| 10 | 2 | `uint16_t` | `rate` |
| 12 | 2 | `uint16_t` | `speed` |
| 14 | 2 | `uint16_t` | `flags` |

### sonarConfig_t

- `sonarConfig` &mdash; PGN 516 (`PG_SONAR_CONFIG`), hash `0xA17AFF25`

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `triggerTag` |
| 1 | 1 | `ioTag_t` | `echoTag` |

### spiPinConfig_t

- `spiPinConfig` &mdash; PGN 520 (`PG_SPI_PIN_CONFIG`), hash `0x81954E3F` &mdash; array of `SPIDEV_COUNT` elements

`sizeof` = 5 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTagSck` |
| 1 | 1 | `ioTag_t` | `ioTagMiso` |
| 2 | 1 | `ioTag_t` | `ioTagMosi` |
| 3 | 1 | `int8_t` | `txDmaopt` |
| 4 | 1 | `int8_t` | `rxDmaopt` |

### statsConfig_t

- `statsConfig` &mdash; PGN 547 (`PG_STATS_CONFIG`), hash `0x87526EDD`

`sizeof` = 24 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 4 | `uint32_t` | `stats_total_flights` |
| 4 | 4 | `uint32_t` | `stats_total_time_s` |
| 8 | 4 | `uint32_t` | `stats_total_dist_m` |
| 12 | 1 | `int8_t` | `stats_min_armed_time_s` |
| 13 | 3 | &mdash; | *(padding)* |
| 16 | 4 | `uint32_t` | `stats_mah_used` |
| 20 | 1 | `uint8_t` | `statsSaveMoveLimit` |
| 21 | 3 | &mdash; | *(tail padding)* |

### statusLedConfig_t

- `statusLedConfig` &mdash; PGN 505 (`PG_STATUS_LED_CONFIG`), hash `0x81FFF062`

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 3 | `ioTag_t` | `ioTags[3]` |
| 3 | 1 | `uint8_t` | `inversion` |

### systemConfig_t

- `systemConfig` &mdash; PGN 18 (`PG_SYSTEM_CONFIG`), hash `0xA315597B`

`sizeof` = 18 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `pidProfileIndex` |
| 1 | 1 | `uint8_t` | `activeRateProfile` |
| 2 | 1 | `uint8_t` | `debug_mode` |
| 3 | 1 | `uint8_t` | `debug_axis` |
| 4 | 1 | `bool` | `task_statistics` |
| 5 | 1 | `uint8_t` | `cpu_overclock` |
| 6 | 1 | `uint8_t` | `powerOnArmingGraceTime` |
| 7 | 8 | `char` | `boardIdentifier[8]` |
| 15 | 1 | `uint8_t` | `hseMhz` |
| 16 | 1 | [`configurationState_e`](#configurationstate_e) | `configurationState` |
| 17 | 1 | `uint8_t` | `activeBatteryProfile` |

### telemetryConfig_t

- `telemetryConfig` &mdash; PGN 31 (`PG_TELEMETRY_CONFIG`), hash `0xBD6D2B1A`

`sizeof` = 44 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `int16_t` | `gpsNoFixLatitude` |
| 2 | 2 | `int16_t` | `gpsNoFixLongitude` |
| 4 | 1 | `bool` | `telemetry_inverted` |
| 5 | 1 | `bool` | `halfDuplex` |
| 6 | 1 | [`frskyGpsCoordFormat_e`](#frskygpscoordformat_e) | `frsky_coordinate_format` |
| 7 | 1 | `uint8_t` | `frsky_unit` |
| 8 | 1 | `uint8_t` | `frsky_vfas_precision` |
| 9 | 1 | `uint8_t` | `hottAlarmSoundInterval` |
| 10 | 1 | `bool` | `pidValuesAsTelemetry` |
| 11 | 1 | `bool` | `report_cell_voltage` |
| 12 | 15 | `uint8_t` | `flysky_sensors[15]` |
| 27 | 1 | &mdash; | *(padding)* |
| 28 | 2 | `uint16_t` | `mavlink_mah_as_heading_divisor` |
| 30 | 2 | &mdash; | *(padding)* |
| 32 | 4 | `uint32_t` | `disabledSensors` |
| 36 | 1 | `uint8_t` | `mavlink_min_txbuff` |
| 37 | 1 | `uint8_t` | `mavlink_extended_status_rate` |
| 38 | 1 | `uint8_t` | `mavlink_rc_channels_rate` |
| 39 | 1 | `uint8_t` | `mavlink_position_rate` |
| 40 | 1 | `uint8_t` | `mavlink_extra1_rate` |
| 41 | 1 | `uint8_t` | `mavlink_extra2_rate` |
| 42 | 1 | `uint8_t` | `mavlink_extra3_rate` |
| 43 | 1 | `bool` | `crsf_tlm_accgyro` |

### timeConfig_t

- `timeConfig` &mdash; PGN 526 (`PG_TIME_CONFIG`), hash `0x4733E1A6`

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `int16_t` | `tz_offsetMinutes` |

### timerIOConfig_t

- `timerIOConfig` &mdash; PGN 534 (`PG_TIMER_IO_CONFIG`), hash `0x09953C82` &mdash; array of `MAX_TIMER_PINMAP_COUNT` elements

`sizeof` = 3 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `ioTag_t` | `ioTag` |
| 1 | 1 | `uint8_t` | `index` |
| 2 | 1 | `int8_t` | `dmaopt` |

### timerUpConfig_t

- `timerUpConfig` &mdash; PGN 549 (`PG_TIMER_UP_CONFIG`), hash `0x838D1137` &mdash; array of `HARDWARE_TIMER_DEFINITION_COUNT` elements

`sizeof` = 1 byte.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `int8_t` | `dmaopt` |

### usbDev_t

- `usbDevConfig` &mdash; PGN 531 (`PG_USB_CONFIG`), hash `0x6CDB14DB`

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `type` |
| 1 | 1 | `ioTag_t` | `mscButtonPin` |
| 2 | 1 | `bool` | `mscButtonUsePullup` |
| 3 | 1 | `ioTag_t` | `detectPin` |

### voltageSensorADCConfig_t

- `voltageSensorADCConfig` &mdash; PGN 258 (`PG_VOLTAGE_SENSOR_ADC_CONFIG`), hash `0xBB15461C` &mdash; array of `MAX_VOLTAGE_SENSOR_ADC` elements

`sizeof` = 3 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `vbatscale` |
| 1 | 1 | `uint8_t` | `vbatresdivval` |
| 2 | 1 | `uint8_t` | `vbatresdivmultiplier` |

## Nested structures

### adcChannelConfig_t

`sizeof` = 3 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `bool` | `enabled` |
| 1 | 1 | `ioTag_t` | `ioTag` |
| 2 | 1 | `int8_t` | `device` |

### channelRange_t

`sizeof` = 2 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `int8_t` | `startStep` |
| 1 | 1 | `int8_t` | `endStep` |

### dronecanDnaEntry_t

`sizeof` = 17 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 16 | `uint8_t` | `uniqueId[16]` |
| 16 | 1 | `uint8_t` | `nodeId` |

### flightDynamicsTrims_u

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 8 | `int16_t` | `raw[4]` |
| 0 | 8 | [`int16_flightDynamicsTrims_s`](#int16_flightdynamicstrims_s) | `values` |

### hsvColor_t

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `h` |
| 2 | 1 | `uint8_t` | `s` |
| 3 | 1 | `uint8_t` | `v` |

### int16_flightDynamicsTrims_s

`sizeof` = 8 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `int16_t` | `roll` |
| 2 | 2 | `int16_t` | `pitch` |
| 4 | 2 | `int16_t` | `yaw` |
| 6 | 2 | `int16_t` | `calibrationCompleted` |

### modeColorIndexes_t

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 6 | `uint8_t` | `color[6]` |

### motorDevConfig_t

`sizeof` = 24 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint16_t` | `motorPwmRate` |
| 2 | 1 | `uint8_t` | `motorProtocol` |
| 3 | 1 | `bool` | `motorInversion` |
| 4 | 1 | `bool` | `useContinuousUpdate` |
| 5 | 1 | [`dshotDmar_e`](#dshotdmar_e) | `useBurstDshot` |
| 6 | 1 | [`dshotTelemetry_e`](#dshottelemetry_e) | `useDshotTelemetry` |
| 7 | 1 | [`dshotEdt_e`](#dshotedt_e) | `useDshotEdt` |
| 8 | 12 | `ioTag_t` | `ioTags[12]` |
| 20 | 1 | `uint8_t` | `motorTransportProtocol` |
| 21 | 1 | [`dshotBitbangMode_e`](#dshotbitbangmode_e) | `useDshotBitbang` |
| 22 | 1 | [`dshotBitbangedTimer_e`](#dshotbitbangedtimer_e) | `useDshotBitbangedTimer` |
| 23 | 1 | &mdash; | *(tail padding)* |

### pidf_t

`sizeof` = 6 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8_t` | `P` |
| 1 | 1 | `uint8_t` | `I` |
| 2 | 1 | `uint8_t` | `D` |
| 3 | 1 | &mdash; | *(padding)* |
| 4 | 2 | `uint16_t` | `F` |

### rollAndPitchTrims_t

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 4 | `int16_t` | `raw[2]` |
| 0 | 4 | [`rollAndPitchTrims_t_def`](#rollandpitchtrims_t_def) | `values` |

### rollAndPitchTrims_t_def

`sizeof` = 4 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 2 | `int16_t` | `roll` |
| 2 | 2 | `int16_t` | `pitch` |

### serialPortConfig_t

`sizeof` = 12 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 4 | `uint32_t` | `functionMask` |
| 4 | 1 | `int8_t` | `identifier` |
| 5 | 1 | `uint8_t` | `msp_baudrateIndex` |
| 6 | 1 | `uint8_t` | `gps_baudrateIndex` |
| 7 | 1 | `uint8_t` | `blackbox_baudrateIndex` |
| 8 | 1 | `uint8_t` | `telemetry_baudrateIndex` |
| 9 | 3 | &mdash; | *(tail padding)* |

### servoDevConfig_t

`sizeof` = 12 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 12 | `ioTag_t` | `ioTags[12]` |

### specialColorIndexes_t

`sizeof` = 11 bytes.

| Offset | Size | Type | Member |
| ---: | ---: | --- | --- |
| 0 | 11 | `uint8_t` | `color[11]` |

## Enumerations

An enum stored in a PG is `PG_ENUM` (packed), so its width is the same in every build. Its enumerator names and values are part of the layout hash: renumbering one changes what a stored byte means, and the hash has to catch that.

### BlackboxDevice_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `BLACKBOX_DEVICE_NONE` | 0 |
| `BLACKBOX_DEVICE_FLASH` | 1 |
| `BLACKBOX_DEVICE_SDCARD` | 2 |
| `BLACKBOX_DEVICE_SERIAL` | 3 |
| `BLACKBOX_DEVICE_VIRTUAL` | 4 |

### BlackboxMode_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `BLACKBOX_MODE_OFF` | 0 |
| `BLACKBOX_MODE_NORMAL` | 1 |
| `BLACKBOX_MODE_ARMED` | 2 |
| `BLACKBOX_MODE_SWITCH` | 3 |

### altitudeSource_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `ALTITUDE_SOURCE_DEFAULT` | 0 |
| `ALTITUDE_SOURCE_BARO_ONLY` | 1 |
| `ALTITUDE_SOURCE_GPS_ONLY` | 2 |
| `ALTITUDE_SOURCE_RANGEFINDER_PREFER` | 3 |
| `ALTITUDE_SOURCE_RANGEFINDER_ONLY` | 4 |

### colorId_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `COLOR_BLACK` | 0 |
| `COLOR_WHITE` | 1 |
| `COLOR_RED` | 2 |
| `COLOR_ORANGE` | 3 |
| `COLOR_YELLOW` | 4 |
| `COLOR_LIME_GREEN` | 5 |
| `COLOR_GREEN` | 6 |
| `COLOR_MINT_GREEN` | 7 |
| `COLOR_CYAN` | 8 |
| `COLOR_LIGHT_BLUE` | 9 |
| `COLOR_BLUE` | 10 |
| `COLOR_DARK_VIOLET` | 11 |
| `COLOR_MAGENTA` | 12 |
| `COLOR_DEEP_PINK` | 13 |
| `COLOR_COUNT` | 14 |

### configurationState_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `CONFIGURATION_STATE_UNCONFIGURED` | 0 |
| `CONFIGURATION_STATE_CONFIGURED` | 1 |

### currentMeterSource_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `CURRENT_METER_NONE` | 0 |
| `CURRENT_METER_ADC` | 1 |
| `CURRENT_METER_VIRTUAL` | 2 |
| `CURRENT_METER_ESC` | 3 |
| `CURRENT_METER_MSP` | 4 |
| `CURRENT_METER_COUNT` | 5 |

### dshotBitbangMode_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `DSHOT_BITBANG_OFF` | 0 |
| `DSHOT_BITBANG_ON` | 1 |
| `DSHOT_BITBANG_AUTO` | 2 |

### dshotBitbangedTimer_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `DSHOT_BITBANGED_TIMER_AUTO` | 0 |
| `DSHOT_BITBANGED_TIMER_TIM1` | 1 |
| `DSHOT_BITBANGED_TIMER_TIM8` | 2 |

### dshotDmar_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `DSHOT_DMAR_OFF` | 0 |
| `DSHOT_DMAR_ON` | 1 |
| `DSHOT_DMAR_AUTO` | 2 |

### dshotEdt_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `DSHOT_EDT_OFF` | 0 |
| `DSHOT_EDT_ON` | 1 |
| `DSHOT_EDT_FORCE` | 2 |

### dshotTelemetry_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `DSHOT_TELEMETRY_OFF` | 0 |
| `DSHOT_TELEMETRY_ON` | 1 |

### failsafeProcedure_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `FAILSAFE_PROCEDURE_AUTO_LANDING` | 0 |
| `FAILSAFE_PROCEDURE_DROP_IT` | 1 |
| `FAILSAFE_PROCEDURE_COUNT` | 2 |

### failsafeSwitchMode_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `FAILSAFE_SWITCH_MODE_STAGE1` | 0 |
| `FAILSAFE_SWITCH_MODE_KILL` | 1 |
| `FAILSAFE_SWITCH_MODE_STAGE2` | 2 |

### frskyGpsCoordFormat_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `FRSKY_FORMAT_DMS` | 0 |
| `FRSKY_FORMAT_NMEA` | 1 |

### ledProfile_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `LED_PROFILE_RACE` | 0 |
| `LED_PROFILE_BEACON` | 1 |
| `LED_PROFILE_STATUS` | 2 |
| `LED_PROFILE_COUNT` | 3 |

### opticalflowType_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `OPTICALFLOW_NONE` | 0 |
| `OPTICALFLOW_MT` | 1 |
| `OPTICALFLOW_UPT1` | 2 |
| `OPTICALFLOW_HARDWARE_COUNT` | 3 |

### rangefinderType_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `RANGEFINDER_NONE` | 0 |
| `RANGEFINDER_HCSR04` | 1 |
| `RANGEFINDER_TFMINI` | 2 |
| `RANGEFINDER_TF02` | 3 |
| `RANGEFINDER_MTF01` | 4 |
| `RANGEFINDER_MTF02` | 5 |
| `RANGEFINDER_MTF01P` | 6 |
| `RANGEFINDER_MTF02P` | 7 |
| `RANGEFINDER_TFNOVA` | 8 |
| `RANGEFINDER_NOOPLOOP_F2` | 9 |
| `RANGEFINDER_NOOPLOOP_F2P` | 10 |
| `RANGEFINDER_NOOPLOOP_F2PH` | 11 |
| `RANGEFINDER_NOOPLOOP_F` | 12 |
| `RANGEFINDER_NOOPLOOP_FP` | 13 |
| `RANGEFINDER_NOOPLOOP_F2MINI` | 14 |
| `RANGEFINDER_UPT1` | 15 |
| `RANGEFINDER_HARDWARE_COUNT` | 16 |

### sdcardMode_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `SDCARD_MODE_NONE` | 0 |
| `SDCARD_MODE_SPI` | 1 |
| `SDCARD_MODE_SDIO` | 2 |

### voltageMeterSource_e

`sizeof` = 1 byte.

| Enumerator | Value |
| --- | ---: |
| `VOLTAGE_METER_NONE` | 0 |
| `VOLTAGE_METER_ADC` | 1 |
| `VOLTAGE_METER_ESC` | 2 |
| `VOLTAGE_METER_COUNT` | 3 |
