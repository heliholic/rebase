# AGENTS.md

This file provides guidance to AI coding agents working with this repository.

## Project Overview

Rotorflight is a safety-critical embedded C firmware for single-rotor RC helicopters,
built on Betaflight. It does not support multi-rotors or airplanes.

It runs on STM32 (F4/F7/G4/H5/H7/C5/N6), APM32, AT32, Raspberry Pi Pico (RP2350),
ESP32, X32, and SITL. Code quality and correctness are paramount — this firmware
controls moving rotors.

Domain vocabulary is in [`.agent/glossary.md`](.agent/glossary.md).

## Build Commands

Full build reference is in [`.agent/build-guide.md`](.agent/build-guide.md).

```bash
make                          # Build default target (STM32F722)
make TARGET=STM32F745         # Build a unified MCU target
make CONFIG=<board>           # Build a board config overlay
make ci                       # Build all CI targets
make clean                    # Delete build artefacts
make arm_sdk_install          # Install ARM toolchain
make TARGET=STM32F722 dfu_flash   # Flash via DFU (TARGET or CONFIG required)
```

Key `make` variables: `TARGET`, `CONFIG`, `DEBUG` (empty/INFO/GDB), `V` (0/1),
`FC_VER_SUFFIX` (optional version suffix, used by CI).

Do not set `TARGET` and `CONFIG` together. Build output goes to `obj/` as
`rotorflight_<version>_<name>.hex` (for example `obj/rotorflight_5.0.0_STM32F722.hex`).

## Testing

```bash
# Run all unit tests
make test

# Run a single test
make test_maths_unittest
```

Tests live in `src/test/unit/` and use Google Test. Test files are `*_unittest.cc` with `*_stubs.c` for dependencies.

## Static Analysis

```bash
make cppcheck
```

The project compiles with `-Werror` — all warnings are errors. Zero tolerance.

## Architecture

### Source Layout

```
src/
├── main/           # Firmware source
│   ├── build/      # Entry, version, debug, main.c
│   ├── fc/         # Flight controller core (arming, failsafe, RC, tasks)
│   ├── flight/     # Control algorithms (PID, IMU, mixer, autopilot)
│   ├── drivers/    # Hardware drivers (accgyro/, barometer/, bus_i2c, etc.)
│   ├── sensors/    # Sensor fusion and alignment
│   ├── rx/         # Receiver protocols (CRSF, SBUS, DSM, IBUS, GHOST, etc.)
│   ├── telemetry/  # Telemetry output (CRSF, S.Port, HoTT, ESC telemetry)
│   ├── io/         # Serial, GPS, VTX, LED, DroneCAN, etc.
│   ├── msp/        # MSP protocol (Rotorflight Configurator communication)
│   ├── cli/        # Command-line interface
│   ├── blackbox/   # Flight log recording
│   ├── scheduler/  # Real-time task scheduler
│   ├── osd/        # On-screen display
│   ├── cms/        # Configuration menu system
│   ├── pg/         # Parameter groups (EEPROM-based config)
│   ├── config/     # Configuration handling
│   ├── msc/        # USB mass-storage
│   ├── target/     # Common target post-processing headers
│   └── common/     # Shared utilities (maths, filters, header.h)
├── platform/       # MCU-specific code (STM32, APM32, AT32, PICO, ESP32, X32, SIMULATOR)
├── config/         # Board config overlays (submodule; hydrate with `make configs`)
└── test/unit/      # Google Test unit tests
```

### Key Subsystems

**Flight control system** (`flight/`): The core control pipeline for helicopter flight.

**Flight controller core** (`fc/`): System init, arming, RC processing, and output management.

**MSP protocol** (`msp/`): Binary protocol used by Rotorflight Configurator.

**CLI** (`cli/`): Text-based configuration interface over serial.

**Parameter groups** (`pg/`): All persistent configuration is stored via the PG system — macros like `PG_DECLARE`.

**Conditional compilation**: Features are gated with `USE_*` defines (e.g., `USE_DSHOT`, `USE_GPS`).
Unified targets enable features in `src/platform/<family>/target/<TARGET>/target.h`.
Board builds additionally apply `src/config/configs/<BOARD>/config.h`.

## C Coding Standards

Full coding standards are in [`.agent/coding-standards.md`](.agent/coding-standards.md).

Key points:
- 4 spaces, never tabs. Max 120 characters per line.
- Allman braces for function definitions, K&R for control structures.
- Variables: `snake_case`; functions: `camelCase`; macros/constants: `UPPERCASE_WITH_UNDERSCORES`; types: `_t` suffix.
- `float` only — never `double`. Float literals must have `f` suffix: `3.14159f`.
- Static allocation only — no `malloc`/`free`. Stack variables ≤ 128 bytes.
- No recursion. ISRs must not block, allocate, or log.
- New files copy the Rotorflight GPL3 header from `src/main/common/header.h`.

## Change Documentation

Any change to CLI commands, MSP protocol, or default values **must** be documented in
`Releases.md` under the heading for the version being released (`# 5.0.0`, or
`# 5.0.0-RC1` for a pre-release). CI extracts that section for GitHub releases.

## PR Guidelines

Full review checklist is in [`.agent/review-checklist.md`](.agent/review-checklist.md).

- Each PR must have a clear, limited scope. Do not mix unrelated changes.
- Describe whether the change is flight-critical (affects control loop, sensor pipelines, failsafes, arming, power management, or timing).
- State testing performed (bench only / hover test / full flight, including heli type).
- Do not make drive-by formatting or renaming changes unless strictly necessary for the PR.
