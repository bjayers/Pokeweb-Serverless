#ifndef POKEWEB_FOLLOWING_H
#define POKEWEB_FOLLOWING_H
#include <stdint.h>
#include <stddef.h>
#define FW_ABI 1
#define FW_TRAIL_CAPACITY 64
#define FW_PARTY_CAPACITY 6
#define FW_CODE_BASE 0x3000u
#define FW_STOCK_ROWS 1008u
#ifdef FW_UPGRADE
#define FW_MAX_SPECIES 1023u
#define FW_MAX_ROWS 6144u
#else
#define FW_MAX_SPECIES 649u
#define FW_MAX_ROWS 4096u
#endif
#define FW_TILE (16 * 4096)
/* Native units, approximately pixels at the ordinary field camera scale. */
#define FW_MAX_SIDE_GAP 6u
#define FW_DEFAULT_SIDE_GAP_BONUS 6u
#define FW_MAX_DIRECTIONAL_GAP (FW_MAX_SIDE_GAP + FW_DEFAULT_SIDE_GAP_BONUS)
#define FW_DIALOGUE_REACH(gap) (FW_TILE + ((gap) + 1u) * 4096u)
typedef enum { FW_ABSENT, FW_WAITING, FW_FOLLOWING, FW_INTERACTING, FW_SUPPRESSED, FW_DESTROYING, FW_EVENT_PAUSED } FwState;
typedef enum { FW_MENU, FW_DIALOGUE, FW_SCRIPT, FW_WARP, FW_BATTLE, FW_BLACKOUT,
    FW_PARTNER, FW_BIKE, FW_SURF, FW_DIVE, FW_FISHING, FW_FIELD_MOVE,
    FW_COMMUNICATION, FW_ACTIVITY, FW_RESOURCE_FAILURE, FW_DISABLED, FW_REASON_COUNT } FwReason;
typedef enum { FW_GRID, FW_LEDGE, FW_RAIL, FW_WORLD } FwMovement;
typedef struct { uint16_t species; uint8_t form, gender, shiny, egg; uint16_t hp, max_hp; uint32_t personality, trainer; } FwPokemon;
typedef struct {
    int32_t x, y, z;
    uint32_t generation, space;
    uint16_t rail, connection;
    uint8_t direction, kind;
    uint16_t duration;
} FwSample;
typedef struct { int32_t x, y, z; } FwPoint;
typedef struct {
    FwSample samples[FW_TRAIL_CAPACITY];
    uint32_t distance;
    uint16_t head, count;
} FwTrail;
typedef struct {
    FwState state;
    uint8_t suppression[FW_REASON_COUNT];
    uint32_t generation;
    uintptr_t actor;
    FwPokemon selected;
    int8_t slot;
    uint8_t has_selection;
    uint8_t side_gap; /* Uses existing sidecar padding: 0..6 world units. */
    int8_t sprite_y; /* Registry draw-only offset; native shadow stays grounded. */
    FwTrail trail;
    uint8_t directional_gap[4]; /* Up, down, left, right; selected ROM positioning row. */
} FwFollower;
int fw_select(const FwPokemon *party, unsigned count);
/* L/R selection wraps over non-Egg party members, preferring healthy members
 * exactly as automatic lead selection does. Does not reorder the party. */
int fw_cycle_slot(const FwPokemon *party, unsigned count, int current, int direction);
int fw_same_identity(const FwPokemon *a, const FwPokemon *b);
int fw_tired(const FwPokemon *mon);
/* The native billboard owns the directional loop.  This gate keeps its last
 * pose during private interaction, scene and effect ownership. */
int fw_idle_animation_enabled(FwState state, unsigned visible, unsigned effect_busy);
void fw_init(FwFollower *f, uint32_t generation);
void fw_enter(FwFollower *f, FwReason reason);
void fw_leave(FwFollower *f, FwReason reason);
int fw_suppressed(const FwFollower *f);
int fw_interact(FwFollower *f);
void fw_end_interaction(FwFollower *f);
void fw_trail_clear(FwTrail *trail);
/* Returns -1 on discontinuity/overflow (caller must recall); 0 while seeding;
 * 1 when a pose on the recorded segment is ready. That pose is the point one
 * tile of follow metric behind the player, so a long run step does not rest
 * farther back than a walk. It is never a shortcut off the recorded route. */
int fw_trail_push(FwTrail *trail, const FwSample *sample, FwSample *follower, unsigned side_gap);
int fw_trail_push_directional(FwTrail *trail, const FwSample *sample, FwSample *follower, const uint8_t gaps[4]);
/* Add a connection delta to every live sample. Distances stay valid.
 * Returns 0 and clears the trail if any coordinate would overflow. */
int fw_trail_translate(FwTrail *trail, int32_t dx, int32_t dy, int32_t dz);
/* Floor tile index. World centers sit at tile*FW_TILE + FW_TILE/2. */
int32_t fw_tile(int32_t coord);
/* 1 when `follower` stands one cardinal tile from `origin`, `face` points
 * from origin onto that tile, the height gap is at most two tiles, and the
 * trail's next tile is origin. That is a step back onto the follower. */
int fw_swap_match(const FwTrail *trail, const FwPoint *follower, const FwPoint *origin, unsigned face);
/* Mirror `player`'s progress from `origin` toward `follower` onto the
 * follower's walk from `follower` back to `origin`. Returns 1 at the far
 * end, 0 while still on the segment, -1 if the player left it. */
int fw_swap_mirror(const FwPoint *follower, const FwPoint *origin, const FwPoint *player, FwPoint *out);
/* Rail points are not grid tiles. The follower is the next recorded point
 * behind `origin`, at any world spacing up to eight tiles. */
int fw_rail_swap_match(const FwTrail *trail, const FwPoint *follower, const FwPoint *origin);
/* Project onto the full rail segment, including a diagonal. 1 at the far end,
 * or when `step_settled` and the player has moved toward the follower (one
 * rail knot is often shorter than the follow gap), 0 while that step is still
 * moving, -1 if the player left the segment. */
int fw_rail_swap_mirror(const FwPoint *follower, const FwPoint *origin, const FwPoint *player, FwPoint *out, int step_settled);
/* Facing applied to the follower. `moving` is the step back toward `origin`;
 * the finished swap uses the player's own facing. */
unsigned fw_swap_face(unsigned player_face, int moving);
/* Rail facing is relative to the line: 0 +side, 1 -side, 2 +front, 3 -front.
 * `dFront`/`dSide` are player minus follower. The larger axis wins; a tie
 * faces along the line. */
unsigned fw_rail_face_toward(int d_front, int d_side);
/* Same choice from world deltas. `front_*` is the +front step and `side_*`
 * is the +side step. Returns 4 when both steps are degenerate. */
unsigned fw_rail_face_at(int rel_x, int rel_z, int front_x, int front_z, int side_x, int side_z);
/* Post-swap facing. Both actors share the player's line, so this is the
 * player's rail facing on a straight line and on a curve. The chord is the
 * world step just taken; it is not a compass to quantize. */
unsigned fw_rail_swap_turn(unsigned player_face, int chord_x, int chord_z);
/* Sprite/palette/size form. A small species table pins Unown (201) to '?'
 * (Gen 5 form 27) and Arceus (493) to Normal (form 0). Other species pass through. */
unsigned fw_follower_form(unsigned species, unsigned form);
/* Replace the trail with a unit-step walk that ends on `player` and uses
 * `gaps`. The pose is one metric tile behind, which is the fresh-load rest,
 * and later steps keep that distance. `follower` supplies the far height.
 * Samples take `player`'s direction, so the next step does not reuse the
 * facing from before the turnaround. */
int fw_trail_reseed(FwTrail *trail, const FwSample *follower, const FwSample *player,
                    const uint8_t gaps[4], FwSample *out);
/* Same rest distance, walked along the chord from `from` to `player` instead
 * of the compass axis. Rail facings are not world axes. */
int fw_trail_reseed_along(FwTrail *trail, const FwSample *from, const FwSample *player,
                          const uint8_t gaps[4], FwSample *out);
/* Player travel, in world units, over which a post-swap gap eases from the
 * close pose out to one metric tile (the fresh-load rest). Distance, not
 * frames: a run covers it in fewer steps than a walk. */
#define FW_EASE_TRAVEL (2u * FW_TILE)
/* Smoothstep in 16.16: 0 at the start, 65536 at `length`, flat at both ends. */
uint32_t fw_ease_smoothstep(uint32_t traveled, uint32_t length);
/* Metric keep. `full_keep` once `traveled` reaches `length`, or when
 * `close_keep` is already there. */
uint32_t fw_ease_keep(uint32_t traveled, uint32_t length, uint32_t close_keep, uint32_t full_keep);
/* Euclidean XZ length. This is the ease clock. */
uint32_t fw_travel(int32_t x0, int32_t z0, int32_t x1, int32_t z1);
/* Point `keep` metric units behind the newest sample. `FW_TILE` matches
 * fw_trail_push_directional, including the older sample's direction. */
int fw_trail_pose_behind(const FwTrail *trail, const uint8_t gaps[4], uint32_t keep, FwSample *out);
/* Metric from the newest sample back to the trail projection of (x, z).
 * 0 when it is not on the trail. Never more than one tile. */
uint32_t fw_trail_metric_to_point(const FwTrail *trail, const uint8_t gaps[4], int32_t x, int32_t z);
/* Grid talk box: one tile of reach on the facing axis. Two tiles do not pass. */
int fw_grid_talk_span(const FwPoint *player, const FwPoint *follower, unsigned face, unsigned gap);
/* Follower lies on the segment from the player to the adjacent rail point.
 * The segment may be wider than a grid tile. A point past that rail point fails. */
int fw_rail_segment_reach(const FwPoint *player, const FwPoint *follower, const FwPoint *forward, unsigned gap);
/* Bound a nearly-adjacent follower's world pose to the same forward reach
 * used by dialogue, without changing its route or authored gap. */
int fw_clamp_dialogue_pose(FwPoint *pose,const FwPoint *player,const FwPoint *art_offset,
                           unsigned face,unsigned gap);
uint16_t fw_stock_row(uint16_t code);
uint16_t fw_object_row(uint16_t code, uint16_t extension_count);
uint32_t fw_descriptor_offset(uint16_t row);
#endif
