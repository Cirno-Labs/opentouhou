#include "th01/boss_b10j.h"

#include "core/base/assert.h"

static RESULT M_LoadBos(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Bos_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadBackground(
    VFS *const vfs, const ALLOCATOR *const a, PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3.GRP", VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadMissiles(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3_M.PTN", VFS_IN_MEMORY, &reader));
    const RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_ReadPillars(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, const B10J_PILLARS **const out)
{
    READER image;
    MUST(ReiidenImage_Open(reader, scratch, scratch_size, &image));
    B10J_PILLARS masks;
    Reader_Seek(&image, 0x3937d);
    Reader_ReadBytes(&image, masks.masks, sizeof(masks.masks));
    MUST(Reader_Result(&image));
    B10J_PILLARS *const allocated = Allocator_Alloc(a, sizeof(masks));
    *allocated = masks;
    *out = allocated;
    return OK;
}

static RESULT M_LoadPillars(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, const B10J_PILLARS **const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    const RESULT result = M_ReadPillars(&reader, scratch, scratch_size, a, out);
    Reader_Close(&reader);
    return result;
}

RESULT B10J_ValidateAssets(const B10J_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->still.pixels == nullptr
            || a->still.coverage == nullptr || a->still.width != 128
            || a->still.height != 160 || a->still.count != 1,
        "Invalid Mima still bank");
    FAIL_IF(
        a->animation.pixels == nullptr || a->animation.coverage == nullptr
            || a->animation.width != 128 || a->animation.height != 64
            || a->animation.count != 5,
        "Invalid Mima animation bank");
    FAIL_IF(
        a->background.width != 640 || a->background.height != 400
            || a->background.pixels == nullptr || !a->background.has_palette,
        "Invalid Mima background");
    MUST(Missiles_ValidateSprites(&a->missiles));
    FAIL_IF(
        a->next_scene.id != 4 || a->next_scene.layouts.stages == nullptr
            || a->pillars == nullptr,
        "Missing Mima scene or pillars");
    return OK;
}

RESULT B10J_LoadAssets(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B10J_ASSETS *const out)
{
    *out = (B10J_ASSETS) {};
    B10J_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS3_1.BOS", a, &loaded.still));
    MUST(M_LoadBos(vfs, "BOSS3_2.BOS", a, &loaded.animation));
    MUST(M_LoadBackground(vfs, a, &loaded.background));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Scene_Load(vfs, 4, a, &loaded.next_scene));
    MUST(M_LoadPillars(vfs, scratch, scratch_size, a, &loaded.pillars));
    MUST(B10J_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B10J_Init(B10J *const b, const ALLOCATOR *const a)
{
    *b = (B10J) { .background = Allocator_Alloc(a, 128 * 160),
                  .meteor_active = true,
                  .spread_interval = 4,
                  .spread_speed = 8 };
}

void B10J_BeginSetup(
    B10J *const b, const B10J_CONTEXT *const c, const FRAME_VSYNC *const vsync)
{
    ASSERT(!b->setup_active && !b->entrance_active && !b->defeating);
    ASSERT(b->background != nullptr);
    ASSERT(Result_Ignore(B10J_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->still, c->sprites, 0, &c->assets->still)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->animation, c->sprites, 1, &c->assets->animation)));
    Palette_Set(c->hit->palette, c->hit->display->video, &c->assets->palette);
    b->post_defeat = c->hit->palette->saved;
    b->boss_palette = c->hit->palette->saved;
    b->still.image = 0;
    b->animation.image = 1;
    Palette_BeginFade(&b->setup_fade, c->hit->palette, PALETTE_FADE_WHITE_IN);
    Frame_BeginWait(&b->setup_wait, vsync);
    b->setup_active = true;
    b->setup_waiting = false;
    b->finished = false;
}

bool B10J_ResumeSetup(
    B10J *const b, const B10J_CONTEXT *const c, const FRAME_VSYNC *const vsync,
    const uint64_t host_tick)
{
    if (!b->setup_active) {
        return true;
    }
    if (b->setup_waiting) {
        ASSERT(host_tick >= b->setup_tick);
        if (host_tick == b->setup_tick) {
            return false;
        }
        if (b->setup_fade.remaining != 0) {
            b->setup_waiting = false;
        }
    }
    if (b->setup_fade.remaining != 0) {
        if (!Frame_WaitReady(&b->setup_wait, vsync)) {
            return false;
        }
        Palette_FadeStep(
            &b->setup_fade, c->hit->palette, c->hit->display->video);
        b->setup_waiting = true;
        b->setup_tick = host_tick;
        Frame_BeginWait(&b->setup_wait, vsync);
        return false;
    }
    BossEntity_SetPosition(
        &b->still, 256, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&b->still, (BOSS_BOUNDS) { 16, 112, 16, 144 });
    b->still.orb_inactive = false;
    b->frame = 0;
    b->phase = 0;
    b->hp = 12;
    c->hit->health->first_white = 6;
    c->hit->health->first_redwhite = 2;
    Particles_Init(c->particles);
    Missiles_Reset(c->missiles);
    *c->stage_palette = c->hit->palette->saved;
    b->setup_active = false;
    return true;
}
