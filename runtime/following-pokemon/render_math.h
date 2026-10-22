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
/* Preserve the player's foreground after follower shadow clearance without
 * putting the follower behind its own shadow or moving either image on screen. */
int fwr_player_in_front(const FwrPose *follower,const FwrPose *player,
    const FwrCamera *camera,FwPoint axis,int32_t margin,int32_t max_advance,
    FwrPose *output,int32_t *after);
/* A loaded static model may occlude part of the original sprite. Reserve its
 * complete projected bounding volume before advancing a camera-facing quad. */
int32_t fwr_box_advance_limit(const FwrPose *body,const FwrCamera *camera,
    const FwPoint corners[8]);
int fwr_cap_advance(const FwrPose *baseline,const FwrPose *adjusted,
    const FwrCamera *camera,int32_t limit,FwrPose *output);
#endif
