#include "tests/test.h"
#include "tests/th02/th02_high_score_entry.c"
#include "th02/high_score_entry/private.h"

#include <string.h>

TEST(registration_rejects_low_scores_and_preserves_name_tail_on_ties)
{
    HIGH_SCORES scores;
    HighScores_Defaults(&scores);
    HighScoreEntry_Init(&g_TestHighScoreEntryRender);
    HIGH_SCORE_SECTION *const section = &scores.sections[0];
    for (size_t i = 0; i < HIGH_SCORES_PLACES; i++) {
        section->records[i].name_glyphs[6] = 0x80 + i;
    }
    const HIGH_SCORE_SECTION before = *section;
    SCORE_RUN run = { .score = 999,
                      .stage = 127,
                      .shot_type = 2,
                      .year = 2026,
                      .day = 21,
                      .month = 11 };
    HIGH_SCORE_ENTRY_EVENTS event =
        HighScoreEntryMenu_Begin(section, &run, false);
    CHECK_EQ_INT(event.result, HIGH_SCORE_ENTRY_WAIT_KEY);
    CHECK(!event.save_section);
    CHECK(memcmp(section, &before, sizeof(before)) == 0);
    CHECK_EQ_INT(HighScoreEntryMenu_Step(0).result, HIGH_SCORE_ENTRY_COMPLETE);
    run.score = 10000;
    event = HighScoreEntryMenu_Begin(section, &run, false);
    CHECK_EQ_INT(event.result, HIGH_SCORE_ENTRY_INPUT);
    CHECK_EQ_INT(p_Menu->place, 0);
    CHECK_EQ_INT(section->records[0].stage, 127);
    CHECK_EQ_INT(section->records[0].year, 2026);
    CHECK_EQ_INT(section->records[1].score, 10000);
    CHECK_EQ_INT(section->records[0].name_glyphs[6], 0x80);
    CHECK_EQ_INT(section->records[1].name_glyphs[6], 0x81);
    CHECK_EQ_INT(section->records[9].name_glyphs[6], 0x89);
}

TEST(registration_last_glyph_enter_jump_precedes_bomb_and_cancel)
{
    HIGH_SCORES scores;
    HighScores_Defaults(&scores);
    HighScoreEntry_Init(&g_TestHighScoreEntryRender);
    SCORE_RUN run = { .score = 10000 };
    HighScoreEntryMenu_Begin(&scores.sections[0], &run, false);
    p_Menu->input_locked = 0;
    p_Menu->col = 12;
    p_Menu->name_pos = 5;
    const HIGH_SCORE_ENTRY_EVENTS event = HighScoreEntryMenu_Step(
        GAME_KEY_SHOT | GAME_KEY_BOMB | GAME_KEY_CANCEL);
    CHECK_EQ_INT(event.result, HIGH_SCORE_ENTRY_COMPLETE);
    CHECK(event.save_section);
    CHECK_EQ_INT(p_Menu->col, 16);
    CHECK_EQ_INT(p_Menu->row, 2);
    CHECK_EQ_INT(p_Menu->name_pos, 4);
    CHECK_EQ_INT(scores.sections[0].records[0].name_glyphs[5], 0xcf);
    CHECK(!HighScoreEntryMenu_Step(GAME_KEY_SHOT).save_section);
}

TEST(registration_enter_bypasses_bomb_and_repeat_delay_wrap_stays_locked)
{
    HIGH_SCORES scores;
    HighScores_Defaults(&scores);
    HighScoreEntry_Init(&g_TestHighScoreEntryRender);
    SCORE_RUN run = { .score = 10000 };
    HighScoreEntryMenu_Begin(&scores.sections[0], &run, false);
    p_Menu->input_locked = 1;
    p_Menu->input_delay = 255;
    CHECK_EQ_INT(
        HighScoreEntryMenu_Step(GAME_KEY_LEFT).result, HIGH_SCORE_ENTRY_FRAME);
    CHECK_EQ_INT(HighScoreEntryMenu_Step(0).result, HIGH_SCORE_ENTRY_INPUT);
    CHECK_EQ_INT(p_Menu->input_delay, 0);
    CHECK_EQ_INT(p_Menu->input_locked, GAME_KEY_LEFT);
    p_Menu->input_delay = 31;
    HighScoreEntryMenu_Step(GAME_KEY_LEFT);
    HighScoreEntryMenu_Step(0);
    CHECK_EQ_INT(p_Menu->input_delay, 32);
    CHECK_EQ_INT(p_Menu->input_locked, 0);
    p_Menu->row = 2;
    p_Menu->col = 16;
    p_Menu->name_pos = 5;
    const HIGH_SCORE_ENTRY_EVENTS event =
        HighScoreEntryMenu_Step(GAME_KEY_SHOT | GAME_KEY_BOMB);
    CHECK(event.save_section);
    CHECK_EQ_INT(p_Menu->name_pos, 5);
    CHECK_EQ_INT(scores.sections[0].records[0].name_glyphs[5], 0xda);
}
