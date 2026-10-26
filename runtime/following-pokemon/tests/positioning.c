/* Host check of the positioning NARC reader: the installed one-member
 * archive, an appended zone-override member, and malformed variants. CALL is
 * redirected to an in-memory file before positioning.c is included. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../following.h"
#define FOLLOWING_NATIVE_H
static uint8_t archive[4096];
static unsigned archiveSize,position;
static void fs_init(void *f){(void)f;position=0;}
static int fs_open(void *f,const char *path){(void)f;position=0;return !strcmp(path,"rom:/following/positioning.narc");}
static unsigned fs_size(void *f){(void)f;return archiveSize;}
static int fs_seek(void *f,int at,unsigned whence){(void)f;if(whence||at<0||(unsigned)at>archiveSize)return 0;position=(unsigned)at;return 1;}
static unsigned fs_read(void *f,void *out,unsigned size){
 (void)f;unsigned n=position+size>archiveSize?archiveSize-position:size;
 memcpy(out,archive+position,n);position+=n;return n;
}
static int fs_close(void *f){(void)f;return 1;}
static void *native(uintptr_t address){
 switch(address){
  case 0x02070ca9:return (void*)fs_init;
  case 0x02070ecd:return (void*)fs_open;
  case 0x02070ded:return (void*)fs_size;
  case 0x02070e55:return (void*)fs_seek;
  case 0x02070e6d:return (void*)fs_read;
  case 0x02070de1:return (void*)fs_close;
 }
 assert(!"unexpected native call");return 0;
}
#define CALL(address, type) ((type)native(address))
#include "../positioning.c"
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t *p,uint32_t v){put16(p,v&0xffffu);put16(p+2,v>>16);}
static uint8_t positioningMember[24+2*12+3*8];
static void build(const uint8_t *const *members,const unsigned *sizes,unsigned count,unsigned names){
 unsigned fatb=12+8*count,fimg=16+fatb+names,data=0;
 for(unsigned i=0;i<count;++i)data+=sizes[i];
 archiveSize=fimg+8+data;assert(archiveSize<=sizeof archive);
 memset(archive,0,sizeof archive);
 put32(archive,0x4352414e);put16(archive+4,0xfeff);put16(archive+6,1);put32(archive+8,archiveSize);put16(archive+12,16);put16(archive+14,3);
 put32(archive+16,0x46415442);put32(archive+20,fatb);put16(archive+24,count);
 for(unsigned i=0,at=0;i<count;++i){put32(archive+28+8*i,at);put32(archive+32+8*i,at+sizes[i]);at+=sizes[i];}
 put32(archive+16+fatb,0x464e5442);put32(archive+20+fatb,names);put32(archive+24+fatb,4);
 put32(archive+fimg,0x46494d47);put32(archive+fimg+4,8+data);
 for(unsigned i=0,at=fimg+8;i<count;++i){memcpy(archive+at,members[i],sizes[i]);at+=sizes[i];}
}
int main(void){
 uint8_t *m=positioningMember;
 put32(m,0x4f505746);put16(m+4,1);put16(m+6,2);put16(m+8,3);m[10]=12;m[11]=8;put32(m+12,0x12345678);
 for(unsigned i=0;i<12;++i){m[24+i]=(uint8_t)(i%12);m[36+i]=(uint8_t)(11-i%12);}
 for(unsigned i=0;i<24;++i)m[48+i]=(uint8_t)(i&3);
 uint8_t row[12];
 /* The installed archive: one member, FNTB of 16. No overrides. */
 const uint8_t *one[1]={m};unsigned oneSize[1]={sizeof positioningMember};
 build(one,oneSize,1,16);
 assert(fwp_land(1,2,0x12345678,row)&&!memcmp(row,m+36,12));
 assert(!fwp_land(0,2,0,row)&&!fwp_land(2,2,0x12345678,row));
 fwp_zones_load();assert(fwp_zone_flags(448)==0&&fwp_zoneCount==0);
 /* An editor appends member 1. Positioning still reads; zones load. */
 uint8_t zones[8+3*4]={0};
 put32(zones,0x4f5a5746);put16(zones+4,1);put16(zones+6,3);
 put16(zones+8,448);zones[10]=FWP_ZONE_LARGE_SWAP;put16(zones+12,12);zones[14]=0x80;put16(zones+16,600);zones[18]=1;
 const uint8_t *two[2]={m,zones};unsigned twoSize[2]={sizeof positioningMember,sizeof zones};
 build(two,twoSize,2,8);
 assert(fwp_land(0,2,0x12345678,row)&&!memcmp(row,m+24,12));
 fwp_zones_load();
 assert(fwp_zoneCount==3&&fwp_zone_flags(448)==FWP_ZONE_LARGE_SWAP&&fwp_zone_flags(12)==0x80&&fwp_zone_flags(600)==1&&fwp_zone_flags(13)==0);
 /* Malformed overrides leave none, and never break positioning. */
 zones[19]=1;build(two,twoSize,2,8);fwp_zones_load();assert(fwp_zoneCount==0&&fwp_land(1,2,0x12345678,row));
 zones[19]=0;put16(zones+6,4);build(two,twoSize,2,8);fwp_zones_load();assert(fwp_zoneCount==0);
 put16(zones+6,3);zones[0]='X';build(two,twoSize,2,8);fwp_zones_load();assert(fwp_zoneCount==0);
 zones[0]=0x46;put16(zones+6,FWP_ZONE_MAX+1);build(two,twoSize,2,8);fwp_zones_load();assert(fwp_zoneCount==0);
 /* Broken archive tables reject both readers. */
 put16(zones+6,3);build(two,twoSize,2,8);
 put32(archive+8,archiveSize+4);assert(!fwp_land(0,2,0x12345678,row));fwp_zones_load();assert(fwp_zoneCount==0);
 build(two,twoSize,2,8);put32(archive+20,20);assert(!fwp_land(0,2,0x12345678,row));
 /* A bad member 1 entry drops the overrides only; a bad member 0 entry drops positioning. */
 build(two,twoSize,2,8);put32(archive+36,0x10000);assert(fwp_land(0,2,0x12345678,row));fwp_zones_load();assert(fwp_zoneCount==0);
 build(two,twoSize,2,8);put32(archive+32,0x10000);assert(!fwp_land(0,2,0x12345678,row));
 build(one,oneSize,1,6);assert(!fwp_land(0,2,0x12345678,row));
 puts("Positioning NARC: one-member archive, appended zone overrides, malformed tables.");
 return 0;
}
