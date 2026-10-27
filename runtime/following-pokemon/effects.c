__attribute__((visibility("hidden"))) void *memset(void *to, int value, unsigned length) {
    unsigned char *p = to;
    while (length--) *p++ = (unsigned char)value;
    return to;
}
#include "effects.h"
#include "effects_assets.h"
#include "ball_assets.h"
#include "render.h"
#include "render_math.h"
/* Field sets this on the update that first shows the body. Same DLL. */
extern int fwfield_first_body(void);
/* Freestanding aggregate copies; no dependency on a gameplay module's libc. */
__attribute__((visibility("hidden"))) void *memcpy(void *to, const void *from, unsigned length) {
    uint8_t *d = to;
    const uint8_t *s = from;
    while (length--) *d++ = *s++;
    return to;
}
/* Private resource ownership: neither a retail palette nor material is edited. */
typedef struct {
    Vec pos, scale;
    int32_t rotation[9];
} Transform;
typedef struct {
    uint16_t geom, pad;
    Vec pos;
    uint16_t face;
    int16_t sx, sy;
    uint16_t rotation, flags, pad2;
} Billboard;
typedef struct {
    void *resource;
    uint16_t width, height;
    uint8_t fw, fh, cols, rows;
    uint32_t tex, pltt, format, dx, dy, texKey, plttKey;
} Material;
typedef struct {
    uint16_t heap, pad;
    Material *materials;
    Billboard *actors;
    uint16_t materialCount, actorCount;
    Vec scale;
    uint16_t diffuse, ambient, specular, emissive;
    uint8_t polygon, origin;
    uint16_t pad2;
    uint8_t rest[20];
} Scene;
_Static_assert(sizeof(Scene) == 60, "billboard scene ABI");
_Static_assert(sizeof(Material) == 40, "billboard material ABI");
_Static_assert(sizeof(Billboard) == 28, "billboard actor ABI");
static struct {
    void *resources[3], *models[2], *actors[2], *animation, *white;
    uint16_t whiteID;
    int ready, mode, age, snapshot;
    uint8_t outlineTop[8], outlineBottom[8], outlineSize, outlineFrames;
    Scene scene;
    Billboard billboard;
    Material material, whiteMaterial;
    Transform transform;
} fwfx;
static Actor *fwfx_player;
/* Set only from the validated installed registry. The retail archive has 975
 * members, but imported follower appearances extend it. */
static uint16_t fwfx_resourceCount;
/* The two bright colors in the imported send-out flash palette (BGR555).
 * Keep a little of the source sprite's light/dark structure in the silhouette. */
#define FWFX_LIGHT_CYAN          0x7f93u
#define FWFX_WHITE               0x7fffu
#define FWFX_RECALL_TICKS        12
#define FWFX_RECALL_SPRITE_TICKS 8
/* Stock White 2 battle effects: switch-out return 620 and send-out 621 both
 * use SE 1383 for the ball. Effect 619 is Sunny Day, not a ball effect. */
#define FWFX_BALL_SE 1383u
/* SEQ_SE_BEEP (pokebw2 generated/sound.txt), the menus' "can't do that". */
#define FWFX_BEEP_SE 1367u
/* Ticks the closed ball stays up for a refused switch, and how many of them
 * shake it a pixel either way. */
#define FWFX_CUE_TICKS 30
#define FWFX_CUE_SHAKE 12
__attribute__((visibility("default"))) volatile uint32_t FollowingEffectsDebug[8] = {0x58465746, 1, 0, 0,
                                                                                     0,          0, 0, 0};
static uint32_t checksum(const uint8_t *p, unsigned n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (unsigned j = 0; j < 8; ++j) c = (c >> 1) ^ ((0u - (c & 1)) & 0xedb88320u);
    }
    return ~c;
}
static int verified(void) {
    uint32_t f[32], data[(FW_EFFECT_BYTES + 3) / 4];
    CALL(0x02070ca9, void (*)(void *))(f);
    if (!CALL(0x02070ecd, int (*)(void *, const char *))(f, "rom:/following/effects.narc")) return 0;
    int ok = CALL(0x02070ded, uint32_t(*)(void *))(f) == FW_EFFECT_BYTES;
    if (ok)
        ok = CALL(0x02070e6d, uint32_t(*)(void *, void *, uint32_t))(f, data, FW_EFFECT_BYTES) ==
                 FW_EFFECT_BYTES &&
             checksum((uint8_t *)data, FW_EFFECT_BYTES) == FW_EFFECT_CRC;
    CALL(0x02070de1, int (*)(void *))(f);
    return ok;
}
/* Validated follower/emote assets are linear I4 frames with transparent index
 * zero. Inspect the existing private resource before its GPU upload; shared
 * native actor materials may have trimmed their CPU pixel data already.
 * Keep only two byte-sized row bounds per pose, never a second texture copy. */
int fwfx_outline(const void *texture, unsigned size, uint8_t *top, uint8_t *bottom) {
    if (!texture || (size != 32 && size != 64) || U32(texture, 0) != 0x30584554u) return 0;
    unsigned length = *(uint16_t *)((const uint8_t *)texture + 12) * 8u;
    unsigned start = U32(texture, 20), total = U32(texture, 4), stride = size * size / 2;
    unsigned frames = length >> (size == 64 ? 11 : 9);
    if (!frames || frames > 8 || (length & (stride - 1)) || total > 128 * 1024 || start < 60 ||
        start > total || length > total - start)
        return 0;
    const uint8_t *pixels = (const uint8_t *)texture + start;
    for (unsigned f = 0; f < frames; ++f) {
        top[f] = (uint8_t)size;
        bottom[f] = 0;
        for (unsigned y = 0; y < size; ++y) {
            const uint8_t *row = pixels + f * stride + y * (size / 2);
            for (unsigned x = 0; x < size / 2; ++x)
                if (row[x]) {
                    if (top[f] == size) top[f] = (uint8_t)y;
                    bottom[f] = (uint8_t)y;
                    break;
                }
        }
    }
    return (int)frames;
}
/* Texture keys stay named by the GPU's queued frame. Land already waits two
 * field ticks before 0x02049561. Emote end and the white-sprite reload do too.
 * Map teardown does not: the field clock stops while a load, battle, or menu
 * has the overlay, so a two-tick wait fires on the first visible frames of
 * the next field and streaks that picture. Those pointers belong to the old
 * allocator. Drop them. The next 3D setup reclaims the banks. */
#define FWFX_HELD 4
typedef struct {
    void *white, *animation;
    void *resources[3], *models[2], *actors[2], *emote[2];
    unsigned tick;
    unsigned drop;
} FwfxHeld;
static FwfxHeld fwfx_held[FWFX_HELD];
static unsigned fwfx_clock;
static int fwfx_drop;
static int held_used(const FwfxHeld *h) {
    if (h->white || h->animation) return 1;
    for (unsigned i = 0; i < 3; ++i)
        if (h->resources[i]) return 1;
    for (unsigned i = 0; i < 2; ++i)
        if (h->models[i] || h->actors[i] || h->emote[i]) return 1;
    return 0;
}
static void held_release(FwfxHeld *h) {
    if (h->drop) {
        *h = (FwfxHeld){0};
        return;
    }
    if (h->white) {
        CALL(0x02049561, int (*)(void *))(h->white);
        CALL(0x02049431, void (*)(void *))(h->white);
        h->white = 0;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (h->actors[i]) {
            CALL(0x02049961, void (*)(void *))(h->actors[i]);
            h->actors[i] = 0;
        }
    if (h->animation) {
        CALL(0x020498b5, void (*)(void *))(h->animation);
        h->animation = 0;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (h->models[i]) {
            CALL(0x02049801, void (*)(void *))(h->models[i]);
            h->models[i] = 0;
        }
    for (unsigned i = 0; i < 3; ++i)
        if (h->resources[i]) {
            if (i < 2) CALL(0x02049561, int (*)(void *))(h->resources[i]);
            CALL(0x02049431, void (*)(void *))(h->resources[i]);
            h->resources[i] = 0;
        }
    for (unsigned i = 0; i < 2; ++i)
        if (h->emote[i]) {
            CALL(0x02049561, int (*)(void *))(h->emote[i]);
            CALL(0x02049431, void (*)(void *))(h->emote[i]);
            h->emote[i] = 0;
        }
}
static void held_pump(void) {
    for (unsigned i = 0; i < FWFX_HELD; ++i)
        if (held_used(&fwfx_held[i]) && (fwfx_held[i].drop || fwfx_clock - fwfx_held[i].tick >= 2u))
            held_release(&fwfx_held[i]);
}
static FwfxHeld *held_open(void) {
    for (unsigned i = 0; i < FWFX_HELD; ++i)
        if (!held_used(&fwfx_held[i])) {
            fwfx_held[i].tick = fwfx_clock;
            fwfx_held[i].drop = fwfx_drop;
            return &fwfx_held[i];
        }
    unsigned oldest = 0;
    for (unsigned i = 1; i < FWFX_HELD; ++i)
        if (fwfx_clock - fwfx_held[i].tick > fwfx_clock - fwfx_held[oldest].tick) oldest = i;
    held_release(&fwfx_held[oldest]);
    fwfx_held[oldest].tick = fwfx_clock;
    fwfx_held[oldest].drop = fwfx_drop;
    return &fwfx_held[oldest];
}
static void clear_white_keys(void) {
    fwfx.snapshot = 0;
    fwfx.outlineFrames = 0;
    fwfx.whiteMaterial.texKey = fwfx.whiteMaterial.plttKey = 0;
    FollowingEffectsDebug[7] = 0;
}
static void hold_white(void) {
    if (fwfx.white) {
        FwfxHeld *h = held_open();
        h->white = fwfx.white;
        fwfx.white = 0;
    }
    clear_white_keys();
}
static void hold_effects(void) {
    int any = fwfx.white || fwfx.animation;
    for (unsigned i = 0; i < 3 && !any; ++i)
        if (fwfx.resources[i]) any = 1;
    for (unsigned i = 0; i < 2 && !any; ++i)
        if (fwfx.models[i] || fwfx.actors[i]) any = 1;
    if (!any) {
        clear_white_keys();
        return;
    }
    FwfxHeld *h = held_open();
    h->white = fwfx.white;
    fwfx.white = 0;
    h->animation = fwfx.animation;
    fwfx.animation = 0;
    for (unsigned i = 0; i < 3; ++i) {
        h->resources[i] = fwfx.resources[i];
        fwfx.resources[i] = 0;
    }
    for (unsigned i = 0; i < 2; ++i) {
        h->models[i] = fwfx.models[i];
        fwfx.models[i] = 0;
        h->actors[i] = fwfx.actors[i];
        fwfx.actors[i] = 0;
    }
    clear_white_keys();
}
static int init(void);
static void free_white(void) {
    hold_white();
}
void fwfx_cancel(void) {
    fwfx.mode = 0;
    FollowingEffectsDebug[2] = 0;
}
void fwfx_set_resource_count(unsigned count) {
    if (count > 65535) count = 0;
    if (fwfx_resourceCount != count) free_white();
    fwfx_resourceCount = (uint16_t)count;
}
void fwfx_destroy(void) {
    /* Unload, battle, and a new actor system all come through here. The clock
     * does not advance while this overlay is gone, so a deferred 0x02049561
     * would run on the first frames the overworld is visible again. */
    fwfx_player = 0;
    fwfx_drop = 1;
    fwfx_emote_end();
    fwfx_rest_at(0);
    fwfx_cancel();
    hold_effects();
    held_pump();
    fwfx_drop = 0;
    fwfx.ready = 0;
    FollowingEffectsDebug[6] = 0;
}
void fwfx_warm(uint32_t heap) {
    /* Effect banks only, during a door or return fade. A follower billboard
     * submit in that window is not covered: it put the load-screen lines back.
     * Emote VRAM stays on the talk bind. 0x0204e55d writes the material it is
     * passed and does not return a key. The next 3D setup reclaims a bank
     * allocated under this fade, so a copied key draws an empty quad. */
    if (fwfx.ready) return;
    if (heap && CALL(0x0203a2d5, uint32_t(*)(uint32_t))(heap) < 65536u) return;
    init();
}
/* Per-ball art. The HGSS ball model is 768 bytes: one 16x16 four-bit texture
 * at TEX0 + 0x90 and an 8-colour palette, the file's last 16 bytes. The art
 * uses up to 15 colours, so the loaded copy is moved to a block 16 bytes
 * longer with a 16-colour palette before its first upload. GFL_G3DResFree
 * frees that block like the original. A swap rewrites both in place and
 * resends them to the same VRAM keys (GFL_G3DResUploadTexDataCore).
 * fwfx_ballShown is the ball ID in the texture, 0 while the HGSS art is still
 * there and no swap is possible; fwfx_ballWant is the follower's. */
#define FWFX_BALL_FILE   768u
#define FWFX_BALL_TEX0   480u
#define FWFX_BALL_PIXELS 0x90u
#define FWFX_BALL_PLTT   0x110u
/* g_G3DSystem.sys; its heap ID is at +24 (White 2 US). */
#define FWFX_G3D_SYSTEM 0x02141964u
static uint8_t fwfx_ballShown, fwfx_ballWant = 4;
static int grow_ball(void) {
    void *res = fwfx.resources[0];
    uint8_t *old = PTR(res, 4), *tex0 = old + FWFX_BALL_TEX0;
    if (U32(old, 8) != FWFX_BALL_FILE || U32(old, 0x14) != FWFX_BALL_TEX0 || U32(tex0, 0) != 0x30584554u ||
        U32(tex0, 4) != FWFX_BALL_FILE - FWFX_BALL_TEX0 || *(uint16_t *)(tex0 + 0x0c) != 16 ||
        U32(tex0, 0x14) != FWFX_BALL_PIXELS || *(uint16_t *)(tex0 + 0x30) != 2 ||
        U32(tex0, 0x38) != FWFX_BALL_PLTT)
        return 0;
    void *sys = *(void **)FWFX_G3D_SYSTEM;
    if (!sys) return 0;
    unsigned heap = *(uint16_t *)((uint8_t *)sys + 24);
    /* GFL_HeapAllocateCore returns 0 when full; GFL_HeapAllocate would halt. */
    if (CALL(0x0203a2d5, uint32_t(*)(uint32_t))(heap) < 4096u) return 0;
    uint8_t *grown = CALL(0x02039dc9, void *(*)(unsigned, uint32_t))(heap, FWFX_BALL_FILE + 16);
    if (!grown) return 0;
    memcpy(grown, old, FWFX_BALL_FILE);
    memset(grown + FWFX_BALL_FILE, 0, 16);
    U32(grown, 8) = FWFX_BALL_FILE + 16;
    U32(grown + FWFX_BALL_TEX0, 4) += 16;
    *(uint16_t *)(grown + FWFX_BALL_TEX0 + 0x30) = 4;
    PTR(res, 4) = grown;
    CALL(0x0203a279, void (*)(void *))(old);
    return 1;
}
static void put_ball(unsigned ball) {
    uint8_t *tex0 = (uint8_t *)PTR(fwfx.resources[0], 4) + FWFX_BALL_TEX0;
    memcpy(tex0 + FWFX_BALL_PIXELS, fw_ball_tex[ball - 1], sizeof fw_ball_tex[0]);
    memcpy(tex0 + FWFX_BALL_PLTT, fw_ball_pltt[ball - 1], sizeof fw_ball_pltt[0]);
    fwfx_ballShown = (uint8_t)ball;
}
/* Update time only, never mid-effect: a recall keeps the ball it began with. */
static void ball_apply(void) {
    if (fwfx.ready != 1 || !fwfx_ballShown || fwfx_ballShown == fwfx_ballWant) return;
    put_ball(fwfx_ballWant);
    fw_vram_window();
    CALL(0x0204961d, int (*)(void *))(fwfx.resources[0]);
}
void fwfx_set_ball(unsigned ball) {
    fwfx_ballWant = (uint8_t)(ball && ball <= FW_BALL_COUNT ? ball : 4u);
    if (!fwfx.mode) ball_apply();
}
static int init_stop(void) {
    /* A deferred free can still own the previous map's heap. Drop this partial
     * load into that queue and try again once the tick releases it. A failed
     * archive check stays failed: there is nothing to wait for. */
    hold_effects();
    fwfx.ready = 0;
    return 0;
}
static int init(void) {
    if (fwfx.ready) return fwfx.ready > 0;
    fwfx.ready = -1;
    if (!verified()) return 0;
    for (unsigned i = 0; i < 3; ++i) {
        fwfx.resources[i] =
            CALL(0x020493f1, void *(*)(const char *, unsigned))("rom:/following/effects.narc", i);
        if (!fwfx.resources[i]) return init_stop();
    }
    /* Without room for the larger palette the HGSS ball stays, unswapped. */
    fwfx_ballShown = 0;
    if (grow_ball()) put_ball(fwfx_ballWant);
    for (unsigned i = 0; i < 2; ++i) {
        fw_vram_window();
        if (!CALL(0x020494d9, int (*)(void *))(fwfx.resources[i])) return init_stop();
        fwfx.models[i] =
            CALL(0x02049759, void *(*)(void *, unsigned, void *))(fwfx.resources[i], 0, fwfx.resources[i]);
        if (!fwfx.models[i]) return init_stop();
    }
    fwfx.animation =
        CALL(0x02049839, void *(*)(void *, void *, unsigned))(fwfx.models[1], fwfx.resources[2], 0);
    if (!fwfx.animation) return init_stop();
    fwfx.actors[0] = CALL(0x020498e5, void *(*)(void *, void **, int))(fwfx.models[0], 0, 0);
    fwfx.actors[1] = CALL(0x020498e5, void *(*)(void *, void **, int))(fwfx.models[1], &fwfx.animation, 1);
    if (!fwfx.actors[0] || !fwfx.actors[1]) return init_stop();
    CALL(0x020499a1, int (*)(void *, unsigned))(fwfx.actors[1], 0);
    fwfx.transform.scale = (Vec){4096, 4096, 4096};
    fwfx.transform.rotation[0] = fwfx.transform.rotation[4] = fwfx.transform.rotation[8] = 4096;
    fwfx.ready = 1;
    FollowingEffectsDebug[6] = 1;
    return 1;
}
void fwfx_prepare(Actor *actor) {
    if (fwfx.mode ||
        CALL(0x0203a2d5, uint32_t(*)(uint32_t))(((ActorSystem *)actor->system)->resourceHeap) < 65536 ||
        !init())
        return;
    uint16_t id = *(uint16_t *)(actor->descriptor + 16);
    if (fwfx.white && fwfx.whiteID == id) return;
    free_white();
    fwfx.whiteID = id;
    if (!fwfx_resourceCount || id >= fwfx_resourceCount) return;
    fwfx.white = CALL(0x020493f1, void *(*)(const char *, unsigned))("rom:/a/0/4/8", id);
    if (!fwfx.white) return;
    void *tex = CALL(0x0204964d, void *(*)(void *))(fwfx.white);
    if (!tex) {
        free_white();
        return;
    }
    fwfx.outlineSize = actor->descriptor[7] == 2 ? 64 : 32;
    fwfx.outlineFrames = (uint8_t)fwfx_outline(tex, fwfx.outlineSize, fwfx.outlineTop, fwfx.outlineBottom);
    unsigned length = CALL(0x020652e5, unsigned (*)(void *))(tex);
    uint16_t *palette = CALL(0x0204974d, uint16_t * (*)(void *))(fwfx.white);
    if (!palette || length > 512 || !length) {
        free_white();
        return;
    }
    unsigned count = length / 2, minLight = ~0u, maxLight = 0;
    for (unsigned i = 1; i < count; ++i) {
        unsigned color = palette[i];
        unsigned light = 3u * (color & 31u) + 6u * ((color >> 5) & 31u) + ((color >> 10) & 31u);
        if (light < minLight) minLight = light;
        if (light > maxLight) maxLight = light;
    }
    unsigned middle = minLight + (maxLight - minLight) / 2u;
    for (unsigned i = 0; i < count; ++i) {
        unsigned color = palette[i];
        unsigned light = 3u * (color & 31u) + 6u * ((color >> 5) & 31u) + ((color >> 10) & 31u);
        palette[i] =
            (uint16_t)(i && ((minLight == maxLight) ? (i & 1u) : (light > middle)) ? FWFX_WHITE
                                                                                   : FWFX_LIGHT_CYAN);
    }
    fw_vram_window();
    CALL(0x0204e55d, void (*)(void *, void *))(&fwfx.whiteMaterial, fwfx.white);
    if (!fwfx.whiteMaterial.texKey || !fwfx.whiteMaterial.plttKey) free_white();
}
void fwfx_snapshot(Actor *a) {
    if (!fwfx.white || fwfx.mode == 2 || (a->flags & 4)) return;
    void *fieldBl = PTR(a->system, 0x28);
    if (!fieldBl) return;
    void *bl = PTR(fieldBl, 4);
    if (!bl) return;
    Scene *scene = PTR(bl, 4);
    unsigned id = *(uint16_t *)a->drawWork;
    if (!scene || id >= *(uint16_t *)((uint8_t *)bl + 0x1c)) return;
    unsigned index = U32(PTR(bl, 0x18), id * 40);
    if (index >= scene->actorCount) return;
    Billboard *actor = &scene->actors[index];
    unsigned mat = actor->geom & 0x3fff;
    if (mat >= scene->materialCount) return;
    fwfx.billboard = *actor;
    fwfx.material = scene->materials[mat];
    /* Do not clone the field header. The recall flash is a second GX submit. */
    fwr_private_scene(&fwfx.scene, scene, &fwfx.billboard, &fwfx.material, 1, 1);
    fwfx.scene.diffuse = fwfx.scene.ambient = 0x7fff;
    fwfx.scene.specular = fwfx.scene.emissive = 0;
    fwfx.billboard.geom &= 0xc000;
    /* Keep the native draw-enable bit and its selected map lights. The old
     * 0xf200 mask cleared both, leaving a loaded silhouette invisible. */
    fwfx.billboard.flags |= 0x0200u;
    FwrEffectPose submitted;
    if (fwr_effect_pose(a, &fwfx.billboard.pos, &submitted)) {
        fwfx.billboard.pos = submitted.position;
        fwfx.billboard.sx = submitted.sx;
        fwfx.billboard.sy = submitted.sy;
    }
    /* Preserve animation layout; bind the already uploaded private VRAM keys. */
    fwfx.material.resource = fwfx.white;
    fwfx.material.tex = fwfx.whiteMaterial.tex;
    fwfx.material.pltt = fwfx.whiteMaterial.pltt;
    fwfx.material.texKey = fwfx.whiteMaterial.texKey;
    fwfx.material.plttKey = fwfx.whiteMaterial.plttKey;
    fwfx.snapshot = 1;
    FollowingEffectsDebug[7] = 1;
}
static void position(Actor *a) {
    fwfx.transform.pos = a->world;
    fwfx.transform.pos.z += 6 * 4096;
}
void fwfx_out(Actor *a) {
    if (fwfx.ready != 1) return;
    ball_apply();
    position(a);
    fwfx.mode = 1;
    fwfx.age = 0;
    FollowingEffectsDebug[2] = 1;
    FollowingEffectsDebug[5] = 0;
    FollowingEffectsDebug[3]++;
}
void fwfx_recall(Actor *a) {
    if (fwfx.ready != 1 || fwfx.mode == 2) return;
    position(a);
    fwfx.mode = 2;
    fwfx.age = 0;
    FollowingEffectsDebug[2] = 2;
    FollowingEffectsDebug[5] = 0;
    FollowingEffectsDebug[4]++;
    CALL(0x02006255, void (*)(unsigned))(FWFX_BALL_SE);
}
/* A switch with nothing allowed to come out: the closed ball where the
 * follower stands, a short shake, the beep. Mode 3. */
void fwfx_cue(const Vec *at) {
    CALL(0x02006255, void (*)(unsigned))(FWFX_BEEP_SE);
    if (fwfx.ready != 1 || fwfx.mode || !at) return;
    fwfx.transform.pos = *at;
    fwfx.transform.pos.z += 6 * 4096;
    fwfx.mode = 3;
    fwfx.age = 0;
    FollowingEffectsDebug[2] = 3;
    FollowingEffectsDebug[5] = 0;
}
/* The closed ball resting where a recalled follower waits. It is not an
 * effect mode, so it never blocks one; any mode draws instead of it. It hops
 * one step per update while the pose moves, and lands before stopping. A
 * recall or a cue that starts on it moves with it, hop included, so the
 * shrink into the ball and the refusal shake travel along the trail. */
static struct {
    Vec at;
    uint8_t on, step, bounce;
} fwfx_rest;
#define FWFX_BOUNCE_TICKS 10u
static const uint8_t fwfx_bounce[FWFX_BOUNCE_TICKS] = {0, 3, 5, 6, 5, 3, 0, 1, 1, 0};
void fwfx_rest_at(const Vec *at) {
    if (!at) {
        fwfx_rest.on = 0;
        fwfx_rest.step = 0;
        fwfx_rest.bounce = 0;
        return;
    }
    /* A requested bounce plays once the ball is up and no effect draws. */
    if (fwfx_rest.bounce && !fwfx.mode && fwfx_rest.on && ++fwfx_rest.bounce > FWFX_BOUNCE_TICKS)
        fwfx_rest.bounce = 0;
    int moved = fwfx_rest.on && (fwfx_rest.at.x != at->x || fwfx_rest.at.z != at->z);
    if (fwfx_rest.on && (fwfx.mode == 2 || fwfx.mode == 3)) {
        int32_t dx = at->x - fwfx_rest.at.x, dy = at->y - fwfx_rest.at.y, dz = at->z - fwfx_rest.at.z;
        fwfx.transform.pos.x += dx;
        fwfx.transform.pos.y += dy;
        fwfx.transform.pos.z += dz;
        if (fwfx.mode == 2) {
            fwfx.billboard.pos.x += dx;
            fwfx.billboard.pos.y += dy;
            fwfx.billboard.pos.z += dz;
        }
    }
    if (moved || (fwfx_rest.step & 7u)) fwfx_rest.step = (uint8_t)((fwfx_rest.step + 1u) & 7u);
    fwfx_rest.at = *at;
    fwfx_rest.on = 1;
}
int fwfx_resting(void) {
    return fwfx_rest.on;
}
void fwfx_rest_bounce(void) {
    fwfx_rest.bounce = 1;
}
void fwfx_rest_recall(void) {
    if (fwfx_rest.on) CALL(0x02006255, void (*)(unsigned))(FWFX_BALL_SE);
    fwfx_rest.on = 0;
    fwfx_rest.step = 0;
}
int fwfx_busy(void) {
    return fwfx.mode != 0;
}
int fwfx_recalling(void) {
    return fwfx.mode == 2;
}
int fwfx_available(void) {
    return fwfx.ready == 1;
}
int fwfx_hides_actor(void) {
    return fwfx.mode == 1 && fwfx.age < 2;
}
void fwfx_tick(void) {
    ++fwfx_clock;
    held_pump();
    if (!fwfx.mode) {
        ball_apply();
        return;
    }
    if (++fwfx.age >= (fwfx.mode == 1 ? 10 : fwfx.mode == 3 ? FWFX_CUE_TICKS : FWFX_RECALL_TICKS)) {
        fwfx_cancel();
        return;
    }
    if (fwfx.mode == 1 && fwfx.age == 2) CALL(0x02006255, void (*)(unsigned))(FWFX_BALL_SE);
    FollowingEffectsDebug[2] = fwfx.mode;
    FollowingEffectsDebug[5] = fwfx.age;
}
static void draw_emote(void *camera, void *light);
/* The closed ball at the effect transform, nudged by dx and raised by the
 * resting hop. */
static void draw_ball(int32_t dx) {
    static const uint8_t hop[8] = {0, 1, 2, 2, 1, 0, 0, 0};
    Vec at = fwfx.transform.pos;
    fwfx.transform.pos.x += dx;
    if (fwfx_rest.bounce > 1)
        fwfx.transform.pos.y += fwfx_bounce[fwfx_rest.bounce - 1u] * 4096;
    else if (fwfx_rest.on)
        fwfx.transform.pos.y += hop[fwfx_rest.step & 7u] * 4096;
    CALL(0x02049b89, void (*)(void *, void *))(fwfx.actors[0], &fwfx.transform);
    fwfx.transform.pos = at;
}
void fwfx_draw(void *camera, void *light) {
    draw_emote(camera, light);
    if (fwfx.ready != 1) return;
    if (!fwfx.mode) {
        if (!fwfx_rest.on) return;
        fwfx.transform.pos = fwfx_rest.at;
        fwfx.transform.pos.z += 6 * 4096;
        draw_ball(0);
        return;
    }
    if (fwfx.mode == 3) {
        draw_ball(fwfx.age < FWFX_CUE_SHAKE ? ((fwfx.age & 2) ? 4096 : -4096) : 0);
    } else if (fwfx.mode == 2 && fwfx.age < FWFX_RECALL_SPRITE_TICKS && fwfx.snapshot) {
        /* Three recolored frames establish the silhouette, then five quick steps
         * pull it into the ball. The remaining four frames retain the ball art. */
        static const int scale[8] = {4096, 4096, 4096, 3440, 2784, 2112, 1456, 768};
        Billboard original = fwfx.billboard;
        fwfx.billboard.sx = (fwfx.billboard.sx * scale[fwfx.age]) >> 12;
        fwfx.billboard.sy = (fwfx.billboard.sy * scale[fwfx.age]) >> 12;
        CALL(0x0204ebdd, void (*)(void *, void *, void *))(&fwfx.scene, camera, light);
        fwfx.billboard = original;
    } else if ((fwfx.mode == 1 && fwfx.age < 2) || fwfx.mode == 2) {
        draw_ball(0);
    } else {
        /* The body's first raster is the retail billboard alone. The flash model
         * starts on the next frame, so the two submits do not share that scan. */
        if (fwfield_first_body()) return;
        int32_t frame = (fwfx.age - 2) * 4096;
        CALL(0x02049a11, int (*)(void *, unsigned, void *))(fwfx.actors[1], 0, &frame);
        CALL(0x02049b89, void (*)(void *, void *))(fwfx.actors[1], &fwfx.transform);
    }
}

static struct {
    void *resources[2];
    Scene scene;
    Billboard billboard;
    Material material[2];
    int active;
    unsigned frame;
    Actor *actor;
    uint8_t bottomInset[2];
    uint8_t hold;
} fwemote;
void fwfx_set_player(Actor *player) {
    fwfx_player = player;
}
void fwfx_emote_end(void) {
    fwemote.active = 0;
    fwemote.actor = 0;
    fwemote.hold = 0;
    if (!fwemote.resources[0] && !fwemote.resources[1]) return;
    FwfxHeld *h = held_open();
    for (unsigned i = 0; i < 2; ++i) {
        h->emote[i] = fwemote.resources[i];
        fwemote.resources[i] = 0;
    }
}
int fwfx_emote_begin(Actor *a, unsigned member) {
    fwfx_emote_end();
    if (member >= 28 ||
        CALL(0x0203a2d5, uint32_t(*)(uint32_t))(((ActorSystem *)a->system)->resourceHeap) < 32768)
        return 0;
    void *fieldBl = PTR(a->system, 0x28);
    if (!fieldBl) return 0;
    void *bl = PTR(fieldBl, 4);
    if (!bl) return 0;
    Scene *scene = PTR(bl, 4);
    unsigned id = *(uint16_t *)a->drawWork;
    if (!scene || id >= *(uint16_t *)((uint8_t *)bl + 0x1c)) return 0;
    unsigned index = U32(PTR(bl, 0x18), id * 40);
    if (index >= scene->actorCount) return 0;
    /* Own the header. The live polygon id and the 20-byte tail are the 3DS
     * black-line state. fwr_private_scene is the only place that chooses the
     * replacement id. */
    fwemote.billboard = scene->actors[index];
    fwr_private_scene(&fwemote.scene, scene, &fwemote.billboard, fwemote.material, 1, 2);
    fwemote.scene.diffuse = fwemote.scene.ambient = 0x7fff;
    fwemote.scene.specular = fwemote.scene.emissive = 0;
    fwemote.billboard.geom &= 0xc000;
    /* Retain the follower's selected map lights and enable rendering for the
     * private quad. Clearing 0xf200 made loaded emotes invisible. */
    fwemote.billboard.flags = (fwemote.billboard.flags | 0x0200u) & ~0x0c00u;
    fwemote.billboard.sx = fwemote.billboard.sy = 8192;
    fwemote.billboard.face = 0;
    for (unsigned i = 0; i < 2; ++i) {
        fwemote.material[i] = (Material){0};
        fwemote.resources[i] =
            CALL(0x020493f1, void *(*)(const char *, unsigned))("rom:/following/emotes.narc", member + i);
        if (!fwemote.resources[i]) {
            fwfx_emote_end();
            return 0;
        }
        uint8_t top[8], bottom[8];
        void *tex = CALL(0x0204964d, void *(*)(void *))(fwemote.resources[i]);
        fwemote.bottomInset[i] =
            fwfx_outline(tex, 32, top, bottom) == 1 && top[0] < 32 ? (uint8_t)(31 - bottom[0]) : 1;
        CALL(0x0204e599, void (*)(void *, unsigned, unsigned, unsigned, unsigned))
        (&fwemote.material[i], 0, 0x22, 32, 32);
        /* This object is the one fwr_private_scene stored. Binding any other
         * material and copying the words does not keep the reservation. */
        fw_vram_window();
        CALL(0x0204e55d, void (*)(void *, void *))(&fwemote.material[i], fwemote.resources[i]);
        if (!fwemote.material[i].texKey || !fwemote.material[i].plttKey) {
            fwfx_emote_end();
            return 0;
        }
    }
    /* The bind and the first submit must not share a scan. The next draw
     * submits this same material. */
    fwemote.hold = 1;
    fwemote.active = 1;
    fwemote.actor = a;
    fwfx_emote_frame(a, 0);
    return 1;
}
void fwfx_emote_frame(Actor *a, unsigned frame) {
    if (!fwemote.active) return;
    fwemote.billboard.geom = (fwemote.billboard.geom & 0xc000) | (frame & 1);
    fwemote.frame = frame;
    fwemote.actor = a;
}
static void draw_emote(void *camera, void *light) {
    if (!fwemote.active || !fwemote.actor || !camera) return;
    if (fwemote.hold) {
        fwemote.hold = 0;
        return;
    }
    Actor *a = fwemote.actor;
    void *fieldBl = PTR(a->system, 0x28), *bl = fieldBl ? PTR(fieldBl, 4) : 0;
    Scene *scene = bl ? PTR(bl, 4) : 0;
    unsigned id = *(uint16_t *)a->drawWork;
    if (!scene || id >= *(uint16_t *)((uint8_t *)bl + 0x1c)) return;
    unsigned index = U32(PTR(bl, 0x18), id * 40);
    if (index >= scene->actorCount) return;
    Billboard *body = &scene->actors[index];
    /* The body quad is restored to the engine foot after submit. The bubble
     * follows the submitted pose: registry Y plus the two-pixel lift, and a
     * bob that landed on the live quad after that restore. It does not keep
     * a second foot line. */
    FwrEffectPose pose = {body->pos, body->sx, body->sy};
    fwr_effect_pose(a, &body->pos, &pose);
    FwrCamera view = {*(FwPoint *)((uint8_t *)camera + 32), *(FwPoint *)((uint8_t *)camera + 56),
                      U32(camera, 0)};
    FwPoint base = {pose.position.x, pose.position.y, pose.position.z}, head;
    /* A 32px emote uses the same pixel scale as the follower, including its
     * draw-only depth compensation. The source bubble occupies 15x16 pixels. */
    int large = body->sy >= 12288;
    fwemote.billboard.sx = large ? pose.sx / 2 : pose.sx;
    fwemote.billboard.sy = large ? pose.sy / 2 : pose.sy;
    unsigned size = large ? 64 : 32, top = 0;
    if (fwfx.white && fwfx.whiteID == *(uint16_t *)(a->descriptor + 16) && fwfx.outlineSize == size &&
        body->face < fwfx.outlineFrames) {
        top = body->flags & 0x0800u ? size - 1 - fwfx.outlineBottom[body->face] : fwfx.outlineTop[body->face];
        if (top >= size) top = 0;
    }
    /* Eight source pixels of air above the opaque head. One pixel still cut
     * the raised body. The outline top stays the per-pose measurement. */
    enum { headClear = 8 };
    int32_t height = (int32_t)(size - top + headClear) * (large ? pose.sy / 4 : pose.sy / 2) -
                     (int32_t)fwemote.bottomInset[fwemote.frame & 1] * (fwemote.billboard.sy / 2);
    if (pose.sy <= 0 || !fwr_head_position(&base, &view, height, &head)) return;
    /* Stay in this 3D pass, after the map. A later text window still covers the
     * bubble. The eye-ray push clears overhanging foliage without moving pixels. */
    FwrPose bubble = {{head.x, head.y, head.z}, fwemote.billboard.sx, fwemote.billboard.sy}, placed;
    FwPoint playerPoint, *playerNear = 0;
    if (fwfx_player && !(fwfx_player->flags & 4u)) {
        playerPoint = (FwPoint){fwfx_player->world.x, fwfx_player->world.y, fwfx_player->world.z};
        playerNear = &playerPoint;
    }
    if (fwr_emote_place(&bubble, playerNear, &view, &placed)) {
        head = placed.position;
        fwemote.billboard.sx = (int16_t)placed.sx;
        fwemote.billboard.sy = (int16_t)placed.sy;
    }
    fwemote.billboard.pos = (Vec){head.x, head.y, head.z};
    CALL(0x0204ebdd, void (*)(void *, void *, void *))(&fwemote.scene, camera, light);
}
