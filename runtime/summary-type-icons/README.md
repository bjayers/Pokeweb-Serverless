# Summary Type Icons

PMC companions for English US Black 2 (`IREO`) and White 2 (`IRDO`), revision 0.
Install through **Code Injection → Graphical/UI Enhancements → Summary Type Icons**,
then export the ROM. PMC is installed automatically. The Summary IV/EV Viewer
can be installed independently or alongside this patch.
The installer card appears only when Pokeweb runs on localhost or a loopback address.

The information page's Type row uses decorated 32×16 badges for the 17 original
types based on the supplied artwork. Stats, move lists, move details and every
other screen continue to use the original type icons.
Each main body color is anchored to its retail RGB555 color. A common hue
rotation and relative saturation/brightness adjustment recolor the particles
and details; white lettering and neutral shadows remain neutral. Pixel-interior
sampling cleans the filtered attachment, followed by deterministic palette
reduction. Each badge uses at most 15 visible colors plus transparent index zero.
Fairy, Stellar, and both question-mark badges are excluded.

The information page displays **Type → Name → Dex No.**, followed by the native
OT, ID and experience rows. The original window identities and text routines
remain in place; their row positions and the two type-actor positions change.

## Rendering and ownership

The native main-engine OBJ palettes are already assigned to footer, marks,
Poké Balls, statuses, move plates and fonts. The patch reserves otherwise unused
VRAM G for extended main OBJ palettes while leaving texture palettes in VRAM F.
Its native graphics configuration retains every other bank assignment.

Badge characters travel as 8bpp OBJ tiles, with only indices 0–15 used. This
permits extended palettes alongside the native 4bpp sprites. Two separately
owned actors, character buffers and palette slots serve the information page.
Its native type actors are hidden while their replacements are shown; their
original characters, palette and animation state are retained for other pages.
Information-page hide calls hide the replacements on page exit. Changed tiles
and palettes upload during the native Summary VBlank callback. Every original
type-character resource and move/category drawing call is retained.

Summary exit destroys the two actors, unit, character buffers, palette and cell
bank before native teardown, then disables extended OBJ palettes. Archive-open
and allocation failures unwind owned resources and retain native icons.

The custom archive is `summary-type-icons/ui.narc`: palette, cell, animation,
then 17 character resources in native type order. The common graphics archive
and other menus' type icons are not edited. The module uses 1,192 bytes of BSS
and its DLL is 7,920 bytes per game.
Installer checks include exact retail hook/helper bytes, VRAM ownership, module
fingerprints, archive hashes, duplicate modules and overlapping relocations.
Staging is atomic; source ROMs and user saves are not written.

## Build and verification

```sh
python3 runtime/summary-type-icons/build.py
python3 runtime/summary-type-icons/verify.py
npx vitest run src/test/summaryTypeIconsModel.test.ts src/test/summaryStatViewerModel.test.ts src/test/codeInjectionEditor.test.ts
npx vite-node scripts/verify-summary-type-icons-install.ts ../cleanwhite2.nds
npx vite-node scripts/verify-summary-type-icons-install.ts ../cleanblack2.nds
npx vite-node runtime/summary-type-icons/emulator.ts ../cleanwhite2.nds
npx vite-node runtime/summary-type-icons/emulator.ts ../cleanblack2.nds
SUMMARY_ICON_FRAMES=1900 npx vite-node runtime/summary-type-icons/emulator.ts ../cleanwhite2.nds --native-types
python3 runtime/summary-type-icons/verify_render.py
```

Build inputs are the two local clean ROMs, the ARM GNU toolchain and RPMTool.
Asset conversion additionally uses NumPy and Pillow. Compiled CPU checks use
the existing Unicorn/pyelftools runtime under `runtime/learnset-viewer/build/python`.
The emulator harness uses the bundled fixture save in memory and a C-Gear party
launcher solely to enter the native Summary; that launcher is not a dependency
of the delivered patch. Rendering and traces are saved under ignored `build/`.
See [VALIDATION.md](VALIDATION.md) for the performed checks and their limits.
