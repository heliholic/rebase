# Compile-time feature defines (`USE_` and `ENABLE_`)

Firmware features and drivers are compiled in or out with preprocessor gates.
Most source files stay in the build and wrap themselves in these macros; unused
code is then dropped by the compiler.

This inventory lists every `USE_*` and `ENABLE_*` name that actually gates
inclusion in `src/main` and `src/platform`. Vendor HAL/DAL callback knobs
(`USE_HAL_*_REGISTER_CALLBACKS`, `USE_DAL_*_REGISTER_CALLBACKS`, and similar)
are omitted. Runtime helpers such as `ENABLE_ARMING_FLAG()` are not feature
flags and are omitted as well.

Contents:

- [`USE_` vs `ENABLE_`](#use_-vs-enable_)
- [Where they are set](#where-they-are-set)
- [`ENABLE_` flags](#enable_-flags-numeric-01)
- [Accel / gyro](#accel--gyro)
- [Magnetometer](#magnetometer)
- [Barometer / vario](#barometer--vario)
- [GPS / rangefinder / optical flow](#gps--rangefinder--optical-flow)
- [Receiver / serial RX](#receiver--serial-rx)
- [Telemetry](#telemetry)
- [OSD / CMS / display / VTX / camera / LED strip](#osd--cms--display--vtx--camera--led-strip)
- [Blackbox / flash / SD card](#blackbox--flash--sd-card)
- [Motors / DSHOT / ESC / servos](#motors--dshot--esc--servos)
- [Serial ports / USB / MSP / CLI](#serial-ports--usb--msp--cli)
- [Buses / DMA / timers / ADC / EXTI](#buses--dma--timers--adc--exti)
- [MCU vendor stacks](#mcu-vendor-stacks)
- [Platform, boot, and FC extras](#platform-boot-and-fc-extras)
- [Notes on implied and removed flags](#notes-on-implied-and-removed-flags)

## `USE_` vs `ENABLE_`

| Style | Test in source | How to turn it off |
| --- | --- | --- |
| `USE_FOO` | `#ifdef USE_FOO` / `#if defined(USE_FOO)` | Leave it undefined, or `#undef USE_FOO` |
| `ENABLE_FOO` | `#if ENABLE_FOO` | Set `ENABLE_FOO` to `0` (including from the compiler command line) |

`ENABLE_` is the newer form. A `USE_` flag can only be “defined or not”, so it
cannot be forced off from `-D` once a header has defined it. An `ENABLE_` flag
is a numeric `0` or `1`, so `-DENABLE_FOO=0` wins.

The migration is gradual. Where both exist (today: `USE_OSD_CUSTOM_TEXT` also
sets `ENABLE_OSD_CUSTOM_TEXT`), new code should use `#if ENABLE_…`.

## Where they are set

Processing order for a normal target build:

1. **`src/main/target/common_pre.h`** — default feature set, before the board
   header. `CLOUD_BUILD` / `CORE_BUILD` / flash size change what is turned on
   here.
2. **MCU `target.h`** (`src/platform/.../target.h`) — peripherals for that
   silicon (UARTs, SPI, I2C, ADC, …).
3. **Board `config.h`** (unified target / cloud build) — board-specific
   drivers, pin maps, and extra `#undef`s.
4. **`src/main/target/common_post.h`** — implied children (for example
   `USE_MAG` pulls in mag drivers), and drops features that cannot work (for
   example telemetry protocols when `USE_TELEMETRY` is off).

MCU vendor stacks are also passed as `-D` from `src/platform/*/mk/*.mk`
(`USE_HAL_DRIVER`, `USE_STDPERIPH_DRIVER`, …).

Parent flags often imply children. Setting `USE_SERIALRX` without naming
protocols gets the default serial-RX set; setting `USE_MAG` without naming
chips gets the full mag driver suite. A board config that already names
specific chips is left as-is.

## `ENABLE_` flags (numeric 0/1)

Defaults of `0` or `1` below are the catch-alls in `common_post.h` (or the
driver header) when nothing else set the flag.

| Define | Default | What it includes |
| --- | --- | --- |
| `ENABLE_42686_EXTENDED_RANGE` | unset / 0 | ICM-42686P 32 g / extended gyro range instead of the 16 g path |
| `ENABLE_AFATFS_DMA_CACHE` | 0 (1 on X32) | Cache maintenance around asyncFAT DMA disk I/O |
| `ENABLE_BARO_SPA06_PROBE` | 0 | One-shot SPA06-003 CHIP_ID probe over STM32C5 I3C-as-I2C after `baroInit()` |
| `ENABLE_BF_OBL` | 0 (1 on STM32N6) | Open Bootloader contract (`BF_OBL_IWDG_REFRESH` and related) |
| `ENABLE_BMI270_ALIGN_AS_ICM` | 0 | Rotate BMI270 axes CW90 so they match ICM-42688; one board alignment then works for either chip |
| `ENABLE_BOOT0_PIN_SELECT` | 1 | STM32C5: take boot source from the BOOT0 pin rather than the BOOT0 option bit |
| `ENABLE_CAN` | 0 (1 on X32) | On-chip CAN peripheral driver |
| `ENABLE_DEBUG_CLI_COMMANDS` | 0 | CLI `dxr` / `dxw` commands that read/write arbitrary 32-bit addresses |
| `ENABLE_DEBUG_DASHBOARD_PAGE` | defined in dashboard header | Extra debug page on the I2C OLED dashboard |
| `ENABLE_DEBUG_UART` | 0 | Poll-mode bring-up UART (`debugUartPuts` and friends); needs `DEBUG_UART*` pin defines |
| `ENABLE_DRONECAN` | `ENABLE_CAN` | DroneCAN/UAVCAN stack (runtime PG `dronecan_enabled` still decides whether the task runs) |
| `ENABLE_DRONECAN_DNA` | `ENABLE_DRONECAN` | DroneCAN dynamic node-ID allocator |
| `ENABLE_DRONECAN_ESC` | `ENABLE_DRONECAN` | DroneCAN ESC command (`RawCommand`) and telemetry (`Status`) |
| `ENABLE_FB_OSD` | unset / 0 | Framebuffer OSD displayport (`displayport_fb_osd`) |
| `ENABLE_GAZEBO_BRIDGE` | 0 | SITL: Gazebo gyro-sign mapping |
| `ENABLE_LCD_CONSOLE` | 0 | LCD debug console; needs exactly one `LCD_CONSOLE_PANEL_*` selector |
| `ENABLE_LCD_PRINTF_REDIRECT` | `ENABLE_LCD_CONSOLE` | Route global `tfp_printf` to the LCD console at boot |
| `ENABLE_MULTICORE_INIT` | unset | Run FC init phases on a second core; requires `USE_MULTICORE` |
| `ENABLE_N6_PWR_EXTERNAL` | 0 | STM32N6: `PWR_EXTERNAL_SOURCE_SUPPLY` instead of on-chip SMPS (N6570-DK style boards) |
| `ENABLE_OSD_CUSTOM_TEXT` | 0, or 1 if `USE_OSD_CUSTOM_TEXT` | Custom OSD text elements and the matching CMS/CLI |
| `ENABLE_OVERCLOCK_108_MHZ` | MCU-dependent | Expose 108 MHz overclock option |
| `ENABLE_OVERCLOCK_120_MHZ` | MCU-dependent | Expose 120 MHz overclock option |
| `ENABLE_OVERCLOCK_192_MHZ` | MCU-dependent | Expose 192 MHz overclock option |
| `ENABLE_OVERCLOCK_216_MHZ` | MCU-dependent | Expose 216 MHz overclock option |
| `ENABLE_OVERCLOCK_240_MHZ` | MCU-dependent | Expose 240 MHz overclock option |
| `ENABLE_RX_UDP` | 0 | UDP receiver (SITL / host) |
| `ENABLE_SDIO_EXTERNAL_DMA` | 0 | SDIO uses an externally configured DMA channel |
| `ENABLE_SDIO_INIT` | 0 (1 on H5/N6 targets) | SDIO peripheral bring-up |
| `ENABLE_SDIO_PIN_CONFIG` | 0 (1 on H5/N6 targets) | SDIO pin configuration from PG/config |
| `ENABLE_SERIAL_SKIP_CHECK_TX` | 0 (1 on X32) | Skip serial TX-ready checks (needed on some USB/SITL paths) |
| `ENABLE_SIMULATOR` | 0 | SITL simulator hooks (gyro injection, scheduler, serial) |
| `ENABLE_SIMULATOR_GYROPID_SYNC` | 0 | Barrier between simulator gyro and PID loop |
| `ENABLE_SIMULATOR_IMU_SYNC` | 0 | Barrier for simulator IMU updates |
| `ENABLE_SIMULATOR_MULTITHREAD` | 0 | Simulator gyro on a separate thread |
| `ENABLE_UNUSED_PINS_INIT` | 1 | At boot, put every GPIO not claimed by a peripheral into a known-safe state |

---

## Accel / gyro

| Define | What it includes |
| --- | --- |
| `USE_ACC` | Accelerometer subsystem (always forced on in `common_post.h` if missing) |
| `USE_GYRO` | Gyroscope subsystem (always forced on in `common_post.h` if missing) |
| `USE_SPI_GYRO` | SPI gyro bus path (implied by any SPI gyro/acc-gyro driver) |
| `USE_I2C_GYRO` | I2C gyro bus path |
| `USE_IMU_CALC` | Onboard attitude/IMU math (SITL often `#undef`s this and injects attitude) |
| `USE_SENSOR_NAMES` | Human-readable sensor names in CLI/MSP |
| `USE_VIRTUAL_ACC` | Virtual accelerometer (SITL / unit tests) |
| `USE_VIRTUAL_GYRO` | Virtual gyroscope (SITL / unit tests) |
| `USE_GYRO_CLKIN` | External gyro CLKIN pin support |
| `USE_GYRO_DLPF_EXPERIMENTAL` | Extra experimental gyro DLPF options |
| `USE_GYRO_LPF2` | Second gyro low-pass filter stage |
| `USE_GYRO_OVERFLOW_CHECK` | Detect and handle gyro full-scale overflow |
| `USE_GYRO_REGISTER_DUMP` | CLI `gyroregisters` dump of the configured gyro |
| `USE_GYRO_SLEW_LIMITER` | Limit sample-to-sample gyro slew (spike rejection) |

### Accel / gyro chip drivers

At least one acc driver and one gyro driver must be selected when `USE_ACC` /
`USE_GYRO` are on, or `common_post.h` errors.

| Define | Device |
| --- | --- |
| `USE_ACC_MPU6050` | InvenSense MPU-6050 accelerometer (I2C) |
| `USE_GYRO_MPU6050` | InvenSense MPU-6050 gyroscope (I2C) |
| `USE_ACC_MPU6500` | InvenSense MPU-6500 accelerometer (I2C); also used for ICM-20601/02/08G |
| `USE_GYRO_MPU6500` | InvenSense MPU-6500 gyroscope (I2C); also used for ICM-20601/02/08G |
| `USE_ACC_SPI_MPU6000` | InvenSense MPU-6000 accelerometer (SPI) |
| `USE_GYRO_SPI_MPU6000` | InvenSense MPU-6000 gyroscope (SPI) |
| `USE_ACC_SPI_MPU6500` | InvenSense MPU-6500 accelerometer (SPI); also the SPI driver for MPU-9250 and ICM-20601/02/08G |
| `USE_GYRO_SPI_MPU6500` | InvenSense MPU-6500 gyroscope (SPI); also the SPI driver for MPU-9250 and ICM-20601/02/08G |
| `USE_ACC_SPI_MPU9250` | InvenSense MPU-9250 accelerometer (SPI); implies `USE_ACC_SPI_MPU6500` |
| `USE_GYRO_SPI_MPU9250` | InvenSense MPU-9250 gyroscope (SPI); implies `USE_GYRO_SPI_MPU6500` |
| `USE_ACC_ICM20601` | InvenSense ICM-20601 accelerometer (I2C alias of MPU-6500 driver) |
| `USE_GYRO_ICM20601` | InvenSense ICM-20601 gyroscope (I2C alias of MPU-6500 driver) |
| `USE_ACC_SPI_ICM20601` | InvenSense ICM-20601 accelerometer (SPI) |
| `USE_GYRO_SPI_ICM20601` | InvenSense ICM-20601 gyroscope (SPI) |
| `USE_ACC_ICM20602` | InvenSense ICM-20602 accelerometer (I2C) |
| `USE_GYRO_ICM20602` | InvenSense ICM-20602 gyroscope (I2C) |
| `USE_ACC_SPI_ICM20602` | InvenSense ICM-20602 accelerometer (SPI) |
| `USE_GYRO_SPI_ICM20602` | InvenSense ICM-20602 gyroscope (SPI) |
| `USE_ACC_ICM20608G` | InvenSense ICM-20608G accelerometer (I2C) |
| `USE_GYRO_ICM20608G` | InvenSense ICM-20608G gyroscope (I2C) |
| `USE_ACC_SPI_ICM20608G` | InvenSense ICM-20608G accelerometer (SPI) |
| `USE_GYRO_SPI_ICM20608G` | InvenSense ICM-20608G gyroscope (SPI) |
| `USE_ACC_SPI_ICM20649` | InvenSense ICM-20649 accelerometer (SPI) |
| `USE_GYRO_SPI_ICM20649` | InvenSense ICM-20649 gyroscope (SPI) |
| `USE_ACC_SPI_ICM20689` | InvenSense ICM-20689 accelerometer (SPI) |
| `USE_GYRO_SPI_ICM20689` | InvenSense ICM-20689 gyroscope (SPI) |
| `USE_ACC_SPI_ICM42605` | InvenSense ICM-42605 accelerometer (SPI) |
| `USE_GYRO_SPI_ICM42605` | InvenSense ICM-42605 gyroscope (SPI) |
| `USE_ACC_SPI_ICM42688P` | InvenSense ICM-42688-P accelerometer (SPI) |
| `USE_GYRO_SPI_ICM42688P` | InvenSense ICM-42688-P gyroscope (SPI) |
| `USE_ACCGYRO_BMI160` | Bosch BMI160 |
| `USE_ACCGYRO_BMI270` | Bosch BMI270 |
| `USE_ACCGYRO_ICM40609D` | TDK/InvenSense ICM-40609-D |
| `USE_ACCGYRO_ICM42622P` | TDK/InvenSense ICM-42622-P |
| `USE_ACCGYRO_ICM42686P` | TDK/InvenSense ICM-42686-P |
| `USE_ACCGYRO_ICM45605` | TDK/InvenSense ICM-45605 |
| `USE_ACCGYRO_ICM45686` | TDK/InvenSense ICM-45686 |
| `USE_ACCGYRO_IIM42652` | TDK IIM-42652 |
| `USE_ACCGYRO_IIM42653` | TDK IIM-42653 |
| `USE_ACCGYRO_LSM6DSO` | ST LSM6DSO |
| `USE_ACCGYRO_LSM6DSV16X` | ST LSM6DSV16X |
| `USE_ACCGYRO_LSM6DSK320X` | ST LSM6DSK320X |
| `USE_GYRO_L3GD20` | ST L3GD20 gyro |

---

## Magnetometer

`USE_MAG` without specific chips pulls in the whole mag driver suite in
`common_post.h`. `USE_VIRTUAL_MAG` is the SITL/test stand-in and skips that.

| Define | What it includes |
| --- | --- |
| `USE_MAG` | Magnetometer subsystem |
| `USE_VIRTUAL_MAG` | Virtual mag (SITL / tests); skips hardware mag drivers |
| `USE_MAG_DATA_READY_SIGNAL` | Mag DRDY / EXTI ready signal |
| `USE_SPI_MAG` | SPI magnetometer bus path |
| `USE_MPU9250_MAG` | Use the AK8963 inside an MPU-9250 as the mag |
| `USE_MAG_HMC5883` | Honeywell HMC5883L (I2C) |
| `USE_MAG_SPI_HMC5883` | Honeywell HMC5883L (SPI) |
| `USE_MAG_QMC5883` | QST QMC5883 family parent (implies L and P) |
| `USE_MAG_QMC5883L` | QST QMC5883L |
| `USE_MAG_QMC5883P` | QST QMC5883P |
| `USE_MAG_LIS2MDL` | ST LIS2MDL |
| `USE_MAG_LIS3MDL` | ST LIS3MDL |
| `USE_MAG_AK8963` | AKM AK8963 (I2C) |
| `USE_MAG_SPI_AK8963` | AKM AK8963 (SPI) |
| `USE_MAG_MPU925X_AK8963` | AK8963 behind an MPU-9250/MPU-9255 |
| `USE_MAG_AK8975` | AKM AK8975 |
| `USE_MAG_IST8310` | iSentek IST8310 |
| `USE_MAG_MMC560X` | MEMSIC MMC5603/MMC5983 family |

---

## Barometer / vario

I2C-only BMP280 and MS5611 are dropped if `USE_I2C` is off. `USE_VARIO` is
implied by baro or GPS.

| Define | What it includes |
| --- | --- |
| `USE_BARO` | Barometer subsystem |
| `USE_VIRTUAL_BARO` | Virtual baro (SITL / tests) |
| `USE_VARIO` | Vario (climb rate) from baro and/or GPS |
| `USE_BARO_MS5611` | MEAS MS5611 (I2C) |
| `USE_BARO_SPI_MS5611` | MEAS MS5611 (SPI) |
| `USE_BARO_BMP085` | Bosch BMP085 |
| `USE_BARO_BMP280` | Bosch BMP280 (I2C) |
| `USE_BARO_SPI_BMP280` | Bosch BMP280 (SPI) |
| `USE_BARO_BMP388` | Bosch BMP388 (I2C) |
| `USE_BARO_SPI_BMP388` | Bosch BMP388 (SPI) |
| `USE_BARO_BMP580` | Bosch BMP580 (I2C) |
| `USE_BARO_SPI_BMP580` | Bosch BMP580 (SPI) |
| `USE_BARO_BMP581` | Bosch BMP581 (I2C) |
| `USE_BARO_SPI_BMP581` | Bosch BMP581 (SPI) |
| `USE_BARO_SPI_BMP5XX` | Bosch BMP5xx SPI family helper |
| `USE_BARO_DPS310` | Infineon DPS310 (I2C); also the driver for SPA06-003 |
| `USE_BARO_SPI_DPS310` | Infineon DPS310 (SPI) |
| `USE_BARO_SPA06_003` | Goertek SPA06-003; implies `USE_BARO_DPS310` |
| `USE_BARO_QMP6988` | QST QMP6988 (I2C) |
| `USE_BARO_SPI_QMP6988` | QST QMP6988 (SPI) |
| `USE_BARO_SPI_LPS` | ST LPS25H-class SPI baro |
| `USE_BARO_LPS22DF` | ST LPS22DF (I2C) |
| `USE_BARO_SPI_LPS22DF` | ST LPS22DF (SPI) |
| `USE_BARO_2SMBP_02B` | 2SMBP-02B (I2C) |
| `USE_BARO_SPI_2SMBP_02B` | 2SMBP-02B (SPI) |

---

## GPS / rangefinder / optical flow

| Define | What it includes |
| --- | --- |
| `USE_GPS` | GPS subsystem |
| `USE_VIRTUAL_GPS` | Virtual GPS (SITL / tests) |
| `USE_GPS_NMEA` | NMEA parser (defaulted on whenever `USE_GPS` is on) |
| `USE_GPS_UBLOX` | u-blox UBX protocol (defaulted on whenever `USE_GPS` is on) |
| `USE_GPS_PLUS_CODES` | Open Location Code (“plus codes”) in GPS OSD/CLI |
| `USE_RANGEFINDER` | Rangefinder subsystem (implied by any rangefinder driver) |
| `USE_RANGEFINDER_HCSR04` | HC-SR04 ultrasonic |
| `USE_RANGEFINDER_TF` | Benewake TF mini / TF Luna serial lidar |
| `USE_RANGEFINDER_MT` | Matek 3901-L0X / similar optical-flow+lidar combo (rangefinder half) |
| `USE_RANGEFINDER_NOOPLOOP` | NoopLoop TOF-Sense |
| `USE_RANGEFINDER_UPT1` | UPT1 rangefinder |
| `USE_OPTICALFLOW` | Optical-flow subsystem (implied by any OF driver) |
| `USE_OPTICALFLOW_MT` | Matek 3901-L0X optical flow (implies `USE_RANGEFINDER_MT`) |
| `USE_OPTICALFLOW_UPT1` | UPT1 optical flow (implies `USE_RANGEFINDER_UPT1`) |

---

## Receiver / serial RX

`USE_SERIALRX` is the parent. Without it, every `USE_SERIALRX_*` protocol is
stripped in `common_post.h`. SPI radio RX has been removed and is not listed.

| Define | What it includes |
| --- | --- |
| `USE_SERIALRX` | Serial receiver subsystem |
| `USE_SERIAL_RX` | Alternate parent used by `common_pre.h` to mean “caller already chose RX protocols” |
| `USE_RX_PPM` | PPM input |
| `USE_RX_MSP` | MSP as an RX provider (Configurator / MSP override) |
| `USE_RX_BIND` | Bind-button / bind-phrase support (CRSF, SRXL2) |
| `USE_RX_LINK_QUALITY_INFO` | Link-quality OSD/telemetry fields |
| `USE_RX_LINK_UPLINK_POWER` | Uplink-power OSD/telemetry field |
| `USE_RX_RSSI_DBM` | RSSI in dBm (CRSF / MAVLink) |
| `USE_RX_RSNR` | Receiver SNR field |
| `USE_SBUS_CHANNELS` | SBUS channel unpack (implied by SBUS or FPort) |
| `USE_SERIALRX_CRSF` | TBS Crossfire / ELRS CRSF |
| `USE_SERIALRX_GHST` | ImmersionRC Ghost |
| `USE_SERIALRX_IBUS` | FlySky / Turnigy iBus |
| `USE_SERIALRX_SBUS` | FrSky/Futaba SBUS |
| `USE_SERIALRX_SPEKTRUM` | Spektrum DSM / SRXL |
| `USE_SERIALRX_SRXL2` | Spektrum SRXL2 |
| `USE_SERIALRX_FPORT` | FrSky FPort (implies SmartPort telemetry) |
| `USE_SERIALRX_XBUS` | JR XBus |
| `USE_SERIALRX_JETIEXBUS` | Jeti ExBus (implies Jeti telemetry) |
| `USE_SERIALRX_SUMD` | Graupner SUMD |
| `USE_SERIALRX_SUMH` | Graupner SUMH (legacy) |
| `USE_SERIALRX_MAVLINK` | MAVLink as serial RX (implies `USE_TELEMETRY_MAVLINK`) |
| `USE_SERIALRX_TARGET_CUSTOM` | Target-supplied custom serial RX |
| `USE_CRSF_V3` | CRSF protocol v3 (extended frames) |
| `USE_CRSF_LINK_STATISTICS` | CRSF link statistics |
| `USE_CRSF_CMS_TELEMETRY` | CRSF CMS (Lua) telemetry |
| `USE_CRSF_ACCGYRO_TELEMETRY` | CRSF attitude/gyro telemetry frames |
| `USE_CRSF_OFFICIAL_SPEC` | Strict official CRSF frame layout |
| `USE_SPEKTRUM_BIND` | Spektrum bind mode |
| `USE_SPEKTRUM_BIND_PLUG` | Spektrum bind-plug hardware |
| `USE_SPEKTRUM_REAL_RSSI` | Spektrum hardware RSSI |
| `USE_SPEKTRUM_VIRTUAL_RSSI` | Spektrum fade-count virtual RSSI |
| `USE_SPEKTRUM_RSSI_PERCENT_CONVERSION` | Convert Spektrum RSSI to percent |
| `USE_SPEKTRUM_VTX_CONTROL` | Spektrum-to-VTX control |
| `USE_SPEKTRUM_VTX_TELEMETRY` | Spektrum VTX telemetry |
| `USE_SPEKTRUM_CMS_TELEMETRY` | Spektrum CMS over telemetry |
| `USE_SPEKTRUM_REGION_CODES` | Spektrum region-code handling |

---

## Telemetry

`USE_TELEMETRY` is the parent. Without it, every `USE_TELEMETRY_*` protocol is
stripped.

| Define | What it includes |
| --- | --- |
| `USE_TELEMETRY` | Telemetry subsystem |
| `USE_MSP_OVER_TELEMETRY` | MSP over CRSF / Ghost / SmartPort |
| `USE_TELEMETRY_SENSORS_DISABLED_DETAILS` | Extra CLI detail for disabled telemetry sensors |
| `USE_HOTT_TEXTMODE` | Graupner HoTT text-mode display |
| `USE_TELEMETRY_CRSF` | CRSF telemetry |
| `USE_TELEMETRY_GHST` | Ghost telemetry |
| `USE_TELEMETRY_FRSKY_HUB` | FrSky Hub (D-series) telemetry |
| `USE_TELEMETRY_SMARTPORT` | FrSky SmartPort |
| `USE_TELEMETRY_SRXL` | Spektrum SRXL telemetry |
| `USE_TELEMETRY_IBUS` | FlySky iBus telemetry |
| `USE_TELEMETRY_IBUS_EXTENDED` | iBus extended sensors |
| `USE_TELEMETRY_JETIEXBUS` | Jeti ExBus telemetry |
| `USE_TELEMETRY_HOTT` | Graupner HoTT telemetry |
| `USE_TELEMETRY_LTM` | LightTelemetry (LTM) |
| `USE_TELEMETRY_MAVLINK` | MAVLink telemetry |

---

## OSD / CMS / display / VTX / camera / LED strip

`USE_OSD` pulls in CMS, canvas, MSP DisplayPort, and the OSD extras. `USE_OSD_SD`
and `USE_OSD_HD` select analog vs HD character-OSD paths. MAX7456 is dropped if
SD OSD is off.

| Define | What it includes |
| --- | --- |
| `USE_OSD` | OSD subsystem |
| `USE_OSD_SD` | Standard-definition (analog / MAX7456) OSD |
| `USE_OSD_HD` | HD character OSD (MSP DisplayPort / HDZero-style) |
| `USE_OSD_PROFILES` | Multiple OSD profiles |
| `USE_OSD_STICK_OVERLAY` | Stick-position overlay element |
| `USE_OSD_ADJUSTMENTS` | In-OSD adjustment UI |
| `USE_OSD_QUICK_MENU` | OSD quick menu |
| `USE_OSD_CUSTOM_TEXT` | Custom OSD text (also sets `ENABLE_OSD_CUSTOM_TEXT`) |
| `USE_OSD_OVER_MSP_DISPLAYPORT` | OSD rendered over MSP DisplayPort |
| `USE_SPEC_PREARM_SCREEN` | Special prearm OSD screen |
| `USE_RC_STATS` | RC-channel statistics OSD element |
| `USE_CMS` | CMS menu system (OLED / OSD / CRSF Lua) |
| `USE_CMS_FAILSAFE_MENU` | CMS menu available during failsafe |
| `USE_EXTENDED_CMS_MENUS` | Extra CMS pages |
| `USE_CANVAS` | Canvas drawing API used by OSD |
| `USE_VIDEO_SYSTEM` | Shared video/OSD timing (MAX7456, FrSkyOSD, MSP DisplayPort) |
| `USE_MAX7456` | Maxim MAX7456 analog OSD chip |
| `USE_FRSKYOSD` | FrSky OSD pixel driver |
| `USE_MSP_DISPLAYPORT` | MSP DisplayPort output |
| `USE_MSP_DISPLAYPORT_FONT` | Uploadable DisplayPort font |
| `USE_DASHBOARD` | I2C OLED dashboard (requires `USE_I2C`; implies `USE_I2C_OLED_DISPLAY`) |
| `USE_I2C_OLED_DISPLAY` | SSD1306-class I2C OLED driver |
| `USE_OLED_GPS_DEBUG_PAGE_ONLY` | Limit OLED GPS pages to the debug page |
| `USE_VTX` | VTX parent (implies common + SmartAudio + Tramp + MSP + table) |
| `USE_VTX_COMMON` | Shared VTX state/API |
| `USE_VTX_CONTROL` | VTX control (band/channel/power) |
| `USE_VTX_TABLE` | User VTX frequency table |
| `USE_VTX_SMARTAUDIO` | TBS SmartAudio |
| `USE_VTX_TRAMP` | IRC Tramp |
| `USE_VTX_MSP` | VTX control over MSP |
| `USE_VTX_RTC6705` | RTC6705 onboard analog VTX |
| `USE_VTX_RTC6705_SOFTSPI` | RTC6705 bit-banged SPI (implies `USE_VTX_RTC6705`) |
| `USE_VTX_COMMON_FREQ_API` | Alternate Spektrum VTX frequency API |
| `USE_AKK_SMARTAUDIO` | AKK SmartAudio dialect |
| `USE_SMARTAUDIO_DPRINTF` | SmartAudio debug prints |
| `USE_SMARTAUDIO_NOPULLDOWN` | Skip SmartAudio TX pulldown |
| `USE_CAMERA_CONTROL` | Camera-control output (implied when `CAMERA_CONTROL_PIN` is set with SD OSD) |
| `USE_LED_STRIP` | WS2811/WS2812 LED strip |
| `USE_LED_STRIP_64` | Allow 64 LEDs (implies `USE_LED_STRIP`; default max is 32) |
| `USE_LED_STRIP_STATUS_MODE` | Per-LED status/color modes (implied by `USE_LED_STRIP`) |
| `USE_LED_STRIP_CACHE_MGMT` | Cache maintenance for LED-strip DMA (H7/N6) |
| `USE_WS2811_SINGLE_COLOUR` | Single-colour WS2811 path when status mode is off |
| `USE_VIRTUAL_LED` | Virtual LED device (tests / SITL) |

---

## Blackbox / flash / SD card

`USE_FLASH` implies `USE_FLASHFS` and `USE_FLASH_TOOLS`. Blackbox is implied if
flash or SD is present. USB MSC is dropped without blackbox + a storage backend.

| Define | What it includes |
| --- | --- |
| `USE_BLACKBOX` | Blackbox flight log |
| `USE_BLACKBOX_VIRTUAL` | Virtual blackbox backend (SITL) |
| `USE_FLASH` | Onboard SPI/QSPI/OSPI flash as a device |
| `USE_FLASH_CHIP` | At least one flash chip driver is present |
| `USE_FLASHFS` | FlashFS filesystem used by blackbox / config |
| `USE_FLASH_TOOLS` | CLI flash tools (`flash_erase`, …) |
| `USE_FLASH_SPI` | Flash on SPI |
| `USE_FLASH_QUADSPI` | Flash on QuadSPI |
| `USE_FLASH_OCTOSPI` | Flash on OctoSPI |
| `USE_FLASH_MEMORY_MAPPED` | Execute/read flash in memory-mapped mode (XIP); implies `USE_RAM_CODE` |
| `USE_FLASH_BOOT_LOADER` | Firmware lives in external flash; bootloader/XIP glue |
| `USE_FLASH_READS_USING_4LINES` | Quad read on W25Q128FV-class NOR |
| `USE_FLASH_WRITES_USING_4LINES` | Quad write on W25Q128FV-class NOR |
| `USE_FLASH_TEST_PRBS` | PRBS flash test pattern |
| `USE_FLASH_M25P16` | Macronix/Winbond-compatible 16 Mbit NOR (M25P16 class) |
| `USE_FLASH_W25N` | Winbond W25N NAND family helper |
| `USE_FLASH_W25N01G` | Winbond W25N01G 1 Gbit NAND |
| `USE_FLASH_W25N02K` | Winbond W25N02K 2 Gbit NAND |
| `USE_FLASH_W25M` | Winbond stacked-die (W25M) helper |
| `USE_FLASH_W25M512` | Winbond W25M512 (2×256 Mbit NOR) |
| `USE_FLASH_W25M02G` | Winbond W25M02G (2×1 Gbit NAND) |
| `USE_FLASH_W25Q128FV` | Winbond W25Q128FV 16 MB NOR |
| `USE_FLASH_PY25Q128HA` | Puya PY25Q128HA 16 MB NOR |
| `USE_FLASH_MX66UW1G45G` | Macronix MX66UW1G45G (OctoSPI NOR, e.g. STM32N6) |
| `USE_FLASH_MT29F` | Micron MT29F NAND (QuadSPI) |
| `USE_SDCARD` | SD card parent |
| `USE_SDCARD_SPI` | SD card over SPI |
| `USE_SDCARD_SDIO` | SD card over SDIO/SDMMC |
| `USE_SDIO_PULLUP` | Enable SDIO pin pull-ups |
| `USE_EMFAT_AUTORUN` | EMFAT USB MSC `autorun.inf` |
| `USE_EMFAT_ICON` | EMFAT USB MSC icon file |
| `USE_EMFAT_README` | EMFAT USB MSC README file |
| `USE_EMFAT_TOOLS` | Extra EMFAT USB MSC files |

---

## Motors / DSHOT / ESC / servos

`USE_DSHOT` implies bitbang, telemetry, and DMAR unless later `#undef`’d.
`USE_PWM_OUTPUT` is implied by DShot, LED strip, beeper, 4-way, or gyro CLKIN.

| Define | What it includes |
| --- | --- |
| `USE_MOTOR` | Motor output subsystem |
| `USE_SERVOS` | Servo outputs (helicopter / airplane) |
| `USE_PWM_OUTPUT` | Timer PWM output engine |
| `USE_BRUSHED` | Brushed-motor PWM protocol |
| `USE_ONESHOT` | Oneshot125 / Oneshot42 |
| `USE_MULTISHOT` | Multishot |
| `USE_PROSHOT` | Proshot1000 |
| `USE_DSHOT` | DShot motor protocol |
| `USE_DSHOT_BITBANG` | Bit-banged DShot (DMA GPIO) |
| `USE_DSHOT_BITBAND` | Cortex bit-band DShot path (F4) |
| `USE_DSHOT_DMAR` | DShot using DMA burst (DMAR) |
| `USE_DSHOT_TELEMETRY` | Bidirectional DShot telemetry |
| `USE_DSHOT_TELEMETRY_STATS` | Extra DShot telemetry statistics |
| `USE_DSHOT_CACHE_MGMT` | Cache maintenance for DShot DMA (H7-class) |
| `USE_ESC_SENSOR` | Serial ESC telemetry input (KISS / BLHeli) |
| `USE_ESC_SENSOR_INFO` | Extra ESC info frames |
| `USE_ESC_SENSOR_TELEMETRY` | Publish ESC sensor data on telemetry |
| `USE_ESCSERIAL` | ESC serial passthrough / 4-way related serial |
| `USE_ESCSERIAL_SIMONK` | SimonK ESC serial dialect |
| `USE_VIRTUAL_ESC` | Virtual ESC (SITL / tests) |
| `USE_SERIAL_4WAY_BLHELI_INTERFACE` | BLHeli 4-way interface parent |
| `USE_SERIAL_4WAY_BLHELI_BOOTLOADER` | BLHeli bootloader 4-way |
| `USE_SERIAL_4WAY_SK_BOOTLOADER` | SimonK 4-way bootloader |

---

## Serial ports / USB / MSP / CLI

UART/LPUART/softserial/PIOUART/VCP instance flags are normalized in
`src/main/target/serial_post.h`. `USE_SOFTSERIAL` turns on both softserial
ports.

| Define | What it includes |
| --- | --- |
| `USE_CLI` | Command-line interface |
| `USE_CLI_BATCH` | CLI batch / `{` `}` scripting |
| `USE_SERIAL_PASSTHROUGH` | Serial passthrough (Configurator / CLI) |
| `USE_UART` | Hardware UART parent (implied if any UART/LPUART/PIOUART is on) |
| `USE_UART0` | UART/USART instance 0 |
| `USE_UART1` | UART/USART instance 1 |
| `USE_UART2` | UART/USART instance 2 |
| `USE_UART3` | UART/USART instance 3 |
| `USE_UART4` | UART/USART instance 4 |
| `USE_UART5` | UART/USART instance 5 |
| `USE_UART6` | UART/USART instance 6 |
| `USE_UART7` | UART/USART instance 7 |
| `USE_UART8` | UART/USART instance 8 |
| `USE_UART9` | UART/USART instance 9 |
| `USE_UART10` | UART/USART instance 10 |
| `USE_UART11` | UART/USART instance 11 |
| `USE_UART12` | UART/USART instance 12 |
| `USE_UART13` | UART/USART instance 13 |
| `USE_UART14` | UART/USART instance 14 |
| `USE_UART15` | UART/USART instance 15 |
| `USE_USART6` | Alternate MCU-header name for USART 6 |
| `USE_USART7` | Alternate MCU-header name for USART 7 |
| `USE_USART8` | Alternate MCU-header name for USART 8 |
| `USE_LPUART` | LPUART parent |
| `USE_LPUART1` | LPUART1 |
| `USE_SOFTSERIAL` | Software serial parent (enables 1 and 2) |
| `USE_SOFTSERIAL1` | Softserial port 1 |
| `USE_SOFTSERIAL2` | Softserial port 2 |
| `USE_PIOUART` | PIO-based UART parent |
| `USE_PIOUART0` | PIO UART instance 0 |
| `USE_PIOUART1` | PIO UART instance 1 |
| `USE_PIOUART2` | PIO UART instance 2 |
| `USE_PIOUART3` | PIO UART instance 3 |
| `USE_PIOUART4` | PIO UART instance 4 |
| `USE_PIOUART5` | PIO UART instance 5 |
| `USE_PIOUART6` | PIO UART instance 6 |
| `USE_PIOUART7` | PIO UART instance 7 |
| `USE_PIOUART8` | PIO UART instance 8 |
| `USE_PIOUART9` | PIO UART instance 9 |
| `USE_INVERTER` | UART signal inverter pins |
| `USE_UART1_TX_DMA` | UART1 TX DMA |
| `USE_UART1_RX_DMA` | UART1 RX DMA |
| `USE_UART2_TX_DMA` | UART2 TX DMA |
| `USE_UART2_RX_DMA` | UART2 RX DMA |
| `USE_UART3_TX_DMA` | UART3 TX DMA |
| `USE_UART3_RX_DMA` | UART3 RX DMA |
| `USE_UART4_TX_DMA` | UART4 TX DMA |
| `USE_UART4_RX_DMA` | UART4 RX DMA |
| `USE_UART5_TX_DMA` | UART5 TX DMA |
| `USE_UART5_RX_DMA` | UART5 RX DMA |
| `USE_UART6_TX_DMA` | UART6 TX DMA |
| `USE_UART6_RX_DMA` | UART6 RX DMA |
| `USE_UART7_TX_DMA` | UART7 TX DMA |
| `USE_UART7_RX_DMA` | UART7 RX DMA |
| `USE_UART8_TX_DMA` | UART8 TX DMA |
| `USE_UART8_RX_DMA` | UART8 RX DMA |
| `USE_VCP` | USB virtual COM port |
| `USE_USB_MSC` | USB mass storage (blackbox dump); needs VCP + blackbox + flash or SD |
| `USE_USB_CDC_HID` | USB HID joystick alongside CDC |
| `USE_USB_DETECT` | VBUS / USB-detect pin |
| `USE_USB_FS` | USB Full Speed |
| `USE_USB_HS` | USB High Speed |
| `USE_USB_HS_IN_FS` | USB HS PHY in FS mode |
| `USE_USB_HS_IN_HS` | USB HS in HS mode |
| `USE_USB_OTG_FS` | STM32 USB OTG FS core |
| `USE_USB_OTG_HS` | STM32 USB OTG HS core |
| `USE_USB_ID` | USB ID pin |
| `USE_USB_CLOCK_PLL3` | USB clock from PLL3 (H7) |
| `USE_USBHS1` | USB HS controller instance 1 (X32 / N6) |
| `USE_USBHS2` | USB HS controller instance 2 (X32 / N6) |
| `USE_MSP_UART` | Dedicated MSP-on-UART helpers |
| `USE_MSP_PUSH_OVER_VCP` | Push MSP asynchronously over VCP |

---

## Buses / DMA / timers / ADC / EXTI

| Define | What it includes |
| --- | --- |
| `USE_SPI` | SPI parent |
| `USE_SPI_DEVICE_0` | SPI controller 0 |
| `USE_SPI_DEVICE_1` | SPI controller 1 |
| `USE_SPI_DEVICE_2` | SPI controller 2 |
| `USE_SPI_DEVICE_3` | SPI controller 3 |
| `USE_SPI_DEVICE_4` | SPI controller 4 |
| `USE_SPI_DEVICE_5` | SPI controller 5 |
| `USE_SPI_DEVICE_6` | SPI controller 6 |
| `USE_SPI_DEVICE_7` | SPI controller 7 |
| `USE_SPI_DMA_ENABLE_EARLY` | Enable SPI DMA in the early init path (F4/APM32) |
| `USE_SPI_DMA_ENABLE_LATE` | Enable SPI DMA in the late init path (H7/H5/N6/X32) |
| `USE_EXTENDED_SPI_DEVICE` | Extra SPI device slots beyond the default map |
| `USE_I2C` | I2C parent (required for I2C baro/mag/OLED) |
| `USE_I2C_DEVICE_0` | I2C controller 0 |
| `USE_I2C_DEVICE_1` | I2C controller 1 |
| `USE_I2C_DEVICE_2` | I2C controller 2 |
| `USE_I2C_DEVICE_3` | I2C controller 3 |
| `USE_I2C_DEVICE_4` | I2C controller 4 |
| `USE_I2C_DEVICE_5` | I2C controller 5 |
| `USE_I2C_DEVICE_6` | I2C controller 6 |
| `USE_I2C_DEVICE_7` | I2C controller 7 |
| `USE_I2C_DEVICE_8` | I2C controller 8 |
| `USE_I2C_DEVICE_9` | I2C controller 9 |
| `USE_I2C_DEVICE_10` | I2C controller 10 |
| `USE_I2C_PULLUP` | Enable internal I2C pull-ups |
| `USE_I2C_PHY` | External I2C PHY / alternate I2C PHY path |
| `USE_SOFT_I2C` | Bit-banged I2C (disables the hardware I2C driver) |
| `USE_I3C_AS_I2C` | Drive I2C through an I3C peripheral in legacy-I2C mode (STM32C5) |
| `USE_QUADSPI` | QuadSPI parent |
| `USE_QUADSPI_DEVICE_1` | QuadSPI instance 1 |
| `USE_OCTOSPI` | OctoSPI / XSPI parent |
| `USE_OCTOSPI_DEVICE_1` | OctoSPI instance 1 |
| `USE_OCTOSPI_EXPERIMENTAL` | Experimental OctoSPI flash ops not yet the default |
| `USE_DMA` | DMA parent |
| `USE_DMA_SPEC` | Resource-managed DMA spec tables (implies `USE_TIMER_DMA`) |
| `USE_DMA_MUX` | DMAMUX (G4/H7/AT32/X32) |
| `USE_DMA_RAM` | Dedicated DMA-capable RAM (G4) |
| `USE_DMA_REGISTER_CACHE` | Cache DMA register writes |
| `USE_TIMER` | Timer parent |
| `USE_TIMER_MGMT` | Timer resource manager (dropped if `USE_DMA_SPEC` is off) |
| `USE_TIMER_AF` | Timer pin alternate-function tables |
| `USE_TIMER_DMA` | Timer-triggered DMA |
| `USE_TIMER_UP_CONFIG` | Timer update-event config used by DShot/LED (H7-class) |
| `USE_TIMER_MAP_PRINT` | CLI dump of the timer map |
| `USE_ADC` | ADC parent |
| `USE_ADC_INTERNAL` | Internal channels (Vref / Vbat / MCU temp) |
| `USE_ADC_INTERRUPT` | ADC conversion-complete IRQ path |
| `USE_ADC3_DIRECT_HAL_INIT` | STM32H7 ADC3 initialized via HAL rather than the shared path |
| `USE_EXTI` | External interrupts (gyro DRDY, etc.) |

---

## MCU vendor stacks

Set from the platform `*.mk` files, not from `common_pre.h`.

| Define | What it includes |
| --- | --- |
| `USE_HAL_DRIVER` | STM32 HAL (F7/G4/H7/H5/C5/N6) |
| `USE_FULL_LL_DRIVER` | STM32 Low-Layer driver |
| `USE_STDPERIPH_DRIVER` | STM32F4 / X32 standard peripheral library |
| `USE_DAL_DRIVER` | APM32 DAL |
| `USE_FULL_DDL_DRIVER` | APM32 DDL (low-layer) |
| `USE_ATBSP_DRIVER` | Artery AT32 board-support driver |
| `USE_FULL_ASSERT` | Vendor `assert_param` / `assert_failed` |

---

## Platform, boot, and FC extras

| Define | What it includes |
| --- | --- |
| `USE_BEEPER` | Beeper output |
| `USE_PINIO` | Configurable GPIO boxes (`pinio`) |
| `USE_PINIOBOX` | Map `pinio` to mode boxes (implied by `USE_PINIO`) |
| `USE_PIN_PULL_UP_DOWN` | Configurable pin pull-up/down (implied by `USE_PINIO`) |
| `USE_BUTTONS` | Hardware button handling |
| `USE_DEBUG_PIN` | Spare debug-toggle pins |
| `USE_TXRX_LED` | Activity LED during 4-way / passthrough |
| `USE_BOARD_INFO` | Board name / manufacturer strings |
| `USE_SIGNATURE` | Board signature (depends on `USE_BOARD_INFO`) |
| `USE_RTC_TIME` | Real-time clock |
| `USE_PERSISTENT_OBJECTS` | Reset-survivable RAM objects (cycle counter, MSC RTC, …) |
| `USE_PERSISTENT_STATS` | Stats that survive reboot |
| `USE_PERSISTENT_MSC_RTC` | Keep RTC across USB MSC remount (needs flashfs + RTC + MSC + persistent objects) |
| `USE_STACK_CHECK` | Stack overflow / watermark checking |
| `USE_RESOURCE_MGMT` | `resource` CLI pin ownership |
| `USE_RESOURCE_INDEX_FROM_ZERO` | Number resources from 0 instead of 1 in CLI |
| `USE_BATTERY_CONTINUE` | Continue mAh used across reboot |
| `USE_CRAFTNAME_MSGS` | Include craft name in telemetry/MSP messages |
| `USE_CUSTOM_BOX_NAMES` | User-named mode boxes |
| `USE_PROFILE_NAMES` | Named PID/rate profiles |
| `USE_RCDEVICE` | RunCam device protocol |
| `USE_HUFFMAN` | Huffman compression (MSP / blackbox helpers) |
| `USE_CONFIG` | Unified-target `config.h` is in play (`common_pre.h` then does not enable every driver) |
| `USE_CONFIG_SOURCE` | Build-system flag: config came from `src/config` / cloud (`-DUSE_CONFIG_SOURCE`) |
| `USE_TARGET_CONFIG` | Call `targetConfigure()` / `config_helper.c` board hook |
| `USE_CONFIG_TARGET_PREINIT` | Board `targetPreInit()` hook |
| `USE_EXST` | External-storage / execute-from-external-flash target |
| `USE_MULTICORE` | Second CPU core is available |
| `USE_RAM_CODE` | Place selected functions in RAM (required for memory-mapped flash disable) |
| `USE_FAST_DATA` | Fast-data section (D-cache / DTCM helpers) |
| `USE_CCM_CODE` | Place selected code in CCM/ITCM-adjacent RAM |
| `USE_ITCM_RAM` | ITCM for hot functions (F7/H7) |
| `USE_LATE_TASK_STATISTICS` | Defer task stats to a late scheduler path |
| `USE_OVERCLOCK` | Overclock CLI/setting (frequencies gated by `ENABLE_OVERCLOCK_*`) |
| `USE_PID_DENOM_CHECK` | Guard PID loop denominator vs gyro rate |
| `USE_PID_DENOM_OVERCLOCK_LEVEL` | Numeric overclock level used with the PID denom check (not a boolean) |
| `USE_BST` | Cleanflight-era I2C “BST” companion protocol |
| `USE_MAIN_ARGS` | `main(int argc, char **argv)` (SITL) |
| `USE_64BIT_TIME` | 64-bit timebase |
| `USE_HARDWARE_REVISION_DETECTION` | Detect board hardware revision at boot |
| `USE_FIRMWARE_PARTITION` | Split firmware partition / dual-image handling |
| `USE_MCO` | MCU clock output parent |
| `USE_MCO_DEVICE1` | MCO instance 1 |
| `USE_MCO_DEVICE2` | MCO instance 2 |
| `USE_MCO_OUTPUTS` | MCO pin outputs |
| `USE_TX_IRQ_HANDLER` | Dedicated UART TX IRQ handler (X32) |
| `USE_OVERRIDE_SOFTSERIAL_BAUDRATE` | Allow overriding softserial baud |
| `USE_TIMEOUT_4WAYIF` | Timeout on 4-way interface operations |
| `USE_F7_CHECK_TX` | F7 UART TX-ready extra check |
| `USE_OTG_DEVICE_MODE` | USB OTG device mode |
| `USE_OTG_HOST_MODE` | USB OTG host mode |
| `USE_HOST_MODE` | USB host mode (legacy USB stack) |
| `USE_DEVICE_MODE` | USB device mode (X32 USBHS; mutually exclusive with host mode) |
| `USE_EMBEDDED_PHY` | On-chip USB HS PHY |
| `USE_ULPI_PHY` | External ULPI USB PHY |
| `USE_CRS_INTERRUPTS` | USB CRS (clock recovery) IRQs |
| `USE_CUBEFRAMEWORK_M7` | STM32 Cube framework M7 path |
| `USE_H7_HSERDY_SLOW_WORKAROUND` | H7 HSE-ready slow-oscillator workaround |
| `USE_H7_HSE_TIMEOUT_WORKAROUND` | H7 HSE timeout workaround |
| `USE_SPRACING_PERSISTENT_RTC_WORKAROUND` | SPRacing F3/F4 persistent-RTC workaround |
| `USE_STM3210C_EVAL` | STM3210C-EVAL board-specific USB/PHY path |
| `USE_PARAMETER_GROUPS` | Parameter group (PG) system; always defined, not used as an `#ifdef` gate in current code |

---

## Notes on implied and removed flags

- **SPI radio RX** (`USE_RX_SPI`, `USE_RX_CC2500`, `USE_RX_FRSKY_SPI`,
  `USE_RX_EXPRESSLRS`, `USE_RX_SX1280`, `USE_RX_SX127X`, `USE_RX_NRF24`,
  `USE_RX_CX10`, `USE_RX_XN297`, `USE_RX_SPEKTRUM` SPI, and related
  PA/LNA/diversity flags) has been removed.
- **`USE_USB_ADVANCED_PROFILES`** is defined when CDC HID or MSC is on, but is
  not itself tested as an include gate.
- Numbered `USE_UART*`, `USE_SPI_DEVICE_*`, `USE_I2C_DEVICE_*`, and
  `USE_PIOUART*` flags only enable that instance; pin maps still come from the
  board config.
- `CLOUD_BUILD` / `CORE_BUILD` / `SITL` / `USE_CONFIG` are *not* `USE_`/`ENABLE_`
  flags, but they change which of the above `common_pre.h` turns on by default.
