#ifndef FOLLOWING_RENDER_MATH_H
#define FOLLOWING_RENDER_MATH_H
#include "following.h"
/* Camera and positions use native 20.12 world units; scale uses native fx16. */
typedef struct { FwPoint eye,target; uint32_t projection; } FwrCamera;
typedef struct { FwPoint position; int32_t sx,sy; } FwrPose;
typedef struct { FwPoint axis; int32_t before,after,policy; } FwrResult;
#define FWR_TIE_MARGIN 512 /* 1/8 world unit; a tile is 16 world units. */
#define FWR_PROP_FOREGROUND_GAP (4 * 4096) /* Clear native Z-buffer quantization. */
#define FWR_NATIVE_FOREGROUND_GAP (4 * 4096) /* Retain native NPC/player depth. */
#define FWR_LARGE 1u
#define FWR_NORTH 2u
#define FWR_NORTH_ANCHOR_Z 7
#define FWR_SOUTH_ANCHOR_Z 6
#define FWR_NORTH_ART_Y 2
/* One world unit is one native Z step at the field camera. The old four-unit
 * target pushed the quad through its shadow and through nearby stair walls. */
#define FWR_SORT_BEHIND (1 * 4096)
/* One tile covers shadow clearance and a step of height. A wilder pose stops
 * here instead of diving through the wall behind the player. */
#define FWR_BEHIND_CAP (16 * 4096)
/* Ordinary field cameras sit inside this eye distance. Past it the
 * behind-target grows with (distance/ref)^2, matching the drop in native
 * Z resolution, and stops at one tile. */
#define FWR_CAMERA_REF (256u * 4096u)
/* Swap crossing and the close post-swap pose. Blends back to the default. */
#define FWR_SORT_STRONG (8 * 4096)
/* View depth past which the follower is nearer the camera. The close-range
 * blend must not pull that sprite behind the player. */
#define FWR_CLEAR_FRONT (4 * 4096)
/* A biased shadow sits one unit further than the sprite it belongs to. */
#define FWR_SHADOW_KEEP (1 * 4096)
/* View-depth band where an overlapping pair still needs a tie break.
 * The slide itself is FWR_PLANE_CAP along the ground, not this wide. */
#define FWR_WORLD_NUDGE (8 * 4096)
/* Ground XZ keep that puts the center about four view units behind the
 * feet at an ordinary field camera, and never as far as half a tile.
 * Screen half-width (8*sx) is not a world distance. */
#define FWR_PLANE_CAP (6 * 4096)
/* Native flat shadows sit about one unit above the ground anchor. A larger
 * gap is a stair step, not that lift. */
#define FWR_SHADOW_FLAT (2 * 4096)
#define FWR_FORCE_FRONT 4u
#define FWR_FORCE_BACK 8u
#define FWR_BACK_MARGIN (12 * 4096)
int fwr_correct(const FwPoint *world,const FwPoint *player_world,
    const FwrPose *native,const FwPoint *player_draw,const FwrCamera *camera,
    unsigned flags,FwrPose *output,FwrResult *result);
/* Keep the opaque billboard in front of its ground shadow without moving its
 * projected image. The shadow itself remains at the native ground anchor. */
int fwr_above_shadow(const FwrPose *native,const FwPoint *ground,
    const FwrCamera *camera,FwrPose *output);
/* Push only the follower quad back along the eye ray until it is `margin`
 * behind the player. `cap` limits that backward move. The player quad, shadow
 * anchor, and logical world position stay where they are. */
int fwr_keep_behind(const FwrPose *follower,const FwrPose *player,
    const FwrCamera *camera,int32_t margin,int32_t cap,FwrPose *output);
/* How far, in view depth, the anchor can move backward along the eye ray
 * before its Y crosses `foot_y`. The ray points down, so this is the clip limit. */
int32_t fwr_ground_back(const FwrPose *pose,const FwrCamera *camera,int32_t foot_y);
/* Eye-to-target length in 20.12. 0 when the camera has no separation. */
uint32_t fwr_view_distance(const FwrCamera *camera);
/* Margin for fwr_keep_behind. `ease_s` is a smoothstep in 0..65536: 0 while
 * the sprites cross or the follower holds the close pose, 65536 when no swap
 * or ease is active. The strong margin blends to the default, so the last
 * eased frame matches an idle draw. Farther than FWR_CAMERA_REF, both ends
 * scale by (distance/ref)^2 and stay within one tile. */
int32_t fwr_sort_margin(const FwrCamera *camera, uint32_t ease_s);
/* View depth of `point` minus `origin`, in 20.12. Positive is nearer the
 * camera. 0 when the camera or the span is unusable. */
int32_t fwr_view_gap(const FwPoint *point, const FwPoint *origin, const FwrCamera *camera);
/* 1 when the two camera-facing quads overlap on screen. Extents are the
 * native sprite footprint, +/-8*sx and 0..16*sy from each foot anchor. */
int fwr_sprites_overlap(const FwrPose *a, const FwrPose *b, const FwrCamera *camera);
/* Margin for an overlapping pair on flat ground. Idle draws upgrade to the
 * swap margin when the follower is not clearly nearer. A swap or ease keeps
 * its own blend, and a height change (stairs, slopes) keeps that blend too. */
int32_t fwr_follow_margin(const FwrCamera *camera, uint32_t ease_s, int32_t depth_rel, int overlap, int flat);
/* 1 when the follower quad should be pushed behind the player. `policy` is
 * fwr_correct's order. Clearly nearer stays in front. A front policy that is
 * already nearer, even inside the clear-front band, is not pulled back while
 * idle. Horizontal-behind order still tucks, which is the corner step. */
int fwr_follow_tuck(int policy, int32_t depth_rel, int32_t clear, int easing, int overlap, int flat);
/* View depth, in 20.12, that counts as clearly nearer the camera. */
int32_t fwr_clear_front(const FwrCamera *camera);
/* Push a camera-facing shadow along the eye ray until it sorts just behind
 * its sprite. Screen position and size stay put. */
int fwr_shadow_behind(const FwrPose *shadow,const FwPoint *sprite,
    const FwrCamera *camera,FwrPose *output);
/* Same bias, with an explicit keep. `keep` is the follower's sort margin:
 * one unit at rest, and the swap margin while a tuck has pulled the sprite
 * back through the oval. The shadow is not moved on screen. */
int fwr_shadow_tuck(const FwrPose *shadow,const FwPoint *sprite,
    const FwrCamera *camera,int32_t keep,FwrPose *output);
/* Feet check from fwr_shadow_on_feet, then fwr_shadow_tuck. */
int fwr_shadow_match(const FwrPose *shadow,const FwPoint *feet,
    const FwPoint *sprite,const FwrCamera *camera,int32_t keep,FwrPose *output);
/* Depth-bias the native shadow behind the sprite. The shadow is not moved
 * onto `feet`: that point only has to be near the oval. Screen position stays
 * on the native ground sample. Returns 1 when the bias runs. */
int fwr_shadow_on_feet(const FwrPose *shadow,const FwPoint *feet,
    const FwPoint *sprite,const FwrCamera *camera,FwrPose *output);
/* Push this ground oval away from the camera by its near-rim depth plus
 * eight world pixels, then stop at one tile. Perspective scale keeps the
 * picture on the ground sample. Orthographic and unknown cameras are left
 * alone. Does not follow the body. */
int fwr_oval_behind(const FwrPose *shadow,const FwrCamera *camera,FwrPose *output);
/* 60-byte private billboard header for a second GX submit. Copies heap,
 * scale, and the four light colors from the live field scene. Zeroes the
 * 20-byte tail and does not copy the live polygon id: that pair is the 3DS
 * black-line state, which emulators do not keep. Polygon id is 1, or 2 when
 * the live scene is already 1. A later hardware pass changes this function
 * only. `actors` and `materials` are stored as 32-bit addresses. */
void fwr_private_scene(void *dst,const void *live,const void *actors,const void *materials,
                       uint16_t actorCount,uint16_t materialCount);
/* Camera-facing grass clears its owner, but stays behind a nearer body. */
int fwr_cover_feet(const FwrPose *grass,const FwrPose *body,
    const FwrPose *other,const FwrCamera *camera,FwrPose *output);
/* Raise an attached bubble in the camera plane, preserving horizontal center
 * and depth even with a perspective camera. Height is in world 20.12 units. */
int fwr_head_position(const FwPoint *base,const FwrCamera *camera,
    int32_t height,FwPoint *output);
/* Pull a head-attached bubble toward the camera along the eye ray so map
 * geometry behind the follower cannot depth-test it away. Screen position
 * stays put. When `player` is at least half a tile closer than the bubble,
 * the bubble stops one unit behind that player instead of covering them.
 * `player` may be null. */
int fwr_emote_place(const FwrPose *bubble,const FwPoint *player,
    const FwrCamera *camera,FwrPose *output);
/* Slide `follower` further from `player` by `step` along horizontal XZ.
 * Y is copied. A pair closer than one unit uses `face`'s back direction
 * (0 north, 1 south, 2 west, 3 east). This is a world move, not a camera ray. */
int fwr_world_apart(const FwPoint *follower,const FwPoint *player,unsigned face,
    int32_t step,FwPoint *output);
/* Move `feet` away from the camera along the ground until it is `view_keep`
 * view-units further, or FWR_PLANE_CAP world units, whichever is smaller.
 * Y is copied. A camera with no horizontal axis leaves the point planted. */
int fwr_plane_keep(const FwPoint *feet,const FwrCamera *camera,int32_t view_keep,
    FwPoint *output);
/* Plant a ground oval on `feet` (terrain Y) and apply fwr_plane_keep.
 * Scale is unchanged. The oval does not leave the ground plane, and its
 * size does not change how far it slides. */
int fwr_shadow_on_plane(const FwrPose *shadow,const FwPoint *feet,
    const FwrCamera *camera,FwrPose *output);
/* A loaded static model may occlude part of the original sprite. Reserve its
 * complete projected bounding volume before advancing a camera-facing quad. */
/* A camera-facing quad seen from `camera`: feet, world half-width and world
 * height (2 * scene scale * sy; the field actor scene is 7.0). */
typedef struct { FwPoint feet; int32_t half,height; } FwrBody;
/* The top row of a 56-unit body with a quarter forward lean is about 74
 * units forward at a 44° camera. Past this a steep camera's top rows lean
 * back again. */
#define FWR_SPLIT_CAP (5 * FW_TILE)
/* Where the strips' top edges stand, along the ground in front of the
 * hinge `safe` world units up the quad: `forward` world units, ramped in over
 * `ramp` units of height, plus `lean` (20.12, 0..1) per unit of height. */
typedef struct { int32_t safe,lean,forward,ramp; } FwrStack;
/* Nearby billboards that can hold a strip back. */
#define FWR_OTHERS_MAX 16u
/* Horizontal strips of a tall body for their own submit. `body` is the
 * submitted camera-facing quad (foot anchor), `half` its world half-width.
 * Strip i spans top - (i+1)*strip .. top - i*strip world units up the camera
 * plane with scale `strip_sy`. Each moves toward the camera along its own eye
 * ray until its top edge stands where `stack` says, capped at FWR_SPLIT_CAP
 * of depth, with its screen pixels unchanged. A billboard in `others` (the
 * player, NPCs) that a strip covers on screen keeps that strip
 * FWR_SORT_BEHIND behind it; one level with the follower holds the strip
 * still; one clearly behind does not count. With `lift` (may be null, one
 * per other) only a sprite's bottom quarter, its feet, holds strips back;
 * `lift[k]` gets the pull that keeps the rest of sprite k FWR_SORT_BEHIND in
 * front of every strip covering it, 0 when none does. `limited` (may be
 * null) gets a bit per strip that was held back. The camera basis is built once for all
 * strips. Returns 0 for an unknown projection, a vertical camera or an
 * unusable pose. */
int fwr_strips(const FwrPose *body,const FwrCamera *camera,int32_t half,int32_t top,int32_t strip,
               unsigned count,const FwrStack *stack,int32_t strip_sy,const FwrBody *others,unsigned other_count,
               FwrPose *output,uint16_t *limited,int32_t *lift);
/* The camera-facing basis fwr_strips uses, built once per frame. */
typedef struct { FwPoint eye,view,right,up,toward; int32_t flat,upToward; int ortho; } FwrBasis;
int fwr_basis(const FwrCamera *camera,FwrBasis *output);
/* The band of `body` from `low` world units up its camera plane, with scale
 * `band_sy`, moved `pull` toward the eye along its ray with its pixels
 * unchanged. Redraws a sprite's upper rows over the strips that cover it. */
int fwr_band(const FwrBasis *basis,const FwrPose *body,int32_t low,int32_t pull,int32_t band_sy,FwrPose *output);
/* View depth of `point` past `origin` along the basis, 20.12; positive is
 * nearer the camera. 0 when the span is unusable. */
int32_t fwr_basis_gap(const FwrBasis *basis,const FwPoint *point,const FwPoint *origin);
/* 1 when band a0..a1 of camera-facing quad `a` and band b0..b1 of `b`
 * (world units up each camera plane) overlap on screen. */
int fwr_bands_overlap(const FwrBasis *basis,const FwrBody *a,int32_t a0,int32_t a1,
                      const FwrBody *b,int32_t b0,int32_t b1);
int32_t fwr_box_advance_limit(const FwrPose *body,const FwrCamera *camera,
    const FwPoint corners[8]);
int fwr_cap_advance(const FwrPose *baseline,const FwrPose *adjusted,
    const FwrCamera *camera,int32_t limit,FwrPose *output);
#endif
