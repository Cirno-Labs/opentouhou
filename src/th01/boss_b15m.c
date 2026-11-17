#include "th01/boss_b15m.h"

#include "core/base/assert.h"

#include <string.h>

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
    MUST(Vfs_Open(vfs, "BOSS5.GRP", VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadGrc(
    VFS *const vfs, const ALLOCATOR *const a, GRC *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS5_GR.GRC", VFS_IN_MEMORY, &reader));
    const RESULT result = Grc_Load(&reader, a, out);
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

static RESULT M_LoadStar(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, const uint8_t **const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    READER image;
    RESULT result = ReiidenImage_Open(&reader, scratch, scratch_size, &image);
    uint8_t mask[8];
    if (IS_OK(result)) {
        Reader_Seek(&image, 0x39252);
        Reader_ReadBytes(&image, mask, sizeof(mask));
        result = Reader_Result(&image);
    }
    Reader_Close(&reader);
    MUST(result);
    uint8_t *const allocated = Allocator_Alloc(a, sizeof(mask));
    memcpy(allocated, mask, sizeof(mask));
    *out = allocated;
    return OK;
}

RESULT B15M_ValidateAssets(const B15M_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->girl.pixels == nullptr || a->girl.coverage == nullptr
            || a->girl.width != 128 || a->girl.height != 96
            || a->girl.count != 6 || a->attack.pixels == nullptr
            || a->attack.coverage == nullptr || a->attack.width != 128
            || a->attack.height != 96 || a->attack.count != 3
            || a->bat.pixels == nullptr || a->bat.coverage == nullptr
            || a->bat.width != 48 || a->bat.height != 32 || a->bat.count != 3,
        "Invalid Elis sprite banks");
    FAIL_IF(
        a->spheres.coverage == nullptr || a->spheres.width != 32
            || a->spheres.height != 32 || a->spheres.count != 6
            || a->star == nullptr || a->background.pixels == nullptr
            || a->background.width != 640 || a->background.height != 400
            || !a->background.has_palette,
        "Invalid Elis effects or background");
    MUST(Missiles_ValidateSprites(&a->missiles));
    FAIL_IF(
        a->next_scene.id != 5 || a->next_scene.layouts.stages == nullptr,
        "Missing Elis next scene");
    return OK;
}

RESULT B15M_LoadAssets(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B15M_ASSETS *const out)
{
    *out = (B15M_ASSETS) {};
    B15M_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS5.BOS", a, &loaded.girl));
    MUST(M_LoadBos(vfs, "BOSS5_2.BOS", a, &loaded.attack));
    MUST(M_LoadBos(vfs, "BOSS5_3.BOS", a, &loaded.bat));
    MUST(M_LoadGrc(vfs, a, &loaded.spheres));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(M_LoadBackground(vfs, a, &loaded.background));
    MUST(Scene_Load(vfs, 5, a, &loaded.next_scene));
    MUST(M_LoadStar(vfs, scratch, scratch_size, a, &loaded.star));
    MUST(B15M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B15M_Init(B15M *const b, const ALLOCATOR *const a)
{
    *b = (B15M) { .background = Allocator_Alloc(a, 128 * 96) };
}

void B15M_Setup(B15M *const b, const B15M_CONTEXT *const c)
{
    ASSERT(!b->entrance_active && !b->defeating && b->pending == 0);
    ASSERT(b->background != nullptr);
    ASSERT(Result_Ignore(B15M_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->girl, c->sprites, 0, &c->assets->girl)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->attack, c->sprites, 1, &c->assets->attack)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->bat, c->sprites, 2, &c->assets->bat)));
    const BOSS_BOUNDS bounds = { 0, 736, 64, 304 };
    BossEntity_SetPosition(&b->girl, 256, 96, bounds);
    BossEntity_SetPosition(&b->attack, 256, 96, bounds);
    BossEntity_SetPosition(&b->bat, 256, 96, bounds);
    BossEntity_SetOrbHitbox(&b->girl, (BOSS_BOUNDS) { 16, 112, -4, 80 });
    BossEntity_SetOrbHitbox(&b->bat, (BOSS_BOUNDS) { -8, 56, -8, 40 });
    b->boss_palette = c->hit->palette->saved;
    b->post_defeat = (PALETTE) {};
    b->phase = 0;
    b->frame = 0;
    b->hp = 14;
    b->finished = false;
    c->hit->health->first_white = 10;
    c->hit->health->first_redwhite = 6;
    *c->rng = c->hit->frame_rand;
    c->pellets->interlace = true;
    Particles_Init(c->particles);
}
