#include "following.h"
#include <limits.h>
int fw_select(const FwPokemon *party, unsigned count) {
    int fallback = -1;
    if (!party || count > FW_PARTY_CAPACITY) return -1;
    for (unsigned i = 0; i < count; ++i) {
        if (party[i].egg || !party[i].species || party[i].species > FW_MAX_SPECIES) continue;
        if (fallback < 0) fallback = (int)i;
        if (party[i].hp) return (int)i;
    }
    return fallback;
}
int fw_cycle_slot(const FwPokemon *party, unsigned count, int current, int direction) {
    if (!party || !count || count > FW_PARTY_CAPACITY || (direction != -1 && direction != 1)) return -1;
    int baseline = fw_select(party, count);
    if (baseline < 0) return -1;
    if (current < 0 || (unsigned)current >= count) current = baseline;
    int healthy = 0;
    for (unsigned i = 0; i < count; ++i)
        if (!party[i].egg && party[i].species && party[i].species <= FW_MAX_SPECIES && party[i].hp) healthy = 1;
    int slot = current;
    for (unsigned step = 1; step <= count; ++step) {
        slot += direction;
        if (slot < 0) slot = (int)count - 1;
        else if ((unsigned)slot >= count) slot = 0;
        const FwPokemon *mon = &party[slot];
        if (!mon->egg && mon->species && mon->species <= FW_MAX_SPECIES && (!healthy || mon->hp))
            return slot == current ? -1 : slot;
    }
    return -1;
}
int fw_same_identity(const FwPokemon *a, const FwPokemon *b) {
    return a->personality == b->personality && a->trainer == b->trainer &&
        a->species == b->species && a->form == b->form &&
        a->gender == b->gender && a->shiny == b->shiny;
}
int fw_tired(const FwPokemon *mon) { return mon->max_hp && (uint32_t)mon->hp * 4 <= mon->max_hp; }
int fw_idle_animation_enabled(FwState state, unsigned visible, unsigned effect_busy) {
    return state == FW_FOLLOWING && visible && !effect_busy;
}
void fw_trail_clear(FwTrail *t) { t->head = t->count = 0; t->distance = 0; }
void fw_init(FwFollower *f, uint32_t generation) {
    f->state = FW_ABSENT; f->actor = 0; f->generation = generation;
    f->slot = -1; f->has_selection = 0; f->side_gap = 0;
    for (unsigned i = 0; i < FW_REASON_COUNT; ++i) f->suppression[i] = 0;
    fw_trail_clear(&f->trail);
}
int fw_suppressed(const FwFollower *f) {
    for (unsigned i = 0; i < FW_REASON_COUNT; ++i) if (f->suppression[i]) return 1;
    return 0;
}
void fw_enter(FwFollower *f, FwReason reason) {
    if ((unsigned)reason >= FW_REASON_COUNT) return;
    /* Saturation latches the guard rather than wrapping to an unsafe release. */
    if (f->suppression[reason] < 255) ++f->suppression[reason];
    f->state = FW_SUPPRESSED; fw_trail_clear(&f->trail);
}
void fw_leave(FwFollower *f, FwReason reason) {
    if ((unsigned)reason >= FW_REASON_COUNT) return;
    if (f->suppression[reason] && f->suppression[reason] != 255) --f->suppression[reason];
    if (!fw_suppressed(f) && f->state == FW_SUPPRESSED) f->state = FW_WAITING;
}
int fw_interact(FwFollower *f) {
    if (f->state != FW_FOLLOWING || fw_suppressed(f) || !f->actor) return 0;
    f->state = FW_INTERACTING; return 1;
}
void fw_end_interaction(FwFollower *f) {
    if (f->state == FW_INTERACTING) f->state = fw_suppressed(f) ? FW_SUPPRESSED : FW_FOLLOWING;
}
static uint32_t abs_delta(int32_t a, int32_t b) {
    int64_t d = (int64_t)a - b;
    if (d > FW_TILE * 2 || d < -FW_TILE * 2) return FW_TILE * 3;
    return (uint32_t)(d < 0 ? -d : d);
}
static uint32_t isqrt(uint64_t n) {
    uint64_t bit = (uint64_t)1 << 62, result = 0;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= result + bit) { n -= result + bit; result = (result >> 1) + bit; }
        else result >>= 1;
        bit >>= 2;
    }
    return (uint32_t)result;
}
static uint32_t distance(const FwSample *a, const FwSample *b) {
    uint32_t x = abs_delta(a->x, b->x), z = abs_delta(a->z, b->z);
    return isqrt((uint64_t)x * x + (uint64_t)z * z);
}
/* One constant metric per appearance: horizontal span 16+gap, vertical 16.
 * Corners follow recorded poses instead of changing a distance target abruptly.
 * Integer weights keep cardinal spacing exact with no extra history buffer. */
static uint32_t trail_distance(const FwSample *a, const FwSample *b, const uint8_t gaps[4]) {
    unsigned gx = b->x < a->x ? gaps[2] : gaps[3], gz = b->z < a->z ? gaps[0] : gaps[1];
    uint32_t raw_x = abs_delta(a->x, b->x),raw_z = abs_delta(a->z, b->z);
    uint32_t x = (raw_x*16u+15u+gx)/(16u+gx), z = (raw_z*16u+15u+gz)/(16u+gz);
    return isqrt((uint64_t)x * x + (uint64_t)z * z);
}
int fw_clamp_dialogue_pose(FwPoint *pose,const FwPoint *player,const FwPoint *art_offset,
                           unsigned face,unsigned gap){
    if(!pose||!player||face>3||gap>FW_MAX_DIRECTIONAL_GAP)return 0;
    FwPoint offset=art_offset?*art_offset:(FwPoint){0,0,0};
    int64_t along=face==0?(int64_t)player->z-pose->z:
                  face==1?(int64_t)pose->z-player->z:
                  face==2?(int64_t)player->x-pose->x:(int64_t)pose->x-player->x;
    int64_t lateral=face<2?(int64_t)pose->x-player->x:(int64_t)pose->z-player->z;
    int64_t art_along=along+(face==0?-(int64_t)offset.z:face==1?offset.z:face==2?-(int64_t)offset.x:offset.x);
    int64_t art_lateral=lateral+(face<2?offset.x:offset.z);
    int64_t height=(int64_t)pose->y-player->y;
    int64_t limit=FW_DIALOGUE_REACH(gap);
    /* A partial native step and draw-anchor displacement can put the world
     * actor just beyond reach. Correct at most half a tile, within the same
     * two-tile corridor. Do not pull a distant or off-axis actor across
     * a turn, wall, stair or discontinuity to make it talkable. */
    int64_t farthest=along>art_along?along:art_along;
    if(farthest<=limit||along>2*FW_TILE||along>limit+8*4096||farthest>limit+16*4096||
       lateral<-(int64_t)FW_TILE/3||lateral>(int64_t)FW_TILE/3||
       art_lateral<-(int64_t)FW_TILE/3||art_lateral>(int64_t)FW_TILE/3||
       height<-8*4096||height>8*4096)return 0;
    int64_t shift=farthest-limit;
    int64_t value=face==0?(int64_t)pose->z+shift:
                  face==1?(int64_t)pose->z-shift:
                  face==2?(int64_t)pose->x+shift:(int64_t)pose->x-shift;
    if(value<INT32_MIN||value>INT32_MAX)return 0;
    if(face<2)pose->z=(int32_t)value;
    else pose->x=(int32_t)value;
    return 1;
}
static unsigned index_at(const FwTrail *t, unsigned at) { return (t->head + at) % FW_TRAIL_CAPACITY; }
/* (delta * over) / span without a 64-bit libcall. The field module only
 * provides __aeabi_lmul; a compiler divide would be an extra import. */
static int32_t scale_step(int32_t delta, uint32_t over, uint32_t span) {
    if (!span || !over || !delta) return 0;
    int neg = delta < 0;
    uint64_t n = (uint64_t)(neg ? (uint64_t)(-(int64_t)delta) : (uint64_t)delta) * over;
    uint64_t q = 0, r = 0;
    for (unsigned bit = 0; bit < 64; ++bit) {
        r = (r << 1) | (n >> 63); n <<= 1; q <<= 1;
        if (r >= span) { r -= span; q |= 1; }
    }
    if (q > 2147483647u) q = 2147483647u;
    return neg ? -(int32_t)q : (int32_t)q;
}
int fw_trail_translate(FwTrail *t, int32_t dx, int32_t dy, int32_t dz) {
    if (!t) return 0;
    if (!t->count || (!dx && !dy && !dz)) return 1;
    for (unsigned i = 0; i < t->count; ++i) {
        FwSample *s = &t->samples[index_at(t, i)];
        int64_t x = (int64_t)s->x + dx, y = (int64_t)s->y + dy, z = (int64_t)s->z + dz;
        if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX || z < INT32_MIN || z > INT32_MAX) {
            fw_trail_clear(t); return 0;
        }
    }
    for (unsigned i = 0; i < t->count; ++i) {
        FwSample *s = &t->samples[index_at(t, i)];
        s->x += dx; s->y += dy; s->z += dz;
    }
    return 1;
}
int fw_trail_push_directional(FwTrail *t, const FwSample *s, FwSample *out, const uint8_t gaps[4]) {
    if (!gaps || gaps[0]>FW_MAX_DIRECTIONAL_GAP||gaps[1]>FW_MAX_DIRECTIONAL_GAP||gaps[2]>FW_MAX_DIRECTIONAL_GAP||gaps[3]>FW_MAX_DIRECTIONAL_GAP||
        !s || !out || s->kind > FW_WORLD || s->direction > 3 || !s->generation) { fw_trail_clear(t); return -1; }
    if (!t->count) { t->samples[0] = *s; t->head = 0; t->count = 1; return 0; }
    const FwSample *last = &t->samples[index_at(t, t->count - 1)];
    uint32_t delta = distance(last, s);
    int disconnected = last->generation != s->generation || last->space != s->space ||
        delta > FW_TILE || abs_delta(last->y, s->y) > FW_TILE * 2;
    if (last->kind == FW_RAIL && s->kind == FW_RAIL && last->rail != s->rail &&
        (last->connection == 0xffff || last->connection != s->connection)) disconnected = 1;
    if (disconnected || t->count == FW_TRAIL_CAPACITY) { fw_trail_clear(t); return -1; }
    /* Stationary facing changes must not consume the bounded history. */
    if (!delta && last->y == s->y) return 0;
    t->samples[index_at(t, t->count++)] = *s;
    t->distance += trail_distance(last, s, gaps);
    if (t->distance < FW_TILE) return 0;
    while (t->count > 1) {
        const FwSample *a = &t->samples[t->head];
        const FwSample *b = &t->samples[index_at(t, 1)];
        uint32_t step = trail_distance(a, b, gaps);
        if (t->distance - step < FW_TILE) break;
        t->distance -= step; t->head = (t->head + 1) % FW_TRAIL_CAPACITY; --t->count;
    }
    /* Sitting on the older sample leaves up to one whole step of lag. A run
     * step is long enough to rest outside the talk box, and the next walk
     * step then drops that sample and the follower jumps forward. The point
     * `over` along this segment is exactly one tile of follow metric. */
    *out = t->samples[t->head];
    if (t->count > 1 && t->distance > FW_TILE) {
        const FwSample *next = &t->samples[index_at(t, 1)];
        uint32_t span = trail_distance(&t->samples[t->head], next, gaps);
        uint32_t over = t->distance - FW_TILE;
        if (span && over) {
            if (over > span) over = span;
            out->x += scale_step(next->x - out->x, over, span);
            out->y += scale_step(next->y - out->y, over, span);
            out->z += scale_step(next->z - out->z, over, span);
        }
    }
    return 1;
}
int fw_trail_push(FwTrail *t, const FwSample *s, FwSample *out, unsigned gap) {
    if(gap>FW_MAX_SIDE_GAP){fw_trail_clear(t);return -1;}
    const uint8_t gaps[4]={0,0,(uint8_t)gap,(uint8_t)gap};
    return fw_trail_push_directional(t,s,out,gaps);
}
int32_t fw_tile(int32_t v) {
    int32_t q = v / (int32_t)FW_TILE;
    if (v % (int32_t)FW_TILE < 0) q -= 1;
    return q;
}
/* (delta * num) / den. Signs are applied after the unsigned divider so a
 * reverse step does not import __aeabi_ldivmod. */
static int32_t mul_div(int32_t delta, int32_t num, int32_t den) {
    if (!den || !num || !delta) return 0;
    int neg = (delta < 0) ^ (num < 0) ^ (den < 0);
    uint64_t ad = delta < 0 ? (uint64_t)-(int64_t)delta : (uint64_t)delta;
    uint64_t an = num < 0 ? (uint64_t)-(int64_t)num : (uint64_t)num;
    uint32_t span = den < 0 ? (uint32_t)-(int64_t)den : (uint32_t)den;
    uint64_t n = ad * an, q = 0, r = 0;
    for (unsigned bit = 0; bit < 64; ++bit) {
        r = (r << 1) | (n >> 63); n <<= 1; q <<= 1;
        if (r >= span) { r -= span; q |= 1; }
    }
    if (q > 2147483647u) q = 2147483647u;
    return neg ? -(int32_t)q : (int32_t)q;
}
static const FwSample *trail_sample(const FwTrail *t, unsigned at) {
    return &t->samples[(t->head + at) % FW_TRAIL_CAPACITY];
}
/* 1 when the first tile ahead of the follower, walking the recorded route
 * toward the player, is `origin`. A head sample still on the previous tile
 * counts when the trail then ends on `origin`. */
static int ahead_is_origin(const FwTrail *t, const FwPoint *follower, const FwPoint *origin) {
    if (!t || t->count < 2) return 0;
    int32_t fx = fw_tile(follower->x), fz = fw_tile(follower->z);
    int32_t ox = fw_tile(origin->x), oz = fw_tile(origin->z);
    int seen = 0;
    for (unsigned i = 0; i < t->count; ++i) {
        const FwSample *s = trail_sample(t, i);
        int32_t tx = fw_tile(s->x), tz = fw_tile(s->z);
        if (tx == fx && tz == fz) { seen = 1; continue; }
        if (seen) return tx == ox && tz == oz;
    }
    if (seen) return 0;
    const FwSample *head = trail_sample(t, 0), *last = trail_sample(t, t->count - 1);
    if (fw_tile(last->x) != ox || fw_tile(last->z) != oz) return 0;
    int32_t hx = fw_tile(head->x), hz = fw_tile(head->z);
    for (unsigned i = 1; i < t->count; ++i) {
        const FwSample *s = trail_sample(t, i);
        int32_t tx = fw_tile(s->x), tz = fw_tile(s->z);
        if (tx != hx || tz != hz) return tx == ox && tz == oz;
    }
    return 0;
}
int fw_swap_match(const FwTrail *trail, const FwPoint *follower, const FwPoint *origin, unsigned face) {
    if (!trail || !follower || !origin || face > 3) return 0;
    int64_t dy = (int64_t)follower->y - origin->y;
    if (dy > 2 * (int64_t)FW_TILE || dy < -2 * (int64_t)FW_TILE) return 0;
    int32_t tx = fw_tile(follower->x) - fw_tile(origin->x);
    int32_t tz = fw_tile(follower->z) - fw_tile(origin->z);
    int cardinal = (face == 0 && tx == 0 && tz == -1) || (face == 1 && tx == 0 && tz == 1) ||
        (face == 2 && tx == -1 && tz == 0) || (face == 3 && tx == 1 && tz == 0);
    return cardinal && ahead_is_origin(trail, follower, origin);
}
static int32_t div_s64(int64_t n, uint32_t d) {
    if (!d || !n || n == INT64_MIN) return 0;
    int neg = n < 0;
    uint64_t u = neg ? (uint64_t)(-n) : (uint64_t)n, q = 0, r = 0;
    for (unsigned bit = 0; bit < 64; ++bit) {
        r = (r << 1) | (u >> 63); u <<= 1; q <<= 1;
        if (r >= d) { r -= d; q |= 1; }
    }
    if (q > 2147483647u) q = 2147483647u;
    return neg ? -(int32_t)q : (int32_t)q;
}
static uint32_t segment_span(int64_t dx, int64_t dz) {
    int64_t ax = dx < 0 ? -dx : dx, az = dz < 0 ? -dz : dz;
    return isqrt((uint64_t)ax * (uint64_t)ax + (uint64_t)az * (uint64_t)az);
}
static int near_sample(const FwSample *s, const FwPoint *p, int32_t tol) {
    int32_t dx = s->x - p->x, dz = s->z - p->z;
    if (dx < 0) dx = -dx;
    if (dz < 0) dz = -dz;
    return dx <= tol && dz <= tol;
}
unsigned fw_swap_face(unsigned player_face, int moving) {
    if (player_face > 3) return 0;
    return moving ? player_face ^ 1u : player_face;
}
int fw_rail_swap_match(const FwTrail *trail, const FwPoint *follower, const FwPoint *origin) {
    if (!trail || !follower || !origin || trail->count < 2) return 0;
    int64_t dy = (int64_t)follower->y - origin->y;
    if (dy > 2 * (int64_t)FW_TILE || dy < -2 * (int64_t)FW_TILE) return 0;
    uint32_t span = segment_span((int64_t)follower->x - origin->x, (int64_t)follower->z - origin->z);
    if (span < 4096 || span > 8u * (uint32_t)FW_TILE) return 0;
    int seen = 0;
    for (unsigned i = 0; i < trail->count; ++i) {
        const FwSample *s = trail_sample(trail, i);
        if (!seen) {
            if (near_sample(s, follower, (int32_t)FW_TILE)) seen = 1;
            continue;
        }
        if (near_sample(s, origin, 8192)) return 1;
    }
    return 0;
}
int fw_rail_swap_mirror(const FwPoint *follower, const FwPoint *origin, const FwPoint *player, FwPoint *out) {
    if (!follower || !origin || !player || !out) return -1;
    *out = *follower;
    int64_t sx = (int64_t)follower->x - origin->x, sz = (int64_t)follower->z - origin->z;
    int64_t sy = (int64_t)follower->y - origin->y;
    if (sy > 2 * (int64_t)FW_TILE || sy < -2 * (int64_t)FW_TILE) return -1;
    uint32_t span = segment_span(sx, sz);
    if (span < 4096 || span > 8u * (uint32_t)FW_TILE) return -1;
    int64_t mx = (int64_t)player->x - origin->x, mz = (int64_t)player->z - origin->z;
    int64_t my = (int64_t)player->y - origin->y;
    if (my > 2 * (int64_t)FW_TILE || my < -2 * (int64_t)FW_TILE) return -1;
    int32_t proj = div_s64(mx * sx + mz * sz, span);
    /* Any step away from the follower is not this segment. A one-unit
     * forward step is a real walk, not slack, and must not start a swap. */
    if (proj < 0) return -1;
    int64_t lat2 = mx * mx + mz * mz - (int64_t)proj * proj;
    int32_t tol = (int32_t)FW_TILE / 4;
    if (lat2 > (int64_t)tol * tol) return -1;
    int done = proj >= (int32_t)span;
    if (done) proj = (int32_t)span;
    out->x = follower->x + div_s64(((int64_t)origin->x - follower->x) * proj, span);
    out->y = follower->y + div_s64(((int64_t)origin->y - follower->y) * proj, span);
    out->z = follower->z + div_s64(((int64_t)origin->z - follower->z) * proj, span);
    return done;
}
/* Display form for the follower sprite, palette, and size row. Identity,
 * dialogue, gifts, and cries keep the party form. Add a row to pin another
 * species; every other species passes its form through. */
static const struct { unsigned species, form; } fw_forced_forms[] = {
    {201u, 27u}, /* Unown '?': Gen 5 forms 0-25 are A-Z, 26 is '!', 27 is '?'. */
    {493u, 0u},  /* Arceus Normal, ignoring the held plate. */
};
unsigned fw_follower_form(unsigned species, unsigned form) {
    for (unsigned i = 0; i < sizeof fw_forced_forms / sizeof fw_forced_forms[0]; ++i)
        if (fw_forced_forms[i].species == species) return fw_forced_forms[i].form;
    return form;
}
int fw_grid_talk_span(const FwPoint *player, const FwPoint *follower, unsigned face, unsigned gap) {
    if (!player || !follower || face > 3 || gap > FW_MAX_DIRECTIONAL_GAP) return 0;
    int x = follower->x - player->x, z = follower->z - player->z, y = follower->y - player->y;
    int ax = x < 0 ? -x : x, az = z < 0 ? -z : z, ay = y < 0 ? -y : y;
    if (ax > (int)(face >= 2 ? FW_DIALOGUE_REACH(gap) : FW_DIALOGUE_REACH(0)) ||
        az > (int)(face < 2 ? FW_DIALOGUE_REACH(gap) : FW_DIALOGUE_REACH(0)) ||
        ay > 8 * 4096) return 0;
    int vx = face == 2 ? -256 : face == 3 ? 256 : 0, vz = face == 0 ? -256 : face == 1 ? 256 : 0;
    int length = vx * vx + vz * vz, px = x / 256, pz = z / 256;
    int along = px * vx + pz * vz, lateral = px * vz - pz * vx;
    if (lateral < 0) lateral = -lateral;
    if (!length || along < length / 2 || along > length + length / 8 + (int)gap * length / 16) return 0;
    return lateral <= length / 2;
}
int fw_rail_segment_reach(const FwPoint *player, const FwPoint *follower, const FwPoint *forward, unsigned gap) {
    if (!player || !follower || !forward || gap > FW_MAX_DIRECTIONAL_GAP) return 0;
    int64_t dy = (int64_t)follower->y - player->y;
    if (dy > 8 * 4096 || dy < -8 * 4096) return 0;
    int64_t sx = (int64_t)forward->x - player->x, sz = (int64_t)forward->z - player->z;
    uint32_t span = segment_span(sx, sz);
    if (span < 4096 || span > 8u * (uint32_t)FW_TILE) return 0;
    int64_t mx = (int64_t)follower->x - player->x, mz = (int64_t)follower->z - player->z;
    int32_t proj = div_s64(mx * sx + mz * sz, span);
    int32_t slack = (int32_t)span / 8 + (int32_t)gap * (int32_t)span / 16;
    if (proj < (int32_t)span / 2 || proj > (int32_t)span + slack) return 0;
    int64_t lat2 = mx * mx + mz * mz - (int64_t)proj * proj;
    int32_t tol = (int32_t)FW_TILE / 2;
    return lat2 <= (int64_t)tol * tol;
}
int fw_swap_mirror(const FwPoint *follower, const FwPoint *origin, const FwPoint *player, FwPoint *out) {
    if (!follower || !origin || !player || !out) return -1;
    *out = *follower;
    int64_t dx = (int64_t)follower->x - origin->x, dz = (int64_t)follower->z - origin->z;
    int64_t ax = dx < 0 ? -dx : dx, az = dz < 0 ? -dz : dz;
    int64_t span = ax >= az ? dx : dz;
    if (!span || span > INT32_MAX || span <= INT32_MIN) return -1;
    int64_t rx = (int64_t)origin->x - follower->x, ry = (int64_t)origin->y - follower->y, rz = (int64_t)origin->z - follower->z;
    if (rx > INT32_MAX || rx < INT32_MIN || ry > INT32_MAX || ry < INT32_MIN || rz > INT32_MAX || rz < INT32_MIN) return -1;
    int64_t py = (int64_t)player->y - origin->y;
    if (py > 2 * (int64_t)FW_TILE || py < -2 * (int64_t)FW_TILE) return -1;
    int alongX = ax >= az;
    int64_t moved = alongX ? (int64_t)player->x - origin->x : (int64_t)player->z - origin->z;
    int64_t lateral = alongX ? (int64_t)player->z - origin->z : (int64_t)player->x - origin->x;
    if (lateral > 4096 || lateral < -4096) return -1;
    if ((span > 0 && moved < 0) || (span < 0 && moved > 0)) return -1;
    int64_t clamped = moved;
    if (span > 0) { if (clamped > span) clamped = span; }
    else if (clamped < span) clamped = span;
    out->x = follower->x + mul_div((int32_t)rx, (int32_t)clamped, (int32_t)span);
    out->y = follower->y + mul_div((int32_t)ry, (int32_t)clamped, (int32_t)span);
    out->z = follower->z + mul_div((int32_t)rz, (int32_t)clamped, (int32_t)span);
    return clamped == span;
}
int fw_trail_reseed(FwTrail *trail, const FwSample *follower, const FwSample *player,
                    const uint8_t gaps[4], FwSample *out) {
    /* Forty units covers a gap of 12 (about 28 units to one metric tile)
     * and stays inside the 64-sample trail. Unit steps match a walked load. */
    enum { units = 40 };
    if (!trail || !follower || !player || !gaps || !out || player->direction > 3 || !player->generation) return 0;
    if (gaps[0] > FW_MAX_DIRECTIONAL_GAP || gaps[1] > FW_MAX_DIRECTIONAL_GAP ||
        gaps[2] > FW_MAX_DIRECTIONAL_GAP || gaps[3] > FW_MAX_DIRECTIONAL_GAP) return 0;
    int64_t dy64 = (int64_t)player->y - follower->y;
    if (dy64 > INT32_MAX || dy64 < INT32_MIN) return 0;
    int32_t dy = (int32_t)dy64;
    int64_t back = (int64_t)units * 4096;
    int32_t x = player->x, z = player->z, dx = 0, dz = 0;
    if (player->direction == 0) {
        if ((int64_t)z + back > INT32_MAX) return 0;
        z += (int32_t)back; dz = -4096;
    } else if (player->direction == 1) {
        if ((int64_t)z - back < INT32_MIN) return 0;
        z -= (int32_t)back; dz = 4096;
    } else if (player->direction == 2) {
        if ((int64_t)x + back > INT32_MAX) return 0;
        x += (int32_t)back; dx = -4096;
    } else {
        if ((int64_t)x - back < INT32_MIN) return 0;
        x -= (int32_t)back; dx = 4096;
    }
    fw_trail_clear(trail);
    int ready = 0;
    for (int i = 0; i <= units; ++i) {
        FwSample step = *player;
        step.x = x + dx * i; step.z = z + dz * i;
        step.y = follower->y + mul_div(dy, i, units);
        step.direction = player->direction;
        ready = fw_trail_push_directional(trail, &step, out, gaps);
        if (ready < 0) return 0;
    }
    return ready == 1;
}
int fw_trail_reseed_along(FwTrail *trail, const FwSample *from, const FwSample *player,
                          const uint8_t gaps[4], FwSample *out) {
    /* Same forty-unit history as the compass reseed, walked along the chord
     * the player actually travelled. Rail facings are not world axes. */
    enum { units = 40 };
    if (!trail || !from || !player || !gaps || !out || player->direction > 3 || !player->generation) return 0;
    if (gaps[0] > FW_MAX_DIRECTIONAL_GAP || gaps[1] > FW_MAX_DIRECTIONAL_GAP ||
        gaps[2] > FW_MAX_DIRECTIONAL_GAP || gaps[3] > FW_MAX_DIRECTIONAL_GAP) return 0;
    int64_t dy64 = (int64_t)player->y - from->y;
    if (dy64 > INT32_MAX || dy64 < INT32_MIN) return 0;
    int64_t dx = (int64_t)player->x - from->x, dz = (int64_t)player->z - from->z;
    uint32_t span = segment_span(dx, dz);
    if (span < 256) return 0;
    int64_t back = (int64_t)units * 4096;
    int32_t ox = div_s64(dx * back, span), oz = div_s64(dz * back, span);
    int64_t fx = (int64_t)player->x - ox, fz = (int64_t)player->z - oz;
    if (fx < INT32_MIN || fx > INT32_MAX || fz < INT32_MIN || fz > INT32_MAX) return 0;
    int32_t dy = (int32_t)dy64;
    fw_trail_clear(trail);
    int ready = 0;
    for (int i = 0; i <= units; ++i) {
        FwSample step = *player;
        step.x = (int32_t)fx + mul_div(ox, i, units);
        step.z = (int32_t)fz + mul_div(oz, i, units);
        step.y = from->y + mul_div(dy, i, units);
        step.direction = player->direction;
        ready = fw_trail_push_directional(trail, &step, out, gaps);
        if (ready < 0) return 0;
    }
    return ready == 1;
}
