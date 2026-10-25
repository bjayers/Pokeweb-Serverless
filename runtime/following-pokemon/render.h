#ifndef FOLLOWING_RENDER_H
#define FOLLOWING_RENDER_H
#include "native.h"
typedef struct { Vec position; int16_t sx,sy; } FwrEffectPose;
/* Native BlAct MatFlags bits 12–15 select the map's enabled lights. */
#define FWR_LIGHT_MASK 0xf000u
/* Directional ground-plane anchor: shared by native sprite and shadow draws. */
void fwr_anchor(Actor *actor,unsigned face);
void fwr_draw(void *system,void *camera,void *light,Actor *follower,Actor *player,int sprite_y);
/* Swap and post-swap ease, for the depth blend. Zeros when neither is active.
 * `traveled` is world units along the ease, and only while an ease is active. */
void fw_depth_query(int *swapping, int *easing, uint32_t *traveled);
/* Submit field+0xc4 before the actor scene when it is this field's shadow
 * system. Returns 1 when that submit ran. The later shadow hook must not
 * submit the oval scene again. */
int fwr_shadow_before_actors(void *actorBl,void *camera,void *light,Actor *follower);
/* Draw the secondary pass. Skips the native submit when the oval scene was
 * already submitted before the bodies. Does not move the oval. */
void fwr_shadow_pass(void *system,void *camera,void *light);
/* Last submitted body, including registry Y and the two-pixel lift.
 * `live` is the quad at the call. An exact native match returns that pose.
 * A quad that has moved since the capture keeps the same draw-only Y lift
 * instead of dropping back to the unlifted foot. A moved actor returns 0. */
int fwr_effect_pose(Actor *actor,const Vec *live,FwrEffectPose *out);
int fwr_grass_work_valid(const void *work);
void fwr_grass_clear(void);
void fwr_effects_draw(void *system,void *camera,void *light,Actor *follower);
#endif
