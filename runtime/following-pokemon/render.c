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
  * descriptor, so turning cannot snap either one. South's extra Z is applied
  * only to the submitted quad. North is not shifted: a quad-only -7 Z lifts
  * the artwork off this shadow. */
 a->dimensions[4]=a->descriptor[15];
}
/* shadowCount is +84 and shadowAt is +88. Version 2 appends:
 * +100 shadowNativeY  billboard Y before the depth bias
 * +104 groundY        follower ground the sprite is anchored to
 * +108 gap            shadowNativeY - groundY
 * +112 snap           0 no quad, 1 depth bias only (shadow is not moved)
 * +116 nearY +120 nearGeom +124 nearWhy
 *     nearest shadow-system quad within four tiles:
 *     0 used, 1 not a shadow quad, 2 too far, 3 player's oval, 4 bias declined
 * +128 hold           1 sprite kept on that ground (slope or yawed camera);
 *     0 standard flat camera, south +6 Z and the north 2px tuck still apply
 * Version 3 appends, leaving +128 in place:
 * +132 viewDistance   eye-to-target length, 20.12
 * +136 sortMargin     eye-ray behind target used this frame
 * +140 easeBlend      0 on a swap or the close pose, 65536 when idle
 * +144 depthRel       follower view depth vs the player before that tuck
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
 * submitted sprite so that pass can tuck the shadow behind it. */
static struct {
 FwPoint ground,sprite,player;int32_t keep;void *actorSystem;uint8_t valid,hasPlayer;
} shadowTie;
/* A rail camera yaws well off world +Z. Ordinary maps look north: the eye
 * sits south of the target, so horizontal X is only camera sway. */
static int standard_view(const FwrCamera *cam){
 int64_t dx=(int64_t)cam->eye.x-cam->target.x,dz=(int64_t)cam->eye.z-cam->target.z;
 int64_t ax=dx<0?-dx:dx,az=dz<0?-dz:dz;
 return dz>0&&az>4096&&az>=ax*8;
}
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
#ifdef FW_STOCK
/* One occluder answer per tile. A second caller in the same draw reuses it
 * even when the owner tile differs, so the 288-holder walk never runs twice. */
static struct {
 uint32_t frame,count;void *bank;uint16_t zone;int16_t tileX,tileZ;int32_t limit;uint8_t valid;
} propLimit;
static uint32_t propFrame;
/* Audited native mapper/prop-bank layouts. Borrow loaded resources only;
 * never request a chunk, change a prop, or retain a pointer across draws. */
static int32_t prop_advance_limit(Actor *owner,const FwrPose *body,const FwrCamera *camera){
 int32_t limit=2*FW_TILE;
 void *field=owner&&owner->system?PTR(owner->system,64):0;
 if(!ram_span(field,0x54))return limit;
 void *mapper=PTR(field,0x50);
 if(!ram_span(mapper,0x68))return limit;
 void *props=PTR(mapper,0x64);
 if(!ram_span(props,0x1468)||PTR(props,4)!=mapper)return limit;
 unsigned count=U32(props,0x224);void *bank=PTR(props,0x220);
 if(!count||count>512||!ram_span(bank,count*8))return limit;
 if(propLimit.valid&&propLimit.frame==propFrame)return propLimit.limit;
 int16_t tileX=(int16_t)(owner->world.x/FW_TILE),tileZ=(int16_t)(owner->world.z/FW_TILE);
 if(propLimit.valid&&propLimit.bank==bank&&propLimit.count==count&&propLimit.zone==owner->zone
    &&propLimit.tileX==tileX&&propLimit.tileZ==tileZ)return propLimit.limit;
 FwPoint base=*(FwPoint*)((uint8_t*)mapper+0x54);
 for(unsigned i=0;i<288;++i){
  uint8_t *holder=(uint8_t*)props+0x268+i*16;
  void *chunk=PTR(holder,0),*instance=PTR(holder,12);
  if(!*(uint16_t*)(holder+10)||!ram_span(chunk,0xdc)||!ram_span(instance,20)||!*(uint16_t*)chunk)continue;
  unsigned index=U32(instance,0);
  if(index>=count)continue;
  void *actor=PTR(bank,index*8);
  if(!ram_span(actor,12))continue;
  void *model=PTR(actor,0);
  if(!ram_span(model,16))continue;
  void *object=PTR(model,4);
  if(!ram_span(object,8))continue;
  void *data=PTR(object,4);
  if(!ram_span(data,64))continue;
  int scale=(int32_t)U32(data,56);
  const int16_t *box=(const int16_t*)((uint8_t*)data+44);
  if(scale<=0||scale>64*4096||box[3]<=0||box[4]<=0||box[5]<=0)continue;
  FwPoint local=*(FwPoint*)((uint8_t*)instance+4),origin=*(FwPoint*)((uint8_t*)chunk+4),corners[8];
  int64_t x=(int64_t)base.x+origin.x+local.x,y=(int64_t)base.y+origin.y+local.y,z=(int64_t)base.z+origin.z+local.z;
  if(x<INT32_MIN||x>INT32_MAX||y<INT32_MIN||y>INT32_MAX||z<INT32_MIN||z>INT32_MAX)continue;
  unsigned rotation=*(uint16_t*)((uint8_t*)instance+16),at=(rotation>>4)*4;
  int sin=*(int16_t*)(uintptr_t)(0x020946e8u+at),cos=*(int16_t*)(uintptr_t)(0x020946eau+at);
  int valid=1;
  for(unsigned j=0;j<8;++j){
   int64_t bx=(int64_t)(box[0]+((j&1)?box[3]:0))*scale/4096;
   int64_t by=(int64_t)(box[1]+((j&2)?box[4]:0))*scale/4096;
   int64_t bz=(int64_t)(box[2]+((j&4)?box[5]:0))*scale/4096;
   int64_t wx=x+(bx*cos+bz*sin)/4096,wy=y+by,wz=z+(-bx*sin+bz*cos)/4096;
   if(wx<INT32_MIN||wx>INT32_MAX||wy<INT32_MIN||wy>INT32_MAX||wz<INT32_MIN||wz>INT32_MAX){valid=0;break;}
   corners[j]=(FwPoint){(int32_t)wx,(int32_t)wy,(int32_t)wz};
  }
  if(valid){
   int32_t minX=corners[0].x,maxX=minX,minZ=corners[0].z,maxZ=minZ;
   for(unsigned j=1;j<8;++j){
    if(corners[j].x<minX)minX=corners[j].x;
    if(corners[j].x>maxX)maxX=corners[j].x;
    if(corners[j].z<minZ)minZ=corners[j].z;
    if(corners[j].z>maxZ)maxZ=corners[j].z;
   }
   /* Reject distant volumes before the perspective projection/divisions.
    * Only nearby model footprints can cross our bounded depth shifts. */
   if((int64_t)owner->world.x<minX-2ll*FW_TILE||(int64_t)owner->world.x>maxX+2ll*FW_TILE||
      (int64_t)owner->world.z<minZ-2ll*FW_TILE||(int64_t)owner->world.z>maxZ+2ll*FW_TILE)continue;
   int32_t cap=fwr_box_advance_limit(body,camera,corners);if(cap<limit)limit=cap;
  }
 }
 propLimit.frame=propFrame;propLimit.count=count;propLimit.bank=bank;propLimit.zone=owner->zone;
 propLimit.tileX=tileX;propLimit.tileZ=tileZ;propLimit.limit=limit;propLimit.valid=1;
 return limit;
}
#endif
void fwr_draw(void *system,void *camera,void *light,Actor *a,Actor *player,int sprite_y){
 Billboard *b=0,*p=0;FwrPose original={0},adjusted,playerOriginal={0},playerSaved={0};FwrResult result={0};int applied=0,changed=0,playerMoved=0,lightingChanged=0;uint16_t originalFlags=0;
#ifdef FW_STOCK
 propFrame++;
#endif
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
   if(p)playerOriginal=(FwrPose){p->position,p->sx,p->sy};
   if(p&&b!=p){
    /* Both quads use this native scene and light object. Custom follower
     * descriptors can select a different light bank than the player. Mirror
     * only the player's light mask for this submission, never its alpha,
     * polygon ID, or actor-owned presentation flags. */
    originalFlags=b->flags;
    b->flags=(b->flags&~FWR_LIGHT_MASK)|(p->flags&FWR_LIGHT_MASK);
    lightingChanged=b->flags!=originalFlags;
   }
   /* The native shadow is the ground point. South's +6 world Z and the north
    * 2px tuck move only this quad, so on a yawed rail camera or a stair they
    * draw the sprite below that shadow. Keep both on an ordinary flat map,
    * where +Z is toward the camera and the tuck sits the north artwork down
    * on the oval. A slope or a yawed view leaves the quad on the shadow. */
   FwrCamera cam={*(FwPoint*)((uint8_t*)camera+32),*(FwPoint*)((uint8_t*)camera+56),U32(camera,0)};
   int64_t py=(int64_t)player->world.y+player->drawOffset.y+player->externalOffset.y;
   int32_t playerY=py<INT32_MIN||py>INT32_MAX?player->world.y:(int32_t)py;
   int64_t rise=(int64_t)a->world.y-playerY;
   int hold=rise>FWR_SHADOW_FLAT||rise<-FWR_SHADOW_FLAT||!standard_view(&cam);
   FollowingRenderDebug.hold=hold;
   int swapping=0,easing=0;uint32_t traveled=0;
   fw_depth_query(&swapping,&easing,&traveled);
   uint32_t ease_s=65536u;
   if(swapping)ease_s=0;
   else if(easing)ease_s=fw_ease_smoothstep(traveled,FW_EASE_TRAVEL);
   int32_t margin=fwr_sort_margin(&cam,ease_s);
   FollowingRenderDebug.viewDistance=(int32_t)fwr_view_distance(&cam);
   FollowingRenderDebug.sortMargin=margin;
   FollowingRenderDebug.easeBlend=(int32_t)ease_s;
   int faceZ=!hold&&a->face==1?FWR_SOUTH_ANCHOR_Z:0;
   if(faceZ){
    int64_t z=(int64_t)adjusted.position.z+(int64_t)faceZ*4096;
    if(z>=INT32_MIN&&z<=INT32_MAX){adjusted.position.z=(int32_t)z;changed=1;}
   }
   /* Descriptor Y moves the attached shadow. The registry offset replaces it
    * on this quad only. The extra north tuck is the flat-camera case above. */
   int delta=sprite_y-(int8_t)a->descriptor[14]-( !hold&&a->face==0?FWR_NORTH_ART_Y:0);
   int64_t y=(int64_t)adjusted.position.y+(int64_t)delta*4096;
   if(delta&&y>=INT32_MIN&&y<=INT32_MAX){adjusted.position.y=(int32_t)y;changed=1;}
#ifdef FW_STOCK
   FwrPose baseline=adjusted;
#endif
   if(p&&b!=p){
    FwrPose corrected;
    FwPoint world={a->world.x,a->world.y,a->world.z},pw={player->world.x,player->world.y,player->world.z};
    /* The north depth restore pays back the 2px tuck. Skip it when the tuck
     * was not applied, or the quad is pushed off the shadow it just joined. */
    unsigned flags=(a->descriptor[7]==2?FWR_LARGE:0)|(!hold&&a->face==0?FWR_NORTH:0);
    applied=fwr_correct(&world,&pw,&adjusted,&p->position,&cam,flags,&corrected,&result);
    if(applied){adjusted=corrected;changed=1;}
    FollowingRenderDebug.size=a->descriptor[7];FollowingRenderDebug.applied=applied;
    FollowingRenderDebug.policy=result.policy;FollowingRenderDebug.before=result.before;FollowingRenderDebug.after=result.after;
    FollowingRenderDebug.axis=result.axis;FollowingRenderDebug.native=original.position;
    FollowingRenderDebug.nativeSX=original.sx;
   }
   /* Anchor height before shadow clearance. A backward eye-ray tuck may drop
    * back down to this, and no further: past it the floor covers the body. */
   int32_t footY=adjusted.position.y;
   int shadowLive=(a->moveflags&0x4000u)&&!(a->moveflags&0x8000u);
   FwPoint ground={a->world.x,a->world.y,a->world.z+(int8_t)a->dimensions[4]*4096};
   if(shadowLive){
    /* The native shadow is submitted in the later effect pass. Some lowered
     * Pokémon quads sit behind its ground plane and are darkened by it. Move
     * only the submitted quad along the camera ray; keep the shadow, screen
     * pixels, logical position, and later effect pass where they are. */
    FwrPose corrected;
    if(fwr_above_shadow(&adjusted,&ground,&cam,&corrected)){
     adjusted=corrected;changed=1;
    }
   }
   int32_t depthRel=0;
   int overlap=0;
   if(p&&b!=p){
    depthRel=fwr_view_gap(&adjusted.position,&playerOriginal.position,&cam);
    overlap=fwr_sprites_overlap(&adjusted,&playerOriginal,&cam);
   }
   FollowingRenderDebug.depthRel=depthRel;
   /* Shadow clearance can pull a trailing quad toward the camera and in
    * front of the player. Sit just behind the player. Idle draws keep the
    * one-unit target, scaled only past the ordinary field camera, unless the
    * quads overlap on flat ground and the follower is behind along the eye
    * ray: that pair gets the same distance-scaled margin as a swap. Someone
    * already in front keeps their place, and clearly nearer stays in front.
    * A swap and the close pose after it already use the deeper target and
    * give it back on the same smoothstep as the spacing. Stairs keep the
    * height policy. The one-tile cap still stops the quad at a nearby wall.
    * On flat ground the follower tuck stops at the feet. Any sort gap still
    * missing is taken by sliding the player toward the camera on the same ray. */
   int blend=ease_s<65536u;
   int flat=rise<=FWR_SHADOW_FLAT&&rise>=-FWR_SHADOW_FLAT;
   int32_t clear=fwr_clear_front(&cam);
   int32_t useMargin=fwr_follow_margin(&cam,ease_s,depthRel,overlap,flat);
   int tuckBehind=p&&b!=p&&fwr_follow_tuck(result.policy,depthRel,clear,blend,overlap,flat);
   FollowingRenderDebug.sortMargin=tuckBehind?useMargin:margin;
   int limitGround=flat&&(blend||useMargin>margin);
   if(tuckBehind&&!limitGround){
    FwrPose behind;
    if(fwr_keep_behind(&adjusted,&playerOriginal,&cam,useMargin,FWR_BEHIND_CAP,&behind)){
     adjusted=behind;changed=1;
    }
   }else if(tuckBehind){
    int32_t room=fwr_ground_back(&adjusted,&cam,footY);
    if(room>512)room-=512;else room=0;
    int32_t need=depthRel>-useMargin?depthRel+useMargin:0;
    int32_t step=need<room?need:room;
    int32_t marginEff=step-depthRel;
    if(marginEff>(int32_t)FW_TILE)marginEff=(int32_t)FW_TILE;
    if(step>=FWR_TIE_MARGIN&&marginEff>=FWR_TIE_MARGIN){
     int32_t cap=marginEff>step?marginEff:step;
     if(cap>2*(int32_t)FW_TILE)cap=2*(int32_t)FW_TILE;
     FwrPose behind;
     if(cap>=marginEff&&fwr_keep_behind(&adjusted,&playerOriginal,&cam,marginEff,cap,&behind)){
      adjusted=behind;changed=1;
     }
    }
   }
#ifdef FW_STOCK
   FwrPose bounded;
   if(fwr_cap_advance(&baseline,&adjusted,&cam,prop_advance_limit(a,&baseline,&cam),&bounded)){
    adjusted=bounded;changed=1;
   }
#endif
   if(limitGround&&tuckBehind&&p&&b!=p&&(result.axis.x||result.axis.y||result.axis.z)){
    int32_t gap=fwr_view_gap(&adjusted.position,&playerOriginal.position,&cam);
    int32_t deficit=gap>-useMargin?gap+useMargin:0;
    int32_t strong=fwr_sort_margin(&cam,0);
    if(deficit>strong)deficit=strong;
    FwrPose advanced;int32_t afterGap=0;
    if(deficit>0&&useMargin>=FWR_TIE_MARGIN&&useMargin<=(int32_t)FW_TILE&&
       fwr_player_in_front(&adjusted,&playerOriginal,&cam,result.axis,useMargin,deficit,&advanced,&afterGap)){
     playerSaved=playerOriginal;
     p->position=advanced.position;p->sx=(int16_t)advanced.sx;p->sy=(int16_t)advanced.sy;
     playerMoved=1;
    }
   }
   if(p&&result.policy){
    FwPoint pref=playerMoved?p->position:playerOriginal.position;
    int64_t dx=(int64_t)adjusted.position.x-pref.x;
    int64_t dy=(int64_t)adjusted.position.y-pref.y;
    int64_t dz=(int64_t)adjusted.position.z-pref.z;
    FollowingRenderDebug.after=result.after=(int32_t)((dx*result.axis.x+dy*result.axis.y+dz*result.axis.z)/4096);
   }
   FollowingRenderDebug.submitted=adjusted.position;
   FollowingRenderDebug.submittedSX=adjusted.sx;
   if(shadowLive){
    shadowTie.ground=ground;shadowTie.sprite=adjusted.position;
    /* Final submitted anchor, after the ground cap. The oval tuck clears its
     * near rim from this point even when the follower itself did not move. */
    shadowTie.keep=tuckBehind?useMargin:FWR_SHADOW_KEEP;
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
 /* Drawing consumes the quad synchronously. Never leave a correction for the
  * next frame, another camera, native effects, save data, or the movement trail. */
 if(changed){b->position=original.position;b->sx=(int16_t)original.sx;b->sy=(int16_t)original.sy;}
 if(playerMoved&&p){p->position=playerSaved.position;p->sx=(int16_t)playerSaved.sx;p->sy=(int16_t)playerSaved.sy;}
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
/* Secondary/shadow pass. The native oval is already on the ground. Bias it
 * behind the sprite along the eye ray, which keeps its screen position, and
 * restore the billboard before returning. Do not move it onto the sprite:
 * on stairs and rails the sprite was the quad sitting too low. */
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
    int code=fwr_shadow_match(&native,&feet,&shadowTie.sprite,&cam,shadowTie.keep,&out);
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
