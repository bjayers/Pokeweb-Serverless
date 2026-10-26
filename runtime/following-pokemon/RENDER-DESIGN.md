# Follower render: world feet

The draw submits the native billboard plus one registry Y and two world pixels, on every facing. It does not plant bob-down, does not add the north-only two pixels, and does not slide the body. The oval's picture stays on the engine ground sample. The trail, swap, ease, rail, recall, and talk code are unchanged. The player quad is not written.

## What the frame does now

The follower actor's world pose is the placement. Trail, swap, ease, and rails write that pose the way they write an NPC. `fwr_draw` does not move the player.

Submitted follower quad:

- Start from the native billboard, which already includes the world pose and the idle hop.
- Apply the registry Y (`sprite_y` minus descriptor byte 14) plus two world pixels on this quad only. Every facing uses that same offset. Nothing adds Z.
- Light the follower from its own tile for the submit, then restore the follower quad. Retail `FldAct_SyncLights` uses light 3 alone on a shaded actor and light 0 alone otherwise; the shade is tile flag `0x100` (`ActorTerrainEffect_Shadow`), which `terrain_update` reads once per follower tile because the native terrain pass never runs for this actor. A player mask other than those two is still shared. The player billboard, world, moveflags, and animation are not written.

`policy` and `sortMargin` stay 0. `before` / `after` / `depthRel` are the submitted view gap, hop included. `hold` is always 0. `snap` is 1 on a frame that depth-biased the follower oval, and 0 when that oval was left alone. The debug record layout is unchanged.

Shadow pass:

Retail order is actors (`FollowingDraw`, `field+0xc0`), then grass (`field+0xc8`), then shadows (`FollowingEffectsDraw`, `field+0xc4`). The shadow submit is a different pass. The setup in front of `0x0218122e` is what makes that oval a ground decal. `59ae58c` called `0x0204f685` on `field+0xc4` from `FollowingDraw` and then skipped the real shadow submit. That drew the oval in the actor pass, with actor depth state, before the engine placed the decal. The oval won the depth test more often. That early submit is gone.

The oval still shares the actor's ground Y. It is a flat disc, so its camera-side rim is closer than its center, and a bob can drop the body onto that center. The shadow pass draws after the body. A fragment that passes the depth test paints over the feet. Position is the sort key. This scene has no per-quad polygon id.

`fwr_shadow_pass` still submits `field+0xc4` once, after the bodies. When the follower's shadow is live, it keeps the one oval within half a tile of actor ground (two tiles of Y) that is not the player's, and pushes that quad away from the camera by its near-rim depth plus eight world pixels, capped at one tile (`fwr_oval_behind`). Perspective scale keeps the picture on the engine's ground sample. The quad is restored after `0x0204f685`. The body is not moved. The player quad is not written. An orthographic camera leaves the engine sample. The push does not follow the sprite, so rest and bob-down get the same oval.

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

On a 3DS, a second billboard submit (`0x0204ebdd`) that still carries the live field scene's polygon id and the 20-byte tail draws black lines on the follower. The edge buffer is hardware state. Emulators retire the frame without it, so the same build looks clean there and lined on a 3DS.

`fwr_private_scene` is the only header for that second submit. It copies the allocator id, the scale, and the four light colors, zeroes the tail, and sets polygon id 1 (or 2 when the field scene is already 1). The recall flash, the emote bubble, the mounted rider, and the Surf rider all go through it. None of them copy `scene` with `*scene` anymore. The walking body is still the retail actor submit. This pass does not add a GX submit or a texture upload.

If a line remains on a follower recall, emote, or rider, change the polygon byte inside `fwr_private_scene` only. Do not open a new draw path for it.

The horizontal streaks on a door, a menu close, and a battle return are that same texture-key release landing after the picture is already up. `fwfx_tick` is the only clock, and it does not run while a load, a battle, or a menu has stopped the actor system. A two-tick wait queued by `fwfx_destroy` therefore calls `0x02049561` on the first frames the overworld is visible again, follower on screen or not. Deferring that free until the new map had set its decal is what stopped the sticky indoor ovals. The free itself was still a VRAM remap while the new picture was scanning.

`fwfx_destroy` now drops those pointers and does not call `0x02049561` or `0x02049431`. Unload, battle, and a new actor system all take that path. The next 3D setup reclaims the banks. Emote end and the white-sprite reload still wait two field ticks, because those happen on the current map while a queued draw may still name the key. A door or return fade (reason bit 16, Following on) calls `fwfx_warm`, which uploads the effect banks only. A hidden door actor does not `fwfx_prepare` until it has already been shown for a frame. Settings off never warms and never prepares.

Emote sheets are not part of that upload. `0x0204e55d` does not return a key. It writes `tex`, `pltt`, `texKey`, and `plttKey` into the material object it is given. `fwr_private_scene` stores a pointer to `fwemote.material`, and `fwfx_draw` submits that scene, so the bind has to be on that object in the 3D context that will draw it. `a4141d0` ran the effect-model upload `0x020494d9` and copied guessed TEX0 words, then `fwfx_emote_begin` returned 0 unless that bank was marked live. `b6e0cfe` ran the real bind on a side array during the fade and copied the struct. The fade's banks are reclaimed by the next 3D setup while the copied keys stay non-zero, and begin still refused to draw when the fade bank was gone. A one-sheet fade warm has the same ownership gap. The talk path loads the two members and binds them on `fwemote.material` when the bubble starts. That draw does not submit. The next `fwfx_draw` submits the same material, so the bubble is one frame later. Do not bind emote VRAM under the fade, and do not submit a billboard there. The load-phase MatCache hook does not bind this material.

`2ed63ab` tried to spend the body's first billboard submit under that fade, or thirty tiles under the floor when there was no fade. Hardware put the load-screen lines back, a bit worse. The fade is a 2D layer on a live 3D scan. A submit in that window is not covered, and a quad below the floor still rasterizes a full-width line. That approach stays reverted. Do not unhide during the fade, do not bind `a/0/4/8` there, and do not park a submit under the floor.

Stock White 2 finishes map-NPC and player `FldActSys_LoadStaticBlact` (`0x0219386d`) in `Field_InitActorSystem`, then `FieldRoutine_MapSpawnFinish` case 0 drains leftover async loads. The stock hook at `0x0217fe88` is that drain's next call, `FieldG3D_Update`, still before `FieldG3D_RenderPhase1` and before the entrance fade. When Following is on it calls the same `LoadStaticBlact` with the follower object code from `select` and `model`, then `FieldBlAct_FindCachedObjMat` (`0x02193e04`) on the field billboard at actor-system `+40`. `GetNPCMdlInfoForOBJCODE` (`0x02167f18`) calls `GetIndexOfObjID` and contains the wide descriptor-offset patch, so a `0x3000+` row is the row spawn already uses. The upload is the retail MatCache path. Following off does not call it. This is not a fade bind and not a billboard submit. The earlier `0x02180aea` site returned before this call: the registry open ran while map load still owned the cart.

`FollowingConfigDebug` (magic `FWCG`, RAM bytes `46 57 43 47`) version 5 stores that lookup and the spawn. Word `+24` is `insert`, word `+28` is the object code, word `+32` is the material in the low 16 bits and the cache flags in the high 16. Word `+36` is `covered`. `insert` 8 means the uid is cached, the material is not `0xFFFF`, and flags bit 2 is clear. `covered` 1 means `CreateNewActorByParam` (`0x021668c1`) then returned an actor whose billboard slot was real and whose `HIDDEN` flag was clear, before `FieldG3D_RenderPhase1`. `stage` at `+8` is the registry load: 7 means the registry opened.

Playtest of version 4 reported `insert` 8, uid `0x39FE`, and `covered` 1, and the reveal still drew the same line. The body sheet was resident and the first billboard submit was already in the MapSpawnFinish render. What still wrote VRAM while that picture was on a live scan was `fwfx_warm` (`0x020494d9` on the effect banks) and, one update after the first draw, `fwfx_prepare` (`0x0204e55d` on the white sheet). `before()` returns early on the entrance fade, but `FollowingUpdate` continues, so those calls landed on the frames the fade was revealing. The hook now runs `fwfx_warm` after config succeeds, and `fwfx_prepare` on the covered actor, both before `FieldG3D_Update` returns into the render. A later fade warm sees the banks already live. A later prepare sees the same sprite id and does not upload. The field update still adopts the actor. An entrance fade (reason bits 4 and 16 only) leaves it in place. A later door fade still recalls. A party change or faint still builds a new actor on the live picture and holds that new body until its own billboard exists. Shadow birth still waits until the update after Following has drawn the body.

The cache has 32 slots and drops a registration with no error when it is full, so that follower still uploads on first draw. L/R and a faint after the zone is up still allocate on the live picture. A code an NPC already cached takes a second slot; spawn finds the first.

`fwemote.material` is not a MatCache entry. A private `0x0204e55d` in this window is unproven: the bubble reads that material, not the retail cache, and a key that is not still on it at talk does not draw. That bind stays the talk path. Do not move it here, and do not bind it under the fade.

The first visible frame used to also birth the shadow task (`0x02194df5`) and, on a send-out, the flash model. Those still wait until the update after the body has been drawn. The ball still draws while the actor is hidden. The entrance effect-bank upload and the white-sheet bind run in the MapSpawnFinish hook, before the fade reveals the picture.

## Tall followers

Field actors are `BLACT_TYPE_FACE_CAMERA` quads (`BlActScene_Draw`, pokebw2 `src/gfl/billboard.c`). A quad parallel to the camera plane leans back by its height times sin(pitch). The field actor scene's scale is 7.0 (`BLACT_SCENE_SCALE` in `fldmmdl_blact.c`), so an NPC quad is 28 world units tall and a 64-pixel follower 56. Maps keep walls clear of the first, not the second, and a big sprite also fills the top rows NPC art leaves empty.

`fwr_draw` splits a 64-pixel body (descriptor byte 7 = 2) whose material is 64×64 faces, not shared with another visible actor, and not flipped vertically:

- Base: the native submit keeps the bottom 8 rows (`FWR_BASE_ROWS`), with the material's face height set to 8, the face moved to the last eighth of its row, and `sy` divided by 8, all restored after `0x0204f685`. That is where grass and the oval meet the feet, and it leans back only 7 × sin(pitch) ≈ 5 units: inside the 8 units to a wall at the tile's edge.
- Strips: the 56 rows above go through one private `0x0204ebdd` submit as seven 8-row strips (`FWR_STRIPS`), seven actors on one material with face height 8. `fwr_strips` anchors each strip on the camera plane at its own height and moves it toward the camera along its own eye ray until its top edge stands a quarter unit per unit of height in front of the feet, measured along the ground (`FWR_LEAN` 1024, hinge 0), capped at five tiles. Pixels stay put. The top row of a 56-unit body ends about 14 units in front of the feet, clear of a wall at the tile's edge and of an awning over that tile. A 1/4 hinge put the stack's line 10 units behind the feet, inside such a wall.
- Player: a strip that overlaps the player's quad on screen stays one unit behind the player's depth, does not move when they are level, and is free when the follower is clearly in front. Following behind, the follower is 24 depth units back, which leaves most strips free.
- The forward lean is the trade: something standing within about a tile in front of the follower and tall enough to cover its top rows draws behind them. `FWR_LEAN` 0 is an upright stack over the feet.

The steps between strips are several world units of depth, so distance fog shades the body in small bands instead of one bright upper half. The camera basis, ground direction and eye distance are built once per frame, and each strip costs one divide (two when the cap or the player limits it); the overlap test cross-multiplies instead of dividing. Pitch, yaw and zoom changes are picked up every frame.

The private header takes the live polygon id, origin, custom normal and texcoord offsets, so the strips meet without an edge-marking seam and are lit the same. A follower that fails a check, or a camera looking straight down, draws whole, as before. 32-pixel followers are unchanged. If a seam shows on hardware, the first thing to try is the polygon id in `split_body`.

## Scanline window

The fade was never the variable. `GFL_G3DResUploadTexData` (`0x020494d8`) ends in `NNS_G3dTexLoad(tex, TRUE)` and `NNS_G3dPlttLoad(tex, TRUE)`, an immediate load: the texture banks go to the CPU for the copy and come back. `BlActMaterial_BindTexture` (`0x0204e55c`) calls it. The 3D engine reads those banks on every line it renders, from line 214 through line 191 of the next frame, so any upload from the field update blanks the lines rendered during the copy. The lines read a mix of stale texels and the new palette. Where they land depends on how far into the frame the update has run, which is why each map draws them at about the same height. Emulators render the whole frame at once and never show it.

Retail does not upload from the field update. Mid-map NPC sheets bind in `FldActSys_AsyncMatLoadTCBFunc`, a VBlank task (`src/ov036/fieldmap.c` in the pokebw2 decomp), and `MapSpawnFinish` loads run before the screen is shown.

`fw_vram_window` in `native.h` spins until `VCOUNT` (`0x04000006`) is in 193–204, after the VBlank interrupt and before the 3D scan restarts, and every `0x020494d9` and `0x0204e55d` call in `effects.c`, `land.c` and `surf.c` goes through it. A 32×32 or 64×64 sheet copies in a few lines, so consecutive binds usually share one window. A bind can cost up to one frame of waiting. `FldActSys_LoadStaticBlact` in the `0x0217fe88` hook is the retail spawn-time path and stays as it is. The Unicorn verifiers map an I/O page with `VCOUNT` 196.

If a line remains with this in place, it is not an upload from this module. Look for another immediate `GX_LoadTex`, `GX_LoadTexPltt`, or VRAM bank change outside VBlank.

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

- Body: native billboard plus the registry Y (`sprite_y - descriptor[14]`) plus two world pixels, for every facing. No north-only pixels, no foot-line plant, no `fwr_plane_keep` on the body. Idle bob stays the engine's bob. Flat, slope, stair, and rail all take this path.
- Oval: the engine sample stays on actor ground. The shadow hook submits that scene once, after the bodies. For that submit only, `fwr_oval_behind` pushes the follower oval away from the camera and scales it so the picture stays on the sample. The quad is restored after the call. No plant, no XZ retarget, no camera keep, and the push does not follow the body.
- Sort. Early submit of `field+0xc4` from the actor hook is reverted. It drew the oval in the actor pass, which put it in front more often. The late pass plus the one depth bias is the order. No world slide, and no return of the eye-ray stack or the player write.
- Props, monitors, counters, and angled floor texels: a later depth budget against static mesh, reading bounds the way `fwr_box_advance_limit` already knows how to read them. It does not write the player, and it does not grow a per-facing table.
- Trail gap: unchanged unless a separate decision shortens gap 6.

The same helpers stay available to full-variant land and surf. This draw does not call them.

### 5. What shipped

Steps 1 and 2 shipped together. Each is one path, not a facing table. Step 3 and the prop budget are not in this change.

1. Body. The foot-line raise, the face-0 two pixels, and the body `fwr_plane_keep` are gone. Registry Y stays on the billboard. Automatic Y is 0: a stored `-pad` and the older `-(pad)-3` are rewritten there, and no second constant is added. `verify_render.py` expectations that locked the plant and the six-unit stair slide now expect the native billboard plus whatever registry Y is stored. Host math tests still cover the helpers; the draw does not call them.
2. Oval. The engine sample is what the shadow hook submits, after the bodies. `fwr_shadow_pass` depth-biases the follower oval for that one call and restores it. `59ae58c`'s early submit is not the order.
3. Not in this change. Prop and floor-mesh clips stay a later static-bounds step, still without writing the player. Do not add a slide to chase a remaining clip.

Black-line texture lifetime stays phase 2 and is not part of footing.

## Sprite sources

The walking body is not a private sheet and not a second loader. Stock species and module-added species take the same spawn. There is no bank0/bank1 branch in this module. Retail object codes already fall into three windows of one descriptor table (`a/0/4/7`, 1008 stock rows) and one model archive (`a/0/4/8`, 975 stock BTX members): codes below `0x179`, `0x1000`–`0x126b`, and `0x2000`–`0x200a`. `fw_stock_row` maps only those windows. Anything else, including `0x3000` before publish, becomes row 10.

Install (`buildGen5FollowerArchives` in `src/pokeweb/followingPokemonProject.ts`) appends descriptor rows at index 1008 and up. Each new row’s object code is `0x3000 + row - 1008`. A stock appearance copies its retail descriptor, including the existing `a/0/4/8` member. Species 494+ and PNG imports append a new BTX member and point only that extension row at it. Stock rows are not retargeted. `rom:/following/` holds the registry, effects, emotes, dialogue, items, and (full variant) mounts. Those NARCs are not the walking body. `following/bundled-assets.narc` is editor preview only.

Runtime, one path:

1. `config()` in `field.c` reads `rom:/following/native.bin` and `rom:/following/runtime-registry.bin`, then `PokewebFollowingPublish` stores how many extension rows exist.
2. `model()` matches species, form, gender, and shiny to a registry row and returns that `0x3000+` code. It also stores `sprite_y`. It does not load pixels. Editor `source` (`stock`, `hg-engine`, `png`) is not in the runtime row.
3. Spawn is the retail constructor `0x021668c1` with that code. The same constructor builds map NPCs and script `CreateNPC` (`0x69`). On stock White 2, `MapSpawnFinish` passes that code to `FldActSys_LoadStaticBlact` after the async drain, so the MatCache entry exists before the first visible billboard. A later species change still misses that entry until the next zone load.
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

The oval is not on that billboard Y. Descriptor byte 14 stays 0, so the engine sample stays on actor ground. The shadow pass does not raise that sample and does not store a new Y: for one submit it depth-biases the follower oval along the eye ray, scales it so the picture stays, and writes the engine sample back. `FollowingRenderDebug.groundY` (`+104`) is `world.y` when the shadow is live. `snap` (`+112`) is 1 when that bias ran. The emote bubble reads the submitted body, not this ground Y: `fwr_effect_pose` keeps the draw-only lift (`submitted.y - native.y`) on the live quad, so the bubble rises with the two-pixel body instead of sitting on the restored foot.

## The foot gap

Playtest after `02a9907` showed feet a little lower than the player, bob-down clipping the floor, and the oval showing on that dip. Automatic `sprite_y` was `-pad`, so the opaque foot sat on actor ground, `pad` pixels below a map NPC whose descriptor Y is 0. Derive now rewrites that value, and the older `-(pad)-3`, to 0. The draw still adds only that registry Y to billboard `position.y`. It does not write `world.y` or descriptor byte 14, so the oval stays the engine sample.

Actor height is already shared. `player_sample` stores `world.y + drawOffset.y + externalOffset.y`, and `bind_pose` writes that into the follower's `world.y`. On flat ground the two actors match.

With automatic Y at 0 and descriptor byte 14 at 0, the native quad bottom is the actor anchor. The visible foot is that bottom plus the sheet pad. A pad of 0 still touches the edge, and the engine hop then dips the opaque foot through the floor and the oval. That is the leftover low look. It is not another down-pull in the module.

`fwr_draw` now adds two world pixels (`2 * 4096`) on top of the registry delta, on every facing. The actor, descriptor byte 14, and the oval are not written. A sheet whose step cells already sit on the quad edge can still clip if the hop is deeper than those two pixels.

Do not hex-edit `following/runtime-registry.bin` to try this. File offset `0x2D` is the first v3 record's Y (header `03 00` at offset 4, records at `0x20`, Y at record `+13`), and a value of 1 or 2 is a legal `int8`. `fw_registry_stream` then rejects the file: the CRC at registry offset 24 and the outer CRC in `native.bin` (`encodeFollowerNativeConfig`, word at offset 24) no longer match. `config()` fails and every follower stays unloaded. Putting the byte back to 0 restores both checksums. Same-version install also does not rewrite that registry, so a derive change does not reach a DLL-only reinstall.

The debug subtraction still reads the result. `submitted.y` is at `FollowingRenderDebug + 64`, `native.y` at `+52`, and `groundY` at `+104`. With registry Y 0, `(submitted.y - native.y) / 4096` is 2. Follower `actor+72` minus player `actor+72`, divided by 4096, stays 0.

## Playtest

Cold-boot the stock field DLL that contains the `0x0217fe88` hook and `FollowingConfigDebug` version 5. Full installs replace `patches/PokewebFollowingFieldW2.dll`. Base installs replace `patches/PokewebFollowingFieldW2Base.dll`. This is DLL-only: do not replace `following/runtime-registry.bin` or any other ROM file. A same-version Pokeweb reinstall keeps the registry already in the ROM.

- Castelia outer ring, walking past street NPCs: the player stays behind people who stand nearer the camera. No constant follower pop in/out.
- Pokémon Center lower floor, both sides of the bad tiles: the follower hops, the player does not, and the oval stays on the ground under the follower. At rest and on bob-down, all four facings, the oval is under the feet, not across the body. Height stays the two-pixel body lift. The follower does not vanish.
- A wide species beside you, and one specific sprite that used to lose to its oval.
- Talk until the emote bubble appears. The bubble tip sits about eight source pixels above the raised head, not on the head and not on the old foot line.
- 3DS hardware, cold boot, Following on. Swap `patches/PokewebFollowingFieldW2Base.dll` for a base install (full swaps `patches/PokewebFollowingFieldW2.dll`). DLL-only: leave `following/runtime-registry.bin` in place. Cold-boot so the new DLL loads. A load-screen door, a menu close, and a battle return should not flash horizontal black lines, including the moment the world reappears and before the follower is called out. Following off stays clean. Flat indoor rooms keep player and NPC ovals behind the feet. With the follower out, the oval stays under the feet, the body stays the two-pixel lift, and the emote clears the raised head. A line that remains only on the follower during recall, emote, or Surf is the next polygon byte in `fwr_private_scene`.
- The same cold boot, after the entrance fade and before the next zone: search RAM for `46 57 43 47` (`FollowingConfigDebug`, `FWCG`). Word `+4` is version `5` (`4` is the previous tip). Word `+8` is `stage` (7 = registry loaded). Word `+24` is `insert` (8 = exist-mat). Word `+28` is the object code. Word `+32` low 16 is the material and high 16 is the cache flags. Word `+36` is `covered`. `1` means the actor was created with a real billboard and `HIDDEN` clear before the MapSpawnFinish render. `2` means the player actor was missing. `3` means `CreateNewActorByParam` returned null. `4` means the actor exists but the billboard slot was still `0xFFFF`. `5` means the pool, id, or heap check refused. `0` means this map did not take the covered spawn.
- When the entrance fade lifts, with `covered` 1: the follower is already on the player's tile with the zone NPCs. A full-width line on that reveal means the effect-bank and white-sheet uploads were not what streaked the revealed picture. A line on the load screen is those uploads, in the same window as the body sheet. `covered` 0, 2, 3, or 5 means the early create did not run and the body can still appear later from `FollowingUpdate`. L/R and a faint still build a new sprite on the live picture.
- Stairs and a stairwell: the follower is not buried in the step, and is not floating off it. Height follows the floor. A side swap on the stairs is the same: onto a higher tread it is not inside the ramp, and onto a lower tread it is not floating. Once the usual follow distance is back, height is unchanged.
- Reverse-step swap: close, facing you, then easing out to the usual gap over about two tiles. Stop and talk hold the gap.
- A rail curve: facing, talk, and the swap finish on the settled step.
- Walk into an NPC: the follower recalls. Castelia crowds do not.
- Door in and out of a flat indoor room with the follower recalled. No horizontal lines on the way in, and player and NPC ovals stay behind the feet. The same on closing a menu and on returning from a battle. With the follower out, the oval stays under the feet and the emote sits on the raised head.
- A trashcan or other small prop you stand against. The player quad is no longer slid past it. Say if the player and the prop tie.
