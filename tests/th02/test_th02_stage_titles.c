#include "tests/test.h"
#include "tests/test_allocator.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/stage/titles.c"
#include "th02/tram.h"

static void M_MakeText(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, TRAM_TEXT *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    CHECK(Result_Ignore(TramText_Read(&reader, size, allocator, out)));
    Reader_Close(&reader);
}

static void M_Assets(
    const ALLOCATOR *const allocator, STAGE_TITLE_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    out->allocator = *allocator;
    out->stage_label[0] = 0xbc;
    out->extra_label[0] = 0xae;
    out->demo_label[0] = 0xad;
    memset(out->halfwidths, 10, sizeof(out->halfwidths));
    uint8_t blank[48];
    memset(blank, ' ', sizeof(blank));
    for (unsigned i = 0; i < 6; i++) {
        const uint8_t letter = (uint8_t)('A' + i);
        M_MakeText(&letter, 1, allocator, &out->stages[i]);
    }
    for (unsigned i = 0; i < 12; i++) {
        const uint8_t letter = (uint8_t)('G' + i);
        M_MakeText(&letter, 1, allocator, &out->music[i]);
    }
    M_MakeText(blank, sizeof(blank), allocator, &out->blank);
    M_MakeText(blank, 26, allocator, &out->music_blank);
}

TEST(built_in_table_loads_and_frees_cleanly)
{
    TEST_ALLOCATOR owned;
    TestAllocator_Init(&owned);
    STAGE_TITLE_ASSETS assets;
    CHECK(Result_Ignore(StageTitles_Load(&owned.allocator, &assets)));
    const char *const expected_stages[] = {
        "紫野　～ Purple Dawn",       "雷獣　～Midnight Rainstorm",
        "紅夢　～ Scarlet Dream",     "霊魔　～Revengeful Ghost",
        "封魔　～東方封魔録 and ...", "異空　～for Lunatic Gamers",
    };
    for (size_t i = 0; i < ARRAY_SIZE(expected_stages); i++) {
        CHECK(strcmp(assets.stages[i].text, expected_stages[i]) == 0);
    }
    const char *const expected_music[] = {
        "　 博麗　～Eastern Wind ", "　  She's in a temper!! ",
        "　   End of Daylight　  ", "   　 やみのちから　　  ",
        "　　　　　幻夢界　　　　", " 　　　死を賭して　　　 ",
        "ひもろぎ、むらさきにもえ", "   　 恋色マジック 　   ",
        "　東方封魔録　～幽幻乱舞", "   Complete Darkness    ",
        "   　 エキストララブ    ", "  戦車むすめのみるゆめ  ",
    };
    for (size_t i = 0; i < ARRAY_SIZE(expected_music); i++) {
        CHECK(strcmp(assets.music[i].text, expected_music[i]) == 0);
    }
    for (unsigned i = 0; i < 6; i++) {
        CHECK(assets.stages[i].count > 0);
    }
    for (unsigned i = 0; i < 12; i++) {
        CHECK(assets.music[i].count > 0);
    }
    CHECK_EQ_INT(assets.blank.count, 48);
    CHECK_EQ_INT(assets.music_blank.count, 26);
    StageTitles_Free(&assets);
    StageTitles_Free(&assets);
    CHECK_EQ_INT(owned.live_blocks, 0);
    CHECK_EQ_INT(owned.size_errors, 0);
}

TEST(stage_and_music_titles_follow_separate_world_boundaries)
{
    STAGE_TITLE_ASSETS assets;
    M_Assets(&g_HeapAllocator, &assets);
    for (uint8_t stage = 0; stage < 6; stage++) {
        Tram_Init();
        StageTitles_Begin(&assets, stage, false);
        CHECK_EQ_INT(m_Tram.codes[12 * 80 + 22], 0);
        StageTitles_Reveal();
        CHECK_EQ_INT(m_Tram.attributes[12 * 80 + (stage == 5 ? 16 : 22)], 0xc1);
        CHECK_EQ_INT(m_Tram.codes[13 * 80 + 18], 'A' + stage);
        StageTitles_Update(159);
        CHECK(m_Titles.banner_active);
        StageTitles_UpdateMusic();
        CHECK_EQ_INT(m_Titles.music_timer, 2);
        CHECK_EQ_INT(m_Tram.codes[23 * 80 + 26], 'G' + stage * 2);
        for (unsigned i = 0; i < 157; i++) {
            StageTitles_UpdateMusic();
        }
        CHECK_EQ_INT(m_Titles.music_timer, 159);
        CHECK_EQ_INT(m_Tram.codes[23 * 80 + 26], 'G' + stage * 2);
        StageTitles_UpdateMusic();
        CHECK_EQ_INT(m_Titles.music_timer, 0);
        CHECK_EQ_INT(m_Tram.codes[23 * 80 + 26], ' ');
        StageTitles_Update(160);
        CHECK(!m_Titles.banner_active);
        CHECK_EQ_INT(m_Tram.codes[13 * 80 + 18], ' ');
        m_Tram.codes[13 * 80 + 18] = 'Z';
        StageTitles_Update(160);
        CHECK_EQ_INT(m_Tram.codes[13 * 80 + 18], 'Z');
        StageTitles_Boss();
        StageTitles_UpdateMusic();
        CHECK_EQ_INT(m_Tram.codes[23 * 80 + 26], 'H' + stage * 2);
    }
    StageTitles_Free(&assets);
}

TEST(demo_keeps_blinking_banner_and_boss_entry_restarts_music_title)
{
    STAGE_TITLE_ASSETS assets;
    M_Assets(&g_HeapAllocator, &assets);

    Tram_Init();
    StageTitles_Begin(&assets, 2, true);
    StageTitles_Reveal();
    const TRAM before = m_Tram;
    StageTitles_UpdateMusic();
    StageTitles_Update(160);
    CHECK(memcmp(&m_Tram, &before, sizeof(m_Tram)) == 0);
    CHECK_EQ_INT(m_Tram.attributes[12 * 80 + 18], 0xc3);
    StageTitles_Boss();
    StageTitles_UpdateMusic();
    CHECK_EQ_INT(m_Tram.codes[23 * 80 + 26], 'L');
    StageTitles_Free(&assets);
}
