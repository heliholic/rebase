# RF 4.6 feature port plan

## Purpose

This is the plan for bringing the Rotorflight functionality of
`rotorflight/RF-4.6.x` into this tree, a new Rotorflight built on Betaflight
2026.6. It lists every feature and fix Rotorflight added on top of its
Betaflight starting point, says what this tree already has, and orders the rest.

Two ground rules shape it.

- **No compatibility with earlier Rotorflight.** This is a new version.
  Configuration dumps, EEPROM contents, CLI names, mode and adjustment numbers,
  serial function bits, Blackbox logs and telemetry sensor IDs from RF 4.x do
  not have to load, parse or match. RF 4.6 is the reference for behaviour, not a
  contract. Every identifier is allocated in this tree's own numbering.
- **MSP is out of scope.** MSP will be rewritten separately. No stage adds or
  changes MSP handlers, and no feature waits for one: features are exposed
  through the CLI, and through adjustments where RF 4.6 had them. This tree
  has no CMS or OSD.

## Source and baseline

- Source: `rotorflight/RF-4.6.x` at `53fd0637ef` (`Spektrum bind update (#496)`,
  2026-09-10). Release 4.6.0 is `118e912026`.
- Destination: `reb357` at `926ee99dda` (`Add the helicopter mixer`). That is
  Betaflight at `b41431ae98` (2026-07-31, the 2026.6 line), with the multirotor
  features, CMS, OSD, VTX, RunCam and camera control removed and the Rotorflight
  refactors listed below.
- Analysis date: 2026-09-28.

RF-4.6.x branched from Betaflight at `d1ead96d18` (2020-06-14, the start of
Betaflight 4.3 development). Its 1700 commits since then fall into three blocks:

| Commits | Range | Content |
| ---: | --- | --- |
| 823 | `6a9ab87640..168f83624f` | Betaflight 4.3 development history, reconstructed and closed by `History flattened since 4.2.0` |
| 18 | `26535f652c..deb315a98d` | F1/F3 removal and Betaflight 4.3-maintenance backports |
| 859 | `2ff7ba52d4..53fd0637ef` | Rotorflight |

The tree at `deb315a98d` differs from Betaflight 4.3.2 only by the F1/F3 removal
and those backports, so **Betaflight 4.3.2 is the starting point** and the 859
commits are the Rotorflight delta. Three of them are further Betaflight
backports (`34a654ca26`, `29f3c001bc`, `869a930932`). By subject, 159 are the
original `RTFL:` rewrite of 2020-2022, 59 are the `MSP:` rework, 38 move
configuration into `pg/`, and about 30 are version, snapshot and release
commits. `Changes.md` and `Releases.md` in RF-4.6.x summarise the user-visible
changes; they were used as a cross-check, but this inventory is built from the
commits and the final source.

Use the final RF 4.6 implementation as the functional reference. Use the history
to recover intent and the reason for each fix, but never replay intermediate
implementations, reverted work or the old Betaflight substrate.

## Already in this tree

These came over with the Rotorflight refactors. They are the base the remaining
stages build on; audit them when a dependent feature lands, but do not import
them again.

| Area | RF 4.6 source | Here |
| --- | --- | --- |
| Identity, versioning, build automation, `FC_VER_SUFFIX`, `INIT_CODE`, `dfu` command, `dfu_erase_and_flash`, `FLASH_CONFIG_ERASE`, source layout, `RESERVED.DAT` | `2ff7ba52d4..1e680269d1` less the Docker and history-test scripts, `4daf24f47c`, `2f660f2516` | `3e18f39425..4fdaca36ff` |
| Removal of multirotor, airplane and navigation features, Betaflight automation and targets | the 61 `RTFL: Remove` commits | the `✂ Remove` series |
| `common/types.h`, utils, maths (incl. #451 split, median and fast approximations), CRC, stream buffer, encoding | `aee64bf3d0`, `81a0f5f69c`, `54fcf400d5`, `25472fdb0e`, `a40acdde70`, `a535da9aa0` | `6f623289a1`, `5174ae99e3`, `c0831e7973`, `8fcbe971e8`, `a00e070ab5`, `eca72cc8f8` |
| Parameter groups moved to `pg/`, layout hashes, generated `docs/pg-format.md` | the 38 `Move ... config to pg/` commits, less the three for removed features | `417388f55c` |
| Printf and `dprintf`, debug modes and `debug_axis`, `DEBUG_TIME`, features, runtime flags, `boxId_e` | `9c60e7d6f9`, `7bcf72f8ee` (#385), `e1424a1bbe`, `1058166391`, `142fee35dd`, `55f33ed777`, `d12c9d0204`, `631eddf348`, `e5b8a3f486`, `61dd16fcc3` | `1f7b9d70d7..dc9bcc8833` |
| Loop timing split and `filter_process_denom` | `209f74c705`, `3918221eef` | `f653cab8e1` |
| Servo path: per-servo centre, travel, scales, rate, speed limit, cyclic speed equaliser, geometry correction, override | `01b04fe835`, `eb02d8d5c2`, `6276b6bfad`, `0302a6ce99`, `c9ba7d7407`, `5bf4f885aa`, `3dd984e069`, `a3d383e996`, `5e343b5b0c` | `1fab87b581` |
| Motor path and motor override with keep-alive timeout | `f2197a5f5a`, `11600653c3`, `f6d4456db7` (#304) | `f6ab120456`, `1eb945573b` |
| Flash loop recording, rolling and initial erase, erase suspend | `5fc85a89ce` (#209), `1622eb17b6` (#211), `3dc051eef7` (#224) | `28d8948f89` |
| Blackbox configuration model: modes OFF/NORMAL/ARMED/SWITCH, denominator, field bitmask, grace period, custom data/string events, attitude field | `9e1ab51a36`, `ec9c0306f3`, `cff2d4b8d9`, `40ef29eb9a` (#219), `982570bf2a` (#246), `f8a4a89180` | `09f48c78f4` |
| RC processing: channel map, collective axis, deflection, automatic throttle range, failsafe throttle, RX rate tracking, RF 4.6 deadband and threshold defaults | `986102a7ef`, `b874d8094e`, `a50975f043`, `52f02396f1`, `33efbd601f` (#332), `37053c2818` (#348), `036b83b915`, `4702ec597e` (#327) | `18ade9d9fb` |
| Rates: Rotorflight curve on roll, pitch, yaw and collective; setpoint parameters stored | `7b5abe818a`, `1961f3812f`, `1988dcc088`, `91da010827`, `36c6128242`, `3f386d629b` (#345), `aebb2fd864` (#413) | `95ea76408a` |
| Modes, arming, `ARMING_DISABLED_OVERRIDE`, ACC calibration required before arming | `4fd5fa5ee1`, `7af085a259`, `eb3edb7cce` (#431), `61fcb2a587` | `3f5e92dcd1`, `aa847bd0b2`, `926ee99dda` |
| Adjustment framework with 42 ranges, profile and rate adjustments, ACC trim adjustments | `f042d3ae36`, `b1ed8a4696`, `abe0be1d7c`, `1c087ec841`, `ff806782b9`, `62458b4609`, `f779ec8678`, `71c7bbe76e`, `901e059b40` (#335), `993989fbea` (#349), `0f5136357c` (#411), `c7cad2c86f`, `83a23a626c` (#116) | `f62c2e725e` |
| Stick commands | `f14f184147` | `347be7c7ed` |
| Generic mixer: swash types incl. the 135/140 fix, phase, ring, trims, pitch limit, geometry and collective tilt correction, rules, inputs, overrides with passthrough, motorised tail | `9fc85af28d`, `b7fe4d6fe0`, `c851d2fe7b`, `41c4cb5605`, `3345557245`, `7d02c541b3`, `97ead3c8e4`, `1792ccf76c` (#208), `e32e39bd6c` (#238), `6ca532cd23`, `6121908d3a`, `c66a72f990`, `9c1a5564a9`, `5c8c2c81b2`, `dc47e42894` (#278), `225a14c5b5`, `820c0d61ef`, `bc80dde6c4`, `2f5b4e3693` (#210) | `926ee99dda` |
| Unit-test framework and GoogleTest update | `bd6f3aaa11..e7d70e9f96` (#187) | `6d84f797cf`, `7c5ad96d8d` |

Everything else Rotorflight added is in the stages below or in
[Not ported](#not-ported).

## Not ported

- **MSP.** The 59 `MSP:` commits, the MSP halves of every feature, the
  MSP-over-telemetry fixes (`b49d8d0eaa`, #344; `e4a0a69a57`, #48) and the
  ELRS guard in `MSP_SET_RX_CONFIG` (`6693e2065f`). Features that existed only
  as an MSP call
  (flight-statistics get/set, pilot user parameters, `MSP_EXPERIMENTAL`,
  setpoint query, servo centre, single-index getters, 4-way ESC selection) are
  for the rewrite to consider.
- **RF 4.6 platform and target tree**: `startup/`, `vcp*/`, the StdPeriph/HAL
  split, legacy and board targets (`ab780554e5` DEVEBOXH743, `71bad9a579` OSD
  targets, the MATEK fixes), and the custom-defaults machinery (`6378e5bc2c`,
  `2de8bdf02e`, `733806aa89`, `81f33725ba`, `14690b781d`), which board config
  overlays replace here.
- **Release and repository plumbing**: snapshots, version bumps, workflows, the
  git-history build test, Coderabbit, Docker and MSYS2 (`d9d821dddc`) support,
  `Changes.md`, the CI version suffix and release erase flag (`73245ca5ca`,
  `9b07da3ca2`),
  script portability (`864141f991`), and the `DEBUG_BUILD` rename
  (`5c9df9febc`). Target feature selection (`2f2b2ea258`, `5d3a331e45`,
  `46f497ff7b`, `c51dcdc4ef`, `9eddf6a547`, `a7b7d82f53`, #430) is redone
  under this tree's target rules in stage 19.
- **Features this tree has removed**: CMS, OSD, VTX, RunCam and camera
  control, and with them their RF 4.6 work. That is the CMS feature flag and
  build fix (`0eb4a9c41a`, `ced4323376`); the flight profile, rescue, PID
  controller, bandwidth and rates menus and their fixes (`480cd07c4d` #104,
  `f9bf901e68` #107, `15545b4235` #109, `27b48d9155` #112, `2459294ecc` #121,
  `80456bd926` #442); the OSD and CMS stick command levels (`3bc151aa01`); the
  OSD profile adjustment fix (`dcf907d45c`, #409); the Spektrum telemetry text
  screen, which is CMS on the transmitter (`eafd3e302e` #106, `fea39886fc`
  #122, `50df4f6d8e` #402); the rcdevice buffer overflow fix (`261f185afe`,
  #459); CMS on unified targets and the OSD build targets (`321baaccde`,
  `221f6249a4`); and the `pg/` moves of their configuration (`f2c4804cf2`,
  `1db612fa29`, `3e18d2dcc8`).
- **Betaflight code RF 4.6 still carried or synced**: GPS Rescue, the
  Betaflight, RaceFlight, KISS, Actual and Quick rate curves, RX-SPI protocols
  and their build fixes (`1ad308e0a8`, `a2ffeb3e88`, `9d222f716e`), and the
  gyro drivers synced from Betaflight master (`5ce7173b2f`, `ff29f26174`),
  which the 2026 base supersedes.
- **Reverted or superseded work**: PID mode 9 and `yaw_d_select`
  (`09f831abc7`, `3eb434a855`, removed by `a3f7d1b83d`); PID mode 2
  (`872f1ae77c`); governor D-gain ×10 (`b184e72fd3`, #192, reverted by
  `19bec90b63`); the throttle-jump fix reverted by `a0496af465`; the linear
  thrust law for the motorised tail (`3a65ee9dc3`, reverted by `dc47e42894`);
  the Spektrum scheduler change (`8d37ce8d42`, #403, reverted by `c57362afb1`);
  the dynamic notch disabled by `4a17343578` and re-enabled by `014bac5ac1`;
  the ACTUAL rates default (`7cf11c925c`); parameters that were added and
  later removed (`rate_limit` (`8e17e13418`), `error_rotation`,
  `rc_arm_throttle` (`920309370f`), `gov_spoolup_min_throttle` (`f722bf581e`),
  `gov_autorotation_bailout_time`, `gov_autorotation_min_entry_time`,
  `gov_lost_headspeed_timeout`, `pid_gyro_filter_type`,
  `yaw_precomp_filter_type`, `gyro_offset_yaw`).
- **Dead code and cosmetics**: the mixer input history (`951c9a0c66`), which
  nothing enables or reads; CLI setting order (`e4e8f34717`); a duplicate
  `PWM_RANGE` define (`0a00ec87eb`); comment formatting (`a27bac8e0e`).
- **Betaflight fixes the 2026 base already contains**, unless a focused test
  shows the Rotorflight behaviour is still missing.

## Decisions to make

Nothing below is blocked by compatibility, so each of these is a straight design
choice. They should be settled before the stage that needs them.

1. **Filter library (stage 1).** Every RF 4.6 flight module is written against
   its own `common/filter` (typed filters, EWMA, biquad Bessel and Butterworth,
   first-order LPF/HPF, filter stacks). Betaflight replaced the biquad with a
   state-variable filter (`3166c82069`, BF #15056), so this tree has neither
   the RF API nor a biquad. Recommendation: port the RF library, since the RF
   defaults were tuned on its responses, and keep the upstream filters for the
   upstream gyro stages that use them. Moving RF modules to SVF is possible but
   is a retuning exercise, not a port.
2. **PID controller (stage 8).** RF 4.6 ships mode 3 (the default, kept frozen
   for backward compatibility), mode 4 (where all new PID work went: I-term
   units for the axis error, shared roll/pitch I/O/F gains, forced I-term decay
   in rate mode, rescaled B, D and yaw precomp cutoff) and mode 0 (feed-forward
   passthrough). Without the compatibility constraint there is no reason to
   carry more than one controller. Mode 3 has the longer flight record; mode 4
   is where development continued, and its fixes (#328, #407) are recent.
   Recommendation: port mode 4 as the only controller and drop `pid_mode`,
   using mode 3 only as a reference when comparing logs.
3. **Profile counts (stages 8 and 10).** RF 4.6 uses six PID, six rate and six
   battery profiles on large-flash targets; this tree has four, four and three.
4. **Log and telemetry naming (stages 15 and 16).** Blackbox field and header
   names and telemetry sensor IDs are free to change. Keeping the RF 4.6 names
   where they are still accurate saves work in the Blackbox explorer and Lua
   tools; changing them costs nothing in firmware.
5. **ESC forward programming (stage 12).** RF 4.6 drives it over MSP. The
   per-ESC engines can be ported with ESC telemetry, but exposing them waits
   for the MSP rewrite unless a CLI front end is wanted in the meantime.

## Porting rules

- **A feature is a vertical slice**: parameter group and defaults, runtime,
  CLI settings and commands, adjustments, Blackbox fields and events, debug
  mode, unit tests and the `Releases.md` entry land together. No MSP. Avoid a
  final catch-up commit for CLI, Blackbox or documentation.
- **Fold the history in.** For every feature, read its full source-to-tip
  history and fold the fixes into the first implementation here, so the series
  never carries a knowingly broken state. The fix commits listed per stage are
  there for that.
- **Allocate identifiers here, once.** Parameter groups follow the `pg_ids.h`
  rule: reuse an uncommented slot nothing registers, then a number this lineage
  never allocated; commented-out numbers are retired. Box permanent IDs,
  adjustment IDs, arming-disable bits (6, 14, 19 and 26-28 are marked unused),
  serial function bits (22 upward; 11, 13, 14, 16, 17, 18 and 20 are retired),
  debug modes, Blackbox fields and telemetry sensor IDs are allocated in this
  tree's enums without reference to RF 4.6 values, and never give an existing
  number a second meaning.
- **Adapt to this tree's interfaces.** Use `controls/` for RC, rates, modes,
  adjustments and sticks, `fc/servos` and `fc/motors` for outputs, the upstream
  scheduler, timers, DMA, serial and bus frameworks, and the upstream position
  estimator. Do not restore the RF 4.6 file layout (`fc/rc*.c`,
  `flight/servos.c`, `flight/motors.c`).
- **Defaults come from RF 4.6 final**, applied with the feature that owns them
  (see stage 19). `Releases.md` records every CLI and default change, as
  `AGENTS.md` requires.
- **Commit messages describe the change on its own terms.** They do not refer
  to RF 4.6, this plan or its stage numbers.

## Dependency order

```text
filter library (used by everything below)
      |
      v
outputs --> RPM input --+--> gyro pipeline ------------------+
                        |                                    |
                        v                                    v
                    governor --> airborne --> setpoint --> PID, levelling
                                     |                           |
                                     v                           v
                                 SmartFuel                    rescue
```

Stages 10 to 18 hang off this core only where their tables say so (SmartFuel
needs airborne detection, telemetry sensors need their providers), and can be
interleaved with it. Hardware drivers can land at any time.

Two dependencies are easy to miss. The governor owns the spooled-up state and
the throttle output that the motorised tail, airborne detection and the PID
controller all read, so it comes before them even though it cannot be flown
first; in OFF and LIMIT mode it is a throttle passthrough. And the dynamic LPF
and the RPM notches both need headspeed, so RPM acquisition comes before the
gyro pipeline.

ESC telemetry is not a governor or RPM-filter source: it updates too slowly.
Those use the frequency input or bidirectional DSHOT, with pole count and gear
ratios applied in one place.

## Stages

Each stage leaves every target buildable and is reviewable on its own. Tables
list the feature, the RF 4.6 commits to read (fixes included), and what this
tree has now.

### 1. Filter library

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Typed filter interface (`filterInit/Update/Apply` by type, null filter) and filter stacks | `99e264e40e`, `60aefc9e59` | absent |
| PT1, PT2 and PT3 with gain-based initialisation; EWMA1-3; differentiator and integrator | `99e264e40e`, `ac313a430c` (#399) | partial: PT1-3 exist with a different API |
| Biquad Bessel, Butterworth and damped (DF1 and DF2); notch; peak | `106451f361` (#287), `ac313a430c` (#399) | absent: upstream uses SVF |
| First-order LPF and HPF | `d2829d6766`, `6229abf01e` (#158) | absent |
| Cutoff fixes: PT1 cutoff, PT1/EWMA1 maximum, cutoff range | `cf6862067c`, `49a967a88e`, `d747165071` | absent |

Gate: per-type unit tests for step and frequency response against reference
values, behaviour near Nyquist, zero and out-of-range cutoffs, and cutoff
updates that do not kick the state.

### 2. Finish the output path

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Swashplate indication wiggles (ready, arming blocked, arming failure, armed), after the output limiter, only before first arming | `96d9559005`, `1e2c978ffe` | absent |
| Servo scale down to 50 (T-40 servos) | `43f93ae082` (#453) | partial: minimum is 100 |
| Servo pulse range 50-2500 µs; motor PWM uses the servo pulse limits for narrow-band throttle servos | `b3741c2c7d` (#470), `37b8eee4b6` | partial: 100-2500 µs |
| Outputs shut down on every reset path (reboot, bootloader, MSC), the servo twitch fix | `35f51fc62e` | partial: only CLI and MSP reboot |
| Grace time before outputs are driven after boot | `be33181f5e` | audit |
| Tail motor starts as soon as the main rotor is spooled | `7ee7c382a0` (#382) | partial: needs the governor's spooled-up state (stage 5) |

Gate: neutral start-up, no pulse before a real position is known, correct sign
for both rotor directions, bounded outputs for every wiggle, no wiggle after
first arming.

### 3. Motor RPM and frequency input

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Timer-capture frequency input with pins, pull, edge and minimum frequency | `4dde8fc18e`, `c23d33a321`, `89f40ce7f6`, `643af66566`, `d36438a894` (#152), `5b0cc3f408` (#452) | absent |
| Per-motor pole count (default 0, input off), RPM low-pass, RPM correction factor | `8872634461`, `56bc02cc9d`, `7ffc8bf4cd` (#333) | absent: one upstream `motorPoleCount` and `kv` |
| Main and tail gear ratios; headspeed and tail speed | `099f71dec5`, `fbc69ec414` | absent |
| RPM source per motor: frequency input, bidirectional DSHOT, ESC telemetry, with a fast-source flag | `cd21a7b839` | absent: upstream DSHOT telemetry decode exists |
| `ARMING_DISABLED_RPM_SIGNAL` | `c4f5feba10` | absent |

Gate: synthetic ramps, dropouts, glitches, wrap-around, zero pole count, direct
drive, and a test that ESC telemetry is refused as a filter or governor source.

### 4. Gyro and filter pipeline

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Gyro decimator with Bessel filter; `gyro_decimation_hz` | `6167a0fa2e`, `106451f361` (#287), `f15d93380a` (#405) | absent |
| Per-axis gyro, D-term and B-term filtering in the gyro module; global D-term filters removed | `332e8b4aab`, `60aefc9e59`, `52fcb258d3` (#414) | absent |
| Gyro filter defaults and cutoff sanity checks | `96efe99e09`, `6948783ef0`, `1ffd8b35d7`, `597ad8f3c0` | absent |
| RPM notch filter: up to 16 notches per axis on main and tail harmonics, presets, Q up to 25, inactive notches parked at minimum Hz, direct-drive check, lower CPU cost, `ARMING_DISABLED_RPMFILTER` | `d6937bc76a`, `8c0b4246f6`, `2ce644db72`, `f9a8c35b33`, `b5dec8d802`, `2a2c1d1548` (#94), `6b18472160`, `ba7357fc48`, `245a5561a6` (#194), `2feff98650` (#288), `b332bb6fc9` (#406) | absent: `USE_RPM_FILTER` removed (`2c32c3fae8`) |
| Dynamic notch, RF version: four notches, on by default, updated in the filter subtask | `2d02ff3a6c` (#46), `e2390575c5`, `54d3537f2a`, `ac8678b7d8`, `014bac5ac1` | absent: removed (`356a4a4d61`) |
| Dynamic gyro LPF scaled by the headspeed ratio | `0fd79ab2b7`, `e18ec01809` | absent: `USE_DYN_LPF` removed (`7e1af61165`) |
| Gyro calibration: Bessel noise filter, DC/HF split, completes below a noise limit | `eead66bbbe` (#391) | absent: upstream sum/variance calibration |
| `gyro_rate_sync`; output data rate policy (4 kHz on F4/F7, 1 kHz on F411, reduced on G4); 1 kHz ODR fix; multi-gyro off on F411 and G4; `gyroAccumulationAverage` removed | `f780027938`, `ec3de33b63` (#291), `1d9dfa6203`, `bc93286630`, `be9b17f7e9`, `6ce61222ef`, `88f9c43227`, `523efa7b46` | absent: compare with upstream ODR selection |
| Loop scheduling: PID loop split into subtasks, filter subtask, per-MCU minimum and maximum PID rate, unsynced motor update limit, filter and target looptime fix, attitude task at 500 Hz | `d176713307`, `e1de251971`, `48c98bf8f2`, `38af740f67`, `8a51b7e38a`, `cb5a620de6`, `f81ae13da7` | partial: `filter_process_denom` only |

Gate: filter responses at the loop rates of every MCU family, notch tracking on
recorded headspeed ramps, CPU load on the smallest F4, no filter reset glitch on
profile or preset change.

### 5. Main-rotor governor

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Modes OFF, LIMIT, DIRECT, ELECTRIC, NITRO; throttle types NORMAL, SWITCH, FUNCTION; states off, idle, spool-up, recovery, active, hold, fallback, autorotation, bailout, bypass | `8d9a2f83c8`, `a0560556c8`, `25024e57d4` (#314), `47f513ff9e`, `c4a55a7a81` (#338), `72606f229f` (#343), `5b53c72d7a` (#353), `99f71bafe8` (#408) | absent |
| Global configuration and per-profile governor: headspeed, P/I/D/F gains and limits, idle/auto/handover/bypass throttle, min/max throttle, dynamic minimum throttle, fallback drop, collective curve, feed-forward weights, filters | `952e6e780a`, `1d083ae519`, `de50608b76`, `f18711ef66`, `0198496f0c` (#175), `0f4fdd6d55` (#264), `87c7b9c731`, `fbd3dce3df`, `f0b89ac822`, `2eb9030bb6` (#185), `8d2e4bc0a6` (#45), `354e06522e` | absent |
| Tail torque assist and its headroom; TTA into swash and tail precomp | `94a5b28a97`, `8992650335`, `0e9733bebc`, `e573a41bf7`, `83a1e1b505`, `a1fcb4de9d`, `f29df16285` (#92), `a7374a2fb9` (#52) | absent: mixer has no `swash_tta_precomp` |
| Passthrough behaviour: tracking in active, throttle tracking in idle | `f137419149`, `1f9792dcd3` | absent |
| Safety: RPM glitch detection, RPM and battery voltage required, operation without a voltage sensor, throttle parameter checks, bypass only above handover | `862aa2b5f6`, `4cfb6ff6b1`, `abf4bda6f8`, `3522e044d7`, `a4ce0e0654` (#433), `79967f9abf` (#416) | absent |
| Spooled-up state and throttle output for mixer, airborne and PID | - | absent: mixer reads `getThrottle()` directly |
| Governor fallback, suspend and bypass modes; `ARMING_DISABLED_GOVERNOR`; Blackbox state event and fields; `blackbox_log_governor`; adjustments; debug mode | `e6967cdfe8`, `25024e57d4` (#314), `13a07bd645`, `901e059b40` (#335) | absent |

Gate: host-side rotor and engine plant simulation, then no-blades bench tests
for spool-up, load steps, autorotation, bailout, RPM loss, sensor spikes,
voltage loss, mode changes and disarm. Output bounded in every state; stale
data goes safe.

### 6. Airborne detection

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Hands-on and airborne detection from stick thresholds, tilt and spooled-up state | `a3bf1019fa`, `92b16244b8`, `679eb83ce6` (#395) | absent: `rc_threshold` is configured |
| Airborne Blackbox event and debug mode | `a3bf1019fa`, `679eb83ce6` (#395) | absent |

Gate: take-off and landing traces, spool-up on the ground, inverted and tilted
states, and no airborne state while disarmed.

### 7. Setpoint

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Setpoint from stick deflection through the rate curve | `7163864bf6` | absent: rate curve present |
| Cyclic ring as a percentage of maximum rate; rates in polar coordinates | `fe1db4a094`, `3f386d629b` (#345), `e4b4e687ec` (#365), `c7550f1454` (#426) | partial: stored, not used |
| Response time and acceleration limits | `e816671c7a`, `74e7754ce6` (#153), `aee6c1fa60` | partial: stored |
| Input smoothing driven by the RX frame rate | `2d3aa1d7ef`, `fda37adfde`, `9ea32ee41e`, `301b89b657` | partial: RX rate tracked, `rc_smoothness` stored |
| Setpoint boost | `29e5ed8a58`, `a3ab3b2dd4` (#232) | partial: stored |
| Yaw dynamic ceiling and deadband | `4e24ff6b9a` (#259), `bf48934e9d` (#394) | partial: stored |
| Yaw sign with `rcCommand` | `1395bf6507` (#63) | audit |
| Setpoint adjustments, Blackbox setpoint, setpoint and RC debug modes | `723a56b774`, `d8352925bb`, `85e2d1b721`, `901e059b40` (#335) | partial: Blackbox `setpoint` field exists |

Port `setpoint_unittest.cc` and add boundary tests at zero and full stick,
diagonals, collective limits, changing RX rates, and polar against non-polar.

### 8. PID controller and stabilised modes

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Rotorflight PID profile: 16-bit P/I/D/F/B/O gains, error decay, offset flood relax, I-term relax, offset and error limits, per-axis D/B/gyro cutoffs, yaw stop gains, yaw precomp, pitch-collective FF, cyclic cross-coupling | `ad8e3d1460`, `910f1ea203`, `24d31b7b71`, `60b5d8c80f` (#285) | absent: profile holds name and P/I/D/F only |
| HSI: offset term and offset flood | `072174f0ea`, `44c3be0757`, `33f3770634`, `f96f88e19c` (#220), `8873f42bfd` (#266), `b7e37837ea` | absent |
| Error decay: linear, ground, curves; yaw I-term ground decay | `663a127597`, `bcd037e2ef`, `664c1d06df`, `421d902eea`, `fe884f301f` (#267) | absent |
| B-term | `29e5ed8a58`, `21fc796a88` | absent |
| Cyclic cross-coupling on a high-pass filter | `e41061d660`, `755b69c757`, `2507814b26`, `6229abf01e` (#158), `81666ca206`, `a864b66551` | absent |
| Yaw precomp: cyclic and collective FF, cutoff, inertia precomp | `7999a0f082`, `e90e57727d`, `4b2de047d7`, `6d384406a5`, `61879f99d7` (#186), `2001d1581b` (#247), `c06382ebc1` (#274), `4114fc95e0` (#290), `74f83ecfdd` | absent |
| Yaw stop gain transition; error rotation | `09a2e9e81e`, `ed70d525be`, `a60db2a73d`, `31fbd8d456` (#294) | absent |
| Controller modes and fixes (decision 2) | `f1fbdb78af` (#293), `7b52163fed` (#328), `1e782ec982` (#407), `de8017ecfe` (#262), `6f509bbb27`, `3b48e6734e`, `29e751307b`, `85ecf66c48`, `67b5a9a6ea`, `48347a240a`, `d0cba17050` (#231), `52fcb258d3` (#414), `822a94f480` (#393) | absent |
| Collective stabilisation | `ad8e3d1460` | partial: `pidGetCollective()` stub feeds the mixer |
| Angle and horizon modes, horizon gain fix, horizon inverted self-levelling | `203d206301`, `960fe247b7`, `ae2ba81e8d` | absent |
| Acro trainer, with the failure fix | `3ff1163202`, `47298a528c` (#352), `f84fda0f52` (#420) | absent: upstream trainer removed (`2ec8e416eb`) |
| Profiles: count (decision 3), no term reset on profile change | `3577718154`, `144cd83d06` (#418) | partial: four profiles, switching present |
| PID, precomp, cross-coupling, level and trainer adjustments | `723a56b774`, `cd48292854`, `daa2d7c8a1`, `2001d1581b` (#247), `4114fc95e0` (#290), `901e059b40` (#335) | absent: framework present |
| Blackbox B and O terms, PID and bandwidth headers, feed-forward encoding | `21fc796a88`, `44c3be0757`, `6dcf75bc60` (#118), `6e942c06a9` (#147), `b65af910da` (#131), `046ace0f16` | absent |

Gate: deterministic vectors for each term, starting from the RF 4.6 unit-test
skeleton (`dc0e970637`, #245), integrator bounds, finite-value and
divide-by-zero guards, profile switching without stale state, and measured loop
budget on F4 and F7.

### 9. Rescue

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Rescue modes (climb, altitude hold) and states (pull-up, flip, climb, hover, exit); flip on by default; inverted controls in hover; setpoint rate and acceleration limits; collective limits | `c8ae303ad4`, `1007d75093`, `39e489c2df`, `23d32d59f7`, `f693954ff9`, `8782caaa3d` (#269), `86700e6491` (#324) | absent |
| Altitude hold PID | `6839a06e26`, `39e489c2df` | absent |
| Rescue mode box, `ARMING_DISABLED_RESC` while active, Blackbox events, adjustments, LED rescue mode | `f36a6ea174`, `22cf63b5f2`, `31a39efb76` (#369) | absent |
| Altitude source: barometer and position refactors, baro/GPS fusion, baro filter defaults, altitude and vario types, BMP388 `powf` | `2dc953946f`, `3abbccccdd`, `5d1eea41cc`, `397f63853c`, `de5b69298a`, `711829d460` (#455) | upstream: use the 2026 position estimator, validate the behaviour |

Gate: SITL or a deterministic attitude and altitude harness covering upright
and inverted entry, bad or missing barometer, stale attitude, activation before
arming, pilot exit, collective bounds and every state timeout.

### 10. Battery, power sensors and SmartFuel

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Voltage sensors for battery, BEC, bus and external on ADC; ESC and FBUS voltage and current sources; cutoffs in Hz; ADC update-rate parameters | `f616b749d0`, `7b38304482`, `986c3d6560`, `64af35e23c` | partial: upstream battery, 5 V, 9 V and 12 V meters and ESC meters, periods in 0.1 s |
| Cell count detection to 12S; charge percentage fix; voltage stability check | `32c16a9384`, `335e061c74`, `183882caff` (#437) | audit against upstream |
| Battery profiles with per-profile capacity and adjustment selection | `50445727f9` (#415) | upstream: `batteryProfiles` and `ADJUSTMENT_BATTERY_PROFILE`; profile count is decision 3 |
| SmartFuel: slew-limited cell voltage, sag compensation from stick load while airborne, sigmoid charge curve, optional coulomb counting, monotonic charge while armed | `f9c077d9d2` (#463) | absent |
| Flight statistics on by default | `23191c2b4c` (#460) | upstream statistics; default only |

Gate: profile switching, reconnect, partial charge, current-sensor and
voltage-sensor loss, cell-count changes, and a charge value that never rises
while armed.

### 11. ESC telemetry

First define one data model for voltage, current, consumption, eRPM, throttle,
PWM, temperatures, BEC voltage and current, status, model ID, corrections, age
and validity, then add protocols one per commit.

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Core: protocol selection, update rate, filter, half duplex, pinswap, current offset, voltage/current/consumption corrections, model ID, stream recording | `b42152ae2f`, `96dc5b0793`, `eb1b81866b`, `d9296ea148`, `1e1c00eb20`, `5b6b660f4e` (#243), `3651c79a25` (#180), `d3c4adb6a6`, `5ba3118ea7`, `17b4a74dc9` (#128), `b8da77de31` (#150), `e4764b9c56` (#356) | absent: upstream BLHeli32/KISS sensor with offset and half duplex only |
| Hobbywing V4 and V5 | `495d29268f`, `25e55644ac`, `736eeb0ee3` (#257), `9c23870602` (#340), `4cd2c79333` (#119), `b7eed4ebe1` (#127) | absent |
| Kontronik | `e4d6974bc7`, `ef7d769938` (#98), `26b874564c` (#100) | absent |
| OMPHobby, ZTW, APD, Scorpion | `0f47f53b1b`, `eb1b81866b`, `036e5fd8fd`, `e03a03a2b0` (#339) | absent |
| OpenYGE | `ca627227bf`, `fb83f371f8` (#87), `bc69ec8b3c` | absent |
| FlyRotor | `fe55ffbdaf` (#159), `c25651f666` (#176), `6de121dfad` (#179), `5842286b1b` (#263), `8402b26d99` (#467) | absent |
| Graupner | `27fe36b62d` (#174) | absent |
| XDFly | `9f531c2c6c` (#242), `ea1e784f94` (#318) | absent |
| Castle, decoded on the motor output line | `725dff7c68` (#251) | absent |
| FBUS as an ESC source | `9d3607088e` (#364) | absent (stage 14) |
| Blackbox ESC, BEC and ESC2 fields | `fc3b5e364e`, `dc17741fea` (#149), `360ea93f6e` | absent |

Use captured valid and invalid byte streams for every parser test, and prove the
UART is restored after every exit and error path.

### 12. ESC forward programming

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| YGE and Scorpion | `be50b727fb` (#108) | absent |
| Hobbywing V5 | `57d62546d7` (#114), `d46897bac5` (#465) | absent |
| FlyRotor | `fe55ffbdaf` (#159), `5edda17411` (#271) | absent |
| XDFly | `9f531c2c6c` (#242), `1c21d286cd` (#284) | absent |
| OMP and ZTW | `899ac294cf` (#347), `abde467083` (#351) | absent |
| AM32 and BLHeli_S/Bluejay over the 4-way interface; reliable teardown | `949f0e9dd7` (#306), `ad8383b2ea` (#429), `3075f4cbb6` (#440) | absent: upstream 4-way passthrough exists |

Programming is refused while armed, and the motor override keep-alive (already
here) stays in force. Exposure waits on decision 5.

### 13. Receiver protocols and fixes

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| FrSky FBUS and F.Port 2 receivers | `95b8c7ed9c` | absent |
| Futaba SBUS2 | `e0e0873082`, `d4c2929b89` (#135) | absent |
| JR XBus Mode A | `9d265352a9` (#226), `2d866c55ba` (#275), `616af6cf5d` (#289), `93b3d9cd87` (#334), `bad802eb0e` (#336) | absent |
| FlySky IBUS2 control | `7bbcaf8cc1` (#424), `fe9d81203d` (#468) | absent |
| SRXL2 forced half duplex; Spektrum bind with pinswap and glitch-free bind pin | `a557d76941` (#310), `53fd0637ef` (#496) | absent |
| SBUS frame time 4 ms (Mikado VBasic) | `b533029688` (#217) | absent: 3 ms |
| Pinswap option for serial RX, telemetry and ESC ports | `240a40e616`, `4d022ace0b` | partial: upstream UART pinswap by pin assignment |
| Push-pull drive on S.Port, F.Port and FBUS; pull-up in F4 bidirectional mode | `aa1cfaa58e` (#221), `cdda4bc121` (#205) | audit |
| Buffer overflow fixes in RX protocols | `c849f69f11` (#445) | audit each against upstream |
| `RX_PROVIDER_NONE` lock-up; RC channel count overflow | `4a16408ae6`, `a78ddd618f` | audit |
| RX configuration and RX failsafe refactors | `b198e2008e`, `d864d6496c` | partial: RC processing done, audit failsafe |

Gate: failsafe and frame-corruption tests per protocol, channel-count bounds,
and UART ownership (never opened by two tasks, never left in the wrong
direction).

### 14. Bus servos and serial output devices

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| SBUS output, with inversion | `97ecd0e80e` (#190), `0ee80a5b18` (#201), `7ad7fdf15b` (#358) | absent |
| FBUS master output: frame rate, pinswap, inversion | `f4baca5cf2` (#355), `c8b5d4dd83` (#376) | absent |
| Bus servos S9-S26 with a mixer or RX source per channel; servo limits for bus outputs; CLI | `ab9db3c749` (#384), `8034d465d8` (#435) | absent |
| FBUS master sensors feeding voltage, current, ESC and GPS | `9d3607088e` (#364), `5618254a36` (#438) | absent |
| S.Port master sensor input, independent of the telemetry feature, in its own task | `e5c319944d` (#432), `8950d642ac` (#458), `ce8c3134d7` (#448) | absent |

Gate: output bounds and deterministic frame rate, sensor frame parsing from
captures, and verified UART teardown.

### 15. Telemetry

Port the sensor registry first, with canonical units, scaling, validity and
update rates, then one transport per commit. Sensors join once their provider
exists.

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Sensor registry (about 120 sensors, including headspeed, tail speed, governor, rescue, arming, profiles, ESC1/ESC2, BEC, loads, model ID) | `7e973c2635`, `fd9838a44f`, `667498a730`, `2b592afe12`, `81cb702bfb`, `5f8f9cb556`, `8c1a80a335` | absent |
| CRSF custom telemetry and flight-mode, attitude and GPS frame reuse; 32-byte frames; native ELRS RPM and temperature frames; device-info overflow fix; real-time load | `aeeb967ce0`, `68d6a4c9ea`, `05ac008c01`, `472a0a107b`, `483a3957ec`, `c1178a755a`, `83350e62f6`, `1aaa5c56bd` (#76), `f31f8ddd40`, `84f39c4c66`, `3c9a6f7d17` (#323), `4cf466bef1` (#326), `67c77895a0`, `44fc672bce`, `34660ebab1`, `398b2ed551`, `121a9af16f` | absent: upstream CRSF telemetry |
| Adjustment value reporting over telemetry | `01fcf19c39` | absent |
| FrSky SmartPort/F.Port/FBUS on the registry, extra sensors, attitude scaling, lost-telemetry fix | `3920aac18a` (#215), `b7ae36c194` (#172), `aa9fbc95cf` (#178), `b2d14727f7` (#182), `c0e649613b` (#89), `4bba1f3362` (#261), `88baa8ef06` (#295), `2384dff632` (#313), `730870300c` (#199) | absent |
| Jeti EX Bus sensors | `ac113d2a08` (#206) | absent |
| SBUS2 sensors | `e0e0873082`, `d4c2929b89` (#135) | absent |
| IBUS2 telemetry | `7bbcaf8cc1` (#424) | absent |
| Spektrum: ESC telemetry to the transmitter, no voltage/current reports without sensors, fast-boot alarm | `a2a09f9ef2` (#354), `29e7f144c2` (#65), `1929c39bad` (#62), `e14e228e5a` (#367) | absent |

Gate: golden encoded frames per transport, sensor-ID uniqueness, unit and
scaling tests, stale-value handling, payload bounds, and task budget.

### 16. Blackbox and storage

Feature fields and events land with their features. What remains here:

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Mixer, headspeed, tail speed, throttle command, bus and BEC voltage, temperature fields | `9e1ab51a36`, `820553d242` (#39), `f616b749d0`, `360ea93f6e` | absent: field selection bits exist |
| Altitude logging, battery voltage and current coding, field condition size, multi-file logging, erase and erase-mode fixes | `64e71a2adb`, `abc03df988`, `92a173cf2f`, `632de73e90`, `ca19eed417`, `22c3c58e2a` | audit |
| Header generation macros and static header buffer | `fe27fd9737`, `dd31502545` (#286), `845894fb1e` | audit |
| Flash: empty page detection, loop gap detection, asynchronous erase, no autoflush, chip-erase guard | `04902c95e4`, `d117ab7f45`, `8ea4a63e84`, `8fd77588a8`, `0bca79f16a` (#337), `c9f3d9be34` | audit: loop recording is here |
| MSC: unique dated file names, underscores, empty log on erased flash, Mac and Android detection, emfat fix | `3a24d57d8f` (#212), `0e7b11b835` (#277), `1b2f4947ae`, `0745605d17`, `bd716703dd` (#296), `80908cb50a` (#308), `687ea88550` | absent: `RTFL_nnn.BBL` naming only |
| Default fields, ARMED mode and rolling erase by default | `6dd236bb7d`, `8a01b9812c`, `4b35aad6ed` (#412) | defaults only |

### 17. Hardware drivers

Independent of the flight stack; they can land at any point.

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Bosch BMI088, with hardware LPF options and a second ACC chip select | `c06de51ae6` (#200), `93540a4b62` (#260), `85cb933d21` | absent |
| Bosch BMI323 | `442f6b1ea7` (#363) | absent |
| Bosch BMP581 | `67673c9956` (#360) | upstream: `barometer_bmp5xx` |
| External gyro clock | `6fd1a39d3d` (#380) | upstream: `GYRO_CLKIN` |
| ICM426xx fixes; AFSR off | `bdebc266fa` (#383), `9b4fe77b0c` | audit #383; AFSR upstream |
| W25N: generic driver, W25N01KV and W25N04KV, fixes, SPI speed, blocking writes on G4 | `da31372819`, `bba2a9c262`, `4c8e26e414` (#357), `01fdf6a40a` (#379), `6061150c81`, `3923b11ebe` | partial: upstream W25N01G and W25N02K |
| PY25Q128HA | `6c9cf9b659` (#462) | upstream |

### 18. System, configuration, CLI and LED strip

| Item | RF 4.6 source | Here now |
| --- | --- | --- |
| Save pending configuration after disarm | `ac2e43a14c`, `c6c75202b4` (#249) | absent |
| `board_design`; `board_name`, `board_design` and `manufacturer_id` locked once set, with a halt on a foreign dump | `7f102f9126`, `bc22e10887` (#427) | absent |
| Model ID and model flags (`model_set_name`, `model_tell_capacity`) | `84f39c4c66`, `36b58c7855` (#156), `02237b2f67` (#317) | absent |
| ACC calibration stored with an MCU-specific key | `7ab0eeae62` (#419) | partial: calibration required, no key |
| ACC start-up calibration and trim reset fixes | `5447fdf629`, `9a42a32276` | audit |
| Real-time load monitor and arming on CPU load; load average fix | `44fc672bce`, `049ad356aa`, `0a8c220d05` | partial: upstream late-task arming check |
| LED strip scale lights (blink pattern and pause, alternate colour, flicker and dimmer overlays, status-alt profile), arm-state tweak, CLI parsing, cache management on H7 | `949713b0ac` (#35), `d2331b7178` (#85), `cf73a6500a`, `2f5d9970b6` (#42), `8d66db57e0`, `ad2764f27a` | absent |
| Disarm beep and profile-change beeps | `8e63deb690` (#132), `327cb347a3` (#117) | audit |
| Hardware parameter dump and `dma ... NONE` dump fixes | `b2f3326c85`, `430554bbca` | audit |
| Debug pin refactor; `-Wstrict-prototypes` | `f43091989b`, `fdfaa4dd30` (#456) | audit; absent |

Platform fixes to validate against the 2026 base rather than port: H7
overclock (`7a13efbc1a`, `cce86cf9f1`), H7 25 MHz HSE start-up (`bf6953f604`),
H7 frozen ADC, G4 caches and SDA2 (the three Betaflight backports), G4 USB
product name, DFU and MSC (`fdbbd52a57`, `0fcf7cbc03` #50, `6de09f49b8` #71),
F4 configuration and data placement in flash (`daa5a1a2fe`, `cc9dcb8745`), SPI
chip-select release (`bc2b988b69` #41), and the SWO pin (`3ee962adca` #297).

### 19. Defaults and completeness audit

- Apply the RF 4.6 final defaults with their owners. The final set is in
  `Changes.md` under Defaults; the history behind it is:

  | Area | RF 4.6 default commits |
  | --- | --- |
  | PID, HSI, precomp and cross-coupling | `8a040f9b75`, `02270d459d`, `66c58a3f8c`, `3341b85ba3`, `aeeebdd5a7`, `cafb2e78bf`, `664c1d06df`, `421d902eea`, `a864b66551`, `66d9a169e3` (#140), `1c83c588c6`, `b3856e2e49` (#292), `a96ee1296a` (#425) |
  | Controller mode | `cb1f64472a` (mode 3; see decision 2) |
  | Governor | `bee0eef755`, `f0b89ac822` |
  | Rescue | `788017ac9b`, `9aad5c1f40`, `86700e6491` (#324) |
  | Rates, deadbands, smoothing | `aebb2fd864` (#413), `bf48934e9d` (#394), `fda37adfde`, `301b89b657`, `aee6c1fa60` (already here except smoothing) |
  | Filters | `96efe99e09`, `6948783ef0`, `54d3537f2a`, `014bac5ac1`, `b332bb6fc9` (#406), `f15d93380a` (#405), `ec3de33b63` (#291) |
  | Mixer and outputs | `3f6f69755c` (CP-120), `3cbef97a19` and `f38372ee42` (main control rate), `cde28c3404` (servo travel), `0f6c1eebdd` (motor protocol PWM), `7ffc8bf4cd` (#333, pole count 0) |
  | Blackbox | `6dd236bb7d`, `8a01b9812c`, `4b35aad6ed` (#412) |
  | Battery and statistics | `fee0be206f` (#316), `23191c2b4c` (#460) |
  | Telemetry | `8c1a80a335` |

- Walk this inventory: every row is done, deliberately adapted, inherited from
  upstream or listed under [Not ported](#not-ported) with a reason.
- Enable features per unified target and board config rules; do not copy RF 4.6
  target headers. Check flash, RAM and CPU on the smallest F4 and loop latency
  on every MCU family.
- Remove temporary stubs, regenerate `docs/pg-format.md`, and bring
  `Releases.md` up to date.

## Verification

### Per change

- Build with the feature enabled and disabled.
- Run the focused unit tests and `make test-representative`.
- Run `make pg-hash-check` and `make pg-docs-check` for every PG change.
- Run `make cppcheck` for new modules or material control-flow changes.
- Check stack use, static allocation, divide-by-zero, finite values,
  saturation, timestamp wrap-around and stale external inputs.
- Confirm CLI inputs are range-checked and arming and failsafe checks cannot be
  bypassed.

### Per stage

- Build STM32 F4, F7, G4, H5, H7, C5 and N6 targets, plus APM32, AT32, X32 and
  SITL, with a full and a minimal feature set, and run `make ci`.
- Compare Blackbox output and behaviour against RF 4.6 on the same recorded
  inputs where the stage has an equivalent.

### Flight-critical bench ladder

1. Host and unit simulation only.
2. Powered FC with no actuators.
3. Servos connected, motor electrically disconnected, blades removed.
4. Motor connected, blades removed, with an independent emergency cut-off.
5. Restrained drivetrain test where appropriate.
6. Hover on a known airframe.
7. Progressive envelope expansion.

Mixer, PID, governor, arming and failsafe, RPM processing and rescue changes do
not pass on compilation alone. Record heli type, drivetrain, RPM source, ESC,
receiver, firmware and configuration hashes, conditions and the Blackbox log for
every flight gate.

## Definition of done

The port is complete when:

- every inventory row is done, adapted, upstream or explicitly not ported;
- no flight-critical stub remains (`flight/pid.c`, the governor passthrough,
  the setpoint);
- all intended features build on the current platform abstraction;
- PG documentation, release notes, tests, static analysis and CI are clean;
- bench and flight evidence covers mixer, PID, RPM filter, governor, rescue,
  receiver failsafe and output overrides.
