#include "th02/stage/backdrop.h"

#include "core/base/assert.h"

#include <string.h>

RESULT StageBackdrop_Load(
    VFS *const files, const ALLOCATOR *const allocator,
    STAGE_BACKDROP *const out)
{
    memset(out, 0, sizeof(*out));
    READER reader;
    MUST(Vfs_Open(files, "eye.pi", VFS_IN_MEMORY, &reader));
    out->allocator = *allocator;
    RESULT result = Pi_Load(&reader, allocator, &out->image);
    Reader_Close(&reader);
    if (IS_OK(result) && (out->image.width != 256 || out->image.height != 80)) {
        result = FAIL("EYE.PI must be 256 by 80 pixels");
    }
    if (!IS_OK(result)) {
        StageBackdrop_Free(out);
    }
    return result;
}

void StageBackdrop_Draw(const STAGE_BACKDROP *const backdrop)
{
    ASSERT(backdrop->image.pixels != nullptr && g_Grcg->mode == 0);
    Palette_Import(backdrop->image.palette);
    Palette_Show();
    Palette_Import(backdrop->image.palette);
    Palette_Show();
    Video_PutImage8(g_Clip, backdrop->image.pixels, 256, 80, 96, 144);
}

void StageBackdrop_Free(STAGE_BACKDROP *const backdrop)
{
    if (backdrop->image.pixels != nullptr) {
        Allocator_Free(
            &backdrop->allocator, backdrop->image.pixels,
            (size_t)backdrop->image.width * backdrop->image.height);
    }
    memset(backdrop, 0, sizeof(*backdrop));
}
