# Map-transition Autosave MVP 0.1.1-test

Experimental US revision-0 Black 2 (`IREO`) and White 2 (`IRDO`) modules. These are testing artifacts, separate from the bundled Pokeweb installer catalog.

## Behavior

- Door/warp transitions (including same-header warps) and seamless area-header changes queue one save.
- Wait for native arrival events, fades, and a stationary/tile-complete player. Native scripts, encounters, communication, and menu events retain priority.
- Show a compact native **Saving...** message. Close it automatically when the native writer finishes. A failed write shows **Save failed.** for four seconds.
- Use the native live-data snapshot and asynchronous save writer, including its backup/checksum, wireless-scan, sound-loading and soft-reset handling.
- Initial Continue, ordinary Bag/Party returns, and transitions without an available native Save menu do not save. Consecutive scripted transitions before neutral control coalesce into the latest destination.
- Advance the unchanged native save routine up to **eight times per field tick**, stopping immediately on success or failure. Native pending-write checks remain active. No interval setting, visibility flag, altered save format, or changed flash protocol is included.

## Implementation

Three field-overlay 36 call hooks wrap map-arrival initialization, event-check registration and the existing seamless-zone update. The native event callback runs first. An ARM9 hook wraps save-data snapshot preparation, preserves normal/manual saving, consumes pending requests on manual saves, and keeps the module resident across field-overlay reconstruction.

The event owns its work, string and window until native printing/writing completes. Heap capacity is checked before entering native constructors. Low memory skips that transition's save. Original menu resources are released by their native destructors. No timer task or separate per-frame map scan is installed.

`build.py` checks complete source-binary hashes, all four hook call targets, independent per-game native signatures, current-location offsets and the native menu types containing Save. `manifest.json` is generated from these exact sources. Unknown language/revision, changed native code, hook conflicts and unrecognized same-name DLLs are rejected by the local installer before export. The exact recognized 0.1.0-test module can be upgraded in place; other modules and source saves remain untouched.

## Build and test

Run from the project root:

```sh
python3 runtime/autosave/build.py
python3 runtime/autosave/verify.py
python3 runtime/autosave/emulator_matrix.py
npx vitest run src/test/mapAutosave.test.ts src/test/pmcModel.test.ts
npx vite-node runtime/autosave/emulator.ts ../cleanwhite2.nds runtime/autosave/inputs/door.json
npx vite-node runtime/autosave/emulator.ts ../cleanblack2.nds runtime/autosave/inputs/seamless.json
```

Toolchain and RPM tool paths can be supplied with `ARM_TOOLCHAIN_BIN` and `RPM_TOOL_JAR`. The build reads clean ROMs from the workspace parent. No external source tree is a build dependency.

`patch-rom.ts input.nds new-output.nds` creates a separate ROM from an existing compatible project export. `install.ts` exports the checked local installer, automatically stages PMC when needed, and stages the matching DLL. `emulator.ts` uses copied bundled save bytes and writes only under ignored `build/`. Test ROM exports are compacted while retaining header capacity and native ROM sections. `deliver.py` copies the verified standalone builds and initial fixture saves to separately named workspace outputs, using exclusive creation.

## Acceptance boundary

Compiled harnesses verify the eight-call bound, continuation across ticks, immediate termination at each possible batch step for success and failure, and both profiles, native event priority, delayed eligibility, Save-enabled/restricted menu types, repeated cleanup, allocation rejection and save failure. Installer tests cover dependencies, idempotence, conflicts, ownership and export/reopening. Emulator evidence covers door transitions, seamless transitions in both directions, repeated transitions, Party/Bag return and cold reload. Exact scenarios, the comparison against the one-step build, and pending checks are recorded in `acceptance.json`. The original one-step measurements are retained in `acceptance-v0.1.0.json`.

This MVP has not received hardware/DSi-mode acceptance, exhaustive story/communication-area coverage, interruption/power-loss testing or broad ROM-hack compatibility validation. Native backup protections remain in place; those limitations are not inferred to pass from the emulator checks.
