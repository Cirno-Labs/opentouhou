#include "th02/ending/images.h"
#include "th02/ending/render.h"
#include "th02/ending/text.h"

#include <string.h>

static RESULT M_Bfnt(VFS *const files, const char *const path, BFNT *const out)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_STREAM, &reader));
    const RESULT result = Bfnt_Load(&reader, &g_HeapAllocator, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_Title(
    VFS *const files, const ALLOCATOR *const allocator,
    ENDING_ASSETS *const out)
{
    BFNT title;
    MUST(M_Bfnt(files, "endft.bft", &title));
    RESULT result = OK;
    if (title.width != 16 || title.height != 32 || title.count != 60
        || title.bits_per_pixel != 4) {
        result = FAIL("endft.bft must contain sixty 16 by 32 sprites");
    } else {
        PatternBank_Init(&out->title, allocator);
        result = PatternBank_AppendBfnt(&out->title, &title);
        memcpy(out->title_palette, title.palette, sizeof(out->title_palette));
    }
    Mem_Free((void *)title.pixels);
    return result;
}

static RESULT M_Gaiji(VFS *const files, GAIJI *const out)
{
    BFNT font;
    MUST(M_Bfnt(files, "mikoft.bft", &font));
    const RESULT result = Gaiji_Register(out, &font);
    Mem_Free((void *)font.pixels);
    return result;
}

static RESULT M_Text(
    VFS *const files, const char *const path, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, ENDING_TEXT_SCRIPT *const story,
    ENDING_TEXT_SCRIPT *const verdict)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_STREAM, &reader));
    RESULT result = EndingText_Load(&reader, font, allocator, story);
    if (IS_OK(result)) {
        Reader_Seek(&reader, 0);
        READER titles;
        result = Vfs_Open(files, "end3.txt", VFS_STREAM, &titles);
        if (IS_OK(result)) {
            result = EndingText_LoadVerdict(
                &reader, &titles, font, allocator, verdict);
            Reader_Close(&titles);
        }
    }
    Reader_Close(&reader);
    return result;
}

static RESULT M_Load(
    VFS *const files, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, ENDING_ASSETS *const out)
{
    MUST(EndingText_LoadLabels(font, allocator, &out->labels));
    MUST(EndingRender_LoadRevealMasks(files, &out->masks));
    static const char *const paths[ENDING_STORY_COUNT] = { "end1.txt",
                                                           "end2.txt" };
    for (size_t i = 0; i < ENDING_STORY_COUNT; i++) {
        MUST(M_Text(
            files, paths[i], font, allocator, &out->story[i],
            &out->verdict[i]));
    }
    MUST(M_Title(files, allocator, out));
    MUST(M_Gaiji(files, &out->gaiji));
    return EndingImages_Load(files, allocator, &out->images);
}

RESULT EndingAssets_Load(
    VFS *const files, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, ENDING_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    const RESULT result = M_Load(files, font, allocator, out);
    if (!IS_OK(result)) {
        memset(out, 0, sizeof(*out));
    }
    return result;
}
