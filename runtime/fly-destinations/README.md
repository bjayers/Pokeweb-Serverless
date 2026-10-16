# BW2 Fly destination editor

Open **Fly Destinations** from the navigation bar or the Headers editor. Select an existing route or area, enable or disable Fly, choose its visit flag, and edit its landing position. Apply the destination, then export the ROM. Project saves and imported exported ROMs preserve the settings.

The destination preview shares the script editor's terrain and event renderer. A cyan crosshair follows draft grid coordinates; drag to pan, scroll or use the buttons to zoom, and choose **Center landing** to focus on the tile. Route 4 and Join Avenue preview their own landing coordinates on the resolved destination map. The preview uses spring terrain and does not validate height or collision. Rail destinations show terrain and their coordinate values, but their landing point cannot yet be plotted. If terrain rendering fails, the preview shows the permission map when available.

This runtime supports the audited retail US Black 2 (`IREO`) and White 2 (`IRDO`) layouts. The installer checks the native constructor, spawn provider, hook bytes, and overlapping installed DLLs before changing a project. BW1, other languages, relocated functions, and conflicting patches need separate bindings and verification.

The Town Map retains its existing 85 records. The editor changes the Fly setting and visit flag in `a/0/8/5`, member 0. Saved visit flags range from 1 to 3059; the three existing Town Map conditions 61440–61442 are also accepted. Flag 65535 means no visit flag and cannot unlock an enabled destination. The area flag shortcut uses existing discovery flags; a custom flag must be set by the user's scripts. Visibility conditions remain independent.

Landing overrides are stored in the bundled Fly DLL configuration, keyed by selected area ID. They affect only Fly. Map-header default spawn fields remain unchanged, preserving their use by Teleport and other transitions. Grid positions use matrix coordinates and signed 16-bit grid bounds; rail destinations expose component, front position and side offset instead.

The first edit pins Route 4 and Join Avenue separately. The Town Map's hardcoded Join Avenue ID replacement is bypassed, and the Fly spawn hook resolves selection 490 to landing map 326 while choosing the override for selection 490. Route 4 selects 326 and uses its own override. White 2's existing map-version logic still resolves Route 4 to map 551. Town Map visuals, marker types, and hit regions stay unchanged: newly enabled routes use their existing selection regions without added destination dots.

Runtime 1.0.1 removes the earlier marker override. Existing 1.0.0 settings can still be loaded; apply any destination and export to replace the old DLL while retaining all configured landing positions and visit flags.

## Build and checks

```sh
python3 runtime/fly-destinations/build.py
python3 runtime/fly-destinations/verify.py
npx vitest run src/test/flyDestinationModel.test.ts
npx vite-node scripts/verify-fly-destinations.ts /path/to/owned-source.nds /separate/new-output.nds
npm run build
```

The ARM946 check executes compiled hooks with controlled native spawn calls. It covers independent arrivals, fallback positions, rail layout, and preserved surrounding work bytes. The build verifies that only the spawn and Join Avenue ID hooks are installed. The ROM check covers installation, repeated edits, project save, export and reimport, preserving map headers, raw overlays, Town Map visuals and hit regions. These checks do not establish landing collision safety or full in-game animation behavior. Choose walkable landing coordinates and check them in an emulator before release.
