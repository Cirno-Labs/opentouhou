#include "th06/note.h"

#include "th06/translation.h"

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/gfx.h"
#include "core/json.h"
#include "core/patch.h"
#include "core/text.h"

#include <stdlib.h>
#include <string.h>

#define M_NOTE_START '\x14'
#define M_SCREEN_WIDTH 640.0f
#define M_SCREEN_HEIGHT 480.0f
#define M_TICKS_PER_SECOND 60u
#define M_ARENA_CAPACITY (16 * 1024 * 1024)
#define M_FACE_SIZE 64
#define M_MAX_OUTLINE_RADIUS 15
// Holds the height of the System font, which GDI draws without a font.
#define M_DEFAULT_FONT_HEIGHT 16
#define M_MAX_FONT_HEIGHT 4096
#define M_BOLD_WEIGHT 700

typedef enum {
    M_VALIGN_TOP,
    M_VALIGN_CENTER,
    M_VALIGN_BOTTOM,
} M_VALIGN;

typedef struct {
    float reference_width, reference_height;
    float left, top, width, height;
    int32_t outline_radius;
    float fade_ms;
    float read_speed;
    M_VALIGN valign;
} M_SETTINGS;

typedef struct {
    char *text;
    size_t size;
    GFX_TEXTURE *texture; // nullptr until the note is first drawn
    bool failed; // the note could not be drawn
} M_NOTE;

typedef ARRAY(M_NOTE) M_NOTE_ARRAY;

typedef struct {
    size_t start;
    size_t size;
    int32_t width;
} M_LINE;

typedef ARRAY(M_LINE) M_LINE_ARRAY;

static bool m_Enabled;
static M_SETTINGS m_Settings;
static TEXT_FONT *m_Font;
static int32_t m_LineHeight;
static TEXT_STYLE m_Style;
static M_NOTE_ARRAY m_Notes;
static int32_t m_Shown = -1;
static int32_t m_Drawn = -1;
static uint32_t m_Ticks; // since the shown note started to fade in

static const M_SETTINGS m_Defaults = {
    .outline_radius = 2,
    .fade_ms = 500.0f,
    .read_speed = 35.0f,
    .valign = M_VALIGN_BOTTOM,
};

static bool M_ReadPair(
    const JSON_VALUE *const value, float *const x, float *const y)
{
    if (value == nullptr || value->type != JSON_ARRAY || value->array.count != 2
        || Json_At(value, 0)->type != JSON_NUMBER
        || Json_At(value, 1)->type != JSON_NUMBER) {
        return false;
    }
    *x = (float)Json_GetNumber(Json_At(value, 0), 0.0);
    *y = (float)Json_GetNumber(Json_At(value, 1), 0.0);
    return true;
}

static void M_ReadRegion(const JSON_VALUE *const notes)
{
    float x, y;
    const JSON_VALUE *value = Json_Get(notes, "reference_resolution");
    if (value != nullptr) {
        if (M_ReadPair(value, &x, &y) && x > 0.0f && y > 0.0f) {
            m_Settings.reference_width = x;
            m_Settings.reference_height = y;
        } else {
            LOG_WARNING("tlnotes: reference_resolution must be positive");
        }
    }
    value = Json_Get(notes, "region_topleft");
    if (value != nullptr) {
        if (M_ReadPair(value, &x, &y)) {
            m_Settings.left = x;
            m_Settings.top = y;
        } else {
            LOG_WARNING("tlnotes: region_topleft must be two numbers");
        }
    }
    value = Json_Get(notes, "region_size");
    if (value != nullptr) {
        if (M_ReadPair(value, &x, &y) && x > 0.0f && y > 0.0f
            && x < m_Settings.reference_width
            && y < m_Settings.reference_height) {
            m_Settings.width = x;
            m_Settings.height = y;
        } else {
            LOG_WARNING(
                "tlnotes: region_size must be positive and smaller than the "
                "reference resolution");
        }
    }
}

static float M_ReadPositive(
    const JSON_VALUE *const notes, const char *const key, const float fallback)
{
    const JSON_VALUE *const value = Json_Get(notes, key);
    if (value == nullptr) {
        return fallback;
    }
    const double number = Json_GetNumber(value, 0.0);
    if (value->type != JSON_NUMBER || number <= 0.0) {
        LOG_WARNING("tlnotes: %s must be a positive number", key);
        return fallback;
    }
    return (float)number;
}

static void M_ReadLook(const JSON_VALUE *const notes)
{
    const JSON_VALUE *value = Json_Get(notes, "outline_radius");
    if (value != nullptr) {
        const int64_t radius = Json_GetInt(value, -1);
        if (radius >= 0 && radius <= M_MAX_OUTLINE_RADIUS) {
            m_Settings.outline_radius = (int32_t)radius;
        } else {
            LOG_WARNING(
                "tlnotes: outline_radius must be an integer from 0 to %d",
                M_MAX_OUTLINE_RADIUS);
        }
    }
    m_Settings.fade_ms = M_ReadPositive(notes, "fade_ms", m_Settings.fade_ms);
    m_Settings.read_speed =
        M_ReadPositive(notes, "read_speed", m_Settings.read_speed);
    value = Json_Get(notes, "valign");
    if (value != nullptr) {
        const char *const valign = Json_GetString(value, "");
        if (strcmp(valign, "top") == 0) {
            m_Settings.valign = M_VALIGN_TOP;
        } else if (strcmp(valign, "center") == 0) {
            m_Settings.valign = M_VALIGN_CENTER;
        } else if (strcmp(valign, "bottom") == 0) {
            m_Settings.valign = M_VALIGN_BOTTOM;
        } else {
            LOG_WARNING("tlnotes: valign must be top, center or bottom");
        }
    }
}

// Copies the argument of a font rule at text into arg, as thcrap's
// fontrule_arg reads it: up to a space, or between single quotes. Returns
// text advanced to the next argument.
static const char *M_ReadArg(
    const char *text, char *const arg, const size_t arg_size)
{
    char delimiter = ' ';
    if (*text == '\'') {
        delimiter = '\'';
        text++;
    }
    size_t i = 0;
    while (i + 1 < arg_size && *text != '\0' && *text != delimiter) {
        arg[i++] = *text++;
    }
    arg[i] = '\0';
    while (*text != '\0' && *text != ' ') {
        text++;
    }
    while (*text == ' ') {
        text++;
    }
    return text;
}

static int32_t M_ReadNumber(const char *const arg)
{
    const long value = strtol(arg, nullptr, 10);
    if (value < -M_MAX_FONT_HEIGHT || value > M_MAX_FONT_HEIGHT) {
        return 0;
    }
    return (int32_t)value;
}

// Opens the font of the font rule, or of no rule. A positive height is a
// cell height, as in GDI; a negative one is taken as a cell height too.
static RESULT M_LoadFont(const char *const rule)
{
    char face[M_FACE_SIZE] = "";
    int32_t height = M_DEFAULT_FONT_HEIGHT;
    int32_t weight = 0;
    const char *family = nullptr;
    if (rule != nullptr) {
        char arg[M_FACE_SIZE];
        const char *text = rule;
        if (*text == '\'') {
            text = M_ReadArg(text, face, sizeof(face));
        } else {
            text = M_ReadArg(text, arg, sizeof(arg));
        }
        text = M_ReadArg(text, arg, sizeof(arg));
        height = abs(M_ReadNumber(arg));
        text = M_ReadArg(text, arg, sizeof(arg));
        M_ReadArg(text, arg, sizeof(arg));
        weight = M_ReadNumber(arg);
        if (height == 0) {
            height = M_DEFAULT_FONT_HEIGHT;
        }
        family = Translation_GetFont();
        if (family == nullptr && face[0] != '\0') {
            family = face;
        }
    }
    m_LineHeight = height;
    m_Style = TextStyle_Default();
    m_Style.face = weight >= M_BOLD_WEIGHT ? TEXT_FACE_BOLD : 0;
    if (family != nullptr) {
        int32_t count;
        const char *const *const paths = Translation_GetFontFiles(&count);
        if (Result_Log(
                TextFont_LoadFamily(
                    paths, count, family, (float)height, &m_Font),
                LOG_LEVEL_WARNING)) {
            return OK;
        }
    }
    return TextFont_LoadDefault((float)height, &m_Font);
}

static RESULT M_Measure(
    const char *const text, const size_t size, int32_t *const width)
{
    char *const span = Mem_Alloc(size + 1);
    memcpy(span, text, size);
    span[size] = '\0';
    int32_t height;
    const RESULT result = Text_Measure(m_Font, span, &m_Style, width, &height);
    Mem_Free(span);
    return result;
}

static size_t M_SkipSpaces(const char *const text, size_t at, const size_t end)
{
    while (at < end && text[at] == ' ') {
        at++;
    }
    return at;
}

static size_t M_SkipWord(const char *const text, size_t at, const size_t end)
{
    while (at < end && text[at] != ' ') {
        at++;
    }
    return at;
}

// Breaks the paragraph from start to end into lines, as GDI's DrawText
// does with DT_WORDBREAK: each line takes as many words as fit in the
// region width, and at least one. The spaces at a break belong to no line.
static RESULT M_BreakParagraph(
    const char *const text, size_t start, const size_t end,
    M_LINE_ARRAY *const lines)
{
    for (;;) {
        size_t taken = M_SkipWord(text, start, end);
        int32_t width;
        MUST(M_Measure(text + start, taken - start, &width));
        while (taken < end) {
            const size_t next =
                M_SkipWord(text, M_SkipSpaces(text, taken, end), end);
            int32_t next_width;
            MUST(M_Measure(text + start, next - start, &next_width));
            if ((float)next_width > m_Settings.width) {
                break;
            }
            taken = next;
            width = next_width;
        }
        Array_Append(lines, ((M_LINE) { start, taken - start, width }));
        start = M_SkipSpaces(text, taken, end);
        if (start >= end) {
            return OK;
        }
    }
}

static RESULT M_BreakLines(const M_NOTE *const note, M_LINE_ARRAY *const lines)
{
    size_t start = 0;
    for (;;) {
        const char *const newline =
            memchr(note->text + start, '\n', note->size - start);
        const size_t end =
            newline != nullptr ? (size_t)(newline - note->text) : note->size;
        MUST(M_BreakParagraph(note->text, start, end, lines));
        if (newline == nullptr) {
            return OK;
        }
        start = end + 1;
    }
}

// Copies the coverage of the glyphs of line to coverage, a width-wide
// buffer, at x, y. The glyphs are clipped to the text, which ends at
// right and bottom.
static RESULT M_RenderLine(
    const M_NOTE *const note, const M_LINE *const line, uint8_t *const coverage,
    const int32_t width, const int32_t right, const int32_t bottom,
    const int32_t x, const int32_t y)
{
    char *const span = Mem_Alloc(line->size + 1);
    memcpy(span, note->text + line->start, line->size);
    span[line->size] = '\0';
    IMAGE glyphs;
    const RESULT result =
        Text_Render(m_Font, span, &m_Style, &g_HeapAllocator, &glyphs);
    Mem_Free(span);
    MUST(result);
    for (int32_t row = 0; row < glyphs.height && y + row < bottom; row++) {
        for (int32_t column = 0; column < glyphs.width && x + column < right;
             column++) {
            coverage[(size_t)(y + row) * (size_t)width + (size_t)(x + column)] =
                glyphs.pixels
                    [((size_t)row * (size_t)glyphs.width + (size_t)column) * 4
                     + 3];
        }
    }
    Image_Free(&glyphs);
    return OK;
}

// Turns coverage into the note's pixels, as thcrap does: the colour is
// the coverage of white text on black, and the alpha is the largest
// coverage within the outline radius.
static void M_Outline(const uint8_t *const coverage, IMAGE *const image)
{
    const int32_t radius = m_Settings.outline_radius;
    for (int32_t y = 0; y < image->height; y++) {
        for (int32_t x = 0; x < image->width; x++) {
            const uint8_t value =
                coverage[(size_t)y * (size_t)image->width + (size_t)x];
            if (value == 0) {
                continue;
            }
            uint8_t *const pixel = image->pixels
                + ((size_t)y * (size_t)image->width + (size_t)x) * 4;
            pixel[0] = value;
            pixel[1] = value;
            pixel[2] = value;
            for (int32_t oy = -radius; oy <= radius; oy++) {
                for (int32_t ox = -radius; ox <= radius; ox++) {
                    if (ox * ox + oy * oy > radius * radius) {
                        continue;
                    }
                    uint8_t *const alpha = image->pixels
                        + ((size_t)(y + oy) * (size_t)image->width
                           + (size_t)(x + ox))
                            * 4
                        + 3;
                    if (*alpha < value) {
                        *alpha = value;
                    }
                }
            }
        }
    }
}

static RESULT M_RenderLines(M_NOTE *const note, const M_LINE_ARRAY *const lines)
{
    int32_t text_width = 0;
    for (int32_t i = 0; i < lines->count; i++) {
        if (lines->items[i].width > text_width) {
            text_width = lines->items[i].width;
        }
    }
    const int32_t radius = m_Settings.outline_radius;
    const int32_t width = text_width + radius * 2;
    const int32_t height = lines->count * m_LineHeight + radius * 2;
    uint8_t *const coverage = Mem_AllocZ((size_t)width * (size_t)height);
    RESULT result = OK;
    for (int32_t i = 0; i < lines->count && IS_OK(result); i++) {
        const M_LINE *const line = &lines->items[i];
        result = M_RenderLine(
            note, line, coverage, width, width - radius, height - radius,
            radius + (text_width - line->width) / 2, radius + i * m_LineHeight);
    }
    if (IS_OK(result)) {
        IMAGE image = Image_Create(width, height, &g_HeapAllocator);
        M_Outline(coverage, &image);
        result = GfxTexture_Create(&image, &note->texture);
        Image_Free(&image);
    }
    Mem_Free(coverage);
    return result;
}

static RESULT M_Render(M_NOTE *const note)
{
    M_LINE_ARRAY lines;
    Array_Init(&lines, &g_HeapAllocator);
    RESULT result = M_BreakLines(note, &lines);
    if (IS_OK(result)) {
        result = M_RenderLines(note, &lines);
    }
    Array_Free(&lines);
    return result;
}

static void M_DrawNote(const M_NOTE *const note, const uint32_t age)
{
    const float texture_width = (float)GfxTexture_GetWidth(note->texture);
    const float texture_height = (float)GfxTexture_GetHeight(note->texture);
    float x = m_Settings.left;
    float y = m_Settings.top;
    float width = m_Settings.width;
    float height = m_Settings.height;
    const float margin = width - texture_width;
    x += margin / 2.0f;
    width -= margin;
    float v = 0.0f;
    float v_size = 1.0f;
    if (texture_height > height) {
        const float ms_per_byte = 1000.0f / m_Settings.read_speed;
        const uint32_t read_ms = (uint32_t)(ms_per_byte * (float)note->size);
        // Keeps thcrap's integer division of the time per row.
        const float ms_per_row = (float)(read_ms / (uint32_t)texture_height);
        float scroll = 0.0f;
        if (ms_per_row > 0.0f) {
            const float row = (float)(age % read_ms) / ms_per_row;
            scroll = (row - height / 2.0f) / (texture_height - height);
            scroll = scroll < 1.0f ? scroll : 1.0f;
            scroll = scroll > 0.0f ? scroll : 0.0f;
        }
        v_size = height / texture_height;
        v = scroll * (1.0f - v_size);
    } else {
        if (m_Settings.valign == M_VALIGN_CENTER) {
            y += height / 2.0f - texture_height / 2.0f;
        } else if (m_Settings.valign == M_VALIGN_BOTTOM) {
            y += height - texture_height;
        }
        height = texture_height;
    }
    const float fade = (float)age / m_Settings.fade_ms;
    const float alpha = fade < 1.0f ? fade : 1.0f;
    const float scale_x = M_SCREEN_WIDTH / m_Settings.reference_width;
    const float scale_y = M_SCREEN_HEIGHT / m_Settings.reference_height;
    const float x0 = x * scale_x;
    const float y0 = y * scale_y;
    const float x1 = x0 + width * scale_x;
    const float y1 = y0 + height * scale_y;
    const GFX_COLOR color = { 255, 255, 255, (uint8_t)(alpha * 255.0f) };
    const GFX_VERTEX vertices[4] = {
        { { x0, y0, 0.0f }, color, { 0.0f, v } },
        { { x1, y0, 0.0f }, color, { 1.0f, v } },
        { { x1, y1, 0.0f }, color, { 1.0f, v + v_size } },
        { { x0, y1, 0.0f }, color, { 0.0f, v + v_size } },
    };
    const GFX_VIEW view = GfxView_Screen();
    Gfx_SetView(&view);
    Gfx_DrawQuad(
        &(GFX_STATE) {
            .blend = GFX_BLEND_ALPHA,
            .filter = GFX_FILTER_LINEAR,
        },
        note->texture, vertices, 0);
}

void Note_Init(const PATCH_STACK *const patches)
{
    Note_Shutdown();
    if (!Translation_HasBaseTsa()) {
        return;
    }
    ARENA arena;
    Arena_Init(&arena, "translator notes", M_ARENA_CAPACITY);
    const JSON_VALUE *const notes = Json_Get(
        Patch_LoadSettings(patches, Arena_Allocator(&arena)), "tlnotes");
    m_Settings = m_Defaults;
    M_ReadRegion(notes);
    M_ReadLook(notes);
    const JSON_VALUE *const font = Json_Get(notes, "font");
    if (font != nullptr && font->type != JSON_STRING) {
        LOG_WARNING("tlnotes: font must be a font rule");
    }
    if (m_Settings.width > 0.0f && m_Settings.reference_width > 0.0f) {
        m_Enabled = Result_Log(
            M_LoadFont(Json_GetString(font, nullptr)), LOG_LEVEL_WARNING);
    }
    Arena_Free(&arena);
    Array_Init(&m_Notes, &g_HeapAllocator);
}

void Note_Shutdown(void)
{
    if (m_Notes.allocator != nullptr) {
        for (int32_t i = 0; i < m_Notes.count; i++) {
            Mem_Free(m_Notes.items[i].text);
            if (m_Notes.items[i].texture != nullptr) {
                GfxTexture_Free(m_Notes.items[i].texture);
            }
        }
        Array_Free(&m_Notes);
    }
    m_Notes = (M_NOTE_ARRAY) {};
    TextFont_Free(m_Font);
    m_Font = nullptr;
    m_Enabled = false;
    m_Shown = -1;
    m_Drawn = -1;
    m_Ticks = 0;
}

void Note_Show(const char *const text)
{
    if (!m_Enabled) {
        return;
    }
    const char *const separator = strchr(text, M_NOTE_START);
    if (separator == nullptr) {
        return;
    }
    const char *const note = separator + 1;
    const size_t size = strcspn(note, "\x14");
    if (size == 0) {
        return;
    }
    for (int32_t i = 0; i < m_Notes.count; i++) {
        const M_NOTE *const known = &m_Notes.items[i];
        if (known->size == size && memcmp(known->text, note, size) == 0) {
            m_Shown = i;
            return;
        }
    }
    char *const copy = Mem_Alloc(size + 1);
    memcpy(copy, note, size);
    copy[size] = '\0';
    Array_Append(&m_Notes, ((M_NOTE) { .text = copy, .size = size }));
    m_Shown = m_Notes.count - 1;
}

void Note_Remove(void)
{
    m_Shown = -1;
}

void Note_Draw(void)
{
    const int32_t drawn = m_Drawn;
    m_Drawn = m_Shown;
    if (m_Shown < 0) {
        return;
    }
    if (drawn != m_Shown) {
        m_Ticks = 0;
    }
    const uint32_t age =
        (uint32_t)((uint64_t)m_Ticks * 1000u / M_TICKS_PER_SECOND);
    m_Ticks++;
    M_NOTE *const note = &m_Notes.items[m_Shown];
    if (note->texture == nullptr && !note->failed) {
        note->failed = !Result_Log(M_Render(note), LOG_LEVEL_WARNING);
    }
    if (note->texture != nullptr) {
        M_DrawNote(note, age);
    }
}
