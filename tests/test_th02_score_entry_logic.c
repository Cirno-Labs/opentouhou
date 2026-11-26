#include "th02_score_entry.c"

#include "test.h"

TEST(registration_rejects_low_scores_and_preserves_name_tail_on_ties)
{
    SCORES scores;
    Scores_Defaults(&scores);
    SCORE_SECTION *const section = &scores.sections[0];
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        section->records[i].name_glyphs[6] = 0x80 + i;
    }
    const SCORE_SECTION before = *section;
    SCORE_RUN run = { .score = 999,
                      .stage = 127,
                      .shot_type = 2,
                      .year = 2026,
                      .day = 21,
                      .month = 11 };
    SCORE_ENTRY_EVENTS event = M_MenuBegin(section, &run, false);
    CHECK_EQ_INT(event.result, SCORE_ENTRY_WAIT_KEY);
    CHECK(!event.save_section);
    CHECK(memcmp(section, &before, sizeof(before)) == 0);
    CHECK_EQ_INT(M_MenuStep(0).result, SCORE_ENTRY_COMPLETE);
    run.score = 10000;
    event = M_MenuBegin(section, &run, false);
    CHECK_EQ_INT(event.result, SCORE_ENTRY_INPUT);
    CHECK_EQ_INT(m_Menu.place, 0);
    CHECK_EQ_INT(section->records[0].stage, 127);
    CHECK_EQ_INT(section->records[0].year, 2026);
    CHECK_EQ_INT(section->records[1].score, 10000);
    CHECK_EQ_INT(section->records[0].name_glyphs[6], 0x80);
    CHECK_EQ_INT(section->records[1].name_glyphs[6], 0x81);
    CHECK_EQ_INT(section->records[9].name_glyphs[6], 0x89);
}

TEST(registration_last_glyph_enter_jump_precedes_bomb_and_cancel)
{
    SCORES scores;
    Scores_Defaults(&scores);
    SCORE_RUN run = { .score = 10000 };
    M_MenuBegin(&scores.sections[0], &run, false);
    m_Menu.input_locked = 0;
    m_Menu.col = 12;
    m_Menu.name_pos = 5;
    const SCORE_ENTRY_EVENTS event =
        M_MenuStep(GAME_KEY_SHOT | GAME_KEY_BOMB | GAME_KEY_CANCEL);
    CHECK_EQ_INT(event.result, SCORE_ENTRY_COMPLETE);
    CHECK(event.save_section);
    CHECK_EQ_INT(m_Menu.col, 16);
    CHECK_EQ_INT(m_Menu.row, 2);
    CHECK_EQ_INT(m_Menu.name_pos, 4);
    CHECK_EQ_INT(scores.sections[0].records[0].name_glyphs[5], 0xcf);
    CHECK(!M_MenuStep(GAME_KEY_SHOT).save_section);
}

TEST(registration_enter_bypasses_bomb_and_repeat_delay_wrap_stays_locked)
{
    SCORES scores;
    Scores_Defaults(&scores);
    SCORE_RUN run = { .score = 10000 };
    M_MenuBegin(&scores.sections[0], &run, false);
    m_Menu.input_locked = 1;
    m_Menu.input_delay = 255;
    CHECK_EQ_INT(M_MenuStep(GAME_KEY_LEFT).result, SCORE_ENTRY_FRAME);
    CHECK_EQ_INT(M_MenuStep(0).result, SCORE_ENTRY_INPUT);
    CHECK_EQ_INT(m_Menu.input_delay, 0);
    CHECK_EQ_INT(m_Menu.input_locked, GAME_KEY_LEFT);
    m_Menu.input_delay = 31;
    M_MenuStep(GAME_KEY_LEFT);
    M_MenuStep(0);
    CHECK_EQ_INT(m_Menu.input_delay, 32);
    CHECK_EQ_INT(m_Menu.input_locked, 0);
    m_Menu.row = 2;
    m_Menu.col = 16;
    m_Menu.name_pos = 5;
    const SCORE_ENTRY_EVENTS event = M_MenuStep(GAME_KEY_SHOT | GAME_KEY_BOMB);
    CHECK(event.save_section);
    CHECK_EQ_INT(m_Menu.name_pos, 5);
    CHECK_EQ_INT(scores.sections[0].records[0].name_glyphs[5], 0xda);
}
