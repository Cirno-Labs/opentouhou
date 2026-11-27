#include "th02/background/assets.h"

#include <string.h>

static RESULT M_Load(
    VFS *const files, char *const path, BACKGROUND_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT map = Map_Load(&reader, &out->allocator, &out->map);
    Reader_Close(&reader);
    MUST(map, "%s", path);
    memcpy(path + 7, "mpn", 3);
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT tiles = Mpn_Load(&reader, &out->allocator, &out->tiles);
    Reader_Close(&reader);
    return tiles;
}

RESULT BackgroundAssets_Load(
    VFS *const files, const uint8_t stage, const ALLOCATOR *const allocator,
    BACKGROUND_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    FAIL_IF(stage > 5, "Invalid stage asset number");
    out->allocator = *allocator;
    char path[] = "stage0.map";
    path[5] += stage;
    const RESULT result = M_Load(files, path, out);
    if (!IS_OK(result)) {
        BackgroundAssets_Free(out);
    }
    return result;
}

void BackgroundAssets_Free(BACKGROUND_ASSETS *const assets)
{
    Allocator_Free(
        &assets->allocator, (void *)assets->map.sections,
        MAP_SECTION_COUNT * sizeof(MAP_SECTION));
    Allocator_Free(
        &assets->allocator, assets->tiles.pixels,
        (size_t)assets->tiles.count * MPN_TILE_WIDTH * MPN_TILE_HEIGHT);
    memset(assets, 0, sizeof(*assets));
}
