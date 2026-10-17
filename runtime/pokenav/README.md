# Pokenav

Pokenav 0.3.0 is a read-only current-area encounter browser for English US revision-0 Black 2 and White 2 and compatible derivatives. It uses C-Gear Quick Actions 0.5.0 and the shared Learnset 1.8.0 companions.

Open **Code Injection → Feature Add-Ons → Pokenav** (directly below C-Gear Quick Actions), or add **Pokenav — current area encounters** in **More → C-Gear Buttons**. The initial button caption is **NAV**. Applying a button design installs its dependencies automatically. Existing button designs and the eight-button limit are preserved. Export a new ROM and cold boot it with an ordinary battery save.

Both screens use angled charcoal panels over a scrolling grid, with native fonts, blue water accents, green land accents, and the native party toolbar's flashing blue selection outline. The bottom title displays only the area name. Four water folder tabs sit above one icon row; three land tabs sit above two spaced icon rows. The original game footer gradient holds only the native red X and blue return icons. Either touch control or B exits. A pages the selected Pokémon's evolution requirement text. L/R switches methods in the selected section from either an icon or a header, skipping empty methods and stopping at the ends. Each method retains its selected icon and column.

The bottom screen shows the active map's seasonal water and land tables. Defaults are Surf and Grass; unavailable defaults fall back to the first populated method. Header Left/Right switches through Surf, Surf Special, Rod, Rod Special or Grass, Grass Doubles, Grass Special, skipping empty methods and stopping at the ends. D-pad navigation and touch select icons and headers. Touching an available folder selects its method directly. Each method remembers its selected icon until the screen closes.

All species/forms are revealed. Duplicate slots become one icon in first-slot order. The top screen uses the existing species information panel, with a percentage replacing its party index. Primary names, stat values and percentages use the native Summary white/gray treatment; section headings and stat labels use the smaller native compact font in muted blue-gray. Hidden abilities remain purple and requirement text uses softer off-white. Percentages add the selected species/form's slot weights within that method and season; they describe the base table distribution before Repel, abilities, and other modifiers. When Nuzlocke Style Encounters is enabled, caught families appear in gray, excluded entries show 0%, and remaining base chances are normalized and rounded to whole percentages. Surf uses 60/30/5/4/1 weights; Rod uses 40/40/15/4/1. An empty section shows “No encounters”; an entirely empty map remains dismissible. Static encounters and Hidden Grottoes are outside this release.

## Runtime ownership

The C-Gear action snapshots the native field's already-selected 232-byte seasonal table. It never replaces encounter selection or maintains a separate species database. Both game profiles verify the field owner/data layout and seasonal loader. The native field application event owns suspension, fades, application completion, and overworld reconstruction. Its tagged request stays owned by the parent until the child finishes.

The shared viewer has a separate Pokenav mode. Its top renderer accepts an explicit species/form and chance, with styling guarded by encounter mode so the ordinary party viewer retains its appearance and controls. A private decrypted PK5 shell supplies the native tutor's hidden icon initialization; it is independent of the party and is never saved. Native tutor teaching states and party navigation are not entered in Pokenav. Session icon buffers use all three ROM icon palettes. Footer artwork and the selector's 64-frame color pulse are extracted and checked independently against both input games.

The dual-screen renderer allocates a bounded 157,284-byte session from the parent application heap (1), including two 49,152-byte tiled 8-bit surfaces, compact scrolling masks and a 128-glyph cache. The optional compact font is loaded from native font archive 23, member 1 and released before application cleanup. Native font glyphs retain their original metrics, bearings and shadow; native control-code strings fall back to the original text renderer. Two optional 61,440-byte panel templates avoid redrawing static top-screen geometry. All at-most-56 distinct encounter icons are loaded once through one bounded archive reader; method changes reuse those pixels.

Pokenav's optional read-only ROM cache has a combined 196,608-byte limit: individual personal/evolution archives are capped at 98,304 bytes and individual text banks at 32,768 bytes. Cached single-block UTF-16/9-bit text follows the ROM editor's representation; unusual, oversized, malformed or uncached data falls back to the existing native reader. Caches belong only to the current encounter session and are discarded on exit. The ordinary party viewer keeps its previous data path.

Selection movement updates the lower outline directly. Folder changes redraw the affected section. The upper preview refreshes on the next input turn and rapid inputs coalesce to the newest selection. Grid work yields to input and pending previews, while the native selector pulse continues. During entry, both displays stay dark until native initialization finishes and both complete surfaces can be presented together. Pokenav bypasses the tutor's palette-restoring opening fade; ordinary party/native tutor fades and the allocation-failure fallback retain their original path. A native VBlank task uploads persistent buffers to BG2; only dirty surfaces are uploaded. The task stops before native application cleanup. Optional templates and icon buffers are released at that boundary, while upload surfaces remain allocated until cleanup finishes. A required surface allocation failure falls back to a dismissible unavailable screen; optional cache failures retain the existing readers and renderer.

Pokenav reserves 777 of the lower background's 1,024 character tiles. Unused tutor windows become off-screen 1×1 windows during Pokenav initialization; their normal sizes and behavior remain unchanged in the ordinary party viewer. This fixes the 0.1.0 opening crash caused by reserving 1,345 tiles and receiving a null native window.

The versioned private configuration lives in `pokenav/ui.narc`. Installation stages PMC, shared companions, private text, configuration, and C-Gear resources atomically. Installing Pokenav alone does not enable the party LEARNSET command. Disable retains its button design, text, and shared runtime. Shared companions cannot be removed while enabled Pokenav or Custom UI needs them. The ordinary party viewer retains its own controls and index indicator.

## Verification

```sh
npm run pokenav:build
npm run pokenav:verify
npx vitest run src/test/pokenav.test.ts src/test/learnsetViewerModel.test.ts src/test/cgearButtons.test.ts src/test/cgearQuickActionsModel.test.ts
npm run pokenav:verify-rom -- /path/to/clean-white2.nds
npm run pokenav:verify-rom -- /path/to/clean-black2.nds
npm run build
```

The host and compiled CPU checks exercise grouping, forms, chances, navigation, ABI validation, missing companions, allocation failure, and cleanup. Installer checks use independent in-memory projects and export/reopen their edited encounter data. Emulator checks and their screenshots are separate evidence; CPU stubs do not establish gameplay or visual acceptance. Test ROMs and fixture copies use separate paths; source ROMs and user saves must remain intact. See [VALIDATION.md](VALIDATION.md) for observed checks and remaining acceptance limits.

## Responsiveness benchmark

`benchmark.py` measures selector/tab feedback and the selected name's first upper-screen update in emulated frames. It requires the separately exported 56-species Route 19 fixture and its NAV button layout. Input ROM and save are copied to a temporary directory before boot; existing output paths are refused.

```sh
python3 runtime/pokenav/benchmark.py TEST.nds --save TEST.sav --library /path/to/libmelonds_headless.dylib --bindings /path/to/emulator/python --out NEW-latency.json
```

These frame measurements describe emulated response, not host wall-clock throughput or physical DS latency.

`verify_entry.py` uses the same copied full Route 19 fixtures to record every opening frame, check that the first visible bottom screen is complete, exercise L/R from both sections, and check repeated B/X/return exits and unchanged party memory. Its output directory must not already exist.

```sh
python3 runtime/pokenav/verify_entry.py TEST.nds --save TEST.sav --library /path/to/libmelonds_headless.dylib --bindings /path/to/emulator/python --out NEW-entry-check
```
