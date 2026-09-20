# PG structures that want to be arrays

A survey of the 87 parameter groups in `src/main/pg/` for one shape: a group
that is really N copies of one thing, written as a singleton.

Ranked by how badly the current type misrepresents the data, not by bytes saved.
Nothing here is a proposal to change code; it is a record of where the model and
the declaration disagree.

## Why the distinction matters

`PG_REGISTER_ARRAY` keeps the element count *out* of the group's layout hash —
`pg_layout_hash()` in `src/utils/pg_dump_structs.py` adds a `pg_array:` marker
and stops there — and `pgLoad()` reconciles a stored record whose count differs
by copying `MIN(stored, pgSize)` and leaving the surplus elements at their reset
defaults.

A member array inside a singleton has no such escape: its length is part of
`sizeof`, so it is part of the hash. That is what `pg/pg_limits.h` exists to
work around, and its own header says so:

> Whole-group array lengths - `PG_DECLARE_ARRAY(type, LENGTH, name)` - are not
> covered here and stay target-derived. The layout hash ignores them, and
> `pgLoad()` reconciles a record whose element count differs.

Every `PG_MAX_*` bound in that file marks a collection that was modelled as a
member array rather than as a group.

## Tier 1 — a singleton wrapping exactly one array

Four groups are a one-member struct whose only member is an array. The struct
adds a name and an indirection and nothing else; `X()->member[i]` is spelling
`x(i)` the long way.

| Group | Type | Size |
| --- | --- | ---: |
| `dronecanDnaConfig` | `{ dronecanDnaEntry_t entry[DRONECAN_DNA_MAX_ENTRIES]; }` | 272 |
| `osdElementConfig` | `{ uint16_t item_pos[OSD_ITEM_COUNT]; }` | 168 |
| `modeActivationConfig` | `{ char box_user_names[4][16]; }` | 64 |
| `pinioBoxConfig` | `{ uint8_t permanentId[PINIO_COUNT]; }` | 4 |

**`dronecanDnaConfig` is the clearest case in the tree.** `pg/dronecan_dna.h`
already declares the element type, `dronecanDnaEntry_t`, and then wraps an array
of it in a second struct purely to have something to register. It is also the
most array-like data here: a slot table mutated at runtime, with `nodeId == 0`
marking a free slot, scanned by index in `io/dronecan/dronecan_dna.c`. Three
call sites.

**`osdElementConfig`** is one `uint16_t` per OSD item. The wrapper is the only
thing standing between `OSD_ITEM_COUNT` and the layout hash — and that enum is
explicitly designed to grow ("new enum values must be appended at the end but
before the `OSD_XXXX_COUNT` entry"). Today each appended element changes
`sizeof`, moves the hash, and the stored record is rejected.

Note this one is *not* covered by `pg_limits.h`: the bound was never pinned
because `OSD_ITEM_COUNT` does not vary by target — it varies by release, which
the fixed-bound technique cannot help with at all.

`modeActivationConfig` and `pinioBoxConfig` are the same shape with less at
stake. `modeActivationConfig` is the odd sibling of `modeActivationConditions`,
which is already an array PG in the same header.

## Tier 2 — parallel arrays indexed in lockstep

The sharpest modelling smell: two or more arrays that are only ever subscripted
with the same index, with nothing in the type tying element *i* of one to
element *i* of another.

### `serialPinConfig` — the code argues the case itself

```c
// pg/serial_port.h
ioTag_t ioTagTx[PG_MAX_SERIAL_RESOURCES];     // 29
ioTag_t ioTagRx[PG_MAX_SERIAL_RESOURCES];     // 29
ioTag_t ioTagInverter[PG_MAX_UART_RESOURCES]; // 16
```

The defaults table it is reset from is *already* an array of structs —
`serialDefaultPin[]` of `{ident, rxIO, txIO, inverterIO}`. The entire job of
`PG_RESET_FN(serialPinConfig_t, serialPinConfig)` in
`drivers/serial_pinconfig.c` is to scatter that array-of-structs into three
parallel arrays.

Honest cost: the inverter array is 13 shorter than the other two, which is why
the reset needs its `resourceIndex < ARRAYLEN(...)` guard. Array-of-structs
would carry an unused inverter byte for the 13 non-UART slots — the guard traded
for 13 bytes.

### `pinioConfig`

```c
ioTag_t ioTag[PINIO_COUNT];
uint8_t config[PINIO_COUNT];
```

The defaults are authored as pairs — `PINIO1_PIN`/`PINIO1_CONFIG`,
`PINIO2_PIN`/`PINIO2_CONFIG` — and `pg/pinio.c` then writes them out in two
separate runs of four assignments, under two separate `#if PINIO_COUNT > 4`
blocks. A `{ioTag, config}` element type would restore the pairing the target
macros already have. (`PINIO_COUNT` is a fixed 4 in `drivers/pinio.h`; the `> 4`
branches are dead.)

### `statusLedConfig`

```c
ioTag_t ioTags[STATUS_LED_COUNT];
uint8_t inversion;   // bitmask, bit i <-> ioTags[i]
```

The bitmask is an array of bool wearing a disguise, indexed by the same LED
index. `{ioTag, inverted}` x 3 is the honest model. Smallest group here at 4
bytes — pure clarity, no other payoff.

## Tier 3 — a collection plus a stray scalar

The scalar is the only reason these are not arrays.

**`vtxConfig`** = `vtxChannelActivationCondition_t conditions[10]` + `bool
halfDuplex`. The correct shape is already in the tree one header over:
`modeActivationConditions` (array PG) beside `modeActivationConfig` (singleton).
And `halfDuplex` is a *transport* property — it has nothing to do with channel
activation conditions and would sit more naturally in `vtxSettingsConfig`.

**`serialConfig`** = `portConfigs[PG_MAX_SERIAL_PORTS]` + update rate + reboot
character. 244 bytes, third-largest group. `serialUartConfig` next door is
already an array PG over a parallel resource index; `serialConfig` is the
outlier. `PG_MAX_SERIAL_PORTS` exists only because of this.

**`ledStripStatusModeConfig`** — largest group at 368 bytes, but it holds
*three* collections with three different index spaces (`ledConfigs[64]` by LED,
`colors[16]` by colour id, `modeColors[6]` by mode) plus `specialColors` and a
scalar. It is not one array. Splitting `ledConfigs` out is defensible on its own
terms; treating the whole group as a collection is not. Around 20 call sites of
`->ledConfigs[i]` across `cli/cli.c`, `msp/msp.c` and `io/ledstrip.c`.

## Tier 4 — one collection split across two groups

**`servoConfig.dev.ioTags[PG_MAX_SERVOS]` vs `servoParams`.** Per-servo data is
currently half in an array PG (`servoParams`, already
`PG_DECLARE_ARRAY(servoParam_t, MAX_SUPPORTED_SERVOS, ...)`) and half in a
singleton's nested dev struct, both indexed by the same servo number. Moving
`ioTag` into `servoParam_t` is the only conversion in this document that needs
no new group and deletes a `PG_MAX_*` outright.

**`motorConfig.dev.ioTags[PG_MAX_MOTORS]`** is the same shape, but there is no
per-motor array PG to move into, so it would mean creating one for a single
member. Weaker on clarity grounds.

**`adcConfig`** is worth reading as a *contrast*. It has
`dmaopt[PG_MAX_ADC_DEVICES]` — a genuine per-device collection, tiny — sitting
beside `adcChannelConfig_t vbat / rssi / current / external1`, four named
channels that are emphatically not interchangeable and are correctly spelled as
named members. Same header, both patterns, both right.

## Numbered fields that are arrays

Member arrays rather than group conversions, but the same question:

- **`gyroConfig`**: `gyro_soft_notch_hz_1, cutoff_1, hz_2, cutoff_2`. Already
  declared in interleaved order, so `struct { uint16_t hz, cutoff; } notch[2]`
  is **byte-identical** — only the hash moves, not the layout.
- **`quadSpiConfig`**: `ioTagBK1IO0..3, ioTagBK1CS, ioTagBK2IO0..3, ioTagBK2CS`.
  Also contiguous in the right order, so
  `struct { ioTag_t io[4]; ioTag_t cs; } bank[2]` is byte-identical too.
- **`adcConfig`**: `tempSensorCalibration1 / 2` look like a numbered pair but are
  the MCU's two distinct calibration points (TS_CAL1/TS_CAL2). Correctly named.

## Not arrays

- **`displayPortProfileMsp` / `Max7456` / `FbOsd`** — three singletons of one
  type, which looks like an array of 3. It is not. They are three named devices,
  each compiled under a different flag, each with different defaults, each used
  only by its own driver (`io/displayport_msp.c`, `io/displayport_max7456.c`,
  `io/displayport_fb_osd.c`) and by `cms/cms_menu_osd.c` for the MAX7456 one.
  Nothing ever iterates them. `displayPortProfile(1)` would be strictly less
  clear than `displayPortProfileMax7456()`, and the three PGNs carry the
  distinction for free.
- **`vtxTableConfig`** — 284 bytes and three `PG_VTX_TABLE_MAX_*` bounds, so it
  looks tempting. But it weaves three index spaces together (band, channel,
  power) plus the counts; a band-major array would strand `channelNames`,
  `powerValues` and `powerLabels`.
- **`pinPullupConfig` / `pinPulldownConfig`** — already arrays. Merging them into
  one array of `{ioTag, direction}` would be *worse*: the two PGNs carry the
  direction for free today, and a per-element direction field is redundant.
- **`osdConfig.timers[2]`, `osdConfig.rcChannels[4]`,
  `accelerometerConfig.accZero`, `boardAlignment`** — small fixed tuples and
  vectors inside heterogeneous settings groups, not collections.

## What a conversion would and would not buy

It buys nothing at the CLI. `clivalue_t` in `cli/settings.h` has no element
index; the index is baked into `offset` at compile time via
`PG_ARRAY_ELEMENT_OFFSET`, so it is still one setting name per (index, member),
macro-expanded by hand as in the `GYRO_DEVICE_RECORDS` block in
`cli/settings.c`. `offsetof(X, arr[I])` becomes
`PG_ARRAY_ELEMENT_OFFSET(X, I, member)` — a wash. The only generic index
mechanism is the section switch in `getValueOffset()`, hardcoded for exactly the
three profile PGs. MSP has no generic per-element access at all.

There is also no exported per-element reset: `pgResetElement` is file-static in
`pg/pg.c`, and `cliDefaults group_id N` works at group granularity.

What it does buy:

- the C-side model, where a collection is declared as one;
- reset templates in place of hand-written scatter functions — tier 2
  especially, and directly in the spirit of "Replace the hand-rolled default
  tables with reset templates";
- for `osdElementConfig` specifically, a layout hash that stops moving every
  time an OSD element is added.

Against that, every conversion changes the stored layout, so the old record is
rejected and the group resets to defaults once.

## Checking any of this

- `make pg-docs` regenerates `docs/pg-format.md`; the diff shows every size and
  hash that moved.
- `make pg-hash-check` proves the layouts still agree across CI targets.
- `make pg-defs TARGET=<t>` gives the readable per-header layout dump for
  eyeballing one group.
- `src/test/unit/` — check the `config_eeprom` coverage before touching
  `pgLoad()` behaviour.
