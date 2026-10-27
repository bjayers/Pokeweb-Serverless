# Width-dependent follower spacing

Stock 0.6.15-alpha and White2Upgrade 0.7.6-alpha share this implementation.

## Artwork measurement

The installer and replacement importer decode the actual displayed resource,
measure opaque horizontal bounds in both side directions and both walk poses,
and use their maximum width. Transparent padding, species names and the nominal
32/64-pixel canvas size do not determine spacing. Mirrored and asymmetric profiles
use their native frame mappings; placeholders use their displayed art.

`extra = clamp(ceil((visibleWidth - 16) / 2), 0, 6)`

| Maximum opaque side width | Extra spacing |
|---|---:|
| 16 pixels or less | 0 |
| 17–18 | 1 |
| 19–20 | 2 |
| 21–22 | 3 |
| 23–24 | 4 |
| 25–26 | 5 |
| 27 or more | 6 |

The gap stays constant across idle and walk frames. These are native world
units, approximately pixels at the ordinary camera scale, not a guaranteed
screen-space pixel distance under camera rotation/zoom. East/west movement is
the widened axis; north/south distance remains one tile.

## Movement and interaction

The existing bounded trail uses the metric
`sqrt((16 * dx)^2 + ((16 + extra) * dz)^2)` with target
`16 * (16 + extra)` world units. Straight horizontal travel follows 16+extra
units behind; vertical travel follows 16. Mixed routes consume the same metric
through recorded positions, rather than switching a target distance at corners.
No render offset, depth bias or collision-coordinate divergence is introduced.
Recorded frame granularity can add less than one movement sample of separation.
Generation, rail-connectivity, overflow, terrain and NPC guards still apply.

Conversation reach admits the additional horizontal spacing, retaining native
adjacent-tile, elevation, obstruction and trail-corridor checks. North/south reach
does not increase. Existing render corrections, menu/PC retention and seamless
zone handling are retained. A changed appearance uses normal trail reseeding.

### Side-by-side stand

An HGSS Easter egg, 32-pixel followers only. Walk down any number of tiles,
step one tile left or right, then turn to face down without moving. The
follower is still above the corner tile by its gap; it takes the rest of the
step onto the corner, two units an update, and stands beside the player
facing down. `fw_trail_side_corner` reads that sequence from the trail. When
the player moves again the follower holds until the trail pose has come down
through the corner, so it never steps back up. Any other facing or path, a
swap or ease in progress, rails, the ball, a 64-pixel follower, an effect, or
an NPC on the corner leave the ordinary pose.

## Storage and compatibility

Stock White 2 0.6.89 widens the dialogue corridor sideways from one-third to
one-half of a tile (eight world units), independently of both actors' facings
and rendered sprite widths. A recent turn can leave a perpendicular-facing
follower offset within its adjacent tile. The supplied Purrloin state has a
16-unit forward and six-unit sideways separation: the previous corridor
rejected it although the native target tile and obstruction query allow it.
The same half-tile limit applies to every retained trail sample, including
rotated rail tangents. Forward separation limits, target-grid admission,
obstructions, generation and height checks remain. Neither walking offsets
nor the existing bounded pose clamp changes.

Full FWDB version 3 uses reserved byte 15 of each unchanged 24-byte row. Compact
version 4 uses bits 3–5 of byte 8 in each unchanged 12-byte row. Values above six
are rejected. Legacy versions 1/2 remain readable with zero added spacing; older
runtime validators reject the new versions. Modules and registry are staged
together. Upgrades recompute spacing from installed artwork while preserving
resources, descriptors and hashed zone policies.

On ARM32, the selected gap occupies a previously unused byte at follower offset
50; the trail remains at 52. The 64-record trail makes the current sidecar 1,852
bytes. Static assertions protect both offsets and size. The ROM page cache stays 1,024 bytes, the species
index stays 1,302/2,050 bytes, and conversation capacity stays 8,192 bytes. No new
heap allocation or per-frame artwork decoding/file I/O is added. Code size can
increase; see the generated [memory audit](MEMORY-AUDIT.md).

## Verification

Host tests cover every gap, direction, walk/run speed, repeated bends/reversals,
elevation, invalid input and history exhaustion. Packaged ARM946 tests cover
spacing and unobstructed/blocked conversations using mocked native services.
Registry checks cover each installed appearance, fallback selection, malformed
spacing, cache bounds and allocation-free reads. Import tests cover transparent
padding, maximum pose width, mirroring, asymmetric sides and legacy formats.
Executed results are recorded in [VALIDATION.md](VALIDATION.md).

The current stock side-on artwork scan is reproducible without an emulator:

```sh
npx vite-node scripts/verify-following-dialogue-assets.ts /path/to/White2-Following-0.6.89-alpha.nds runtime/following-pokemon/build/dialogue-assets.json
python3 runtime/following-pokemon/verify_dialogue_assets.py runtime/following-pokemon/build/dialogue-assets.json runtime/following-pokemon/build/dialogue-assets-cpu.json
```

For a base ROM, set `FOLLOWING_BUILD_DIR=runtime/following-pokemon/build/base`
and `FOLLOWING_MODULE_SUFFIX=W2Base` on the Python command. The scan binds its
catalog to the installed field DLL hash and includes all forms, genders and
shiny appearances, not just the narrowest artwork.

Human acceptance is pending: run W01–W05 in [EMULATOR-CHECKLIST.md](EMULATOR-CHECKLIST.md)
on both profiles, including large sprites on stairs and near buildings, and
U16 for later generations. Cold boot with the matching ordinary save; older
emulator states contain older code and data. No game emulator or hardware testing
is performed by the build scripts.

## 64-pixel followers

A 64-pixel body (descriptor byte 7 = 2) is about twice an NPC's width, so at
an authored or automatic gap its edge sits on the player. The runtime adds
`FW_LARGE_GAP_BONUS` (8 world units) to every side for those actors, on top
of the selected positioning row: `walk_gaps()` in `field.c` for the trail,
swap ease and reseed, and `fwt_reach` in `interaction.c` for talking.

The stored gap stays in 0–12 (`FW_MAX_AUTHORED_GAP`), which is what the
registry, `positioning.narc` and the Pokeweb editor validate, so editing a
row still tunes a large follower without a runtime reinstall. The effective
gap reaches 20 (`FW_MAX_DIRECTIONAL_GAP`), 36 units from the player at rest.

Dialogue reach already scales with the gap. The two-tile corridors in
`fw_clamp_dialogue_pose` and the talk trail check widen only by the gap past
12 (`FW_GAP_BEYOND`), so 32-pixel followers keep exactly the bounds they had
and a large one keeps the same three units of partial-step room past reach.
The native grid check admits the tile in front of the player and the next
one, which covers 40 units. Reseeds lay 48 units of history.

## Where 64-pixel followers come out

A 64-pixel follower stays in its ball in an interior and comes out in an
exterior. The area's exterior flag decides (`FieldEffects_GetAreaIsExterior`,
the same flag seasons use). A zone row in `positioning.narc` member 1 swaps the
rule for that zone: a large interior that lets it out, or a cramped exterior or
cave that keeps it in.

- Doors keep the existing recall before the fade. Inside, the
  field update builds the actor hidden; `move()` keeps it hidden.
- L/R where a large follower is barred skips large party members other than
  the lead, and works while the large one is in its ball. With no small one to bring out, the
  closed ball shows where the follower stands, shakes a pixel either way, and
  plays `SEQ_SE_BEEP` (1367); `fwfx_cue`, about a second.
- The selection is the player's: a large follower left in its ball comes back
  out at the next exterior; a small one chosen inside stays until switched.
- Outdoors, a walkable layer 1.5–3.5 tiles above the follower's feet (a bridge
  or upper walkway its 56-unit sprite would come through) also keeps it in its
  ball, until the trail has been clear for 20 updates. The trail point is
  probed and so is a tile either way along the line toward the player, so it
  goes in a tile before it walks under and comes out a tile after. Three
  terrain queries per trail tile and height (`FieldG3DMapper_GetTerrainAll`).
  While it is held there, the closed ball stays out on the follower's hidden
  pose and follows the trail, hopping a pixel or two per step and landing
  when the player stops (`fwfx_rest_at`; not an effect mode, so it blocks
  nothing). The recall into it does not stop the player, and its shrink
  travels with the ball. Release sends out from the ball.
- The ball keeps the tile right behind the player: every gap is walked down
  to 0, two units per update the player moves, and back out one unit per
  update after the send-out (`fw_trail_remeasure` re-sums the trail on each
  change). It swaps places with the player like the follower.
- Wandering NPCs treat the ball as a one-tile object, as they do the
  follower. A forced move through it, or the failsafe for an NPC already on
  its tile, takes it away with the ball sound, and it returns with the trail.
  A talk leaves it in place.
- Indoors the ball shows too, appearing once the trail is ready, with no
  send-out. Under a floor L/R is locked with the refusal cue. Indoors L/R
  still switches: small party members come out, other large ones are
  skipped, and the lead (the member a fresh map brings out) comes back as
  its ball with one bounce.

32-pixel followers are not affected.

### Zone override member

`positioning.narc` as installed has one member, the positioning table, and
reads exactly as before. An editor may append member 1 (at most four members
in all). The reader walks the NARC's FAT, so FNTB may be any 4-byte multiple
from 8 to 256 bytes.

| Offset | Size | Value |
|---|---:|---|
| 0 | 4 | `FWZO` (`46 57 5A 4F`) |
| 4 | 2 | version, 1 |
| 6 | 2 | row count, at most 256 |
| 8 + 4n | 2 | zone ID |
| 10 + 4n | 1 | flags; bit 0 swaps the large-follower rule. Other bits are reserved and ignored. |
| 11 + 4n | 1 | 0 |

The member may be longer than `8 + 4 × count` (alignment padding). A bad
magic, version, count or reserved byte leaves no overrides; positioning still
reads. Rows load once per field load (`fwp_zones_load`).
