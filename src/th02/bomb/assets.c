#include "th02/bomb/assets.h"

#include "pc98/zun/sprite_sheet.h"

#include <string.h>

static RESULT M_Bytes(
    VFS *const files, const char *const path, const size_t offset,
    uint8_t *const out, const size_t size)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_STREAM, &reader));
    Reader_Seek(&reader, offset);
    Reader_ReadBytes(&reader, out, size);
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    return result;
}

static RESULT M_Load(
    VFS *const files, const uint8_t shot_type, BOMB_ASSETS *const out)
{
    FAIL_IF(shot_type >= 3, "Invalid bomb shot type");
    uint8_t *particles;
    size_t particles_size;
    MUST(SpriteSheet_Load(
        files, "bombpart.bmp", 8, 8, &g_HeapAllocator, &particles,
        &particles_size));
    RESULT result = OK;
    if (particles_size != sizeof(out->particles)) {
        result = FAIL("Invalid bombpart.bmp size");
    } else {
        memcpy(out->particles, particles, sizeof(out->particles));
    }
    Mem_Free(particles);
    MUST(result);
    MUST(M_Bytes(
        files, "bombs.bft", 32, (uint8_t *)out->transition,
        sizeof(out->transition)));
    if (shot_type == 1) {
        MUST(M_Bytes(
            files, "bomb1.bft", 80, (uint8_t *)out->text, sizeof(out->text)));
    }
    static const char *const names[] = { "bomb1.pi", "bomb2.pi", "bomb3.pi" };
    READER reader;
    MUST(Vfs_Open(files, names[shot_type], VFS_STREAM, &reader));
    const RESULT pi_result =
        Pi_Load(&reader, out->allocator, &out->illustration);
    Reader_Close(&reader);
    MUST(pi_result, "%s", names[shot_type]);
    static const int32_t widths[] = { 224, 384, 384 };
    static const int32_t heights[] = { 216, 232, 368 };
    FAIL_IF(
        out->illustration.width != widths[shot_type]
            || out->illustration.height != heights[shot_type],
        "Invalid bomb illustration dimensions: %s", names[shot_type]);
    out->shot_type = shot_type;
    return OK;
}

RESULT BombAssets_Load(
    VFS *const files, const uint8_t shot_type, const ALLOCATOR *const allocator,
    BOMB_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    out->allocator = allocator;
    const RESULT result = M_Load(files, shot_type, out);
    if (!IS_OK(result)) {
        BombAssets_Free(out);
    }
    return result;
}

void BombAssets_Free(BOMB_ASSETS *const assets)
{
    if (assets->illustration.pixels != nullptr) {
        Allocator_Free(
            assets->allocator, assets->illustration.pixels,
            (size_t)assets->illustration.width * assets->illustration.height);
    }
    memset(assets, 0, sizeof(*assets));
}
