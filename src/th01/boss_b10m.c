#include "th01/boss_b10m.h"

#include "core/base/assert.h"

static RESULT M_LoadBos(
    VFS *const vfs, const ALLOCATOR *const a, BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS2.BOS", VFS_IN_MEMORY, &reader));
    RESULT result = Bos_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadBackground(
    VFS *const vfs, const ALLOCATOR *const a, PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS2.GRP", VFS_IN_MEMORY, &reader));
    RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadMissiles(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3_M.PTN", VFS_IN_MEMORY, &reader));
    RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

RESULT B10M_ValidateAssets(const B10M_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->eyes.pixels == nullptr || a->eyes.coverage == nullptr
            || a->eyes.width != 64 || a->eyes.height != 48
            || a->eyes.count != 7,
        "Invalid YuugenMagan eye bank");
    FAIL_IF(
        a->background.width != 640 || a->background.height != 400
            || a->background.pixels == nullptr || !a->background.has_palette,
        "Invalid YuugenMagan background");
    MUST(Missiles_ValidateSprites(&a->missiles));
    FAIL_IF(
        a->next_scene.id != 3 || a->next_scene.layouts.stages == nullptr,
        "Missing YuugenMagan next scene");
    return OK;
}

RESULT B10M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, B10M_ASSETS *const out)
{
    *out = (B10M_ASSETS) {};
    B10M_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, a, &loaded.eyes));
    MUST(M_LoadBackground(vfs, a, &loaded.background));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Scene_Load(vfs, 3, a, &loaded.next_scene));
    MUST(B10M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B10M_Setup(B10M *const b, const B10M_CONTEXT *const c)
{
    ASSERT(!b->defeating);
    ASSERT(Result_Ignore(B10M_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->eyes[0], c->sprites, 0, &c->assets->eyes)));
    for (int32_t i = 1; i < 5; i++) {
        BossEntity_CopyMetadata(&b->eyes[i], &b->eyes[0]);
    }
    Palette_Set(c->hit->palette, c->hit->display->video, &c->assets->palette);
    b->boss_palette = c->hit->palette->saved;
    b->post_defeat = c->hit->palette->saved;
    *c->stage_palette = b->post_defeat;
    const int16_t left[] = { 64, 512, 192, 384, 288 };
    const int16_t top[] = { 128, 128, 176, 176, 64 };
    for (int32_t i = 0; i < 5; i++) {
        b->eyes[i].image = 0;
        BossEntity_SetPosition(
            &b->eyes[i], left[i], top[i], (BOSS_BOUNDS) { 0, 736, 64, 304 });
        BossEntity_SetOrbHitbox(&b->eyes[i], (BOSS_BOUNDS) { -4, 68, -4, 48 });
        b->eyes[i].orb_inactive = true;
    }
    b->frame = 0;
    b->phase = 0;
    b->hp = 16;
    b->finished = false;
    c->hit->health->first_white = 12;
    c->hit->health->first_redwhite = 8;
    Particles_Init(c->particles);
    Missiles_Reset(c->missiles);
}
