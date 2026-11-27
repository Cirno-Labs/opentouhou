#include "core/text/private.h"

#include "core/base/memory.h"

#include <SDL3_ttf/SDL_textengine.h>

#include <limits.h>
#include <stdlib.h>

typedef struct {
    int offset;
    int x;
    int32_t adjust;
    int32_t shift;
    bool has_glyph;
} M_CLUSTER;

typedef struct {
    const FONT *font;
    TTF_Text *text;
    M_CLUSTER *clusters;
    int32_t count;
    int32_t width;
    int32_t height;
} M_RUN;

static TTF_TextEngine m_Engine = { .version = sizeof(TTF_TextEngine) };

static int32_t M_Adjust(const FONT *const font, const TTF_Font *const face)
{
    for (const FONT *f = font; f != nullptr; f = f->fallback) {
        if (f->font == face && (TTF_GetFontStyle(face) & TTF_STYLE_BOLD) != 0) {
            return f->bold_advance_adjust;
        }
    }
    return 0;
}

static bool M_NeedsAdjustment(const FONT *const font)
{
    for (const FONT *f = font; f != nullptr; f = f->fallback) {
        if (M_Adjust(font, f->font) != 0) {
            return true;
        }
    }
    return false;
}

static int M_CompareOffset(const void *const a, const void *const b)
{
    const M_CLUSTER *const left = a;
    const M_CLUSTER *const right = b;
    return (left->offset > right->offset) - (left->offset < right->offset);
}

static int M_ComparePosition(const void *const a, const void *const b)
{
    const M_CLUSTER *const left = a;
    const M_CLUSTER *const right = b;
    const int position = (left->x > right->x) - (left->x < right->x);
    return position != 0 ? position : M_CompareOffset(a, b);
}

static M_CLUSTER *M_Find(const M_RUN *const run, const int offset)
{
    const M_CLUSTER key = { .offset = offset };
    return bsearch(
        &key, run->clusters, (size_t)run->count, sizeof(M_CLUSTER),
        M_CompareOffset);
}

static void M_Close(M_RUN *const run)
{
    TTF_DestroyText(run->text);
    Mem_Free(run->clusters);
}

static RESULT M_Open(
    const FONT *const font, const char *const text, const size_t size,
    M_RUN *const run)
{
    *run = (M_RUN) { .font = font };
    run->text = TTF_CreateText(&m_Engine, font->font, text, size);
    FAIL_IF(run->text == nullptr, "Cannot lay out glyphs: %s", SDL_GetError());
    FAIL_IF(
        !TTF_SetTextWrapWhitespaceVisible(run->text, true),
        "Cannot retain text spacing: %s", SDL_GetError());
    FAIL_IF(
        !TTF_GetTextSize(run->text, &run->width, &run->height),
        "Cannot measure glyphs: %s", SDL_GetError());
    const TTF_TextData *const data = run->text->internal;
    run->clusters = Mem_AllocArr((size_t)data->num_clusters, sizeof(M_CLUSTER));
    for (int32_t i = 0; i < data->num_clusters; i++) {
        const TTF_SubString *const cluster = &data->clusters[i];
        if ((cluster->flags & (TTF_SUBSTRING_TEXT_END | TTF_SUBSTRING_LINE_END))
            == 0) {
            run->clusters[run->count++] = (M_CLUSTER) {
                .offset = cluster->offset,
                .x = cluster->rect.x,
            };
        }
    }
    qsort(
        run->clusters, (size_t)run->count, sizeof(M_CLUSTER), M_CompareOffset);
    for (int32_t i = 0; i < data->num_ops; i++) {
        const TTF_DrawOperation *const op = &data->ops[i];
        if (op->cmd != TTF_DRAW_COMMAND_COPY) {
            continue;
        }
        M_CLUSTER *const cluster = M_Find(run, op->copy.text_offset);
        FAIL_IF(cluster == nullptr, "Glyph has no text cluster");
        cluster->adjust += M_Adjust(font, op->copy.glyph_font);
        cluster->has_glyph = true;
    }
    // Spaces have a shaped advance but no bitmap operation.
    for (int32_t i = 0; i < run->count; i++) {
        M_CLUSTER *const cluster = &run->clusters[i];
        if (cluster->has_glyph) {
            continue;
        }
        const char *cursor = text + cluster->offset;
        size_t remaining = size - (size_t)cluster->offset;
        const Uint32 codepoint = SDL_StepUTF8(&cursor, &remaining);
        for (const FONT *f = font; f != nullptr; f = f->fallback) {
            if (TTF_FontHasGlyph(f->font, codepoint)) {
                cluster->adjust = M_Adjust(font, f->font);
                break;
            }
        }
    }
    qsort(
        run->clusters, (size_t)run->count, sizeof(M_CLUSTER),
        M_ComparePosition);
    int64_t shift = 0;
    for (int32_t i = 0; i < run->count; i++) {
        FAIL_IF(
            shift < INT32_MIN || shift > INT32_MAX,
            "Glyph spacing exceeds the image limit");
        run->clusters[i].shift = (int32_t)shift;
        shift += run->clusters[i].adjust;
    }
    int64_t width = run->width + shift;
    qsort(
        run->clusters, (size_t)run->count, sizeof(M_CLUSTER), M_CompareOffset);
    for (int32_t i = 0; i < data->num_ops; i++) {
        const TTF_DrawOperation *const op = &data->ops[i];
        if (op->cmd == TTF_DRAW_COMMAND_COPY) {
            const M_CLUSTER *const cluster = M_Find(run, op->copy.text_offset);
            const int64_t right =
                (int64_t)op->copy.dst.x + cluster->shift + op->copy.dst.w;
            width = right > width ? right : width;
        }
    }
    FAIL_IF(
        width < 0 || width > INT32_MAX,
        "Glyph spacing exceeds the image limit");
    run->width = (int32_t)width;
    return OK;
}

static RESULT M_Render(
    const M_RUN *const run, const bool gdi, SDL_Surface **const out)
{
    SDL_Surface *const surface =
        SDL_CreateSurface(run->width, run->height, SDL_PIXELFORMAT_RGBA32);
    FAIL_IF(
        surface == nullptr, "Cannot create text pixels: %s", SDL_GetError());
    const TTF_TextData *const data = run->text->internal;
    RESULT result = OK;
    for (int32_t i = 0; i < data->num_ops; i++) {
        const TTF_DrawOperation *const op = &data->ops[i];
        if (op->cmd == TTF_DRAW_COMMAND_FILL) {
            SDL_Rect rect = op->fill.rect;
            rect.w += run->width - data->w;
            if (!SDL_FillSurfaceRect(
                    surface, &rect,
                    SDL_MapSurfaceRGBA(surface, 255, 255, 255, 255))) {
                result =
                    FAIL("Cannot draw text decoration: %s", SDL_GetError());
                break;
            }
        } else if (op->cmd == TTF_DRAW_COMMAND_COPY) {
            SDL_Surface *glyph;
            int32_t offset_x = 0;
            int32_t offset_y = 0;
            if (gdi) {
                const FONT *font = run->font;
                while (font != nullptr && font->font != op->copy.glyph_font) {
                    font = font->fallback;
                }
                result = font != nullptr ? TextRaster_Glyph(
                                               font, op->copy.glyph_index,
                                               &glyph, &offset_x, &offset_y)
                                         : FAIL("Glyph has no loaded font");
                if (!IS_OK(result)) {
                    break;
                }
                if (glyph == nullptr) {
                    continue;
                }
            } else {
                glyph = TTF_GetGlyphImageForIndex(
                    op->copy.glyph_font, op->copy.glyph_index, nullptr);
            }
            if (glyph == nullptr) {
                result = FAIL("Cannot render glyph: %s", SDL_GetError());
                break;
            }
            SDL_Rect dest = op->copy.dst;
            dest.x += M_Find(run, op->copy.text_offset)->shift;
            dest.x += offset_x;
            dest.y += offset_y;
            const bool drawn = SDL_BlitSurface(
                glyph, gdi ? nullptr : &op->copy.src, surface, &dest);
            SDL_DestroySurface(glyph);
            if (!drawn) {
                result = FAIL("Cannot draw glyph: %s", SDL_GetError());
                break;
            }
        }
    }
    if (!IS_OK(result)) {
        SDL_DestroySurface(surface);
        return result;
    }
    *out = surface;
    return OK;
}

RESULT TextGlyphs_Measure(
    const FONT *const font, const char *const text, const size_t size,
    const bool gdi, int32_t *const width, int32_t *const height)
{
    if (gdi && TextRaster_IsSimple(font, text, size)) {
        return TextRaster_MeasureSimple(font, text, size, width, height);
    }
    if (!M_NeedsAdjustment(font)) {
        FAIL_IF(
            !TTF_GetStringSize(font->font, text, size, width, height),
            "Cannot measure text: %s", SDL_GetError());
        return OK;
    }
    M_RUN run;
    const RESULT result = M_Open(font, text, size, &run);
    if (IS_OK(result)) {
        *width = run.width;
        *height = run.height;
    }
    M_Close(&run);
    return result;
}

RESULT TextGlyphs_Render(
    const FONT *const font, const char *const text, const size_t size,
    const bool gdi, SDL_Surface **const out)
{
    *out = nullptr;
    if (gdi && TextRaster_IsSimple(font, text, size)) {
        return TextRaster_RenderSimple(font, text, size, out);
    }
    if (!gdi && !M_NeedsAdjustment(font)) {
        SDL_Surface *const rendered = TTF_RenderText_Blended(
            font->font, text, size, (SDL_Color) { 255, 255, 255, 255 });
        FAIL_IF(rendered == nullptr, "Cannot render text: %s", SDL_GetError());
        *out = SDL_ConvertSurface(rendered, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(rendered);
        FAIL_IF(
            *out == nullptr, "Cannot convert text pixels: %s", SDL_GetError());
        return OK;
    }
    M_RUN run;
    RESULT result = M_Open(font, text, size, &run);
    if (IS_OK(result)) {
        result = M_Render(&run, gdi, out);
    }
    M_Close(&run);
    return result;
}
