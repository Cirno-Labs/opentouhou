#include "core/patch.h"

#include "core/base/memory.h"

#include "patch_builder.h"
#include "test.h"

#include <string.h>

static const PATCH_GAME m_Game = {
    .build = "v1.02h",
    .layout = PATCH_LAYOUT_FLAT,
};

static PATCH_STACK *M_Open(const char *const dir)
{
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(dir, "th06", &m_Game, nullptr, &stack),
        LOG_LEVEL_ERROR));
    return stack;
}

static void M_CheckPlain(const char *const text, const char *const expected)
{
    char *const plain = Patch_PlainText(text);
    CHECK_EQ_STR(plain, expected);
    Mem_Free(plain);
}

TEST(stringlocs_map_relative_and_absolute_addresses)
{
    char *const dir = TestDir_Prepare("locations");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/lang", "{dependencies: ['base']}");
    PatchBuilder_Write(
        dir, "a/base/th06/stringlocs.v1.02h.js",
        "{Rx6a98c: 'song', rx6b100: 'log', '0x46c440': 'easy', "
        "'[Rx10] + 1': 'expression'}");
    PatchBuilder_Write(
        dir, "a/lang/stringdefs.js",
        "{song: 'Song: %s', log: 'Log', easy: 'Simple', expression: 'x'}");
    PATCH_STACK *const stack = M_Open(dir);
    PATCH_STRINGS *const strings = Patch_LoadStrings(stack, 0x400000);
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x46a98c, "♪%s"), "Song: %s");
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x46b100, "log"), "Log");
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x46c440, "Easy   "), "Simple");
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x400010, "kept"), "kept");
    CHECK_EQ_STR(Patch_Translate(strings, "easy", "Easy"), "Simple");
    CHECK_EQ_STR(Patch_Translate(strings, "missing", "Easy"), "Easy");
    Patch_FreeStrings(strings);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_later_patch_translates_over_an_earlier_one)
{
    char *const dir = TestDir_Prepare("later");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/lang", "{dependencies: ['base']}");
    PatchBuilder_Write(dir, "a/base/th06/stringlocs.js", "{Rx10: 'x'}");
    PatchBuilder_Write(dir, "a/base/stringdefs.js", "{x: 'base'}");
    PatchBuilder_Write(dir, "a/lang/stringdefs.js", "{x: 'lang'}");
    PATCH_STACK *const stack = M_Open(dir);
    PATCH_STRINGS *const strings = Patch_LoadStrings(stack, 0x400000);
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x400010, "orig"), "lang");
    Patch_FreeStrings(strings);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_translation_with_other_conversions_is_rejected)
{
    char *const dir = TestDir_Prepare("rejected");
    PatchBuilder_AddPatch(dir, "a/p", "{}");
    PatchBuilder_Write(dir, "a/p/th06/stringlocs.js", "{Rx10: 'x', Rx20: 'y'}");
    PatchBuilder_Write(dir, "a/p/stringdefs.js", "{x: 'Stage %s', y: 'Plain'}");
    PATCH_STACK *const stack = M_Open(dir);
    PATCH_STRINGS *const strings = Patch_LoadStrings(stack, 0x400000);
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x400010, "STAGE %d"), "STAGE %d");
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x400020, "Easy"), "Plain");
    Patch_FreeStrings(strings);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(no_strings_return_the_original)
{
    PATCH_STRINGS *const strings = Patch_LoadStrings(nullptr, 0x400000);
    CHECK_EQ_STR(Patch_TranslateAt(strings, 0x400010, "orig"), "orig");
    Patch_FreeStrings(strings);
    CHECK_EQ_STR(Patch_TranslateAt(nullptr, 0x400010, "orig"), "orig");
    CHECK_EQ_STR(Patch_Translate(nullptr, "x", "orig"), "orig");
    Patch_FreeStrings(nullptr);
}

TEST(conversions_match_by_their_arguments)
{
    CHECK(Patch_SameConversions("STAGE %d  %.9d", "Stage %d - %09d"));
    CHECK(Patch_SameConversions("%8s %9d(%d)", "%-10s %d (%d)"));
    CHECK(Patch_SameConversions("100%% %s", "%s at 100%%"));
    CHECK(Patch_SameConversions("plain", "other plain"));
    CHECK(Patch_SameConversions("%*d", "%-*d"));
    CHECK(Patch_SameConversions("%.*s", "%*s"));
}

TEST(conversions_that_differ_do_not_match)
{
    CHECK(!Patch_SameConversions("%d", "%s"));
    CHECK(!Patch_SameConversions("%d %s", "%s %d"));
    CHECK(!Patch_SameConversions("%d", "%d %d"));
    CHECK(!Patch_SameConversions("%d", "plain"));
    CHECK(!Patch_SameConversions("plain", "100%"));
    CHECK(!Patch_SameConversions("%d", "%ld"));
    CHECK(!Patch_SameConversions("%d", "%*d"));
    CHECK(!Patch_SameConversions("%s", "%%s"));
    CHECK(!Patch_SameConversions("%*d", "%*i"));
}

TEST(a_malformed_format_matches_only_itself)
{
    CHECK(Patch_SameConversions("50%", "50%"));
    CHECK(!Patch_SameConversions("50%", "50 %"));
    CHECK(!Patch_SameConversions("%y", "%d"));
}

TEST(markup_becomes_its_text)
{
    M_CheckPlain("<c$Rumia$Youkai of the Dusk>", "Rumia");
    M_CheckPlain("<c$Reimu:$12345678> \"Hello.\"", "Reimu: \"Hello.\"");
    M_CheckPlain("<c$$12345678\"> rest", " rest");
    M_CheckPlain("No. <r$%2d$99>  %s", "No. %2d  %s");
    M_CheckPlain("<t$♪><ls$%d> %s", "♪ %s");
    M_CheckPlain("<b$<i$nested$x>$y>!", "nested!");
}

TEST(text_that_is_not_markup_stays)
{
    M_CheckPlain("a <text> b", "a <text> b");
    M_CheckPlain("unclosed <c$a$b", "unclosed <c$a$b");
    M_CheckPlain("1 < 2 > 0", "1 < 2 > 0");
    M_CheckPlain("", "");
}

TEST(a_translator_note_is_cut_off)
{
    M_CheckPlain("Line\x14note about it", "Line");
    M_CheckPlain("<c$A\x14note$x> B", "A B");
}
