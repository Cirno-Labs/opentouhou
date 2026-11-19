#include "th06/anm/text.h"

#include "th06/anm/texture.h"
#include "stg/note.h"
#include "stg/translation.h"

#include "core/base/log.h"
#include "core/base/cp932.h"
#include "core/text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static FONT *m_Font;

static GFX_COLOR M_Color(const uint32_t color)
{
    return (GFX_COLOR) { (uint8_t)color, (uint8_t)(color >> 8),
                         (uint8_t)(color >> 16), 255 };
}

static TEXT_STYLE M_Style(void)
{
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    return style;
}

static uint8_t M_Expand5(const int32_t value)
{
    return (uint8_t)((value << 3) | (value >> 2));
}

// Shades a span in memory order, including the RGB of transparent pixels.
static void M_Shade(IMAGE *const surface, const int32_t count)
{
    for (int32_t i = 0; i < surface->width * surface->height; i++) {
        uint8_t *const pixel = surface->pixels + (size_t)i * 4;
        if (i >= count) {
            pixel[3] = 0;
            continue;
        }
        int32_t r = pixel[0] >> 3;
        int32_t g = pixel[1] >> 3;
        int32_t b = pixel[2] >> 3;
        if (pixel[3] != 0) {
            r -= r * i / count / 2;
            g -= g * i / count / 2;
            b -= b * i / count / 4;
        } else {
            r = g = 31 - i * 31 / count / 2;
            b = 31 - i * 31 / count / 4;
        }
        pixel[0] = M_Expand5(r);
        pixel[1] = M_Expand5(g);
        pixel[2] = M_Expand5(b);
    }
}

// Measures text as a row of font_height draws it, at the doubled size.
static RESULT M_MeasureRow(
    const char *const text, const int32_t font_height, int32_t *const width)
{
    *width = 0;
    if (m_Font == nullptr) {
        return OK;
    }
    FAIL_IF(font_height < 2 || font_height > 255, "Invalid ANM font height");
    int32_t offset_y;
    MUST(Font_SetSizeRounded(m_Font, (float)font_height * 2.0f, &offset_y));
    const TEXT_STYLE style = M_Style();
    int32_t height;
    MUST(Text_MeasureMarkup(m_Font, text, &style, width, &height));
    return OK;
}

static int32_t M_ByteLength(const char *const text)
{
    if (Translation_HasBaseTsa() || strchr(text, '<') != nullptr
        || strchr(text, '\x14') != nullptr) {
        return -1;
    }
    char *bytes;
    if (!IS_OK(Cp932_Encode(text, strlen(text), &bytes))) {
        return -1;
    }
    const int32_t count = (int32_t)strlen(bytes);
    Mem_Free(bytes);
    return count;
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
    const ANM_SPRITE *const sprite = vm->sprite_data;
    const int32_t font_height = vm->font_width > 0 ? vm->font_width : 15;
    int32_t x = (int32_t)sprite->x0;
    if (alignment != 0) {
        const int32_t count = M_ByteLength(text);
        float advance;
        if (count >= 0) {
            advance = (float)count * (float)(font_height + 1);
        } else {
            int32_t measured;
            const RESULT measuring = M_MeasureRow(text, font_height, &measured);
            if (!IS_OK(measuring)) {
                Mem_Free(text);
                return measuring;
            }
            advance = (float)measured;
        }
        x = alignment == 1
            ? (int32_t)(sprite->x0 + sprite->width - advance / 2.0f)
            : (int32_t)(sprite->x0 + sprite->width / 2.0f - advance / 4.0f);
    }
    const RESULT result = Anm_RenderTextToTextureIndex(
        sprite->slot, x, (int32_t)sprite->y0, (int32_t)sprite->width,
        font_height, color, shadow, text);
    Mem_Free(text);
    vm->shown = true;
    return result;
}

RESULT Anm_InitText(
    const char *const family, const char *const *const paths,
    const int32_t count)
{
    if (m_Font == nullptr) {
        MUST(Text_Init());
        if (family != nullptr
            && Result_Log(
                Font_LoadFamily(paths, count, family, 30.0f, &m_Font),
                LOG_LEVEL_WARNING)) {
            return OK;
        }
        const RESULT result = Font_LoadDefault(30.0f, &m_Font);
        if (!IS_OK(result)) {
            Text_Shutdown();
            return result;
        }
    }
    return OK;
}

void Anm_ShutdownText(void)
{
    Font_Free(m_Font);
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
    Note_Show(text);
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
    const bool halved = Translation_HasBaseTsa();
    const int32_t surface_width = halved ? 1024 : 640;
    const int32_t source_width = halved ? width * 2 : width * 2 - 2;
    const int32_t source_height = halved ? 32 : height * 2 - 2;
    FAIL_IF(
        source_width > surface_width || source_height > 64,
        "Text row exceeds the GDI surface");
    int32_t offset_y;
    MUST(Font_SetSizeRounded(m_Font, (float)height * 2.0f, &offset_y));
    const TEXT_STYLE style = M_Style();
    IMAGE glyphs;
    MUST(Text_RenderMarkup(m_Font, text, &style, &g_HeapAllocator, &glyphs));
    IMAGE surface = Image_Create(surface_width, 64, &g_HeapAllocator);
    if (shadow != UINT32_MAX) {
        Text_StampGdi(
            &surface, &glyphs, x * 2 + 3, offset_y + 2, M_Color(shadow));
    }
    Text_StampGdi(&surface, &glyphs, x * 2, offset_y, M_Color(color));
    Image_Free(&glyphs);
    const int64_t span =
        (int64_t)(halved ? surface_width : width * 2) * (height * 2 + 6);
    M_Shade(
        &surface,
        span < surface_width * 64 ? (int32_t)span : surface_width * 64);
    IMAGE source;
    const RESULT cropping = Image_Crop(
        &surface, 0, 0, source_width, source_height, &g_HeapAllocator, &source);
    Image_Free(&surface);
    MUST(cropping);
    if (width <= source.width && 16 <= source.height) {
        Image_DownsampleTriangle(
            &source, width, 16, IMAGE_DEPTH_A1R5G5B5,
            Anm_GetTextureDepth(texture));
    } else {
        Image_Resize(&source, width, 16);
    }
    const RESULT result =
        Anm_UpdateTexture(texture, (GFX_RECT) { 0, y, width, 16 }, &source);
    Image_Free(&source);
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

RESULT Anm_MeasureText(
    const char *const text, const int32_t font_height, float *const width)
{
    *width = 0.0f;
    FAIL_IF(text == nullptr, "Invalid text");
    if (m_Font == nullptr) {
        return OK;
    }
    const int32_t count = M_ByteLength(text);
    if (count >= 0) {
        *width = (float)count * (float)font_height / 2.0f;
        return OK;
    }
    int32_t measured;
    MUST(M_MeasureRow(text, font_height, &measured));
    *width = (float)measured / 2.0f;
    return OK;
}
