#include "core/io/locale.h"

#include <SDL3/SDL.h>
#include <string.h>

static bool M_IsLower(const char c)
{
    return c >= 'a' && c <= 'z';
}

static bool M_IsUpper(const char c)
{
    return c >= 'A' && c <= 'Z';
}

static bool M_IsDigit(const char c)
{
    return c >= '0' && c <= '9';
}

static bool M_IsLanguage(const char *const code)
{
    const size_t size = strlen(code);
    if (size < 2 || size > 3) {
        return false;
    }
    for (size_t i = 0; i < size; i++) {
        if (!M_IsLower((char)SDL_tolower(code[i]))) {
            return false;
        }
    }
    return true;
}

static bool M_IsCountry(const char *const code)
{
    const size_t size = strlen(code);
    if (size == 2) {
        return M_IsUpper((char)SDL_toupper(code[0]))
            && M_IsUpper((char)SDL_toupper(code[1]));
    }
    return size == 3 && M_IsDigit(code[0]) && M_IsDigit(code[1])
        && M_IsDigit(code[2]);
}

LOCALE Locale_GetPreferred(void)
{
    LOCALE locale = {};
    int count = 0;
    SDL_Locale **const locales = SDL_GetPreferredLocales(&count);
    if (locales == nullptr) {
        return locale;
    }
    const SDL_Locale *const first = count > 0 ? locales[0] : nullptr;
    if (first != nullptr && first->language != nullptr
        && M_IsLanguage(first->language)) {
        for (size_t i = 0; first->language[i] != '\0'; i++) {
            locale.language[i] = (char)SDL_tolower(first->language[i]);
        }
        if (first->country != nullptr && M_IsCountry(first->country)) {
            for (size_t i = 0; first->country[i] != '\0'; i++) {
                locale.country[i] = (char)SDL_toupper(first->country[i]);
            }
        }
    }
    SDL_free(locales);
    return locale;
}
