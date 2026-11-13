#include "core/patch/select.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/ini.h"

#include "patch_builder.h"
#include "test.h"

#include <string.h>

static PATCH_SELECTION *M_Select(
    const char *const dir, const PATCH_CHOICE *const choice)
{
    PATCH_SELECTION *selection = nullptr;
    CHECK(Result_Log(
        Patch_Select(dir, "th06", choice, &selection), LOG_LEVEL_ERROR));
    return selection;
}

// Checks the description of the selection of dir against expected, which
// leaves out the first line.
static void M_CheckDescription(
    const char *const dir, const PATCH_CHOICE *const choice,
    const char *const expected)
{
    PATCH_SELECTION *const selection = M_Select(dir, choice);
    char *const text = Patch_DescribeSelection(selection);
    char *const full = Str_Format("Patches in %s:\n%s", dir, expected);
    CHECK_EQ_STR(text, full);
    Mem_Free(full);
    Mem_Free(text);
    Patch_FreeSelection(selection);
}

static LOCALE M_Locale(const char *const language, const char *const country)
{
    LOCALE locale = {};
    snprintf(locale.language, sizeof(locale.language), "%s", language);
    snprintf(locale.country, sizeof(locale.country), "%s", country);
    return locale;
}

TEST(the_chosen_language_loads_with_its_dependencies_and_the_mods)
{
    char *const dir = TestDir_Prepare("example");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_de", "{dependencies: ['lang_en']}");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_en", "{dependencies: ['nmlgc/base_tsa']}");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_fr", "{dependencies: ['lang_en']}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_es", "{}");
    PatchBuilder_AddPatch(dir, "nmlgc/base_tsa", "{}");
    PatchBuilder_AddPatch(
        dir, "RealOkuu/OmegaPatchy", "{dependencies: ['nmlgc/base_tsa']}");
    PatchBuilder_AddPatch(
        dir, "farawayvision/old_spell_names",
        "{dependencies: ['thpatch/lang_fr']}");
    PatchBuilder_AddPatch(dir, "Bravi/BossDisplayFixes", "{}");
    const PATCH_CHOICE choice = {
        .enabled = true,
        .disabled = " Bravi/BossDisplayFixes , x/y",
        .language = "de",
    };
    M_CheckDescription(
        dir, &choice,
        "  language: thpatch/lang_de ([patches] language = de)\n"
        "  load  thpatch/lang_de                  language\n"
        "  load  thpatch/lang_en                  library, needed by "
        "thpatch/lang_de\n"
        "  load  nmlgc/base_tsa                   library, needed by "
        "thpatch/lang_en, RealOkuu/OmegaPatchy\n"
        "  load  RealOkuu/OmegaPatchy             mod\n"
        "  skip  thpatch/lang_es                  language, not chosen\n"
        "  skip  thpatch/lang_fr                  library, not needed\n"
        "  skip  farawayvision/old_spell_names    mod, needs "
        "thpatch/lang_fr, outside the chosen language\n"
        "  skip  Bravi/BossDisplayFixes           disabled in [patches] "
        "disabled\n"
        "Load order: nmlgc/base_tsa, thpatch/lang_en, thpatch/lang_de, "
        "RealOkuu/OmegaPatchy\n");
    Mem_Free(dir);
}

TEST(mods_apply_after_the_language)
{
    char *const dir = TestDir_Prepare("mods_after");
    PatchBuilder_AddPatch(dir, "thpatch/lang_en", "{dependencies: ['b/lib']}");
    PatchBuilder_AddPatch(dir, "a/mod", "{dependencies: ['b/lib', 'c/own']}");
    PatchBuilder_AddPatch(dir, "b/lib", "{}");
    PatchBuilder_AddPatch(dir, "c/own", "{}");
    M_CheckDescription(
        dir, nullptr,
        "  language: thpatch/lang_en (the only installed language)\n"
        "  load  thpatch/lang_en                  language\n"
        "  load  b/lib                            library, needed by "
        "thpatch/lang_en, a/mod\n"
        "  load  a/mod                            mod\n"
        "  load  c/own                            library, needed by a/mod\n"
        "Load order: b/lib, thpatch/lang_en, c/own, a/mod\n");
    Mem_Free(dir);
}

TEST(a_mod_may_need_the_chosen_language)
{
    char *const dir = TestDir_Prepare("mod_needs_language");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_de", "{dependencies: ['lang_en']}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_en", "{}");
    PatchBuilder_AddPatch(dir, "a/mod", "{dependencies: ['thpatch/lang_en']}");
    M_CheckDescription(
        dir, nullptr,
        "  language: thpatch/lang_de (the only installed language)\n"
        "  load  thpatch/lang_de                  language\n"
        "  load  thpatch/lang_en                  library, needed by "
        "thpatch/lang_de, a/mod\n"
        "  load  a/mod                            mod\n"
        "Load order: thpatch/lang_en, thpatch/lang_de, a/mod\n");
    Mem_Free(dir);
}

TEST(several_languages_follow_the_locales)
{
    char *const dir = TestDir_Prepare("locales");
    PatchBuilder_AddPatch(dir, "thpatch/lang_de", "{}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_es", "{}");
    const LOCALE locales[] = { M_Locale("fr", "FR"), M_Locale("de", "AT") };
    const PATCH_CHOICE choice = {
        .enabled = true,
        .locales = locales,
        .locale_count = 2,
    };
    M_CheckDescription(
        dir, &choice,
        "  language: thpatch/lang_de (system language de-AT)\n"
        "  load  thpatch/lang_de                  language\n"
        "  skip  thpatch/lang_es                  language, not chosen\n"
        "Load order: thpatch/lang_de\n");
    Mem_Free(dir);
}

TEST(several_languages_without_a_matching_locale_choose_none)
{
    char *const dir = TestDir_Prepare("no_locale");
    PatchBuilder_AddPatch(dir, "thpatch/lang_de", "{}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_es", "{}");
    PatchBuilder_AddPatch(dir, "a/mod", "{}");
    const LOCALE locales[] = { M_Locale("fr", "") };
    const PATCH_CHOICE choice = {
        .enabled = true,
        .locales = locales,
        .locale_count = 1,
    };
    M_CheckDescription(
        dir, &choice,
        "  language: original (no installed language matches the system "
        "languages)\n"
        "  load  a/mod                            mod\n"
        "  skip  thpatch/lang_de                  language, not chosen\n"
        "  skip  thpatch/lang_es                  language, not chosen\n"
        "Load order: a/mod\n");
    Mem_Free(dir);
}

TEST(a_japanese_locale_chooses_the_original)
{
    char *const dir = TestDir_Prepare("japanese_locale");
    PatchBuilder_AddPatch(dir, "thpatch/lang_de", "{}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_es", "{}");
    const LOCALE locales[] = { M_Locale("ja", "JP"), M_Locale("de", "") };
    const PATCH_CHOICE choice = {
        .enabled = true,
        .locales = locales,
        .locale_count = 2,
    };
    M_CheckDescription(
        dir, &choice,
        "  language: original (system language ja-JP)\n"
        "  skip  thpatch/lang_de                  language, not chosen\n"
        "  skip  thpatch/lang_es                  language, not chosen\n"
        "Load order: none\n");
    Mem_Free(dir);
}

TEST(a_language_under_another_is_not_top_level)
{
    char *const dir = TestDir_Prepare("top_level");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_de", "{dependencies: ['lang_en']}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_en", "{}");
    const LOCALE locales[] = { M_Locale("en", "US") };
    const PATCH_CHOICE choice = {
        .enabled = true,
        .locales = locales,
        .locale_count = 1,
    };
    M_CheckDescription(
        dir, &choice,
        "  language: thpatch/lang_de (the only installed language)\n"
        "  load  thpatch/lang_de                  language\n"
        "  load  thpatch/lang_en                  library, needed by "
        "thpatch/lang_de\n"
        "Load order: thpatch/lang_en, thpatch/lang_de\n");
    Mem_Free(dir);
}

TEST(no_language_pack_loads_the_mods_alone)
{
    char *const dir = TestDir_Prepare("no_language");
    PatchBuilder_AddPatch(dir, "a/mod", "{dependencies: ['nmlgc/base_tsa']}");
    PatchBuilder_AddPatch(dir, "nmlgc/base_tsa", "{}");
    M_CheckDescription(
        dir, nullptr,
        "  language: original (no language installed)\n"
        "  load  a/mod                            mod\n"
        "  load  nmlgc/base_tsa                   library, needed by a/mod\n"
        "Load order: nmlgc/base_tsa, a/mod\n");
    Mem_Free(dir);
}

TEST(the_original_language_holds_back_mods_that_need_a_language)
{
    char *const dir = TestDir_Prepare("original");
    PatchBuilder_AddPatch(dir, "thpatch/lang_en", "{}");
    PatchBuilder_AddPatch(dir, "a/names", "{dependencies: ['b/needs_en']}");
    PatchBuilder_AddPatch(
        dir, "b/needs_en", "{dependencies: ['thpatch/lang_en']}");
    PatchBuilder_AddPatch(dir, "c/mod", "{}");
    const PATCH_CHOICE choice = { .enabled = true, .language = "ja" };
    M_CheckDescription(
        dir, &choice,
        "  language: original ([patches] language = ja)\n"
        "  load  c/mod                            mod\n"
        "  skip  b/needs_en                       library, not needed\n"
        "  skip  thpatch/lang_en                  library, not needed\n"
        "  skip  a/names                          mod, needs thpatch/lang_en, "
        "outside the chosen language\n"
        "Load order: c/mod\n");
    Mem_Free(dir);
}

TEST(a_missing_language_chooses_the_original)
{
    char *const dir = TestDir_Prepare("missing_language");
    PatchBuilder_AddPatch(dir, "thpatch/lang_en", "{}");
    const PATCH_CHOICE choice = { .enabled = true, .language = "XX_yy" };
    PATCH_SELECTION *const selection = M_Select(dir, &choice);
    CHECK(Patch_IsLanguageMissing(selection));
    Patch_FreeSelection(selection);
    M_CheckDescription(
        dir, &choice,
        "  language: original ([patches] language = XX_yy, but "
        "thpatch/lang_xx-yy is not installed)\n"
        "  skip  thpatch/lang_en                  language, not chosen\n"
        "Load order: none\n");
    Mem_Free(dir);
}

TEST(a_disabled_language_chooses_the_original)
{
    char *const dir = TestDir_Prepare("disabled_language");
    PatchBuilder_AddPatch(dir, "thpatch/lang_de", "{}");
    const PATCH_CHOICE choice = {
        .enabled = true,
        .disabled = "thpatch/lang_de",
        .language = "de",
    };
    PATCH_SELECTION *const selection = M_Select(dir, &choice);
    CHECK(Patch_IsLanguageMissing(selection));
    Patch_FreeSelection(selection);
    M_CheckDescription(
        dir, &choice,
        "  language: original ([patches] language = de, but "
        "thpatch/lang_de is disabled)\n"
        "  skip  thpatch/lang_de                  disabled in [patches] "
        "disabled\n"
        "Load order: none\n");
    Mem_Free(dir);
}

TEST(only_disabled_languages_choose_the_original)
{
    char *const dir = TestDir_Prepare("all_disabled");
    PatchBuilder_AddPatch(dir, "thpatch/lang_de", "{}");
    const PATCH_CHOICE choice = {
        .enabled = true,
        .disabled = "thpatch/lang_de",
    };
    M_CheckDescription(
        dir, &choice,
        "  language: original (every language is disabled)\n"
        "  skip  thpatch/lang_de                  disabled in [patches] "
        "disabled\n"
        "Load order: none\n");
    Mem_Free(dir);
}

TEST(a_language_code_matches_its_full_tag_then_its_language)
{
    char *const dir = TestDir_Prepare("tags");
    PatchBuilder_AddPatch(dir, "thpatch/lang_pt", "{}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_PT-pt", "{}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_zh-hant", "{}");
    const char *const codes[][2] = {
        { "PT_br", "thpatch/lang_pt" },
        { "pt_PT", "thpatch/lang_PT-pt" },
        { "zh-TW", "thpatch/lang_zh-hant" },
        { "zh_hk", "thpatch/lang_zh-hant" },
    };
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        const PATCH_CHOICE choice = { .enabled = true,
                                      .language = codes[i][0] };
        PATCH_SELECTION *const selection = M_Select(dir, &choice);
        CHECK(!Patch_IsLanguageMissing(selection));
        CHECK_EQ_INT(Patch_GetLoadCount(selection), 1);
        CHECK_EQ_STR(Patch_GetLoad(selection, 0)->id, codes[i][1]);
        Patch_FreeSelection(selection);
    }
    Mem_Free(dir);
}

TEST(a_language_code_may_name_a_language_under_another)
{
    char *const dir = TestDir_Prepare("code_library");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_de", "{dependencies: ['lang_en']}");
    PatchBuilder_AddPatch(dir, "thpatch/lang_en", "{}");
    const PATCH_CHOICE choice = { .enabled = true, .language = "en" };
    M_CheckDescription(
        dir, &choice,
        "  language: thpatch/lang_en ([patches] language = en)\n"
        "  load  thpatch/lang_en                  language\n"
        "  skip  thpatch/lang_de                  language, not chosen\n"
        "Load order: thpatch/lang_en\n");
    Mem_Free(dir);
}

TEST(patches_off_load_nothing)
{
    char *const dir = TestDir_Prepare("off");
    PatchBuilder_AddPatch(dir, "thpatch/lang_en", "{dependencies: ['b/lib']}");
    PatchBuilder_AddPatch(dir, "b/lib", "{}");
    PatchBuilder_AddPatch(dir, "a/mod", "{}");
    PatchBuilder_AddPatch(dir, "a/off", "{}");
    const PATCH_CHOICE choice = { .disabled = "a/off" };
    M_CheckDescription(
        dir, &choice,
        "  language: original ([patches] enabled = false)\n"
        "  skip  thpatch/lang_en                  language, [patches] "
        "enabled = false\n"
        "  skip  b/lib                            library, [patches] enabled "
        "= false\n"
        "  skip  a/mod                            mod, [patches] enabled = "
        "false\n"
        "  skip  a/off                            disabled in [patches] "
        "disabled\n"
        "Load order: none\n");
    Mem_Free(dir);
}

TEST(broken_patches_hold_back_the_patches_that_need_them)
{
    char *const dir = TestDir_Prepare("broken");
    PatchBuilder_AddPatch(dir, "a/broken", "{dependencies: [");
    PatchBuilder_AddPatch(dir, "a/array", "[]");
    PatchBuilder_AddPatch(dir, "a/needs_broken", "{dependencies: ['broken']}");
    PatchBuilder_AddPatch(dir, "a/needs_gone", "{dependencies: ['a/gone']}");
    PatchBuilder_AddPatch(dir, "a/one", "{dependencies: ['two']}");
    PatchBuilder_AddPatch(dir, "a/two", "{dependencies: ['one']}");
    PatchBuilder_AddPatch(dir, "a/after", "{dependencies: ['one']}");
    PatchBuilder_AddPatch(dir, "a/self", "{dependencies: ['self']}");
    PatchBuilder_AddPatch(dir, "a/th07", "{supported_games: ['th07']}");
    PatchBuilder_AddPatch(dir, "a/needs_off", "{dependencies: ['off']}");
    PatchBuilder_AddPatch(dir, "a/off", "{}");
    PatchBuilder_AddPatch(dir, "a/unused", "{dependencies: ['a/gone']}");
    PatchBuilder_AddPatch(dir, "a/off_user", "{dependencies: ['unused']}");
    const PATCH_CHOICE choice = {
        .enabled = true,
        .disabled = "a/off,a/off_user",
    };
    M_CheckDescription(
        dir, &choice,
        "  language: original (no language installed)\n"
        "  skip  a/broken                         library, cannot read "
        "patch.js\n"
        "  skip  a/one                            library, its dependencies "
        "form a loop\n"
        "  skip  a/two                            library, its dependencies "
        "form a loop\n"
        "  skip  a/unused                         library, not needed\n"
        "  skip  a/after                          mod, needs a/one, which "
        "cannot load\n"
        "  skip  a/array                          mod, cannot read patch.js\n"
        "  skip  a/needs_broken                   mod, needs a/broken, which "
        "cannot load\n"
        "  skip  a/needs_gone                     mod, needs a/gone, which is "
        "not installed\n"
        "  skip  a/needs_off                      mod, needs a/off, which is "
        "disabled\n"
        "  skip  a/self                           mod, its dependencies form "
        "a loop\n"
        "  skip  a/th07                           mod, does not support th06\n"
        "  skip  a/off                            disabled in [patches] "
        "disabled\n"
        "  skip  a/off_user                       disabled in [patches] "
        "disabled\n"
        "Load order: none\n");
    Mem_Free(dir);
}

TEST(a_missing_folder_has_no_patches)
{
    char *const dir = TestDir_Prepare("missing");
    char *const missing = TestDir_Join(dir, "nothing");
    M_CheckDescription(
        missing, nullptr,
        "  language: original (no language installed)\n"
        "Load order: none\n");
    Mem_Free(missing);
    Mem_Free(dir);
}

TEST(entries_hold_roles_and_dependencies)
{
    char *const dir = TestDir_Prepare("entries");
    PatchBuilder_AddPatch(
        dir, "thpatch/lang_en", "{dependencies: ['nmlgc/base', 'gone']}");
    PatchBuilder_AddPatch(dir, "nmlgc/base", "{}");
    PatchBuilder_AddPatch(dir, "a/mod", "{}");
    PATCH_SELECTION *const selection = M_Select(dir, nullptr);
    CHECK_EQ_INT(Patch_GetEntryCount(selection), 3);
    const PATCH_ENTRY *const mod = Patch_GetEntry(selection, 0);
    const PATCH_ENTRY *const base = Patch_GetEntry(selection, 1);
    const PATCH_ENTRY *const language = Patch_GetEntry(selection, 2);
    CHECK_EQ_STR(mod->id, "a/mod");
    CHECK(mod->role == PATCH_ROLE_MOD && mod->load);
    CHECK(base->role == PATCH_ROLE_LIBRARY && !base->load);
    CHECK(language->role == PATCH_ROLE_LANGUAGE && !language->load);
    CHECK_EQ_STR(
        language->reason, "language, needs gone, which is not installed");
    CHECK_EQ_INT(language->dependency_count, 1);
    CHECK_EQ_INT(language->dependencies[0], 1);
    char *const expected = Str_Format("%s/nmlgc/base/", dir);
    CHECK_EQ_STR(base->dir, expected);
    Mem_Free(expected);
    Patch_FreeSelection(selection);
    Mem_Free(dir);
}

static PATCH_CHOICE M_ReadChoice(const char *const text, INI **const ini)
{
    CHECK(Result_Log(
        Ini_Parse(text, strlen(text), &g_HeapAllocator, ini), LOG_LEVEL_ERROR));
    PATCH_CHOICE choice = {};
    Patch_ReadChoice(*ini, &choice);
    return choice;
}

TEST(the_choice_comes_from_the_patches_section)
{
    INI *ini;
    const PATCH_CHOICE choice = M_ReadChoice(
        "[patches]\nenabled = Off\ndisabled = a/b, c/d\nlanguage = de\n", &ini);
    CHECK(!choice.enabled);
    CHECK_EQ_STR(choice.disabled, "a/b, c/d");
    CHECK_EQ_STR(choice.language, "de");
    Ini_Free(ini);
}

TEST(the_choice_without_settings_is_enabled)
{
    INI *ini;
    const PATCH_CHOICE choice =
        M_ReadChoice("[video]\nfullscreen = true\n", &ini);
    CHECK(choice.enabled);
    CHECK(choice.disabled == nullptr);
    CHECK(choice.language == nullptr);
    Ini_Free(ini);
}

TEST(enabled_takes_the_boolean_words_and_defaults_to_true)
{
    const char *const values[] = { "1", "yes", "TRUE", "on", "maybe" };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        char *const text = Str_Format("[patches]\nenabled = %s\n", values[i]);
        INI *ini;
        CHECK(M_ReadChoice(text, &ini).enabled);
        Ini_Free(ini);
        Mem_Free(text);
    }
    const char *const offs[] = { "0", "no", "FALSE", "off" };
    for (size_t i = 0; i < sizeof(offs) / sizeof(offs[0]); i++) {
        char *const text = Str_Format("[patches]\nenabled = %s\n", offs[i]);
        INI *ini;
        CHECK(!M_ReadChoice(text, &ini).enabled);
        Ini_Free(ini);
        Mem_Free(text);
    }
}
