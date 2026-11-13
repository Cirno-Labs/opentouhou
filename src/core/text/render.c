// SPDX-License-Identifier: GPL-3.0-only

#include "core/text/private.h"

#include "core/base/array.h"

#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Caps the parameters of a markup command that the layout reads: the
// command, the text and the column.
#define M_MAX_PARAMS 3
// Starts a translator note, which runs to the end of the text.
#define M_NOTE_START '\x14'

static void M_SetFace(const TEXT_FONT *const font, const uint32_t face)
{
    TTF_FontStyleFlags flags = TTF_STYLE_NORMAL;
    if ((face & TEXT_FACE_BOLD) != 0) {
        flags |= TTF_STYLE_BOLD;
    }
    if ((face & TEXT_FACE_ITALIC) != 0) {
        flags |= TTF_STYLE_ITALIC;
    }
    if ((face & TEXT_FACE_UNDERLINE) != 0) {
        flags |= TTF_STYLE_UNDERLINE;
    }
    for (const TEXT_FONT *f = font; f != nullptr; f = f->fallback) {
        if (TTF_GetFontStyle(f->font) != flags) {
            TTF_SetFontStyle(f->font, flags);
        }
    }
}

static RESULT M_Measure(
    const TEXT_FONT *const font, const char *const text, const size_t size,
    const TEXT_STYLE *const style, int32_t *const width, int32_t *const height)
{
    *width = 0;
    *height = 0;
    FAIL_IF(
        font == nullptr || text == nullptr || style == nullptr
            || !isfinite(style->width_scale) || style->width_scale <= 0.0f
            || style->shadow_x == INT32_MIN || style->shadow_y == INT32_MIN,
        "Invalid text or style");
    if (size == 0) {
        return OK;
    }
    M_SetFace(font, style->face);
    int natural_width, natural_height;
    FAIL_IF(
        !TTF_GetStringSize(
            font->font, text, size, &natural_width, &natural_height),
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
        if (y + row < 0 || y + row >= destination->height) {
            continue;
        }
        const uint8_t *const pixels = (const uint8_t *)source->pixels
            + (size_t)row * (size_t)source->pitch;
        for (int32_t column = 0; column < width; column++) {
            if (x + column < 0 || x + column >= destination->width) {
                continue;
            }
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

// Holds one piece of a line of markup: text drawn at x in the faces face,
// or, when draw is false, a piece that only moves the pen.
typedef struct {
    const char *text;
    size_t size;
    uint32_t face;
    int32_t x;
    int32_t width;
    bool draw;
} M_PIECE;

typedef ARRAY(M_PIECE) M_PIECES;

typedef struct {
    const char *start;
    size_t size;
} M_PARAM;

// Holds the laid out pieces of a line of markup and its size without the
// shadow.
typedef struct {
    M_PIECES pieces;
    int32_t width;
    int32_t height;
    bool plain;
} M_LAYOUT;

// Splits the markup at text, which starts with '<', into its parameters,
// as thcrap's layout_match does. Returns its size, or 0 when text does not
// start a markup with at least two parameters and a closing '>'.
static size_t M_MatchMarkup(
    const char *const text, const size_t size, M_PARAM *const params,
    int32_t *const count)
{
    int32_t depth = 0;
    const char *start = text + 1;
    *count = 0;
    for (size_t i = 1; i < size; i++) {
        const char c = text[i];
        depth += c == '<';
        depth -= c == '>';
        if ((depth == 0 && c == '$') || depth == -1) {
            if (*count < M_MAX_PARAMS) {
                params[*count] =
                    (M_PARAM) { start, (size_t)(text + i - start) };
                (*count)++;
            }
            start = text + i + 1;
            if (depth == -1) {
                return *count >= 2 ? i + 1 : 0;
            }
        }
    }
    return 0;
}

static uint32_t M_ParseFace(const M_PARAM *const command, uint32_t face)
{
    for (size_t i = 0; i < command->size; i++) {
        switch (command->start[i]) {
        case 'b':
            face |= TEXT_FACE_BOLD;
            break;
        case 'i':
            face |= TEXT_FACE_ITALIC;
            break;
        case 'u':
            face |= TEXT_FACE_UNDERLINE;
            break;
        default:
            break;
        }
    }
    return face;
}

static RESULT M_MeasurePiece(
    const TEXT_FONT *const font, const TEXT_STYLE *const style,
    M_LAYOUT *const layout, M_PIECE *const piece)
{
    TEXT_STYLE face = *style;
    face.face = piece->face;
    int32_t height;
    MUST(M_Measure(
        font, piece->text, piece->size, &face, &piece->width, &height));
    if (height > layout->height) {
        layout->height = height;
    }
    return OK;
}

// Places a command as thcrap's layout_parse_tabs does: the column ends at
// the width of the third parameter, or of the text without one, and l, c
// and r align the text in it. s draws nothing and gives no width.
static RESULT M_PlaceCommand(
    const TEXT_FONT *const font, const TEXT_STYLE *const style,
    M_LAYOUT *const layout, const M_PARAM *const params, const int32_t count,
    int32_t *const pen)
{
    M_PIECE piece = {
        .text = params[1].start,
        .size = params[1].size,
        .face = M_ParseFace(&params[0], style->face),
        .draw = true,
    };
    MUST(M_MeasurePiece(font, style, layout, &piece));
    int32_t column_end = *pen + piece.width;
    if (count > 2 && params[2].size > 0) {
        M_PIECE column = {
            .text = params[2].start,
            .size = params[2].size,
            .face = piece.face,
        };
        MUST(M_MeasurePiece(font, style, layout, &column));
        column_end = *pen + column.width;
    }
    int32_t x = *pen;
    bool aligned = false;
    for (size_t i = 0; i < params[0].size; i++) {
        switch (params[0].start[i]) {
        case 's':
            column_end = x;
            piece.draw = false;
            aligned = true;
            break;
        case 'l':
            aligned = true;
            break;
        case 'c':
            x += (column_end - x) / 2 - piece.width / 2;
            aligned = true;
            break;
        case 'r':
            x = column_end - piece.width;
            aligned = true;
            break;
        default:
            break;
        }
    }
    piece.x = x;
    Array_Append(&layout->pieces, piece);
    *pen = x + (aligned ? column_end - x : piece.width);
    return OK;
}

static RESULT M_Layout(
    const TEXT_FONT *const font, const char *const text,
    const TEXT_STYLE *const style, M_LAYOUT *const layout)
{
    *layout = (M_LAYOUT) {};
    Array_Init(&layout->pieces, &g_HeapAllocator);
    FAIL_IF(text == nullptr || style == nullptr, "Invalid text or style");
    const char *const note = strchr(text, M_NOTE_START);
    const size_t size = note != nullptr ? (size_t)(note - text) : strlen(text);
    int32_t pen = 0;
    size_t i = 0;
    // The text between two commands is one piece, so that it is measured
    // and drawn as one string.
    const char *run_start = nullptr;
    int32_t run_pen = 0;
    int32_t run_piece = -1;
    while (i < size) {
        M_PARAM params[M_MAX_PARAMS];
        int32_t count;
        const size_t matched = text[i] == '<'
            ? M_MatchMarkup(text + i, size - i, params, &count)
            : 0;
        if (matched > 0) {
            run_start = nullptr;
            run_piece = -1;
            MUST(M_PlaceCommand(font, style, layout, params, count, &pen));
            i += matched;
            continue;
        }
        const char *const next = memchr(text + i + 1, '<', size - i - 1);
        const size_t run =
            next != nullptr ? (size_t)(next - text) - i : size - i;
        if (run_start == nullptr) {
            run_start = text + i;
            run_pen = pen;
        }
        i += run;
        M_PIECE piece = {
            run_start, (size_t)(text + i - run_start), style->face, run_pen, 0,
            true,
        };
        MUST(M_MeasurePiece(font, style, layout, &piece));
        if (run_piece >= 0) {
            layout->pieces.items[run_piece] = piece;
        } else {
            run_piece = layout->pieces.count;
            Array_Append(&layout->pieces, piece);
        }
        pen = run_pen + piece.width;
    }
    layout->plain = note == nullptr && layout->pieces.count == 1
        && layout->pieces.items[0].text == text
        && layout->pieces.items[0].size == size;
    int32_t right = pen;
    for (int32_t j = 0; j < layout->pieces.count; j++) {
        const M_PIECE *const piece = &layout->pieces.items[j];
        if (piece->draw && piece->x + piece->width > right) {
            right = piece->x + piece->width;
        }
    }
    layout->width = right > 0 ? right : 0;
    return OK;
}

static RESULT M_DrawPieces(
    const TEXT_FONT *const font, const M_LAYOUT *const layout,
    const TEXT_STYLE *const style, IMAGE *const image, const int32_t x,
    const int32_t y, const GFX_COLOR color)
{
    for (int32_t i = 0; i < layout->pieces.count; i++) {
        const M_PIECE *const piece = &layout->pieces.items[i];
        if (!piece->draw || piece->size == 0) {
            continue;
        }
        M_SetFace(font, piece->face);
        SDL_Surface *const rendered = TTF_RenderText_Blended(
            font->font, piece->text, piece->size,
            (SDL_Color) { 255, 255, 255, 255 });
        FAIL_IF(rendered == nullptr, "Cannot render text: %s", SDL_GetError());
        SDL_Surface *const surface =
            SDL_ConvertSurface(rendered, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(rendered);
        FAIL_IF(
            surface == nullptr, "Cannot convert text pixels: %s",
            SDL_GetError());
        const double width = ceil(surface->w * (double)style->width_scale);
        if (width <= INT32_MAX) {
            M_Composite(image, surface, (int32_t)width, x + piece->x, y, color);
        }
        SDL_DestroySurface(surface);
    }
    return OK;
}

// Returns the size of the laid out line with the shadow of style.
static RESULT M_LayoutSize(
    const M_LAYOUT *const layout, const TEXT_STYLE *const style,
    int32_t *const width, int32_t *const height)
{
    *width = layout->width;
    *height = layout->height;
    if (*width != 0 && *height != 0 && style->shadow.alpha != 0) {
        const int64_t full_width = (int64_t)*width + llabs(style->shadow_x);
        const int64_t full_height = (int64_t)*height + llabs(style->shadow_y);
        FAIL_IF(
            full_width > INT32_MAX || full_height > INT32_MAX
                || full_width * full_height > INT32_MAX / 4,
            "Text shadow exceeds the image limit");
        *width = (int32_t)full_width;
        *height = (int32_t)full_height;
    }
    return OK;
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
    FAIL_IF(text == nullptr, "Invalid text or style");
    MUST(M_Measure(font, text, strlen(text), style, width, height));
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
    int32_t width = 0;
    int32_t height = 0;
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

RESULT Text_MeasureMarkup(
    const TEXT_FONT *const font, const char *const text,
    const TEXT_STYLE *const style, int32_t *const width, int32_t *const height)
{
    *width = 0;
    *height = 0;
    M_LAYOUT layout;
    RESULT result = M_Layout(font, text, style, &layout);
    if (IS_OK(result)) {
        result = layout.plain ? Text_Measure(font, text, style, width, height)
                              : M_LayoutSize(&layout, style, width, height);
    }
    Array_Free(&layout.pieces);
    return result;
}

RESULT Text_RenderMarkup(
    const TEXT_FONT *const font, const char *const text,
    const TEXT_STYLE *const style, const ALLOCATOR *const allocator,
    IMAGE *const out)
{
    *out = (IMAGE) {};
    FAIL_IF(allocator == nullptr, "Missing text image allocator");
    M_LAYOUT layout;
    RESULT result = M_Layout(font, text, style, &layout);
    int32_t width = 0;
    int32_t height = 0;
    if (IS_OK(result) && layout.plain) {
        Array_Free(&layout.pieces);
        return Text_Render(font, text, style, allocator, out);
    }
    if (IS_OK(result)) {
        result = M_LayoutSize(&layout, style, &width, &height);
    }
    if (!IS_OK(result) || width == 0 || height == 0) {
        Array_Free(&layout.pieces);
        return result;
    }
    IMAGE image = Image_Create(width, height, allocator);
    const bool shadow = style->shadow.alpha != 0;
    const int32_t origin_x =
        shadow && style->shadow_x < 0 ? -style->shadow_x : 0;
    const int32_t origin_y =
        shadow && style->shadow_y < 0 ? -style->shadow_y : 0;
    if (shadow) {
        result = M_DrawPieces(
            font, &layout, style, &image, origin_x + style->shadow_x,
            origin_y + style->shadow_y, style->shadow);
    }
    if (IS_OK(result)) {
        result = M_DrawPieces(
            font, &layout, style, &image, origin_x, origin_y, style->color);
    }
    Array_Free(&layout.pieces);
    if (!IS_OK(result)) {
        Image_Free(&image);
        return result;
    }
    *out = image;
    return OK;
}
