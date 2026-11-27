#include "th02/boss/mima/assets.h"

#include <string.h>

typedef struct {
    MIMA_BANK bank;
    const char *path;
} M_FILE;

static const M_FILE m_Files[] = {
    { MIMA_BANK_SUMMON, "mima.bft" },
    { MIMA_BANK_FIRST, "mima1.bft" },
    { MIMA_BANK_FIRST, "stage3_b.btt" },
    { MIMA_BANK_SECOND, "mima2.bft" },
};

static RESULT M_Append(
    VFS *const files, MIMA_ASSETS *const out, const M_FILE *const file)
{
    READER reader;
    MUST(Vfs_Open(files, file->path, VFS_IN_MEMORY, &reader), "%s", file->path);
    BFNT sprites;
    const RESULT loaded = Bfnt_Load(&reader, &g_HeapAllocator, &sprites);
    Reader_Close(&reader);
    MUST(loaded, "%s", file->path);
    const RESULT registered =
        PatternBank_AppendBfnt(&out->patterns[file->bank], &sprites);
    if (sprites.has_palette) {
        memcpy(
            out->palettes[file->bank], sprites.palette,
            sizeof(sprites.palette));
        out->has_palette[file->bank] = true;
    }
    Mem_Free((void *)sprites.pixels);
    return registered;
}

RESULT MimaAssets_Load(
    VFS *const files, const ALLOCATOR *const allocator, MIMA_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < MIMA_BANK_COUNT; i++) {
        PatternBank_Init(&out->patterns[i], allocator);
        out->patterns[i].next = 128;
    }
    RESULT result = OK;
    for (size_t i = 0; i < sizeof(m_Files) / sizeof(*m_Files) && IS_OK(result);
         i++) {
        result = M_Append(files, out, &m_Files[i]);
    }
    if (!IS_OK(result)) {
        MimaAssets_Free(out);
    }
    return result;
}

void MimaAssets_Free(MIMA_ASSETS *const assets)
{
    for (int i = 0; i < MIMA_BANK_COUNT; i++) {
        PatternBank_Reset(&assets->patterns[i]);
    }
    memset(assets, 0, sizeof(*assets));
}
