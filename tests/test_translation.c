#include "stg/translation.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/patch.h"

#include "patch_builder.h"
#include "test.h"

#include <string.h>

// A synthetic string table and game descriptor, so this test needs no
// game's own strings or patch classification.
#define TEST_STRINGS(X)                                                        \
    X(SONG_NAME_FORMAT, 0x401000, "@%s")                                       \
    X(BOMB_NAME, 0x402000, "Bomb Name")                                        \
    X(UNKNOWN_SPELL_CARD, 0, "？？？？？")                                     \
    X(CLEAR_LABEL, 0, "(C)")

#define M_STRING_ID(id_, address_, text_) STRING_##id_,
#define M_DEFAULT(id_, address_, text_) [STRING_##id_] = text_,
#define M_ADDRESS(id_, address_, text_) [STRING_##id_] = address_,

typedef enum {
    TEST_STRINGS(M_STRING_ID) STRING_COUNT,
} STRING;

static const char *g_Strings[STRING_COUNT] = { TEST_STRINGS(M_DEFAULT) };
static const char *const m_Defaults[STRING_COUNT] = { TEST_STRINGS(M_DEFAULT) };
static const uint32_t m_Addresses[STRING_COUNT] = { TEST_STRINGS(M_ADDRESS) };
static const char *const m_StringKeys[STRING_COUNT] = {
    [STRING_CLEAR_LABEL] = "game clear label",
};

static const TRANSLATION_GAME m_Game = {
    .id = "game",
    .image_base = 0x400000,
    .strings = g_Strings,
    .defaults = m_Defaults,
    .addresses = m_Addresses,
    .string_keys = m_StringKeys,
    .string_count = STRING_COUNT,
    .spell_count = 4,
    .stage_count = 2,
};

static const PATCH_GAME m_PatchGame = {
    .build = "v1",
    .layout = PATCH_LAYOUT_FLAT,
};

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
        Patch_OpenStack(m_Directory, "game", &m_PatchGame, nullptr, &m_Stack),
        LOG_LEVEL_ERROR));
    Translation_Init(m_Stack, &m_Game);
}

static void M_End(void)
{
    Translation_Shutdown();
    Patch_CloseStack(m_Stack);
    Mem_Free(m_Directory);
}

TEST(named_patch_strings_do_not_need_a_retail_address)
{
    const char *const files[] = {
        "thpatch/lang/stringdefs.js",
        "{'game clear label': '(Clear)'}",
    };
    M_Begin("named_string", files, 2);
    CHECK_EQ_STR(g_Strings[STRING_CLEAR_LABEL], "(Clear)");
    M_End();
    CHECK_EQ_STR(g_Strings[STRING_CLEAR_LABEL], "(C)");
}

TEST(exe_strings_take_their_translation_by_address)
{
    const char *const files[] = {
        "nmlgc/base_tsa/game/stringlocs.v1.js",
        "{Rx1000: 'game BGM In-game format', Rx2000: 'game Bomb Name'}",
        "thpatch/lang/stringdefs.js",
        ("{'game BGM In-game format': 'BGM: %s', "
         "'game Bomb Name': 'Spirit Sign <b$\"Fantasy Seal\"$>'}"),
    };
    M_Begin("exe_strings", files, 4);
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "BGM: %s");
    CHECK_EQ_STR(g_Strings[STRING_BOMB_NAME], "Spirit Sign \"Fantasy Seal\"");
    M_End();
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "@%s");
    CHECK_EQ_STR(g_Strings[STRING_BOMB_NAME], "Bomb Name");
}

TEST(markup_that_drops_a_conversion_is_rejected)
{
    const char *const files[] = {
        "nmlgc/base_tsa/game/stringlocs.js",
        "{Rx1000: 'song'}",
        "thpatch/lang/stringdefs.js",
        "{song: '<s$%s$> hidden'}",
    };
    M_Begin("markup_rejected", files, 4);
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "@%s");
    M_End();
}

TEST(spell_cards_stages_and_the_theme_are_translated)
{
    const char *const files[] = {
        "thpatch/lang/game/spells.js",
        "{'0': 'Moon Sign \"Moonlight Ray\"', '3': 'Last'}",
        "thpatch/lang/game/stages.js",
        "{'1': 'Scroll of the Night', '2': ['Extra', 'second line']}",
        "thpatch/lang/themes.js",
        "{game_01: 'Title', game_02: 'Stage 1', game_03: 'Boss 1'}",
    };
    M_Begin("names", files, 6);
    CHECK_EQ_STR(
        Translation_GetSpellName(0, "月符"), "Moon Sign \"Moonlight Ray\"");
    CHECK_EQ_STR(Translation_GetSpellName(3, "x"), "Last");
    CHECK_EQ_STR(Translation_GetSpellName(1, "original"), "original");
    CHECK_EQ_STR(Translation_GetSpellName(4, "original"), "original");
    CHECK_EQ_STR(Translation_GetSpellName(-1, "original"), "original");
    CHECK_EQ_STR(Translation_GetStageName(1, "x"), "Scroll of the Night");
    CHECK_EQ_STR(Translation_GetStageName(2, "x"), "Extra");
    CHECK_EQ_STR(Translation_GetStageName(3, "original"), "original");
    CHECK_EQ_STR(Translation_GetTheme(1, "x"), "Title");
    CHECK_EQ_STR(Translation_GetTheme(2, "x"), "Stage 1");
    CHECK_EQ_STR(Translation_GetTheme(3, "x"), "Boss 1");
    CHECK_EQ_STR(Translation_GetTheme(4, "original"), "original");
    CHECK_EQ_STR(Translation_GetMusicTitle(0, "x"), "Title");
    CHECK_EQ_STR(Translation_GetMusicTitle(2, "x"), "Boss 1");
    CHECK_EQ_STR(Translation_GetMusicTitle(3, "original"), "original");
    M_End();
}

TEST(music_comments_fill_every_row_and_keep_markup)
{
    const char *const files[] = {
        "thpatch/lang/themes.js",
        "{game_01: 'A Dream More Scarlet than Red'}",
        "nmlgc/base_tsa/stringdefs.js",
        "{'Music Room Numbered Title': 'No. <r$%2d$99>  %s'}",
        "thpatch/lang/game/musiccmt.js",
        "{'1': ['@', 'The title theme.', '<c$centred$x>']}",
    };
    M_Begin("music", files, 6);
    CHECK_EQ_STR(
        Translation_GetMusicComment(0, 0),
        "No. <r$ 1$99>  A Dream More Scarlet than Red");
    CHECK_EQ_STR(Translation_GetMusicComment(0, 1), "The title theme.");
    CHECK_EQ_STR(Translation_GetMusicComment(0, 2), "<c$centred$x>");
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
        "{game_02: 'Lunate Elf'}",
        "thpatch/lang/game/musiccmt.js",
        "{'2': ['@']}",
    };
    M_Begin("music_plain", files, 4);
    CHECK_EQ_STR(Translation_GetMusicComment(1, 0), "Lunate Elf");
    M_End();
}

TEST(the_font_comes_from_the_settings)
{
    const char *const files[] = {
        "nmlgc/base_tsa/patch.js",  "{}",
        "thpatch/lang/patch.js",    "{dependencies: ['nmlgc/base_tsa']}",
        "nmlgc/base_tsa/global.js", "{font: 'Global'}",
        "thpatch/lang/game.js",     "{font: 'Aroania'}",
    };
    M_Begin("font", files, 8);
    CHECK(Translation_HasBaseTsa());
    CHECK_EQ_STR(Translation_GetFont(), "Aroania");
    M_End();
}

TEST(base_tsa_applies_the_font_cache_weight)
{
    M_Begin("font_weight", nullptr, 0);
    CHECK_EQ_INT(Translation_GetFontWeight(400), 400);
    Translation_Shutdown();
    TRANSLATION_GAME game = m_Game;
    game.font_cache_weight = 700;
    Translation_Init(m_Stack, &game);
    CHECK_EQ_INT(Translation_GetFontWeight(400), 700);
    M_End();
    Translation_Init(nullptr, &game);
    CHECK_EQ_INT(Translation_GetFontWeight(400), 400);
}

TEST(no_patches_translate_nothing)
{
    Translation_Init(nullptr, &m_Game);
    CHECK(!Translation_HasBaseTsa());
    CHECK(Translation_GetFont() == nullptr);
    CHECK_EQ_STR(Translation_GetSpellName(0, "original"), "original");
    CHECK_EQ_STR(g_Strings[STRING_SONG_NAME_FORMAT], "@%s");
    CHECK(Translation_GetMusicComment(0, 0) == nullptr);
    Translation_Shutdown();
}
