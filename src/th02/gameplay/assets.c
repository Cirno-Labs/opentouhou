#include "th02/gameplay/assets.h"

#include <string.h>

static RESULT M_Bfnt(VFS *const files, const char *const path, BFNT *const out)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader));
    const RESULT result = Bfnt_Load(&reader, &g_HeapAllocator, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_RegisterSheet(
    GAMEPLAY_ASSETS *const assets, const BFNT *const sprites,
    const int32_t width, const int32_t height, const int32_t count)
{
    FAIL_IF(
        sprites->width != width || sprites->height != height
            || sprites->count != count || sprites->bits_per_pixel != 4,
        "Invalid gameplay sprite sheet dimensions");
    MUST(PatternBank_AppendBfnt(&assets->patterns, sprites));
    if (sprites->has_palette) {
        memcpy(
            assets->sprite_palette, sprites->palette,
            sizeof(assets->sprite_palette));
        assets->has_sprite_palette = true;
    }
    return OK;
}

static RESULT M_Load(VFS *const files, GAMEPLAY_ASSETS *const assets)
{
    MUST(PointNumberFont_Load(files, &assets->numbers));
    MUST(SparkImages_Load(files, &assets->sparks));
    MUST(BulletImages_Load(files, &assets->bullets));
    MUST(StageBonusAssets_Load(&assets->patterns.allocator, &assets->bonus));
    MUST(DialogueScripts_Load(&assets->bosses.scripts));
    MUST(PauseMenuAssets_Load(&assets->patterns.allocator, &assets->pause));
    MUST(StageTitles_Load(&assets->patterns.allocator, &assets->titles));
    MUST(StageBackdrop_Load(
        files, &assets->patterns.allocator, &assets->backdrop));
    static const struct {
        const char *path;
        int32_t width;
        int32_t height;
        int32_t count;
    } sheets[] = {
        { "miko.bft", 32, 48, 10 },
        { "miko32.bft", 32, 32, 38 },
        { "miko16.bft", 16, 16, 80 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(sheets); i++) {
        BFNT sprites;
        MUST(M_Bfnt(files, sheets[i].path, &sprites), "%s", sheets[i].path);
        const RESULT result = M_RegisterSheet(
            assets, &sprites, sheets[i].width, sheets[i].height,
            sheets[i].count);
        Mem_Free((void *)sprites.pixels);
        MUST(result, "%s", sheets[i].path);
    }
    for (uint16_t slot = 48; slot < 128; slot++) {
        MUST(PatternBank_ConvertTiny(&assets->patterns, slot));
    }
    BFNT font;
    MUST(M_Bfnt(files, "mikoft.bft", &font));
    const RESULT result = Gaiji_Register(&assets->gaiji, &font);
    Mem_Free((void *)font.pixels);
    return result;
}

RESULT GameplayAssets_Load(
    VFS *const files, const ALLOCATOR *const allocator,
    GAMEPLAY_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    PatternBank_Init(&out->patterns, allocator);
    const RESULT result = M_Load(files, out);
    if (!IS_OK(result)) {
        GameplayAssets_Free(out);
    }
    return result;
}

void GameplayAssets_Free(GAMEPLAY_ASSETS *const assets)
{
    for (size_t i = 0; i < ARRAY_SIZE(assets->bosses.stages); i++) {
        DialogueAssets_Free(&assets->bosses.stages[i]);
    }
    MarisaAssets_Free(&assets->bosses.marisa);
    MimaAssets_Free(&assets->bosses.mima);
    SigmaAssets_Free(&assets->bosses.sigma);
    PauseMenuAssets_Free(&assets->pause);
    StageTitles_Free(&assets->titles);
    StageBonusAssets_Free(&assets->bonus);
    StageBackdrop_Free(&assets->backdrop);
    PatternBank_Reset(&assets->patterns);
    memset(assets, 0, sizeof(*assets));
}
