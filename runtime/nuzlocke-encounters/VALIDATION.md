# Nuzlocke Style Encounters 0.1.0 validation

Recorded 2026-10-16 for English US revision-0 White 2 and Black 2. The optional integration uses Pokenav 0.3.0, Learnset companions 1.8.0, and C-Gear Quick Actions 0.5.0. Compatible edited ROMs must preserve the pinned native interfaces; this is not acceptance of every Upgrade derivative or physical DS/DSi hardware.

## Automated checks

- 135 focused Vitest tests passed across Nuzlocke, Pokenav, Learnset, C-Gear buttons/actions, and the installer page. TypeScript and the production build passed. Vite reports its existing large-chunk advisory.
- Host C++ exhaustively checked all 4,096 grass exclusion masks against each surviving slot's weight. Family branches, cycles, uniform selection, endpoints, and configuration checks passed.
- Compiled ARM filters ran against each game's actual overlay-36 selection functions and native caught-bit lookup. Checks passed for the 50% Ivysaur/50% Charizard example, grass, Surf, fishing, all four native encounter-power weight variants, uniform swarms, and type attraction without dupe reintroduction. Caught status was refreshed without rebuilding the family graph. All-dupe prevention and optional original-table fallback passed.
- Families and forms were read from ROM archives. Eevee's branches, Rotom's alternate forms, changed evolution edges, missing module/configuration, both supported NARC header encodings, malformed input, allocation failure, closed handles, freed allocations, and unchanged Dex bytes passed.
- Both installer orders passed against independent clean fixtures: Nuzlocke first in White 2, Pokenav first in Black 2. Standalone installation did not install C-Gear or enable LEARNSET. Injected feature-download and shared-viewer failures left project state unchanged. Reinstall, disable/re-enable, exported status recognition, private text/IDs, Custom UI, button designs, disabled companion settings, and edited encounter/evolution export/reopen passed.
- Compiled Pokenav checks passed for legacy and current requests, gray colors from all three native icon palettes, all-dupe 0%, mixed 0%/100%, corrected Rod weights, selectable dupes, and existing grouping/navigation/layout/lifetime behavior. Native font/text checks matched 4,516 ROM entries. Shared Learnset and C-Gear CPU regression harnesses passed in both games; these include party navigation, tutor read-only guards, private text, native call preservation, saved options, button input/ownership, native map/save requests, allocation failure, skins, and Pokéradar behavior.

## Observed emulator evidence

Both games cold booted separately exported Route 19 fixtures using copies of their existing battery saves in melonDS. Caught flags were changed only in temporary emulator RAM to create deterministic fixtures. These are not observations of an actual capture, release, or save-write sequence.

With only Bulbasaur registered caught, Pokenav showed Bulbasaur, Ivysaur, and Venusaur in gray at 0%. Selecting Charmander showed 20% after the excluded 50% family mass was removed. Shoulder navigation and remembered Grass selection worked. B returned to the field, repeated entry worked, and party memory remained unchanged.

Ordinary movement triggered an eligible native grass encounter after the Bulbasaur family was excluded. The live output recorded a valid 12-slot Grass method and an eligible species; its battle introduction was inspected. After restoring the temporary field checkpoint and marking every species caught in RAM, Pokenav showed all icons gray at 0%. Ordinary grass movement then produced 12 native generation attempts returning no encounter in each game, and the field remained playable. Native EFP/output buffers were read from ARM9 DTCM, rather than the mirrored memory bus, when recording those calls.

Live evidence covers Grass and Pokenav; the other methods, abilities, forms, edited family graphs, and failure cases are established by the compiled/installer checks above. A full battle, actual capture followed by a second encounter, native tutors, and long-session hardware play were not acceptance-tested for this patch.

Local logs and native-resolution captures are in the ignored `build/` directory. `verify_emulator.py` reproduces the checks using separately copied fixtures; it refuses an existing output directory.

## Test delivery and save preservation

The separately named `Nuzlocke-Style-Encounters-v0.1.0-Black2-test.nds` and `Nuzlocke-Style-Encounters-v0.1.0-White2-test.nds` retain the previous 56-distinct-slot Route 19 encounter fixture. Each has a separately copied battery save. Existing Pokenav ROMs and user saves were not overwritten.

| File | SHA-256 |
|---|---|
| Black 2 test ROM | `107260ac2f9d9dd3f7145f2ae03ba92525897882502eeedc905a1e8300fa3cea` |
| White 2 test ROM | `97d2fb0905f8421e3b20d3ab0fdd9920d04d4deb04e637523eb41af3b16b35fe` |
| Black 2 original/copied save | `bfde30239cf2b7858db1e212cce78fd1a357774634d50a737d3cb83c06fe90ed` |
| White 2 original/copied save | `61c9134a458b9cf1b77a46b1a3c0835d5d0249e98de5f226edc030fd43bf5699` |
