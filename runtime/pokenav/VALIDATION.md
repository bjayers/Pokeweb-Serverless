# Pokenav 0.2.2 validation

Earlier Pokenav 0.2.2 evidence, recorded 2026-10-16 for English US revision-0 Black 2 and White 2. The bundled runtime uses Learnset 1.7.2 and C-Gear Quick Actions 0.4.0. Compatible derivatives must retain the independently checked native interfaces; these results do not certify every Upgrade ROM.

## 0.2.2 entry, typography and shoulder controls

The previous 0.2.1 White 2 build reproduced the reported lower-screen stripe pattern during a frame-by-frame cold boot. The supplied `botloading.mln` uses a newer minor state format than the local headless runner accepts, so that state was not replayed. The fix gates initial BG2 mode/palette uploads until both complete surfaces and the native input state are ready, and bypasses the tutor's cached-palette opening fade only in Pokenav. Native/party and unavailable-screen fades retain their original call, including all seven arguments.

Both new games cold booted copied Route 19 saves. A frame-by-frame opening check passed four consecutive entries in each game: both screens stayed dark during preparation and the first visible encounter screen was complete. First presentation occurred 67–68 emulated frames after touch delivery, including the existing C-Gear transition and resource preparation. This removes the partial frame; it does not claim zero loading time. Initial and reopened 256×192 captures were inspected on both screens. The top uses native Summary white/gray glyph pixels, compact muted native labels, purple hidden abilities and softer requirement text; the lower layout and icon palettes are retained.

Native checks exercised L/R from icon focus in both sections, all seven methods, method endpoints, remembered selection, D-pad/touch, A paging, rapid input coalescing, percentages including 1%, four B/X/return exits, and return to C-Gear. The 1,328-byte party record stayed unchanged. The canonical `verify_entry.py` copies both inputs into a temporary directory and refuses an existing output directory. Versioned test ROMs retain unique species in all 56 Route 19 slots across all four seasons; original save hashes are checked separately from emulator evidence.

Automated checks passed: 177 focused Vitest tests, production build, both compiled Pokenav profiles, exact native compact-label/glyph and white/gray palette comparisons, initial presentation gating, optional compact-font/cache failures, required surface allocation failure, retained-buffer cleanup, and the seven-argument fade veneer. Both ROM installer checks passed atomic failure, update/reinstall, disabled/shared companion roles, private text and edited encounter export/reopen with opposite companion installation orders. The White 2 benchmark retained 1–2 frame icon feedback and 2–4 frame tab feedback; upper-name changes took 4–7 frames. Both ordinary party header and Custom UI compiled regressions also passed, including native type actors, long/edited names, navigation, visibility, animation and cleanup. The unchanged native/party opening fade was verified through its compiled seven-argument veneer; a new live NPC tutor session was not run for this revision. These are emulated-frame measurements, separate from host throughput and physical hardware.

Local logs, opening traces, screenshots and copied fixtures are under `runtime/pokenav/build/refinement-check/`. The earlier version records below retain their original scope. No physical DS/DSi acceptance is claimed.

## 0.2.1 performance checks

The same full Route 19 fixture was cold booted in both games. Measurements run from touch delivery until the selector/tab appears in lower BG2 VRAM, and separately until the selected name changes in the upper header. The 0.2.0 White 2 baseline took 33–37 emulated frames for icon selection and 35–40 for folder changes. With 0.2.1, both games measured 1–2 frames for icon feedback and 2–4 for folder feedback. Upper-name updates took 3–5 frames for icons and 4–7 for folders. Preview rendering is deliberately deferred behind input feedback; these results do not claim every detail is redrawn in one frame.

Native gameplay checks covered all seven methods, both icon rows, remembered selection, endpoints, 1% chances, A paging, rapid touch inputs, the final coalesced preview, exit while a preview was pending, and four entries/exits through B, red X and blue return. The 1,328-byte party record stayed unchanged. Both screens were inspected at 256×192; the design, native icon colors, grid and footer remain intact. New ROMs and save copies use 0.2.1 filenames; existing save hashes were checked separately.

Automated checks passed: 177 focused Vitest tests, production build, both compiled Pokenav profiles, ordinary party header and Custom UI regressions, and both ROM installers. The compiled raster checks compare incremental lower-screen updates byte-for-byte with full redraws, compare native icon/glyph pixels, exercise both optional-cache and required-surface allocation failures, and verify teardown. A separate decoder test matches 4,516 ROM text entries and covers compressed/Unicode text, truncation, canaries, malformed bounds and multi-block fallback. Installer checks cover atomic failure, updates, private text, dependency roles, disable/re-enable and edited encounter export/reopen with both companion installation orders.

The native benchmark can be reproduced with `benchmark.py` as described in [README.md](README.md). Local run logs and screenshots are under `runtime/pokenav/build/perf-check/`. CPU reconstruction, installer results and native gameplay captures are separate evidence. The historical records below retain their original version scope.

## 0.2.0 automated checks

- 177 focused Vitest tests passed across eight Pokenav, Learnset, BW1 Learnset, C-Gear, Custom UI and Code Injection editor suites. Production TypeScript/Vite build passed with the existing large-chunk advisory.
- Both compiled ARM profiles passed launch, encounter/navigation, ABI, allocation-failure and cleanup checks. The explicit information context passed 1%, 30%, 100%, and empty previews separately.
- Both ordinary party information headers and Custom UI renderers passed their separate compiled regressions, including native type actors, long names, animation, visibility controls and cleanup.
- Both compiled raster renderers checked the full 17-icon layout against native pixels and all three icon palette banks, folder touch controls, the scrolling mask, selector clearance below the grass underline, native footer artwork, and 1%/100% styled badges. Session buffers remained allocated after the upload task stopped, through the simulated native cleanup boundary. Forced failure of the 123,408-byte surface allocation produced a dismissible fallback and freed its other allocations.
- The two ROM installers passed atomic failure, reinstall, disable/re-enable, private text preservation, dependency removal guards and edited encounter export/reopen with companions installed both before and after Pokenav.

## 0.2.0 observed emulator evidence

The separately exported Black 2 and White 2 candidates cold booted copied battery saves at Route 19 in native melonDS. Existing user saves were not opened for writing. Route 19 encounter record 128 contains all seven populated methods in all four seasons: 12 distinct slots in each land table and five in each water table, 56 distinct species overall. The last Rod Special slot is Eevee to exercise evolution text paging. Export/reopen checks compared the edited encounter bytes.

- Both games displayed the matching angled-panel design at native 256×192 resolution on both screens: area-only title, scrolling grid, four water flaps, three land flaps, five water icons and twelve land icons, and the native gradient/X/return footer. The grass selection outline stays below its green underline and outside opaque icon pixels.
- Touch and D-pad checks traversed all seven methods, nearest-column movement, row and method endpoints, and remembered selections. Runtime species/form/chance contexts were asserted against each selection, including the 1% land slot and Eevee's 1% water slot. A advanced Eevee's evolution text from page 1/7 to 2/7.
- Four consecutive openings and exits per game used B, the red X, the blue return icon, then B again. Defaults reset on reopening, BG2 controls restored after exit, and C-Gear resumed. The 1,328-byte party record remained identical throughout each run.

The 0.2.0 harness, screenshots, and traces are local build outputs under `runtime/pokenav/build/style-check/`. Raster CPU reconstructions under `runtime/pokenav/build/` are separate from these native gameplay captures. The historical 0.1.1 evidence below records additional cases and must not be interpreted as a replay of all those flows with the new theme.

## Historical 0.1.1 evidence

The following checks used Learnset 1.6.1 and C-Gear Quick Actions 0.4.0 before the dual-screen theme.

## 0.1.1 opening-crash regression

The supplied 0.1.0 Route 19 crash capture showed the native window constructor returning null at window index 8. Cold booting a copy of the supplied ordinary Route 19 save in melonDS reproduced the null window and ARM9 data abort. Pokenav had added a full-screen bitmap while retaining the unused tutor windows, requiring 1,345 characters from a 1,024-character lower BG pool.

Version 0.1.1 keeps the native window lifecycle but reduces unused lower windows to off-screen 1×1 allocations. The resulting pool use is 777 characters. A new compiled regression executes both games' complete retail 16-window template tables through the Pokenav constructor with the native tile limit enforced. The repaired White 2 ROM cold boots the copied Route 19 save, creates all 16 windows successfully, and passes three consecutive native melonDS entries/exits. The source save and crash capture are retained unchanged; the updated test ROM/save use new 0.1.1 filenames.

## Automated checks

- 155 focused Vitest tests passed across the Pokenav, C-Gear Buttons, Quick Actions, Learnset, Custom UI, installation, and Code Injection editor suites.
- Production TypeScript/Vite build passed. Vite reported its existing large-chunk advisory.
- Host encounter tests passed: first-slot grouping order, forms, exact slot-weight sums, 1% and 100%, maximum counts, unavailable defaults, skipped methods, endpoints, partial rows, nearest-column movement, and empty data.
- Both compiled ARM profiles passed launch ABI rejection, native field/application ownership, missing companion handling, method input, empty data, Back, allocation failure, and teardown. The private initialization shell also passed the pinned native PK5 decrypt-flag routine without modifying its bytes.
- Both compiled information renderers passed explicit 1%/30%/100% headers and clearing an empty preview, including hidden type badges. Palette checks cover the native NCLR's trailing index chunk, all three sub-screen BG banks, and restoring those banks after the opening fade. Existing party information/wrapper and C-Gear runtime suites passed separately.
- Both ROM installers passed reinstall, disable/re-enable, atomic rollback on a failed companion fetch, shared dependency removal guards, private text/ID preservation, and edited encounter export/reopen. Custom UI and the enhanced party-menu companion were installed both before and after Pokenav in each game. Pokenav alone left the party LEARNSET command disabled; an existing enabled command stayed enabled.

Reproduce the main checks with the commands in [README.md](README.md). Run the coexistence cases with `npm run pokenav:verify-rom -- INPUT.nds --companions-first` and `--companions-last`.

## Observed emulator evidence

Cold boots used the bundled fixture save copied into emulator memory, with separately exported test ROMs. Input ROMs and existing user saves were not written.

- Both games opened the screen through NAV, accepted touch and D-pad input, and returned to C-Gear. A constructed encounter fixture exercised all seven methods, five water icons, twelve land icons, duplicate grouping, a distinct Basculin form, and 1%/100% chances. Runtime species/form/percentage values were asserted against the displayed information context. The initial land icon's opaque rendered pixels were also compared with colors and tiles obtained through each ROM's native icon routines.
- Three consecutive sessions exercised B and touch Back, endpoint behavior, and resetting defaults on reopen. The instrumented 1,328-byte party record stayed identical throughout browsing and after exit. The Black 2 fixture also opened with an empty party and the enhanced party-menu companion installed.
- White 2 was checked with wireless on and off. Separate native map boots covered Aspertia City and Route 19. The Route 19 edited encounter tables were read through the native active-season buffer.
- In both games, an edited no-encounter map opened twice and closed through B and touch Back. Assertions confirmed its launch snapshot contained no tables. A separate retail-table case moved from water to an empty land header and back, asserting that the empty preview removed its species and type actors.
- Both screens were inspected at native 256×192 resolution. The full-grid capture uses deliberately edited encounter data; it is not a claim about retail Aspertia City's encounter distribution. A native party-menu opening/return was also observed separately.

The emulator harness writes captures and traces under `runtime/cgear-quick-actions/build/emulator-*/`. Its `pokenav` scenario exercises the full table fixture; `pokenavEmpty` with `--pokenav-empty-map` checks empty maps; `pokenavPartial` checks the empty-section preview transition. Run `pokenav:verify` before using `--pokenav-fixture`: it generates the ROM-derived color/tile references for the pixel comparison. `--write-rom NEW.nds` creates a separate ROM and matching fixture save only after a successful run, refusing existing output paths.

## Limits of the evidence

Allocation failures were injected in compiled CPU checks, not forced in live emulator heaps. Seasonal selection is verified against the native loader signatures and active seasonal snapshot; changing the calendar through all four seasons was not observed in gameplay. Separate map boots do not establish an in-session walking transition. Native tutor dispatch and Custom UI have automated regression coverage, but their complete gameplay flows were not replayed here. Party bytes were compared; the entire running save structure was not compared because native time/state maintenance continues.

No physical DS/DSi acceptance is claimed. Static encounters, Hidden Grottoes, encounter searching, seasonal browsing, and additional detail fields remain outside V1.

## Pokenav 0.3.0 integration update

Learnset 1.8.0 and C-Gear Quick Actions 0.5.0 add the optional Nuzlocke launch snapshot and gray icons. Rod base weights are corrected to 40/40/15/4/1; Surf remains 60/30/5/4/1. Legacy version-1 requests remain readable. Current compiled palette/lifetime/navigation checks passed in both games, and the installer page is covered by the 135-test focused suite. New cold-boot caught-family integration evidence and its exact limits are recorded in [Nuzlocke validation](../nuzlocke-encounters/VALIDATION.md). Earlier gameplay claims above apply to their recorded builds.
