# BW1 C-Gear acceptance checklist

Current test release **0.1.2-bw1**, 2026-10-14. English US revision-0
Black (`IRBO`) and White (`IRAO`) only.

## 0.1.2 startup background fix

- [x] The reported Black savestate has the native panel plane enabled in
  hardware while the native cached mask excludes it. The former conditional
  hide call returns without changing the display in this case.
- [x] Both compiled native harnesses execute the retail visibility functions
  with this mismatch, verify startup/VBlank suppression and preservation of
  all other planes, and check restoration on cleanup/allocation failure.
- [x] Final DS emulator default-preset smoke passes in Black with wireless on
  and White with wireless off, including boot, Bike, PC, Party, Bag, Map and
  native Power cancellation/return. Serialized display-register checks every
  30 frames show the panel plane hidden whenever the custom C-Gear is visible.
  Boot screenshots also cover the initialization frames before custom actors.
- [x] **122 tests in six files** pass, including BW2 regressions.
- [x] Exact 0.1.0 and 0.1.1 runtime upgrades preserve source metadata and IDs.
- [x] Production build passes; compiled archive graphics and save record format
  are unchanged. Cold boot is required after applying the new runtime.

## 0.1.1 visual update

- [x] Five native male panel shapes cycle in the DS emulator in both games,
  with eight custom buttons, native circle/leader markers and callout labels.
- [x] Native wrench, help and Power artwork/palettes remain. Save is removed.
  The top-right signal display is replaced by plain native-green `>>`.
- [x] Held browse press advances once. Wrench dragging keeps the grab offset.
  Native Power confirmation and touch-based No return reconstruct the custom
  buttons in Black with wireless on and White with wireless off.
- [x] Final 0.1.1 default-preset DS smoke completes in both games: Repel,
  Bike mount/dismount, direct PC choices, Party, Bag, eligible Fly map, native
  Power prompt and No return. Custom buttons rebuild after each application.
- [x] All ten male/female compiled shapes and four appearance states agree
  with decoded OAM graphics at native color precision. No fixed Save cell is
  exposed in previews.
- [x] Eight-button budget: 15,680 / 16,384 sprite bytes, 63 / 128 OAM entries.
  Runtime scheduled uploads, shape changes, callout hits, transparent corners,
  dimmed-layer touch consumption, allocation rollback and cleanup pass in both
  compiled CPU harnesses.
- [x] **120 tests in six files**, TypeScript and production build pass,
  including BW2 regressions and exact 0.1.0-bw1 update recognition. Authored
  source and stable IDs survive updates and export/reopening.

Tracked style inputs: `inputs/bw1-native-style.json` (wireless off) and
`inputs/bw1-native-style-on.json` (wireless on). Local style captures and traces
are in ignored `build/bw1-{B,W}-native-style-v011/`.

## Functional coverage from 0.1.0-bw1

- [x] Independent Black/White native signatures, ARM/Thumb modes and hook
  targets checked against their retail binaries.
- [x] 0.1.0 packaged binaries completed the combined default-button DS smoke
  in both games: Bike mount/dismount, direct PC storage menu, Party, Bag,
  eligible Fly map and Save confirmation/cancel. Each returns to C-Gear.
- [x] Native Wireless, IR, Online and Radar controls hidden and their touch
  manager suppressed. Functional wireless-off controls in both games; an
  eight-button wireless-on Bag run also completed.
- [x] Ordinary Town Map without a flyer, accepted Fly travel and native
  return checked in Black.
- [x] Dowsing Machine and Pal Pad launch in both games. Dowsing's native
  close control returns in Black. Pal Pad cancellation returns in both.
- [x] Vs. Recorder launches in both games; its native touch-based Cancel
  returns in White. Startup's “Tap here” screen remains native.
- [x] Super Rod respects the native invalid-location check in both games;
  an eligible White shoreline run casts the rod normally.
- [x] Pokéradar creates a native shaking-grass record immediately in both
  games. A held press leaves the same phenomenon active; the button dims.
- [x] Native normal saving and cold restart restore custom coordinates and
  enabled Repel in Black. Original saves are not modified.
- [x] Eight simultaneous authored buttons initialize. Background cycling and
  wrench dragging work. The old fixed Save control was removed in 0.1.1.
- [x] Compiled CPU harness passes for both adapters: touch layering,
  invisible/dimmed actions, read-only flags, held presses across teardown,
  blocked press discard, native event priority, ID-based saved positions,
  reset/adjacent-byte preservation, owned application requests, scheduled
  uploads, allocation-failure rollback and repeated cleanup.
- [x] That release's focused suite passed **117 tests in six files**, including
  BW2 regressions. Covers compiler/editor behavior, limits, resource totals,
  flag bounds, action catalog, atomic installation, conflicts/corruption,
  disable/removal, draft/applied separation and exported ROM recovery.
- [x] Uninstalled BW1 drafts survive export/reopening and install afterward.
- [x] TypeScript and production Vite build pass. Existing large-chunk
  warnings remain.

The default-button smoke input is tracked in
`inputs/bw1-default-smoke.json`. Native emulator logs, screenshots, SRAM and
RAM traces are local ignored build outputs. Additional focused gameplay runs
were performed during development; the earlier binaries also completed the
combined default-button smoke after the last input fix.

## Remaining acceptance coverage

- [ ] Live emulator coverage of the female shape set; all ten forms are
  covered by compiled graphics checks and CPU harnesses.
- [ ] Exercise every Bag item, Party field skill, storage submenu and native
  communication prompt, including nested cancellation paths.
- [ ] Repeat successful fishing, accepted Fly travel, Dowsing/Recorder close,
  and normal save/cold reload in the second game.
- [ ] Pokéradar water, caves, bridges and actually entering generated spots.
- [ ] Long sessions, repeated complex application handoffs and the complete
  companion-patch combination matrix.
- [ ] Live DSi mode and physical hardware. DS emulation does not establish
  acceptance for either.

## Test files

Separately named Black/White test ROMs use the six-button preset and copied
fixture saves with required key items and a flyer. They preserve the original
ROM inputs and personal saves. Cold boot with the matching copied `.sav`;
do not load a state from another build.

See [BW1 implementation/build notes](BW1.md) for supported actions, resource
ownership, save-record layout and known presentation limits.
