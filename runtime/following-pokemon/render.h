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
/* Draw the secondary pass, biasing the follower shadow behind its sprite. */
void fwr_shadow_pass(void *system,void *camera,void *light);
/* Recall reuses the last submitted pose, after draw-only ground/depth fixes. */
int fwr_effect_pose(Actor *actor,const Vec *native,FwrEffectPose *out);
int fwr_grass_work_valid(const void *work);
void fwr_grass_clear(void);
void fwr_effects_draw(void *system,void *camera,void *light,Actor *follower);
#endif
