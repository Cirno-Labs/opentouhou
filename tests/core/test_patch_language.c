#include "core/patch/language.h"

#include "test.h"

#include <stdio.h>

static LOCALE M_Locale(const char *const language, const char *const country)
{
    LOCALE locale = {};
    snprintf(locale.language, sizeof(locale.language), "%s", language);
    snprintf(locale.country, sizeof(locale.country), "%s", country);
    return locale;
}

TEST(a_language_pack_has_the_thpatch_prefix)
{
    CHECK(Patch_IsLanguagePack("thpatch/lang_en"));
    CHECK(!Patch_IsLanguagePack("thpatch/lang_"));
    CHECK(!Patch_IsLanguagePack("nmlgc/lang_en"));
    CHECK(!Patch_IsLanguagePack("thpatch/base"));
}

TEST(a_code_with_a_region_tries_the_region_first)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    CHECK_EQ_INT(Patch_GetLanguageTags("pt_BR", tags), 2);
    CHECK_EQ_STR(tags[0], "pt-br");
    CHECK_EQ_STR(tags[1], "pt");
}

TEST(a_code_without_a_region_gives_the_language)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    CHECK_EQ_INT(Patch_GetLanguageTags("DE", tags), 1);
    CHECK_EQ_STR(tags[0], "de");
}

TEST(chinese_of_china_and_singapore_is_simplified)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    const char *const codes[] = { "zh-CN", "zh_sg" };
    for (int i = 0; i < 2; i++) {
        CHECK_EQ_INT(Patch_GetLanguageTags(codes[i], tags), 2);
        CHECK_EQ_STR(tags[0], "zh-hans");
        CHECK_EQ_STR(tags[1], "zh");
    }
}

TEST(chinese_of_taiwan_hong_kong_and_macao_is_traditional)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    const char *const codes[] = { "zh-TW", "zh-HK", "zh_mo" };
    for (int i = 0; i < 3; i++) {
        CHECK_EQ_INT(Patch_GetLanguageTags(codes[i], tags), 2);
        CHECK_EQ_STR(tags[0], "zh-hant");
    }
}

TEST(an_empty_or_long_code_gives_no_tag)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    CHECK_EQ_INT(Patch_GetLanguageTags("", tags), 0);
    CHECK_EQ_INT(
        Patch_GetLanguageTags("en-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", tags),
        0);
}

TEST(a_locale_gives_its_code)
{
    char code[PATCH_LANGUAGE_TAG_SIZE];
    Patch_GetLocaleCode(M_Locale("pt", "BR"), code);
    CHECK_EQ_STR(code, "pt-BR");
    Patch_GetLocaleCode(M_Locale("de", ""), code);
    CHECK_EQ_STR(code, "de");
    Patch_GetLocaleCode(M_Locale("", "BR"), code);
    CHECK_EQ_STR(code, "");
}

TEST(pack_codes_compare_without_case_and_with_either_separator)
{
    CHECK(Patch_LanguageIs("pt-BR", "pt-br"));
    CHECK(Patch_LanguageIs("PT_br", "pt-br"));
    CHECK(!Patch_LanguageIs("pt", "pt-br"));
    CHECK(!Patch_LanguageIs("pt-br", "pt"));
}

TEST(japanese_is_the_original)
{
    CHECK(Patch_IsOriginalLanguage("ja"));
    CHECK(Patch_IsOriginalLanguage("ja-jp"));
    CHECK(!Patch_IsOriginalLanguage("jam"));
    CHECK(!Patch_IsOriginalLanguage("en"));
}
