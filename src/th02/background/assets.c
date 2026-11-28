#include "th02/background/assets.h"

#include <string.h>

static RESULT M_Load(
    VFS *const files, char *const path, BACKGROUND_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT map =
        BackgroundAssets_LoadMap(&reader, &out->allocator, &out->map);
    Reader_Close(&reader);
    MUST(map, "%s", path);
    memcpy(path + 7, "mpn", 3);
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT tiles = Mpn_Load(&reader, &out->allocator, &out->tiles);
    Reader_Close(&reader);
    return tiles;
}

RESULT BackgroundAssets_LoadMap(
    READER *const reader, const ALLOCATOR *const allocator, MAP *const out)
{
    *out = (MAP) {};
    MUST(Reader_Result(reader));
    FAIL_IF(
        Reader_Remaining(reader)
            < 10 + MAP_SECTION_COUNT * MAP_SECTION_ROWS * MAP_COLUMNS,
        "Truncated map data");
    Reader_Skip(reader, 10);
    MAP_SECTION sections[MAP_SECTION_COUNT];
    for (size_t section = 0; section < MAP_SECTION_COUNT; section++) {
        for (size_t row = 0; row < MAP_SECTION_ROWS; row++) {
            Reader_ReadBytes(reader, sections[section].tiles[row], MAP_COLUMNS);
        }
    }
    MUST(Reader_Result(reader));
    MAP_SECTION *const loaded = Allocator_Alloc(allocator, sizeof(sections));
    memcpy(loaded, sections, sizeof(sections));
    out->sections = loaded;
    return OK;
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
