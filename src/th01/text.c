#include "th01/text.h"

#include "core/base/array.h"
#include "core/base/assert.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef ARRAY(GAME_TEXT_GLYPH) GLYPH_ARRAY;

static bool M_Scalar(const uint32_t value)
{
    return value <= 0x10ffff && (value < 0xd800 || value > 0xdfff);
}

static uint32_t M_Normalize(const uint32_t value)
{
    return value < 0x20 || (value >= 0x7f && value <= 0x9f) ? 0x20 : value;
}

static uint8_t M_Width(const uint32_t value)
{
    return value < 0x80 || (value >= 0xff61 && value <= 0xff9f) ? 8 : 16;
}

static size_t M_Decode(
    const char *const text, const size_t size, uint32_t *const value)
{
    if (size == 0) {
        return 0;
    }
    const uint8_t first = (uint8_t)text[0];
    if (first < 0x80) {
        *value = M_Normalize(first);
        return 1;
    }
    const size_t length = first >= 0xc2 && first <= 0xdf ? 2
        : first >= 0xe0 && first <= 0xef                 ? 3
        : first >= 0xf0 && first <= 0xf4                 ? 4
                                                         : 0;
    if (length == 0 || length > size) {
        return 0;
    }
    uint32_t result = first & (0x7f >> length);
    for (size_t i = 1; i < length; i++) {
        const uint8_t byte = (uint8_t)text[i];
        if ((byte & 0xc0) != 0x80) {
            return 0;
        }
        result = (result << 6) | (byte & 0x3f);
    }
    if (!M_Scalar(result) || (length == 2 && result < 0x80)
        || (length == 3 && result < 0x800)
        || (length == 4 && result < 0x10000)) {
        return 0;
    }
    *value = M_Normalize(result);
    return length;
}

static int M_Compare(const void *const first, const void *const second)
{
    const uint32_t a = ((const GAME_TEXT_GLYPH *)first)->codepoint;
    const uint32_t b = ((const GAME_TEXT_GLYPH *)second)->codepoint;
    return (a > b) - (a < b);
}

static RESULT M_Characters(const char *const text, GLYPH_ARRAY *const glyphs)
{
    const size_t size = strlen(text);
    for (size_t offset = 0; offset < size;) {
        uint32_t codepoint;
        const size_t length =
            M_Decode(text + offset, size - offset, &codepoint);
        FAIL_IF(length == 0, "Invalid UTF-8 at text offset %zu", offset);
        const GAME_TEXT_GLYPH glyph = { .codepoint = codepoint,
                                        .width = M_Width(codepoint) };
        Array_Append(glyphs, glyph);
        offset += length;
    }
    if (glyphs->count > 1) {
        qsort(
            glyphs->items, (size_t)glyphs->count, sizeof(*glyphs->items),
            M_Compare);
    }
    int32_t count = 0;
    for (int32_t i = 0; i < glyphs->count; i++) {
        if (count == 0
            || glyphs->items[i].codepoint
                != glyphs->items[count - 1].codepoint) {
            glyphs->items[count++] = glyphs->items[i];
        }
    }
    glyphs->count = count;
    return OK;
}

static void M_Copy(const FONT_ROM *const rom, GAME_TEXT_GLYPH *const glyph)
{
    uint8_t width;
    if (!FontRom_Glyph(rom, glyph->codepoint, glyph->rows, &width)) {
        LOG_WARNING("The font ROM has no cell for U+%04X", glyph->codepoint);
    }
}

static void M_Style(const GAME_TEXT_STYLE *const style)
{
    ASSERT(
        style != nullptr && style->weight >= GAME_TEXT_NORMAL
        && style->weight <= GAME_TEXT_BLACK && style->spacing >= 0
        && style->spacing <= 7);
}

static uint16_t M_Weight(uint16_t row, const GAME_TEXT_WEIGHT weight)
{
    if (weight == GAME_TEXT_HEAVY || weight == GAME_TEXT_BLACK) {
        row |= (uint16_t)(row << 1);
    }
    if (weight == GAME_TEXT_BOLD || weight == GAME_TEXT_BLACK) {
        const uint16_t before = row;
        row |= (uint16_t)(row << 1);
        const uint16_t added = before ^ row;
        row &= (uint16_t)~(added << 1);
    }
    return row;
}

static void M_Fill(
    int64_t left, int64_t top, int64_t right, int64_t bottom,
    const uint8_t color)
{
    left = left < 0 ? 0 : left;
    top = top < 0 ? 0 : top;
    right = right >= VIDEO_WIDTH ? VIDEO_WIDTH - 1 : right;
    bottom = bottom >= VIDEO_HEIGHT ? VIDEO_HEIGHT - 1 : bottom;
    if (left <= right && top <= bottom) {
        Video_FillBox(
            (int32_t)left, (int32_t)top, (int32_t)right, (int32_t)bottom,
            color);
    }
}

RESULT GameText_LoadGlyphs(
    const GAME_TEXT_GLYPH *const glyphs, const int32_t count,
    const ALLOCATOR *const allocator, GAME_TEXT_FONT *const out)
{
    *out = (GAME_TEXT_FONT) {};
    FAIL_IF(
        count < 0 || (count != 0 && glyphs == nullptr),
        "Invalid graphics glyph array");
    for (int32_t i = 0; i < count; i++) {
        FAIL_IF(
            !M_Scalar(glyphs[i].codepoint) || glyphs[i].codepoint == 0
                || glyphs[i].width != M_Width(glyphs[i].codepoint),
            "Invalid graphics glyph %d", i);
        for (int32_t y = 0; y < 16; y++) {
            FAIL_IF(
                glyphs[i].width == 8 && (glyphs[i].rows[y] & 0xff) != 0,
                "Halfwidth glyph %d has pixels outside its cell", i);
        }
        FAIL_IF(
            M_Normalize(glyphs[i].codepoint) != glyphs[i].codepoint,
            "Control glyph %d must use the space code point", i);
    }
    if (count == 0) {
        return OK;
    }
    const size_t size = Mem_ArrSize((size_t)count, sizeof(*glyphs));
    GAME_TEXT_GLYPH *const sorted = Mem_Dup(glyphs, size);
    qsort(sorted, (size_t)count, sizeof(*sorted), M_Compare);
    for (int32_t i = 1; i < count; i++) {
        if (sorted[i - 1].codepoint == sorted[i].codepoint) {
            Mem_Free(sorted);
            return FAIL("Duplicate graphics glyph");
        }
    }
    GAME_TEXT_GLYPH *const copied = Allocator_Alloc(allocator, size);
    memcpy(copied, sorted, size);
    Mem_Free(sorted);
    *out = (GAME_TEXT_FONT) { copied, count };
    return OK;
}

RESULT GameText_Load(
    const FONT_ROM *const rom, const char *const characters,
    const ALLOCATOR *const allocator, GAME_TEXT_FONT *const out)
{
    *out = (GAME_TEXT_FONT) {};
    FAIL_IF(
        rom == nullptr || characters == nullptr,
        "Missing font ROM or characters");
    GLYPH_ARRAY glyphs;
    Array_Init(&glyphs, &g_HeapAllocator);
    RESULT result = M_Characters(characters, &glyphs);
    for (int32_t i = 0; IS_OK(result) && i < glyphs.count; i++) {
        M_Copy(rom, &glyphs.items[i]);
    }
    if (IS_OK(result) && glyphs.count != 0) {
        const size_t size =
            Mem_ArrSize((size_t)glyphs.count, sizeof(*glyphs.items));
        GAME_TEXT_GLYPH *const copied = Allocator_Alloc(allocator, size);
        memcpy(copied, glyphs.items, size);
        *out = (GAME_TEXT_FONT) { copied, glyphs.count };
    }
    Array_Free(&glyphs);
    return result;
}

const GAME_TEXT_GLYPH *GameText_Find(
    const GAME_TEXT_FONT *const font, const uint32_t codepoint)
{
    const uint32_t normalized = M_Normalize(codepoint);
    int32_t left = 0;
    int32_t right = font->count;
    while (left < right) {
        const int32_t middle = left + (right - left) / 2;
        const GAME_TEXT_GLYPH *const glyph = &font->glyphs[middle];
        if (glyph->codepoint == normalized) {
            return glyph;
        }
        if (glyph->codepoint < normalized) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return nullptr;
}

static int32_t M_Measure(
    const GAME_TEXT_FONT *const font, const char *const text,
    const GAME_TEXT_STYLE *const style)
{
    ASSERT(font != nullptr && text != nullptr);
    M_Style(style);
    const size_t size = strlen(text);
    int64_t extent = 0;
    for (size_t offset = 0; offset < size;) {
        uint32_t codepoint;
        const size_t length =
            M_Decode(text + offset, size - offset, &codepoint);
        ASSERT(length != 0);
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(font, codepoint);
        extent += (glyph != nullptr ? glyph->width : M_Width(codepoint))
            + style->spacing;
        ASSERT(extent - style->spacing <= INT32_MAX);
        offset += length;
    }
    return size == 0 ? 0 : (int32_t)(extent - style->spacing);
}

void GameText_Draw(
    const GAME_TEXT_FONT *const font, const char *const text,
    const int32_t left, const int32_t top, const uint8_t color,
    const GAME_TEXT_STYLE *const style)
{
    const int32_t width = M_Measure(font, text, style);
    if (width == 0) {
        return;
    }
    if (style->clear_background) {
        M_Fill(
            left, top, (int64_t)left + width - 1,
            (int64_t)top + (style->underline ? 17 : 15), 0);
    }
    if (style->underline) {
        M_Fill(
            left, (int64_t)top + 17, (int64_t)left + width - 1,
            (int64_t)top + 17, 7);
    }
    const size_t size = strlen(text);
    int64_t pen = left;
    for (size_t offset = 0; offset < size;) {
        uint32_t codepoint;
        const size_t length =
            M_Decode(text + offset, size - offset, &codepoint);
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(font, codepoint);
        const uint8_t cell_width =
            glyph != nullptr ? glyph->width : M_Width(codepoint);
        if (pen > VIDEO_WIDTH - cell_width) {
            break;
        }
        for (int32_t y = 0; y < 16; y++) {
            uint16_t row =
                M_Weight(glyph != nullptr ? glyph->rows[y] : 0, style->weight);
            if (style->reverse) {
                row ^= cell_width == 8 ? 0xff00 : 0xffff;
            }
            const int64_t py = (int64_t)top + y;
            if (py >= 0 && py < VIDEO_HEIGHT) {
                for (int32_t x = 0; x < cell_width; x++) {
                    const int64_t px = pen + x;
                    if ((row & (0x8000u >> x)) != 0 && px >= 0
                        && px < VIDEO_WIDTH) {
                        Video_Pixel((int32_t)px, (int32_t)py, color);
                    }
                }
            }
        }
        pen += cell_width + style->spacing;
        offset += length;
    }
}
