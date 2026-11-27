#include "th02/boss/sigma/assets.h"

#include <string.h>

static RESULT M_Append(
    VFS *const files, SIGMA_ASSETS *const out, const char *const path)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader), "%s", path);
    BFNT sprites;
    const RESULT loaded = Bfnt_Load(&reader, &g_HeapAllocator, &sprites);
    Reader_Close(&reader);
    MUST(loaded, "%s", path);
    const RESULT registered = PatternBank_AppendBfnt(&out->patterns, &sprites);
    if (sprites.has_palette) {
        memcpy(out->palette, sprites.palette, sizeof(sprites.palette));
        out->has_palette = true;
    }
    Mem_Free((void *)sprites.pixels);
    return registered;
}

RESULT SigmaAssets_Load(
    VFS *const files, const ALLOCATOR *const allocator, SIGMA_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    PatternBank_Init(&out->patterns, allocator);
    out->patterns.next = 128;
    RESULT result = M_Append(files, out, "stage5b1.bft");
    if (IS_OK(result)) {
        result = M_Append(files, out, "stage5b2.bft");
    }
    if (!IS_OK(result)) {
        SigmaAssets_Free(out);
    }
    return result;
}

void SigmaAssets_Free(SIGMA_ASSETS *const assets)
{
    PatternBank_Reset(&assets->patterns);
    memset(assets, 0, sizeof(*assets));
}
