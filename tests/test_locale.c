// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/locale.h"

#include "test.h"

#include <SDL3/SDL.h>
#include <string.h>

TEST(preferred_is_well_formed)
{
    const LOCALE locale = Locale_GetPreferred();
    const size_t language = strlen(locale.language);
    CHECK(language == 0 || language == 2 || language == 3);
    CHECK(language != 0 || locale.country[0] == '\0');
}

// SDL reads the locale from these variables on Linux only. macOS and Windows
// ask the system, so the tests below cannot choose the locale there.
#ifdef __linux__
static void M_SetLocale(const char *const lang)
{
    SDL_Environment *const environment = SDL_GetEnvironment();
    SDL_UnsetEnvironmentVariable(environment, "LC_ALL");
    SDL_UnsetEnvironmentVariable(environment, "LC_MESSAGES");
    SDL_UnsetEnvironmentVariable(environment, "LANGUAGE");
    SDL_SetEnvironmentVariable(environment, "LANG", lang, true);
}

TEST(preferred_gives_language_and_country)
{
    M_SetLocale("pt_BR.UTF-8");
    const LOCALE locale = Locale_GetPreferred();
    CHECK_EQ_STR(locale.language, "pt");
    CHECK_EQ_STR(locale.country, "BR");
}

TEST(preferred_without_a_country_has_an_empty_country)
{
    M_SetLocale("de");
    const LOCALE locale = Locale_GetPreferred();
    CHECK_EQ_STR(locale.language, "de");
    CHECK_EQ_STR(locale.country, "");
}

TEST(preferred_of_the_c_locale_is_unknown)
{
    M_SetLocale("C");
    const LOCALE locale = Locale_GetPreferred();
    CHECK_EQ_STR(locale.language, "");
    CHECK_EQ_STR(locale.country, "");
}

TEST(preferred_of_a_language_name_is_unknown)
{
    M_SetLocale("german");
    const LOCALE locale = Locale_GetPreferred();
    CHECK_EQ_STR(locale.language, "");
}
#endif
