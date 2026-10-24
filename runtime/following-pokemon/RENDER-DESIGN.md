# Follower render: world feet

Shipped on `cursor/following-fixes-852f`. This replaces the eye-ray sort stack (`fwr_correct` in the draw path, `fwr_above_shadow`, `fwr_keep_behind`, `fwr_player_in_front`, and the shadow `position.y = feet.y` patch). The trail, swap, ease, rail, recall, and talk code are unchanged.

## What the frame does now

The follower actor's world pose is the placement. Trail, swap, ease, and rails write that pose the way they write an NPC. `fwr_draw` does not move the player, and it does not slide either quad along the camera ray.

Submitted follower quad:

- Start from the native billboard, which already includes the world pose and the idle hop.
- Apply the registry Y (`sprite_y` minus descriptor byte 14) on this quad only. North-facing art also drops two pixels (`FWR_NORTH_ART_Y`), in world Y, for every camera. South does not add Z.
- If the draw is idle (not a swap and not the post-swap ease), the screen rects overlap, and the view gap is inside `(-FWR_WORLD_NUDGE, FWR_CLEAR_FRONT)`, slide the follower 8 world units further from the player along horizontal XZ. Y, including the hop, stays. The tie test uses terrain Y, so the hop cannot toggle the slide. A pair already stacked uses the follower's back-facing direction.
- Copy the player's light-mask bits onto the follower for the submit, then restore the follower quad. The player billboard, world, moveflags, and animation are not written.

`policy` in `FollowingRenderDebug` is 0 for feet-only and 2 when that nudge ran. `before` / `after` / `depthRel` are view gaps. `hold` is always 0. The debug record layout is unchanged.

Shadow pass (`fwr_shadow_pass`):

- The oval is still found by the old window (one XZ tile and two Y of the actor ground, and not the oval nearer the player).
- `fwr_shadow_on_plane` plants it on the submitted body's XZ and the actor's terrain Y, then slides the center away from the camera along the ground by the near rim (`8 * max(sx, sy)`, capped at one tile). Scale is unchanged. Y is not assigned again after that slide.

`fwr_world_apart` and `fwr_shadow_on_plane` are the new helpers. `fwr_player_in_front` is gone. The older ray helpers remain for the emote bubble, grass, and the full-variant land/surf draws. The base draw path does not call them.

The prop walk that used to push quads behind furniture is gone with the ray advance it was capping. A follower is no longer driven through a stair wall to win a Z test, and the player is no longer moved to clear a trashcan.

## Why the ray stack lost

Gen 5 sorts a billboard by the position it submits. The old path edited the follower, and then the player, along the eye ray so the picture would stay put while the depth changed, then wrote the bytes back after `0x0204f685`. NPCs in that submit were compared to the shifted player. On flat ground `643cea36` forced the follower's ground room to 0, so the whole gap was the player advance. That is enough for the player to draw over nearby Castelia NPCs. It is also the only write that can make the player quad hop while the follower is clamped still. Restoring RAM after the call does not undo the frame.

A downward camera turns a backward ray into a drop in world Y. Capping that, then assigning the oval's Y back to the feet, fought the same ray and put the oval over the lower body on wide sprites and oblique tiles.

Those bugs are structurally gone: there is no player write, and the oval never leaves the ground plane.

## Behaviors left on the existing owners

- Follow spacing, the post-swap ease (`FW_EASE_*`, about two tiles of travel), and the close face-in stay in the trail. The draw nudge is skipped while a swap or ease is active so it does not pry that pose apart.
- Rail facing, talk reach, and the swap finish stay in the rail and interaction code.
- NPC contact recall, seamless-transition recall, and the intentional drops stay in the scene policy.
- Unown form 27 and Arceus form 0 stay in the form table.
- Grass still uses `fwr_cover_feet` on the saved submitted pose.

Stairs, mats, and Center lips use the actor's Y. There is no flat-versus-slope ray branch. A follower standing higher on a stair is closer to a downward camera and can sort in front, the same way an NPC on that step does.

## Black lines

Not fixed on hardware. The draw path does not add a GX submit or a texture upload.

What did change: `fwfx_emote_begin` no longer copies the live field scene (`fwemote.scene = *scene`). The bubble keeps that scene's heap id and scale, zeroes the rest, and sets polygon id 1. The submit is still the one existing `0x0204ebdd` call.

Still phase 2, and it needs a DS: `fwfx_destroy` / `fwfx_prepare` still release and re-upload texture keys across a loading wipe, and the recall flash still clones a field scene header. Emulators retire that frame cleanly. Do not fold another submit into the sort path to chase it.

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
