#include "../following.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FwSample sample(int x, int y, int z, unsigned gen, unsigned kind) {
    FwSample s = {0}; s.x=x;s.y=y;s.z=z;s.generation=gen;s.kind=kind;s.connection=0xffff;return s;
}
/* 1x1 follower and NPC, same height. Matches scene occupancy for that volume. */
static int npc_tile(int fx, int fz, int x0, int z0, int x1, int z1) {
    int xmin=x0<x1?x0:x1, xmax=x0>x1?x0:x1, zmin=z0<z1?z0:z1, zmax=z0>z1?z0:z1;
    return xmin<=fx && xmax>=fx && zmin<=fz && zmax>=fz;
}
static int near_eq(int32_t a, int32_t b) { int32_t d = a - b; return d < 64 && d > -64; }
/* The swap finish keeps `natural` (no jump). Walking or running then eases the
 * metric keep to one tile, which is the fresh-load pose. A stop holds it. */
static void ease_line(int along, unsigned direction, int32_t *doneBehind) {
    const uint8_t gaps[4] = {6, 6, 6, 6};
    const int32_t player0 = 24 * (int32_t)FW_TILE + 4 * 4096;
    const int32_t naturalX = player0 - (int32_t)FW_TILE;
    for (int speed = 1; speed <= 2; ++speed) {
        FwTrail fresh, swapped;
        FwSample pose, after;
        fw_trail_clear(&fresh);
        for (int32_t x = 0;;) {
            FwSample s = sample(x, 0, 0, 1, FW_WORLD);
            s.direction = (uint8_t)direction;
            assert(fw_trail_push_directional(&fresh, &s, &pose, gaps) >= 0);
            if (x == player0) break;
            int32_t next = x + speed * 4096;
            if (next > player0) next = player0;
            x = next;
        }
        FwSample far = sample(naturalX, 0, 0, 1, FW_WORLD);
        FwSample at = sample(player0, 0, 0, 1, FW_WORLD);
        far.direction = at.direction = (uint8_t)direction;
        int seeded = along ? fw_trail_reseed_along(&swapped, &far, &at, gaps, &after)
                           : fw_trail_reseed(&swapped, &far, &at, gaps, &after);
        assert(seeded == 1);
        assert(after.direction == direction);
        assert(after.x < naturalX && naturalX - after.x > (int32_t)FW_TILE / 4);
        uint32_t close = fw_trail_metric_to_point(&swapped, gaps, naturalX, 0);
        assert(close > 0 && close < FW_TILE);
        FwSample held;
        assert(fw_trail_pose_behind(&swapped, gaps, close, &held) == 1);
        assert(near_eq(held.x, naturalX) && held.z == 0);
        assert(fw_trail_pose_behind(&swapped, gaps, FW_TILE, &held) == 1);
        assert(held.x == after.x && held.y == after.y && held.z == after.z);
        int finishTile = fw_tile(naturalX), playerTile = fw_tile(player0);
        assert(finishTile != playerTile);
        assert(npc_tile(finishTile, 0, finishTile, 0, finishTile, 0));
        assert(!npc_tile(finishTile, 0, playerTile, 0, playerTile, 0));
        assert(!npc_tile(finishTile, 0, finishTile + 2, 0, finishTile + 2, 0));
        int32_t player = player0, kept = naturalX;
        uint32_t traveled = 0;
        int sawHold = 0, recorded = 0;
        for (int n = 0; n < 48; ++n) {
            int32_t prev = player;
            player += speed * 4096;
            FwSample s = sample(player, 0, 0, 1, FW_WORLD);
            s.direction = (uint8_t)direction;
            assert(fw_trail_push_directional(&fresh, &s, &pose, gaps) == 1);
            assert(fw_trail_push_directional(&swapped, &s, &after, gaps) == 1);
            assert(after.direction == direction);
            uint32_t nextTravel = traveled + fw_travel(prev, 0, player, 0);
            FwSample shown = after;
            if (nextTravel < FW_EASE_TRAVEL) {
                traveled = nextTravel;
                uint32_t keep = fw_ease_keep(traveled, FW_EASE_TRAVEL, close, FW_TILE);
                assert(fw_trail_pose_behind(&swapped, gaps, keep, &shown) == 1);
                shown.direction = after.direction;
                assert(shown.x > kept);
                assert(player - shown.x > (int32_t)FW_TILE);
                assert(player - shown.x < player - after.x);
            } else {
                traveled = nextTravel;
                shown = after;
            }
            kept = shown.x;
            int tile = fw_tile(shown.x), ptile = fw_tile(player);
            assert(npc_tile(tile, 0, tile, 0, tile, 0));
            assert(tile != ptile && !npc_tile(tile, 0, ptile, 0, ptile, 0));
            assert(!npc_tile(tile, 0, tile + 2, 0, tile + 2, 0));
            if (!sawHold && traveled >= FW_TILE && traveled < FW_EASE_TRAVEL) {
                sawHold = 1;
                int32_t frozen = shown.x;
                uint32_t frozenTravel = traveled;
                for (int idle = 0; idle < 5; ++idle) {
                    FwSample idleOut;
                    assert(fw_trail_push_directional(&swapped, &s, &idleOut, gaps) == 0);
                    assert(fw_trail_push_directional(&fresh, &s, &idleOut, gaps) == 0);
                }
                FwSample again;
                uint32_t keep = fw_ease_keep(frozenTravel, FW_EASE_TRAVEL, close, FW_TILE);
                assert(fw_trail_pose_behind(&swapped, gaps, keep, &again) == 1);
                assert(again.x == frozen && traveled == frozenTravel);
                assert(player - frozen > (int32_t)FW_TILE + 64);
                assert(player - frozen < player - after.x - 64);
            }
            if (traveled >= FW_EASE_TRAVEL) {
                assert(near_eq(shown.x, pose.x) && shown.z == pose.z);
                assert(player - shown.x > (int32_t)FW_TILE + (int32_t)FW_TILE / 4);
                assert(fw_tile(shown.x) == fw_tile(pose.x));
                if (!recorded) {
                    recorded = 1;
                    doneBehind[speed - 1] = player - shown.x;
                    /* The grown gap has crossed a tile the close offset never occupied. */
                    assert(fw_tile(shown.x) != fw_tile(player - (int32_t)FW_TILE));
                }
            }
        }
        assert(sawHold && recorded && traveled >= FW_EASE_TRAVEL);
    }
}
static void ease_bend(void) {
    const uint8_t gaps[4] = {6, 6, 6, 6};
    const unsigned face = 2;
    FwPoint player0 = {10 * (int32_t)FW_TILE, 0, 10 * (int32_t)FW_TILE};
    FwSample from = sample(player0.x - 3 * (int32_t)FW_TILE, 0, player0.z - 4 * (int32_t)FW_TILE, 1, FW_WORLD);
    FwSample at = sample(player0.x, 0, player0.z, 1, FW_WORLD);
    from.direction = at.direction = face;
    for (int speed = 1; speed <= 2; ++speed) {
        FwTrail swapped, fresh;
        FwSample after, pose;
        assert(fw_trail_reseed_along(&swapped, &from, &at, gaps, &after) == 1);
        assert(fw_trail_reseed_along(&fresh, &from, &at, gaps, &pose) == 1);
        assert(after.direction == face && pose.direction == face);
        uint32_t rest = fw_travel(after.x, after.z, at.x, at.z);
        assert(rest > FW_TILE);
        int32_t nx = after.x, nz = after.z;
        uint32_t best = 0xffffffffu;
        for (unsigned i = 0; i < swapped.count; ++i) {
            const FwSample *sm = &swapped.samples[(swapped.head + i) % FW_TRAIL_CAPACITY];
            uint32_t d = fw_travel(sm->x, sm->z, at.x, at.z);
            if (d >= rest || d < FW_TILE / 2) continue;
            uint32_t err = d > FW_TILE ? d - FW_TILE : FW_TILE - d;
            if (err < best) { best = err; nx = sm->x; nz = sm->z; }
        }
        assert(best < 4096u);
        assert(nz != at.z);
        uint32_t close = fw_trail_metric_to_point(&swapped, gaps, nx, nz);
        assert(close > 0 && close < FW_TILE);
        FwSample held;
        assert(fw_trail_pose_behind(&swapped, gaps, close, &held) == 1);
        assert(near_eq(held.x, nx) && near_eq(held.z, nz));
        assert(fw_trail_pose_behind(&swapped, gaps, FW_TILE, &held) == 1);
        assert(held.x == after.x && held.z == after.z);
        int sawHold = 0;
        uint32_t traveled = 0;
        FwPoint player = player0;
        for (int n = 0; n < 40; ++n) {
            FwPoint prev = player;
            player.x += speed * 4096;
            FwSample s = sample(player.x, 0, player.z, 1, FW_WORLD);
            s.direction = face;
            assert(fw_trail_push_directional(&fresh, &s, &pose, gaps) == 1);
            assert(fw_trail_push_directional(&swapped, &s, &after, gaps) == 1);
            assert(after.direction == face && pose.direction == face);
            uint32_t nextTravel = traveled + fw_travel(prev.x, prev.z, player.x, player.z);
            FwSample shown = after;
            if (nextTravel < FW_EASE_TRAVEL) {
                traveled = nextTravel;
                uint32_t keep = fw_ease_keep(traveled, FW_EASE_TRAVEL, close, FW_TILE);
                assert(fw_trail_pose_behind(&swapped, gaps, keep, &shown) == 1);
                uint32_t gap = fw_travel(shown.x, shown.z, player.x, player.z);
                uint32_t full = fw_travel(after.x, after.z, player.x, player.z);
                assert(gap + 64 < full);
            } else {
                traveled = nextTravel;
                shown = after;
            }
            int fx = fw_tile(shown.x), fz = fw_tile(shown.z);
            int px = fw_tile(player.x), pz = fw_tile(player.z);
            assert(npc_tile(fx, fz, fx, fz, fx, fz));
            assert(!npc_tile(fx, fz, fx + 2, fz + 2, fx + 2, fz + 2));
            if (fx != px || fz != pz) assert(!npc_tile(fx, fz, px, pz, px, pz));
            if (!sawHold && traveled >= FW_TILE && traveled < FW_EASE_TRAVEL) {
                sawHold = 1;
                int32_t fx0 = shown.x, fz0 = shown.z;
                for (int idle = 0; idle < 4; ++idle) {
                    FwSample idleOut;
                    assert(fw_trail_push_directional(&swapped, &s, &idleOut, gaps) == 0);
                    assert(fw_trail_push_directional(&fresh, &s, &idleOut, gaps) == 0);
                }
                FwSample again;
                assert(fw_trail_pose_behind(&swapped, gaps, fw_ease_keep(traveled, FW_EASE_TRAVEL, close, FW_TILE), &again) == 1);
                assert(again.x == fx0 && again.z == fz0);
            }
            if (traveled >= FW_EASE_TRAVEL) {
                assert(shown.x == pose.x && shown.z == pose.z);
                assert(shown.z == player.z && nz != player.z);
                uint32_t gap = fw_travel(shown.x, shown.z, player.x, player.z);
                assert(gap > FW_TILE && gap < FW_TILE * 2u);
            }
        }
        assert(sawHold && traveled >= FW_EASE_TRAVEL);
    }
}
static void check_ease(void) {
    assert(fw_ease_smoothstep(0, FW_EASE_TRAVEL) == 0);
    assert(fw_ease_smoothstep(FW_EASE_TRAVEL / 2, FW_EASE_TRAVEL) == 32768u);
    assert(fw_ease_smoothstep(FW_EASE_TRAVEL, FW_EASE_TRAVEL) == 65536u);
    assert(fw_ease_smoothstep(FW_EASE_TRAVEL + 8, FW_EASE_TRAVEL) == 65536u);
    assert(fw_ease_smoothstep(FW_EASE_TRAVEL / 8, FW_EASE_TRAVEL) < 8192u);
    uint32_t rise0 = fw_ease_smoothstep(FW_EASE_TRAVEL / 8, FW_EASE_TRAVEL);
    uint32_t mid0 = fw_ease_smoothstep(FW_EASE_TRAVEL * 3 / 8, FW_EASE_TRAVEL);
    uint32_t mid1 = fw_ease_smoothstep(FW_EASE_TRAVEL / 2, FW_EASE_TRAVEL);
    uint32_t end0 = fw_ease_smoothstep(FW_EASE_TRAVEL * 7 / 8, FW_EASE_TRAVEL);
    assert(rise0 < mid1 - mid0 && 65536u - end0 < mid1 - mid0);
    uint32_t prev = 0;
    for (uint32_t i = 0; i <= 8; ++i) {
        uint32_t s = fw_ease_smoothstep(FW_EASE_TRAVEL / 8 * i, FW_EASE_TRAVEL);
        assert(s >= prev);
        prev = s;
    }
    assert(fw_ease_keep(0, FW_EASE_TRAVEL, 40000u, FW_TILE) == 40000u);
    assert(fw_ease_keep(FW_EASE_TRAVEL, FW_EASE_TRAVEL, 40000u, FW_TILE) == FW_TILE);
    assert(fw_ease_keep(1, FW_EASE_TRAVEL, FW_TILE, FW_TILE) == FW_TILE);
    prev = 40000u;
    for (uint32_t i = 0; i <= 8; ++i) {
        uint32_t k = fw_ease_keep(FW_EASE_TRAVEL / 8 * i, FW_EASE_TRAVEL, 40000u, FW_TILE);
        assert(k >= prev && k <= FW_TILE);
        prev = k;
    }
    assert(fw_travel(0, 0, (int32_t)FW_TILE, 0) == FW_TILE);
    assert(fw_travel(5, -7, 5 + 3 * (int32_t)FW_TILE, -7 + 4 * (int32_t)FW_TILE) == 5u * FW_TILE);
    int32_t gridBehind[2], railBehind[2];
    ease_line(0, 3, gridBehind);
    ease_line(1, 1, railBehind);
    assert(near_eq(gridBehind[0], gridBehind[1]));
    assert(near_eq(railBehind[0], railBehind[1]));
    assert(near_eq(gridBehind[0], railBehind[0]));
    ease_bend();
}
int main(void) {
    FwPokemon party[6] = {{0}};
    assert(fw_select(0, 0) == -1 && fw_select(party, 0) == -1 && fw_select(party, 7) == -1);
    party[0].species=1;party[0].egg=1;party[0].hp=10; assert(fw_select(party,1)==-1);
    party[1].species=2;party[2].species=3;party[2].hp=1;
    assert(fw_select(party,3)==2);party[2].hp=0;assert(fw_select(party,3)==1);
    party[1].hp=1;party[2].hp=1;party[3].species=4;party[3].hp=1;
    assert(fw_cycle_slot(party,4,1,1)==2);
    assert(fw_cycle_slot(party,4,1,-1)==3);
    assert(fw_cycle_slot(party,4,3,1)==1);
    party[2].hp=0;assert(fw_cycle_slot(party,4,1,1)==3);
    party[1].hp=party[3].hp=0;assert(fw_cycle_slot(party,4,1,1)==2);
    party[2].egg=1;assert(fw_cycle_slot(party,4,1,1)==3);party[2].egg=0;
    party[1].egg=party[3].egg=1;assert(fw_cycle_slot(party,4,2,1)==-1);
    party[1].egg=party[3].egg=0;
    assert(fw_cycle_slot(party,4,1,0)==-1 && fw_cycle_slot(party,7,1,1)==-1);
    party[1].hp=26;party[2].hp=0;
    party[1].max_hp=100;party[1].hp=25;assert(fw_tired(&party[1]));party[1].hp=26;assert(!fw_tired(&party[1]));
    FwPokemon a=party[1],b=a;assert(fw_same_identity(&a,&b)); b.personality++;assert(!fw_same_identity(&a,&b));
    b=a;b.shiny=1;assert(!fw_same_identity(&a,&b)); b=a;b.form++;assert(!fw_same_identity(&a,&b));
    FwFollower f;fw_init(&f,1);f.state=FW_FOLLOWING;f.actor=1;assert(fw_interact(&f));assert(f.state==FW_INTERACTING);
    for(unsigned state=FW_ABSENT;state<=FW_EVENT_PAUSED;++state)
        assert(fw_idle_animation_enabled((FwState)state,1,0)==(state==FW_FOLLOWING));
    assert(!fw_idle_animation_enabled(FW_FOLLOWING,0,0));
    assert(!fw_idle_animation_enabled(FW_FOLLOWING,1,1));
    fw_enter(&f,FW_WARP);fw_end_interaction(&f);assert(f.state==FW_SUPPRESSED);
    fw_enter(&f,FW_MENU);fw_enter(&f,FW_MENU);fw_leave(&f,FW_MENU);fw_leave(&f,FW_WARP);assert(fw_suppressed(&f));
    fw_leave(&f,FW_MENU);assert(!fw_suppressed(&f)&&f.state==FW_WAITING);fw_leave(&f,FW_MENU);assert(!fw_suppressed(&f));
    for(int i=0;i<256;i++) fw_enter(&f,FW_SCRIPT);
    for(int i=0;i<300;i++) fw_leave(&f,FW_SCRIPT);
    assert(fw_suppressed(&f));
    FwTrail t;FwSample out;fw_trail_clear(&t);
    for(int i=0;i<=32;i++){FwSample s=sample(i*4096,0,0,1,FW_GRID);int r=fw_trail_push(&t,&s,&out,6);if(i>=22){assert(r==1);assert(out.x==(i-22)*4096+2);}else assert(r==0);}
    /* Reverse along the recorded route; no seek-to-player shortcut.
       The +2 is the truncated metric remainder on this one-unit gap-6 step. */
    for(int i=31;i>=16;i--){FwSample s=sample(i*4096,0,0,1,FW_GRID);assert(fw_trail_push(&t,&s,&out,6)==1);assert(out.x==(42-i)*4096+2);}
    FwSample s=sample(16*4096,0,0,2,FW_GRID);assert(fw_trail_push(&t,&s,&out,6)==-1&&t.count==0);
    assert(fw_trail_push(&t,&s,&out,6)==0);s.x+=FW_TILE*2;assert(fw_trail_push(&t,&s,&out,6)==-1);
    fw_trail_clear(&t);s=sample(0,0,0,1,FW_RAIL);s.rail=1;assert(fw_trail_push(&t,&s,&out,6)==0);
    s.x=4096;s.rail=2;assert(fw_trail_push(&t,&s,&out,6)==-1);
    s.connection=5;assert(fw_trail_push(&t,&s,&out,6)==0);s.x+=4096;s.rail=3;assert(fw_trail_push(&t,&s,&out,6)==0);
    fw_trail_clear(&t);s=sample(0,0,0,1,FW_WORLD);assert(!fw_trail_push(&t,&s,&out,6));
    for(int i=0;i<1000;i++) assert(!fw_trail_push(&t,&s,&out,6));assert(t.count==1);
    for(unsigned i=1;i<FW_TRAIL_CAPACITY;i++){s.x=(int)i;assert(!fw_trail_push(&t,&s,&out,6));}
    assert(t.count==FW_TRAIL_CAPACITY);
    s.x++;assert(fw_trail_push(&t,&s,&out,6)==-1&&t.count==0);
    assert(fw_trail_push(&t,&s,&out,6)==0&&t.count==1);
    fw_trail_clear(&t);
    for(int i=0;i<=48;i++){s=sample(i*4096,i<16?0:(i<32?(i-16)*1024:0),0,1,FW_LEDGE);int r=fw_trail_push(&t,&s,&out,6);if(i>=38){assert(r==1);assert(out.kind==FW_LEDGE);assert(out.y==(i-38)*1024);}}
    /* Cardinal gaps at walking/running speeds and both coordinate signs. */
    for(unsigned gap_value=0;gap_value<=6;gap_value++) for(int axis=0;axis<2;axis++) for(int sign=-1;sign<=1;sign+=2) for(int speed=1;speed<=2;speed++) {
        fw_trail_clear(&t);int gap=axis?16:16+(int)gap_value;
        for(int i=0;i<=400;i+=speed){
            s=sample(axis?0:sign*i*4096,0,axis?sign*i*4096:0,1,FW_WORLD);
            int r=fw_trail_push(&t,&s,&out,gap_value);
            assert(r==(i>=gap));
            if(r){
                int32_t sampleAt=sign*(i-gap-(i-gap)%speed)*4096;
                int32_t coord=axis?out.z:out.x;
                int32_t toward=sampleAt+sign*speed*4096;
                if(sign>0)assert(coord>=sampleAt&&coord<=toward);
                else assert(coord<=sampleAt&&coord>=toward);
                /* A one-unit step only has metric rounding left. A two-unit
                 * step may be pulled forward by the extra unit of lag. */
                if(speed==1)assert(coord-sampleAt<64&&sampleAt-coord<64);
            }
        }
        FwSample old=out;unsigned count=t.count,head=t.head;uint32_t dist=t.distance;
        s.direction=2;for(int idle=0;idle<60;idle++)assert(!fw_trail_push(&t,&s,&out,gap_value));
        assert(t.count==count&&t.head==head&&t.distance==dist&&!memcmp(&old,&out,sizeof(out)));
    }
    /* Run steps of three units and walk steps of one unit rest on the same
       point, one tile behind the player. Walking on from the run does not
       jump by the leftover run step. */
    fw_trail_clear(&t);
    for(int i=0;i<=48;i+=3){s=sample(i*4096,0,0,1,FW_WORLD);assert(fw_trail_push(&t,&s,&out,0)>=0);}
    assert(out.x==48*4096-(int)FW_TILE);
    int runRest=out.x;
    fw_trail_clear(&t);
    for(int i=0;i<=48;i++){s=sample(i*4096,0,0,1,FW_WORLD);assert(fw_trail_push(&t,&s,&out,0)>=0);}
    assert(out.x==runRest);
    fw_trail_clear(&t);
    for(int i=0;i<=48;i+=3){s=sample(i*4096,0,0,1,FW_WORLD);fw_trail_push(&t,&s,&out,0);}
    int prev=out.x;
    for(int i=49;i<=64;i++){
        s=sample(i*4096,0,0,1,FW_WORLD);
        assert(fw_trail_push(&t,&s,&out,0)==1);
        assert(out.x>=prev&&out.x-prev<=4096);
        prev=out.x;
    }
    assert(out.x==64*4096-(int)FW_TILE);
    /* Each facing gets its own offset; a prior direction's gap never leaks. */
    const uint8_t directional[4]={1,2,11,12};
    for(int direction=0;direction<4;direction++){
        fw_trail_clear(&t);
        int sign=(direction==0||direction==2)?-1:1, axis=direction<2, spacing=16+directional[direction];
        for(int i=0;i<=40;i++){
            s=sample(axis?0:sign*i*4096,0,axis?sign*i*4096:0,1,FW_WORLD);
            s.direction=(uint8_t)direction;
            int r=fw_trail_push_directional(&t,&s,&out,directional);
            assert(r==(i>=spacing));
            if(r){
                int32_t expect=sign*(i-spacing)*4096;
                int32_t coord=axis?out.z:out.x;
                if(sign>0)assert(coord>=expect&&coord<expect+4096);
                else assert(coord<=expect&&coord>expect-4096);
            }
        }
    }
    /* toofar.mln: the actor is 28 units west at a ten-unit side gap, while
       dialogue reaches only 27. Its art is two units back toward the player. */
    FwPoint playerPose={13795328,0,42696704};
    FwPoint actorPose={13680640,0,42696704};
    FwPoint artOffset={8192,6144,-8192};
    assert(fw_clamp_dialogue_pose(&actorPose,&playerPose,&artOffset,2,10));
    assert(actorPose.x==playerPose.x-(int32_t)FW_DIALOGUE_REACH(10));
    assert(playerPose.x-(actorPose.x+artOffset.x)<=(int32_t)FW_DIALOGUE_REACH(10));
    assert(!fw_clamp_dialogue_pose(&actorPose,&playerPose,&artOffset,2,10));
    /* An art offset away from the player must also remain inside the cap. */
    FwPoint outward={-8192,0,0};
    assert(fw_clamp_dialogue_pose(&actorPose,&playerPose,&outward,2,10));
    assert(playerPose.x-(actorPose.x+outward.x)==(int32_t)FW_DIALOGUE_REACH(10));
    for(unsigned face=0;face<4;++face){
        FwPoint origin={100*FW_TILE,0,100*FW_TILE},candidate=origin,art={0,0,0};
        int32_t reach=(int32_t)FW_DIALOGUE_REACH(12);
        if(face==0){candidate.z-=reach+4096;art.z=-8192;}
        if(face==1){candidate.z+=reach+4096;art.z=8192;}
        if(face==2){candidate.x-=reach+4096;art.x=-8192;}
        if(face==3){candidate.x+=reach+4096;art.x=8192;}
        assert(fw_clamp_dialogue_pose(&candidate,&origin,&art,face,12));
        int32_t world=face==0?origin.z-candidate.z:face==1?candidate.z-origin.z:
                      face==2?origin.x-candidate.x:candidate.x-origin.x;
        assert(world<=reach&&world+8192==reach);
    }
    FwPoint distant={playerPose.x-33*4096,0,playerPose.z};
    assert(!fw_clamp_dialogue_pose(&distant,&playerPose,0,2,10));
    FwPoint offAxis={playerPose.x-28*4096,0,playerPose.z+8*4096};
    assert(!fw_clamp_dialogue_pose(&offAxis,&playerPose,0,2,10));
    /* 0.6.84 toofar.mln: an eight-unit gap leaves the actor three units
       outside its 25-unit reach after a partial native step. */
    actorPose=(FwPoint){playerPose.x-28*4096,0,playerPose.z};
    assert(fw_clamp_dialogue_pose(&actorPose,&playerPose,&artOffset,2,8));
    assert(actorPose.x==playerPose.x-25*4096);
    /* Repeated L bends and heights: the head sample advances monotonically
       through history. The emitted point stays on that sample's segment, so a
       long step cannot leave extra lag and a six-unit gap is not collapsed. */
    FwSample route[800];fw_trail_clear(&t);int x=0,z=0,last_index=-1;
    for(int i=0;i<800;i++){
        if(i){if((i/40)%2)x+=4096;else z+=4096;}
        route[i]=sample(x,(i%13)*512,z,1,FW_WORLD);route[i].duration=(uint16_t)i;
        int r=fw_trail_push(&t,&route[i],&out,6);assert(r>=0);
        if(r){int at=out.duration;assert(at<i&&at>=last_index);
            assert(out.kind==route[at].kind&&out.direction==route[at].direction&&out.duration==route[at].duration);
            const FwSample *seg=&route[at],*next=&route[at+1];
            int32_t x0=seg->x<next->x?seg->x:next->x,x1=seg->x>next->x?seg->x:next->x;
            int32_t y0=seg->y<next->y?seg->y:next->y,y1=seg->y>next->y?seg->y:next->y;
            int32_t z0=seg->z<next->z?seg->z:next->z,z1=seg->z>next->z?seg->z:next->z;
            assert(out.x>=x0&&out.x<=x1&&out.y>=y0&&out.y<=y1&&out.z>=z0&&out.z<=z1);
            if(last_index>=0)assert(at-last_index<=2);
            last_index=at;
        }
    }
    for(unsigned code=0;code<=65535;code++){
        unsigned expected=10;
        if(code<377)expected=code;else if(code>=4096&&code<4716)expected=code-3719;else if(code>=8192&&code<8203)expected=code-7195;
        assert(fw_stock_row(code)==expected);assert(fw_object_row(code,0)==expected);
        if(code<0x3000||code>=0x3000+4096)assert(fw_object_row(code,4096)==expected);
    }
    assert(fw_object_row(0x3000,1)==1008);assert(fw_object_row(0x3001,1)==10);assert(fw_object_row(0x3000,4097)==10);
    assert(fw_descriptor_offset(2341)==65552);assert(fw_descriptor_offset(5103)==142888);
    /* A connection jump shifts every sample and keeps the follow distance. */
    fw_trail_clear(&t);
    for(int i=0;i<4;i++){s=sample(i*4096,0,0,1,FW_WORLD);assert(fw_trail_push(&t,&s,&out,0)==0);}
    uint32_t carried=t.distance;
    assert(fw_trail_translate(&t,3*FW_TILE,FW_TILE,-2*4096));
    assert(t.count==4&&t.distance==carried&&t.head==0);
    assert(t.samples[0].x==3*FW_TILE&&t.samples[3].x==3*4096+3*FW_TILE);
    assert(t.samples[0].y==FW_TILE&&t.samples[0].z==-2*4096);
    s=sample(3*4096+3*FW_TILE+4096,FW_TILE,-2*4096,1,FW_WORLD);
    assert(fw_trail_push(&t,&s,&out,0)==0);
    assert(!fw_trail_translate(&t,INT32_MAX,0,0)&&t.count==0);
    /* Reverse step: one cardinal tile, trail ahead is the vacated tile. */
    assert(fw_tile(FW_TILE/2)==0&&fw_tile(FW_TILE)==1);
    assert(fw_tile(-1)==-1&&fw_tile(-FW_TILE)==-1&&fw_tile(-FW_TILE-1)==-2);
    const int32_t center=4*(int32_t)FW_TILE+FW_TILE/2;
    for(unsigned face=0;face<4;++face){
        fw_trail_clear(&t);t.count=2;
        int32_t fx=center,fz=center,ox=center,oz=center;
        if(face==0)fz-=FW_TILE;else if(face==1)fz+=FW_TILE;else if(face==2)fx-=FW_TILE;else fx+=FW_TILE;
        t.samples[0]=sample(fx,0,fz,1,FW_WORLD);t.samples[1]=sample(ox,0,oz,1,FW_WORLD);
        FwPoint follower={fx,0,fz},origin={ox,0,oz};
        assert(fw_swap_match(&t,&follower,&origin,face)==1);
        assert(fw_swap_match(&t,&follower,&origin,face^1)==0);
    }
    fw_trail_clear(&t);
    for(int i=0;i<=32;++i){s=sample(i*4096+FW_TILE/2,0,center,1,FW_WORLD);s.direction=3;assert(fw_trail_push(&t,&s,&out,0)>=0);}
    FwPoint origin={32*4096+FW_TILE/2,0,center},follower={out.x,out.y,out.z};
    assert(origin.x-follower.x==(int32_t)FW_TILE);
    assert(fw_swap_match(&t,&follower,&origin,2)==1);
    assert(fw_swap_match(&t,&follower,&origin,3)==0);
    FwPoint diagonal=follower;diagonal.z+=FW_TILE;
    assert(!fw_swap_match(&t,&diagonal,&origin,2));
    FwPoint high=follower;high.y=3*(int32_t)FW_TILE;
    assert(!fw_swap_match(&t,&high,&origin,2));
    /* Head still on the tile behind the interpolated follower. */
    fw_trail_clear(&t);t.count=2;
    t.samples[0]=sample(center-2*(int32_t)FW_TILE,0,center,1,FW_WORLD);
    t.samples[1]=sample(center,0,center,1,FW_WORLD);
    follower=(FwPoint){center-(int32_t)FW_TILE,0,center};origin=(FwPoint){center,0,center};
    assert(fw_swap_match(&t,&follower,&origin,2)==1);
    FwPoint mid;
    assert(fw_swap_mirror(&follower,&origin,&origin,&mid)==0&&mid.x==follower.x&&mid.z==follower.z);
    FwPoint half=origin;half.x=origin.x+(follower.x-origin.x)/2;
    assert(fw_swap_mirror(&follower,&origin,&half,&mid)==0);
    assert(mid.x==follower.x+(origin.x-follower.x)/2&&mid.z==origin.z);
    assert(fw_swap_mirror(&follower,&origin,&follower,&mid)==1&&mid.x==origin.x&&mid.y==origin.y&&mid.z==origin.z);
    FwPoint past=follower;past.x-=4096;
    assert(fw_swap_mirror(&follower,&origin,&past,&mid)==1&&mid.x==origin.x);
    FwPoint side=half;side.z+=4097;
    assert(fw_swap_mirror(&follower,&origin,&side,&mid)==-1);
    FwPoint back=origin;back.x+=4096;
    assert(fw_swap_mirror(&follower,&origin,&back,&mid)==-1);
    FwPoint ledge=half;ledge.y=3*(int32_t)FW_TILE;
    assert(fw_swap_mirror(&follower,&origin,&ledge,&mid)==-1);
    assert(fw_swap_mirror(&follower,&origin,&origin,&mid)==0&&mid.x==follower.x&&mid.y==follower.y&&mid.z==follower.z);
    FwSample start=sample(origin.x,origin.y,origin.z,1,FW_WORLD);start.direction=2;
    FwSample end=sample(follower.x,follower.y,follower.z,1,FW_WORLD);end.direction=2;
    const uint8_t nogap[4]={0,0,0,0};
    assert(fw_trail_reseed(&t,&start,&end,nogap,&out)==1&&out.x==start.x);
    assert(fw_trail_push(&t,&end,&out,0)==0&&out.x==start.x);
    end.x-=4096;
    int32_t rested=out.x;
    assert(fw_trail_push(&t,&end,&out,0)==1);
    assert(out.x<=rested&&rested-out.x<=4096);
    /* Post-swap rest matches a fresh load for walk and run steps. The occupied
     * tile is that pose: an NPC step onto it hits, and the player tile does not. */
    const int32_t home=8*(int32_t)FW_TILE+(int32_t)FW_TILE/2;
    for(unsigned gap=0;gap<=6;gap+=6) for(int speed=1;speed<=2;++speed) for(int sign=-1;sign<=1;sign+=2){
        uint8_t gaps[4]={(uint8_t)gap,(uint8_t)gap,(uint8_t)gap,(uint8_t)gap};
        unsigned face=sign>0?3u:2u;
        int32_t goal=home;
        FwTrail fresh,swapped;FwSample pose,after;
        fw_trail_clear(&fresh);
        int32_t x=sign>0?0:16*(int32_t)FW_TILE;
        for(;;){
            s=sample(x,0,0,1,FW_WORLD);s.direction=(uint8_t)face;
            assert(fw_trail_push_directional(&fresh,&s,&pose,gaps)>=0);
            if(x==goal)break;
            int32_t next=x+sign*speed*4096;
            if((sign>0&&next>goal)||(sign<0&&next<goal))next=goal;
            x=next;
        }
        assert(fw_tile(pose.x)!=fw_tile(goal));
        int32_t gap_behind=sign>0?goal-pose.x:pose.x-goal;
        if(gap==0)assert(gap_behind==(int32_t)FW_TILE);
        else assert(gap_behind>(int32_t)FW_TILE);
        FwSample far=sample(sign>0?goal-(int32_t)FW_TILE:goal+(int32_t)FW_TILE,0,0,1,FW_WORLD);
        FwSample at=sample(goal,0,0,1,FW_WORLD);
        far.direction=at.direction=(uint8_t)face;
        assert(fw_trail_reseed(&swapped,&far,&at,gaps,&after)==1);
        assert(after.x-pose.x<64&&pose.x-after.x<64);
        assert(fw_tile(after.x)==fw_tile(pose.x)&&fw_tile(after.x)!=fw_tile(goal));
        int fx=fw_tile(pose.x),px=fw_tile(goal);
        assert(npc_tile(fx,0,fx,-1,fx,0)&&npc_tile(fx,0,fx,0,fx,0));
        assert(!npc_tile(fx,0,px,0,px,0)&&!npc_tile(fx,0,fx,2,fx,2));
        assert(npc_tile(fw_tile(after.x),0,fw_tile(after.x),-1,fw_tile(after.x),0));
        assert(!npc_tile(fw_tile(after.x),0,px,0,px,0));
        for(int n=1;n<=24;++n){
            int32_t step_x=goal+sign*n*speed*4096;
            s=sample(step_x,0,0,1,FW_WORLD);s.direction=(uint8_t)face;
            assert(fw_trail_push_directional(&fresh,&s,&pose,gaps)==1);
            assert(fw_trail_push_directional(&swapped,&s,&after,gaps)==1);
            assert(after.x-pose.x<64&&pose.x-after.x<64);
            assert(fw_tile(after.x)==fw_tile(pose.x)&&fw_tile(after.x)!=fw_tile(step_x));
            int32_t behind=sign>0?step_x-after.x:after.x-step_x;
            if(gap==0)assert(behind>(int32_t)FW_TILE-64&&behind<(int32_t)FW_TILE+64);
            else assert(behind>(int32_t)FW_TILE);
            int tile=fw_tile(after.x),player_tile=fw_tile(step_x);
            assert(npc_tile(tile,0,tile,0,tile,0));
            assert(!npc_tile(tile,0,player_tile,0,player_tile,0));
            assert(!npc_tile(tile,0,tile+2,0,tile+2,0));
        }
    }
    /* Rail reverse step: the segment is wider than a tile and not cardinal.
       The finished facing is the player's, including a rail direction key. */
    assert(fw_swap_face(3,0)==3&&fw_swap_face(3,1)==2&&fw_swap_face(9,1)==0);
    fw_trail_clear(&t);t.count=2;
    follower=(FwPoint){center+2*(int32_t)FW_TILE+20000,0,center+(int32_t)FW_TILE};
    origin=(FwPoint){center,0,center};
    t.samples[0]=sample(follower.x,0,follower.z,1,FW_WORLD);
    t.samples[1]=sample(origin.x,0,origin.z,1,FW_WORLD);
    assert(fw_rail_swap_match(&t,&follower,&origin)==1);
    assert(fw_swap_match(&t,&follower,&origin,3)==0);
    assert(fw_rail_swap_mirror(&follower,&origin,&follower,&mid,0)==1);
    assert(mid.x==origin.x&&mid.y==origin.y&&mid.z==origin.z);
    FwPoint away=origin;away.x-=(int32_t)FW_TILE;
    assert(fw_rail_swap_mirror(&follower,&origin,&away,&mid,0)==-1);
    assert(fw_rail_swap_mirror(&follower,&origin,&away,&mid,1)==-1);
    /* The follow gap is longer than one knot. Stopping on that knot finishes
     * the swap; the facing is the player's rail face, not the chord compass. */
    FwPoint gapFollower=origin;gapFollower.x+=90110;
    FwPoint oneKnot=origin;oneKnot.x+=(int32_t)FW_TILE;
    assert(fw_rail_swap_mirror(&gapFollower,&origin,&oneKnot,&mid,0)==0);
    assert(fw_rail_swap_mirror(&gapFollower,&origin,&oneKnot,&mid,1)==1);
    FwPoint curveOrigin={0,0,0};
    FwPoint curveFollower={3*(int32_t)FW_TILE,0,4*(int32_t)FW_TILE};
    FwPoint curvePlayer={3*(int32_t)FW_TILE/2,0,2*(int32_t)FW_TILE};
    assert(fw_rail_swap_mirror(&curveFollower,&curveOrigin,&curvePlayer,&mid,0)==0);
    assert(fw_rail_swap_mirror(&curveFollower,&curveOrigin,&curvePlayer,&mid,1)==1);
    assert(fw_rail_swap_turn(2,3*(int)FW_TILE,4*(int)FW_TILE)==2);
    assert(fw_rail_swap_turn(2,(int)FW_TILE,0)==2);
    assert(fw_rail_swap_turn(0,0,-(int)FW_TILE)==0);
    assert(fw_rail_swap_turn(3,-5,1)==3);
    assert(fw_rail_swap_turn(9,1,1)==0);
    /* Curved talk: front and side deltas, including a chord whose world
     * compass is south while the line's +front is the 3-4-5 diagonal. */
    assert(fw_rail_face_toward(5,0)==2&&fw_rail_face_toward(-4,0)==3);
    assert(fw_rail_face_toward(0,3)==0&&fw_rail_face_toward(0,-1)==1);
    assert(fw_rail_face_toward(3,1)==2&&fw_rail_face_toward(-5,2)==3);
    assert(fw_rail_face_toward(1,4)==0&&fw_rail_face_toward(2,-5)==1);
    assert(fw_rail_face_toward(4,4)==2&&fw_rail_face_toward(-4,4)==3);
    assert(fw_rail_face_at(3*(int)FW_TILE,4*(int)FW_TILE,3*(int)FW_TILE,4*(int)FW_TILE,-4*(int)FW_TILE,3*(int)FW_TILE)==2);
    assert(fw_rail_face_at(-4*(int)FW_TILE,3*(int)FW_TILE,3*(int)FW_TILE,4*(int)FW_TILE,-4*(int)FW_TILE,3*(int)FW_TILE)==0);
    assert(fw_rail_face_at(-3*(int)FW_TILE,-4*(int)FW_TILE,3*(int)FW_TILE,4*(int)FW_TILE,-4*(int)FW_TILE,3*(int)FW_TILE)==3);
    assert(fw_rail_face_at(4*(int)FW_TILE,-3*(int)FW_TILE,3*(int)FW_TILE,4*(int)FW_TILE,-4*(int)FW_TILE,3*(int)FW_TILE)==1);
    assert(fw_rail_face_at(1,0,0,0,0,0)==4);
    /* Wide rail knot is talkable. Two grid tiles, and the old 22-unit north
       rejection, still fail. One grid tile still succeeds. */
    FwPoint talkPlayer={10*(int32_t)FW_TILE,0,10*(int32_t)FW_TILE};
    FwPoint talkFar=talkPlayer;talkFar.x-=2*(int32_t)FW_TILE;
    assert(!fw_grid_talk_span(&talkPlayer,&talkFar,2,12));
    FwPoint talkNear=talkPlayer;talkNear.x-=(int32_t)FW_TILE;
    assert(fw_grid_talk_span(&talkPlayer,&talkNear,2,0)==1);
    FwPoint talkNorth=talkPlayer;talkNorth.z-=22*4096;
    assert(!fw_grid_talk_span(&talkPlayer,&talkNorth,0,0));
    talkNorth.z=talkPlayer.z-(int32_t)FW_TILE;
    assert(fw_grid_talk_span(&talkPlayer,&talkNorth,0,0)==1);
    FwPoint wide=talkPlayer;wide.x+=2*(int32_t)FW_TILE+20000;
    assert(fw_rail_segment_reach(&talkPlayer,&wide,&wide,0)==1);
    FwPoint railDiagonal=talkPlayer;railDiagonal.x+=3*(int32_t)FW_TILE/2;railDiagonal.z+=(int32_t)FW_TILE;
    assert(fw_rail_segment_reach(&talkPlayer,&railDiagonal,&railDiagonal,6)==1);
    FwPoint railPast=talkPlayer;railPast.x+=2*(wide.x-talkPlayer.x);railPast.z+=2*(wide.z-talkPlayer.z);
    assert(!fw_rail_segment_reach(&talkPlayer,&railPast,&wide,12));
    FwPoint beside=wide;beside.z+=(int32_t)FW_TILE;
    assert(!fw_rail_segment_reach(&talkPlayer,&beside,&wide,12));
    /* Reseed along the chord, not the compass of a rail facing key. */
    FwSample fromS=sample(0,0,0,1,FW_WORLD);
    FwSample atS=sample(3*(int32_t)FW_TILE,0,4*(int32_t)FW_TILE,1,FW_WORLD);
    atS.direction=1;
    const uint8_t gap0[4]={0,0,0,0},gap6[4]={6,6,6,6};
    assert(fw_trail_reseed_along(&t,&fromS,&atS,gap0,&out)==1);
    assert(out.direction==1);
    assert(out.x>fromS.x&&out.x<atS.x&&out.z>fromS.z&&out.z<atS.z);
    int64_t back0x=(int64_t)atS.x-out.x,back0z=(int64_t)atS.z-out.z;
    int64_t tile2=(int64_t)FW_TILE*(int64_t)FW_TILE;
    int64_t dist0=back0x*back0x+back0z*back0z;
    assert(dist0>tile2-tile2/8&&dist0<tile2+tile2/4);
    FwSample along;
    assert(fw_trail_reseed_along(&t,&fromS,&atS,gap6,&along)==1);
    assert(along.direction==atS.direction);
    int64_t back6x=(int64_t)atS.x-along.x,back6z=(int64_t)atS.z-along.z;
    assert(back6x*back6x+back6z*back6z>dist0);
    FwSample onward=atS;onward.x+=3*(int32_t)FW_TILE/5;onward.z+=4*(int32_t)FW_TILE/5;
    assert(fw_trail_push_directional(&t,&onward,&out,gap6)==1);
    int64_t keepx=(int64_t)onward.x-out.x,keepz=(int64_t)onward.z-out.z;
    assert(keepx*keepx+keepz*keepz>tile2);
    assert(fw_trail_reseed_along(&t,&atS,&atS,gap0,&out)==0);
    /* Unown's follower sheet is always '?'. Arceus is always Normal. */
    assert(fw_follower_form(201,0)==27&&fw_follower_form(201,12)==27&&fw_follower_form(201,27)==27);
    assert(fw_follower_form(493,9)==0&&fw_follower_form(493,0)==0&&fw_follower_form(493,16)==0);
    assert(fw_follower_form(25,1)==1&&fw_follower_form(200,0)==0&&fw_follower_form(494,1)==1);
    check_ease();
    puts("Follower host logic: selection, identity, suppression, trail, mapping and wide offsets passed.");
}
