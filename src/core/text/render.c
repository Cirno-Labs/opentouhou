#include "core/text/private.h"

#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static RESULT M_Measure(
    const TEXT_FONT *const font, const char *const text,
    const TEXT_STYLE *const style, int32_t *const width, int32_t *const height)
{
    *width = 0;
    *height = 0;
    FAIL_IF(
        font == nullptr || text == nullptr || style == nullptr
            || !isfinite(style->width_scale) || style->width_scale <= 0.0f
            || style->shadow_x == INT32_MIN || style->shadow_y == INT32_MIN,
        "Invalid text or style");
    if (text[0] == '\0') {
        return OK;
    }
    int natural_width, natural_height;
    FAIL_IF(
        !TTF_GetStringSize(
            font->font, text, strlen(text), &natural_width, &natural_height),
        "Cannot measure text: %s", SDL_GetError());
    const double scaled_width =
        ceil(natural_width * (double)style->width_scale);
    FAIL_IF(
        scaled_width > INT32_MAX || natural_width < 0 || natural_height < 0,
        "Text dimensions exceed the image limit");
    *width = (int32_t)scaled_width;
    *height = natural_height;
    return OK;
}

static void M_Composite(
    IMAGE *const destination, const SDL_Surface *const source,
    const int32_t width, const int32_t x, const int32_t y,
    const GFX_COLOR color)
{
    for (int32_t row = 0; row < source->h; row++) {
        const uint8_t *const pixels = (const uint8_t *)source->pixels
            + (size_t)row * (size_t)source->pitch;
        for (int32_t column = 0; column < width; column++) {
            const int32_t sample =
                (int32_t)((int64_t)column * source->w / width);
            const uint32_t alpha =
                (pixels[(size_t)sample * 4 + 3] * color.alpha + 127u) / 255u;
            uint8_t *const out = destination->pixels
                + ((size_t)(y + row) * (size_t)destination->width
                   + (size_t)(x + column))
                    * 4;
            const uint32_t remaining = out[3] * (255u - alpha);
            const uint32_t total = alpha * 255u + remaining;
            if (total == 0) {
                continue;
            }
            const uint8_t channels[3] = { color.red, color.green, color.blue };
            for (int32_t channel = 0; channel < 3; channel++) {
                out[channel] =
                    (uint8_t)((channels[channel] * alpha * 255u
                               + out[channel] * remaining + total / 2u)
                              / total);
            }
            out[3] = (uint8_t)((total + 127u) / 255u);
        }
    }
}

TEXT_STYLE TextStyle_Default(void)
{
    return (TEXT_STYLE) { .width_scale = 1.0f,
                          .color = { 255, 255, 255, 255 } };
}

RESULT Text_Measure(
    const TEXT_FONT *const font, const char *const text,
    const TEXT_STYLE *const style, int32_t *const width, int32_t *const height)
{
    MUST(M_Measure(font, text, style, width, height));
    if (*width != 0 && style->shadow.alpha != 0) {
        const int64_t full_width = (int64_t)*width + llabs(style->shadow_x);
        const int64_t full_height = (int64_t)*height + llabs(style->shadow_y);
        FAIL_IF(
            full_width > INT32_MAX || full_height > INT32_MAX,
            "Text shadow exceeds the image limit");
        *width = (int32_t)full_width;
        *height = (int32_t)full_height;
    }
    return OK;
}

RESULT Text_Render(
    const TEXT_FONT *const font, const char *const text,
    const TEXT_STYLE *const style, const ALLOCATOR *const allocator,
    IMAGE *const out)
{
    *out = (IMAGE) {};
    FAIL_IF(allocator == nullptr, "Missing text image allocator");
    int32_t width, height;
    MUST(Text_Measure(font, text, style, &width, &height));
    if (width == 0 || height == 0) {
        return OK;
    }
    SDL_Surface *const rendered = TTF_RenderText_Blended(
        font->font, text, strlen(text), (SDL_Color) { 255, 255, 255, 255 });
    FAIL_IF(rendered == nullptr, "Cannot render text: %s", SDL_GetError());
    SDL_Surface *const surface =
        SDL_ConvertSurface(rendered, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(rendered);
    FAIL_IF(
        surface == nullptr, "Cannot convert text pixels: %s", SDL_GetError());
    const bool shadow = style->shadow.alpha != 0;
    const int32_t shadow_x = shadow ? style->shadow_x : 0;
    const int32_t shadow_y = shadow ? style->shadow_y : 0;
    const int32_t origin_x = shadow_x < 0 ? -shadow_x : 0;
    const int32_t origin_y = shadow_y < 0 ? -shadow_y : 0;
    const double glyph_width = ceil(surface->w * (double)style->width_scale);
    if (!isfinite(glyph_width) || glyph_width > INT32_MAX) {
        SDL_DestroySurface(surface);
        return FAIL("Rendered text exceeds the image limit");
    }
    const int64_t full_width = (int64_t)glyph_width + llabs(shadow_x);
    const int64_t full_height = (int64_t)surface->h + llabs(shadow_y);
    if (full_width > INT32_MAX || full_height > INT32_MAX
        || full_width * full_height > INT32_MAX / 4) {
        SDL_DestroySurface(surface);
        return FAIL("Rendered text exceeds the image limit");
    }
    IMAGE image =
        Image_Create((int32_t)full_width, (int32_t)full_height, allocator);
    if (shadow) {
        M_Composite(
            &image, surface, (int32_t)glyph_width, origin_x + shadow_x,
            origin_y + shadow_y, style->shadow);
    }
    M_Composite(
        &image, surface, (int32_t)glyph_width, origin_x, origin_y,
        style->color);
    SDL_DestroySurface(surface);
    *out = image;
    return OK;
}
