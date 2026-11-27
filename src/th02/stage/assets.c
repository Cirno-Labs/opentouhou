#include "th02/stage/assets.h"

#include <string.h>

static RESULT M_Load(
    VFS *const files, PATTERN_BANK *const patterns, STAGE_ASSETS *const out)
{
    char path[] = "stage0.bft";
    path[5] += out->stage;
    static const char *const extensions[] = { "bft", "bmt", "bbt" };
    for (size_t i = 0; i < ARRAY_SIZE(extensions); i++) {
        memcpy(path + 7, extensions[i], 3);
        READER reader;
        MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
        BFNT sprites;
        const RESULT loaded = Bfnt_Load(&reader, &g_HeapAllocator, &sprites);
        Reader_Close(&reader);
        MUST(loaded, "%s", path);
        const RESULT registered = PatternBank_AppendBfnt(patterns, &sprites);
        if (sprites.has_palette) {
            memcpy(
                out->sprite_palette, sprites.palette, sizeof(sprites.palette));
            out->has_sprite_palette = true;
        }
        Mem_Free((void *)sprites.pixels);
        MUST(registered, "%s", path);
    }
    const RESULT background = BackgroundAssets_Load(
        files, out->stage, &out->allocator, &out->background);
    MUST(background, "%s", path);
    READER reader;
    memcpy(path + 7, "dt1", 3);
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT data = StageData_Load(&reader, &out->allocator, &out->data);
    Reader_Close(&reader);
    return data;
}

static RESULT M_Register(
    PATTERN_BANK *const patterns, PATTERN_BANK *const loaded,
    STAGE_ASSETS *const out)
{
    FAIL_IF(patterns->next < 128, "Missing startup patterns");
    for (uint16_t slot = 0; slot < 128; slot++) {
        FAIL_IF(patterns->entries[slot].size == 0, "Missing startup pattern");
    }
    const uint16_t next = patterns->next <= 193 ? 128 : patterns->next;
    FAIL_IF(
        loaded->next > PATTERN_LIMIT - next,
        "Stage patterns exceed bank capacity");
    for (uint16_t slot = 193; slot-- > 128;) {
        if (patterns->entries[slot].size != 0) {
            Result_Ignore(PatternBank_Cancel(patterns, slot));
        }
    }
    out->pattern_first = patterns->next;
    out->pattern_count = loaded->next;
    for (uint16_t slot = 0; slot < loaded->next; slot++) {
        PATTERN *const target = &patterns->entries[patterns->next++];
        if (target->data != nullptr) {
            Allocator_Free(
                &patterns->allocator, target->data, target->storage_size);
        }
        *target = loaded->entries[slot];
        memset(&loaded->entries[slot], 0, sizeof(loaded->entries[slot]));
    }
    loaded->next = 0;
    return OK;
}

RESULT StageAssets_Load(
    VFS *const files, const uint8_t stage, const ALLOCATOR *const allocator,
    PATTERN_BANK *const patterns, STAGE_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    FAIL_IF(stage > 5, "Invalid stage asset number");
    out->allocator = *allocator;
    out->stage = stage;
    PATTERN_BANK *const loaded = Mem_Alloc(sizeof(*loaded));
    PatternBank_Init(loaded, &patterns->allocator);
    RESULT result = M_Load(files, loaded, out);
    if (IS_OK(result)) {
        result = M_Register(patterns, loaded, out);
    }
    PatternBank_Reset(loaded);
    Mem_Free(loaded);
    if (!IS_OK(result)) {
        StageAssets_Free(out);
    }
    return result;
}

void StageAssets_Free(STAGE_ASSETS *const assets)
{
    BackgroundAssets_Free(&assets->background);
    Allocator_Free(
        &assets->allocator, (void *)assets->data.map, assets->data.map_count);
    Allocator_Free(
        &assets->allocator, (void *)assets->data.formations,
        assets->data.formation_count * sizeof(STAGE_FORMATION));
    Allocator_Free(
        &assets->allocator, (void *)assets->data.scripts,
        assets->data.script_count * sizeof(STAGE_SCRIPT));
    Allocator_Free(
        &assets->allocator, (void *)assets->data.lines,
        assets->data.line_count * sizeof(STAGE_LINE));
    memset(assets, 0, sizeof(*assets));
}
