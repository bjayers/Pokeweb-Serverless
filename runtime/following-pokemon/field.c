#include "native.h"
#include "effects.h"
#include "interaction.h"
#include "scene.h"
#include "core_api.h"
#include "registry.h"
#include "render.h"
#include "positioning.h"
#ifdef FW_MOUNT
#include "land.h"
#include "transition.h"
#endif
#ifdef FW_ARCEUS_SURF
#include "surf.h"
#endif
#define API __attribute__((visibility("default")))
#define USE 1u
#define HIDDEN 4u
#define PAUSE_ANM 16u
#define NONBLOCKING (128u|256u|32768u)
#define NO_QUERY (512u|1024u)
#define KEEP_ZONE 32u
#define NOT_SAVE 1048576u
#define NO_EFFECT 2097152u
#define SHADOW_SET 16384u
#define SHADOW_VANISH 32768u
/* No retail structure owns this state. It is discarded with overlay 36. */
static FwFollower fwfield_follower;
static ActorSystem *fwfield_owner;
static Actor *fwfield_player;
static uint32_t fwfield_tick, fwfield_generation;
static uint16_t fwfield_code;
static uint8_t fwfield_flying;
static uint32_t fwfield_registryCrc;
static uint16_t fwfield_descriptorCount;
#ifdef FW_MOUNT
static uint8_t fwfield_landCarry,fwfield_landCheckAfterPause;
static uint32_t fwfield_landToggleTick;
#endif
/* Grid followers bypass the retail movement process. Remember only the last
 * visible tile whose field effect was checked; this is not save state. */
static Actor *fwfield_terrainActor;
static void *fwfield_terrainController;
static int16_t fwfield_terrainX,fwfield_terrainZ;
static uint16_t fwfield_terrainZone;
/* Last player zone whose trail samples share the current coordinate frame.
 * Not save data. A seamless connection that jumps more than one tile
 * translates the trail instead of clearing it. */
static uint16_t fwfield_trailZone;
static uint8_t fwfield_trailZoneValid;
/* 1 after a draw has seen this actor unhidden. The update that first clears
 * HIDDEN still has 0 here, which is the body's first raster. */
static uint8_t fwfield_shownNow;
static uint8_t fwfield_reveal;
typedef struct {
 uint32_t reserved[2];uint16_t oldTerrain,terrain;
 uint32_t extra[2];void *controller;
} FwShoalContext;
_Static_assert(sizeof(FwShoalContext)==24 && offsetof(FwShoalContext,terrain)==10 &&
               offsetof(FwShoalContext,controller)==20,"stock shoal context offsets");
static int fwfield_configState,fwfield_eventsReady;
/* A manual choice lives only in the field runtime; party data is untouched.
 * Keep the previous safe follower tile for the recall -> send-out sequence. */
static Vec fwfield_cyclePose;
static int16_t fwfield_cycleGrid[3];
static uint16_t fwfield_cycleFace;
static uint8_t fwfield_cyclePending;
#ifdef FW_ARCEUS_SURF
static uint32_t fwfield_surfAppearance;
#endif
static FweRestore fwfield_restore;
#define REGISTRY_MAX (32u+FW_MAX_ROWS*24u)
static uint8_t fwfield_page[FW_REGISTRY_PAGE];
static uint16_t fwfield_index[FW_MAX_SPECIES+2];
static unsigned fwfield_registrySize,fwfield_stride,fwfield_pageFirst,fwfield_pageCount;
/* FWCG v2: stage, total registry bytes, cache capacity, native read count. */
API volatile struct {uint32_t magic,version,stage,registryBytes,cacheBytes,reads;} FollowingConfigDebug={.magic=0x47435746,.version=2,.cacheBytes=FW_REGISTRY_PAGE};
/* Searchable emulator diagnostics; values never become save data. */
API volatile struct { uint32_t magic,version,ticks,spawns,deletes,reason,species,fwfield_code,actor,fwfield_player,visible,resets,system,systemFlags,field,cameraYaw,cameraPitch,depthClass,depthX,depthY,depthZ,modelSize; } FollowingDebug={.magic=0x47445746,.version=2};
API volatile struct { uint32_t magic,version,attempts,attached,actor,playerFlags,actorFlags; } FollowingShadowDebug={.magic=0x48535746,.version=1};
/* FWSW: active during a reverse step, face applied this frame, progress along
 * the vacated tile, blocked when that tile was not enterable. */
API volatile struct { uint32_t magic,version,active,face,progress,blocked; } FollowingSwapDebug={.magic=0x57535746,.version=1};
static struct { uint8_t active,departed,rail; uint16_t savedFace; FwPoint follower,origin,player; } swapOn;
/* Post-swap gap. Not sidecar state: the 1856-byte follower is fixed, and a
 * reload drops this with the overlay. `close_keep` is the metric of the pose
 * the swap actually reached; travel grows it to one tile. */
static struct { uint8_t active; uint32_t traveled, close_keep; FwPoint last; } ease;
static Vec settled;
static uint8_t haveSettled,haveLast;
static void clear_swap(void){swapOn.active=swapOn.departed=swapOn.rail=0;FollowingSwapDebug.active=FollowingSwapDebug.progress=0;}
static void clear_ease(void){ease.active=0;ease.traveled=ease.close_keep=0;}
void fw_depth_query(int *swapping,int *easing,uint32_t *traveled){
 if(swapping)*swapping=swapOn.active?1:0;
 if(easing)*easing=ease.active?1:0;
 if(traveled)*traveled=ease.active?ease.traveled:0;
}
static void drop_settled(void){haveSettled=0;haveLast=0;}
static void cleanup(void);
static void clear_restore(void){
 fwfield_restore.magic=0;fwfield_restore.player=(Vec){0,0,0};fwfield_restore.follower=(Vec){0,0,0};
 fwfield_restore.grid[0]=fwfield_restore.grid[1]=fwfield_restore.grid[2]=0;
 fwfield_restore.face=fwfield_restore.zone=0;
}
static void noop(Actor *a) { (void)a; }
static void removed(Actor *a) {
 if ((uintptr_t)a==fwfield_follower.actor) fwt_cancel();
#ifdef FW_MOUNT
 if ((uintptr_t)a==fwfield_follower.actor) fwland_end();
#endif
 if ((uintptr_t)a==fwfield_follower.actor) { fwfield_follower.actor=0; fwfield_follower.state=FW_ABSENT; if(!fwfield_cyclePending)fw_trail_clear(&fwfield_follower.trail); clear_ease(); FollowingDebug.actor=0; FollowingDebug.visible=0; ++FollowingDebug.deletes; }
}
/* The actor renderer advances the imported directional animation itself.  Its
 * pause flag belongs to presentation only: never advance it while an owned
 * interaction, external safe scene or ball effect owns the follower. */
static void animation_gate(Actor *a) {
 if(fw_idle_animation_enabled(fwfield_follower.state,!(a->flags&HIDDEN),fwfx_busy()))a->flags&=~PAUSE_ANM;
 else a->flags|=PAUSE_ANM;
}
static int coord_ok(int64_t v){return v>=INT32_MIN&&v<=INT32_MAX;}
static int add_coord(Vec *v,int32_t dx,int32_t dy,int32_t dz){
 int64_t x=(int64_t)v->x+dx,y=(int64_t)v->y+dy,z=(int64_t)v->z+dz;
 if(!coord_ok(x)||!coord_ok(y)||!coord_ok(z))return 0;
 v->x=(int32_t)x;v->y=(int32_t)y;v->z=(int32_t)z;return 1;
}
/* Same disconnect test as the trail: more than one tile in XZ, or two in Y. */
static int connection_jump(int64_t dx,int64_t dy,int64_t dz){
 int64_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy,az=dz<0?-dz:dz;
 if(ay>2*(int64_t)FW_TILE||ax>2*(int64_t)FW_TILE||az>2*(int64_t)FW_TILE)return 1;
 /* Match trail isqrt: a gap disconnects only once the distance exceeds one tile. */
 int64_t limit=(int64_t)FW_TILE+1;
 return ax*ax+az*az>=limit*limit;
}
/* A zone change with a large coordinate jump is a seamless connection, not a
 * warp. Shift the recorded route (and the follower, if it is still live) by
 * that delta. Door warps rebuild the actor system and still drop the trail. */
static int rebase_connection(Actor *a,Vec *pose,const FwSample *sample){
 if(!fwfield_trailZoneValid){fwfield_trailZone=fwfield_player->zone;fwfield_trailZoneValid=1;return 1;}
 if(fwfield_player->zone==fwfield_trailZone)return 1;
 fwfield_trailZone=fwfield_player->zone;
 FwTrail *t=&fwfield_follower.trail;
 if(!t->count)return 1;
 const FwSample *last=&t->samples[(t->head+t->count-1u)%FW_TRAIL_CAPACITY];
 int64_t dx=(int64_t)sample->x-last->x,dy=(int64_t)sample->y-last->y,dz=(int64_t)sample->z-last->z;
 if(!connection_jump(dx,dy,dz))return 1;
 if(!coord_ok(dx)||!coord_ok(dy)||!coord_ok(dz)){fw_trail_clear(t);return 0;}
 Vec savedActor={0},savedPose={0};
 if(a)savedActor=a->world;
 if(pose)savedPose=*pose;
 if(a&&!add_coord(&a->world,(int32_t)dx,(int32_t)dy,(int32_t)dz)){fw_trail_clear(t);return 0;}
 if(pose&&!add_coord(pose,(int32_t)dx,(int32_t)dy,(int32_t)dz)){if(a)a->world=savedActor;fw_trail_clear(t);return 0;}
 if(!fw_trail_translate(t,(int32_t)dx,(int32_t)dy,(int32_t)dz)){if(a)a->world=savedActor;if(pose)*pose=savedPose;return 0;}
 if(a){
  for(unsigned i=0;i<3;++i)a->previous[i]=a->grid[i];
  a->grid[0]=(int16_t)(a->world.x/FW_TILE);a->grid[1]=(int16_t)(a->world.y/FW_TILE);a->grid[2]=(int16_t)(a->world.z/FW_TILE);
 }
 return 1;
}
static void player_sample(FwSample *sample){
 *sample=(FwSample){.x=fwfield_player->world.x,.y=fwfield_player->world.y+fwfield_player->drawOffset.y+fwfield_player->externalOffset.y,
  .z=fwfield_player->world.z,.generation=fwfield_generation,.space=1,.rail=0,.connection=0,.direction=(uint8_t)fwfield_player->face,.kind=FW_WORLD,.duration=1};
}
static int centered(const Vec *v){return ((uint32_t)v->x&0xffffu)==0x8000u&&((uint32_t)v->z&0xffffu)==0x8000u;}
static int same_tile(int32_t x,int32_t z,int32_t ox,int32_t oz){return fw_tile(x)==fw_tile(ox)&&fw_tile(z)==fw_tile(oz);}
static int on_rail(void){return (fwfield_player->flags&8192)&&PTR(fwfield_player->moveWork,0);}
/* Grid centers still settle immediately. A rail point is almost never a
 * tile center, so the previous frame's rail position is the step origin.
 * The first frame only records that a position exists. */
static void note_settled(void){
 if(swapOn.active)return;
 if(centered(&fwfield_player->world)){settled=fwfield_player->world;haveSettled=1;}
 else if(on_rail()&&haveLast){settled=fwfield_player->world;haveSettled=1;}
 haveLast=1;
}
static uint32_t swap_progress(const FwPoint *from,const FwPoint *to){
 int64_t dx=(int64_t)to->x-from->x,dz=(int64_t)to->z-from->z;
 if(dx<0)dx=-dx;
 if(dz<0)dz=-dz;
 uint64_t p=(uint64_t)dx+(uint64_t)dz;return p>0xffffffffu?0xffffffffu:(uint32_t)p;
}
static void bind_pose(Actor *a,int32_t x,int32_t y,int32_t z,unsigned face){
 a->world=(Vec){x,y,z};
 for(unsigned i=0;i<3;++i)a->previous[i]=a->grid[i];
 a->grid[0]=(int16_t)(x/(int32_t)FW_TILE);a->grid[1]=(int16_t)(y/(int32_t)FW_TILE);a->grid[2]=(int16_t)(z/(int32_t)FW_TILE);
 CALL(0x02167099,void(*)(Actor*,uint32_t))(a,face);
 fwr_anchor(a,face);
 a->moveflags&=~(2048u|4096u);
 FollowingSwapDebug.face=face;
}
static void recall_blocked(Actor *a){
 if(!(a->flags&HIDDEN))fwfx_recall(a);
 a->flags|=HIDDEN;fwfield_follower.state=FW_WAITING;FollowingDebug.visible=0;
 fw_trail_clear(&fwfield_follower.trail);++FollowingDebug.resets;
 clear_swap();clear_ease();drop_settled();FollowingSwapDebug.blocked=1;
}
/* 1 when `natural` is strictly closer than a fresh load, along the reseeded
 * trail. The follower turns in place on that pose, or on the start floor when
 * the mirrored end is closer than that. The next walk eases from there. */
static int ease_from(const FwPoint *natural,const FwPoint *player,FwSample *start){
 uint32_t close=fw_trail_metric_to_point(&fwfield_follower.trail,fwfield_follower.directional_gap,natural->x,natural->z);
 if(!close||close>=FW_TILE){clear_ease();return 0;}
 if(close<FW_EASE_START_KEEP)close=FW_EASE_START_KEEP;
 if(!fw_trail_pose_behind(&fwfield_follower.trail,fwfield_follower.directional_gap,close,start)){clear_ease();return 0;}
 ease.active=1;ease.traveled=0;ease.close_keep=close;ease.last=*player;return 1;
}
static void finish_swap(Actor *a,const FwSample *sample,const FwPoint *player,const FwPoint *natural){
 int rail=swapOn.rail;
 /* Grid keeps the compass the player is walking. A rail facing is the line
  * frame (0 +side, 1 -side, 2 +front, 3 -front); copy it, and reseed every
  * sample with it before this function returns. move() does not push the
  * trail on this frame, so the pre-turn head cannot replace the turn.
  * The reseed pose is the fresh-load rest. Binding it here is the snap.
  * Keep the mirrored end when it is already past the start floor. A closer
  * end starts from that floor, still on the trail, and eases out from there. */
 unsigned face=rail?fw_rail_swap_turn(fwfield_player->face,player->x-swapOn.origin.x,player->z-swapOn.origin.z):fwfield_player->face;
 FwSample start=*sample,end=*sample,pose,eased;
 start.x=swapOn.origin.x;start.y=swapOn.origin.y;start.z=swapOn.origin.z;start.direction=(uint8_t)face;
 end.x=player->x;end.y=player->y;end.z=player->z;end.direction=(uint8_t)face;
 int seeded=rail?fw_trail_reseed_along(&fwfield_follower.trail,&start,&end,fwfield_follower.directional_gap,&pose)
  :fw_trail_reseed(&fwfield_follower.trail,&start,&end,fwfield_follower.directional_gap,&pose);
 if(seeded&&ease_from(natural,player,&eased))
  bind_pose(a,eased.x,eased.y,eased.z,face);
 else if(seeded)
  bind_pose(a,pose.x,pose.y,pose.z,face);
 else{
  fw_trail_clear(&fwfield_follower.trail);clear_ease();
  bind_pose(a,swapOn.origin.x,swapOn.origin.y,swapOn.origin.z,face);
 }
 /* Drop the tile left behind by the swap. Grid matches the pose just bound. */
 for(unsigned i=0;i<3;++i)a->previous[i]=a->grid[i];
 clear_swap();FollowingSwapDebug.face=face;
 settled=fwfield_player->world;haveSettled=centered(&settled)?1:0;
 if(!fwfx_hides_actor())a->flags&=~HIDDEN;
 fwfield_follower.state=FW_FOLLOWING;FollowingDebug.visible=!(a->flags&HIDDEN);
 animation_gate(a);
}
static int near_origin(const FwPoint *p){
 int64_t dx=(int64_t)p->x-swapOn.origin.x,dy=(int64_t)p->y-swapOn.origin.y,dz=(int64_t)p->z-swapOn.origin.z;
 if(dy>4096||dy<-4096)return 0;
 return dx*dx+dz*dz<=(int64_t)4096*4096;
}
/* Tile centers are not rail knots. A knot shorter than the follow gap never
 * reaches the follower's world pose, so the travel facing would stay applied. */
static int drive_rail_swap(Actor *a,const FwSample *sample,const FwPoint *player){
 FwPoint mirrored;
 int dx=player->x-swapOn.player.x,dz=player->z-swapOn.player.z;
 if(dx<0)dx=-dx;
 if(dz<0)dz=-dz;
 /* A finished knot sits still. A sub-unit bob must not keep the travel
  * facing applied; a real step moves far more than this between frames. */
 int stepSettled=swapOn.departed&&dx<=256&&dz<=256;
 int mirror=fw_rail_swap_mirror(&swapOn.follower,&swapOn.origin,player,&mirrored,stepSettled);
 if(mirror<0){clear_swap();return 0;}
 int32_t walked;
 if(fw_trail_height_at(&fwfield_follower.trail,mirrored.x,mirrored.z,&walked))mirrored.y=walked;
 Vec vacated={swapOn.origin.x,swapOn.origin.y,swapOn.origin.z};
 if(!fws_step_clear(a,&vacated)){recall_blocked(a);return 1;}
 if(mirror==1){finish_swap(a,sample,player,&mirrored);return 1;}
 if(near_origin(player)&&mirror==0&&swapOn.departed){
  bind_pose(a,swapOn.follower.x,swapOn.follower.y,swapOn.follower.z,swapOn.savedFace);
  clear_swap();settled=fwfield_player->world;haveSettled=1;animation_gate(a);return 1;
 }
 if(!near_origin(player))swapOn.departed=1;
 swapOn.player=*player;
 bind_pose(a,mirrored.x,mirrored.y,mirrored.z,fw_swap_face(fwfield_player->face,1));
 FollowingSwapDebug.active=1;FollowingSwapDebug.blocked=0;
 FollowingSwapDebug.progress=swap_progress(&swapOn.follower,&mirrored);
 if(fwfield_follower.state==FW_FOLLOWING&&!fwfx_hides_actor()){a->flags&=~HIDDEN;FollowingDebug.visible=1;}
 animation_gate(a);return 1;
}
/* Returns 1 when this frame belongs to the swap. A -1 mirror falls through
 * so the untouched trail becomes authoritative again. */
static int drive_swap(Actor *a,const FwSample *sample,const FwPoint *player){
 if(swapOn.rail)return drive_rail_swap(a,sample,player);
 FwPoint mirrored;
 int mirror=fw_swap_mirror(&swapOn.follower,&swapOn.origin,player,&mirrored);
 if(mirror<0){clear_swap();return 0;}
 int32_t walked;
 if(fw_trail_height_at(&fwfield_follower.trail,mirrored.x,mirrored.z,&walked))mirrored.y=walked;
 Vec vacated={swapOn.origin.x,swapOn.origin.y,swapOn.origin.z};
 if(!fws_step_clear(a,&vacated)){recall_blocked(a);return 1;}
 int onFollower=centered(&fwfield_player->world)&&same_tile(player->x,player->z,swapOn.follower.x,swapOn.follower.z);
 int onOrigin=centered(&fwfield_player->world)&&same_tile(player->x,player->z,swapOn.origin.x,swapOn.origin.z);
 if(mirror==1||onFollower){finish_swap(a,sample,player,&mirrored);return 1;}
 int gridLeads=fwfield_player->grid[0]==(int16_t)fw_tile(swapOn.follower.x)&&fwfield_player->grid[2]==(int16_t)fw_tile(swapOn.follower.z);
 if(onOrigin&&mirror==0&&(swapOn.departed||!gridLeads)){
  bind_pose(a,swapOn.follower.x,swapOn.follower.y,swapOn.follower.z,swapOn.savedFace);
  clear_swap();settled=fwfield_player->world;haveSettled=1;animation_gate(a);return 1;
 }
 if(!(onOrigin&&mirror==0))swapOn.departed=1;
 bind_pose(a,mirrored.x,mirrored.y,mirrored.z,fwfield_player->face^1u);
 FollowingSwapDebug.active=1;FollowingSwapDebug.blocked=0;
 FollowingSwapDebug.progress=swap_progress(&swapOn.follower,&mirrored);
 if(fwfield_follower.state==FW_FOLLOWING&&!fwfx_hides_actor()){a->flags&=~HIDDEN;FollowingDebug.visible=1;}
 animation_gate(a);return 1;
}
static int begin_rail_swap(Actor *a,const FwSample *sample,const FwPoint *player){
 FwPoint follower={a->world.x,a->world.y,a->world.z},origin={settled.x,settled.y,settled.z};
 if(player->x==origin.x&&player->y==origin.y&&player->z==origin.z)return 0;
 if(!fw_rail_swap_match(&fwfield_follower.trail,&follower,&origin))return 0;
 FwPoint probe;
 if(fw_rail_swap_mirror(&follower,&origin,player,&probe,0)<0)return 0;
 if(probe.x==follower.x&&probe.y==follower.y&&probe.z==follower.z)return 0;
 Vec vacated={origin.x,origin.y,origin.z};
 if(!fws_step_clear(a,&vacated)){recall_blocked(a);return 1;}
 clear_ease();
 swapOn.active=1;swapOn.departed=0;swapOn.rail=1;swapOn.follower=follower;swapOn.origin=origin;swapOn.player=*player;swapOn.savedFace=a->face;
 FollowingSwapDebug.blocked=0;FollowingSwapDebug.active=1;
 return drive_swap(a,sample,player);
}
static int begin_swap(Actor *a,const FwSample *sample,const FwPoint *player){
 if(swapOn.active||!haveSettled||fwfield_follower.state!=FW_FOLLOWING||fwfx_hides_actor())return 0;
 if(on_rail())return begin_rail_swap(a,sample,player);
 FwPoint follower={a->world.x,a->world.y,a->world.z},origin={settled.x,settled.y,settled.z};
 if(!fw_swap_match(&fwfield_follower.trail,&follower,&origin,fwfield_player->face))return 0;
 int stepping=!centered(&fwfield_player->world);
 int gridLeads=centered(&fwfield_player->world)&&fwfield_player->grid[0]==(int16_t)fw_tile(follower.x)&&fwfield_player->grid[2]==(int16_t)fw_tile(follower.z);
 int jumped=centered(&fwfield_player->world)&&same_tile(player->x,player->z,follower.x,follower.z);
 if(!stepping&&!gridLeads&&!jumped)return 0;
 FwPoint probe;
 if(fw_swap_mirror(&follower,&origin,player,&probe)<0)return 0;
 Vec vacated={origin.x,origin.y,origin.z};
 if(!fws_step_clear(a,&vacated)){recall_blocked(a);return 1;}
 clear_ease();
 swapOn.active=1;swapOn.departed=0;swapOn.follower=follower;swapOn.origin=origin;swapOn.savedFace=a->face;
 FollowingSwapDebug.blocked=0;FollowingSwapDebug.active=1;
 return drive_swap(a,sample,player);
}
static void move(Actor *a) {
 if ((uintptr_t)a!=fwfield_follower.actor || !fwfield_player || a->system!=fwfield_owner) return;
#ifdef FW_MOUNT
 if(fwland_active()){fwland_follow_player(a,fwfield_player);a->flags&=~PAUSE_ANM;return;}
#endif
 if(fwfield_follower.state==FW_INTERACTING){a->flags|=PAUSE_ANM;return;}
 int paused=fwfield_follower.state==FW_EVENT_PAUSED;
 if(paused)a->flags|=PAUSE_ANM;
 FwSample sample,target;
 player_sample(&sample);
 uint16_t zoneBefore=fwfield_trailZone;uint8_t zoneValid=fwfield_trailZoneValid;
 if(!rebase_connection(a,0,&sample)){a->flags|=HIDDEN;fwfield_follower.state=FW_WAITING;FollowingDebug.visible=0;++FollowingDebug.resets;clear_swap();clear_ease();drop_settled();animation_gate(a);return;}
 if(zoneValid&&fwfield_player->zone!=zoneBefore){clear_swap();clear_ease();drop_settled();}
 FwPoint player={fwfield_player->world.x,fwfield_player->world.y,fwfield_player->world.z};
 if(paused){clear_swap();drop_settled();}
 else if(swapOn.active){if(drive_swap(a,&sample,&player))return;}
 else if(begin_swap(a,&sample,&player))return;
 int result=fw_trail_push_directional(&fwfield_follower.trail,&sample,&target,fwfield_follower.directional_gap);
 if(result<0) { a->flags|=HIDDEN; fwfield_follower.state=FW_WAITING; FollowingDebug.visible=0; ++FollowingDebug.resets;clear_swap();clear_ease();drop_settled();animation_gate(a); return; }
 /* An event may own the sprite, but the route has to keep the player's
  * steps or a short pause while running exceeds one tile and the trail drops.
  * Pause and a full stop do not advance the ease, so a held gap stays put. */
 if(paused)return;
 if(fwfield_follower.state==FW_FOLLOWING && !fwfx_hides_actor()){a->flags&=~HIDDEN;FollowingDebug.visible=1;}
 if(result!=1){note_settled();animation_gate(a);return;}
 if(ease.active){
  if(!fwfield_follower.trail.count)clear_ease();
  else{
   int64_t dy=(int64_t)sample.y-a->world.y;
   if(dy<0)dy=-dy;
   uint32_t step=fw_travel(ease.last.x,ease.last.z,player.x,player.z);
   uint32_t traveled=ease.traveled+step;
   if(traveled<ease.traveled)traveled=0xffffffffu;
   ease.last=player;
   /* A stair or ledge completes onto the ordinary trail pose. Past the
    * travel budget the push output is the fresh-load rest, exactly. */
   if(dy>(int64_t)FW_TILE||traveled>=FW_EASE_TRAVEL)clear_ease();
   else{
    ease.traveled=traveled;
    uint32_t keep=fw_ease_keep(traveled,FW_EASE_TRAVEL,ease.close_keep,FW_TILE);
    FwSample posed;
    if(fw_trail_pose_behind(&fwfield_follower.trail,fwfield_follower.directional_gap,keep,&posed)){
     target.x=posed.x;target.y=posed.y;target.z=posed.z;
    }
   }
  }
 }
 Vec next={target.x,target.y,target.z};
 if(!fws_step_clear(a,&next)){
  if(!(a->flags&HIDDEN))fwfx_recall(a);
  a->flags|=HIDDEN;fwfield_follower.state=FW_WAITING;FollowingDebug.visible=0;
  fw_trail_clear(&fwfield_follower.trail);clear_ease();++FollowingDebug.resets;return;
 }
 a->world=(Vec){target.x,target.y,target.z};
 for(unsigned i=0;i<3;++i) a->previous[i]=a->grid[i];
 a->grid[0]=(int16_t)(target.x/FW_TILE); a->grid[1]=(int16_t)(target.y/FW_TILE); a->grid[2]=(int16_t)(target.z/FW_TILE);
 CALL(0x02167099,void(*)(Actor*,uint32_t))(a,target.direction);
 fwr_anchor(a,target.direction);
 a->moveflags&=~(2048u|4096u);
 if(fwfield_follower.state!=FW_FOLLOWING){
#ifdef FW_MOUNT
  /* Dismount returns through this ordinary trail-spawn path and keeps its
   * ball send-out effect. */
#endif
  fwfx_out(a);
 }
 if(!fwfx_hides_actor())a->flags&=~HIDDEN;
 fwfield_follower.state=FW_FOLLOWING; FollowingDebug.visible=!(a->flags&HIDDEN);
 /* This dispatcher belongs to a native movement context absent from our
  * synthetic stock actor. Calling it on a grid follower with flag 0x400
  * cleared dereferences a null Actor+0x94 context and freezes the game. Keep
  * stock out of this path until a grid-specific adapter is audited. */
 animation_gate(a);
 note_settled();
}
static const Moves fwfield_moves={0,noop,move,removed,noop};
static int owned(void) {
 Actor *a=(Actor*)fwfield_follower.actor;
 return a && fwfield_owner && a>=fwfield_owner->actors && a<fwfield_owner->actors+fwfield_owner->capacity && (a->flags&USE) && a->system==fwfield_owner && a->callbacks==&fwfield_moves;
}
static void cleanup_impl(int keepTrail) {
 fwr_grass_clear();
#ifdef FW_MOUNT
 fwland_end();fwfield_landCheckAfterPause=0;
#endif
 fwt_cancel();
 if(owned()) CALL(0x02166981,void(*)(Actor*))((Actor*)fwfield_follower.actor);
 fwfield_follower.actor=0; fwfield_player=0; fwfield_follower.state=FW_ABSENT;
 fwfield_shownNow=fwfield_reveal=0;
 if(!keepTrail){fw_trail_clear(&fwfield_follower.trail);fwfield_trailZoneValid=0;}
 fwfield_terrainActor=0;
 fwfield_cyclePending=0;
 clear_swap();clear_ease();drop_settled();settled=(Vec){0,0,0};
 FollowingDebug.actor=FollowingDebug.visible=0;
}
static void cleanup(void) {cleanup_impl(0);}
/* The retail key-direction veneer is used by ordinary field movement. A
 * recall owns the player's next step until the ball effect has finished. */
API unsigned FollowingPlayerMoveDir(void *player,unsigned key){
 if(fwfx_recalling())return 9;
 return CALL(0x0219b161,unsigned(*)(void*,unsigned))(PTR(player,4),key);
}
static void scene_recall(void){
#ifdef FW_MOUNT
 if(fwland_active()&&owned()){((Actor*)fwfield_follower.actor)->flags|=HIDDEN;fwland_end();}
#endif
 if(owned()&&!(((Actor*)fwfield_follower.actor)->flags&HIDDEN))fwfx_recall((Actor*)fwfield_follower.actor);
 cleanup();
}
static uint16_t read16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
/* CONFIG bits 0..10 are retail settings. Bit 11 is the saved Followers Off
 * choice; zero preserves the behavior of every existing save. */
static int followers_off(void *game){
 if(!game)return 0;
 void *save=PTR(game,0);
 void *options=save?CALL(0x02008ddd,void*(*)(void*))(save):0;
 return options&&(*(const uint16_t*)options&(1u<<11));
}
static int registry_read(void *file,unsigned at,void *out,unsigned n){
 if(at>fwfield_registrySize||n>fwfield_registrySize-at)return 0;
 ++FollowingConfigDebug.reads;
 return CALL(0x02070e55,int(*)(void*,int,unsigned))(file,(int)at,0)
  &&CALL(0x02070e6d,unsigned(*)(void*,void*,unsigned))(file,out,n)==n;
}
__attribute__((noinline)) static int config(void) {
 if(fwfield_configState)return fwfield_configState>0;
 fwfield_configState=-1;fwfield_pageCount=0;FollowingConfigDebug.stage=1;
 if(FollowingCoreAPI.abi!=FWC_ABI||FollowingCoreAPI.size!=sizeof(FwcApi))return 0;
 FollowingCoreAPI.clear();FollowingConfigDebug.stage=2;
 uint32_t file[32],header[8];
 CALL(0x02070ca9,void(*)(void*))(file);
 if(!CALL(0x02070ecd,int(*)(void*,const char*))(file,"rom:/following/native.bin"))return 0;
 unsigned size=CALL(0x02070ded,unsigned(*)(void*))(file);
 int ok=size==sizeof(header)&&CALL(0x02070e6d,unsigned(*)(void*,void*,unsigned))(file,header,sizeof(header))==sizeof(header);
 CALL(0x02070de1,int(*)(void*))(file);
 if(ok)ok=header[0]==0x544e5746&&header[1]==1&&header[2]==2&&header[3]>=32&&header[3]<=REGISTRY_MAX&&header[4]<=65535&&header[5]<=65535&&header[7]==0;
 if(!ok)return 0;
 fwfield_registryCrc=header[6];fwfield_descriptorCount=(uint16_t)header[4];
 FollowingConfigDebug.stage=3;FollowingConfigDebug.registryBytes=header[3];
 CALL(0x02070ca9,void(*)(void*))(file);
 if(!CALL(0x02070ecd,int(*)(void*,const char*))(file,"rom:/following/runtime-registry.bin"))return 0;
 fwfield_registrySize=CALL(0x02070ded,unsigned(*)(void*))(file);
 FwRegistryInput input={registry_read,file,fwfield_page,fwfield_index,FW_MAX_SPECIES+2,fwfield_registrySize,header[6],header[4],header[5],0,0};
 FollowingConfigDebug.stage=4;
 ok=fwfield_registrySize==header[3]&&fw_registry_stream(&input)
    &&FollowingCoreAPI.publish(input.descriptors);
 CALL(0x02070de1,int(*)(void*))(file);
 fwfield_pageCount=0;
 if(ok){fwfield_stride=input.stride;fwfx_set_resource_count(header[5]);fwfield_configState=1;FollowingConfigDebug.stage=7;}
 else {fwfx_set_resource_count(0);FollowingCoreAPI.clear();fwfield_registrySize=fwfield_stride=0;}
 return ok;
}
/* Only called on selection/appearance changes, never during movement/drawing. */
__attribute__((noinline)) static uint16_t model(const FwPokemon *p) {
 if(fwfield_configState!=1||!p||!p->species||p->species>FW_MAX_SPECIES)return 0;
 unsigned first=fwfield_index[p->species],end=fwfield_index[p->species+1];
 fwfield_follower.side_gap=0;fwfield_follower.sprite_y=0;
 uint16_t fallback=0x3000;int best=-100,opened=0,artX=0;uint32_t file[32];
 for(unsigned i=first;i<end;++i){
  if(i<fwfield_pageFirst||i-fwfield_pageFirst>=fwfield_pageCount){
   if(!opened){
    CALL(0x02070ca9,void(*)(void*))(file);
    if(!CALL(0x02070ecd,int(*)(void*,const char*))(file,"rom:/following/runtime-registry.bin"))goto fail;
    opened=1;if(CALL(0x02070ded,unsigned(*)(void*))(file)!=fwfield_registrySize)goto fail;
   }
   unsigned n=FW_REGISTRY_PAGE/fwfield_stride;if(n>end-i)n=end-i;
   fwfield_pageCount=0;
   if(!registry_read(file,32+i*fwfield_stride,fwfield_page,n*fwfield_stride))goto fail;
   fwfield_pageFirst=i;fwfield_pageCount=n;
  }
  const uint8_t *r=fwfield_page+(i-fwfield_pageFirst)*fwfield_stride;
  unsigned species,form,gender,shiny,row;
  if(fwfield_stride==12){
   uint32_t key=read16(r)|(uint32_t)read16(r+2)<<16;
   species=key>>11;form=(key>>3)&255;gender=(key>>1)&3;shiny=key&1;row=read16(r+4);
  }else{species=read16(r);form=r[2];gender=r[3];shiny=r[4];row=read16(r+6);}
  if(species!=p->species)goto fail;
  unsigned shown=fw_follower_form(p->species,p->form);
  int score=(form==shown?8:form==0?0:-20)+(gender==p->gender?4:gender==2?2:-20)+(shiny==p->shiny?1:-20);
  if(score>best){best=score;fallback=FW_CODE_BASE+row-FW_STOCK_ROWS;fwfield_follower.side_gap=fwfield_stride==12?(r[8]>>3)&7:r[15];fwfield_follower.sprite_y=(int8_t)(fwfield_stride==12?r[10]:r[13]);
   artX=(int8_t)(fwfield_stride==12?r[9]:r[12]);
#ifdef FW_MOUNT
   fwland_animation_profile(fwfield_stride==12?(r[8]&4u):r[11]);
#endif
  }
 }
 if(opened)CALL(0x02070de1,int(*)(void*))(file);
 if(artX<0)artX=-artX;
 unsigned maximum=artX<10?10u-(unsigned)artX:0u;
 unsigned gap=fwfield_follower.side_gap+FW_DEFAULT_SIDE_GAP_BONUS;
 if(gap>maximum)gap=maximum;
 if(gap>FW_MAX_DIRECTIONAL_GAP)gap=FW_MAX_DIRECTIONAL_GAP;
 for(unsigned i=0;i<4;++i)fwfield_follower.directional_gap[i]=(uint8_t)gap;
 if(first<end&&fallback>=FW_CODE_BASE){
  uint8_t positions[12];
  if(fwp_land(fallback-FW_CODE_BASE,fwfield_descriptorCount-FW_STOCK_ROWS,fwfield_registryCrc,positions)){
   unsigned uniform=0;
   for(unsigned i=0;i<4;++i)if(positions[i]>uniform)uniform=positions[i];
   if(uniform>FW_MAX_DIRECTIONAL_GAP)uniform=FW_MAX_DIRECTIONAL_GAP;
   for(unsigned i=0;i<4;++i)fwfield_follower.directional_gap[i]=(uint8_t)uniform;
  }
 }
 return first<end?fallback:0;
fail:
 if(opened)CALL(0x02070de1,int(*)(void*))(file);
 fwfield_pageCount=0;FollowingConfigDebug.stage=8;return 0;
}
static int select(void *game,void *field,FwPokemon *out,FwrSnapshot *snapshot,int direction,int *chosenSlot) {
 void *party=CALL(0x0201735d,void*(*)(void*))(game);
 if(!party)return 0;
 unsigned count=CALL(0x0201fe25,unsigned(*)(void*))(party); if(count>6)return 0;
 FwPokemon mons[6];
 for(unsigned i=0;i<count;++i) {
  void *mon=CALL(0x0201ff35,void*(*)(void*,unsigned))(party,i);
  if(!mon)return 0;
  uint32_t(*param)(void*,unsigned,void*)=CALL(0x0201cd25,uint32_t(*)(void*,unsigned,void*));
  mons[i]=(FwPokemon){.species=param(mon,5,0),.form=param(mon,0x6f,0),.gender=param(mon,0x6e,0),.egg=param(mon,0x4c,0),.hp=param(mon,0xa0,0),.max_hp=param(mon,0xa1,0),.personality=param(mon,0,0),.trainer=param(mon,7,0),.shiny=CALL(0x0201cdd9,int(*)(void*))(mon)};
 }
 int slot=fw_select(mons,count); if(slot<0)return 0;
 if(direction){
  int current=-1;
  if(fwfield_follower.has_selection){
   int hint=fwfield_follower.slot;
   if(hint>=0 && (unsigned)hint<count && fw_same_identity(&mons[hint],&fwfield_follower.selected))current=hint;
   else for(unsigned i=0;i<count;++i)if(fw_same_identity(&mons[i],&fwfield_follower.selected)){current=(int)i;break;}
  }
  slot=fw_cycle_slot(mons,count,current,direction);
 }else if(fwfield_follower.slot>=0 && fwfield_follower.has_selection){
  int manual=-1,hint=fwfield_follower.slot;
  if((unsigned)hint<count && fw_same_identity(&mons[hint],&fwfield_follower.selected))manual=hint;
  else for(unsigned i=0;i<count;++i)if(fw_same_identity(&mons[i],&fwfield_follower.selected)){manual=(int)i;break;}
  if(manual>=0 && !mons[manual].egg && mons[manual].species && mons[manual].species<=FW_MAX_SPECIES
     && (mons[manual].hp || !mons[slot].hp)){slot=manual;fwfield_follower.slot=(int8_t)manual;}
  else fwfield_follower.slot=-1;
 }
 if(slot<0)return 0;
 *out=mons[slot];
 if(chosenSlot)*chosenSlot=slot;
 void *selectedMon=CALL(0x0201ff35,void*(*)(void*,unsigned))(party,(unsigned)slot);
 if(!selectedMon)return 0;
 uint32_t(*selectedParam)(void*,unsigned,void*)=CALL(0x0201cd25,uint32_t(*)(void*,unsigned,void*));
 unsigned type1=selectedParam(selectedMon,0xae,0),type2=selectedParam(selectedMon,0xaf,0);
 fwfield_flying=(type1==2||type2==2);
 if(snapshot){
  *snapshot=(FwrSnapshot){.pokemon=*out};
  snapshot->friendship=(uint8_t)selectedParam(selectedMon,9,0);snapshot->status=selectedParam(selectedMon,157,0);snapshot->type1=(uint8_t)type1;snapshot->type2=(uint8_t)type2;snapshot->zone=fw_field_zone(field);
  selectedParam(selectedMon,116,snapshot->nickname);snapshot->nickname[11]=0xffff;
  const uint16_t *name=CALL(0x0201736d,const uint16_t*(*)(void*))(game);
  if(!name)return 0;
  for(unsigned i=0;i<8;++i)snapshot->playerName[i]=name[i];
  snapshot->playerName[8]=0xffff;
 }
 return 1;
}
/* MapSpawnFinish case 0 has finished the actor and weather drains.
 * field+8 is GameData. field+0x40 is the actor system. The entrance fade
 * has not run, so LoadStaticBlact is the covered retail upload.
 * A config miss on this first try is cleared; the field update can retry.
 * Emote sheets stay off this path. */
#ifdef FW_STOCK
API void FollowingPreloadFollower(void *field, void *actorSystem) {
 if(!field||!actorSystem)return;
 void *game=PTR(field,8);
 if(!game||followers_off(game))return;
 int fresh=!fwfield_configState;
 if(!config()){if(fresh)fwfield_configState=0;return;}
 FwPokemon selected;
 if(!select(game,field,&selected,0,0,0))return;
 uint16_t code=model(&selected);
 if(!code)return;
 CALL(0x0219386d,void(*)(void*,unsigned))(actorSystem,code);
}
#endif
static void cycle_follower(void *game,void *field){
#ifdef FW_MOUNT
 if(fwland_active())return;
#endif
 unsigned keys=CALL(0x0203df29,unsigned(*)(void))()&0x300u;
 if(keys!=0x100u && keys!=0x200u)return; /* Both shoulders together have no direction. */
 if(!owned() || fwfield_follower.state!=FW_FOLLOWING || !fwfield_player || fwt_active() || fwfx_busy())return;
 Actor *actor=(Actor*)fwfield_follower.actor;
 if(actor->flags&HIDDEN)return;
 fwfx_prepare(actor);
 if(!fwfx_available())return; /* A switch must have its recall and send-out effects. */
 uint8_t oldFlying=fwfield_flying;
 FwPokemon next;int slot=-1;
 if(!select(game,field,&next,0,keys==0x100u?1:-1,&slot))return;
 uint8_t oldGap=fwfield_follower.side_gap;
 uint8_t oldDirectional[4];for(unsigned i=0;i<4;++i)oldDirectional[i]=fwfield_follower.directional_gap[i];
 int8_t oldY=fwfield_follower.sprite_y;
 uint16_t code=model(&next);
 if(!code){
  fwfield_follower.side_gap=oldGap;fwfield_follower.sprite_y=oldY;
  for(unsigned i=0;i<4;++i)fwfield_follower.directional_gap[i]=oldDirectional[i];
  fwfield_flying=oldFlying;
  return;
 }
 fwfield_cyclePose=actor->world;
 for(unsigned i=0;i<3;++i)fwfield_cycleGrid[i]=actor->grid[i];
 fwfield_cycleFace=actor->face;
 fwfx_snapshot(actor);fwfx_recall(actor);
 Actor *keep=fwfield_player;
 fwfield_cyclePending=1; /* The native delete callback must retain the route. */
 cleanup_impl(1);fwfield_player=keep;
 fwfield_follower.selected=next;fwfield_follower.has_selection=1;
 fwfield_follower.slot=(int8_t)slot;fwfield_code=code;
 fwfield_cyclePending=1;
 clear_restore();
}
static void cycle_track(void){
 if(!fwfield_cyclePending||!fwfield_player)return;
 FwSample sample,target;
 player_sample(&sample);
 if(!rebase_connection(0,&fwfield_cyclePose,&sample)){fwfield_cyclePending=0;return;}
 int step=fw_trail_push_directional(&fwfield_follower.trail,&sample,&target,fwfield_follower.directional_gap);
 if(step<0){fwfield_cyclePending=0;return;}
 if(step==1){
  fwfield_cyclePose=(Vec){target.x,target.y,target.z};
  fwfield_cycleGrid[0]=(int16_t)(target.x/FW_TILE);
  fwfield_cycleGrid[1]=(int16_t)(target.y/FW_TILE);
  fwfield_cycleGrid[2]=(int16_t)(target.z/FW_TILE);
  fwfield_cycleFace=target.direction;
 }
}
#ifdef FW_ARCEUS_SURF
/* Surf's native permission flow runs separately. This read only chooses art:
 * the selected follower wins when it knows Surf, otherwise party order wins.
 * Neither HP nor remaining move PP changes eligibility. */
API uint32_t FollowingSurfPartyAppearance(void *game){
 if(!game)return 0;
 void *party=CALL(0x0201735d,void*(*)(void*))(game);
 if(!party)return 0;
 unsigned count=CALL(0x0201fe25,unsigned(*)(void*))(party);
 if(count>6)return 0;
 uint32_t(*param)(void*,unsigned,void*)=CALL(0x0201cd25,uint32_t(*)(void*,unsigned,void*));
 uint32_t first=0,preferred=0;
 for(unsigned i=0;i<count;++i){
  void *mon=CALL(0x0201ff35,void*(*)(void*,unsigned))(party,i);
  if(!mon)continue;
  unsigned species=param(mon,5,0);
  if(!species||species>FW_MAX_SPECIES||param(mon,0x4c,0))continue;
  unsigned knows=0;
  for(unsigned move=0;move<4;++move)if(param(mon,0x36+move,0)==57u){knows=1;break;}
  if(!knows)continue;
  unsigned form=param(mon,0x6f,0),gender=param(mon,0x6e,0);
  unsigned shiny=!!CALL(0x0201cdd9,int(*)(void*))(mon);
  if(form>255||gender>2)continue;
  uint32_t appearance=species|(fw_follower_form(species,form)<<10)|(gender<<18)|(shiny<<26);
  if(!first)first=appearance;
  if(fwfield_follower.has_selection){
   const FwPokemon *selected=&fwfield_follower.selected;
   if(species==selected->species && form==selected->form && gender==selected->gender
      && shiny==selected->shiny && param(mon,0,0)==selected->personality
      && param(mon,7,0)==selected->trainer){
    if(fwfield_follower.slot==(int)i)return appearance;
    if(!preferred)preferred=appearance;
   }
  }
 }
 return preferred?preferred:first;
}
#endif
static void before(ActorSystem *sys) {
 fwt_poll_input();
 ++fwfield_tick; FollowingDebug.ticks=fwfield_tick;
#ifdef FW_MOUNT
 fwland_tick(fwfield_tick);
#endif
 if(fwfield_owner!=sys) {
#ifdef FW_MOUNT
  unsigned carry=fwland_active();
#endif
  FwPokemon manual=fwfield_follower.selected;
  int manualSlot=fwfield_follower.slot;
  int keepManual=fwfield_owner&&fwfield_follower.has_selection&&manualSlot>=0;
  fws_detach();cleanup();fwt_unload();fwfx_destroy();fwfield_owner=sys;
  fw_init(&fwfield_follower,++fwfield_generation);
  if(keepManual){fwfield_follower.selected=manual;fwfield_follower.slot=(int8_t)manualSlot;fwfield_follower.has_selection=1;}
  fwfield_eventsReady=fws_attach(sys,&fwfield_follower,fwfield_generation,scene_recall);
  clear_restore();
  if(sys&&sys->field&&followers_off(PTR(sys->field,8)))FollowingEventsAPI.discard();
  else if(fwfield_eventsReady)fws_take_restore(&fwfield_restore);
#ifdef FW_MOUNT
  if(fwfield_eventsReady&&sys&&sys->field&&!followers_off(PTR(sys->field,8))){
   FweMount mount;
   if(FollowingEventsAPI.consumeMount(PTR(sys->field,8),&mount)&&
      fwtm_menu_restore(sys->field,&mount)){
    fwfield_follower.selected=mount.pokemon;
    fwfield_follower.slot=(int8_t)mount.slot;
    fwfield_follower.has_selection=1;
   }
  }
  fwfield_landCarry=(uint8_t)(carry&&fwfield_restore.magic==FWE_RESTORE_MAGIC);
#endif
 }
 void *field=sys->field;
 if(fwt_active()&&!fwt_owns(field))fwt_cancel();
 int ownTalk=fwt_field_owns(field);
 unsigned eventState=fws_poll();
 if(eventState==FWS_INVALID)fwfield_eventsReady=0;
 int paused=eventState==FWS_PAUSED;
 FollowingDebug.system=(uint32_t)sys; FollowingDebug.systemFlags=sys->flags; FollowingDebug.field=(uint32_t)field;
 uint32_t reason=0;
 if(!config())reason|=1;
 if(field&&followers_off(PTR(field,8)))reason|=4096;
 if(!fwfield_eventsReady)reason|=1024;
 if(eventState==FWS_RECALLED)reason|=2048;
 if(!field || (sys->flags&17)!=17 || (sys->flags&6))reason|=2;
 if(!reason) {
  void *fp=PTR(field,0x94),*game=PTR(field,8);
  fwfield_player=fp?CALL(0x0219a6e1,Actor*(*)(void*))(fp):0;
  fws_set_player(fwfield_player);
  if(!fwfield_player || !(fwfield_player->flags&USE) || (fwfield_player->flags&HIDDEN) || ((fwfield_player->moveflags&8)&&!ownTalk&&!paused))reason|=4;
  if(fp && CALL(0x0219a705,unsigned(*)(void*))(fp)!=0)reason|=8;
  if((CALL(0x0218130d,int(*)(void*))(field)&&!ownTalk&&!paused) || (U32(field,0x148)&&!fws_pc_fade()&&!fws_battle_entry()))reason|=16;
  if(CALL(0x0216a2b5,int(*)(void*))(game))reason|=32;
#ifdef FW_MOUNT
  if(paused&&fwland_active())fwfield_landCheckAfterPause=1;
  if(!reason&&!paused&&fwland_active()&&(fwfield_landCheckAfterPause||fwfield_tick%30==0)){
   fwfield_landCheckAfterPause=0;fwland_validate();
  }
#endif
  if(!reason &&
#ifdef FW_MOUNT
      !fwland_invalid() &&
#endif
      !ownTalk && (!paused||fwfield_restore.magic==FWE_RESTORE_MAGIC) && (!fwfield_follower.actor || fwfield_tick%30==0 || (eventState&FWS_REFRESH))) {
   FwPokemon selected;
   if(!select(game,field,&selected,0,0,0))reason|=64;
   else if(!fwfield_follower.has_selection || !fw_same_identity(&selected,&fwfield_follower.selected)) {
    uint16_t code=model(&selected);
    if(!code){reason|=1;fwfield_configState=-1;FollowingCoreAPI.clear();}
    else {Actor *keep=fwfield_player; cleanup(); fwfield_player=keep; fwfield_follower.selected=selected; fwfield_follower.has_selection=1; fwfield_code=code;}
   }
  }
  if(!reason&&!ownTalk&&!paused&&!fwt_active())cycle_follower(game,field);
 }
 FollowingDebug.reason=reason;
 if(reason){if(fwfield_eventsReady)FollowingEventsAPI.discard();clear_restore();fwt_cancel();
#ifdef FW_MOUNT
  if(fwland_active()){((Actor*)fwfield_follower.actor)->flags|=HIDDEN;fwland_end();}
  fwfield_landCarry=0;
#endif
  if(!(reason&4096)&&(reason&16))fwfx_warm(sys->resourceHeap);
  if(owned() && !( ((Actor*)fwfield_follower.actor)->flags&HIDDEN) && (reason&(8|16|32)))fwfx_recall((Actor*)fwfield_follower.actor);else if(fwfield_follower.actor)fwfx_cancel();cleanup();return;}
#ifdef FW_MOUNT
 if(fwland_invalid()&&owned()){
  Actor *a=(Actor*)fwfield_follower.actor;
  fwland_end();a->flags|=HIDDEN;a->moveflags&=~SHADOW_VANISH;
  fw_trail_clear(&fwfield_follower.trail);clear_ease();fwfield_follower.state=FW_WAITING;
  FollowingDebug.visible=0;
 }
#endif
 FollowingDebug.fwfield_player=(uint32_t)fwfield_player; FollowingDebug.species=fwfield_follower.selected.species; FollowingDebug.fwfield_code=fwfield_code;
 if(fwfield_follower.actor && !owned()){fwfield_follower.actor=0;fw_trail_clear(&fwfield_follower.trail);clear_ease();}
 if(paused&&fwfield_restore.magic!=FWE_RESTORE_MAGIC)return;
 /* Only a matching storage return may reconstruct during a paused event. */
 if(fwfield_restore.magic==FWE_RESTORE_MAGIC&&(!fw_same_identity(&fwfield_restore.selected,&fwfield_follower.selected)
    ||fwfield_restore.zone!=fwfield_player->zone||fwfield_restore.player.x!=fwfield_player->world.x
    ||fwfield_restore.player.y!=fwfield_player->world.y||fwfield_restore.player.z!=fwfield_player->world.z))clear_restore();
 if(paused&&fwfield_restore.magic!=FWE_RESTORE_MAGIC)return;
 #ifdef FW_MOUNT
 if(fwtm_return_pending()&&owned()){
  Actor *a=(Actor*)fwfield_follower.actor;
  int ready=fwtm_return_ready(PTR(field,8),&fwfield_follower.selected);
  fwsurf_destroy();
  if(ready){
   a->flags&=~HIDDEN;fwfield_follower.state=FW_FOLLOWING;
   if(fwland_begin(sys,a,fwfield_player,field,fwfield_code,
      fwfield_registryCrc,fwfield_descriptorCount,&fwfield_follower.selected,fwfield_follower.slot)){
    fw_trail_clear(&fwfield_follower.trail);clear_ease();fwland_follow_player(a,fwfield_player);
    FollowingDebug.visible=1;
   }else{a->flags|=HIDDEN;fwfield_follower.state=FW_WAITING;FollowingDebug.visible=0;}
  }
  fwtm_return_done();
  return;
 }
 #endif
 if(!fwfield_follower.actor) {
  if(fwfx_busy()){cycle_track();return;}
  /* Native allocation asserts on a full pool. Check capacity and leave room. */
  unsigned free=0; for(unsigned i=0;i<sys->capacity;++i)if(!(sys->actors[i].flags&USE))++free;
  if(free<2){FollowingDebug.reason=128;return;}
  unsigned id;
  for(id=0xf0;id<=0xf8;++id) { unsigned i; for(i=0;i<sys->capacity;++i)if((sys->actors[i].flags&USE) && sys->actors[i].id==id)break; if(i==sys->capacity)break; }
  if(id>0xf8){FollowingDebug.reason=256;return;}
  if(CALL(0x0203a2d5,uint32_t(*)(uint32_t))(sys->resourceHeap)<32768){FollowingDebug.reason=512;return;}
  Actor *a=CALL(0x021668c1,Actor*(*)(ActorSystem*,int,int,unsigned,unsigned,unsigned,unsigned,unsigned))(sys,fwfield_player->grid[0],fwfield_player->grid[2],fwfield_player->face,id,fwfield_code,0,fwfield_player->zone);
  if(!a)return;
  a->flags|=HIDDEN|NONBLOCKING|NO_QUERY|KEEP_ZONE; a->moveflags|=NOT_SAVE|NO_EFFECT; a->moveflags&=~(8u|2048u|4096u);
  a->callbacks=&fwfield_moves; a->world=fwfield_player->world;
  fwr_anchor(a,fwfield_player->face);
  fwfield_follower.actor=(uintptr_t)a; fwfield_follower.state=FW_WAITING;
#ifdef FW_MOUNT
  if(fwtm_return_pending()){
   int ready=fwtm_return_ready(PTR(field,8),&fwfield_follower.selected);
   fwsurf_destroy();
   if(ready){
    a->flags&=~HIDDEN;fwfield_follower.state=FW_FOLLOWING;
    if(fwland_begin(sys,a,fwfield_player,field,fwfield_code,
       fwfield_registryCrc,fwfield_descriptorCount,&fwfield_follower.selected,fwfield_follower.slot)){
     fw_trail_clear(&fwfield_follower.trail);clear_ease();fwland_follow_player(a,fwfield_player);
     FollowingDebug.visible=1;
    }else{a->flags|=HIDDEN;fwfield_follower.state=FW_WAITING;}
   }
   fwtm_return_done();
  }else
#endif
  if(fwfield_cyclePending){
   fwfield_cyclePending=0;
   if(fws_step_clear(a,&fwfield_cyclePose)){
    a->world=fwfield_cyclePose;
    for(unsigned i=0;i<3;++i)a->grid[i]=a->previous[i]=fwfield_cycleGrid[i];
    CALL(0x02167099,void(*)(Actor*,uint32_t))(a,fwfield_cycleFace);
    fwr_anchor(a,fwfield_cycleFace);
    fwfx_out(a);
    if(!fwfx_hides_actor())a->flags&=~HIDDEN;
    fwfield_follower.state=FW_FOLLOWING;FollowingDebug.visible=!(a->flags&HIDDEN);
   }
  }else if(fwfield_restore.magic==FWE_RESTORE_MAGIC&&fwfield_restore.zone==fwfield_player->zone
     &&fwfield_restore.player.x==fwfield_player->world.x&&fwfield_restore.player.y==fwfield_player->world.y&&fwfield_restore.player.z==fwfield_player->world.z){
   a->world=fwfield_restore.follower;
   for(unsigned i=0;i<3;++i)a->grid[i]=a->previous[i]=fwfield_restore.grid[i];
   CALL(0x02167099,void(*)(Actor*,uint32_t))(a,fwfield_restore.face);
   fwr_anchor(a,fwfield_restore.face);
   a->flags&=~HIDDEN;fwfield_follower.state=FW_FOLLOWING;FollowingDebug.visible=1;fws_restored();
#ifdef FW_MOUNT
   if(fwfield_landCarry){
    if(fwland_begin(sys,a,fwfield_player,field,fwfield_code,fwfield_registryCrc,fwfield_descriptorCount,&fwfield_follower.selected,fwfield_follower.slot)){
     fw_trail_clear(&fwfield_follower.trail);clear_ease();fwland_follow_player(a,fwfield_player);
    }
    fwfield_landCarry=0;
   }
#endif
  }
  clear_restore();
  FollowingDebug.actor=(uint32_t)a; ++FollowingDebug.spawns;
 }
}
static void terrain_update(void) {
#ifdef FW_MOUNT
 if(fwland_active()){
  if(owned()){
   Actor *a=(Actor*)fwfield_follower.actor;
   if(a->moveflags&0x40000u)CALL(0x021676fd,void(*)(Actor*,unsigned))(a,0);
  }
  fwfield_terrainActor=0;return;
 }
#endif
 if(!owned()) {fwfield_terrainActor=0;return;}
 Actor *a=(Actor*)fwfield_follower.actor;
 if((a->flags&HIDDEN)||fwfield_flying){
  if(a->moveflags&0x40000u)CALL(0x021676fd,void(*)(Actor*,unsigned))(a,0);
  fwfield_terrainActor=0;return;
 }
 /* This controller lookup is the same one used by the native grass entry
  * path. The grid-tile attribute query is separate from the rail movement
  * dispatcher, which dereferences our actor's absent +0x94 context. */
 void *controller=CALL(0x02194f19,void*(*)(Actor*))(a);
 if(!controller)return;
 if(a==fwfield_terrainActor && controller==fwfield_terrainController &&
    a->grid[0]==fwfield_terrainX && a->grid[2]==fwfield_terrainZ &&
    a->zone==fwfield_terrainZone)return;
 Vec center={(int32_t)a->grid[0]*FW_TILE+FW_TILE/2,a->world.y,
             (int32_t)a->grid[2]*FW_TILE+FW_TILE/2};
 uint32_t attr=0;
 /* An unmarked tile reports no attribute. Still visit the shoal adapter so
  * it clears the native effect bit and retires the attached splash task. */
 CALL(0x0215e8e5,int(*)(Actor*,const Vec*,uint32_t*))(a,&center,&attr);
 fwfield_terrainActor=a;fwfield_terrainController=controller;
 fwfield_terrainX=a->grid[0];fwfield_terrainZ=a->grid[2];
 fwfield_terrainZone=a->zone;
 /* The stock shoal adapter reads only the current terrain ID at +10 and the
  * effect controller at +20. It owns the actor-bound shallow-water task and
  * its exit bit; unlike the movement dispatcher, it does not read +0x94. */
 FwShoalContext shoal={0};
 shoal.terrain=(uint16_t)attr;shoal.controller=controller;
 CALL(0x0215e151,void(*)(Actor*,void*))(a,&shoal);
 /* Grass remains an entry effect, once per crossed tile. */
 if(attr!=UINT32_MAX&&((attr>>16)&0x20u))
  CALL(0x02194d8d,void*(*)(Actor*,uint32_t))(a,attr);
}
API void FollowingUpdate(ActorSystem *sys) {
 fwfx_tick();before(sys);
#ifdef FW_ARCEUS_SURF
 unsigned surfing=0;
 if(fwfield_configState==1&&sys&&sys->field&&!followers_off(PTR(sys->field,8))){
  void *field=sys->field,*fp=PTR(field,0x94);
  surfing=fp&&CALL(0x0219a705,unsigned(*)(void*))(fp)==2;
  /* Arm both entry draw hooks while still on land; refresh party edits on
   * every field update, including the first update after Surf starts. */
  fwfield_surfAppearance=
#ifdef FW_MOUNT
   fwtm_appearance(PTR(field,8));
  if(!fwfield_surfAppearance)fwfield_surfAppearance=
#endif
   FollowingSurfPartyAppearance(PTR(field,8));
 }
 else fwfield_surfAppearance=0;
 fwsurf_update(sys,fwfield_surfAppearance,surfing
#ifdef FW_MOUNT
  ||fwtm_exiting()
#endif
  ,fwfield_tick);
#endif
 CALL(0x021667b9,void(*)(ActorSystem*))(sys);
#ifdef FW_MOUNT
 if(fwland_active()&&owned()&&fwfield_player)fwland_follow_player((Actor*)fwfield_follower.actor,fwfield_player);
#endif
 terrain_update();
 if(owned()){
  Actor *a=(Actor*)fwfield_follower.actor;
  /* This actor's route callback writes coordinates directly, so the retail
   * move-start attribute pass never registers its shadow. The audited native
   * shadow helper owns the effect task, billboard, visibility and teardown. */
  /* First raster of the body is the retail billboard only. Shadow-task birth
   * and the white-sheet bind wait until the next update. Neither runs during
   * a fade: reason bit 16 returns before this block. */
  int shown=!(a->flags&HIDDEN);
  int firstBody=shown && !fwfield_shownNow;
  fwfield_reveal=firstBody;
  if(
#ifdef FW_MOUNT
     !fwland_active() &&
#endif
     shown && !firstBody && fwfield_player && (fwfield_player->moveflags&SHADOW_SET) &&
     !(fwfield_player->moveflags&SHADOW_VANISH) && !(a->moveflags&SHADOW_SET)){
   FollowingShadowDebug.actor=(uint32_t)a;
   FollowingShadowDebug.playerFlags=fwfield_player->moveflags;
   FollowingShadowDebug.actorFlags=a->moveflags;
   ++FollowingShadowDebug.attempts;
   CALL(0x02194df5,void(*)(Actor*))(a);
   FollowingShadowDebug.attached=!!(a->moveflags&SHADOW_SET);
  }
  /* A hidden door actor is not on screen yet. Binding its sheet on the frame
   * the world reappears, or on the body's first raster, is the scanline. */
  if(shown && !firstBody){fwfx_prepare(a);fwfx_snapshot(a);}
 }
}
int fwfield_first_body(void){return fwfield_reveal;}
API void FollowingDraw(void *system,void *camera,void *light){
 fwfield_shownNow=owned()&&!(((Actor*)fwfield_follower.actor)->flags&HIDDEN);
#ifdef FW_MOUNT
 if(fwland_active()){
  fwland_draw(system,camera,light,owned()?(Actor*)fwfield_follower.actor:0,fwfield_player,fwfield_follower.sprite_y,fwfield_tick);
  return;
 }
#endif
#ifdef FW_ARCEUS_SURF
 fwsurf_draw_scene(system,camera,light,owned()?(Actor*)fwfield_follower.actor:0,fwfield_player,fwfield_follower.sprite_y);
#else
 fwr_draw(system,camera,light,owned()?(Actor*)fwfield_follower.actor:0,fwfield_player,fwfield_follower.sprite_y);
#endif
}
API void FollowingEffectsDraw(void *system,void *camera,void *light){
 fwr_shadow_pass(system,camera,light);
 fwfx_set_player(fwfield_player&&!(fwfield_player->flags&HIDDEN)?fwfield_player:0);
 fwfx_draw(camera,light);
}
API void FollowingTerrainDraw(void *system,void *camera,void *light){
 fwr_effects_draw(system,camera,light,owned()?(Actor*)fwfield_follower.actor:0);
}
extern int FollowingOriginalUnload(void*,void*);
API int FollowingUnload(void *game,void *field) {
#ifdef FW_MOUNT
 FweMount mount;
 if(fwfield_eventsReady&&!followers_off(PTR(field,8))&&fws_menu_active()&&fwtm_menu_snapshot(field,&mount))
  FollowingEventsAPI.preserveMount(field,fwfield_generation,&mount);
 else FollowingEventsAPI.discardMount();
#endif
 fws_detach();fwfield_eventsReady=0;cleanup();fwt_unload();fwfx_destroy();fwfx_set_resource_count(0);FollowingCoreAPI.clear();
#ifdef FW_MOUNT
 fwfield_landCarry=0;
 fwtm_cancel();
#endif
#ifdef FW_ARCEUS_SURF
 fwsurf_destroy();fwfield_surfAppearance=0;
#endif
fwfield_pageCount=fwfield_stride=0;
 fwfield_owner=0;fwfield_configState=0;fwfield_registrySize=0;return FollowingOriginalUnload(game,field); }

/* Original event providers retain first refusal for every retail event. */
static void *try_talk(void *game,void *field){
#ifdef FW_MOUNT
 if(fwland_active())return 0;
#endif
 if(!owned()||fwfield_owner->field!=field||fwt_active()||!fwfield_player||fwfield_follower.state!=FW_FOLLOWING||FollowingDebug.reason)return 0;
 if(!(CALL(0x0203df29,unsigned(*)(void))()&1)||U32(field,0x148))return 0;
 void *fp=PTR(field,0x94);if(!fp)return 0;
 unsigned state=CALL(0x0219a6f9,unsigned(*)(void*))(fp);if(state!=0&&state!=3)return 0;
 if(CALL(0x0219a705,unsigned(*)(void*))(fp)!=0)return 0;
 FwPokemon selected;FwrSnapshot snapshot;
 if(!select(PTR(field,8),field,&selected,&snapshot,0,0)||!fw_same_identity(&selected,&fwfield_follower.selected))return 0;
 snapshot.direction=(uint8_t)(fwfield_player->face^1u);
 return fwt_begin(&fwfield_follower,fwfield_player,(Actor*)fwfield_follower.actor,field,game,&snapshot);
}
#ifdef FW_MOUNT
static int land_toggle(void *field){
 unsigned pressed=CALL(0x0203df29,unsigned(*)(void))();
 unsigned held=CALL(0x0203df4d,unsigned(*)(void))();
 int chord=(held&3u)==3u && (pressed&3u)!=0;
 if(chord && fwfield_landToggleTick && fwfield_landToggleTick==fwfield_tick)return 1;
 if(chord && owned() && fwfield_owner->field==field && fwfield_player &&
    fwfield_follower.state==FW_FOLLOWING && !FollowingDebug.reason && !fwt_active() && !fwfx_busy() &&
    !U32(field,0x148) && !(held&0xf0u)){
  void *fp=PTR(field,0x94);
  if(fp && CALL(0x0219a6f9,unsigned(*)(void*))(fp)==0 && CALL(0x0219a705,unsigned(*)(void*))(fp)==0){
   Actor *a=(Actor*)fwfield_follower.actor;
   fwfield_landCheckAfterPause=0;
   if(fwland_active()){
    fwland_end();a->flags|=HIDDEN;a->moveflags&=~SHADOW_VANISH;
    fw_trail_clear(&fwfield_follower.trail);clear_ease();fwfield_follower.state=FW_WAITING;
    FollowingDebug.visible=0;
   }else if(!(a->flags&HIDDEN) && fwland_begin(fwfield_owner,a,fwfield_player,field,fwfield_code,
      fwfield_registryCrc,fwfield_descriptorCount,&fwfield_follower.selected,fwfield_follower.slot)){
   fw_trail_clear(&fwfield_follower.trail);clear_ease();
   fwland_follow_player(a,fwfield_player);
    CALL(0x02006255,void(*)(unsigned))(1374u);
   }
   fwfield_landToggleTick=fwfield_tick;
   return 1; /* Consume this A press before the retail event provider. */
  }
 }
 return 0;
}
static void *try_water(void *game,void *field){
 if(!fwland_active()||!owned()||!fwfield_owner||fwfield_owner->field!=field||
    fwfield_follower.state!=FW_FOLLOWING||!fwfield_player||fwt_active()||fwfx_busy()||
    FollowingDebug.reason||U32(field,0x148))return 0;
 unsigned dir,attribute;
 if(!fwland_validate())return 0;
 int slot=fwland_slot();
 if(slot<0||!fwtm_eligible(PTR(field,8),field,&fwfield_follower.selected,
     slot,&dir,&attribute))return 0;
 Actor *a=(Actor*)fwfield_follower.actor;
 unsigned appearance=fwfield_follower.selected.species|
  (fw_follower_form(fwfield_follower.selected.species,fwfield_follower.selected.form)<<10)|
  ((unsigned)fwfield_follower.selected.gender<<18)|
  ((unsigned)fwfield_follower.selected.shiny<<26);
 /* Keep the land rider's texture allocations live while the Surf materials
  * load: GPU commands queued earlier in this frame can still use them. */
 if(!fwsurf_prepare(fwfield_owner,appearance,fwfield_tick))return 0;
 void *event=fwtm_enter(game,field,&fwfield_follower.selected,
  slot,appearance,dir,attribute);
 if(!event){fwsurf_destroy();return 0;}
 fwland_handoff(fwfield_tick);
 a->flags|=HIDDEN;fwfield_follower.state=FW_WAITING;
 fw_trail_clear(&fwfield_follower.trail);clear_ease();FollowingDebug.visible=0;
 return event;
}
#endif
API void *FollowingGridEvents(void *game,void *field,int *deleteEffect,int *menu){
#ifdef FW_MOUNT
 if(land_toggle(field)){if(deleteEffect)*deleteEffect=0;return 0;}
#endif
 void *event=CALL(0x0218151d,void*(*)(void*,void*,int*,int*))(game,field,deleteEffect,menu);
#ifdef FW_MOUNT
 if(event)fwtm_observe_exit(event,game,field);
 if(!event){event=try_water(game,field);if(event&&deleteEffect)*deleteEffect=1;}
#endif
 if(event&&menu&&*menu)fws_mark_menu(event);
 if(!event){event=try_talk(game,field);if(event)*deleteEffect=1;}return event;
}
API void *FollowingRailEvents(void *game,void *field){
#ifdef FW_MOUNT
 if(land_toggle(field))return 0;
#endif
 void *event=CALL(0x02181aa1,void*(*)(void*,void*))(game,field);
 return event?event:try_talk(game,field);
}
