#ifndef FW_EFFECTS_H
#define FW_EFFECTS_H
#include "native.h"
void fwfx_set_resource_count(unsigned count);
void fwfx_prepare(Actor *actor);
void fwfx_snapshot(Actor *actor);
void fwfx_out(Actor *actor);
void fwfx_recall(Actor *actor);
void fwfx_tick(void);
void fwfx_cancel(void);
void fwfx_destroy(void);
/* Effect-bank upload while a door or return fade is up. Not a billboard submit,
 * and not an emote VRAM bind. The talk path owns that material. */
void fwfx_warm(uint32_t heap);
int fwfx_hides_actor(void);
int fwfx_busy(void);
int fwfx_recalling(void);
int fwfx_available(void);
void fwfx_draw(void *camera,void *light);
/* Player used only to keep a reaction bubble behind someone standing in front. */
void fwfx_set_player(Actor *player);
/* Private reaction emotes; frame resources never alias a native actor palette. */
int fwfx_emote_begin(Actor *actor,unsigned member);
void fwfx_emote_frame(Actor *actor,unsigned frame);
void fwfx_emote_end(void);

#endif
