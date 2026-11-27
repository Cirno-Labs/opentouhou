#include "th02/dialogue/render.h"

#include "th02/clip.h"
#include "th02/draw.h"
#include "th02/grcg.h"
#include "th02/stage.h"
#include "th02/video.h"

#include "core/base/assert.h"

#include <string.h>

static void M_Tile(
    const uint8_t *const pixels, const int16_t left, const int16_t top)
{
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t row = 0; row < 16; row++) {
        const size_t offset = ((size_t)top + row) * 80 + left / 8;
        if (offset < VIDEO_PLANE_SIZE) {
            memcpy(page + offset * 8, pixels + row * 16, 16);
        } else {
            // Only blue and red stores reach another VRAM plane.
            uint8_t *const destination = page + (offset - VIDEO_PLANE_SIZE) * 8;
            for (size_t x = 0; x < 16; x++) {
                destination[x] =
                    (destination[x] & ~6u) | ((pixels[row * 16 + x] & 3u) << 1);
            }
        }
    }
}

static void M_Wipe(void *const context)
{
    DIALOGUE_RENDER *const render = context;
    for (uint16_t line = 0; line < 2; line++) {
        Tram_PutText(14, 21 + line, &render->assets->blank, UINT16_MAX, 0xe1);
    }
}

static void M_Face(void *const context, const int16_t image)
{
    DIALOGUE_RENDER *const render = context;
    ASSERT(g_Stage->scroll.line >= 0 && g_Stage->scroll.line < 400);
    int16_t top = 328 + g_Stage->scroll.line;
    if (top >= 400) {
        top -= 400;
    }
    ASSERT(g_Grcg->mode == 0);
    if (image == 255) {
        Grcg_SetColor(0);
        Draw_BoxFill(g_Clip, 40, top, 87, top + 47);
        Grcg_Off();
        return;
    }
    ASSERT(image >= 0 && image + 34 < render->assets->faces.count);
    for (size_t row = 0; row < 3; row++) {
        for (size_t column = 0; column < 3; column++) {
            const size_t tile = image + row * 16 + column;
            M_Tile(
                render->assets->faces.pixels + tile * 256,
                (int16_t)(40 + column * 16), top);
        }
        top += 16;
        if (top >= 400) {
            top -= 400;
        }
    }
}

static void M_Text(
    void *const context, const uint8_t box, const uint8_t line,
    const uint16_t bytes)
{
    DIALOGUE_RENDER *const render = context;
    ASSERT(box < render->assets->count && line < 2 && bytes <= 36);
    const TRAM_TEXT *const text =
        render->show_continues && box == render->continued_box && line == 0
        ? &render->continued_line
        : &render->assets->boxes[box].lines[line];
    Tram_PutText(14, 21 + line, text, bytes, 0xe1);
}

DIALOGUE_VIEW DialogueRender_View(DIALOGUE_RENDER *const render)
{
    return (DIALOGUE_VIEW) { M_Wipe, M_Face, M_Text, render };
}

void DialogueRender_Continues(
    DIALOGUE_RENDER *const render, const uint8_t box,
    const TRAM_CHARACTER digits[10], const uint16_t continues)
{
    ASSERT(continues > 0 && continues <= 99);
    ASSERT(box < render->assets->count);
    const TRAM_TEXT *const text = &render->assets->boxes[box].lines[0];
    ASSERT(text->count <= 36);
    memcpy(
        render->continued_characters, text->characters,
        (size_t)text->count * sizeof(*text->characters));
    unsigned replaced = 0;
    uint16_t previous = 0;
    for (int32_t i = 0; i < text->count; i++) {
        TRAM_CHARACTER *const character = &render->continued_characters[i];
        const uint16_t end = character->source_end;
        if (previous == 8 || previous == 10) {
            ASSERT(character->wide && end == previous + 2);
            const unsigned digit =
                previous == 8 ? continues / 10 : continues % 10;
            if (previous == 10 || digit != 0) {
                const uint32_t offset = character->text_offset;
                *character = digits[digit];
                character->source_end = end;
                character->text_offset = offset;
            }
            replaced++;
        }
        previous = end;
    }
    ASSERT(replaced == 2);
    render->continued_line = *text;
    render->continued_line.characters = render->continued_characters;
    render->continued_box = box;
    render->show_continues = true;
}
