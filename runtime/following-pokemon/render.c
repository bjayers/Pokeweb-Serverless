#include "render.h"
#include "render_math.h"
#include <limits.h>
/* Audited IRDO billboard/camera layouts, also used by the effect snapshots. */
typedef struct { uint16_t geom,pad; FwPoint position; uint16_t frame; int16_t sx,sy; uint16_t rotation,flags,pad2; } Billboard;
_Static_assert(sizeof(Billboard)==28,"billboard ABI");
void fwr_anchor(Actor *a,unsigned face){
 (void)face;
 if(!a)return;
 /* Control Z moves the model and its shadow together. It stays on the
  * descriptor, so turning cannot snap either one. The submitted quad does
  * not add a facing Z of its own. */
 a->dimensions[4]=a->descriptor[15];
}
/* shadowCount is +84 and shadowAt is +88. Version 2 appends:
 * +100 shadowNativeY  billboard Y before the depth bias
 * +104 groundY        follower ground the sprite is anchored to
 * +108 gap            shadowNativeY - groundY
 * +112 snap           1 when this frame's follower oval was depth-biased
 * +116 nearY +120 nearGeom +124 nearWhy
 *     nearY/nearGeom record the biased oval; nearWhy is 1 when the bias ran
 * +128 hold           always 0; slope no longer changes placement
 * Version 3 appends, leaving +128 in place:
 * +132 viewDistance   eye-to-target length, 20.12
 * +136 sortMargin     always 0; the body is not slid
 * +140 easeBlend      0 on a swap or the close pose, 65536 when idle
 * +144 depthRel       submitted follower view depth vs the player
 * policy at +24 is always 0. before/after are that view gap.
 */
typedef struct {
 uint32_t magic,version,frames,actor,size,applied;int32_t policy,before,after;
 FwPoint axis,native,submitted; int32_t nativeSX,submittedSX,shadowChange,shadowCount;
 FwPoint shadowAt;
 int32_t shadowNativeY,groundY,gap,snap,nearY,nearGeom,nearWhy,hold;
 int32_t viewDistance,sortMargin,easeBlend,depthRel;
} FwrDebug;
_Static_assert(offsetof(FwrDebug,shadowCount)==84,"shadow count");
_Static_assert(offsetof(FwrDebug,shadowAt)==88,"shadow position");
_Static_assert(offsetof(FwrDebug,shadowNativeY)==100,"native shadow Y");
_Static_assert(offsetof(FwrDebug,snap)==112,"shadow snap");
_Static_assert(offsetof(FwrDebug,hold)==128,"sprite hold");
_Static_assert(offsetof(FwrDebug,viewDistance)==132,"view distance");
_Static_assert(offsetof(FwrDebug,sortMargin)==136,"sort margin");
_Static_assert(offsetof(FwrDebug,easeBlend)==140,"ease blend");
_Static_assert(offsetof(FwrDebug,depthRel)==144,"relative depth");
__attribute__((visibility("default"))) volatile FwrDebug FollowingRenderDebug={.magic=0x44525746,.version=3};
/* Filled in the actor pass. The shadow pass uses it once, then clears it.
 * `sprite` is the submitted body. The oval bias does not follow it. */
static struct {
 FwPoint ground,sprite,player;int32_t keep;void *actorSystem;uint8_t valid,hasPlayer;
} shadowTie;
static struct {
 Actor *actor; Vec world,native; uint16_t resource;
 FwrEffectPose submitted;
 Actor *player;Vec playerWorld;FwrEffectPose playerSubmitted;
} effectPose;
/* Work points 24 bytes into a fixed-pool task; leave the complete checked
 * task span inside real RAM, including the original 192-byte tail margin. */
int fwr_grass_work_valid(const void *work){
 uintptr_t address=(uintptr_t)work;
 if(address<0x02000018u||(address&3u))return 0;
 if(address<=0x023fff40u)return 1;
 return address<=0x02ffff40u&&CALL(0x0207acb9,int(*)(void))()!=0;
}
void fwr_grass_clear(void){effectPose.actor=effectPose.player=0;}
void fwr_effects_draw(void *system,void *camera,void *light,Actor *a){
 /* Native grass quads retain their task work in renderer slot +36. Walk the
  * live slots rather than remembering entry calls: retail player steps and
  * shared/recycled tiles need the same correction as follower-created grass. */
 Billboard *changed[8];Billboard original[8];unsigned count=0;
 if(system&&camera&&a&&effectPose.actor==a&&!(a->flags&4u)
    &&effectPose.world.x==a->world.x&&effectPose.world.y==a->world.y&&effectPose.world.z==a->world.z){
  void *scene=PTR(system,4);
  FwrCamera view={*(FwPoint*)((uint8_t*)camera+32),*(FwPoint*)((uint8_t*)camera+56),U32(camera,0)};
  unsigned capacity=*(uint16_t*)((uint8_t*)system+0x1c);
  uint8_t *slots=PTR(system,0x18);
  Actor *player=effectPose.player;
  int playerLive=player&&player->system==a->system&&!(player->flags&4u)&&
   player->world.x==effectPose.playerWorld.x&&player->world.y==effectPose.playerWorld.y&&player->world.z==effectPose.playerWorld.z;
  for(unsigned i=0;scene&&slots&&capacity<=256&&i<capacity&&count<8;++i){
   uint8_t *slot=slots+i*40;
   unsigned index=U32(slot,0);if(index>=*(uint16_t*)((uint8_t*)scene+14))continue;
   uint8_t *work=PTR(slot,36);
   /* Other effect kinds also use opaque work pointers. Check the native RAM
    * range before looking for the exact fixed-pool grass descriptor. */
   if(!fwr_grass_work_valid(work))continue;
   uint8_t *task=work-24;
   /* Active task, exact grass callbacks, and the live actor stamp. Pool
    * recycling cannot turn a stale token into an unrelated NPC effect. */
   if(U32(task,4)!=1||U32(task,188)!=0x021a419du||U32(task,196)!=0x021a428du)continue;
   Actor *owner=PTR(work,8);void *controller=PTR(work,4);
   if((owner!=a&&(!playerLive||owner!=player))||!controller||PTR(controller,4)!=system
      ||*(uint16_t*)(work+40)!=owner->id||*(uint16_t*)(work+42)!=owner->model
      ||*(uint16_t*)(work+44)!=owner->zone||*(uint16_t*)(work+48)!=i
      ||*(int32_t*)(work+12)!=owner->grid[0]||*(int32_t*)(work+16)!=owner->grid[2])continue;
   Billboard *b=(Billboard*)PTR(scene,8)+index;
   if((b->geom>>14)!=0||(b->geom&0x3fffu)==0x3fffu||!(b->flags&512))continue;
   FwrPose native={b->position,b->sx,b->sy},out;
   FwrPose followerBody={{effectPose.submitted.position.x,effectPose.submitted.position.y,effectPose.submitted.position.z},effectPose.submitted.sx,effectPose.submitted.sy};
   FwrPose playerBody={{effectPose.playerSubmitted.position.x,effectPose.playerSubmitted.position.y,effectPose.playerSubmitted.position.z},effectPose.playerSubmitted.sx,effectPose.playerSubmitted.sy};
   const FwrPose *body=owner==a?&followerBody:&playerBody;
   const FwrPose *other=playerLive?(owner==a?&playerBody:&followerBody):0;
   if(fwr_cover_feet(&native,body,other,&view,&out)){
    changed[count]=b;original[count++]=*b;
    b->position=out.position;b->sx=(int16_t)out.sx;b->sy=(int16_t)out.sy;
   }
  }
 }
 CALL(0x0204f685,void(*)(void*,void*,void*))(system,camera,light);
 for(unsigned i=0;i<count;++i)*changed[i]=original[i];
}
int fwr_effect_pose(Actor *actor,const Vec *live,FwrEffectPose *out){
 if(!actor||!live||!out||effectPose.actor!=actor||
    effectPose.resource!=*(uint16_t*)(actor->descriptor+16)||
    effectPose.world.x!=actor->world.x||effectPose.world.y!=actor->world.y||effectPose.world.z!=actor->world.z)return 0;
 /* Draw has not restored yet: the live quad is already the submitted pose. */
 if(live->x==effectPose.submitted.position.x&&live->y==effectPose.submitted.position.y&&live->z==effectPose.submitted.position.z){
  *out=effectPose.submitted;return 1;
 }
 /* Restored native, or a bob that landed after the restore. Keep the
  * draw-only lift (registry Y plus two world pixels). Do not drop the
  * attached effect back to the unlifted foot. */
 int32_t lift=effectPose.submitted.position.y-effectPose.native.y;
 int64_t y=(int64_t)live->y+lift;
 if(y<INT32_MIN||y>INT32_MAX)return 0;
 *out=effectPose.submitted;
 out->position.x=live->x;out->position.y=(int32_t)y;out->position.z=live->z;
 return 1;
}
static Billboard *billboard(void *system,Actor *a){
 if(!a||!(a->flags&1)||(a->flags&4)||!a->system)return 0;
 void *fieldBl=PTR(a->system,0x28);if(!fieldBl||PTR(fieldBl,4)!=system)return 0;
 void *scene=PTR(system,4),*slots=PTR(system,0x18);
 unsigned id=*(uint16_t*)a->drawWork;
 if(!scene||!slots||id>=*(uint16_t*)((uint8_t*)system+0x1c))return 0;
 unsigned index=U32(slots,id*40);
 if(index>=*(uint16_t*)((uint8_t*)scene+14)||!PTR(scene,8))return 0;
 Billboard *b=(Billboard*)PTR(scene,8)+index;
 if((b->geom>>14)!=0||!(b->flags&512))return 0;
 return b;
}
void fwr_draw(void *system,void *camera,void *light,Actor *a,Actor *player,int sprite_y){
 Billboard *b=0,*p=0;FwrPose original={0},adjusted;int changed=0,lightingChanged=0;uint16_t originalFlags=0;
 effectPose.actor=effectPose.player=0;shadowTie.valid=shadowTie.hasPlayer=0;
 FollowingRenderDebug.frames++;FollowingRenderDebug.applied=0;FollowingRenderDebug.actor=(uint32_t)a;
 FollowingRenderDebug.policy=FollowingRenderDebug.before=FollowingRenderDebug.after=0;
 FollowingRenderDebug.hold=0;
 FollowingRenderDebug.viewDistance=FollowingRenderDebug.sortMargin=0;
 FollowingRenderDebug.easeBlend=65536;FollowingRenderDebug.depthRel=0;
 if(system&&camera&&a&&player&&a!=player&&a->system==player->system){
  b=billboard(system,a);p=billboard(system,player);
  if(b){
   original=(FwrPose){b->position,b->sx,b->sy};
   adjusted=original;
   if(p&&b!=p){
    originalFlags=b->flags;
    b->flags=(b->flags&~FWR_LIGHT_MASK)|(p->flags&FWR_LIGHT_MASK);
    lightingChanged=b->flags!=originalFlags;
   }
   FwrCamera cam={*(FwPoint*)((uint8_t*)camera+32),*(FwPoint*)((uint8_t*)camera+56),U32(camera,0)};
   int64_t py=(int64_t)player->world.y+player->drawOffset.y+player->externalOffset.y;
   int32_t playerY=py<INT32_MIN||py>INT32_MAX?player->world.y:(int32_t)py;
   /* Registry Y replaces descriptor byte 14 on this quad only. Every facing
    * uses that one offset, then two world pixels of body lift. The engine hop
    * stays in the native billboard. The camera does not move the quad, no
    * facing adds Z, and the actor and its oval are not written. */
   int delta=sprite_y-(int8_t)a->descriptor[14]+2;
   int64_t y=(int64_t)adjusted.position.y+(int64_t)delta*4096;
   if(delta&&y>=INT32_MIN&&y<=INT32_MAX){adjusted.position.y=(int32_t)y;changed=1;}
   int swapping=0,easing=0;uint32_t traveled=0;
   fw_depth_query(&swapping,&easing,&traveled);
   uint32_t ease_s=65536u;
   if(swapping)ease_s=0;
   else if(easing)ease_s=fw_ease_smoothstep(traveled,FW_EASE_TRAVEL);
   FollowingRenderDebug.viewDistance=(int32_t)fwr_view_distance(&cam);
   FollowingRenderDebug.easeBlend=(int32_t)ease_s;
   FollowingRenderDebug.size=a->descriptor[7];
   FollowingRenderDebug.native=original.position;
   FollowingRenderDebug.nativeSX=original.sx;
   if(p&&b!=p){
    /* The player quad is only read. Its position is not a sort handle. */
    FwrPose playerFeet={p->position,p->sx,p->sy};
    playerFeet.position.y=playerY;
    int32_t gap=fwr_view_gap(&adjusted.position,&playerFeet.position,&cam);
    FollowingRenderDebug.depthRel=gap;
    FollowingRenderDebug.before=gap;
    FollowingRenderDebug.after=gap;
   }
   FollowingRenderDebug.applied=changed;
   FollowingRenderDebug.submitted=adjusted.position;
   FollowingRenderDebug.submittedSX=adjusted.sx;
   int shadowLive=(a->moveflags&0x4000u)&&!(a->moveflags&0x8000u);
   FwPoint ground={a->world.x,a->world.y,a->world.z+(int8_t)a->dimensions[4]*4096};
   if(shadowLive){
    shadowTie.ground=ground;shadowTie.sprite=adjusted.position;shadowTie.keep=0;
    shadowTie.player=(FwPoint){player->world.x,playerY,player->world.z};
    shadowTie.actorSystem=system;shadowTie.hasPlayer=1;shadowTie.valid=1;
   }
   effectPose.world=a->world;
   effectPose.native=(Vec){original.position.x,original.position.y,original.position.z};
   effectPose.resource=*(uint16_t*)(a->descriptor+16);
   effectPose.submitted=(FwrEffectPose){
    .position={adjusted.position.x,adjusted.position.y,adjusted.position.z},
    .sx=(int16_t)adjusted.sx,.sy=(int16_t)adjusted.sy};
   effectPose.actor=a;
   if(p){effectPose.player=player;effectPose.playerWorld=player->world;
    effectPose.playerSubmitted=(FwrEffectPose){{p->position.x,p->position.y,p->position.z},p->sx,p->sy};}
   if(changed){b->position=adjusted.position;b->sx=(int16_t)adjusted.sx;b->sy=(int16_t)adjusted.sy;}
  }
 }
 CALL(0x0204f685,void(*)(void*,void*,void*))(system,camera,light);
 /* Drawing consumes the quad synchronously. The player billboard is never
  * written. Restore only the follower quad and its light mask. */
 if(changed){b->position=original.position;b->sx=(int16_t)original.sx;b->sy=(int16_t)original.sy;}
 if(lightingChanged)b->flags=originalFlags;
}
/* Nearest live quad within half a tile (XZ) and two tiles (Y) of actor
 * ground. An oval that sits on the player, inside six world pixels, is not
 * the follower's. The actor scene is never walked: this system is field+0xc4. */
static Billboard *follower_oval(void *system){
 void *scene=PTR(system,4);
 uint8_t *slots=PTR(system,0x18);
 unsigned capacity=*(uint16_t*)((uint8_t*)system+0x1c);
 if(!scene||!slots||capacity>256||!PTR(scene,8))return 0;
 unsigned actors=*(uint16_t*)((uint8_t*)scene+14);
 Billboard *bills=(Billboard*)PTR(scene,8),*chosen=0;
 uint64_t best=~(uint64_t)0;
 int32_t half=(int32_t)(FW_TILE/2),tall=2*(int32_t)FW_TILE;
 for(unsigned i=0;i<capacity;++i){
  unsigned index=U32(slots,i*40);
  if(index>=actors)continue;
  Billboard *b=bills+index;
  if((b->geom>>14)!=0||(b->geom&0x3fffu)==0x3fffu||!(b->flags&512))continue;
  int64_t dx=(int64_t)b->position.x-shadowTie.ground.x;
  int64_t dy=(int64_t)b->position.y-shadowTie.ground.y;
  int64_t dz=(int64_t)b->position.z-shadowTie.ground.z;
  if(dx>half||dx<-(int64_t)half||dz>half||dz<-(int64_t)half||dy>tall||dy<-(int64_t)tall)continue;
  if(shadowTie.hasPlayer){
   int64_t px=(int64_t)b->position.x-shadowTie.player.x;
   int64_t pz=(int64_t)b->position.z-shadowTie.player.z;
   if(px<=6*4096&&px>=-6*4096&&pz<=6*4096&&pz>=-6*4096){
    uint64_t pd=(uint64_t)(px*px)+(uint64_t)(pz*pz);
    uint64_t fd=(uint64_t)(dx*dx)+(uint64_t)(dz*dz);
    if(pd<=(uint64_t)6*4096*(uint64_t)6*4096&&pd<fd)continue;
   }
  }
  uint64_t dist=(uint64_t)(dx*dx)+(uint64_t)(dz*dz);
  if(dist<best){best=dist;chosen=b;}
 }
 return chosen;
}
/* Truncate toward zero. A compiler divide of this span is an
 * __aeabi_ldivmod import, and the field package allows only the two APIs. */
static int32_t div_span(int64_t value,int64_t span){
 if(span<=0)return 0;
 if(value==INT64_MIN)return INT32_MIN;
 int neg=value<0;
 uint64_t n=neg?(uint64_t)-value:(uint64_t)value,d=(uint64_t)span,q=0,r=0;
 for(unsigned bit=0;bit<64;++bit){
  r=(r<<1)|(n>>63);n<<=1;q<<=1;
  if(r>=d){r-=d;q|=1;}
 }
 if(q>2147483647u)q=2147483647u;
 return neg?-(int32_t)q:(int32_t)q;
}
/* Move along (eye-target), scaled by that vector's largest component.
 * Negative means the quad moved away from the camera. */
static int32_t view_push(const FwrCamera *cam,FwPoint from,FwPoint to){
 int64_t vx=(int64_t)cam->eye.x-cam->target.x;
 int64_t vy=(int64_t)cam->eye.y-cam->target.y;
 int64_t vz=(int64_t)cam->eye.z-cam->target.z;
 int64_t span=vx<0?-vx:vx,ay=vy<0?-vy:vy,az=vz<0?-vz:vz;
 if(ay>span)span=ay;
 if(az>span)span=az;
 if(span<4096)return 0;
 int64_t depth=div_span((int64_t)(to.x-from.x)*vx+(int64_t)(to.y-from.y)*vy+(int64_t)(to.z-from.z)*vz,span);
 if(depth>INT32_MAX)return INT32_MAX;
 if(depth<INT32_MIN)return INT32_MIN;
 return (int32_t)depth;
}
/* Retail shadow pass. Actors have already been submitted. This call is the
 * one oval submit, in the shadow pass's own depth state. The follower oval
 * is pushed away from the camera for that submit and then restored. The
 * body, the player, and every other oval stay on the engine sample. */
void fwr_shadow_pass(void *system,void *camera,void *light){
 Billboard *chosen=0,saved={0};
 int applied=0;
 FollowingRenderDebug.shadowChange=0;FollowingRenderDebug.shadowCount=0;
 FollowingRenderDebug.shadowNativeY=0;FollowingRenderDebug.groundY=shadowTie.valid?shadowTie.ground.y:0;FollowingRenderDebug.gap=0;
 FollowingRenderDebug.snap=0;FollowingRenderDebug.nearY=0;FollowingRenderDebug.nearGeom=-1;FollowingRenderDebug.nearWhy=0;
 if(shadowTie.valid&&system&&camera&&system!=shadowTie.actorSystem){
  chosen=follower_oval(system);
  if(chosen){
   FwrCamera cam={*(FwPoint*)((uint8_t*)camera+32),*(FwPoint*)((uint8_t*)camera+56),U32(camera,0)};
   FwrPose native={chosen->position,chosen->sx,chosen->sy},out;
   if(fwr_oval_behind(&native,&cam,&out)){
    saved=*chosen;
    chosen->position=out.position;
    chosen->sx=(int16_t)out.sx;chosen->sy=(int16_t)out.sy;
    applied=1;
    FollowingRenderDebug.shadowCount=1;
    FollowingRenderDebug.shadowAt=out.position;
    FollowingRenderDebug.shadowNativeY=native.position.y;
    FollowingRenderDebug.gap=native.position.y-shadowTie.ground.y;
    FollowingRenderDebug.snap=1;
    FollowingRenderDebug.shadowChange=view_push(&cam,native.position,out.position);
    FollowingRenderDebug.nearY=native.position.y;
    FollowingRenderDebug.nearGeom=saved.geom;
    FollowingRenderDebug.nearWhy=1;
   }
  }
 }
 CALL(0x0204f685,void(*)(void*,void*,void*))(system,camera,light);
 if(applied)*chosen=saved;
 shadowTie.valid=shadowTie.hasPlayer=0;shadowTie.keep=FWR_SHADOW_KEEP;
}
