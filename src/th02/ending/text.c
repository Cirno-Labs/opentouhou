#include "th02/ending/text.h"

#include "core/base/assert.h"

#include <string.h>

static ENDING_TEXT_EVENT M_Input(ENDING_TEXT *const text)
{
    text->phase =
        text->cursor < text->length ? ENDING_TEXT_REVEAL : ENDING_TEXT_HOLD;
    return (ENDING_TEXT_EVENT) { ENDING_TEXT_INPUT, 0 };
}

static ENDING_TEXT_EVENT M_Finish(ENDING_TEXT *const text)
{
    if (text->fast_forward) {
        EndingText_Clear();
    }
    text->phase = ENDING_TEXT_FINISHED;
    return (ENDING_TEXT_EVENT) { ENDING_TEXT_DONE, 0 };
}

RESULT EndingText_Load(
    READER *const reader, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, ENDING_TEXT_SCRIPT *const out)
{
    *out = (ENDING_TEXT_SCRIPT) {};
    MUST(Reader_Result(reader));
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(
        size == 0 || size % 46 != 0 || size / 46 > 100,
        "Invalid ending text record count");
    const size_t count = size / 46;
    DRAW_TEXT *const lines =
        Allocator_AllocZ(allocator, count * sizeof(*lines));
    for (size_t i = 0; i < count; i++) {
        READER field = Reader_Slice(reader, 46);
        const RESULT result = DrawText_Read(
            &field, ENDING_TEXT_LINE_BYTES, font, allocator, &lines[i]);
        Reader_Close(&field);
        MUST(result);
    }
    *out = (ENDING_TEXT_SCRIPT) { lines, (uint16_t)count };
    return OK;
}

RESULT EndingText_LoadVerdict(
    READER *const story, READER *const titles, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, ENDING_TEXT_SCRIPT *const out)
{
    *out = (ENDING_TEXT_SCRIPT) {};
    MUST(Reader_Result(story));
    MUST(Reader_Result(titles));
    const size_t size = Reader_Remaining(story);
    FAIL_IF(
        size <= 12 * 46 || size > 100 * 46 || size % 46 != 0
            || Reader_Remaining(titles) != 12 * 46,
        "Invalid verdict text extent");
    uint8_t *const bytes = Mem_AllocZ(size + 1);
    Reader_ReadBytes(story, bytes, size);
    Reader_ReadBytes(titles, bytes, 12 * 46);
    RESULT result = Reader_Result(story);
    if (IS_OK(result)) {
        result = Reader_Result(titles);
    }
    DRAW_TEXT *const lines = Allocator_AllocZ(allocator, 12 * sizeof(*lines));
    for (size_t i = 0; i < 12 && IS_OK(result); i++) {
        READER string;
        Reader_OpenBuffer(&string, bytes + i * 46, size + 1 - i * 46, nullptr);
        result = DrawText_Read(
            &string, Reader_Remaining(&string), font, allocator, &lines[i]);
        Reader_Close(&string);
    }
    Mem_Free(bytes);
    MUST(result);
    *out = (ENDING_TEXT_SCRIPT) { lines, 12 };
    return OK;
}

// MAINE.EXE's twenty credits/version/verdict labels (DGROUP), Shift-JIS,
// baked in without their zero terminator, in ENDING_LABEL order.
static const uint8_t m_Labels[ENDING_LABEL_COUNT][42] = {
    { 0x76, 0x65, 0x72, 0x20, 0x31, 0x2e, 0x30, 0x30 },
    { 0x95, 0x95, 0x96, 0x82, 0x98, 0x5e, 0x81, 0x40, 0x81, 0x40,
      0x82, 0x72, 0x82, 0x73, 0x82, 0x60, 0x82, 0x65, 0x82, 0x65 },
    { 0x83, 0x76, 0x83, 0x8d, 0x83, 0x4f, 0x83, 0x89, 0x83, 0x80, 0x81,
      0x40, 0x81, 0x40, 0x81, 0x40, 0x82, 0x79, 0x82, 0x74, 0x82, 0x6d },
    { 0x83, 0x4f, 0x83, 0x89, 0x83, 0x74, 0x83, 0x42, 0x83, 0x62,
      0x83, 0x4e, 0x81, 0x40, 0x82, 0x79, 0x82, 0x74, 0x82, 0x6d },
    { 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x81,
      0x40, 0x81, 0x40, 0x8d, 0x82, 0x95, 0x8d, 0x93, 0xfa, 0x8c, 0xfc },
    { 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x81,
      0x69, 0x83, 0x49, 0x81, 0x5b, 0x83, 0x8b, 0x83, 0x4e,
      0x83, 0x8a, 0x83, 0x41, 0x89, 0xe6, 0x81, 0x6a },
    { 0x82, 0x6c, 0x82, 0x74, 0x82, 0x72, 0x82, 0x68, 0x82, 0x62,
      0x81, 0x40, 0x81, 0x40, 0x82, 0x79, 0x82, 0x74, 0x82, 0x6d },
    { 0x82, 0x73, 0x82, 0x64, 0x82, 0x72, 0x82, 0x73, 0x81, 0x40, 0x82,
      0x6f, 0x82, 0x6b, 0x82, 0x60, 0x82, 0x78, 0x82, 0x64, 0x82, 0x71 },
    { 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x82, 0xa9, 0x82, 0xa8,
      0x82, 0xe9 },
    { 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x81, 0x40, 0x82, 0x62, 0x81, 0x7c,
      0x82, 0x76 },
    { 0x81, 0x40, 0x81, 0x40, 0x20, 0x8e, 0x52, 0x93, 0x63, 0x81, 0x40, 0x8c,
      0x62, 0x88, 0xea, 0x98, 0x59 },
    { 0x20, 0x82, 0xbb, 0x82, 0xcc, 0x91, 0xbc, 0x91, 0xe5, 0x90, 0xa8,
      0x82, 0xcc, 0x82, 0xdd, 0x82, 0xc8, 0x82, 0xb3, 0x82, 0xf1 },
    { 0x82, 0x73, 0x82, 0x88, 0x82, 0x81, 0x82, 0x8e, 0x82, 0x8b, 0x81,
      0x40, 0x82, 0x99, 0x82, 0x8f, 0x82, 0x95, 0x81, 0x40, 0x82, 0x86,
      0x82, 0x8f, 0x82, 0x92, 0x81, 0x40, 0x82, 0x90, 0x82, 0x8c, 0x82,
      0x81, 0x82, 0x99, 0x82, 0x89, 0x82, 0x8e, 0x82, 0x87 },
    { 0x81, 0x40, 0x8d, 0xc5, 0x8f, 0x49, 0x93, 0xbe, 0x93, 0x5f },
    { 0x83, 0x52, 0x83, 0x93, 0x83, 0x65, 0x83, 0x42, 0x83, 0x6a, 0x83, 0x85,
      0x81, 0x5b },
    { 0x81, 0x40, 0x20, 0x83, 0x89, 0x83, 0x93, 0x83, 0x4e },
    { 0x83, 0x76, 0x83, 0x8c, 0x83, 0x43, 0x83, 0x84, 0x81, 0x5b, 0x8f, 0x89,
      0x8a, 0xfa },
    { 0x20, 0x97, 0xec, 0x8c, 0x82, 0x8f, 0x89, 0x8a, 0xfa, 0x90, 0x94 },
    { 0x82, 0xa0, 0x82, 0xc8, 0x82, 0xbd, 0x82, 0xcc, 0x98, 0x72, 0x91, 0x4f },
    { 0x82, 0x50, 0x82, 0x58, 0x82, 0x58, 0x82, 0x56, 0x81, 0x40, 0x82, 0x79,
      0x82, 0x74, 0x82, 0x6d, 0x20, 0x20, 0x28, 0x41, 0x6d, 0x75, 0x73, 0x65,
      0x6d, 0x65, 0x6e, 0x74, 0x20, 0x4d, 0x61, 0x6b, 0x65, 0x72, 0x73, 0x29 },
};

static const uint8_t m_LabelSizes[ENDING_LABEL_COUNT] = {
    8,  20, 22, 20, 22, 26, 20, 22, 14, 14,
    17, 21, 42, 10, 14, 9,  14, 11, 12, 36,
};
// The ending program's four gaiji rank labels, raw TRAM/graphic bytes
// (DGROUP+0x90), zero-terminated at byte 7 of each 8-byte row.
static const uint8_t m_Ranks[4][8] = {
    { 0xcf, 0xcf, 0xae, 0xaa, 0xbc, 0xc2, 0xcf, 0 },
    { 0xcf, 0xb6, 0xb8, 0xbb, 0xb7, 0xaa, 0xb5, 0 },
    { 0xcf, 0xcf, 0xb1, 0xaa, 0xbb, 0xad, 0xcf, 0 },
    { 0xb5, 0xbe, 0xb6, 0xaa, 0xbd, 0xb2, 0xac, 0 },
};

RESULT EndingText_LoadLabels(
    const DRAW_TEXT_FONT *const font, const ALLOCATOR *const allocator,
    ENDING_LABELS *const out)
{
    memset(out, 0, sizeof(*out));
    ENDING_LABELS labels = {};
    for (size_t i = 0; i < ENDING_LABEL_COUNT; i++) {
        READER reader;
        Reader_OpenBuffer(&reader, m_Labels[i], m_LabelSizes[i], nullptr);
        const RESULT result = DrawText_Read(
            &reader, m_LabelSizes[i], font, allocator, &labels.text[i]);
        Reader_Close(&reader);
        MUST(result);
    }
    memcpy(labels.ranks, m_Ranks, sizeof(labels.ranks));
    *out = labels;
    return OK;
}

void EndingText_Clear(void)
{
    Grcg_SetColor(0);
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t y = 332; y < 348; y++) {
        memset(page + y * VIDEO_WIDTH + 144, 0, 352);
    }
    Grcg_Off();
}

void EndingText_Put(
    const DRAW_TEXT *const line, const uint16_t bytes, const int16_t left,
    const int16_t top, const uint16_t style)
{
    int32_t count = 0;
    while (count < line->count
           && line->cells.characters[count].source_end <= bytes) {
        count++;
    }
    if (count < line->count) {
        const uint16_t previous =
            count == 0 ? 0 : line->cells.characters[count - 1].source_end;
        ASSERT(bytes == previous);
    }
    Draw_Text(line->glyphs, count, left, top, style);
}

void EndingText_Begin(
    ENDING_TEXT *const text, const DRAW_TEXT *const line, const int16_t left,
    const int16_t top, const uint16_t length, const uint16_t frames_per_pair,
    const uint16_t style, const bool fast_forward)
{
    ASSERT(length <= 78 && length % 2 == 0);
    memset(text, 0, sizeof(*text));
    text->line = line;
    text->left = left;
    text->top = top;
    text->length = length;
    text->frames_per_pair = frames_per_pair;
    text->style = style;
    text->fast_forward = fast_forward;
    text->holds = 20;
}

ENDING_TEXT_EVENT EndingText_Step(
    ENDING_TEXT *const text, const TH02_INPUT *const sample)
{
    if (text->phase == ENDING_TEXT_FINISHED) {
        return (ENDING_TEXT_EVENT) { ENDING_TEXT_DONE, 0 };
    }
    if (text->phase == ENDING_TEXT_BEGIN
        || text->phase == ENDING_TEXT_REVEAL_WAIT) {
        return M_Input(text);
    }
    if (text->phase == ENDING_TEXT_HOLD_WAIT) {
        return text->holds == 0 ? M_Finish(text) : M_Input(text);
    }
    ASSERT(sample != nullptr);
    text->keys = GameInput_Sense(sample);
    const bool fast = text->fast_forward && text->keys != 0;
    if (text->phase == ENDING_TEXT_REVEAL) {
        const uint16_t cursor = text->cursor;
        text->cursor += 2;
        EndingText_Put(
            text->line, text->cursor, text->left, text->top, text->style);
        if (fast && (cursor & 3) == 0) {
            return M_Input(text);
        }
        text->phase = ENDING_TEXT_REVEAL_WAIT;
        return (ENDING_TEXT_EVENT) { ENDING_TEXT_DELAY,
                                     fast ? text->frames_per_pair / 3
                                          : text->frames_per_pair };
    }
    ASSERT(text->phase == ENDING_TEXT_HOLD && text->holds != 0);
    text->holds--;
    text->phase = ENDING_TEXT_HOLD_WAIT;
    return (ENDING_TEXT_EVENT) { ENDING_TEXT_DELAY, fast ? 0 : 2 };
}
