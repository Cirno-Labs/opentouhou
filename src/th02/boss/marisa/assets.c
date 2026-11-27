#include "th02/boss/marisa/assets.h"

#include <string.h>

static RESULT M_Append(
    VFS *const files, PATTERN_BANK *const bank, const char *const path,
    GFX_COLOR palette[16], bool *const has_palette)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
    BFNT sprites;
    const RESULT loaded = Bfnt_Load(&reader, &g_HeapAllocator, &sprites);
    Reader_Close(&reader);
    MUST(loaded, "%s", path);
    const RESULT registered = PatternBank_AppendBfnt(bank, &sprites);
    if (sprites.has_palette) {
        memcpy(palette, sprites.palette, sizeof(sprites.palette));
        *has_palette = true;
    }
    Mem_Free((void *)sprites.pixels);
    return registered;
}

RESULT MarisaAssets_Load(
    VFS *const files, const ALLOCATOR *const allocator,
    MARISA_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    PatternBank_Init(&out->summon, allocator);
    PatternBank_Init(&out->combat, allocator);
    out->summon.next = out->combat.next = 128;
    RESULT result = M_Append(
        files, &out->summon, "mima.bft", out->summon_palette,
        &out->has_summon_palette);
    if (IS_OK(result)) {
        result = M_Append(
            files, &out->combat, "stage3_b.bft", out->combat_palette,
            &out->has_combat_palette);
    }
    if (IS_OK(result)) {
        result = M_Append(
            files, &out->combat, "stage3_b.btt", out->combat_palette,
            &out->has_combat_palette);
    }
    if (!IS_OK(result)) {
        MarisaAssets_Free(out);
    }
    return result;
}

void MarisaAssets_Free(MARISA_ASSETS *const assets)
{
    PatternBank_Reset(&assets->summon);
    PatternBank_Reset(&assets->combat);
    memset(assets, 0, sizeof(*assets));
}
