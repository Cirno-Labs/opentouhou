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

static LOCALE M_FromSdl(const SDL_Locale *const sdl)
{
    LOCALE locale = {};
    if (sdl == nullptr || sdl->language == nullptr
        || !M_IsLanguage(sdl->language)) {
        return locale;
    }
    for (size_t i = 0; i < LOCALE_CODE_SIZE - 1 && sdl->language[i] != '\0';
         i++) {
        locale.language[i] = (char)SDL_tolower(sdl->language[i]);
    }
    if (sdl->country != nullptr && M_IsCountry(sdl->country)) {
        for (size_t i = 0; i < LOCALE_CODE_SIZE - 1 && sdl->country[i] != '\0';
             i++) {
            locale.country[i] = (char)SDL_toupper(sdl->country[i]);
        }
    }
    return locale;
}

LOCALE Locale_GetPreferred(void)
{
    int count = 0;
    SDL_Locale **const locales = SDL_GetPreferredLocales(&count);
    if (locales == nullptr) {
        return (LOCALE) {};
    }
    const LOCALE locale = M_FromSdl(count > 0 ? locales[0] : nullptr);
    SDL_free(locales);
    return locale;
}

int32_t Locale_ListPreferred(LOCALE *const locales, const int32_t capacity)
{
    int count = 0;
    SDL_Locale **const sdl = SDL_GetPreferredLocales(&count);
    if (sdl == nullptr) {
        return 0;
    }
    int32_t written = 0;
    for (int i = 0; i < count && written < capacity; i++) {
        const LOCALE locale = M_FromSdl(sdl[i]);
        if (locale.language[0] != '\0') {
            locales[written++] = locale;
        }
    }
    SDL_free(sdl);
    return written;
}
