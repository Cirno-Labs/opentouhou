#include "th01/boss_b20m.h"

#include "core/base/assert.h"
#include "th01/sprite.h"

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
static RESULT M_LoadPi(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadGrc(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    GRC *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Grc_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadAudio(
    VFS *const vfs, const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "TENSI.MDT", VFS_IN_MEMORY, &reader));
    RESULT result = MdrvData_LoadMusic(&reader, a, &out->music);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "SYUGEN.MDT", VFS_IN_MEMORY, &reader));
    result = MdrvData_LoadMusic(&reader, a, &out->second_music);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "ZIGOKU.MDE", VFS_IN_MEMORY, &reader));
    result = MdrvData_LoadEffects(&reader, a, &out->effects);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadMasks(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    INTERLEAVE_ASSETS *interleave;
    RESULT result =
        Interleave_LoadAssets(&reader, scratch, scratch_size, a, &interleave);
    READER image;
    if (IS_OK(result)) {
        Reader_Seek(&reader, 0);
        result = ReiidenImage_Open(&reader, scratch, scratch_size, &image);
    }
    uint8_t masks[40];
    if (IS_OK(result)) {
        Reader_Seek(&image, 0x39252);
        Reader_ReadBytes(&image, masks, 16);
        Reader_Seek(&image, 0x3967d);
        Reader_ReadBytes(&image, masks + 16, 24);
        result = Reader_Result(&image);
    }
    Reader_Close(&reader);
    MUST(result);
    uint8_t *const storage = Allocator_Alloc(a, sizeof(masks));
    memcpy(storage, masks, sizeof(masks));
    out->star = storage;
    out->flake = storage + 8;
    out->spark = storage + 16;
    out->leaf_left = storage + 24;
    out->leaf_right = storage + 32;
    out->interleave = interleave;
    return OK;
}
RESULT B20M_ValidateAssets(const B20M_ASSETS *const a)
{
    FAIL_IF(a == nullptr, "Missing Sariel assets");
    const BOS *const banks[] = { &a->shield, &a->wand, &a->dress };
    const int32_t widths[] = { 48, 96, 96 };
    const int32_t counts[] = { 4, 2, 4 };
    for (int32_t i = 0; i < 3; i++) {
        FAIL_IF(
            banks[i]->pixels == nullptr || banks[i]->coverage == nullptr
                || banks[i]->width != widths[i] || banks[i]->height != widths[i]
                || banks[i]->count != counts[i],
            "Invalid Sariel sprite bank");
    }
    const GRC *const grcs[] = { &a->vortex, &a->bird, &a->cross, &a->splash };
    const int32_t image_counts[] = { 6, 8, 2, 3 };
    for (int32_t i = 0; i < 4; i++) {
        FAIL_IF(
            grcs[i]->coverage == nullptr || grcs[i]->width != 32
                || grcs[i]->height != 32 || grcs[i]->count != image_counts[i],
            "Invalid Sariel monochrome bank");
    }
    const PI_IMAGE *const images[] = { &a->lower,          &a->upper,
                                       &a->backgrounds[0], &a->backgrounds[1],
                                       &a->backgrounds[2], &a->backgrounds[3],
                                       &a->backgrounds[4], &a->backgrounds[5] };
    for (int32_t i = 0; i < 8; i++) {
        FAIL_IF(
            images[i]->pixels == nullptr || images[i]->width != 640
                || images[i]->height != 400 || !images[i]->has_palette,
            "Invalid Sariel background");
    }
    FAIL_IF(
        a->music.bytes == nullptr || a->second_music.bytes == nullptr
            || a->effects.bytes == nullptr || a->interleave == nullptr
            || a->star == nullptr || a->flake == nullptr || a->spark == nullptr
            || a->leaf_left == nullptr || a->leaf_right == nullptr,
        "Missing Sariel audio or shape masks");
    return OK;
}
RESULT B20M_LoadAssets(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    *out = (B20M_ASSETS) {};
    B20M_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS6_1.BOS", a, &loaded.shield));
    MUST(M_LoadBos(vfs, "BOSS6_2.BOS", a, &loaded.wand));
    MUST(M_LoadBos(vfs, "BOSS6_3.BOS", a, &loaded.dress));
    MUST(M_LoadGrc(vfs, "BOSS6GR1.GRC", a, &loaded.vortex));
    MUST(M_LoadGrc(vfs, "BOSS6GR2.GRC", a, &loaded.bird));
    MUST(M_LoadGrc(vfs, "BOSS6GR3.GRC", a, &loaded.cross));
    MUST(M_LoadGrc(vfs, "BOSS6GR4.GRC", a, &loaded.splash));
    MUST(M_LoadPi(vfs, "BOSS6_L.GRP", a, &loaded.lower));
    MUST(M_LoadPi(vfs, "BOSS6_H.GRP", a, &loaded.upper));
    const char *const names[] = { "BOSS6_A1.GRP", "BOSS6_A2.GRP",
                                  "BOSS6_A3.GRP", "BOSS6_A4.GRP",
                                  "BOSS6_A5.GRP", "BOSS6_A6.GRP" };
    for (int32_t i = 0; i < 6; i++) {
        MUST(M_LoadPi(vfs, names[i], a, &loaded.backgrounds[i]));
    }
    MUST(M_LoadAudio(vfs, a, &loaded));
    MUST(M_LoadMasks(vfs, scratch, scratch_size, a, &loaded));
    MUST(B20M_ValidateAssets(&loaded));
    *out = loaded;
    return OK;
}
void B20M_Init(B20M *const b, const ALLOCATOR *const a)
{
    *b = (B20M) { .ray_previous_x = 999,
                  .ray_previous_y = 999,
                  .flake_state = -1,
                  .wand_background = Allocator_AllocZ(a, 128 * 96) };
}
void B20M_Setup(B20M *const b, const B20M_CONTEXT *const c)
{
    ASSERT(b->sequence == 0 && b->wait_kind == 0 && !b->entrance_active);
    ASSERT(Result_Ignore(B20M_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->shield, c->sprites, 0, &c->assets->shield)));
    ASSERT(Result_Ignore(
        BossAnimation_Bind(&b->wand, c->sprites, 0, &c->assets->wand)));
    ASSERT(Result_Ignore(
        BossAnimation_Bind(&b->dress, c->sprites, 1, &c->assets->dress)));
    BossEntity_SetPosition(
        &b->shield, 304, 144, (BOSS_BOUNDS) { 0, 640, 0, 400 });
    BossEntity_SetOrbHitbox(&b->shield, (BOSS_BOUNDS) { -16, 64, -16, 64 });
    b->shield.orb_inactive = false;
    b->dress.left = 280;
    b->dress.top = 192;
    b->wand.left = 296;
    b->wand.top = 48;
    b->hp = 18;
    b->phase = 0;
    b->frame = 0;
    b->finished = false;
    b->boss_palette = c->hit->palette->saved;
    c->hit->health->first_white = 8;
    c->hit->health->first_redwhite = 2;
}
