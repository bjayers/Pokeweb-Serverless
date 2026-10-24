# Follower render: findings and a lower-level redesign

Analysis of `runtime/following-pokemon` at `643cea36` ("manual tweaks to the shadow; other bugs introduced"), which sits on `4b09181` / `513e2d3`. No runtime behavior is changed by this note. Base package (no `FW_MOUNT`).

Playtest symptoms this note is answering:

1. Foot oval is mostly behind the sprite after `643cea36`, and still wins on a few sprites and tiles.
2. In Castelia's outer ring the player draws in front of some NPCs.
3. On specific lower-floor Pokémon Center tiles, either the oval jumps in front of the follower again, or the player hops while standing still and the follower holds still.
4. Original module, hardware only: black lines flicker across the screen on a loading-screen area change, and when the follower emote bubble appears. Emulators do not show it. Follower has to be enabled.

## 1. Findings

### What the draw path actually does

`field.s` replaces three overlay-36 calls:

| Site | Replacement | Submit |
| --- | --- | --- |
| `0x0218119a` | `FollowingDraw` → `fwr_draw` | `0x0204f685` |
| `0x021811b0` (stock) | `FollowingTerrainDraw` → `fwr_effects_draw` | `0x0204f685` again, grass quads only mutated |
| `0x0218122e` | `FollowingEffectsDraw` → `fwr_shadow_pass` then `fwfx_draw` | `0x0204f685` again, then `0x0204ebdd` for an emote or ball |

Each hook calls the native submit it replaced. The emote and ball calls are extra scenes that do not exist in the stock game. `FollowingUnload` (`FULL_COPY` of `0x021801e4`) frees follower GPU objects and then tails into `0x021801ed`.

`fwr_draw` (`render.c`) is the whole follower sort. Order on the current tip:

1. Copy the follower billboard, and the player billboard when it is a real quad (`flags & 512`, geom kind 0).
2. Mirror the player's light-mask bits onto the follower. Polygon id and the rest of the flags are left alone.
3. Optional north 2px Y tuck (`FWR_NORTH_ART_Y`) when `hold` is clear. South `+6` Z is compiled out (`faceZ = 0`).
4. `fwr_correct` chooses a policy from **world** XZ (bob is ignored there). `643cea36` then throws the result away when the corrected Y is below the Y captured before that call.
5. If the follower shadow is live (`moveflags & 0x4000`, not `0x8000`), `fwr_above_shadow` pulls the follower quad toward the eye until it is 6 world units in front of the ground point.
6. On flat ground `room` is forced to 0, so `fwr_keep_behind` does not move the follower. The whole leftover gap is given to `fwr_player_in_front`, which rewrites the **player** billboard (`position`, `sx`, `sy`) toward the camera.
7. `0x0204f685` submits that scene. NPCs, the floor, and props are sorted against whatever the player quad is during this call.
8. Both billboards are written back. There is no early return between the call and the restore.

The shadow pass runs later, on a different scene system. It guesses the follower oval by walking slots, rejects kind `> 1` and quads without `flags & 512`, rejects anything outside one XZ tile / two Y tiles of the follower ground, and rejects an oval that is nearer the player than the feet and inside 6 world units of the player. Accepted ovals are pushed with `fwr_shadow_match(..., 6 * 4096)`. `shadowTie.keep` is stored as `FWR_SHADOW_KEEP` (1 unit) and then ignored. After the eye-ray push, `out.position.y = feet.y`.

Idle hop is not implemented in this module. The native actor pass writes it into the billboard before `FollowingDraw`. Nothing here writes `player->world`, `player->drawOffset`, `player->externalOffset`, `player->moveflags`, or the player animation frame. The only player draw write is the billboard triple in `fwr_draw`.

### Symptom 2 — player in front of Castelia NPCs

**Confirmed from code.** This does not need a stuck restore.

On flat ground (`|follower.world.y - playerY| <= FWR_SHADOW_FLAT`, 2 units) `643cea36` sets `limitGround` and forces `room = 0`. Any `tuckBehind` frame whose view gap is short of `-useMargin` calls `fwr_player_in_front`. The step is the full deficit, capped by `fwr_sort_margin(camera, 0)`: 8 world units at an ordinary camera (`FWR_CAMERA_REF` = 256 units), up to one tile once the camera is farther. `fwr_cap_advance` limits the follower quad against nearby prop boxes. It is not applied to the player step, and NPCs are not props.

`0x0204f685` depth-tests the whole actor list. Moving the player quad toward the eye changes its depth against every NPC in that submit. An NPC who was only a few units nearer the camera ends up behind the player. Castelia's outer ring is flat, crowded, and often overlapping, so `tuckBehind` is true on many frames. The restore after the call puts the RAM quad back; the frame already submitted the shifted one.

`513e2d3` only advanced the player by whatever the ground cap still owed after a real follower tuck. `643cea36` advances the player by the entire margin, because the follower tuck is skipped. That is large enough to reorder nearby NPCs. A failed restore is not required and is not present in this function.

**Likely, needs playtest.** How many outer-ring NPCs flip, and whether it is only while the follower overlaps the player. Disabling the `fwr_player_in_front` block should put NPC order back immediately. If it does not, the cause is elsewhere.

### Symptom 3 — Center tiles, oval in front, player hops

**Confirmed from code, two different paths.**

Flat vs not flat is per tile. `flat` is the follower/player world-Y comparison above. A mat, counter lip, or door seam steeper than 2 units takes the other branch: `fwr_keep_behind` runs on the follower with the full margin and no ground cap, which is the sink `513e2d3` was written to stop. The oval stays on the ground. A sunk body loses to it. That matches "upper-left of the room, shadow in front again" if those tiles are the ones that fail `flat` or `standard_view` (`hold` skips the north tuck and changes `fwr_correct`).

On the flat tiles, the follower quad is not tucked. `fwr_above_shadow` still runs. Its comment and its math clamp the follower to a fixed view depth in front of the ground and scale `sx`/`sy` so the screen footprint stays put ("continuous depth clamp across bobbing frames"). Whenever the native hop would drop the quad behind that 6-unit target, the hop is removed from the submitted follower. The follower actor is still animating. The submitted quad is held still.

The player quad is the one `fwr_player_in_front` moves. The step is `current - target + 64` along the eye ray, and `current` is the follower-vs-player view gap, so it tracks the follower billboard, including whatever of the hop `fwr_above_shadow` did not clamp. `sx` and `sy` change with that step. The player's ground oval is a different scene and is not moved. World pose and moveflags stay put.

**Likely, needs playtest.** Perspective `shift_depth` is built so the projected quad does not move, and the host projection tests lock the corners. A pure perspective shift should not translate the player sprite by itself. A visible hop on those tiles means one of these, and the code cannot tell which:

- The retail billboard does not scale around the same foot anchor `shift_depth` assumes, so the world-Y term (about 0.8 of the step on the ordinary field camera) shows up on screen, while the player's oval stays on the ground. That reads as the player hopping and the follower standing.
- `tuckBehind` / overlap / the 6-unit clamp turn the step on and off as the hop crosses a threshold, so the player quad pops between two depths against the counter and the floor.
- Those tiles are not actually perspective (`camera` word 0 is treated as the projection; value 2 takes the ortho path, which moves Y and does not scale).

What is already ruled out: we do not copy the follower's animation onto the player, and we do not leave the player quad shifted for the next frame. The role reversal is real at submit time. Confirm it by watching player billboard Y/`sx`/`sy` on the bad tile with the advance removed.

The other Center failure (oval in front) is also fed by the shadow Y assignment below, on tiles where the follower was not sunk.

### Symptom 1 — oval still covers a few sprites

**Confirmed from code.** `643cea36` undoes the part of `4b09181` that was doing the work.

`4b09181` biased the oval along the eye ray by the near rim (`8 * max(sx, sy)` projected by the horizontal view component) plus the same margin as the sprite, and it stopped treating the follower's own oval as the player's when the two stand together. The screen position of an eye-ray shift does not change.

The current pass then does three things that fight that:

- `shadowTie.keep` is forced to 1 unit and never read. The pass always asks for 6 units, and `fwr_shadow_tuck` raises that again to `rim + idle margin`, capped at one tile.
- `out.position.y = feet.y` after the shift. The camera looks down (view Y is the larger component, about 0.8 on the ordinary field camera). Putting Y back gives that vertical depth back and slides the oval in XZ, so it is no longer the screen-locked shift. Wide ovals and oblique rays (a pair standing toward a corner of the room) give back more depth than the leftover XZ still buys. That is the "specific sprites / specific tiles" shape.
- South `+6` Z is off, and any `fwr_correct` that lowers Y is dropped. Some sprites therefore sit on the shadow plane instead of in front of it. The oval pass cannot fix a body that was never moved forward.

**Likely.** Species whose shadow `sx`/`sy` make the rim exceed the one-tile cap, and frames where `follower_shadow` returns 3 because the oval is nearer the player than the feet. `FollowingRenderDebug.nearWhy` (1 not a shadow, 2 too far, 3 player's oval, 4 bias declined) distinguishes these on hardware. Not confirmed which of those the remaining screenshots are.

### Symptom 4 — hardware black lines

**Confirmed from code: this is not the player-billboard path.** It is in the original module, it requires the follower, and the two moments the user named are the two moments this module touches GPU resources outside the stock submit.

Loading-screen enter/exit (not a seamless map):

- `before()` in `FollowingUpdate`, when `fwfield_owner != sys`, calls `fwfx_destroy()`. So does `FollowingUnload`. Destroy releases textures (`0x02049561`, `0x02049431`), models, and actors, then the original unload runs.
- The next field update calls `fwfx_prepare`, which loads `rom:/a/0/4/8`, recolors the palette in place, and uploads it with `0x0204e55d`.
- Texture VRAM on the DS is not double-buffered. A key freed or rewritten while the wipe's GX commands still name it is a one-frame garbage pattern. `surf.c` already documents the same class of bug for the land-to-water handoff ("VRAM key still referenced by queued land draw commands"). Emulators generally retire the frame before tearing down resources, so they stay clean.

Emote bubble:

- `fwfx_emote_begin` (`interaction.c` stage 2) loads two `rom:/following/emotes.narc` members and uploads them with `0x0204e55d` on the talk tick, which is the field update, not the draw hook.
- `draw_emote` then copies the **live field scene header** (`fwemote.scene = *scene`), points it at one private billboard and two private materials, and submits it with `0x0204ebdd` from the effects hook after the shadow pass has already called `0x0204f685`. The copied header keeps the field scene's polygon id, origin, scale, and the unchecked `rest[20]` bytes.
- `fwr_emote_place` can walk the bubble up to three steps of two tiles toward the eye, then pull it back only when the player is actually in front. That scale-up is real, but the field camera is ~234 units away, so six tiles is not by itself a near-plane clip. The extra submit and the header copy are the sharper hardware risks: a second GX begin in the same frame (polygon RAM is 2048 on hardware; overflow becomes spans), and the DS clipper drawing a full-width line from a polygon whose copied state it does not own. Software rasterizers in emulators clip or drop that polygon.

**Likely, not proven.** Which of VRAM reuse vs extra `0x0204ebdd` vs the cloned header is the line. Both triggers the user named line up with those calls, and both are absent when the follower is off. A redesign of sort does not have to change these hooks to fix Castelia or the Center. It must not add another submit or another upload in the draw hook. The black lines should be a separate, hardware-tested change: stop freeing texture keys inside the unload that races the wipe, and give the bubble its own scene header instead of cloning the field one.

### Stock sort vs what we force

The field drawer projects each billboard position and depth-tests it with everyone else in that `0x0204f685` call. Later draw wins a Z tie. There is no per-actor "draw in front of the player" flag on this path. Light-mask bits are the only flags we copy, on purpose, so polygon id stays the follower's.

An eye-ray tuck exists so the picture stays put while the depth changes. That only works for the quad we meant to reorder. Applying it to the player reorders the player against the whole map. Applying a non-ray Y write to the oval (`feet.y`) changes both depth and pixels, which is the failure mode the eye-ray helper was added to avoid.

## 2. Why this architecture fights the engine

Three stacked tricks, each compensating for the last:

**Mutate and restore.** Sort is faked by editing live billboards around one opaque submit, then putting the bytes back. Anything that submit reads — NPCs, props, the floor, the player's own oval in a later pass that copied `effectPose.playerSubmitted` — sees the lie. The restore fixes RAM and does not fix the frame. Grass (`fwr_cover_feet`) even stores the shifted player and uses it on the next hook.

**Borrow the player.** `513e2d3` did this only for the residual after the follower hit the floor. `643cea36` does it for the whole gap, on every flat `tuckBehind` frame, because `room` is hardcoded to 0 to "ignore false vertical clearance from bobbing." The follower hop is a billboard delta. Zeroing the ground room does not ignore it; it moves the compensation onto the one actor who is sorted against the crowd. The player advance also has no prop cap.

**Eye-ray tuck, then a Y patch.** Backward along a downward camera drops world Y. Capping that with `fwr_ground_back` is the right constraint. Forcing the room to 0, rejecting every `fwr_correct` that lowers Y, and then assigning `out.position.y = feet.y` on the oval are three different ways of fighting the same camera. The Y assignment is the same class of bug as the horizontal slide that read as floating beside the player: it is not on the ray, so the picture moves and the depth just bought is partly returned.

The engine's expectation is smaller than what we are doing. It wants each sprite's submitted position to be where that sprite is. Depth hacks belong on the follower quad and on an oval we know we own, and they have to stay on the ray if the picture is supposed to hold still.

## 3. Design proposals

Constraints that any of these have to keep: no player-actor writes for follower sort, follower and oval ordered explicitly, fewer per-frame world edits, compact integer C, no new heap, no floating point, no `__aeabi_ldivmod`, do not grow the 1856-byte sidecar or `FollowingSwapDebug`. Do not add a GX submit while fixing sort. Black-line work stays isolated from the sort change.

### A. One capped follower ray, no player write

Stop calling `fwr_player_in_front` from `fwr_draw`. On flat ground, compute `room` with `fwr_ground_back` again (do not force 0). Tuck the follower along the eye ray by `min(deficit, room - epsilon)`, still through `fwr_keep_behind`, still capped at one tile. If the floor binds first, leave the remaining gap alone. A short Z tie is acceptable. Reordering an NPC is not.

Shadow pass: delete `out.position.y = feet.y`. Pass the margin the sprite actually moved, and keep the `4b09181` rim boost inside `fwr_shadow_tuck`. Keep the "nearer the player than the feet" rejection so the player's oval stays his. Restore `shadowTie.keep` as that margin so the debug record matches the submit. Leave south `+6` Z off until a playtest asks for it; it is independent.

`fwr_above_shadow` should clamp from the un-bobbed anchor (world ground vs the pose with the hop subtracted) and then reapply the hop as a screen-space delta, so the follower idle hop comes back. If that is awkward, a smaller first step is to leave `fwr_above_shadow` as it is and only remove the player write; the hop can return in the same phase once Castelia is confirmed.

**Fixes.** Castelia NPC order, because the player quad is untouched. The Center role reversal, because the player quad is untouched and the follower is the quad that moves. Floor clipping, because the tuck still stops at `footY`. Most of the oval-over-sprite cases, because the oval stays on the ray and is no longer Y-snapped. Does not by itself fix a zoomed camera where `room` is 0 and the quads still Z-tie.

**Risks.** Corner overlap on a zoomed camera can come back when the floor cap binds. That is the case `643cea36` was papering over with the player. Stairs stay on the height policy (`flat` is false), which is the path that can still sink the body; keep the ground cap on that path too, or stairs regress. `fwr_correct`'s "reject if Y dropped" rule should go, or it will keep discarding the backward tuck this proposal needs. `verify_anchor.py` still owns the north foreground numbers; do not "fix" `fwr_correct` internally.

**Migration.** One change set in `fwr_draw` / `fwr_shadow_pass` / the `room == 0` block. No new hooks. Playtest Castelia outer ring, both Center tiles, a zoomed corner, stairs, and one wide species before anything else. Host `tests/test_render_math.py` still describes the ray helpers; the caller policy is what changes.

### B. Draw follower and oval after the actor list

Use the effects hook that already exists. In the actor pass, clear only the follower quad's draw bit (`flags & ~512`) for the duration of `0x0204f685`, the same way land suppresses the player body, and restore it after. In the effects pass, submit a **new** one-quad scene built from zeros plus the follower billboard, not a copy of the field scene header. Submit the oval the same way, or keep biasing the native oval only if it is the one `follower_shadow` accepted.

Order is then "map and NPCs, then follower, then oval behind the follower," owned by which submit runs last, not by edited positions. Eye-ray margin shrinks to whatever is still needed against the floor and against foliage. The player actor is never an input to sort.

**Fixes.** Leftover Z ties from A, including zoomed corners where the floor cap is 0. Oval identity, because we stop scanning slots for a quad that happens to sit near the feet. Player and NPC order stay stock.

**Risks.** This is a second GX begin, which is the same surface as the hardware black lines. It is safe only if the private scene is a zeroed header with its own polygon id, the texture keys are the follower's already-uploaded keys, and no upload happens inside the draw. Cloning `fwemote.scene = *scene` here would repeat the emote hazard. Hiding the follower during the actor pass also hides it from the grass hook unless `effectPose` still carries the pose; `fwr_effects_draw` has to keep using the saved pose. A bad private scene is a full-screen line on hardware and a clean frame on Unicorn.

**Migration.** Only after A has been playtested and a specific sprite still loses. Land and surf already call `0x0204ebdd` for a private quad; copy that shape, not the emote's header copy. Hardware-test a door transition and an emote before calling this done. Do not land it in the same change as A's sort revert.

### C. Patch submitted depth, leave positions alone

Leave every billboard's `position` / `sx` / `sy` as the native pass wrote them. After the engine has turned the quad into a polygon, add a constant to that polygon's depth so the follower clears the player and the oval clears the follower. Screen position cannot move. The floor cannot swallow the body. The player hop cannot be transferred.

**Fixes.** Every symptom that is a Z-order problem, including Castelia and the oval, without a world edit.

**Risks.** We never see the polygon. `0x0204f685` and `0x0204ebdd` are opaque. A hook inside them, or a guess at a depth word in the billboard, is how a bad command becomes the black-line bug. There is no separate sort key on the 28-byte billboard we already model (`geom`, position, frame, `sx`, `sy`, rotation, flags). Until one is found in a traced submit, this proposal is a research spike, not a patch.

**Migration.** Do not start here. If A and B both fail a hardware Z test, trace one `0x0204f685` frame and see whether depth is a field we can bias after vertex setup and before the FIFO. Budget a hardware check for every such write. No emulator result counts.

## 4. Recommended next step

Take **A**. It deletes the only player-quad write, which is the confirmed Castelia mechanism and the only mechanism that can swap who is being driven on the Center floor. It puts the oval back on a pure eye-ray bias, which is the confirmed reason a few sprites still lose to the shadow. It does not touch unload, emote upload, or `0x0204ebdd`.

Phases, still not implemented here:

1. **Sort revert (proposal A).** Remove the `fwr_player_in_front` call and the forced `room = 0`. Cap the follower tuck with `fwr_ground_back`. Remove the shadow `feet.y` assignment and honor the rim margin from `4b09181`. Drop the "reject `fwr_correct` when Y falls" guard so the capped tuck is allowed to run. Do not add hooks, fields, or submits. Playtest Castelia outer ring, both Center regions, a wide species, a zoomed corner, stairs, the post-swap ease, and the bubble.
2. **Only if a zoomed corner still ties (proposal B).** Private follower submit on the existing effects hook, zeroed scene header, no new texture upload. Hardware-test door transitions and the emote in the same pass, because this adds a GX begin.
3. **Black lines, separate from sort.** Stop `fwfx_destroy` from releasing texture keys while a wipe can still be drawing, and delay `fwfx_prepare`'s `0x0204e55d` until the new map's first frame has finished. Rebuild the emote scene as its own header instead of `fwemote.scene = *scene`. Do not fold this into phase 1. Emulator runs will stay green either way; this needs a DS.

Proposal C waits until a traced submit shows a real depth word. Opening with it is how the black-line bug gets a second source.
