#include "th02_clip_fixture.h"
#include "th02/ending/text.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    size_t calls;
    size_t fail_at;
} FONT;

static RESULT M_Font(
    void *const context, const DRAW_TEXT_CHARACTER *const character,
    uint16_t rows[16])
{
    FONT *const font = context;
    font->calls++;
    FAIL_IF(font->calls == font->fail_at, "Missing ending glyph");
    for (size_t y = 0; y < 16; y++) {
        rows[y] = 0x8001;
    }
    return OK;
}

static DRAW_TEXT M_Line(ARENA *const arena)
{
    uint8_t bytes[] = { 0x82, 0xa0, 0x82, 0xa2 };
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    FONT state = {};
    const DRAW_TEXT_FONT font = { M_Font, &state };
    DRAW_TEXT line;
    CHECK(Result_Ignore(DrawText_Read(
        &reader, sizeof(bytes), &font, Arena_Allocator(arena), &line)));
    Reader_Close(&reader);
    return line;
}

TEST(ending_records_own_text_and_masks_and_ignore_padding)
{
    ARENA arena;
    Arena_Init(&arena, "ending text records", 8192);
    uint8_t bytes[92] = { 0x82, 0xa0, 'A', 'B' };
    for (size_t i = 46; i < 90; i += 2) {
        bytes[i] = 0x82;
        bytes[i + 1] = 0xa2;
    }
    bytes[44] = bytes[45] = bytes[90] = bytes[91] = 0xff;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    FONT state = {};
    const DRAW_TEXT_FONT font = { M_Font, &state };
    ENDING_TEXT_SCRIPT script;
    CHECK(Result_Ignore(
        EndingText_Load(&reader, &font, Arena_Allocator(&arena), &script)));
    CHECK_EQ_INT(Reader_Tell(&reader), sizeof(bytes));
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    CHECK_EQ_INT(script.count, 2);
    CHECK_EQ_INT(state.calls, 25);
    CHECK(strcmp(script.lines[0].cells.text, "あAB") == 0);
    CHECK_EQ_INT(script.lines[0].count, 3);
    CHECK_EQ_INT(script.lines[1].count, 22);
    CHECK_EQ_INT(script.lines[1].cells.characters[21].source_end, 44);
    CHECK_EQ_INT(script.lines[0].glyphs[0].rows[0], 0x8001);
    CHECK_EQ_INT(script.lines[0].glyphs[1].rows[0], 0x8000);
    Arena_Free(&arena);
}

TEST(ending_records_reject_invalid_lengths_text_and_font_failures)
{
    uint8_t bytes[4646] = {};
    const size_t sizes[] = { 0, 45, 47, sizeof(bytes), 46, 92 };
    for (size_t i = 0; i < ARRAY_SIZE(sizes); i++) {
        ARENA arena;
        Arena_Init(&arena, "invalid ending text", 8192);
        memset(bytes, 0, sizeof(bytes));
        FONT state = {};
        if (i == 4) {
            bytes[43] = 0x82;
            memset(bytes, 'A', 43);
        } else if (i == 5) {
            bytes[0] = bytes[46] = 'A';
            state.fail_at = 2;
        }
        const DRAW_TEXT_FONT font = { M_Font, &state };
        READER reader;
        Reader_OpenBuffer(&reader, bytes, sizes[i], nullptr);
        ENDING_TEXT_SCRIPT script = { .count = 9 };
        CHECK(!Result_Ignore(
            EndingText_Load(&reader, &font, Arena_Allocator(&arena), &script)));
        CHECK(script.lines == nullptr);
        CHECK_EQ_INT(script.count, 0);
        Reader_Close(&reader);
        Arena_Free(&arena);
    }
}

TEST(ending_reveal_keeps_delays_and_clear_separate_from_input_requests)
{
    for (int fast = 0; fast < 2; fast++) {
        for (int held = 0; held < 2; held++) {
            ARENA arena;
            Arena_Init(&arena, "ending reveal", 1024 * 1024);
            DRAW_TEXT line = M_Line(&arena);
            Video_Init(Arena_Allocator(&arena));
            memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
            Video_AccessPage(1);
            TestClip_Set(0, 0, 0, 0);
            Grcg_Init();

            ENDING_TEXT text;
            EndingText_Begin(&text, &line, 144, 332, 4, 6, 0x2f, fast != 0);
            const TH02_INPUT sample = { .shot = held != 0 };
            ENDING_TEXT_EVENT event = EndingText_Step(&text, nullptr);
            size_t inputs = 0;
            size_t delays = 0;
            while (event.wait != ENDING_TEXT_DONE) {
                if (event.wait == ENDING_TEXT_INPUT) {
                    inputs++;
                    event = EndingText_Step(&text, &sample);
                    if (inputs == 1) {
                        CHECK_EQ_INT(g_Video.pages[1][332 * 640 + 144], 15);
                        CHECK_EQ_INT(g_Video.pages[1][332 * 640 + 160], 3);
                    }
                } else {
                    CHECK_EQ_INT(event.wait, ENDING_TEXT_DELAY);
                    if (fast != 0 && held != 0) {
                        CHECK_EQ_INT(event.frames, delays == 0 ? 2 : 0);
                    } else {
                        CHECK_EQ_INT(event.frames, delays < 2 ? 6 : 2);
                    }
                    delays++;
                    event = EndingText_Step(&text, nullptr);
                }
            }
            CHECK_EQ_INT(inputs, 22);
            CHECK_EQ_INT(delays, fast != 0 && held != 0 ? 21 : 22);
            CHECK_EQ_INT(text.keys, held != 0 ? GAME_KEY_SHOT : 0);
            for (size_t y = 332; y < 348; y++) {
                for (size_t x = 144; x < 496; x++) {
                    if (fast != 0) {
                        CHECK_EQ_INT(g_Video.pages[1][y * 640 + x], 0);
                    }
                }
            }
            CHECK_EQ_INT(g_Video.pages[1][331 * 640 + 144], 3);
            CHECK_EQ_INT(g_Video.pages[1][332 * 640 + 496], 3);
            CHECK_EQ_INT(g_Video.pages[0][332 * 640 + 144], 0);
            CHECK_EQ_INT(g_Video.drawing_page, 1);
            CHECK_EQ_INT(g_Grcg->mode, 0);
            CHECK_EQ_INT(g_Grcg->tiles[0], fast != 0 ? 0 : 255);
            CHECK_EQ_INT(
                EndingText_Step(&text, nullptr).wait, ENDING_TEXT_DONE);
            Arena_Free(&arena);
        }
    }
}

TEST(empty_reveal_still_requests_twenty_holds)
{
    ENDING_TEXT text;
    const DRAW_TEXT line = {};
    EndingText_Begin(&text, &line, 0, 0, 0, 0, 0, false);
    const TH02_INPUT sample = {};
    CHECK_EQ_INT(EndingText_Step(&text, nullptr).wait, ENDING_TEXT_INPUT);
    for (size_t i = 0; i < 20; i++) {
        const ENDING_TEXT_EVENT event = EndingText_Step(&text, &sample);
        CHECK_EQ_INT(event.wait, ENDING_TEXT_DELAY);
        CHECK_EQ_INT(event.frames, 2);
        CHECK_EQ_INT(
            EndingText_Step(&text, nullptr).wait,
            i == 19 ? ENDING_TEXT_DONE : ENDING_TEXT_INPUT);
    }
}

TEST(ending_labels_decode_the_built_in_text_masks_and_rank_strings)
{
    ARENA arena;
    Arena_Init(&arena, "ending labels", 65536);
    FONT state = {};
    const DRAW_TEXT_FONT font = { M_Font, &state };
    ENDING_LABELS labels;
    CHECK(Result_Ignore(
        EndingText_LoadLabels(&font, Arena_Allocator(&arena), &labels)));
    CHECK(state.calls > 0);
    for (size_t i = 0; i < ENDING_LABEL_COUNT; i++) {
        CHECK(labels.text[i].count > 0);
        CHECK(labels.text[i].glyphs != nullptr);
    }
    for (size_t rank = 0; rank < 4; rank++) {
        CHECK_EQ_INT(labels.ranks[rank][7], 0);
    }
    CHECK_EQ_INT(labels.ranks[0][0], 0xcf);
    Arena_Free(&arena);

    Arena_Init(&arena, "ending labels fail", 65536);
    state = (FONT) { .fail_at = 1 };
    CHECK(!Result_Ignore(
        EndingText_LoadLabels(&font, Arena_Allocator(&arena), &labels)));
    CHECK(labels.text[0].glyphs == nullptr);
    Arena_Free(&arena);
}

TEST(verdict_titles_keep_padding_and_the_preceding_story_suffix)
{
    uint8_t story[14 * 46];
    uint8_t titles[12 * 46];
    memset(story, 'Z', sizeof(story));
    memset(titles, 'A', sizeof(titles));
    for (size_t i = 0; i < 12; i++) {
        titles[i * 46 + 44] = '\r';
        titles[i * 46 + 45] = '\n';
    }
    ARENA arena;
    Arena_Init(&arena, "verdict title overlay", 1024 * 1024);
    READER previous;
    READER next;
    Reader_OpenBuffer(&previous, story, sizeof(story), nullptr);
    Reader_OpenBuffer(&next, titles, sizeof(titles), nullptr);
    FONT state = {};
    const DRAW_TEXT_FONT font = { M_Font, &state };
    ENDING_TEXT_SCRIPT script;
    CHECK(Result_Ignore(EndingText_LoadVerdict(
        &previous, &next, &font, Arena_Allocator(&arena), &script)));
    CHECK_EQ_INT(Reader_Remaining(&previous), 0);
    CHECK_EQ_INT(Reader_Remaining(&next), 0);
    Reader_Close(&previous);
    Reader_Close(&next);
    memset(story, 0, sizeof(story));
    memset(titles, 0, sizeof(titles));
    CHECK_EQ_INT(script.count, 12);
    CHECK_EQ_INT(script.lines[11].count, 3 * 46);
    CHECK_EQ_INT(script.lines[11].cells.text[43], 'A');
    CHECK_EQ_INT(script.lines[11].cells.text[44], '\r');
    CHECK_EQ_INT(script.lines[11].cells.text[45], '\n');
    CHECK_EQ_INT(script.lines[11].cells.text[46], 'Z');
    CHECK_EQ_INT(script.lines[11].cells.text[3 * 46], 0);
    CHECK_EQ_INT(script.lines[11].glyphs[46].rows[0], 0x8000);
    Reader_OpenBuffer(&previous, story, 46, nullptr);
    Reader_OpenBuffer(&next, titles, sizeof(titles), nullptr);
    CHECK(!Result_Ignore(EndingText_LoadVerdict(
        &previous, &next, &font, Arena_Allocator(&arena), &script)));
    CHECK(script.lines == nullptr);
    CHECK_EQ_INT(script.count, 0);
    Reader_Close(&previous);
    Reader_Close(&next);
    Arena_Free(&arena);
}
