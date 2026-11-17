#include "th01/boss_b15j.h"

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
    MUST(Vfs_Open(vfs, "BOSS4.GRP", VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadRipples(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "TAMAYEN.PTN", VFS_IN_MEMORY, &reader));
    const RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

RESULT B15J_ValidateAssets(const B15J_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->souls.pixels == nullptr
            || a->souls.coverage == nullptr || a->souls.width != 32
            || a->souls.height != 32 || a->souls.count != 3
            || a->tears.pixels == nullptr || a->tears.coverage == nullptr
            || a->tears.width != 16 || a->tears.height != 16
            || a->tears.count != 3,
        "Invalid Kikuri sprite banks");
    FAIL_IF(
        a->ripples.pixels == nullptr || a->ripples.count != 1
            || a->background.width != 640 || a->background.height != 400
            || a->background.pixels == nullptr || !a->background.has_palette,
        "Invalid Kikuri background or ripples");
    FAIL_IF(
        a->next_scene.id != 6 || a->next_scene.layouts.stages == nullptr,
        "Missing Kikuri next scene");
    return OK;
}

RESULT B15J_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, B15J_ASSETS *const out)
{
    *out = (B15J_ASSETS) {};
    B15J_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "TAMASII.BOS", a, &loaded.souls));
    MUST(M_LoadBos(vfs, "TAMASII2.BOS", a, &loaded.tears));
    MUST(M_LoadRipples(vfs, a, &loaded.ripples));
    MUST(M_LoadBackground(vfs, a, &loaded.background));
    MUST(Scene_Load(vfs, 6, a, &loaded.next_scene));
    MUST(B15J_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B15J_Setup(B15J *const b, const B15J_CONTEXT *const c)
{
    ASSERT(!b->entrance_active && !b->defeating);
    ASSERT(Result_Ignore(B15J_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->souls[0], c->sprites, 0, &c->assets->souls)));
    BossEntity_CopyMetadata(&b->souls[1], &b->souls[0]);
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->tears[0], c->sprites, 1, &c->assets->tears)));
    for (int32_t i = 1; i < 10; i++) {
        BossEntity_CopyMetadata(&b->tears[i], &b->tears[0]);
    }
    b->boss_palette = c->hit->palette->saved;
    for (int32_t i = 0; i < 10; i++) {
        b->tear_frame[i] = 0;
    }
    c->pellets->interlace = true;
    b->phase = 0;
    b->frame = 0;
    b->hp = 14;
    b->finished = false;
    c->hit->health->first_white = 10;
    c->hit->health->first_redwhite = 6;
    for (int32_t i = 0; i < 2; i++) {
        BossEntity_SetPosition(
            &b->souls[i], 0, 0, (BOSS_BOUNDS) { 32, 576, 64, 400 });
    }
    b->post_defeat = (PALETTE) {};
    Particles_Init(c->particles);
}
