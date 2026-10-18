# BW1 C-Gear Buttons

Release **0.1.3-bw1** supports English US revision-0 Black (`IRBO`)
and White (`IRAO`). Open **More → C-Gear Buttons**, or the Configure Buttons
link in **Code Injection → Quality of Life → C-Gear Quick Actions**.

## Available actions

The initial preset remains REPEL, PC, BIKE, MAP, PARTY and BAG. Up to eight
editable buttons are supported, including duplicate actions. The BW1 catalog
offers unlimited Repel, direct PC storage choices, Bicycle, Town Map / eligible
Fly, HM02-gated Fly without a learned-move requirement, ordinary Town Map,
Dowsing Machine, Super Rod, Vs. Recorder, Pal Pad, Party,
Bag and Pokéradar. Item ownership, native location restrictions, confirmations,
party selection and application completion remain native.

The separate **Town Map / Fly — HM02, no Fly Pokémon required** option is hidden
without HM02 (item 421) in the TM/HM pocket. It uses the first non-Egg for the
native animation without changing moves or consuming the HM. Native location,
progression, destination and confirmation rules remain. Prohibited locations
and empty/Egg-only parties open ordinary Town Map. The existing MAP preset and
its Town Map/Fly-user requirements are unchanged.

Pokéradar immediately searches for a native phenomenon near the player. The
first badge, suitable nearby terrain, encounter data and native placement
checks still apply. It makes at most 32 searches, leaves existing phenomena
alone, and does not change ordinary walking probabilities. A press without a
suitable tile produces no spot. Grass was tested in both games; water, caves,
bridges and entering generated encounters remain on the gameplay checklist.

BW1's Xtransceiver has no usable field-item route. Medal Box, DNA Splicers and
Reveal Glass are BW2-only. The editor also retains the requested exclusion of
Gracidea. Arbitrary script IDs and function addresses are not exposed.

Native Wireless, IR, Online and Radar artwork and touch routes are removed
while the replacement is active. This also applies when an existing save has
wireless enabled. Controls work with wireless off; installation never enables
wireless. The native wrench, help and Power controls retain their original
positions, palettes and actions. There is no fixed Save control in BW1. A plain
green **>>** replaces the top-right signal display and cycles the five native
backgrounds with press feedback and the native menu selection sound. The
native logo can also cycle backgrounds.

Custom buttons use the loaded ROM's five native panel shapes for both player
genders, with the native inner circle, diagonal leader and white/gray callout
lettering. The selected color controls the panel shading. Shape changes follow
the native background selector. The editor renders these compiled resources.
BW2 `.cgb` skins are incompatible and their importer is hidden. BW2 keeps its
own runtime, skin catalog and archive formats.

The native panel background stays hidden from initial C-Gear startup through
returns from native applications. Startup can leave the native visibility
cache out of sync with the display register. The adapter reads the actual
plane mask and uses the native unconditional setter to hide only the panel
plane and synchronize the cache. Other background/sprite planes are preserved.
The original panel visibility is restored during cleanup or allocation failure;
disabled installations retain native behavior. Cold boot after updating: a
savestate restores the old runtime code as well as game state.

## Editor, installation and persistence

The shared editor supports labels, colors, dragging, numeric positions,
undo/redo and optional saved-flag visibility. Centers remain within X 16–240,
Y 16–176. Key-item buttons disappear without the item. Shown actions failing
temporary native checks are dimmed and consume their touch. Saved flags
1–2911 are read only. PC retains default hide flag `0x05ED`; Repel has no flag.
Hacks can choose their own unused flags in the editor.

PMC is the only code dependency and installs automatically. Native hook and
function signatures, field-handler groups, graphics and PC script identity
are checked before staging. An unknown conflict prevents installation.
Compilation, PMC, DLL and archive staging are atomic. Disable restores native
C-Gear behavior; staged removal follows the existing installer rules.

The owned `quick-actions/ui.narc` is format 9. It contains compiled graphics,
editable draft/applied metadata and a native graphics backup. Ordinary edits
do not rebuild ARM code. Drafts, including drafts never installed, survive ROM
export/reopening. Unapplied changes remain separate from the active design.
The native archive `a/0/3/0` is left unchanged. Exact 0.1.0-bw1 and 0.1.1-bw1 installations
upgrade in place, preserving authored source, stable IDs and saved positions.
New BW1 presets place MAP at (140, 152) to clear the restored native controls;
existing authored layouts retain their coordinates.

BW1's native C-Gear save block has no spare tail large enough for the BW2
record. The BW1 adapter uses 48 reserved bytes at offset 164 in ETC save block
68. It preserves bytes 0–163 and 212–251. The record has marker `CQG1`, version
1, a Repel preference, eight `{id, x, y}` entries, CRC16 and its complement.
Foreign nonzero data is never overwritten. No visibility flags are written.

Positions follow stable button IDs through reordering and label/action edits.
New IDs use authored defaults. Use the wrench to drag buttons. Hold L + R for
one second in wrench mode to reset only custom positions, once per hold.
Normal saving persists positions and the Repel preference. Disabling or
removing all Repel actions leaves its preference dormant.

## Native adapter and resources

Each game has independent checked bindings for field overlays 10/21 and
C-Gear overlay 29. BW1's application request is 64 bytes. Native applications
receive owned parent requests that survive C-Gear teardown. PC uses a private
copy of native script 10090; ordinary PCs are unchanged. Power uses the native
communication confirmation and shutdown/startup flow.

The original 68 actor slots remain; eight buttons plus browse add nine.
Each custom button packs six OAM pieces into 1,024 character bytes, keeping
the native panel behind its marker and label. Browse uses 512 bytes. The
maximum is 15,680 / 16,384 total sprite character bytes and 63 / 128 OAM
entries. Nine palette banks (288 bytes) occupy 1–9; native clock, logo, wrench,
help, Power and network palettes retain their original banks. The compiler
checks the loaded ROM's totals. Native wireless panels are hidden; the native
background frame and checker pattern remain.

Uploads use owned main-RAM buffers and the native VBlank scheduler after
native fades. A changed shape uploads 1,024 bytes per custom button; a pose
change updates its palette without re-uploading characters. Worst-case owned
sprite transfer is 8,992 bytes. Native background changes also upload their
5,120-byte character resource. Runtime capacity/heap checks and allocation
rollback retain the native menu if initialization cannot complete.

Native events, wrench and background controls retain priority. A held touch
activates once, including across native application reconstruction. Touches
started while blocked and queued actions superseded by native events are
discarded. Teardown releases actors, graphics, upload tasks and buffers.

## Build and checks

Set `QUICK_ACTIONS_B_ROM` / `QUICK_ACTIONS_W_ROM` for retail inputs, plus
`ARM_TOOLCHAIN_BIN` / `RPM_TOOL_JAR` if needed. Python requires ndspy, Unicorn
and pyelftools; building also requires Java and the ARM GNU toolchain.

```sh
npm run cgear:build-bw1
npm run cgear:verify-bw1
npx vitest run src/test/cgearButtons.test.ts src/test/cgearSkins.test.ts src/test/cgearQuickActionsModel.test.ts src/test/cgearQuickActionsBw1.test.ts src/test/codeInjectionEditor.test.ts src/test/codeInjectionInstall.test.ts
npm run cgear:emulator-bw1 -- /path/to/retail.nds --frames 15000 --input runtime/cgear-quick-actions/inputs/bw1-default-smoke.json --captures 4500,7500,9500,11500,13500
npm run build
```

The emulator runner writes separately named ROMs, copied fixtures, actual
emulator SRAM, screenshots and RAM traces under ignored `build/`. Original
ROMs and personal saves are read only. Use ordinary copied saves for cold
boot; emulator states retain their embedded old runtime.

See [BW1 acceptance checklist](acceptance-bw1.md) for completed checks and
remaining gameplay/hardware coverage.
