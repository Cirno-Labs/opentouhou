#include "th07/anm/text.h"

#include "th07/anm/texture.h"

#include "stg/note.h"
#include "stg/translation.h"

#include "core/base/log.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/math/x87.h"
#include "core/text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Retail's GDI surface, which all text is drawn on first.
#define M_SURFACE_WIDTH 1024
#define M_SURFACE_HEIGHT 64

static TEXT_FONT *m_Font;

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

// Keeps only the text of the first count pixels in memory order, darkened
// along a gradient (lighter when light is set), and clears the rest.
static void M_Shade(IMAGE *const surface, const int32_t count, const bool light)
{
    const int32_t total = count * 2;
    for (int32_t i = 0; i < surface->width * surface->height; i++) {
        uint8_t *const pixel = surface->pixels + (size_t)i * 4;
        if (i >= count || pixel[3] == 0) {
            memset(pixel, 0, 4);
            continue;
        }
        const int32_t offset = i * 2;
        int32_t r = pixel[0] >> 3;
        int32_t g = pixel[1] >> 3;
        int32_t b = pixel[2] >> 3;
        if (r >= b) {
            r -= light ? r * offset / total / 4 : r * offset * 2 / total / 3;
            g -= light ? g * offset / total / 4 : g * offset * 2 / total / 3;
        } else {
            b -= light ? b * offset / total / 4 : b * offset / total / 2;
            g -= light ? g * offset / total / 4 : g * offset / total / 2;
        }
        pixel[0] = M_Expand5(r);
        pixel[1] = M_Expand5(g);
        pixel[2] = M_Expand5(b);
    }
}

static RESULT M_Measure(
    const char *const text, const int32_t font_height, int32_t *const width)
{
    *width = 0;
    if (m_Font == nullptr) {
        return OK;
    }
    FAIL_IF(
        font_height < 2 || font_height * 2 > M_SURFACE_HEIGHT,
        "Invalid ANM font height");
    int32_t offset_y;
    MUST(TextFont_SetSizeRounded(
        m_Font, (float)(font_height * 2 - 2), &offset_y));
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
    int32_t font_width = vm->font_width;
    int32_t x = X87_Ftol(sprite->x0);
    if (alignment != 0) {
        font_width = font_width > 0 ? font_width : 15;
        const int32_t count = M_ByteLength(text);
        float advance;
        if (count >= 0) {
            advance = (float)count * (float)font_width * sprite->scale_x;
        } else {
            const bool translated = Translation_HasBaseTsa();
            const int32_t font = translated
                ? font_width
                : X87_Ftol((float)font_width * sprite->scale_x);
            int32_t measured;
            const RESULT measuring = M_Measure(text, font, &measured);
            if (!IS_OK(measuring)) {
                Mem_Free(text);
                return measuring;
            }
            if (translated) {
                const int32_t half = measured / 2;
                advance = alignment == 1
                    ? (float)(half + 4) * 2.0f
                    : (float)(half * 2 + 16) * sprite->scale_x;
            } else {
                advance = (float)measured * (float)font / (float)(font - 1);
            }
        }
        x = alignment == 1
            ? X87_Ftol(
                  sprite->x0 + sprite->width * sprite->scale_x - advance / 2.0f)
            : X87_Ftol(
                  sprite->x0 + sprite->width * sprite->scale_x / 2.0f
                  - advance / 4.0f);
    }
    const RESULT result = Anm_RenderTextToTextureIndex(
        sprite->slot, x, X87_Ftol(sprite->y0), X87_Ftol(sprite->texture_width),
        font_width, vm->font_height, color, shadow, text, sprite->scale_x,
        sprite->scale_y);
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
                TextFont_LoadFamily(paths, count, family, 30.0f, &m_Font),
                LOG_LEVEL_WARNING)) {
            return OK;
        }
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
    const int32_t width, int32_t font_height, int32_t dest_height,
    const uint32_t color, const uint32_t shadow, const char *const text,
    const float scale_x, const float scale_y)
{
    FAIL_IF(
        texture < 0 || texture >= ANM_TEXTURE_COUNT || text == nullptr,
        "Invalid text texture or string");
    Note_Show(text);
    if (m_Font == nullptr) {
        return OK;
    }
    font_height = font_height > 0 ? font_height : 15;
    dest_height = dest_height > 0 ? dest_height : 15;
    const int32_t font = X87_Ftol((float)font_height * scale_x);
    const int32_t dest = X87_Ftol((float)dest_height * scale_y);
    GFX_TEXTURE *const destination = Anm_GetTexture(texture);
    FAIL_IF(destination == nullptr, "Text texture is not loaded");
    FAIL_IF(
        font < 2 || font * 2 > M_SURFACE_HEIGHT || dest < 1 || width < 1
            || width > GfxTexture_GetWidth(destination) || x < -16384
            || x > 16384 || y < 0
            || (int64_t)y + dest > GfxTexture_GetHeight(destination),
        "Text row exceeds the texture");
    int32_t offset_y;
    MUST(TextFont_SetSizeRounded(m_Font, (float)(font * 2 - 2), &offset_y));
    const TEXT_STYLE style = M_Style();
    IMAGE glyphs;
    MUST(Text_RenderMarkup(m_Font, text, &style, &g_HeapAllocator, &glyphs));
    IMAGE surface =
        Image_Create(M_SURFACE_WIDTH, M_SURFACE_HEIGHT, &g_HeapAllocator);
    if (glyphs.pixels != nullptr) {
        const GFX_COLOR black = { 0, 0, 0, 255 };
        const int32_t left = x * 2;
        if (shadow != UINT32_MAX) {
            Text_StampGdi(&surface, &glyphs, left + 4, offset_y + 2, black);
            Text_StampGdi(&surface, &glyphs, left, offset_y + 2, black);
            Text_StampGdi(&surface, &glyphs, left + 2, offset_y, black);
            Text_StampGdi(&surface, &glyphs, left + 2, offset_y + 4, black);
        } else {
            Text_StampGdi(&surface, &glyphs, left + 3, offset_y + 2, black);
            Text_StampGdi(&surface, &glyphs, left + 1, offset_y + 2, black);
            Text_StampGdi(&surface, &glyphs, left + 2, offset_y + 1, black);
            Text_StampGdi(&surface, &glyphs, left + 2, offset_y + 3, black);
        }
        Text_StampGdi(
            &surface, &glyphs, left + 2, offset_y + 2, M_Color(color));
    }
    Image_Free(&glyphs);
    // Retail shades a span of the surface's memory, not a rectangle.
    const int64_t span = (int64_t)width * 2 * (font * 2 + 6);
    M_Shade(
        &surface,
        span < M_SURFACE_WIDTH * M_SURFACE_HEIGHT
            ? (int32_t)span
            : M_SURFACE_WIDTH * M_SURFACE_HEIGHT,
        shadow == UINT32_MAX);
    const int32_t source_width =
        width * 2 < M_SURFACE_WIDTH ? width * 2 : M_SURFACE_WIDTH;
    IMAGE source;
    MUST(Image_Crop(
        &surface, 0, 0, source_width, font * 2, &g_HeapAllocator, &source));
    Image_Free(&surface);
    if (width <= source.width && dest <= source.height) {
        Image_DownsampleTriangle(
            &source, width, dest, IMAGE_DEPTH_A1R5G5B5,
            Anm_GetTextureDepth(texture));
    } else {
        Image_Resize(&source, width, dest);
    }
    const RESULT result =
        Anm_UpdateTexture(texture, (GFX_RECT) { 0, y, width, dest }, &source);
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
