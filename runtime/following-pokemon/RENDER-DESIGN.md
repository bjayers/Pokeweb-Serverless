# Follower render: world feet

Shipped through `48114d7` on `cursor/following-fixes-852f`. This replaces the eye-ray sort stack (`fwr_correct` in the draw path, `fwr_above_shadow`, `fwr_keep_behind`, `fwr_player_in_front`, and the shadow `position.y = feet.y` patch). The trail, swap, ease, rail, recall, and talk code are unchanged.

The [footing redesign](#footing-redesign-not-implemented) below is analysis after the `48114d7` playtest. It is not in the binary.

## What the frame does now

The follower actor's world pose is the placement. Trail, swap, ease, and rails write that pose the way they write an NPC. `fwr_draw` does not move the player, and it does not slide either quad along the camera ray.

Submitted follower quad:

- Start from the native billboard, which already includes the world pose and the idle hop.
- Apply the registry Y (`sprite_y` minus descriptor byte 14) on this quad only. North-facing art also drops two pixels (`FWR_NORTH_ART_Y`), in world Y, for every camera. South does not add Z.
- If the screen rects overlap and the view gap is inside `(-FWR_WORLD_NUDGE, FWR_CLEAR_FRONT)`, slide the follower away from the camera along the ground by at most `FWR_PLANE_CAP` (6 world units). That is a tie break, not follow distance. Y, including the hop, stays. The tie test uses the art foot line, so the hop cannot toggle it. A swap skips it. Ease and idle use the same rule, so the end of the ease does not jump.
- The art foot line is actor Y plus the registry offset (and the north two pixels). A bob or walk dip below that line is planted on it. The hop above the line stays.
- Copy the player's light-mask bits onto the follower for the submit, then restore the follower quad. The player billboard, world, moveflags, and animation are not written.

`policy` in `FollowingRenderDebug` is 0 for feet-only and 2 when the ground tie-break ran. `before` / `after` / `depthRel` are view gaps. `sortMargin` is that ground slide. `hold` is always 0. The debug record layout is unchanged.

Shadow pass (`fwr_shadow_pass`):

- The oval is still found by the old window (one XZ tile and two Y of the actor ground, and not the oval nearer the player).
- `fwr_shadow_on_plane` plants it on the submitted body's XZ and the actor's terrain Y, then slides the center away from the camera along the ground by `fwr_plane_keep` (about four view units, capped at 6 world units). The quad's `sx`/`sy` do not change the distance. Scale is unchanged. Y is not assigned again after that slide.

`fwr_plane_keep` and `fwr_shadow_on_plane` place the tie-break and the oval. `fwr_world_apart` remains for tests; the draw does not call it. `fwr_player_in_front` is gone. The older ray helpers remain for the emote bubble, grass, and the full-variant land/surf draws. The base draw path does not call them.

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

Bob-down and some walk frames put the billboard under the art foot line. The foot line now plants those frames. The hop above the line is unchanged. One odd southbound incline step is the same ground keep, not a tile exception; if it is still a full step high or low, that is actor Y and needs another trace.

## Footing redesign (not implemented)

Playtest of `48114d7` (nine shots). Clips into props and into the ground. Side-facing art often floats; front and back usually sit down, with the oval still drawn over the feet. A turn can float a north/south pose. Mid-step, the follower's height matches the player, and that matched height is the reference. Stairs and inclines still look seated. An NPC can still pass the rest gap. Outer Castelia rails keep height; tighter, grid-like rails do not, and the oval sits on the back-facing sprite there. Recalls, interaction, perf, and the untouched player quad still hold.

### 1. Incline and flat are the same function

`fwr_draw` and `fwr_shadow_pass` do not branch on flat, slope, stair, or rail. `fwr_follow_margin`'s flat flag is not on this path. What changes is the pose the engine already built, and whether our later edits find a quad to rewrite.

Shared path, every map:

1. The trail writes actor world Y from the player sample (`world.y + drawOffset.y + externalOffset.y`), including slope and rail knots. `fwr_anchor` copies descriptor byte 15 into `dimensions[4]` for every facing. That control Z stays on the actor. The draw does not add a facing Z.
2. The field engine builds the native billboard from that actor before `FollowingDraw`. Mid-step, that billboard is the same height family as the player. That is the picture in the transition shot.
3. `fwr_draw` then edits only the follower billboard. Registry Y is `sprite_y - descriptor[14] - (face == 0 ? 2 : 0)`, in world pixels. The art foot line is `world.y + (sprite_y - (face == 0 ? 2 : 0)) * 4096`. If the quad is below that line, Y is raised to it. The line never pulls a high quad down. Hop above the line stays.
4. When the screen rects overlap and the view gap is inside `(-8, +4)` world units of view depth, `fwr_plane_keep` slides the follower up to 6 world units away from the camera on XZ. Y is not part of that slide. A swap skips it. Ease and idle both run it. At a normal field camera the cap is what actually runs (~6 units, mostly along Z).
5. `fwr_shadow_pass` runs later, on the shadow scene. It keeps an oval inside one XZ tile and two Y tiles of actor ground, and rejects the oval nearer the player. It then plants that oval on the submitted body's XZ and on `world.y`, and runs `fwr_plane_keep` again. The body and the oval can each move 6 units. The oval's scale does not.

Why a slope looks seated: actor Y and the native billboard both follow the step, so they already match the player, and the foot line is that same Y. The plant is a no-op when the quad is already on the step. The native oval is on that mesh. Our search often still rewrites it, but the rewrite lands on the same height the mesh is using. A steep or short rail knot is the case that misses: oval Y more than two tiles off the actor, or the knot Y off the visible boards. The pass leaves the engine oval alone, which is the seated picture.

Why flat shows every edit: actor Y is constant, so registry Y, the north two pixels, the one-sided plant, and both camera slides are the whole difference between the native mid-step pose and the rest pose. The oval search hits, because the native oval is within a tile of that constant Y. Flat is not a second path. It is the path with nothing else covering our edits.

Props and the ground clip are the hole left when the prop depth budget left with the ray stack. `fwr_box_advance_limit` is not called. Nothing reserves a counter, a monitor, or an angled floor polygon. A quad whose anchor Y is plausible still loses texels to that mesh. Inclines hide this when the step and the actor agree. This is not fixed by another foot line.

### 2. Why plant, keep, and oval disagree by facing

Three different anchors, and only one of them knows the facing.

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

Use the pose that already matches on a slope, a stair, and mid-step. Stop maintaining a second foot line.

- Body: native billboard plus the registry Y (`sprite_y - descriptor[14]`) for every facing. No north-only pixels, no foot-line plant, no `fwr_plane_keep` on the body. Idle bob stays the engine's bob. Flat, slope, stair, and rail all take this path.
- Oval: leave the engine sample. Do not search it, do not retarget its XZ, do not assign `world.y` over it, do not run a camera keep. The slope looks seated when this sample survives. The shadow hook still submits the scene it already submits. No new GX upload.
- Sort, if the oval is still over the feet after that: the two quads share a foot and the oval is submitted later. The next change is order inside the existing passes, after one traced frame shows the depth word. No world slide, and no return of the eye-ray stack or the player write.
- Props, monitors, counters, and angled floor texels: a later depth budget against static mesh, reading bounds the way `fwr_box_advance_limit` already knows how to read them. It does not write the player, and it does not grow a per-facing table.
- Trail gap: unchanged unless a separate decision shortens gap 6.

`fwr_world_apart` stays unused by the draw. `fwr_plane_keep` leaves the draw with the oval rewrite. Full-variant land and surf keep their own calls. Base does not grow `FW_MOUNT`.

### 5. Implementation plan

Do this in three playtest steps. Each step is one path, not a facing table.

1. Body only. Remove the foot-line raise, the face-0 two pixels, and the body `fwr_plane_keep`. Keep the registry Y. Update `verify_render.py` expectations that lock the plant and the six-unit stair slide. Host math tests stay on the helpers until step 2 deletes their draw calls. Playtest the nine shots: side height against the mid-step reference, north/south after a turn, the low-feet oval, ground clip, and a stair that must stay seated.
2. Oval only, after step 1 is seated. `fwr_shadow_pass` stops moving quads. The native oval stays under the engine feet on flat, slope, and rails. Playtest front/back oval order, the rail back-sprite, and the stair. If the oval is still over the body, capture `FollowingRenderDebug` and stop. Do not add a slide in that same step.
3. Only with that trace: move the existing oval submit ahead of the body, or apply a depth word the trace actually shows. Prop and floor-mesh clips are a fourth step, using the static-bounds helper, still without writing the player.

Black-line texture lifetime stays phase 2 and is not part of footing.

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
