# Nuzlocke Style Encounters

Version **0.1.0** filters caught evolution families from native wild encounter selection in English US revision-0 Black 2 and White 2. Compatible edited ROMs must retain the verified native encounter/save interfaces. Black and White are not supported.

Open **Code Injection → Feature Add-Ons → Nuzlocke Style Encounters**, immediately after Pokenav. Install, export a new ROM, and cold boot with an ordinary battery save. Pokenav is optional. Installing this feature alone does not install C-Gear or enable the party LEARNSET command.

A family is excluded if any member has ever been registered as caught in this save's Pokédex. Released, traded-away, boxed, and lost Pokémon still count. Merely seen species do not count. Family connections and forms come from the exported ROM's personal and evolution archives, including edited branches and cycles. No separate species database or save format is introduced.

Remaining slots keep their relative native probability. For example, 50% Ivysaur and 50% Charizard becomes 100% Charizard after Bulbasaur has been caught. **When every slot is a dupe, the default is to prevent that encounter.** An optional installer setting allows the original distribution instead. Disable restores native selection at the next cold boot without altering encounter tables or caught records.

Pokenav 0.3.0 keeps dupe icons visible in gray and allows their information to be selected. Their chance is 0%; eligible species show the normalized base chance for the selected method. Whole percentages are rounded, so displayed totals can differ slightly from 100. With the optional original-distribution fallback, an entirely excluded table retains its original percentages. With the default prevention setting, every entry is 0%.

The seven land/water methods and native uniform swarm slot selection are covered. Native level/form generation, double encounters, type-attracting abilities, encounter powers, Repel, and battle creation remain in their existing native paths. Type-attracting selection cannot reintroduce an excluded family. Pokenav reports base method chances before those modifiers. Static encounters, roaming Pokémon, Hidden Grottoes, gifts, and scripted battles are outside this patch.

## Runtime and installation

The standalone DLL patches six verified overlay-36 selection sites. It has no dynamic companion imports. A bounded union-find graph uses 8,200 bytes of static state; a temporary reader uses approximately 5 KiB and closes every file before returning. Personal/evolution archives are capped at 2 MiB and 4,096 records, with at most 32 evolution edges per member. The encounter species field supports IDs below 2,048. Malformed or unavailable input, missing configuration/module, or allocation failure falls back to native selection.

Caught flags are queried through the native save interface on every generated encounter and Pokenav entry. Caught status refreshes while remaining in the same map. The graph and configuration are cached until a cold boot. The private versioned configuration is `nuzlocke/encounters.narc`; disabling retains the DLL and editable settings.

Installation stages PMC, the DLL, configuration, and any installed Pokenav companion updates atomically. It preserves private text IDs, party LEARNSET enablement, Custom UI settings, C-Gear button designs, and disabled companion settings. Pokenav integration uses a version-2 bounded launch snapshot; shared viewers still accept the original version-1 request. BW1 companions are unchanged.

## Checks

```sh
npm run nuzlocke:build
npm run nuzlocke:verify
npm run nuzlocke:verify-rom -- /path/to/clean-white2.nds
npm run nuzlocke:verify-rom -- /path/to/clean-black2.nds --nav-first
npm run pokenav:verify
npm run build
```

`verify.py` executes compiled ARM code and the original native selection functions. `test_logic.cpp` exhaustively checks all 4,096 grass exclusion masks. `verify_emulator.py` cold boots copies of the Route 19 test fixtures and changes caught flags only in temporary emulator RAM. Installer checks export/reopen edited encounter and evolution records, exercise both installation orders, and inject download failures. These evidence classes are recorded separately in [VALIDATION.md](VALIDATION.md). User saves and source ROMs are never overwritten.
