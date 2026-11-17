#include "th01/boss_b20j.h"

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
static RESULT M_LoadBody(
    VFS *const vfs, const ALLOCATOR *const a, GRZ *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS8.GRZ", VFS_IN_MEMORY, &reader));
    const RESULT result = Grz_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadAudio(
    VFS *const vfs, const ALLOCATOR *const a, B20J_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "ALICE.MDT", VFS_IN_MEMORY, &reader));
    RESULT result = MdrvData_LoadMusic(&reader, a, &out->music);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "ZIGOKU.MDE", VFS_IN_MEMORY, &reader));
    result = MdrvData_LoadEffects(&reader, a, &out->effects);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadMasks(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20J_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    READER image;
    RESULT result = ReiidenImage_Open(&reader, scratch, scratch_size, &image);
    uint8_t masks[24];
    if (IS_OK(result)) {
        Reader_Seek(&image, 0x3924a);
        Reader_ReadBytes(&image, masks, sizeof(masks));
        result = Reader_Result(&image);
    }
    Reader_Close(&reader);
    MUST(result);
    uint8_t *const allocated = Allocator_Alloc(a, 16);
    memcpy(allocated, masks, 8);
    memcpy(allocated + 8, masks + 16, 8);
    out->diamond = allocated;
    out->flake = allocated + 8;
    return OK;
}
RESULT B20J_ValidateAssets(const B20J_ASSETS *const a)
{
    FAIL_IF(a == nullptr, "Missing Konngara assets");
    const BOS *const banks[] = { &a->head, &a->face, &a->aim };
    const int32_t counts[] = { 3, 6, 3 };
    const int32_t heights[] = { 80, 32, 32 };
    for (int32_t i = 0; i < 3; i++) {
        FAIL_IF(
            banks[i]->pixels == nullptr || banks[i]->coverage == nullptr
                || banks[i]->count != counts[i] || banks[i]->width != 80
                || banks[i]->height != heights[i],
            "Invalid Konngara sprite bank");
    }
    FAIL_IF(a->body.count != 16, "Missing Konngara body images");
    for (int32_t i = 0; i < 16; i++) {
        const GRX *const image = &a->body.images[i];
        FAIL_IF(
            image->stream_count < 1 || image->pixels == nullptr
                || image->coverage == nullptr,
            "Invalid Konngara body image");
    }
    const PI_IMAGE *const images[] = { &a->scroll,    &a->upper,
                                       &a->layers[0], &a->layers[1],
                                       &a->layers[2], &a->layers[3] };
    for (int32_t i = 0; i < 6; i++) {
        FAIL_IF(
            images[i]->pixels == nullptr || images[i]->width <= 0
                || images[i]->height <= 0 || !images[i]->has_palette,
            "Invalid Konngara background");
    }
    FAIL_IF(
        a->music.bytes == nullptr || a->effects.bytes == nullptr
            || a->diamond == nullptr || a->flake == nullptr,
        "Missing Konngara audio or shape masks");
    return OK;
}
RESULT B20J_LoadAssets(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20J_ASSETS *const out)
{
    *out = (B20J_ASSETS) {};
    B20J_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS8_1.BOS", a, &loaded.head));
    MUST(M_LoadBos(vfs, "BOSS8_E1.BOS", a, &loaded.face));
    MUST(M_LoadBos(vfs, "BOSS8_E2.BOS", a, &loaded.aim));
    MUST(M_LoadBody(vfs, a, &loaded.body));
    MUST(M_LoadPi(vfs, "BOSS7_D1.GRP", a, &loaded.scroll));
    MUST(M_LoadPi(vfs, "BOSS8_A1.GRP", a, &loaded.upper));
    const char *const names[] = { "BOSS8_D1.GRP", "BOSS8_D2.GRP",
                                  "BOSS8_D3.GRP", "BOSS8_D4.GRP" };
    for (int32_t i = 0; i < 4; i++) {
        MUST(M_LoadPi(vfs, names[i], a, &loaded.layers[i]));
    }
    MUST(M_LoadAudio(vfs, a, &loaded));
    MUST(M_LoadMasks(vfs, scratch, scratch_size, a, &loaded));
    MUST(B20J_ValidateAssets(&loaded));
    *out = loaded;
    return OK;
}
void B20J_Init(B20J *const b)
{
    *b = (B20J) { .ray_previous_x = 999, .ray_previous_y = 999 };
}
void B20J_Setup(B20J *const b, const B20J_CONTEXT *const c)
{
    ASSERT(!b->entrance_active && !b->defeating && b->wait_kind == 0);
    ASSERT(Result_Ignore(B20J_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->head, c->sprites, 0, &c->assets->head)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->face, c->sprites, 1, &c->assets->face)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->aim, c->sprites, 2, &c->assets->aim)));
    b->boss_palette = c->hit->palette->saved;
    b->hp = 18;
    b->phase = 0;
    b->frame = 0;
    b->direction_can_change = true;
    b->face_expression = 0;
    b->face_direction = 2;
    b->finished = false;
    c->hit->health->first_white = 16;
    c->hit->health->first_redwhite = 10;
}
