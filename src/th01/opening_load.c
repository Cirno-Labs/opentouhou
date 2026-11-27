#include "th01/opening.h"
#include "th01/opening_strings.h"

#include "pc98/zun/pf.h"

#include <string.h>

static RESULT M_Image(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    RESULT result = Pi_Load(&reader, allocator, out);
    Reader_Close(&reader);
    if (IS_OK(result)
        && (out->width != VIDEO_WIDTH || out->height != VIDEO_HEIGHT)) {
        result = FAIL("%s must be a 640 by 400 image", name);
    }
    return result;
}

RESULT Opening_MountArchive(VFS *const vfs)
{
    const char *name = "東方靈異.伝";
    if (!Vfs_Exists(vfs, name)) {
        name = "\x93\x8c\x95\xfb\xe8\xcb\x88\xd9.\x93\x60";
    }
    FAIL_IF(!Vfs_Exists(vfs, name), "Missing TH01 data archive: 東方靈異.伝");
    READER file;
    MUST(Vfs_Open(vfs, name, VFS_STREAM, &file));
    VFS_SOURCE *source;
    MUST(Pf_Open(&file, PF_TH01, name, &g_HeapAllocator, &source));
    Vfs_MountSource(vfs, "", source, 1);
    return OK;
}

RESULT Opening_LoadAssets(
    VFS *const vfs, const FONT_ROM *const rom, const ALLOCATOR *const allocator,
    OPENING_ASSETS *const out)
{
    *out = (OPENING_ASSETS) {};
    MUST(M_Image(vfs, "REIIDEN2.grp", allocator, &out->title));
    MUST(M_Image(vfs, "REIIDEN3.grp", allocator, &out->revealed));
    MUST(M_Image(vfs, "op_win.grp", allocator, &out->window));
    char characters[8192] = "0123456789";
    const char *const *const groups[] = {
        g_OpeningMainMenuItems,  g_OpeningOptionMenuItems,
        g_OpeningMusicMenuItems, g_OpeningRanks,
        g_OpeningMusicModes,     g_OpeningLives,
        g_OpeningTrackTitles,
    };
    const int32_t counts[] = { 4, 5, 2, 4, 2, 5, 15 };
    strcat(characters, g_OpeningPrompt);
    for (int32_t group = 0; group < 7; group++) {
        for (int32_t i = 0; i < counts[group]; i++) {
            strcat(characters, groups[group][i]);
        }
    }
    return GameText_Load(rom, characters, allocator, &out->font);
}
