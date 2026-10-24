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
 * +112 snap           0 no quad, 1 oval planted on the ground plane
 * +116 nearY +120 nearGeom +124 nearWhy
 *     nearest shadow-system quad within four tiles:
 *     0 used, 1 not a shadow quad, 2 too far, 3 player's oval, 4 bias declined
 * +128 hold           always 0; slope no longer changes placement
 * Version 3 appends, leaving +128 in place:
 * +132 viewDistance   eye-to-target length, 20.12
 * +136 sortMargin     ground tie-break length when policy is 2, else 0
 * +140 easeBlend      0 on a swap or the close pose, 65536 when idle
 * +144 depthRel       follower view depth vs the player, bob removed
 * policy at +24: 0 feet only, 2 ground tie-break. before/after are that view gap.
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
/* The effect pass draws the shadow after this actor pass. Remember the
 * submitted sprite and the terrain feet so that pass can plant the oval. */
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
int fwr_effect_pose(Actor *actor,const Vec *native,FwrEffectPose *out){
 if(!actor||!native||!out||effectPose.actor!=actor||
    effectPose.resource!=*(uint16_t*)(actor->descriptor+16)||
    effectPose.world.x!=actor->world.x||effectPose.world.y!=actor->world.y||effectPose.world.z!=actor->world.z||
    effectPose.native.x!=native->x||effectPose.native.y!=native->y||effectPose.native.z!=native->z)return 0;
 *out=effectPose.submitted;return 1;
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
static int ram_span(const void *pointer,unsigned bytes){
 uintptr_t at=(uintptr_t)pointer;
 if((at&3u)||at<0x02000000u||bytes>0x01000000u)return 0;
 if(at<=0x02400000u-bytes)return 1;
 return at<=0x03000000u-bytes&&CALL(0x0207acb9,int(*)(void))()!=0;
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
   /* Registry Y replaces the descriptor lift on this quad only. North-facing
    * art sits two pixels down so its feet meet the oval. Both are world Y.
    * The camera does not choose them, and south adds no extra Z. */
   int north=a->face==0?FWR_NORTH_ART_Y:0;
   int delta=sprite_y-(int8_t)a->descriptor[14]-north;
   int64_t y=(int64_t)adjusted.position.y+(int64_t)delta*4096;
   if(delta&&y>=INT32_MIN&&y<=INT32_MAX){adjusted.position.y=(int32_t)y;changed=1;}
   /* Bob and the walk cycle share this quad's Y. The art foot line is the
    * actor's ground plus the registry offset. A dip below that line is
    * planted; the hop above it stays on the follower. */
   int64_t footLine=(int64_t)a->world.y+(int64_t)(sprite_y-north)*4096;
   if(footLine>=INT32_MIN&&footLine<=INT32_MAX&&adjusted.position.y<(int32_t)footLine){
    adjusted.position.y=(int32_t)footLine;changed=1;
   }
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
    /* Bob lives in the billboard Y. The tie test uses the art foot line so a
     * hop cannot toggle the keep. The player quad is only read. A tie slides
     * along the ground, away from the camera, by a few units. It does not
     * add follow distance, and a swap keeps the mirrored pose. */
    FwrPose feet=adjusted,playerFeet={p->position,p->sx,p->sy};
    if(footLine>=INT32_MIN&&footLine<=INT32_MAX)feet.position.y=(int32_t)footLine;
    playerFeet.position.y=playerY;
    int32_t gap=fwr_view_gap(&feet.position,&playerFeet.position,&cam);
    int overlap=fwr_sprites_overlap(&feet,&playerFeet,&cam);
    FollowingRenderDebug.depthRel=gap;
    FollowingRenderDebug.before=gap;
    FollowingRenderDebug.after=gap;
    if(!swapping&&overlap&&gap>-FWR_WORLD_NUDGE&&gap<FWR_CLEAR_FRONT){
     FwPoint kept;
     if(fwr_plane_keep(&feet.position,&cam,FWR_PROP_FOREGROUND_GAP,&kept)){
      int32_t dx=kept.x-feet.position.x,dz=kept.z-feet.position.z;
      if(dx||dz){
       adjusted.position.x=kept.x;adjusted.position.z=kept.z;changed=1;
       FollowingRenderDebug.policy=2;
       int32_t ax=dx<0?-dx:dx,az=dz<0?-dz:dz;
       FollowingRenderDebug.sortMargin=ax>az?ax:az;
       FwPoint shown=adjusted.position;shown.y=feet.position.y;
       FollowingRenderDebug.after=fwr_view_gap(&shown,&playerFeet.position,&cam);
      }
     }
    }
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
/* The player's own oval sits on the player. Six units rejects that oval
 * without also rejecting a follower shadow that rests on the step between
 * them. One tile of XZ and two of Y still bound the follower's quad. */
#define FWR_SHADOW_PLAYER (6 * 4096)
/* 0 accept, 2 outside the follower window, 3 player's oval. */
static int follower_shadow(FwPoint point){
 int64_t dx=(int64_t)point.x-shadowTie.ground.x,dy=(int64_t)point.y-shadowTie.ground.y,dz=(int64_t)point.z-shadowTie.ground.z;
 if(dx>FW_TILE||dx<-FW_TILE||dz>FW_TILE||dz<-FW_TILE||dy>2*FW_TILE||dy<-2*FW_TILE)return 2;
 if(shadowTie.hasPlayer){
  int64_t px=(int64_t)point.x-shadowTie.player.x,pz=(int64_t)point.z-shadowTie.player.z;
  int64_t nearPlayer=px*px+pz*pz,nearFeet=dx*dx+dz*dz;
  /* Reject the player's oval, not the follower's, when the two stand together. */
  if(nearPlayer<nearFeet&&nearPlayer<(int64_t)FWR_SHADOW_PLAYER*FWR_SHADOW_PLAYER)return 3;
 }
 return 0;
}
/* Secondary/shadow pass. Plant the follower oval on the actor's ground
 * under the submitted body and keep it a few units further from the camera.
 * Restore the native billboard before returning. */
void fwr_shadow_pass(void *system,void *camera,void *light){
 Billboard *changed[4],original[4];unsigned count=0;
 FollowingRenderDebug.shadowChange=0;FollowingRenderDebug.shadowCount=0;
 FollowingRenderDebug.shadowNativeY=0;FollowingRenderDebug.groundY=shadowTie.ground.y;FollowingRenderDebug.gap=0;
 FollowingRenderDebug.snap=0;FollowingRenderDebug.nearY=0;FollowingRenderDebug.nearGeom=-1;FollowingRenderDebug.nearWhy=0;
 if(shadowTie.valid&&system&&camera&&system!=shadowTie.actorSystem&&ram_span(system,0x20)&&ram_span(camera,60)){
  FwPoint feet=shadowTie.ground;
  FollowingRenderDebug.groundY=feet.y;
  void *scene=PTR(system,4);uint8_t *slots=PTR(system,0x18);
  unsigned capacity=*(uint16_t*)((uint8_t*)system+0x1c);
  if(scene&&slots&&capacity&&capacity<=256&&ram_span(scene,16)&&ram_span(slots,capacity*40)){
   FwrCamera cam={*(FwPoint*)((uint8_t*)camera+32),*(FwPoint*)((uint8_t*)camera+56),U32(camera,0)};
   unsigned actors=*(uint16_t*)((uint8_t*)scene+14);Billboard *bill=PTR(scene,8);
   int64_t nearest=(int64_t)1<<62;
   for(unsigned i=0;i<capacity&&count<4;++i){
    unsigned index=U32(slots,i*40);
    if(index>=actors||!ram_span(bill,(index+1u)*sizeof(Billboard)))continue;
    Billboard *b=bill+index;
    int64_t dx=(int64_t)b->position.x-shadowTie.ground.x,dz=(int64_t)b->position.z-shadowTie.ground.z;
    int close=dx<=4*(int64_t)FW_TILE&&dx>=-4*(int64_t)FW_TILE&&dz<=4*(int64_t)FW_TILE&&dz>=-4*(int64_t)FW_TILE;
    int64_t dist=close?dx*dx+dz*dz:nearest;
    unsigned kind=b->geom>>14;
    int why=kind>1||!(b->flags&512)||b->sx<=0||b->sy<=0?1:follower_shadow(b->position);
    if(close&&dist<nearest){
     nearest=dist;FollowingRenderDebug.nearY=b->position.y;FollowingRenderDebug.nearGeom=b->geom;FollowingRenderDebug.nearWhy=why;
    }
    if(why)continue;
    FwrPose native={b->position,b->sx,b->sy},out;
    /* Same XZ as the submitted body, terrain Y from the actor. A short
     * ground keep puts the oval behind the body. Y is not assigned again,
     * and the quad is not scaled. */
    FwPoint anchor={shadowTie.sprite.x,feet.y,shadowTie.sprite.z};
    int code=fwr_shadow_on_plane(&native,&anchor,&cam,&out);
    FollowingRenderDebug.shadowNativeY=native.position.y;
    FollowingRenderDebug.gap=native.position.y-feet.y;
    if(!code){if(close&&dist==nearest&&!FollowingRenderDebug.snap)FollowingRenderDebug.nearWhy=4;continue;}
    
    changed[count]=b;original[count++]=*b;
    b->position=out.position;b->sx=(int16_t)out.sx;b->sy=(int16_t)out.sy;
    FollowingRenderDebug.shadowAt=out.position;FollowingRenderDebug.shadowCount=count;
    FollowingRenderDebug.shadowChange=out.position.z-native.position.z;
    FollowingRenderDebug.snap=code;
   }
  }
 }
 CALL(0x0204f685,void(*)(void*,void*,void*))(system,camera,light);
 for(unsigned i=0;i<count;++i)*changed[i]=original[i];
 shadowTie.valid=shadowTie.hasPlayer=0;shadowTie.keep=FWR_SHADOW_KEEP;
}
