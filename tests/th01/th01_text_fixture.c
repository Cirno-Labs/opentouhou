#include "tests/th01/th01_text_fixture.h"

#include "th01/text_private.h"

#include "core/base/memory.h"

#include <stdlib.h>
#include <string.h>

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
            !Text_Scalar(glyphs[i].codepoint) || glyphs[i].codepoint == 0
                || glyphs[i].width != Text_Width(glyphs[i].codepoint),
            "Invalid graphics glyph %d", i);
        for (int32_t y = 0; y < 16; y++) {
            FAIL_IF(
                glyphs[i].width == 8 && (glyphs[i].rows[y] & 0xff) != 0,
                "Halfwidth glyph %d has pixels outside its cell", i);
        }
        FAIL_IF(
            Text_Normalize(glyphs[i].codepoint) != glyphs[i].codepoint,
            "Control glyph %d must use the space code point", i);
    }
    if (count == 0) {
        return OK;
    }
    const size_t size = Mem_ArrSize((size_t)count, sizeof(*glyphs));
    GAME_TEXT_GLYPH *const sorted = Mem_Dup(glyphs, size);
    qsort(sorted, (size_t)count, sizeof(*sorted), Text_Compare);
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
