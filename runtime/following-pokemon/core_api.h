#ifndef FOLLOWING_CORE_API_H
#define FOLLOWING_CORE_API_H
#include <stdint.h>
#define FWC_ABI 4u
typedef struct {
 uint32_t abi,size;
 int (*publish)(unsigned);
 void (*clear)(void);
 /* The resident token store (resident_events.h), for field state that must
  * outlive the field overlay. */
 int (*store)(unsigned kind,const void *source,unsigned size);
 int (*consume)(unsigned kind,void *destination,unsigned size);
} FwcApi;
extern const FwcApi FollowingCoreAPI;
#endif
