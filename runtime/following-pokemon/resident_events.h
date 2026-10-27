#ifndef FOLLOWING_RESIDENT_EVENTS_H
#define FOLLOWING_RESIDENT_EVENTS_H
#include <stdint.h>
#define FWRE_ABI 1u
enum {
    FWRE_RESTORE_TOKEN = 1,
    FWRE_MOUNT_TOKEN = 2,
    FWRE_SUMMON_TOKEN = 3,
    FWRE_RESTORE_BYTES = 60,
    FWRE_MOUNT_BYTES = 44,
    FWRE_SUMMON_BYTES = 16
};
/* Why the next field places the follower at once. The field module reloads
 * with its overlay, so this lives here. Valid from boot as source 1. */
typedef struct {
    uint32_t source, zone;
    int32_t x, z;
} FwreSummon;
enum { FWRE_SUMMON_BOOT = 1, FWRE_SUMMON_MENU = 3, FWRE_SUMMON_BATTLE = 5 };
typedef struct {
    uint32_t abi, size;
    unsigned (*opcode)(void *);
    int (*callback)(void *);
    void (*freeEvent)(void *);
    void (*freeVm)(void *);
} FwrEvents;
int FollowingResidentEventBind(const FwrEvents *hooks);
void FollowingResidentEventClear(void);
int FollowingResidentTokenStore(unsigned kind, const void *source, unsigned size);
int FollowingResidentTokenConsume(unsigned kind, void *destination, unsigned size);
void FollowingResidentTokenDiscard(unsigned kind);
#endif
