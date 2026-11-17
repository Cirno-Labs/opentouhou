#include "th01/boss_b05.h"

#include "core/base/assert.h"

static RESULT M_LoadBos(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    RESULT result = Bos_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadBackground(
    VFS *const vfs, const ALLOCATOR *const allocator, PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS1.GRP", VFS_IN_MEMORY, &reader));
    RESULT result = Pi_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

RESULT B05_ValidateAssets(const B05_ASSETS *const assets)
{
    const BOS *const banks[] = { &assets->sphere, &assets->flash,
                                 &assets->person };
    const int32_t minimum[] = { 8, 3, 5 };
    for (int32_t i = 0; i < 3; i++) {
        FAIL_IF(
            banks[i]->pixels == nullptr || banks[i]->coverage == nullptr
                || banks[i]->width != 96 || banks[i]->height != 96
                || banks[i]->count < minimum[i] || banks[i]->count > 8,
            "Invalid SinGyoku sprite bank");
    }
    FAIL_IF(
        assets->background.width != VIDEO_WIDTH
            || assets->background.height != VIDEO_HEIGHT
            || assets->background.pixels == nullptr
            || !assets->background.has_palette,
        "Invalid SinGyoku background");
    return OK;
}

RESULT B05_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const allocator, B05_ASSETS *const out)
{
    *out = (B05_ASSETS) {};
    B05_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS1.BOS", allocator, &loaded.sphere));
    MUST(M_LoadBos(vfs, "BOSS1_2.BOS", allocator, &loaded.flash));
    MUST(M_LoadBos(vfs, "BOSS1_3.BOS", allocator, &loaded.person));
    MUST(M_LoadBackground(vfs, allocator, &loaded.background));
    MUST(B05_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B05_Setup(B05 *const boss, const B05_CONTEXT *const c)
{
    ASSERT(boss->pending == B05_IDLE || boss->pending == B05_FINISHED);
    ASSERT(Result_Ignore(B05_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&boss->sphere, c->sprites, 0, &c->assets->sphere)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&boss->flash, c->sprites, 1, &c->assets->flash)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&boss->person, c->sprites, 2, &c->assets->person)));
    boss->post_defeat = c->assets->palette;
    *c->stage_palette = boss->post_defeat;
    Palette_Set(c->hit->palette, c->hit->display->video, &boss->post_defeat);
    boss->boss_palette = c->hit->palette->saved;
    Palette_Set(
        c->hit->palette, c->hit->display->video, &c->hit->palette->saved);
    BossEntity_SetPosition(
        &boss->sphere, 640, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&boss->sphere, (BOSS_BOUNDS) { 8, 88, 8, 88 });
    boss->sphere.orb_inactive = false;
    boss->sphere.image = 0;
    boss->hp = 8;
    c->hit->health->first_white = 6;
    c->hit->health->first_redwhite = 2;
    boss->phase = 0;
    boss->pending = B05_IDLE;
    Particles_Init(c->particles);
}
