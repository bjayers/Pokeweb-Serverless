# Follower render: world feet

The draw submits the native billboard plus one registry Y, on every facing. It does not plant bob-down, does not add the north-only two pixels, and does not slide the body or the oval. The trail, swap, ease, rail, recall, and talk code are unchanged. The player quad is not written.

## What the frame does now

The follower actor's world pose is the placement. Trail, swap, ease, and rails write that pose the way they write an NPC. `fwr_draw` does not move the player.

Submitted follower quad:

- Start from the native billboard, which already includes the world pose and the idle hop.
- Apply the registry Y (`sprite_y` minus descriptor byte 14) on this quad only. Every facing uses that same offset. Nothing adds Z.
- Copy the player's light-mask bits onto the follower for the submit, then restore the follower quad. The player billboard, world, moveflags, and animation are not written.

`policy` and `sortMargin` stay 0. `before` / `after` / `depthRel` are the submitted view gap, hop included. `hold` is always 0. `snap` is always 0. The debug record layout is unchanged.

Shadow pass (`fwr_shadow_pass`):

- The hook still calls `0x0204f685`. It does not search the oval, plant it, or slide it. The engine sample is the oval that draws.

Install grounding (`deriveFollowerGrounding`) sets non-Flying automatic Y to 0, the same value as descriptor byte 14 and a map NPC. A stored `-pad` or `-(pad)-3` is rewritten to 0. Explicit offsets stay. Flying rows are left untouched. Descriptor byte 14 is not written, so the oval's actor anchor does not take a registry Y.

`fwr_world_apart`, `fwr_plane_keep`, and `fwr_shadow_on_plane` stay in the math helpers. The draw does not call them. `fwr_player_in_front` is gone. The older ray helpers remain for the emote bubble, grass, and the full-variant land/surf draws, including the mounted north offset. The base draw path does not call them. Base does not grow `FW_MOUNT`.

The prop walk that used to push quads behind furniture is gone with the ray advance it was capping. A follower is no longer driven through a stair wall to win a Z test, and the player is no longer moved to clear a trashcan.

## Why the ray stack lost

Gen 5 sorts a billboard by the position it submits. The old path edited the follower, and then the player, along the eye ray so the picture would stay put while the depth changed, then wrote the bytes back after `0x0204f685`. NPCs in that submit were compared to the shifted player. On flat ground `643cea36` forced the follower's ground room to 0, so the whole gap was the player advance. That is enough for the player to draw over nearby Castelia NPCs. It is also the only write that can make the player quad hop while the follower is clamped still. Restoring RAM after the call does not undo the frame.

A downward camera turns a backward ray into a drop in world Y. Capping that, then assigning the oval's Y back to the feet, fought the same ray and put the oval over the lower body on wide sprites and oblique tiles.

Those bugs are structurally gone: there is no player write, and the oval never leaves the ground plane.

## Behaviors left on the existing owners

- Follow spacing, the post-swap ease (`FW_EASE_*`, about two tiles of travel), and the close face-in stay in the trail. The draw does not add a follow-line gap on top of that pose. A swap still skips the tie-break.
- Rail facing, talk reach, and the swap finish stay in the rail and interaction code.
- NPC contact recall, seamless-transition recall, and the intentional drops stay in the scene policy.
- Unown form 27 and Arceus form 0 stay in the form table.
- Grass still uses `fwr_cover_feet` on the saved submitted pose.

Stairs, mats, and Center lips use the actor's Y. There is no flat-versus-slope ray branch. A follower standing higher on a stair is closer to a downward camera and can sort in front, the same way an NPC on that step does.

## Black lines

Not fixed on hardware. The draw path does not add a GX submit or a texture upload.

What did change: `fwfx_emote_begin` no longer copies the live field scene (`fwemote.scene = *scene`). The bubble keeps that scene's heap id and scale, zeroes the rest, and sets polygon id 1. The submit is still the one existing `0x0204ebdd` call.

Still phase 2, and it needs a DS: `fwfx_destroy` / `fwfx_prepare` still release and re-upload texture keys across a loading wipe, and the recall flash still clones a field scene header. Emulators retire that frame cleanly. Do not fold another submit into the sort path to chase it.

## Round 2

The rest snap was draw-only. The trail still rests at the gap-6 metric (about 1.375 tiles, straight-line 90110). Ease already ends on that pose.

- Idle overlap slid the follower 8 world units further along the follow line, and that slide was skipped during ease. Side-by-side sprites overlap at the real rest gap, so every flat rest took the extra half tile and the ease end jumped. Talk still used the actor, which is why the far picture stayed interactive. An NPC could walk the visual gap.
- The oval then slid by `8 * max(sx, sy)` on every frame. That product is the screen half-width, half a tile to a full tile for a normal quad, not a tie margin. It runs across the camera, so an east-west follower left its oval and a north-facing one wore the oval. On a slope the search often missed, so the native oval stayed under the feet; the flat step found it and moved it.

At `48114d7`, bob-down and some walk frames put the billboard under an art foot line, and that line planted them. The hop above the line stayed. One odd southbound incline step was the same ground keep. The draw no longer plants or keeps.

## Footing redesign

The draw follows section 4 below. Prop clips, oval submit order, and the trail gap are not in this change.

Playtest of `48114d7` (nine shots). Clips into props and into the ground. Side-facing art often floats; front and back usually sit down, with the oval still drawn over the feet. A turn can float a north/south pose. Mid-step, the follower's height matches the player, and that matched height is the reference. Stairs and inclines still look seated. An NPC can still pass the rest gap. Outer Castelia rails keep height; tighter, grid-like rails do not, and the oval sits on the back-facing sprite there. Recalls, interaction, perf, and the untouched player quad still hold.

### 1. Incline and flat were the same function

At `48114d7`, `fwr_draw` and `fwr_shadow_pass` did not branch on flat, slope, stair, or rail. `fwr_follow_margin`'s flat flag was not on that path. What changed the picture was the pose the engine already built, and whether the later edits found a quad to rewrite. Section 4 is the path that replaced those edits.

That shared path, on every map:

1. The trail writes actor world Y from the player sample (`world.y + drawOffset.y + externalOffset.y`), including slope and rail knots. `fwr_anchor` copies descriptor byte 15 into `dimensions[4]` for every facing. That control Z stays on the actor. The draw does not add a facing Z.
2. The field engine builds the native billboard from that actor before `FollowingDraw`. Mid-step, that billboard is the same height family as the player. That is the picture in the transition shot.
3. `fwr_draw` then edits only the follower billboard. Registry Y is `sprite_y - descriptor[14] - (face == 0 ? 2 : 0)`, in world pixels. The art foot line is `world.y + (sprite_y - (face == 0 ? 2 : 0)) * 4096`. If the quad is below that line, Y is raised to it. The line never pulls a high quad down. Hop above the line stays.
4. When the screen rects overlap and the view gap is inside `(-8, +4)` world units of view depth, `fwr_plane_keep` slides the follower up to 6 world units away from the camera on XZ. Y is not part of that slide. A swap skips it. Ease and idle both run it. At a normal field camera the cap is what actually runs (~6 units, mostly along Z).
5. `fwr_shadow_pass` runs later, on the shadow scene. It keeps an oval inside one XZ tile and two Y tiles of actor ground, and rejects the oval nearer the player. It then plants that oval on the submitted body's XZ and on `world.y`, and runs `fwr_plane_keep` again. The body and the oval can each move 6 units. The oval's scale does not.

Why a slope looks seated: actor Y and the native billboard both follow the step, so they already match the player, and the foot line is that same Y. The plant is a no-op when the quad is already on the step. The native oval is on that mesh. Our search often still rewrites it, but the rewrite lands on the same height the mesh is using. A steep or short rail knot is the case that misses: oval Y more than two tiles off the actor, or the knot Y off the visible boards. The pass leaves the engine oval alone, which is the seated picture.

Why flat shows every edit: actor Y is constant, so registry Y, the north two pixels, the one-sided plant, and both camera slides are the whole difference between the native mid-step pose and the rest pose. The oval search hits, because the native oval is within a tile of that constant Y. Flat is not a second path. It is the path with nothing else covering our edits.

Props and the ground clip are the hole left when the prop depth budget left with the ray stack. `fwr_box_advance_limit` is not called. Nothing reserves a counter, a monitor, or an angled floor polygon. A quad whose anchor Y is plausible still loses texels to that mesh. Inclines hide this when the step and the actor agree. This is not fixed by another foot line.

### 2. Why plant, keep, and oval disagreed by facing

On that same `48114d7` path, three different anchors, and only one of them knew the facing.

| Piece | XZ | Y | Facing |
| --- | --- | --- | --- |
| Native billboard | engine, from the actor, including the hop | engine anchor for that pose | the pose the engine selected |
| Foot plant | not moved | `world.y` plus registry Y, minus 2px only for face 0 | north only |
| Body keep | up to 6 units away from the camera, only inside the tie band | unchanged | none; east-west rest is inside the band, north-south rest is usually outside it |
| Oval | submitted body XZ, then another keep | actor `world.y`, not the planted billboard Y | none |

Side poses float because the plant never lowers a quad. Side art is anchored higher than its pixels' feet. Front and back art is anchored nearer the feet, so those poses look seated. North then drops two pixels that south, east, and west do not. After a turn the engine swaps the pose and our north term flips, so a north/south sprite can pop off the height it had mid-step.

The oval is locked to actor Y and to a camera-shifted XZ, not to the billboard anchor. When the art feet are below that Y (negative `sprite_y`, or the plant allowed a dip down to a low foot line), the oval is the higher quad. The shadow pass submits after the body, and a downward camera treats the higher quad as nearer, so the oval covers the feet. When the art feet are above that Y, the body floats and the oval sits on the ground under empty space. The second keep shoves the oval across the camera, onto the back of a sprite that is facing away, which is the rail back-sprite and the old "oval on the torso" shot. East-west rest also takes the body keep, so the sprite and the oval leave the trail on Z by up to 12 units between them. North-south rest usually skips the body keep (the view gap is already past 8 units) and still takes the oval keep, so the oval walks up the sprite while the body stays on the trail.

Rails use this same split. A long Castelia rail has knot Y on the road, so actor Y, the billboard, and the mesh agree. A grid-like rail has knot Y off the boards between knots. The plant and the oval both trust that knot Y. The native billboard was the one that matched the player during the step.

### 3. The NPC at rest is the real trail gap

Confirmed from the trail, not from the draw.

`directional_gap` starts as `side_gap + 6` (`FW_DEFAULT_SIDE_GAP_BONUS`), then may be replaced by the positioning row. `trail_distance` compresses a cardinal step by `16 / (16 + gap)`. The push holds one tile of that metric (`FW_TILE`). For gap 6 the straight rest is 90110, which is 1.375 tiles. `tests/logic.c` locks that number. Ease ends on the same metric, so the ease is not a shorter target that the stop then abandons.

A player on a tile center and a follower 90110 behind share no empty tile between them. The follower's center falls in the adjacent tile, 8194 units from the far side of that tile. The open floor an NPC walks through is that 1.375-tile center gap. Collision uses the actor, which is this pose. The draw no longer adds a follow-line slide on top. Closing it would be a trail-metric change, and it is out of this footing work.

### 4. One footing path

Shipped in the draw. The body is the native billboard plus registry Y. Automatic registry Y is 0. The oval is the engine sample. Sort-by-submit-order and the prop depth budget are not in this change.

Use the pose that already matches on a slope, a stair, and mid-step. Stop maintaining a second foot line.

- Body: native billboard plus the registry Y (`sprite_y - descriptor[14]`) for every facing. No north-only pixels, no foot-line plant, no `fwr_plane_keep` on the body. Idle bob stays the engine's bob. Flat, slope, stair, and rail all take this path.
- Oval: leave the engine sample. Do not search it, do not retarget its XZ, do not assign `world.y` over it, do not run a camera keep. The slope looks seated when this sample survives. The shadow hook still submits the scene it already submits. No new GX upload.
- Sort, if the oval is still over the feet after that: the two quads share a foot and the oval is submitted later. The next change is order inside the existing passes, after one traced frame shows the depth word. No world slide, and no return of the eye-ray stack or the player write.
- Props, monitors, counters, and angled floor texels: a later depth budget against static mesh, reading bounds the way `fwr_box_advance_limit` already knows how to read them. It does not write the player, and it does not grow a per-facing table.
- Trail gap: unchanged unless a separate decision shortens gap 6.

The same helpers stay available to full-variant land and surf. This draw does not call them.

### 5. What shipped

Steps 1 and 2 shipped together. Each is one path, not a facing table. Step 3 and the prop budget are not in this change.

1. Body. The foot-line raise, the face-0 two pixels, and the body `fwr_plane_keep` are gone. Registry Y stays on the billboard. Automatic Y is 0: a stored `-pad` and the older `-(pad)-3` are rewritten there, and no second constant is added. `verify_render.py` expectations that locked the plant and the six-unit stair slide now expect the native billboard plus whatever registry Y is stored. Host math tests still cover the helpers; the draw does not call them.
2. Oval. `fwr_shadow_pass` does not move quads. The engine oval is what submits.
3. Not in this change. If a playtest still shows the oval over the feet, capture `FollowingRenderDebug` before changing submit order or a depth word. Do not add a slide. Prop and floor-mesh clips stay a later static-bounds step, still without writing the player.

Black-line texture lifetime stays phase 2 and is not part of footing.

## Sprite sources

The walking body is not a private sheet and not a second loader. Stock species and module-added species take the same spawn. There is no bank0/bank1 branch in this module. Retail object codes already fall into three windows of one descriptor table (`a/0/4/7`, 1008 stock rows) and one model archive (`a/0/4/8`, 975 stock BTX members): codes below `0x179`, `0x1000`–`0x126b`, and `0x2000`–`0x200a`. `fw_stock_row` maps only those windows. Anything else, including `0x3000` before publish, becomes row 10.

Install (`buildGen5FollowerArchives` in `src/pokeweb/followingPokemonProject.ts`) appends descriptor rows at index 1008 and up. Each new row’s object code is `0x3000 + row - 1008`. A stock appearance copies its retail descriptor, including the existing `a/0/4/8` member. Species 494+ and PNG imports append a new BTX member and point only that extension row at it. Stock rows are not retargeted. `rom:/following/` holds the registry, effects, emotes, dialogue, items, and (full variant) mounts. Those NARCs are not the walking body. `following/bundled-assets.narc` is editor preview only.

Runtime, one path:

1. `config()` in `field.c` reads `rom:/following/native.bin` and `rom:/following/runtime-registry.bin`, then `PokewebFollowingPublish` stores how many extension rows exist.
2. `model()` matches species, form, gender, and shiny to a registry row and returns that `0x3000+` code. It also stores `sprite_y`. It does not load pixels. Editor `source` (`stock`, `hg-engine`, `png`) is not in the runtime row.
3. Spawn is the retail constructor `0x021668c1` with that code. The same constructor builds map NPCs; their code comes from the map’s retail overworld sprite id.
4. ARM9 `0x0200fe34` is `PokewebFollowingObjectRow`. After publish, `0x3000+` codes become rows 1008+. Overlay 12 `0x02167fb8` stops truncating the descriptor file offset to 16 bits, so `4 + row * 28` still works past 64 KiB. The fixed native cache is not grown.
5. The engine copies that 28-byte descriptor onto the actor and loads `rom:/a/0/4/8` member `descriptor+16`. Byte 7 is the 32 or 64 draw mode. Byte 9 is 13 (six frames, right mirrored) or 14 (eight frames). The module does not upload that texture on the draw path. Recall’s white flash (`fwfx_prepare`) opens the same member again.

`animation_gate` only clears `PAUSE_ANM` so the engine may step the sheet. It does not author a hop. Which pixels move, and which quad motion is the engine's, is in [Billboard, bob, and sheet contracts](#billboard-bob-and-sheet-contracts). `sprite_y` is a constant registry offset. Descriptor byte 14 stays 0 so the native shadow stays on the ground.

A map NPC still cannot show an added follower sprite. Its sprite id is a retail object code, so it still resolves to a stock row and that row’s original member. The overworld editor lists those retail ids, not registry rows. Nothing in map data passes `0x3000`. Until publish, that code is row 10 anyway.

## Billboard, bob, and sheet contracts

Measured from the three unpacked strips, then checked against the draw. Background pixels are not feet.

| Sheet | Cells | Foot pad (empty rows under the opaque foot) | Within a facing |
| --- | --- | --- | --- |
| Native 6-frame (selector 13) | 32×192, six cells | 2 on every cell | Pairs match. No baked bob. |
| 8-frame (selector 14) | 32×256, eight cells | 1 on the first four, 2 on the side four | Pairs match. The 1px difference is facing, not a frame. |
| Module-added source | 32×384, twelve cells | Groups of three: 1, then 0, then 0 | The whole sprite drops one pixel on the two step cells, and those cells touch the bottom row. |

The quad is foot-anchored. `screen_span` in `render_math.c` covers ±8·`sx` sideways and 0..16·`sy` upward from the billboard position. The map preview builds the same quad: bottom edge at Y=0 with texture V=1, top at the cell height with V=0 (`buildNpcSpritePrimitive`). The full cell is on the quad. This module does not crop the bottom texel. A pad of 0 means the foot pixel is the quad’s bottom edge. That pixel is the one the floor mesh can cover. A pad of 1 or 2 is empty cell, so the visible foot sits that many pixels above the anchor until something else moves the quad.

Byte 7 picks the cell: 0 is 32, 2 is 64, 1 is 16 and crops (`followerBillboardDrawMode`). Byte 9 picks the controller. Install forces 13 for six frames and 14 for eight (`followerAnimationSelector`). The editor’s controller table gives both types two frames per direction (`NPC_SPRITE_FRAMES_PER_DIRECTION` in `map3dModel.ts`). Order in the follower preview is up, down, left, then left mirrored for six frames, or an explicit right pair for eight (`followerAnimationFrame`). A twelve-cell strip is four facings times three frames. `validateFollowerResource` rejects that count. Controllers 13 and 14 never index a third frame. The installed member is six or eight of those cells. If the kept pair is the pad-1 cell and a pad-0 cell, the one-pixel drop is what plays.

Bob, per sheet:

- Six-frame native. Nothing in the cells moves. In play, this layout does not hop. `animation_gate` still unpauses it, so the two frames can swap. The native Y curve for controller 13 is not in this repo; the still picture matches a controller with no Y translation. `sprite_y` cannot be that hop, because it is one constant for every frame.
- Eight-frame. Cells in a pair share a foot. The hop seen in play is the engine moving the quad, already in the billboard Y when `fwr_draw` runs. The same unpause applies as for six frames. The draw no longer flattens that dip.
- Twelve-cell added art. The sheet drops the sprite by one pixel on the step cells, onto the quad edge. That is independent of the engine. Whichever selector the installed pair uses, the engine hop for that selector still runs, because `animation_gate` does not look at the sheet. A pad-0 frame can sit on the anchor while the quad Y is exactly on the foot line.

`deriveFollowerGrounding` sets automatic Y to 0. A stored `-pad` or `-(pad)-3` is rewritten to 0 on the next derive. Those pads are 2, 1, and 0 on these three strips, so the old automatic values were −2, −1, or 0, and −5, −4, or −3 for the extra-three bury. Descriptor byte 14 stays 0, so the oval does not follow the registry. The draw does not add a north-only two pixels or another constant.

What a follower changes relative to a map NPC, for height:

- A map NPC keeps its retail controller and its descriptor Y on the actor, so the body and the shadow share it. A follower forces byte 9 to 13 or 14. The artwork apply stores 0 in byte 14; the catalog copy leaves the source byte, which that path treats as 0. Visual Y is only on the body quad (`sprite_y` in `fwr_draw`). The oval stays at actor ground.
- A map NPC is not passed through `animation_gate`. A visible follower clears `PAUSE_ANM` (flag 16) even while standing on the trail, so that controller keeps running at rest. The player is not on this gate.
- The foot plant and the north-only two pixels are gone from the follower draw. A map NPC does not have them either.

A one-pixel change to `sprite_y` was not added. It would lift every frame and leave a 1/0/0 sheet alternating. Automatic grounding is 0 for every facing. The pad stays inside the cell, which is where a map NPC leaves it.

Keep one foot row inside the cell, with at least one transparent row, on every frame of a facing. Let the engine own the hop.

## Axes

At draw time the up axis is world Y. It is the same Y the map tools in this repo call height. It is not the north/south axis, and it is not Blender's Z.

| Name | East / west | North / south | Up |
| --- | --- | --- | --- |
| Gen 5 actor and `fwr_draw` | `world.x`, `grid[0]`. Positive is east. | `world.z`, `grid[2]`. Positive is south. | `world.y`, `grid[1]` |
| This repo's map preview and `actor_set_g_pos` | X | Z | Y |
| 2D matrix and dialogue bounds | X, increasing east | Y, increasing south. There is no Z. | not this pair |
| Blender after the glTF import | horizontal | horizontal | Z. The importer turns the game's Y-up into Blender's Z-up. |

Proof, from code this module runs:

- `fwr_world_apart` copies Y and, when the pair is closer than one unit, steps the back direction: face 0 adds Z, face 1 subtracts Z, face 2 adds X, face 3 subtracts X. North's back is south, so positive Z is south and positive X is east. Y is not a compass axis.
- `camera_plane` builds the ground right vector as `(view.z, 0, -view.x)`. The zero is world Y. XZ is the ground plane.
- `screen_span` puts the quad bottom on the billboard position and the top `sy * 16` along that camera up. The position is the foot anchor. `fwr_draw` adds `(sprite_y - descriptor[14]) * 4096` to `position.y` only. One registry pixel is one world unit (`4096`). `FW_TILE` is `16 * 4096`.
- `interaction.c` states it: native grid Y is height, grid Z is map tile Y. Spawn passes `grid[0]` and `grid[2]` to the retail constructor. `centered` tests X and Z, not Y.
- The NPC event record stores map X and map Z as tiles and height as an fx32 (`map3dModel.ts`). Registry byte 14 is `wPosOffY`, the same descriptor byte `fwr_draw` subtracts. `actor_set_g_pos` (`0x6d`) documents Y as height and Z as the north/south axis.
- `docs/map-blender-export.md` says the exported map is Y-up and Blender's importer converts it to Z-up. A height nudge typed into Blender's Z is this module's Y. A nudge typed into Blender's Y is not height. Do not swap again on import (`docs/map-glb-import-plan.md`).

The oval is not on that billboard Y. `fwr_shadow_pass` does not move it. Descriptor byte 14 stays 0 so the engine sample stays on the actor. `FollowingRenderDebug.groundY` (`+104`) is `world.y` when the shadow is live.

## The foot gap

Playtest after `02a9907` showed feet a little lower than the player, bob-down clipping the floor, and the oval showing on that dip. Automatic `sprite_y` was `-pad`, so the opaque foot sat on actor ground, `pad` pixels below a map NPC whose descriptor Y is 0. Derive now rewrites that value, and the older `-(pad)-3`, to 0. The draw still adds only that registry Y to billboard `position.y`. It does not write `world.y` or descriptor byte 14, so the oval stays the engine sample.

Actor height is already shared. `player_sample` stores `world.y + drawOffset.y + externalOffset.y`, and `bind_pose` writes that into the follower's `world.y`. On flat ground the two actors match.

With automatic Y at 0 and descriptor byte 14 at 0, the settled quad bottom is the native billboard. The visible foot is that bottom plus the sheet pad, which is the map-NPC placement. A pad of 0 on a step cell still touches the quad edge. Bob-down is the engine hop. A shared leftover after this, the same pixel count on every species and every facing, is the only reason to add one more `position.y` constant, and that constant is measured pixels times 4096.

The same debug subtraction still reads the result. `submitted.y` is at `FollowingRenderDebug + 64`, `native.y` at `+52`, and `groundY` at `+104`. `(submitted.y - groundY) / 4096` is the quad bottom against actor ground. Follower `actor+72` minus player `actor+72`, divided by 4096, is the actor-height gap. Expect 0.

## Playtest

- Castelia outer ring, walking past street NPCs: the player stays behind people who stand nearer the camera. No constant follower pop in/out.
- Pokémon Center lower floor, both sides of the bad tiles: the follower hops, the player does not, and the oval stays on the ground under the follower.
- A wide species beside you, and one specific sprite that used to lose to its oval.
- Stairs and a stairwell: the follower is not buried in the step, and is not floating off it. Height follows the floor.
- Reverse-step swap: close, facing you, then easing out to the usual gap over about two tiles. Stop and talk hold the gap.
- A rail curve: facing, talk, and the swap finish on the settled step.
- Walk into an NPC: the follower recalls. Castelia crowds do not.
- Door in and out (loading screen), and talk until the emote bubble appears. Note any full-screen black flicker; that is the phase-2 GPU lifecycle, not the sort path.
- A trashcan or other small prop you stand against. The player quad is no longer slid past it. Say if the player and the prop tie.
