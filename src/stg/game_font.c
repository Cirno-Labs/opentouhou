#include "stg/game_font.h"

#include "core/base/log.h"
#include "core/text/font_registry.h"

#define M_GOTHIC "ＭＳ ゴシック"
#define M_UI_GOTHIC "MS UI Gothic"
#define M_FREE_GOTHIC "IPAGothic"
#define M_FREE_UI_GOTHIC "IPAPGothic"

// Opens family linked with fallback_family, or returns nullptr when family
// does not open. A missing fallback is left out.
static FONT *M_OpenLinked(
    const char *const family, const char *const fallback_family,
    const float pixel_height)
{
    FONT *font;
    if (!Result_Ignore(FontRegistry_Open(family, pixel_height, &font))) {
        return nullptr;
    }
    FONT *fallback;
    if (Result_Ignore(
            FontRegistry_Open(fallback_family, pixel_height, &fallback))) {
        Font_SetFallback(font, fallback);
    }
    return font;
}

// Opens the default font, or returns nullptr when neither pair opens.
static FONT *M_OpenDefault(const float pixel_height)
{
    FONT *const font = M_OpenLinked(M_GOTHIC, M_UI_GOTHIC, pixel_height);
    if (font != nullptr) {
        return font;
    }
    FONT *const free_font =
        M_OpenLinked(M_FREE_GOTHIC, M_FREE_UI_GOTHIC, pixel_height);
    if (free_font != nullptr
        && !Result_Log(Font_SetEmToCell(free_font), LOG_LEVEL_WARNING)) {
        Font_Free(free_font);
        return nullptr;
    }
    return free_font;
}

RESULT GameFont_Open(
    const char *const family, const float pixel_height, FONT **const out)
{
    *out = nullptr;
    FONT *const base = M_OpenDefault(pixel_height);
    if (family != nullptr
        && Result_Log(
            FontRegistry_Open(family, pixel_height, out), LOG_LEVEL_WARNING)) {
        if (base != nullptr) {
            Font_SetFallback(*out, base);
        }
        return OK;
    }
    *out = base;
    FAIL_IF(
        base == nullptr, "No font has the family %s or %s", M_GOTHIC,
        M_FREE_GOTHIC);
    return OK;
}
