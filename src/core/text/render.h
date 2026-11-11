#pragma once

#include "core/gfx/types.h"
#include "core/image.h"
#include "core/text/font.h"

// Selects the faces of a TEXT_STYLE, combined with '|'. Bold on a font
// that is not bold emboldens its glyphs, as GDI does for a heavier weight;
// bold on a bold font changes nothing. Italic on a font that is not italic
// slants its glyphs.
typedef enum {
    TEXT_FACE_BOLD = 1 << 0,
    TEXT_FACE_ITALIC = 1 << 1,
    TEXT_FACE_UNDERLINE = 1 << 2,
} TEXT_FACE;

typedef struct {
    float width_scale;
    // The TEXT_FACE flags; 0 for the font as it is.
    uint32_t face;
    GFX_COLOR color;
    GFX_COLOR shadow;
    int32_t shadow_x;
    int32_t shadow_y;
} TEXT_STYLE;

// Returns opaque white text at its natural width, without a shadow.
TEXT_STYLE TextStyle_Default(void);

// Measures one UTF-8 line, including the shadow. An empty line has no pixels.
RESULT Text_Measure(
    const TEXT_FONT *font, const char *text, const TEXT_STYLE *style,
    int32_t *width, int32_t *height);

// Renders one UTF-8 line to straight-alpha RGBA8. Owns the returned image
// through allocator. Leaves out empty on failure or for an empty line.
RESULT Text_Render(
    const TEXT_FONT *font, const char *text, const TEXT_STYLE *style,
    const ALLOCATOR *allocator, IMAGE *out);

// Measures one UTF-8 line of thcrap's layout markup, including the shadow,
// as Text_RenderMarkup draws it. A line without markup measures as
// Text_Measure measures it.
RESULT Text_MeasureMarkup(
    const TEXT_FONT *font, const char *text, const TEXT_STYLE *style,
    int32_t *width, int32_t *height);

// Renders one UTF-8 line of thcrap's layout markup to straight-alpha RGBA8,
// as thcrap's layout_process lays it out. A command is
// "<flags$text$column>", where the column is optional:
// - b, i and u draw the text in bold, italic and underlined faces;
// - l, c and r align the text left, centered and right in a column as wide
//   as the column text in the same faces, or as the text itself without a
//   column, and the line goes on after the column;
// - s draws nothing and takes no width.
// A command needs a text and its closing '>'; without them, the '<' is
// text. The column text is measured, never drawn, and markup inside the
// parameters is text. An empty column is as wide as the text. Text from
// U+0014 on is a translator note and is left out. A line without markup
// renders as Text_Render renders it. Owns the returned image through
// allocator. Leaves out empty on failure or for an empty line.
RESULT Text_RenderMarkup(
    const TEXT_FONT *font, const char *text, const TEXT_STYLE *style,
    const ALLOCATOR *allocator, IMAGE *out);
