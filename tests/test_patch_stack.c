#include "core/patch.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include "patch_builder.h"
#include "test.h"

#include <string.h>

static PATCH_FILE_KIND M_Classify(const char *const name)
{
    if (Str_EndsWith(name, ".ecl")) {
        return PATCH_FILE_GAMEPLAY;
    }
    if (Str_EndsWith(name, ".png")) {
        return PATCH_FILE_IGNORED;
    }
    return PATCH_FILE_DATA;
}

static const PATCH_GAME m_Game = {
    .build = "v1.02h",
    .layout = PATCH_LAYOUT_FLAT,
    .classify = M_Classify,
};

static PATCH_STACK *M_Open(
    const char *const dir, const char *const *const disabled,
    const int32_t disabled_count)
{
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(dir, "th06", &m_Game, disabled, disabled_count, &stack),
        LOG_LEVEL_ERROR));
    return stack;
}

// Returns the ids of the stack, in order, joined by ','. Mem_Free releases
// the text.
static char *M_Order(const PATCH_STACK *const stack)
{
    char *order = Str_Dup("");
    for (int32_t i = 0; i < Patch_GetCount(stack); i++) {
        char *const next = Str_Format(
            "%s%s%s", order, i == 0 ? "" : ",", Patch_Get(stack, i)->id);
        Mem_Free(order);
        order = next;
    }
    return order;
}

static void M_CheckOrder(
    const char *const dir, const char *const expected,
    const char *const *const disabled, const int32_t disabled_count)
{
    PATCH_STACK *const stack = M_Open(dir, disabled, disabled_count);
    char *const order = M_Order(stack);
    CHECK_EQ_STR(order, expected);
    Mem_Free(order);
    Patch_CloseStack(stack);
}

TEST(a_missing_folder_gives_an_empty_stack)
{
    char *const dir = TestDir_Prepare("missing");
    char *const missing = TestDir_Join(dir, "nothing");
    PATCH_STACK *const stack = M_Open(missing, nullptr, 0);
    CHECK(stack != nullptr);
    CHECK_EQ_INT(Patch_GetCount(stack), 0);
    CHECK(!Patch_ChangesGameplay(stack));
    Patch_CloseStack(stack);
    Mem_Free(missing);
    Mem_Free(dir);
}

TEST(dependencies_apply_first)
{
    char *const dir = TestDir_Prepare("dependencies");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_en",
        "{dependencies: ['nmlgc/base_tsa', 'nmlgc/script_latin']}");
    PatchBuilder_AddPatch(dir, "nmlgc/script_latin", "{dependencies: ['zz']}");
    PatchBuilder_AddPatch(dir, "nmlgc/zz", "{}");
    PatchBuilder_AddPatch(dir, "nmlgc/base_tsa", "{}");
    M_CheckOrder(
        dir, "nmlgc/base_tsa,nmlgc/zz,nmlgc/script_latin,thpatch/lang_en",
        nullptr, 0);
    Mem_Free(dir);
}

TEST(unrelated_patches_apply_in_id_order)
{
    char *const dir = TestDir_Prepare("id_order");
    PatchBuilder_AddPatch(dir, "a/x", "{}");
    PatchBuilder_AddPatch(dir, "a-b/y", "{}");
    PatchBuilder_AddPatch(dir, "B/z", "{}");
    M_CheckOrder(dir, "B/z,a-b/y,a/x", nullptr, 0);
    Mem_Free(dir);
}

TEST(a_bare_dependency_prefers_the_same_repository)
{
    char *const dir = TestDir_Prepare("bare_same_repo");
    PatchBuilder_AddPatch(dir, "a/base", "{dependencies: ['c/later']}");
    PatchBuilder_AddPatch(dir, "c/later", "{}");
    PatchBuilder_AddPatch(dir, "b/base", "{}");
    PatchBuilder_AddPatch(dir, "b/top", "{dependencies: ['base']}");
    M_CheckOrder(dir, "b/base,b/top,c/later,a/base", nullptr, 0);
    Mem_Free(dir);
}

TEST(a_bare_dependency_falls_back_to_the_first_repository)
{
    char *const dir = TestDir_Prepare("bare_other_repo");
    PatchBuilder_AddPatch(dir, "z/top", "{dependencies: ['base']}");
    PatchBuilder_AddPatch(dir, "m/base", "{dependencies: ['n/other']}");
    PatchBuilder_AddPatch(dir, "n/other", "{}");
    PatchBuilder_AddPatch(dir, "y/base", "{}");
    M_CheckOrder(dir, "n/other,m/base,y/base,z/top", nullptr, 0);
    Mem_Free(dir);
}

TEST(a_missing_dependency_leaves_out_the_patch_and_its_dependents)
{
    char *const dir = TestDir_Prepare("missing_dependency");
    PatchBuilder_AddPatch(dir, "a/needs", "{dependencies: ['a/gone']}");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['needs']}");
    PatchBuilder_AddPatch(dir, "a/fine", "{}");
    M_CheckOrder(dir, "a/fine", nullptr, 0);
    Mem_Free(dir);
}

TEST(a_disabled_patch_leaves_out_its_dependents)
{
    char *const dir = TestDir_Prepare("disabled");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['base']}");
    PatchBuilder_AddPatch(dir, "a/other", "{}");
    const char *const disabled[] = { "a/base" };
    M_CheckOrder(dir, "a/other", disabled, 1);
    Mem_Free(dir);
}

TEST(a_patch_for_other_games_is_left_out)
{
    char *const dir = TestDir_Prepare("supported_games");
    PatchBuilder_AddPatch(dir, "a/th07_only", "{supported_games: ['th07']}");
    PatchBuilder_AddPatch(dir, "a/both", "{supported_games: ['th07', 'th06']}");
    PatchBuilder_AddPatch(dir, "a/any", "{}");
    M_CheckOrder(dir, "a/any,a/both", nullptr, 0);
    Mem_Free(dir);
}

TEST(a_broken_patch_js_is_left_out)
{
    char *const dir = TestDir_Prepare("broken");
    PatchBuilder_AddPatch(dir, "a/broken", "{dependencies: [");
    PatchBuilder_AddPatch(dir, "a/array", "[]");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['broken']}");
    PatchBuilder_AddPatch(dir, "a/fine", "{}");
    M_CheckOrder(dir, "a/fine", nullptr, 0);
    Mem_Free(dir);
}

TEST(a_dependency_cycle_is_left_out)
{
    char *const dir = TestDir_Prepare("cycle");
    PatchBuilder_AddPatch(dir, "a/one", "{dependencies: ['two']}");
    PatchBuilder_AddPatch(dir, "a/two", "{dependencies: ['one']}");
    PatchBuilder_AddPatch(dir, "a/self", "{dependencies: ['self']}");
    PatchBuilder_AddPatch(dir, "a/after", "{dependencies: ['one']}");
    PatchBuilder_AddPatch(dir, "a/fine", "{}");
    M_CheckOrder(dir, "a/fine", nullptr, 0);
    Mem_Free(dir);
}

TEST(a_directory_without_patch_js_is_not_a_patch)
{
    char *const dir = TestDir_Prepare("not_a_patch");
    PatchBuilder_Write(dir, "a/empty/th06/msg1.dat", "x");
    PatchBuilder_Write(dir, "loose.txt", "x");
    PatchBuilder_AddPatch(dir, "a/real", "{}");
    M_CheckOrder(dir, "a/real", nullptr, 0);
    Mem_Free(dir);
}

TEST(the_info_holds_the_title_and_directory)
{
    char *const dir = TestDir_Prepare("info");
    PatchBuilder_AddPatch(dir, "a/titled", "{title: 'English language pack'}");
    PatchBuilder_AddPatch(dir, "a/untitled", "{}");
    PATCH_STACK *const stack = M_Open(dir, nullptr, 0);
    CHECK_EQ_INT(Patch_GetCount(stack), 2);
    CHECK_EQ_STR(Patch_Get(stack, 0)->title, "English language pack");
    CHECK_EQ_STR(Patch_Get(stack, 1)->title, "a/untitled");
    char *const expected = Str_Format("%s/a/titled/", dir);
    CHECK_EQ_STR(Patch_Get(stack, 0)->dir, expected);
    Mem_Free(expected);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(the_info_lists_the_true_fonts)
{
    char *const dir = TestDir_Prepare("fonts");
    PatchBuilder_AddPatch(
        dir, "a/fonts",
        "{fonts: {'One.ttf': true, 'Off.ttf': false, 'Two.otf': true, "
        "'Odd.ttf': 1}}");
    PATCH_STACK *const stack = M_Open(dir, nullptr, 0);
    const PATCH_INFO *const info = Patch_Get(stack, 0);
    CHECK_EQ_INT(info->font_count, 2);
    char *const one = Str_Format("%s/a/fonts/One.ttf", dir);
    char *const two = Str_Format("%s/a/fonts/Two.otf", dir);
    CHECK_EQ_STR(info->fonts[0], one);
    CHECK_EQ_STR(info->fonts[1], two);
    Mem_Free(two);
    Mem_Free(one);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_patch_without_fonts_lists_none)
{
    char *const dir = TestDir_Prepare("no_fonts");
    PatchBuilder_AddPatch(dir, "a/plain", "{fonts: ['One.ttf']}");
    PATCH_STACK *const stack = M_Open(dir, nullptr, 0);
    CHECK_EQ_INT(Patch_Get(stack, 0)->font_count, 0);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_gameplay_file_marks_the_stack)
{
    char *const dir = TestDir_Prepare("gameplay");
    PatchBuilder_AddPatch(dir, "a/mod", "{}");
    PatchBuilder_Write(dir, "a/mod/th06/ECLDATA1.ECL", "x");
    PATCH_STACK *const stack = M_Open(dir, nullptr, 0);
    CHECK(Patch_ChangesGameplay(stack));
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(other_files_leave_the_stack_unmarked)
{
    char *const dir = TestDir_Prepare("not_gameplay");
    PatchBuilder_AddPatch(dir, "a/mod", "{}");
    PatchBuilder_Write(dir, "a/mod/th06/front.png", "x");
    PatchBuilder_Write(dir, "a/mod/th06/msg1.dat.jdiff", "{}");
    PatchBuilder_Write(dir, "a/mod/th06/ecldata1.ecl.jdiff", "{}");
    PatchBuilder_Write(dir, "a/mod/th07/ecldata1.ecl", "x");
    PatchBuilder_Write(dir, "a/mod/ecldata1.ecl", "x");
    PATCH_STACK *const stack = M_Open(dir, nullptr, 0);
    CHECK(!Patch_ChangesGameplay(stack));
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_build_specific_gameplay_file_marks_the_stack)
{
    char *const dir = TestDir_Prepare("gameplay_build");
    PatchBuilder_AddPatch(dir, "a/mod", "{}");
    PatchBuilder_Write(dir, "a/mod/th06/ecldata1.v1.02h.ecl", "x");
    PATCH_STACK *const stack = M_Open(dir, nullptr, 0);
    CHECK(Patch_ChangesGameplay(stack));
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_left_out_patch_does_not_mark_the_stack)
{
    char *const dir = TestDir_Prepare("gameplay_disabled");
    PatchBuilder_AddPatch(dir, "a/mod", "{}");
    PatchBuilder_Write(dir, "a/mod/th06/ecldata1.ecl", "x");
    const char *const disabled[] = { "a/mod" };
    PATCH_STACK *const stack = M_Open(dir, disabled, 1);
    CHECK(!Patch_ChangesGameplay(stack));
    Patch_CloseStack(stack);
    Mem_Free(dir);
}
