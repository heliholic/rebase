# Rotorflight Build Guide

Reference for building the firmware and using the Makefile.

---

## Toolchain Setup

```bash
make arm_sdk_install   # Download and install the ARM GCC toolchain into the repo
make arm_sdk_clean     # Remove the installed toolchain
```

The toolchain is installed locally under the repo; no system-wide installation is required.

---

## Core Build Commands

```bash
make                         # Build the default target (STM32F722)
make TARGET=STM32F745        # Build a unified MCU target
make TARGET=STM32F745 V=1    # Build with verbose compiler output
make ci                      # Build all CI targets (BASE_TARGETS minus .exclude)
make base                    # Build all base targets including excluded ones
make clean                   # Delete all build artefacts in obj/
make clean_all               # Clean every target
make TARGET=STM32F745_clean  # Clean one specific target
```

Build output goes to `obj/`. Binaries are named
`obj/rotorflight_<version>_<name>.hex` / `.bin`
(for example `obj/rotorflight_5.0.0_STM32F722.hex`).

`make version` prints the firmware version string (`5.0.0` plus optional `FC_VER_SUFFIX`).

### Valid TARGET values

Unified MCU targets live under `src/platform/<family>/target/<TARGET>/`.
Current families: STM32, APM32, AT32, PICO, ESP32, X32, SIMULATOR.

Run `make targets` for the current list. Do not assume names such as `STM32F7X2`
or `STM32G47X` — this tree uses `STM32F722` and `STM32G474`.

Some targets are excluded from CI via a `.exclude` file in the target directory
(ESP32, RP2350, SITL, and a few STM32 parts). `make ci` skips those; `make base` does not.

---

## Key Make Variables

| Variable | Default | Purpose |
|---|---|---|
| `DEFAULT_TARGET` | `STM32F722` | Target built by bare `make` / `make all` |
| `TARGET` | *(none)* | Unified MCU target. Do not set together with `CONFIG` |
| `CONFIG` | *(none)* | Board config overlay (enables `USE_CONFIG`) |
| `DEBUG` | *(empty)* | `INFO` = debug symbols + optimisations; `GDB` = minimal optimisations |
| `EXST` | `no` | `yes` = build for External Storage Bootloader |
| `RAM_BASED` | `no` | `yes` = image loaded into RAM |
| `FLASH_SIZE` | *(auto)* | Override flash size in KB |
| `V` | `0` | Verbosity: `0` = quiet, `1` = full compiler commands |
| `OPTIONS` | *(empty)* | Extra compile-time `-D` feature flags |
| `EXTRA_FLAGS` | *(empty)* | Arbitrary extra CFLAGS |
| `PARALLEL_JOBS` | `$(nproc)` | `-j` value used for parallel sub-makes |
| `FC_VER_SUFFIX` | *(empty)* | Appended to the version string and passed as `FC_VERSION_SUFFIX` |

CI invokes make with `FC_VER_SUFFIX` set (for example `CI-<sha>`, `PR<n>-<sha>`, or a
pre-release tag suffix).

---

## Flashing

Flashing needs a `TARGET` or `CONFIG` so the Makefile knows which artefact to write.

```bash
make TARGET=STM32F745_flash          # Build and flash via the default method
make TARGET=STM32F722 dfu_flash      # Flash .bin via DFU (USB)
make TARGET=STM32F722 tty_flash      # Flash .hex via serial port
make TARGET=STM32F722 st-flash       # Flash .bin via ST-Link
make TARGET=STM32F722 unbrick        # Emergency unbrick procedure
```

`SERIAL_DEVICE` defaults to the first `/dev/ttyACM*` or `/dev/ttyUSB*` found.

---

## Testing

```bash
make test                     # Run the full unit-test suite
make test-representative      # Run a representative subset (faster; one expansion per test)
make test-all                 # Run all tests including all per-target expansions
make junittest                # Run tests and emit JUnit XML in obj/test/
make test_clean               # Remove test build artefacts
make test_help                # List available individual tests
make test_versions            # Print compiler versions used for tests
make test_<name>              # Run a single test, e.g. make test_maths_unittest
```

Tests live in `src/test/unit/`. They use Google Test. Each test file is
`*_unittest.cc`; stubs for firmware dependencies live in `*_stubs.c`.

---

## Static Analysis

```bash
make cppcheck
```

The firmware compiles with `-Werror` — every warning is a build error.

---

## Release / Distribution

```bash
make TARGET=STM32F745_zip    # Build target and create a distributable .zip
make TARGET=STM32F745_rev    # Build and include git revision in the filename
make version                 # Print firmware version string
make all_rev                 # Build all CI targets with revision in filenames
```

---

## Configs (Board Overlays)

Most flight-controller boards are built from a `CONFIG` overlay in the `src/config`
submodule, not from a raw `TARGET`. The overlay selects the MCU target and board pins.

```bash
make configs                 # Populate/refresh src/config
make CONFIG=<board>          # Build a config-based target
make <board>_clean           # Clean a config-based target
```

`TARGET` and `CONFIG` are mutually exclusive.

---

## Local Developer Overrides

Create `mk/local.mk` (gitignored) to override variables without touching the
main Makefile. For example:

```makefile
PARALLEL_JOBS := 8
DEFAULT_TARGET := STM32H743
```

---

## Useful Introspection Targets

```bash
make help               # Print the full help message
make targets            # Machine-readable list of platforms and targets
make target-mcu         # Print MCU type for the current TARGET
make targets-by-mcu     # Build all targets matching a given MCU_TYPE
make targets-ci-print   # Print the list of CI targets (space-separated)
```
