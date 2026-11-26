#include "th02_tram_fixture.h"
#include "../src/th02/score_entry/logic.c"

#include "th02/vsync_irq.h"

#include "test.h"

typedef struct {
    SCORES scores;
} FIXTURE;

static void M_Begin(
    FIXTURE *const f, const int32_t score, const bool ending,
    const TH02_INPUT *const input)
{
    Tram_Init();
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(f, 0, sizeof(*f));
    Scores_Defaults(&f->scores);
    g_VSync.count2 = 65530;
    const SCORE_RUN run = { .score = score,
                            .stage = ending ? 127 : 2,
                            .shot_type = 2,
                            .year = 2026,
                            .day = 22,
                            .month = 11 };
    ScoreEntry_Begin(&f->scores.sections[1], &run, ending, input);
}

static SCORE_ENTRY_EVENTS M_Frame(const TH02_INPUT *const input)
{
    VSyncIrq_VSync();
    return ScoreEntry_Update(input);
}

TEST(qualified_entry_binds_text_and_date_and_requests_save_only_once)
{
    for (size_t ending = 0; ending < 2; ending++) {
        FIXTURE f;
        TH02_INPUT input = { .shot = true };
        M_Begin(&f, 123456, ending != 0, &input);
        CHECK_EQ_INT(m_Menu.place, 0);
        CHECK(m_Menu.section == &f.scores.sections[1]);
        SCORE_RECORD *const record = &f.scores.sections[1].records[0];
        CHECK_EQ_INT(record->score, 123456);
        CHECK_EQ_INT(record->stage, ending != 0 ? 127 : 2);
        CHECK_EQ_INT(record->year, 2026);
        CHECK_EQ_INT(record->month, 11);
        CHECK_EQ_INT(record->day, 22);
        CHECK_EQ_INT(record->shot_type, 2);
        SCORE_ENTRY_EVENTS event = ScoreEntry_Update(&input);
        CHECK_EQ_INT(event.result, SCORE_ENTRY_FRAME);
        CHECK_EQ_INT(m_Menu.name_pos, 0);
        const SCORE_ENTRY held = m_Entry;
        const SCORE_ENTRY_MENU controls = m_Menu;
        CHECK_EQ_INT(ScoreEntry_Update(&input).result, SCORE_ENTRY_FRAME);
        CHECK(memcmp(&held, &m_Entry, sizeof(held)) == 0);
        CHECK(memcmp(&controls, &m_Menu, sizeof(controls)) == 0);
        input.shot = false;
        CHECK_EQ_INT(M_Frame(&input).result, SCORE_ENTRY_FRAME);
        CHECK_EQ_INT(M_Frame(&input).result, SCORE_ENTRY_FRAME);
        input.shot = true;
        event = M_Frame(&input);
        CHECK_EQ_INT(event.result, SCORE_ENTRY_FRAME);
        CHECK_EQ_INT(record->name_glyphs[0], ScoreEntry_Alphabet(0, 0));
        CHECK_EQ_INT(m_Menu.name_pos, 1);
        input.shot = false;
        M_Frame(&input);
        M_Frame(&input);
        input.cancel = true;
        event = M_Frame(&input);
        CHECK_EQ_INT(event.result, SCORE_ENTRY_COMPLETE);
        CHECK(event.save_section);
        CHECK_EQ_INT(g_VSync.count2, 0);
        CHECK(!ScoreEntry_Update(nullptr).save_section);
        CHECK_EQ_INT(ScoreEntry_Update(nullptr).result, SCORE_ENTRY_COMPLETE);
        CHECK_EQ_INT(f.scores.sections[0].records[0].score, 10000);
        CHECK_EQ_INT(f.scores.sections[2].records[0].score, 10000);
    }
}

TEST(nonqualifying_wait_ignores_space_and_requires_release_then_held_key)
{
    FIXTURE f;
    TH02_INPUT input = { .shot = true };
    M_Begin(&f, 999, false, &input);
    const SCORES saved = f.scores;
    for (size_t frame = 0; frame < 8; frame++) {
        CHECK_EQ_INT(M_Frame(&input).result, SCORE_ENTRY_WAIT_KEY);
    }
    input.shot = false;
    input.space = true;
    for (size_t frame = 0; frame < 4; frame++) {
        CHECK_EQ_INT(M_Frame(&input).result, SCORE_ENTRY_WAIT_KEY);
    }
    input.space = false;
    input.shot = true;
    bool done = false;
    for (size_t frame = 0; frame < 4 && !done; frame++) {
        const SCORE_ENTRY_EVENTS event = M_Frame(&input);
        CHECK(!event.save_section);
        done = event.result == SCORE_ENTRY_COMPLETE;
    }
    CHECK(done);
    CHECK(memcmp(&saved, &f.scores, sizeof(saved)) == 0);
    CHECK(!ScoreEntry_Update(nullptr).save_section);
}

TEST(main_registration_score_uses_modulo_arithmetic_and_full_continue_word)
{
    CHECK_EQ_INT(ScoreEntry_MainScore(12345, 7), 123457);
    CHECK_EQ_INT(ScoreEntry_MainScore(INT32_MIN, UINT16_MAX), 65535);
    CHECK_EQ_INT(ScoreEntry_MainScore(INT32_MAX, 0), -10);
    CHECK_EQ_INT(ScoreEntry_MainScore(-1, UINT16_MAX), 65525);
}

TEST(table_keeps_blank_digits_and_alphabet_for_unqualified_score)
{
    SCORES scores;
    Scores_Defaults(&scores);
    SCORE_SECTION *const section = &scores.sections[0];
    section->records[0].score = 0;
    section->records[1].score = 100000000;
    section->records[2].score = -1;
    section->records[3].stage = 127;
    const SCORE_SECTION before = *section;

    memset(&m_Tram, 0x5a, sizeof(m_Tram));
    ScoreEntryRender_Table(section, false, -1);
    for (size_t x = 26; x < 42; x++) {
        CHECK_EQ_INT(m_Tram.codes[6 * 80 + x], 0x5a5a);
        CHECK_EQ_INT(m_Tram.attributes[7 * 80 + x], 0x5a5a);
    }
    CHECK_EQ_INT(m_Tram.codes[8 * 80 + 40], 0x1f57);
    CHECK_EQ_INT(m_Tram.codes[9 * 80 + 44], 0x7057);
    CHECK_EQ_INT(m_Tram.attributes[6 * 80 + 10], 0xe1);
    for (size_t cell = 18 * 80; cell < TRAM_PLANE_CELLS; cell++) {
        CHECK_EQ_INT(m_Tram.codes[cell], 0x5a5a);
        CHECK_EQ_INT(m_Tram.attributes[cell], 0x5a5a);
    }
    CHECK(memcmp(section, &before, sizeof(before)) == 0);
}

TEST(main_and_ending_keep_distinct_header_and_tenth_place_positions)
{
    SCORES scores;
    Scores_Defaults(&scores);
    for (int ending = 0; ending < 2; ending++) {

        Tram_Init();
        ScoreEntryRender_Table(&scores.sections[0], ending != 0, 9);
        CHECK_EQ_INT(m_Tram.codes[4 * 80 + 27 + ending], 0x3957);
        CHECK_EQ_INT(m_Tram.codes[4 * 80 + 41 + ending], 0x3c57);
        CHECK_EQ_INT(m_Tram.codes[15 * 80 + 5 - ending], 0x2157);
        CHECK_EQ_INT(m_Tram.codes[15 * 80 + 7 - ending], 0x2057);
        CHECK_EQ_INT(m_Tram.attributes[15 * 80 + 10], 0x81);
        CHECK_EQ_INT(m_Tram.attributes[14 * 80 + 10], 0xe1);
        CHECK_EQ_INT(m_Tram.attributes[18 * 80 + 10], 0x85);
        CHECK_EQ_INT(m_Tram.attributes[18 * 80 + 11], 0x85);
        CHECK_EQ_INT(m_Tram.codes[18 * 80 + 34], 0x3757);
        CHECK_EQ_INT(m_Tram.codes[18 * 80 + 36], 0x3657);
    }
}

TEST(name_cursor_can_write_terminator_glyph_without_clearing_following_cells)
{
    SCORES scores;
    Scores_Defaults(&scores);
    scores.sections[0].records[2].name_glyphs[1] = 0;

    memset(&m_Tram, 0x5a, sizeof(m_Tram));
    ScoreEntryRender_Name(&scores.sections[0], 2, 1);
    CHECK_EQ_INT(m_Tram.codes[8 * 80 + 12], 0x56);
    CHECK_EQ_INT(m_Tram.codes[8 * 80 + 13], 0x8056);
    CHECK_EQ_INT(m_Tram.attributes[8 * 80 + 10], 0x81);
    CHECK_EQ_INT(m_Tram.attributes[8 * 80 + 12], 0x85);
    CHECK_EQ_INT(m_Tram.codes[8 * 80 + 14], 0x5a5a);
}

TEST(controller_updates_the_rendered_name_and_alphabet_without_redrawing_table)
{
    SCORES scores;
    Scores_Defaults(&scores);

    Tram_Init();
    const SCORE_RUN run = { .score = 12345, .stage = 127 };
    M_MenuBegin(&scores.sections[0], &run, true);
    M_MenuStep(0);
    M_MenuStep(0);
    M_MenuStep(GAME_KEY_RIGHT | GAME_KEY_SHOT);
    CHECK_EQ_INT(scores.sections[0].records[0].name_glyphs[0], 0xab);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 10], 0x2b57);
    CHECK_EQ_INT(m_Tram.attributes[6 * 80 + 12], 0x85);
    CHECK_EQ_INT(m_Tram.attributes[18 * 80 + 10], 0xe1);
    CHECK_EQ_INT(m_Tram.attributes[18 * 80 + 12], 0x85);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 44], 0x7057);
}
