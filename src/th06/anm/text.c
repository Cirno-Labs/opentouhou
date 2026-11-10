#include "th06/anm/text.h"

#include "th06/anm/texture.h"

#include "core/base/cp932.h"
#include "core/text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static TEXT_FONT *m_Font;

static GFX_COLOR M_Color(const uint32_t color)
{
    return (GFX_COLOR) { (uint8_t)color, (uint8_t)(color >> 8),
                         (uint8_t)(color >> 16), 255 };
}

static RESULT M_Format(
    ANM_VM *const vm, const uint32_t color, const uint32_t shadow,
    const char *const format, va_list args, const int32_t alignment)
{
    FAIL_IF(
        vm == nullptr || vm->sprite_data == nullptr || format == nullptr,
        "Text VM has no sprite or format");
    va_list measure;
    va_copy(measure, args);
    const int size = vsnprintf(nullptr, 0, format, measure);
    va_end(measure);
    FAIL_IF(size < 0 || size > 65535, "Invalid or excessive formatted text");
    char *const text = Mem_Alloc((size_t)size + 1);
    vsnprintf(text, (size_t)size + 1, format, args);
    char *encoded = nullptr;
    const RESULT encoding = Cp932_Encode(text, (size_t)size, &encoded);
    if (!IS_OK(encoding)) {
        Mem_Free(text);
        return encoding;
    }
    const ANM_SPRITE *const sprite = vm->sprite_data;
    const int32_t font_height = vm->font_width > 0 ? vm->font_width : 15;
    int32_t x = (int32_t)sprite->x0;
    if (alignment != 0) {
        const float advance = (float)strlen(encoded) * (float)(font_height + 1);
        x = alignment == 1
            ? (int32_t)(sprite->x0 + sprite->texture_width - advance / 2.0f)
            : (int32_t)(sprite->x0 + sprite->texture_width / 2.0f
                        - advance / 4.0f);
    }
    Mem_Free(encoded);
    const RESULT result = Anm_RenderTextToTextureIndex(
        sprite->slot, x, (int32_t)sprite->y0, (int32_t)sprite->texture_width,
        font_height, color, shadow, text);
    Mem_Free(text);
    vm->shown = true;
    return result;
}

RESULT Anm_InitText(void)
{
    if (m_Font == nullptr) {
        MUST(Text_Init());
        const RESULT result = TextFont_LoadDefault(30.0f, &m_Font);
        if (!IS_OK(result)) {
            Text_Shutdown();
            return result;
        }
    }
    return OK;
}

void Anm_ShutdownText(void)
{
    TextFont_Free(m_Font);
    m_Font = nullptr;
    Text_Shutdown();
}

RESULT Anm_RenderTextToTextureIndex(
    const int32_t texture, const int32_t x, const int32_t y,
    const int32_t width, const int32_t font_height, const uint32_t color,
    const uint32_t shadow, const char *const text)
{
    FAIL_IF(
        texture < 0 || texture >= ANM_TEXTURE_COUNT || text == nullptr,
        "Invalid text texture or string");
    if (m_Font == nullptr) {
        return OK;
    }
    GFX_TEXTURE *const destination = Anm_GetTexture(texture);
    FAIL_IF(destination == nullptr, "Text texture is not loaded");
    FAIL_IF(
        width < 2 || width > 8192 || x < -16384 || x > 16384 || y < 0
            || width > GfxTexture_GetWidth(destination)
            || (int64_t)y + 16 > GfxTexture_GetHeight(destination),
        "Text row exceeds the texture");
    const int32_t height = font_height > 0 ? font_height : 15;
    FAIL_IF(height < 2 || height > 255, "Invalid ANM font height");
    MUST(TextFont_SetSize(m_Font, (float)height * 2.0f));
    TEXT_STYLE style = TextStyle_Default();
    style.color = M_Color(color);
    style.shadow = shadow == UINT32_MAX ? (GFX_COLOR) {} : M_Color(shadow);
    style.shadow_x = 3;
    style.shadow_y = 2;
    IMAGE glyphs;
    MUST(Text_Render(m_Font, text, &style, &g_HeapAllocator, &glyphs));
    IMAGE surface =
        Image_Create(width * 2 - 2, height * 2 - 2, &g_HeapAllocator);
    for (int32_t row = 0; row < glyphs.height && row < surface.height; row++) {
        for (int32_t column = 0; column < glyphs.width; column++) {
            const int32_t target = column + x * 2;
            if (target >= 0 && target < surface.width) {
                memcpy(
                    surface.pixels
                        + ((size_t)row * (size_t)surface.width + (size_t)target)
                            * 4,
                    glyphs.pixels
                        + ((size_t)row * (size_t)glyphs.width + (size_t)column)
                            * 4,
                    4);
            }
        }
    }
    Image_Free(&glyphs);
    Image_Quantize(&surface, IMAGE_DEPTH_A1R5G5B5);
    Image_Resize(&surface, width, 16);
    const RESULT result =
        Anm_UpdateTexture(texture, (GFX_RECT) { 0, y, width, 16 }, &surface);
    Image_Free(&surface);
    return result;
}

RESULT Anm_RenderText(
    ANM_VM *const vm, const uint32_t color, const uint32_t shadow,
    const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    const RESULT result = M_Format(vm, color, shadow, format, args, 0);
    va_end(args);
    return result;
}

RESULT Anm_RenderTextRightAligned(
    ANM_VM *const vm, const uint32_t color, const uint32_t shadow,
    const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    const RESULT result = M_Format(vm, color, shadow, format, args, 1);
    va_end(args);
    return result;
}

RESULT Anm_RenderTextCentered(
    ANM_VM *const vm, const uint32_t color, const uint32_t shadow,
    const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    const RESULT result = M_Format(vm, color, shadow, format, args, 2);
    va_end(args);
    return result;
}
