# BW1 Party Toolbar

US revision-0 Black (`IRBO`) and White (`IRAO`) use release `0.1.0-bw1`.
BW2 retains its existing release and upgrade recognition.

## Runtime

`party_tools.cpp` contains the shared toolbar behavior. `adapter.h` selects the
BW2 adapter or the generated BW1 adapter. `configure_bw1.py` records independent
Black/White native entries, ARM/Thumb modes, structure offsets, sounds, saved-flag
bounds and signatures. Party overlay 91 and field overlay 10 own the native
applications and their completion callbacks.

Candy uses the native medicine, move-learning and evolution routes. Request
ownership follows those applications until ordinary field Party selection is
restored. Other Party request modes retain the native menu.

BW1 status writes use the retail status bit fields. Graphics use BW1 HP/status
resources, downscaled native item icons and matching XP lettering. The private
`party-tools/ui.narc` retains configuration version 3 and six optional hide flags.
The toolbar defaults on; a blank flag imposes no restriction. Set flags hide the
whole toolbar or its Candy, Heal, HP, EXP and Status sections. Remaining buttons
pack left. The runtime reads visibility flags without changing them.

The toolbar reserves nine character uploads (4,608 bytes), three palette banks
(96 bytes), nine actors and an animation handle. Allocation and upload failures
retain the native menu and release owned resources. Native compressed Pokémon
palettes leave their size field undefined, so capacity checks use their verified
three-bank span. Installation verifies the palette resource layout before doing
so. The native Back control remains available.

## Build and verification

Set `PARTY_TOOLS_B_ROM` / `PARTY_TOOLS_W_ROM` for alternate retail input paths.
The reference inventory accepts `BW1_REFERENCE_ROOT` and reads it without editing.
The build uses the workspace ARM toolchain and RPM converter.

```
npm run infinitecandy:build-bw1
npm run partytools:build-bw1
npm run partytools:verify-bw1
npm run partytools:emulator-bw1 -- /path/to/retail.nds --scenario hp
npm run partytools:check-bw1-ds
```

The compiled harness executes both adapters and the retail PK5 routines for all
32 block orders, encrypted and plaintext states. It covers all 32 section masks,
packed touches/navigation, ownership, cancellation, allocation failures,
compressed palettes, scheduled uploads and cleanup. The dependency harness
executes retail ordinary Rare Candy subtraction while retaining item 622.

The emulator runner creates copied fixtures, records screenshots and RAM traces,
and permits candidate installation only within that process. Its in-memory
checkpoints are bound to the exact candidate ROM and DLL hashes. Uninterrupted
runs are used for native saving and cold reload. Test outputs stay in `build/`.

Normal installation is gated on acceptance of both exact Black/White bundles.
The acceptance record identifies their DLL/archive hashes and required DS runs.
A changed bundle invalidates acceptance. Hardware and live DSi acceptance are
separate and are not inferred from DS emulator results.

Other languages/revisions, new actions and a BW1 C-Gear port are outside this
release.
