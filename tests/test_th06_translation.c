#include "th06/translation.h"

#include "th06/files.h"
#include "th06/strings.h"

#include "core/base/memory.h"
#include "core/patch.h"

#include "patch_builder.h"
#include "test.h"

#include <string.h>

static PATCH_STACK *m_Stack;
static char *m_Directory;

// Writes a patch folder with the files of a lang patch and a base patch
// below it, and loads its translations.
static void M_Begin(
    const char *const name, const char *const *const files, const int32_t count)
{
    m_Directory = TestDir_Prepare(name);
    PatchBuilder_AddPatch(m_Directory, "nmlgc/base_tsa", "{}");
    PatchBuilder_AddPatch(
        m_Directory, "thpatch/lang", "{dependencies: ['nmlgc/base_tsa']}");
    for (int32_t i = 0; i < count; i += 2) {
        PatchBuilder_Write(m_Directory, files[i], files[i + 1]);
    }
    m_Stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(
            m_Directory, "th06", &g_PatchGame, nullptr, 0, &m_Stack),
        LOG_LEVEL_ERROR));
    Translation_Init(m_Stack);
}

static void M_End(void)
{
    Translation_Shutdown();
    Patch_CloseStack(m_Stack);
    Mem_Free(m_Directory);
}

TEST(exe_strings_take_their_translation_by_address)
{
    const char *const files[] = {
        "nmlgc/base_tsa/th06/stringlocs.v1.02h.js",
        ("{Rx6a98c: 'th06 BGM In-game format', "
         "Rx6a3f4: 'th06 Bomb Reimu A', "
         "Rx6c4c8: 'th06_practice_format'}"),
        "thpatch/lang/stringdefs.js",
        ("{'th06 BGM In-game format': 'BGM: %s', "
         "'th06 Bomb Reimu A': 'Spirit Sign <b$\"Fantasy Seal\"$>', "
         "th06_practice_format: 'STAGE %s'}"),
    };
    M_Begin("exe_strings", files, 4);
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "BGM: %s");
    CHECK_EQ_STR(
        g_Strings[STRING_BOMB_NAME_REIMU_A], "Spirit Sign \"Fantasy Seal\"");
    CHECK_EQ_STR(g_Strings[STRING_MENU_PRACTICE_SCORE], "STAGE %d  %.9d");
    CHECK_EQ_STR(g_Strings[STRING_MENU_STAGE_1], "Stage1");
    M_End();
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "♪%s");
    CHECK_EQ_STR(g_Strings[STRING_BOMB_NAME_REIMU_A], "霊符「夢想封印」");
}

TEST(markup_that_drops_a_conversion_is_rejected)
{
    const char *const files[] = {
        "nmlgc/base_tsa/th06/stringlocs.js",
        "{Rx6a98c: 'song'}",
        "thpatch/lang/stringdefs.js",
        "{song: '<s$%s$> hidden'}",
    };
    M_Begin("markup_rejected", files, 4);
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "♪%s");
    M_End();
}

TEST(spell_cards_stages_and_songs_are_translated)
{
    const char *const files[] = {
        "thpatch/lang/th06/spells.js",
        "{'0': 'Moon Sign \"Moonlight Ray\"', '63': 'Last'}",
        "thpatch/lang/th06/stages.js",
        "{'1': 'Scroll of the Night', '7': ['Extra', 'second line']}",
        "thpatch/lang/themes.js",
        ("{th06_01: 'Title', th06_02: 'Stage 1', th06_03: 'Boss 1', "
         "th06_15: 'Extra boss'}"),
    };
    M_Begin("names", files, 6);
    CHECK_EQ_STR(
        Translation_GetSpellName(0, "月符"), "Moon Sign \"Moonlight Ray\"");
    CHECK_EQ_STR(Translation_GetSpellName(63, "x"), "Last");
    CHECK_EQ_STR(Translation_GetSpellName(1, "original"), "original");
    CHECK_EQ_STR(Translation_GetSpellName(64, "original"), "original");
    CHECK_EQ_STR(Translation_GetSpellName(-1, "original"), "original");
    CHECK_EQ_STR(Translation_GetStageName(1, "x"), "Scroll of the Night");
    CHECK_EQ_STR(Translation_GetStageName(7, "x"), "Extra");
    CHECK_EQ_STR(Translation_GetStageName(2, "original"), "original");
    CHECK_EQ_STR(Translation_GetSongName(1, 0, "x"), "Stage 1");
    CHECK_EQ_STR(Translation_GetSongName(1, 1, "x"), "Boss 1");
    CHECK_EQ_STR(Translation_GetSongName(7, 1, "x"), "Extra boss");
    CHECK_EQ_STR(Translation_GetSongName(1, 2, "original"), "original");
    CHECK_EQ_STR(Translation_GetMusicTitle(0, "x"), "Title");
    CHECK_EQ_STR(Translation_GetMusicTitle(2, "x"), "Boss 1");
    CHECK_EQ_STR(Translation_GetMusicTitle(3, "original"), "original");
    M_End();
}

TEST(music_comments_fill_every_row_and_number_the_title)
{
    const char *const files[] = {
        "thpatch/lang/themes.js",
        "{th06_01: 'A Dream More Scarlet than Red'}",
        "nmlgc/base_tsa/stringdefs.js",
        "{'Music Room Numbered Title': 'No. <r$%2d$99>  %s'}",
        "thpatch/lang/th06/musiccmt.js",
        "{'1': ['@', 'The title theme.', '<c$centred$x>']}",
    };
    M_Begin("music", files, 6);
    CHECK_EQ_STR(
        Translation_GetMusicComment(0, 0),
        "No.  1  A Dream More Scarlet than Red");
    CHECK_EQ_STR(Translation_GetMusicComment(0, 1), "The title theme.");
    CHECK_EQ_STR(Translation_GetMusicComment(0, 2), "centred");
    CHECK_EQ_STR(Translation_GetMusicComment(0, 7), "");
    CHECK(Translation_GetMusicComment(1, 0) == nullptr);
    CHECK(Translation_GetMusicComment(0, 8) == nullptr);
    M_End();
    CHECK(Translation_GetMusicComment(0, 0) == nullptr);
}

TEST(a_numbered_title_without_its_format_is_the_title_alone)
{
    const char *const files[] = {
        "thpatch/lang/themes.js",
        "{th06_02: 'Lunate Elf'}",
        "thpatch/lang/th06/musiccmt.js",
        "{'2': ['@']}",
    };
    M_Begin("music_plain", files, 4);
    CHECK_EQ_STR(Translation_GetMusicComment(1, 0), "Lunate Elf");
    M_End();
}

TEST(no_patches_translate_nothing)
{
    Translation_Init(nullptr);
    CHECK_EQ_STR(Translation_GetSpellName(0, "original"), "original");
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "♪%s");
    CHECK(Translation_GetMusicComment(0, 0) == nullptr);
    Translation_Shutdown();
}
