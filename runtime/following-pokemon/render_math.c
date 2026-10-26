#include "render_math.h"
#include <limits.h>
/* Bounded freestanding division: no compiler-runtime or gameplay-module import. */
static int32_t divide(int64_t value,int32_t divisor){
 uint64_t n=value<0?(uint64_t)-value:(uint64_t)value,r=0,q=0;
 if(divisor<=0)return 0;
 for(unsigned bit=0;bit<64;++bit){r=(r<<1)|(n>>63);n<<=1;q<<=1;if(r>=(uint32_t)divisor){r-=(uint32_t)divisor;q|=1;}}
 if(q>INT32_MAX)q=INT32_MAX;
 return value<0?-(int32_t)q:(int32_t)q;
}
static uint32_t isqrt(uint64_t n){
 uint64_t bit=(uint64_t)1<<62,result=0;
 while(bit>n)bit>>=2;
 while(bit){
  if(n>=result+bit){n-=result+bit;result=(result>>1)+bit;}
  else result>>=1;
  bit>>=2;
 }
 return result>0xffffffffu?0xffffffffu:(uint32_t)result;
}
static uint32_t root(uint32_t n){
 uint32_t answer=0,bit=1u<<30;
 while(bit>n)bit>>=2;
 while(bit){if(n>=answer+bit){n-=answer+bit;answer=(answer>>1)+bit;}else answer>>=1;bit>>=2;}
 return answer;
}
static int delta(FwPoint a,FwPoint b,FwPoint *out,int32_t limit){
 int64_t x=(int64_t)a.x-b.x,y=(int64_t)a.y-b.y,z=(int64_t)a.z-b.z;
 if(x>limit||x<-limit||y>limit||y<-limit||z>limit||z<-limit)return 0;
 *out=(FwPoint){(int32_t)x,(int32_t)y,(int32_t)z};return 1;
}
static int32_t dot(FwPoint a,FwPoint b){return (int32_t)(((int64_t)a.x*b.x+(int64_t)a.y*b.y+(int64_t)a.z*b.z)/4096);}
static int axis(FwPoint d,FwPoint *out){
 int32_t m=d.x<0?-d.x:d.x,t=d.y<0?-d.y:d.y;if(t>m)m=t;t=d.z<0?-d.z:d.z;if(t>m)m=t;
 if(m<4096)return 0;
 d.x=divide((int64_t)d.x*4096,m);d.y=divide((int64_t)d.y*4096,m);d.z=divide((int64_t)d.z*4096,m);
 int32_t length=(int32_t)root((uint32_t)(d.x*d.x+d.y*d.y+d.z*d.z));
 *out=(FwPoint){divide((int64_t)d.x*4096,length),divide((int64_t)d.y*4096,length),divide((int64_t)d.z*4096,length)};return 1;
}
static int shift_depth(const FwrPose *native,const FwrCamera *camera,FwPoint direction,
                       int32_t change,FwrPose *out){
 *out=*native;
 if(change>2*FW_TILE||change<-2*FW_TILE)return 0;
 if(camera->projection==2){
  out->position.x+=divide((int64_t)direction.x*change,4096);
  out->position.y+=divide((int64_t)direction.y*change,4096);
  out->position.z+=divide((int64_t)direction.z*change,4096);
 }else{
  /* Shift on the eye ray and scale the quad so its screen pixels stay put. */
  FwPoint ray;
  if(!delta(camera->eye,native->position,&ray,1<<27))return 0;
  int32_t distance=dot(ray,direction);
  if(distance<4*FW_TILE||distance-change<4*FW_TILE)return 0;
  out->position.x+=divide((int64_t)ray.x*change,distance);
  out->position.y+=divide((int64_t)ray.y*change,distance);
  out->position.z+=divide((int64_t)ray.z*change,distance);
  out->sx+=divide(-(int64_t)native->sx*change,distance);
  out->sy+=divide(-(int64_t)native->sy*change,distance);
  if(out->sx<=0||out->sy<=0||out->sx>32767||out->sy>32767){*out=*native;return 0;}
 }
 return 1;
}
int fwr_head_position(const FwPoint *base,const FwrCamera *camera,int32_t height,FwPoint *out){
 FwPoint view,right,up;
 if(height<0||height>5*FW_TILE||!delta(camera->eye,camera->target,&view,1<<27)
    ||!axis(view,&view))return 0;
 right=(FwPoint){view.z,0,-view.x};
 /* axis expects at least one full world unit; this vector is already unit. */
 right.x*=4096;right.z*=4096;
 if(!axis(right,&right))return 0;
 up=(FwPoint){view.y*right.z,view.z*right.x-view.x*right.z,-view.y*right.x};
 if(!axis(up,&up))return 0;
 *out=(FwPoint){base->x+divide((int64_t)up.x*height,4096),
  base->y+divide((int64_t)up.y*height,4096),base->z+divide((int64_t)up.z*height,4096)};
 return 1;
}
int32_t fwr_box_advance_limit(const FwrPose *body,const FwrCamera *camera,const FwPoint corners[8]){
 FwPoint view,right,up,ray,relative;
 int32_t minX=INT32_MAX,maxX=INT32_MIN,minY=INT32_MAX,maxY=INT32_MIN;
 int32_t minDepth=INT32_MAX,maxDepth=INT32_MIN;
 if(camera->projection>2||body->sx<=0||body->sy<=0||body->sx>32767||body->sy>32767||
    !delta(camera->eye,camera->target,&view,1<<27)||!axis(view,&view))return 0;
 right=(FwPoint){view.z*4096,0,-view.x*4096};
 if(!axis(right,&right))return 0;
 up=(FwPoint){view.y*right.z,view.z*right.x-view.x*right.z,-view.y*right.x};
 if(!axis(up,&up)||!delta(camera->eye,body->position,&ray,1<<27))return 0;
 int32_t distance=dot(ray,view);
 if(distance<4*FW_TILE)return 0;
 for(unsigned i=0;i<8;++i){
  if(!delta(corners[i],body->position,&relative,16*FW_TILE))return 2*FW_TILE;
  int32_t depth=dot(relative,view),x=dot(relative,right),y=dot(relative,up);
  if(camera->projection!=2){
   if(distance-depth<4*FW_TILE)return 0;
   x=divide((int64_t)x*distance,distance-depth);y=divide((int64_t)y*distance,distance-depth);
  }
  if(x<minX)minX=x;
  if(x>maxX)maxX=x;
  if(y<minY)minY=y;
  if(y>maxY)maxY=y;
  if(depth<minDepth)minDepth=depth;
  if(depth>maxDepth)maxDepth=depth;
 }
 /* Native sprite geometry spans +/-8*sx and 0..16*sy from its foot anchor.
  * Empty texture margins can only make this conservative overlap smaller. */
 if(minX>=body->sx*8||maxX<=-body->sx*8||minY>=body->sy*16||maxY<=0||maxDepth<=0)return 2*FW_TILE;
 /* A geometric tie gap can collapse to the same native 15-bit Z value at
  * the field camera distance. Reserve the same four-unit clearance used
  * for native NPCs; never advance a baseline with less room than that. */
 return minDepth>FWR_PROP_FOREGROUND_GAP?minDepth-FWR_PROP_FOREGROUND_GAP:0;
}
int fwr_cap_advance(const FwrPose *baseline,const FwrPose *adjusted,
                    const FwrCamera *camera,int32_t limit,FwrPose *out){
 *out=*adjusted;FwPoint view,relative;
 if(limit<0||limit>2*FW_TILE||camera->projection>2||
    !delta(camera->eye,camera->target,&view,1<<27)||!axis(view,&view)||
    !delta(adjusted->position,baseline->position,&relative,8*FW_TILE))return 0;
 int32_t advance=dot(relative,view);
 if(advance<=limit)return 0;
 return shift_depth(adjusted,camera,view,limit-advance-16,out);
}
int fwr_above_shadow(const FwrPose *native,const FwPoint *ground,
                     const FwrCamera *camera,FwrPose *out){
 *out=*native;
 if(camera->projection>2||native->sx<=0||native->sy<=0||native->sx>32767||native->sy>32767)return 0;
 FwPoint direction,relative;
 if(!delta(camera->eye,camera->target,&direction,1<<27)||!axis(direction,&direction)
  ||!delta(native->position,*ground,&relative,8*FW_TILE))return 0;
 /* The shadow sits one unit above ground and spans several ground pixels.
  * Clear the whole footprint with a continuous depth clamp across bobbing
  * frames, so the player/follower overlap cannot toggle at a threshold. */
 int32_t current=dot(relative,direction),target=6*4096;
 if(current>=target)return 0;
 return shift_depth(native,camera,direction,target-current+16,out);
}
#define FWR_ZOOM_LIMIT (8u * FWR_CAMERA_REF)
_Static_assert(FWR_SORT_STRONG > FWR_SORT_BEHIND, "strong tuck");
_Static_assert(FWR_SORT_STRONG <= (int)FW_TILE, "strong tuck fits the cap");
static uint32_t view_span(const FwrCamera *camera){
 int64_t dx=(int64_t)camera->eye.x-camera->target.x;
 int64_t dy=(int64_t)camera->eye.y-camera->target.y;
 int64_t dz=(int64_t)camera->eye.z-camera->target.z;
 if(dx<0)dx=-dx;
 if(dy<0)dy=-dy;
 if(dz<0)dz=-dz;
 if(dx>=(int64_t)FWR_ZOOM_LIMIT||dy>=(int64_t)FWR_ZOOM_LIMIT||dz>=(int64_t)FWR_ZOOM_LIMIT)
  return FWR_ZOOM_LIMIT;
 return isqrt((uint64_t)dx*(uint64_t)dx+(uint64_t)dy*(uint64_t)dy+(uint64_t)dz*(uint64_t)dz);
}
/* (distance/ref)^2 in 16.16. One at the ordinary field camera, saturated
 * once the scaled margin is past the one-tile cap anyway. */
static uint32_t zoom_sq(uint32_t distance){
 if(distance<=FWR_CAMERA_REF)return 65536u;
 if(distance>=FWR_ZOOM_LIMIT)return 64u*65536u;
 int32_t q=divide((int64_t)distance<<16,(int32_t)FWR_CAMERA_REF);
 if(q<=65536)return 65536u;
 uint32_t s=(uint32_t)(((uint64_t)(uint32_t)q*(uint32_t)q)>>16);
 return s<65536u?65536u:s;
}
static int32_t scale_margin(int32_t base,uint32_t zoom){
 if(base<=0)return 0;
 uint64_t v=((uint64_t)(uint32_t)base*zoom)>>16;
 if(v<(uint32_t)base)v=(uint32_t)base;
 if(v>FW_TILE)v=FW_TILE;
 return (int32_t)v;
}
uint32_t fwr_view_distance(const FwrCamera *camera){
 if(!camera)return 0;
 return view_span(camera);
}
int32_t fwr_sort_margin(const FwrCamera *camera,uint32_t ease_s){
 uint32_t zoom=zoom_sq(camera?view_span(camera):0);
 int32_t base=scale_margin(FWR_SORT_BEHIND,zoom);
 if(ease_s>=65536u)return base;
 int32_t strong=scale_margin(FWR_SORT_STRONG,zoom);
 if(strong<base)strong=base;
 uint32_t span=(uint32_t)(strong-base);
 uint32_t drop=(uint32_t)(((uint64_t)span*ease_s)>>16);
 int32_t margin=strong-(int32_t)drop;
 return margin<base?base:margin;
}
int32_t fwr_clear_front(const FwrCamera *camera){
 uint32_t zoom=zoom_sq(camera?view_span(camera):0);
 int32_t clear=scale_margin(FWR_CLEAR_FRONT,zoom);
 /* A full tile toward the camera has to stay in front on a zoomed view. */
 if(clear>(int32_t)(FW_TILE/2))clear=(int32_t)(FW_TILE/2);
 return clear;
}
static int view_axis(const FwrCamera *camera,FwPoint *view){
 if(!camera||!delta(camera->eye,camera->target,view,1<<27)||!axis(*view,view))return 0;
 return 1;
}
int32_t fwr_view_gap(const FwPoint *point,const FwPoint *origin,const FwrCamera *camera){
 FwPoint view,relative;
 if(!point||!origin||!view_axis(camera,&view)||!delta(*point,*origin,&relative,8*(int32_t)FW_TILE))return 0;
 return dot(relative,view);
}
/* Camera basis. `view` points toward the eye. `right` and `up` span the
 * camera plane, matching the head-attachment basis. */
static int camera_plane(const FwrCamera *camera,FwPoint *view,FwPoint *right,FwPoint *up){
 FwPoint flat;
 if(!view_axis(camera,view))return 0;
 flat=(FwPoint){view->z,0,-view->x};
 flat.x*=4096;flat.z*=4096;
 if(!axis(flat,right))return 0;
 flat=(FwPoint){view->y*right->z,view->z*right->x-view->x*right->z,-view->y*right->x};
 return axis(flat,up);
}
/* Screen interval of one camera-facing quad, in 16.16. Orthographic values
 * stay in 20.12, which is the same comparison space for both sprites. */
static int screen_span(const FwrPose *pose,const FwrCamera *camera,FwPoint view,FwPoint right,FwPoint up,
                       int32_t *x0,int32_t *x1,int32_t *y0,int32_t *y1){
 FwPoint fromEye;
 if(!pose||pose->sx<=0||pose->sy<=0||pose->sx>32767||pose->sy>32767)return 0;
 if(!delta(pose->position,camera->eye,&fromEye,1<<27))return 0;
 int32_t forward=-dot(fromEye,view);
 if(forward<4*(int32_t)FW_TILE)return 0;
 int32_t rx=dot(fromEye,right),uy=dot(fromEye,up);
 int32_t half=pose->sx*8,tall=pose->sy*16;
 int32_t left=rx-half,rightEdge=rx+half,bottom=uy,top=uy+tall;
 if(camera->projection==2){*x0=left;*x1=rightEdge;*y0=bottom;*y1=top;return 1;}
 *x0=divide((int64_t)left<<16,forward);
 *x1=divide((int64_t)rightEdge<<16,forward);
 *y0=divide((int64_t)bottom<<16,forward);
 *y1=divide((int64_t)top<<16,forward);
 return 1;
}
static int span_hits(int32_t a0,int32_t a1,int32_t b0,int32_t b1){
 if(a0>a1){int32_t t=a0;a0=a1;a1=t;}
 if(b0>b1){int32_t t=b0;b0=b1;b1=t;}
 return a0<=b1&&b0<=a1;
}
int fwr_sprites_overlap(const FwrPose *a,const FwrPose *b,const FwrCamera *camera){
 FwPoint view,right,up;
 int32_t ax0,ax1,ay0,ay1,bx0,bx1,by0,by1;
 if(!a||!b||!camera||camera->projection>2)return 0;
 if(!camera_plane(camera,&view,&right,&up))return 0;
 if(!screen_span(a,camera,view,right,up,&ax0,&ax1,&ay0,&ay1))return 0;
 if(!screen_span(b,camera,view,right,up,&bx0,&bx1,&by0,&by1))return 0;
 return span_hits(ax0,ax1,bx0,bx1)&&span_hits(ay0,ay1,by0,by1);
}
int32_t fwr_follow_margin(const FwrCamera *camera,uint32_t ease_s,int32_t depth_rel,int overlap,int flat){
 int32_t margin=fwr_sort_margin(camera,ease_s);
 /* The ease already blends this gap. Stairs keep the height policy. An idle
  * overlap that is not clearly in front needs the swap-sized gap: a corner
  * step can sit a unit toward the camera after shadow clearance and still
  * lose the Z test. */
 if(ease_s<65536u||!flat||!overlap||depth_rel>fwr_clear_front(camera))return margin;
 int32_t strong=fwr_sort_margin(camera,0);
 return strong>margin?strong:margin;
}
int fwr_follow_tuck(int policy,int32_t depth_rel,int32_t clear,int easing,int overlap,int flat){
 if(policy==-1)return 1;
 if(depth_rel>clear)return 0;
 if(easing&&policy==1)return 1;
 /* Idle overlap adds a tuck only when the follower is behind or tied. A
  * front-ordered quad that is already nearer stays there, so the north
  * foreground restore is not pulled back through the player. */
 return flat&&overlap&&depth_rel<=0&&policy!=-2;
}
int fwr_keep_behind(const FwrPose *follower,const FwrPose *player,
                    const FwrCamera *camera,int32_t margin,int32_t cap,FwrPose *out){
 *out=*follower;
 if(camera->projection>2||margin<FWR_TIE_MARGIN||margin>FW_TILE||
    cap<margin||cap>2*FW_TILE||
    follower->sx<=0||follower->sy<=0||follower->sx>32767||follower->sy>32767)return 0;
 FwPoint direction,relative;
 if(!delta(camera->eye,camera->target,&direction,1<<27)||!axis(direction,&direction)
    ||!delta(follower->position,player->position,&relative,8*FW_TILE))return 0;
 int32_t current=dot(relative,direction),target=-margin;
 if(current<=target)return 0;
 int32_t change=target-current;
 if(change<-cap)change=-cap;
 return shift_depth(follower,camera,direction,change-16,out);
}
int32_t fwr_ground_back(const FwrPose *pose,const FwrCamera *camera,int32_t foot_y){
 if(!pose||!camera||camera->projection>2)return 0;
 if(pose->position.y<=foot_y)return 0;
 FwPoint view;
 if(!delta(camera->eye,camera->target,&view,1<<27)||!axis(view,&view))return 0;
 int32_t drop=pose->position.y-foot_y,limit;
 if(camera->projection==2){
  if(view.y<=0)return 2*(int32_t)FW_TILE;
  limit=divide((int64_t)drop<<12,view.y);
 }else{
  FwPoint ray;
  if(!delta(camera->eye,pose->position,&ray,1<<27)||ray.y<=0)return 2*(int32_t)FW_TILE;
  int32_t distance=dot(ray,view);
  if(distance<4*(int32_t)FW_TILE)return 0;
  limit=divide((int64_t)drop*distance,ray.y);
 }
 if(limit<0)return 0;
 if(limit>2*(int32_t)FW_TILE)return 2*(int32_t)FW_TILE;
 return limit;
}
int fwr_emote_place(const FwrPose *bubble,const FwPoint *player,
                    const FwrCamera *camera,FwrPose *out){
 *out=*bubble;
 if(!bubble||!camera||bubble->sx<=0||bubble->sy<=0||bubble->sx>32767||bubble->sy>32767)return 0;
 FwPoint view;
 if(camera->projection>2||!delta(camera->eye,camera->target,&view,1<<27)||!axis(view,&view))return 0;
 /* Three tile-pair pushes clear a canopy that overhangs toward the camera.
  * Each step stays inside shift_depth's limit and keeps the screen pixels. */
 FwrPose shifted=*bubble;
 int moved=0;
 for(int i=0;i<3;++i){
  FwrPose next;
  if(!shift_depth(&shifted,camera,view,2*(int32_t)FW_TILE,&next))break;
  shifted=next;moved=1;
 }
 /* Only a player who is actually in front of the unpushed bubble gives that
  * clearance back. Someone beside the follower must not undo it. */
 if(player){
  FwPoint relative;
  if(delta(*player,bubble->position,&relative,8*(int32_t)FW_TILE)&&dot(relative,view)>FW_TILE/2){
   FwrPose playerPose={{player->x,player->y,player->z},8192,8192};
   for(int i=0;i<4;++i){
    FwrPose behind;
    /* cap stops one unit short of shift_depth's limit; that helper subtracts 16. */
    if(!fwr_keep_behind(&shifted,&playerPose,camera,FWR_SORT_BEHIND,2*(int32_t)FW_TILE-16,&behind))break;
    shifted=behind;moved=1;
   }
  }
 }
 *out=shifted;
 return moved;
}
int fwr_shadow_tuck(const FwrPose *shadow,const FwPoint *sprite,
                    const FwrCamera *camera,int32_t keep,FwrPose *out){
 if(!shadow||!sprite||!camera||!out)return 0;
 *out=*shadow;
 
 /* Calculate a distance-scaled margin to prevent depth-buffer ties */
 int32_t scaled_keep = fwr_sort_margin(camera, 65536u);
 if(keep<scaled_keep)keep=scaled_keep;
 if(keep>(int32_t)FW_TILE)keep=(int32_t)FW_TILE;
 
 if(camera->projection>2||shadow->sx<=0||shadow->sy<=0||shadow->sx>32767||shadow->sy>32767)return 0;
 FwPoint direction;
 if(!delta(camera->eye,camera->target,&direction,1<<27)||!axis(direction,&direction))return 0;
 
 /* Flat oval: the camera-side rim is closer than the center. Clear that rim,
  * not just the center, or the near half still wins against the sprite. */
 {
  int32_t half=shadow->sx>shadow->sy?shadow->sx:shadow->sy;
  if(half<1)half=1;
  half*=8;
  if(half>(int32_t)FW_TILE)half=(int32_t)FW_TILE;
  uint32_t horiz=isqrt((uint64_t)direction.x*(uint64_t)direction.x+(uint64_t)direction.z*(uint64_t)direction.z);
  int32_t rim=divide((int64_t)half*(int32_t)horiz,4096);
  
  /* Apply scaled_keep to the rim cover check */
  int32_t cover=rim+scaled_keep;
  if(cover>(int32_t)FW_TILE)cover=(int32_t)FW_TILE;
  if(keep<cover)keep=cover;
 }
 
 /* Use scaled_keep to dictate whether this is a strong tuck or idle gap */
 int32_t cap=keep<=scaled_keep?FWR_BEHIND_CAP:(int32_t)(2*FW_TILE-64);
 FwrPose cursor=*shadow;
 int moved=0;
 
 for(int step=0;step<3;++step){
  FwPoint relative;
  if(!delta(cursor.position,*sprite,&relative,8*(int32_t)FW_TILE))break;
  int32_t ahead=dot(relative,direction);
  if(ahead<=-keep)break;
  int32_t change=-keep-ahead;
  if(change<-cap)change=-cap;
  FwrPose next;
  if(!shift_depth(&cursor,camera,direction,change-16,&next))break;
  cursor=next;moved=1;
  if(keep<=scaled_keep)break;
 }
 *out=cursor;
 return moved;
}
int fwr_shadow_behind(const FwrPose *shadow,const FwPoint *sprite,
                      const FwrCamera *camera,FwrPose *out){
 return fwr_shadow_tuck(shadow,sprite,camera,FWR_SHADOW_KEEP,out);
}
int fwr_shadow_match(const FwrPose *shadow,const FwPoint *feet,
                     const FwPoint *sprite,const FwrCamera *camera,int32_t keep,FwrPose *out){
 *out=*shadow;
 if(camera->projection>2||shadow->sx<=0||shadow->sy<=0||shadow->sx>32767||shadow->sy>32767)return 0;
 FwPoint relative;
 if(!delta(shadow->position,*feet,&relative,4*FW_TILE))return 0;
 (void)relative;
 
 /* FIX: Pass the true sprite position directly to preserve native engine depth offsets, 
  * such as the 2-pixel North tuck (FWR_NORTH_ART_Y). This ensures wide sprites 
  * push the shadow back far enough to clear their geometry. */
 return fwr_shadow_tuck(shadow,sprite,camera,keep,out);
}
int fwr_shadow_on_feet(const FwrPose *shadow,const FwPoint *feet,
                       const FwPoint *sprite,const FwrCamera *camera,FwrPose *out){
 return fwr_shadow_match(shadow,feet,sprite,camera,FWR_SHADOW_KEEP,out);
}
int fwr_oval_behind(const FwrPose *shadow,const FwrCamera *camera,FwrPose *out){
 if(!shadow||!camera||!out)return 0;
 *out=*shadow;
 /* Orthographic shift_depth cannot hold the picture still. Unknown modes
  * are not a field camera. */
 if(camera->projection>=2||shadow->sx<=0||shadow->sy<=0||shadow->sx>32767||shadow->sy>32767)return 0;
 FwPoint direction;
 if(!delta(camera->eye,camera->target,&direction,1<<27)||!axis(direction,&direction))return 0;
 /* The oval is flat. Its camera-side rim is closer than the center, and a
  * bob can drop the body onto that center. Cover both, and never chase the
  * sprite: a large keep is what walked the oval up the body. */
 int32_t half=shadow->sx>shadow->sy?shadow->sx:shadow->sy;
 if(half<1)half=1;
 half*=8;
 if(half>(int32_t)FW_TILE)half=(int32_t)FW_TILE;
 uint32_t horiz=isqrt((uint64_t)direction.x*(uint64_t)direction.x+(uint64_t)direction.z*(uint64_t)direction.z);
 int32_t rim=divide((int64_t)half*(int32_t)horiz,4096);
 int32_t back=rim+8*4096;
 if(back<FWR_SHADOW_KEEP)back=FWR_SHADOW_KEEP;
 if(back>(int32_t)FW_TILE)back=(int32_t)FW_TILE;
 return shift_depth(shadow,camera,direction,-back,out);
}
void fwr_private_scene(void *dst,const void *live,const void *actors,const void *materials,
                       uint16_t actorCount,uint16_t materialCount){
 uint8_t *d=dst;const uint8_t *s=live;unsigned i;
 if(!d)return;
 for(i=0;i<60;++i)d[i]=0;
 if(!s)return;
 d[0]=s[0];d[1]=s[1];
 for(i=0;i<12;++i)d[16+i]=s[16+i];
 for(i=0;i<8;++i)d[28+i]=s[28+i];
 {
  uintptr_t material=(uintptr_t)materials,actor=(uintptr_t)actors;
  d[4]=(uint8_t)material;d[5]=(uint8_t)(material>>8);d[6]=(uint8_t)(material>>16);d[7]=(uint8_t)(material>>24);
  d[8]=(uint8_t)actor;d[9]=(uint8_t)(actor>>8);d[10]=(uint8_t)(actor>>16);d[11]=(uint8_t)(actor>>24);
 }
 d[12]=(uint8_t)materialCount;d[13]=(uint8_t)(materialCount>>8);
 d[14]=(uint8_t)actorCount;d[15]=(uint8_t)(actorCount>>8);
 /* Same polygon id as the field scene, plus the copied tail, is what the
  * 3DS edge buffer draws as black lines. Stay off the live id. */
 d[36]=s[36]==1?2:1;
}
int fwr_world_apart(const FwPoint *follower,const FwPoint *player,unsigned face,
                   int32_t step,FwPoint *out){
 if(!follower||!player||!out)return 0;
 *out=*follower;
 if(step<FWR_TIE_MARGIN||step>(int32_t)FW_TILE)return 0;
 int64_t dx=(int64_t)follower->x-player->x,dz=(int64_t)follower->z-player->z;
 if(dx>8*(int64_t)FW_TILE||dx<-8*(int64_t)FW_TILE||dz>8*(int64_t)FW_TILE||dz<-8*(int64_t)FW_TILE)return 0;
 uint64_t adx=dx<0?(uint64_t)-dx:(uint64_t)dx,adz=dz<0?(uint64_t)-dz:(uint64_t)dz;
 uint32_t horiz=isqrt(adx*adx+adz*adz);
 int32_t ox,oz;
 if(horiz<4096){
  ox=oz=0;
  if(face==0)oz=step;
  else if(face==1)oz=-step;
  else if(face==2)ox=step;
  else if(face==3)ox=-step;
  else return 0;
 }else{
  ox=divide(dx*step,(int32_t)horiz);
  oz=divide(dz*step,(int32_t)horiz);
 }
 int64_t x=(int64_t)follower->x+ox,z=(int64_t)follower->z+oz;
 if(x<INT32_MIN||x>INT32_MAX||z<INT32_MIN||z>INT32_MAX)return 0;
 out->x=(int32_t)x;out->y=follower->y;out->z=(int32_t)z;
 return 1;
}
int fwr_plane_keep(const FwPoint *feet,const FwrCamera *camera,int32_t viewKeep,FwPoint *out){
 if(!feet||!camera||!out)return 0;
 *out=*feet;
 if(camera->projection>2||viewKeep<FWR_TIE_MARGIN||viewKeep>(int32_t)FW_TILE)return 0;
 FwPoint view;
 if(!delta(camera->eye,camera->target,&view,1<<27)||!axis(view,&view))return 0;
 int64_t vx=view.x,vz=view.z;
 uint32_t horiz=isqrt((uint64_t)(vx*vx)+(uint64_t)(vz*vz));
 if(horiz<256)return 1;
 int32_t slide=divide((int64_t)viewKeep*4096,(int32_t)horiz);
 if(slide<FWR_SHADOW_KEEP)slide=FWR_SHADOW_KEEP;
 if(slide>FWR_PLANE_CAP)slide=FWR_PLANE_CAP;
 int32_t mx=divide((int64_t)view.x*slide,(int32_t)horiz);
 int32_t mz=divide((int64_t)view.z*slide,(int32_t)horiz);
 int64_t x=(int64_t)feet->x-mx,z=(int64_t)feet->z-mz;
 if(x<INT32_MIN||x>INT32_MAX||z<INT32_MIN||z>INT32_MAX)return 1;
 out->x=(int32_t)x;out->y=feet->y;out->z=(int32_t)z;
 return 1;
}
int fwr_shadow_on_plane(const FwrPose *shadow,const FwPoint *feet,
                        const FwrCamera *camera,FwrPose *out){
 if(!shadow||!feet||!camera||!out)return 0;
 *out=*shadow;
 if(camera->projection>2||shadow->sx<=0||shadow->sy<=0||shadow->sx>32767||shadow->sy>32767)return 0;
 FwPoint planted;
 if(!fwr_plane_keep(feet,camera,FWR_PROP_FOREGROUND_GAP,&planted))return 0;
 out->position=planted;
 return 1;
}
int fwr_cover_feet(const FwrPose *grass,const FwrPose *body,
                   const FwrPose *other,const FwrCamera *camera,FwrPose *out){
 *out=*grass;
 FwPoint direction,relative;
 if(camera->projection>2||!delta(camera->eye,camera->target,&direction,1<<27)||!axis(direction,&direction)
    ||!delta(grass->position,body->position,&relative,8*FW_TILE))return 0;
 /* Native geom type zero uses the inverse camera rotation for both actor
  * and grass quads: every vertex shares its center's camera-normal depth.
  * Keep the grass in its owner's depth interval, rather than advancing it
  * in front of both actors. Its native height still counts toward depth. */
 int32_t current=dot(relative,direction),lower=4*4096,upper=INT32_MAX;
 if(other&&delta(other->position,body->position,&relative,8*FW_TILE)){
  int32_t gap=dot(relative,direction);
  if(gap>0){
   if(gap<256)return 0;
   int32_t margin=divide(gap,3);if(margin>lower)margin=lower;
   lower=margin;upper=gap-margin;
  }
 }
 if(current>=lower&&current<=upper)return 0;
 int32_t change=current<lower?lower-current+16:upper-current-16;
 if(camera->projection==2)change=divide((int64_t)change*4096,dot(direction,direction));
 return shift_depth(grass,camera,direction,change,out);
}
int fwr_correct(const FwPoint *world,const FwPoint *player_world,
 const FwrPose *native,const FwPoint *player_draw,const FwrCamera *camera,
 unsigned flags,FwrPose *out,FwrResult *result){
 *out=*native;*result=(FwrResult){0};
 FwPoint direction,relative,base;
 if(camera->projection>2||native->sx<=0||native->sy<=0||native->sx>32767||native->sy>32767
  ||!delta(camera->eye,camera->target,&direction,1<<27)||!axis(direction,&direction)
  ||!delta(*world,*player_world,&base,4*FW_TILE)||!delta(native->position,*player_draw,&relative,8*FW_TILE))return 0;
 result->axis=direction;result->before=result->after=dot(relative,direction);
 /* Base positions choose ordering; cosmetic bobbing never changes that choice.
  * Elevation cases retain the separately tested stair policies for large art. */
 FwPoint horizontal=direction;horizontal.y=0;
 int32_t horizontal_length=(int32_t)root((uint32_t)(horizontal.x*horizontal.x+horizontal.z*horizontal.z));
 if(horizontal_length<256)return 0;
 int32_t h=divide((int64_t)dot(base,horizontal)*4096,horizontal_length),target=-FWR_TIE_MARGIN;
 int front=h>FW_TILE/2,unequal=base.y>8192||base.y<-8192,lower=0,cap_back=0;
 if(flags&(FWR_FORCE_FRONT|FWR_FORCE_BACK)){
  front=!!(flags&FWR_FORCE_FRONT);
  /* Surf's seated rider and mount have overlapping quads. A tie-sized depth
   * gap can quantize away as the rider bobs, switching their visible order. */
  target=front?4*4096:-4*4096;
 }else if(unequal){
  /* A higher follower north of the player (south stairs, south ledges) used
   * to keep native order and then draw in front. Clamp that quad behind.
   * Large sprites already in front, and large sprites below-and-behind,
   * keep their previous stair targets. */
  if(front){
   if(!(flags&FWR_LARGE))return 0;
   target=8*4096;
  }else if((flags&FWR_LARGE)&&base.y<=0){
   lower=1;target=-4*4096;
  }else{target=-FWR_SORT_BEHIND;cap_back=1;}
 }else if(front){
  target=FWR_TIE_MARGIN;
  if((flags&FWR_NORTH)&&h<=3*FW_TILE/2){
   /* North artwork is tucked two pixels down and shares the shadow's ground
    * plane. Restore the depth that tuck costs. The old -7 Z shift is not
    * applied, so it is not added back here. */
   FwPoint lost={(int32_t)0,FWR_NORTH_ART_Y*4096,(int32_t)0};
   int32_t previousDepth=result->before+dot(lost,direction);
   if(previousDepth>target)target=previousDepth;
  }
 }
 result->policy=lower?-2:(front?1:-1);
 int32_t current=result->before;
 if(lower?current>=target:(front?current>=target:current<=target))return 0;
 int32_t change=target-current;
 /* A pose already far in front must not be driven through a nearby wall. */
 if(cap_back&&change<-FWR_BEHIND_CAP)change=-FWR_BEHIND_CAP;
 /* Cover fixed-point normalization and translation rounding at submission. */
 change+=change<0?-16:16;
 /* A corrupt/disconnected pose must never cause a large visual teleport. */
 if(!shift_depth(native,camera,direction,change,out))return 0;
 if(!delta(out->position,*player_draw,&relative,8*FW_TILE)){*out=*native;return 0;}
 result->after=dot(relative,direction);return 1;
}
/* 1 when [a0,a1]/fa and [b0,b1]/fb overlap. Forward distances are
 * positive, so cross-multiplying compares without a divide. */
static int scaled_hits(int32_t a0,int32_t a1,int64_t fa,int32_t b0,int32_t b1,int64_t fb){
 return (int64_t)a0*fb<=(int64_t)b1*fa&&(int64_t)b0*fa<=(int64_t)a1*fb;
}
int fwr_basis(const FwrCamera *camera,FwrBasis *out){
 /* The head-attachment basis, without renormalizing `up`, which is the
  * cross of two unit vectors. */
 if(!camera||!out||camera->projection>2||!view_axis(camera,&out->view))return 0;
 FwPoint view=out->view;
 uint32_t flat=root((uint32_t)(view.x*view.x+view.z*view.z));
 if(flat<64u)return 0;
 out->right=(FwPoint){divide((int64_t)view.z*4096,(int32_t)flat),0,divide(-(int64_t)view.x*4096,(int32_t)flat)};
 FwPoint r=out->right;
 out->up=(FwPoint){(int32_t)((int64_t)view.y*r.z/4096),(int32_t)(((int64_t)view.z*r.x-(int64_t)view.x*r.z)/4096),
  (int32_t)(-(int64_t)view.y*r.x/4096)};
 /* Ground direction toward the eye. `up` leans back, so its share of that
  * direction is negative: -sin(pitch). */
 out->toward=(FwPoint){divide((int64_t)view.x*4096,(int32_t)flat),0,divide((int64_t)view.z*4096,(int32_t)flat)};
 out->upToward=dot(out->up,out->toward);
 out->eye=camera->eye;out->flat=(int32_t)flat;out->ortho=camera->projection==2;
 return 1;
}
/* The pose `low` world units up `body`'s camera plane, `pull` toward the
 * eye along that point's ray, scaled so its pixels stay put. */
static int band_pose(const FwrBasis *b,const FwrPose *body,int32_t distance,int32_t low,int32_t pull,
                     int64_t q,int32_t band_sy,FwrPose *o){
 FwPoint junction={body->position.x+(int32_t)((int64_t)b->up.x*low/4096),
  body->position.y+(int32_t)((int64_t)b->up.y*low/4096),body->position.z+(int32_t)((int64_t)b->up.z*low/4096)};
 *o=(FwrPose){junction,body->sx,band_sy};
 if(pull<=0)return 1;
 if(b->ortho){
  o->position.x+=(int32_t)((int64_t)b->view.x*pull/4096);
  o->position.y+=(int32_t)((int64_t)b->view.y*pull/4096);
  o->position.z+=(int32_t)((int64_t)b->view.z*pull/4096);
  return 1;
 }
 /* Every junction is in the body's camera plane, so they share `distance`. */
 if(distance-pull<4*(int32_t)FW_TILE)return 0;
 if(q<0)q=divide((int64_t)pull<<16,distance);
 FwPoint toEye={b->eye.x-junction.x,b->eye.y-junction.y,b->eye.z-junction.z};
 o->position.x+=(int32_t)(toEye.x*q/65536);
 o->position.y+=(int32_t)(toEye.y*q/65536);
 o->position.z+=(int32_t)(toEye.z*q/65536);
 o->sx=(int32_t)(body->sx-(int64_t)body->sx*q/65536);
 o->sy=(int32_t)(band_sy-(int64_t)band_sy*q/65536);
 return o->sx>0&&o->sy>0;
}
int fwr_strips(const FwrPose *body,const FwrCamera *camera,int32_t half,int32_t top,int32_t strip,
               unsigned count,const FwrStack *stack,int32_t strip_sy,const FwrBody *others,unsigned other_count,
               FwrPose *out,uint16_t *limited,int32_t *lift){
 if(!body||!camera||!stack||!out||!count||count>16||other_count>FWR_OTHERS_MAX||(other_count&&!others))return 0;
 int32_t safe=stack->safe,lean=stack->lean,ahead=stack->forward,ramp=stack->ramp;
 if(camera->projection>2||body->sx<=0||body->sx>32767||strip_sy<=0||strip_sy>32767||half<=0||
    strip<=0||top>5*(int32_t)FW_TILE||top-(int64_t)strip*count<0||safe<0||lean<0||lean>4096||
    ahead<-5*(int32_t)FW_TILE||ahead>2*(int32_t)FW_TILE||ramp<=0||ramp>4*(int32_t)FW_TILE)return 0;
 if(limited)*limited=0;
 for(unsigned k=0;lift&&k<other_count;++k)lift[k]=0;
 FwrBasis b;FwPoint ray;
 if(!fwr_basis(camera,&b)||!delta(camera->eye,body->position,&ray,1<<27))return 0;
 int32_t distance=dot(ray,b.view);
 if(distance<4*(int32_t)FW_TILE)return 0;
 int32_t eyeToward=dot(ray,b.toward);
 /* Screen rectangles, as camera-plane spans over forward distance. Every
  * strip shares the body's columns and forward distance. A sprite's bottom
  * quarter is its feet band; with `lift` the rest is redrawn over the
  * strips instead of holding them back. */
 int64_t mineF=b.ortho?1:distance;
 int32_t mineX=-dot(ray,b.right),mineY=-dot(ray,b.up);
 struct { int64_t f; int32_t y0,feet,y1,gap; uint8_t k; } near[FWR_OTHERS_MAX];
 unsigned nears=0;
 for(unsigned k=0;k<other_count;++k){
  const FwrBody *o=&others[k];
  FwPoint them,relative;
  if(o->half<=0||o->height<=0||!delta(o->feet,camera->eye,&them,1<<27)||
     !delta(o->feet,body->position,&relative,8*(int32_t)FW_TILE))continue;
  int32_t forward=-dot(them,b.view),gap=dot(relative,b.view);
  /* Clearly behind the follower: drawing over it is right. */
  if(forward<4*(int32_t)FW_TILE||gap<-FWR_CLEAR_FRONT)continue;
  int64_t f=b.ortho?1:forward;
  int32_t x=dot(them,b.right),y=dot(them,b.up);
  if(!scaled_hits(mineX-half,mineX+half,mineF,x-o->half,x+o->half,f))continue;
  near[nears].f=f;near[nears].y0=y;near[nears].y1=y+o->height;
  near[nears].feet=lift?y+o->height/4:y+o->height;near[nears].gap=gap;near[nears].k=(uint8_t)k;++nears;
 }
 for(unsigned i=0;i<count;++i){
  int32_t high=top-(int32_t)i*strip,low=high-strip;
  /* A camera-facing quad leans back by its height. Each strip comes
   * forward along its eye ray until its top edge stands, measured along
   * the ground, `forward` (ramped in over `ramp` of height) plus `lean` per
   * unit of height past `safe` in front of the hinge. The top gains `dh`
   * ground units; moving a fraction k of the way to the eye gains k times
   * its ground distance to the eye, and pulls the strip k * distance. */
  int32_t pull=0;int64_t q=-1;
  int32_t past=high-safe;
  int32_t dh=past>0?(int32_t)((int64_t)past*(lean-b.upToward)/4096)+divide((int64_t)ahead*(past<ramp?past:ramp),ramp):0;
  if(dh>0){
   if(b.ortho)pull=divide((int64_t)dh*4096,b.flat);
   else{
    /* A top that cannot reach that ground point before the eye takes the
     * cap below, as a steep camera does. */
    int32_t gain=eyeToward-(int32_t)((int64_t)high*b.upToward/4096);
    if(gain<=dh)pull=FWR_SPLIT_CAP+1;
    else{q=divide((int64_t)dh<<16,gain);pull=(int32_t)(q*distance/65536);}
   }
  }
  int32_t limit=FWR_SPLIT_CAP;
  /* A sprite's feet band this strip covers on screen keeps it a unit
   * behind; level with the follower, it holds the strip still. */
  for(unsigned k=0;k<nears&&pull>0;++k){
   if(!scaled_hits(mineY+low,mineY+high,mineF,near[k].y0,near[k].feet,near[k].f))continue;
   int32_t allowed=near[k].gap>FWR_SORT_BEHIND?near[k].gap-FWR_SORT_BEHIND:0;
   if(allowed<limit){limit=allowed;if(limited&&pull>allowed)*limited|=(uint16_t)(1u<<i);}
  }
  if(pull>limit){pull=limit;q=-1;}
  /* The rest of a sprite this strip covers comes forward over it. */
  for(unsigned k=0;lift&&k<nears&&pull>0;++k){
   if(near[k].feet>=near[k].y1||!scaled_hits(mineY+low,mineY+high,mineF,near[k].feet,near[k].y1,near[k].f))continue;
   int32_t need=pull-near[k].gap+FWR_SORT_BEHIND;
   if(need>lift[near[k].k])lift[near[k].k]=need;
  }
  if(!band_pose(&b,body,distance,low,pull,q,strip_sy,&out[i]))return 0;
 }
 return 1;
}
