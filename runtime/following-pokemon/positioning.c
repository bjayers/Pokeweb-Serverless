#include "native.h"
#include "positioning.h"
#define FWP_FILE "rom:/following/positioning.narc"
/* Members a later editor may append after the positioning table. */
#define FWP_MEMBERS 4u
static uint16_t u16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|(uint32_t)u16(p+2)<<16;}
static int read_at(void *file,unsigned at,void *out,unsigned size){
 return CALL(0x02070e55,int(*)(void*,int,unsigned))(file,(int)at,0)&&
  CALL(0x02070e6d,unsigned(*)(void*,void*,unsigned))(file,out,size)==size;
}
/* Member `index` of the open NARC: its file offset and size. The archive is
 * one positioning member as installed, or that member followed by up to
 * three more. Every section is checked against the file size. */
static int member(void *file,unsigned index,unsigned *at,unsigned *bytes){
 uint8_t head[28+8*FWP_MEMBERS],tail[8];
 unsigned size=CALL(0x02070ded,unsigned(*)(void*))(file);
 if(!read_at(file,0,head,28)||u32(head)!=0x4352414eu||u16(head+4)!=0xfeffu||u16(head+6)!=1u||
    u32(head+8)!=size||u16(head+12)!=16u||u32(head+16)!=0x46415442u)return 0;
 unsigned count=u16(head+24),fatb=u32(head+20);
 if(!count||count>FWP_MEMBERS||u16(head+26)||fatb!=12u+8u*count||index>=count||
    !read_at(file,28,head+28,8u*count))return 0;
 unsigned fntb=16u+fatb;
 if(!read_at(file,fntb,tail,8)||u32(tail)!=0x464e5442u)return 0;
 unsigned names=u32(tail+4),fimg=fntb+names;
 if(names<8u||names>256u||(names&3u)||!read_at(file,fimg,tail,8)||u32(tail)!=0x46494d47u)return 0;
 unsigned data=fimg+8u,length=u32(tail+4);
 if(length<8u||fimg+length!=size)return 0;
 unsigned start=u32(head+28+8*index),end=u32(head+32+8*index);
 if(start>end||end>length-8u)return 0;
 *at=data+start;*bytes=end-start;return 1;
}
static int record(unsigned kind,unsigned row,unsigned count,uint32_t crc,uint8_t *out){
 uint32_t file[32];uint8_t head[24];unsigned at,length,land,surf,bytes;
 CALL(0x02070ca9,void(*)(void*))(file);
 if(!CALL(0x02070ecd,int(*)(void*,const char*))(file,FWP_FILE))return 0;
 int ok=member(file,0,&at,&length)&&length>=24u&&read_at(file,at,head,sizeof(head));
 if(ok){
  land=u16(head+6);surf=u16(head+8);
  ok=u32(head)==0x4f505746u&&u16(head+4)==1u&&head[10]==12u&&head[11]==8u&&
   length==24u+land*12u+surf*8u&&(kind?surf:land)==count&&row<count;
  if(!kind)ok=ok&&u32(head+12)==crc;
  bytes=kind?8u:12u;
  if(ok)ok=read_at(file,at+24u+(kind?land*12u+row*8u:row*12u),out,bytes);
  if(ok&&!kind)for(unsigned i=0;i<4;++i)if(out[i]>FW_MAX_AUTHORED_GAP)ok=0;
  if(ok&&kind)for(unsigned i=0;i<8;++i){int value=(int8_t)out[i];if(value<-32||value>32)ok=0;}
 }
 CALL(0x02070de1,int(*)(void*))(file);
 return ok;
}
int fwp_land(unsigned row,unsigned count,uint32_t registry_crc,uint8_t out[12]){
 return record(0,row,count,registry_crc,out);
}
#ifdef FW_MOUNT
int fwp_surf(unsigned row,unsigned count,uint8_t out[8]){
 return record(1,row,count,0,out);
}
#endif
/* Zone overrides, member 1: "FWZO", u16 version 1, u16 count, then count
 * rows of u16 zone, u8 flags, u8 zero. Rows are read once per field load
 * into these arrays. A missing member, a bad header or more rows than fit
 * leaves no overrides, which is the installed archive's behavior. */
static uint16_t fwp_zoneIds[FWP_ZONE_MAX];
static uint8_t fwp_zoneFlags[FWP_ZONE_MAX];
static uint16_t fwp_zoneCount;
void fwp_zones_load(void){
 uint32_t file[32];uint8_t head[8],rows[64];unsigned at,length;
 fwp_zoneCount=0;
 CALL(0x02070ca9,void(*)(void*))(file);
 if(!CALL(0x02070ecd,int(*)(void*,const char*))(file,FWP_FILE))return;
 int ok=member(file,1,&at,&length)&&length>=8u&&read_at(file,at,head,8);
 unsigned count=ok?u16(head+6):0;
 ok=ok&&u32(head)==0x4f5a5746u&&u16(head+4)==1u&&count<=FWP_ZONE_MAX&&length>=8u+count*4u;
 for(unsigned done=0;ok&&done<count;){
  unsigned n=count-done;if(n>sizeof(rows)/4u)n=sizeof(rows)/4u;
  ok=read_at(file,at+8u+done*4u,rows,n*4u);
  for(unsigned i=0;ok&&i<n;++i){
   if(rows[i*4+3])ok=0;
   fwp_zoneIds[done+i]=u16(rows+i*4);fwp_zoneFlags[done+i]=rows[i*4+2];
  }
  done+=n;
 }
 if(ok)fwp_zoneCount=(uint16_t)count;
 CALL(0x02070de1,int(*)(void*))(file);
}
unsigned fwp_zone_flags(unsigned zone){
 for(unsigned i=0;i<fwp_zoneCount;++i)if(fwp_zoneIds[i]==zone)return fwp_zoneFlags[i];
 return 0;
}
