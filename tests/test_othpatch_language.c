// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/language.h"

#include "test.h"

static LOCALE M_Locale(const char *const language, const char *const country)
{
    LOCALE locale = {};
    snprintf(locale.language, sizeof(locale.language), "%s", language);
    snprintf(locale.country, sizeof(locale.country), "%s", country);
    return locale;
}

TEST(locale_with_a_country_tries_the_country_first)
{
    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE];
    CHECK_EQ_INT(Language_FromLocale(M_Locale("pt", "BR"), names), 2);
    CHECK_EQ_STR(names[0], "lang_pt-br");
    CHECK_EQ_STR(names[1], "lang_pt");
}

TEST(locale_without_a_country_gives_the_language)
{
    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE];
    CHECK_EQ_INT(Language_FromLocale(M_Locale("de", ""), names), 1);
    CHECK_EQ_STR(names[0], "lang_de");
}

TEST(chinese_of_china_and_singapore_is_simplified)
{
    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE];
    CHECK_EQ_INT(Language_FromLocale(M_Locale("zh", "CN"), names), 1);
    CHECK_EQ_STR(names[0], "lang_zh-hans");
    CHECK_EQ_INT(Language_FromLocale(M_Locale("zh", "SG"), names), 1);
    CHECK_EQ_STR(names[0], "lang_zh-hans");
}

TEST(chinese_of_taiwan_hong_kong_and_macao_is_traditional)
{
    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE];
    const char *const countries[] = { "TW", "HK", "MO" };
    for (int i = 0; i < 3; i++) {
        CHECK_EQ_INT(
            Language_FromLocale(M_Locale("zh", countries[i]), names), 1);
        CHECK_EQ_STR(names[0], "lang_zh-hant");
    }
}

TEST(unknown_locale_gives_no_name)
{
    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE];
    CHECK_EQ_INT(Language_FromLocale(M_Locale("", ""), names), 0);
}

TEST(argument_language_code_names_its_pack)
{
    char buffer[LANGUAGE_NAME_SIZE];
    CHECK_EQ_STR(Language_FromArgument("en", buffer), "lang_en");
    CHECK_EQ_STR(Language_FromArgument("pt_BR", buffer), "lang_pt-br");
    CHECK_EQ_STR(Language_FromArgument("en-gb", buffer), "lang_en-gb");
    CHECK_EQ_STR(Language_FromArgument("zh-hans", buffer), "lang_zh-hans");
}

TEST(argument_other_names_stay)
{
    char buffer[LANGUAGE_NAME_SIZE];
    CHECK_EQ_STR(Language_FromArgument("lang_en", buffer), "lang_en");
    CHECK_EQ_STR(Language_FromArgument("aero", buffer), "aero");
    CHECK_EQ_STR(Language_FromArgument("nmlgc/aero", buffer), "nmlgc/aero");
    CHECK_EQ_STR(Language_FromArgument("en-", buffer), "en-");
    const char *const long_code = "en-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    CHECK(Language_FromArgument(long_code, buffer) == long_code);
}
