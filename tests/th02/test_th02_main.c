#include "tests/test.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/audio.h"
#include "th02/ending.h"
#include "th02/game_over.h"
#include "th02/high_score_entry.h"
#include "th02/high_scores.h"
#include "th02/main.h"
#include "th02/opening.h"
#include "th02/presenter.h"
#include "th02/program/private.h"
#include "th02/stage_bonus.h"
#include "th02/stage_effects.h"
#include "th02/text_font.h"

static HIGH_SCORE_ENTRY_EVENTS M_TestRegistration(const TH02_INPUT *input);
static void M_TestResume(void);
static GAME_OVER_EVENT M_TestGameOver(uint16_t keys);
static OPENING_EVENTS M_TestOpening(bool edge, uint16_t keys, bool skip);
static void M_TestBeginProgram(void)
{
}

static RESULT M_TestSave(VFS *files, const HIGH_SCORES *scores);
static VFS *M_TestUserFiles(void);
static void M_TestSettings(void);
static RESULT M_TestAudio(GAME_AUDIO *audio, PMD *pmd, size_t frames);
static void M_TestDraw(
    bool visible, const GAIJI *gaiji, const TRAM_FONT *font, uint16_t cell,
    bool blink);
static void M_TestClose(void);
static TRAM_FONT M_TestFont(const TEXT_FONT *font);

static void M_TestEnding(
    const ENDING_ASSETS *assets, const TH02_SESSION *session, STORY_AUDIO music,
    void *music_context, bool extra_unlocked);

static bool M_TestEndingUpdate(
    const TH02_INPUT *input, bool music_active, uint16_t measure);
static uint16_t M_TestMeasure(const PMD *pmd);

static bool M_TestBonus(const TH02_INPUT *input);
static void M_TestBeginProgram(void);

#define Ending_Begin M_TestEnding
#define StageBonus_Update M_TestBonus
#define Ending_Update M_TestEndingUpdate
#define Pmd_Measure M_TestMeasure
#define HighScoreEntry_Update M_TestRegistration
#define GameOver_Resume M_TestResume
#define GameOver_Update M_TestGameOver
#define Opening_Update M_TestOpening
#define Opening_BeginProgram M_TestBeginProgram
#define HighScoresFile_Save M_TestSave
#define Shell_GetUserFiles M_TestUserFiles
#define Shell_RequestSaveSettings M_TestSettings
#define GameAudio_Render M_TestAudio
#define Presenter_Draw M_TestDraw
#define Presenter_Close M_TestClose
#define TextFont_Tram M_TestFont
#define M_Render M_RenderClock
#include "th02/audio_clock.c"
#undef M_Render
#include "th02/main.c"
#include "th02/program/assets.c"
#include "th02/program/audio.c"
#include "th02/program/scenes.c"

#include <string.h>
#undef Ending_Begin
#undef StageBonus_Update
#undef Ending_Update
#undef Pmd_Measure
#undef HighScoreEntry_Update
#undef GameOver_Resume
#undef GameOver_Update
#undef Opening_BeginProgram
#undef Opening_Update
#undef HighScoresFile_Save
#undef Shell_GetUserFiles
#undef Shell_RequestSaveSettings
#undef GameAudio_Render
#undef Presenter_Draw
#undef Presenter_Close
#undef TextFont_Tram

typedef struct {
    char events[64];
    size_t count;
    bool fail_save;
    bool fail_audio;
    bool quit;
    bool advance_measure;
    bool ending_music;
    bool ending_done;
    bool bonus_done;
    bool clocked;
    bool entry_fade_ready;
    size_t audio_samples;
    uint16_t driver_measure;
    uint16_t ending_measure;
    MAIN_MENU_ACTION action;
    HIGH_SCORE_ENTRY_EVENTS registration;
    HIGH_SCORES written;
} MAIN_TEST;

static MAIN_TEST m_Test;

static void M_Event(const char event)
{
    CHECK(m_Test.count + 1 < sizeof(m_Test.events));
    m_Test.events[m_Test.count++] = event;
}

static void M_TestEnding(
    const ENDING_ASSETS *const assets, const TH02_SESSION *const session,
    const STORY_AUDIO music, void *const music_context,
    const bool extra_unlocked)
{
    M_Event('E');
    CHECK(assets == &p_Program->assets.ending);
    CHECK(session == &p_Program->scenes.session);
    CHECK_EQ_INT(session->stage, 127);
}

static bool M_TestEndingUpdate(
    const TH02_INPUT *const input, const bool music_active,
    const uint16_t measure)
{
    M_Event('U');
    m_Test.ending_music = music_active;
    m_Test.ending_measure = measure;
    return m_Test.ending_done;
}

static uint16_t M_TestMeasure(const PMD *const pmd)
{
    M_Event('M');
    CHECK(pmd == p_Program->audio.pmd);
    return m_Test.driver_measure;
}

static bool M_TestBonus(const TH02_INPUT *const input)
{
    M_Event('B');
    return m_Test.bonus_done;
}

static HIGH_SCORE_ENTRY_EVENTS M_TestRegistration(const TH02_INPUT *const input)
{
    M_Event('R');
    return m_Test.registration;
}

static void M_TestResume(void)
{
    M_Event('C');
}

static GAME_OVER_EVENT M_TestGameOver(const uint16_t keys)
{
    M_Event('G');
    return GAME_OVER_WAIT;
}

static OPENING_EVENTS M_TestOpening(
    const bool edge, const uint16_t keys, const bool skip)
{
    M_Event('O');
    m_Test.entry_fade_ready = MusicWait_Ready(&p_Program->audio.music_wait);
    return (OPENING_EVENTS) { .menu = { .quit = m_Test.quit,
                                        .action = m_Test.action } };
}

static RESULT M_TestSave(VFS *const files, const HIGH_SCORES *const scores)
{
    M_Event('S');
    CHECK(scores == &p_Program->scenes.scores);
    m_Test.written = *scores;
    FAIL_IF(m_Test.fail_save, "Authored score write failure");
    return OK;
}

static VFS *M_TestUserFiles(void)
{
    return nullptr;
}

static void M_TestSettings(void)
{
    M_Event('T');
}

static RESULT M_TestAudio(
    GAME_AUDIO *const audio, PMD *const pmd, const size_t frames)
{
    M_Event('A');
    if (!m_Test.clocked) {
        CHECK_EQ_INT(frames, 800);
    }
    m_Test.audio_samples += frames;
    if (m_Test.advance_measure) {
        if (m_Test.clocked) {
            m_Test.driver_measure = (uint16_t)(m_Test.audio_samples / 48);
        } else {
            m_Test.driver_measure++;
        }
    }
    FAIL_IF(m_Test.fail_audio, "Authored audio failure");
    return OK;
}

static void M_TestDraw(
    const bool visible, const GAIJI *const gaiji, const TRAM_FONT *const font,
    const uint16_t cell, const bool blink)
{
    M_Event('P');
}

static void M_TestClose(void)
{
}

static TRAM_FONT M_TestFont(const TEXT_FONT *const font)
{
    return (TRAM_FONT) {};
}

static bool M_Step(const TH02_INPUT *const input)
{
    return g_GameDesc.tick(input);
}

static void M_Begin(void)
{
    memset(&m_Test, 0, sizeof(m_Test));
    AudioClock_Init((PMD *)&m_Test, nullptr, &p_Program->audio.music_wait);
    MusicWait_Init(&p_Program->audio.music_wait);
    p_Program->scenes.save_scores = p_Program->scenes.save_settings =
        p_Program->failed = false;
    p_Program->initialized = true;
    p_Program->scenes.request = MAIN_MENU_NONE;
    p_Program->scenes.scene = TH02_SCENE_REGISTRATION;
    Arena_Init(&p_Program->assets.arena, "Tick save test", 1024);
    HighScores_Defaults(&p_Program->scenes.scores);
    HighScoreEntry_Init();
    VSyncIrq_BeginApplication(&p_Program->clock);
    m_Test.registration =
        (HIGH_SCORE_ENTRY_EVENTS) { HIGH_SCORE_ENTRY_COMPLETE, true };
}

TEST(registration_saves_after_the_rest_of_its_tick)
{
    M_Begin();
    p_Program->scenes.scores.sections[1].records[0].score = 123456;
    const TH02_INPUT input = {};
    CHECK(g_GameDesc.tick(&input));
    CHECK(strcmp(m_Test.events, "RCGAPS") == 0);
    CHECK(!p_Program->scenes.save_scores);
    CHECK_EQ_INT(m_Test.written.sections[1].records[0].score, 123456);
    CHECK(!Th02_Failed());
    CHECK(M_Step(&input));
    CHECK(strcmp(m_Test.events, "RCGAPSGAP") == 0);
    g_GameDesc.shutdown();
    CHECK(strcmp(m_Test.events, "RCGAPSGAP") == 0);
}

TEST(registration_retains_its_root_destination_across_waiting_ticks)
{
    M_Begin();
    m_Test.registration.result = HIGH_SCORE_ENTRY_FRAME;
    m_Test.registration.save_section = false;
    const TH02_INPUT input = {};
    for (unsigned ending = 0; ending < 2; ending++) {
        M_Registration(&input, ending != 0);
        const TH02_SCENE scene = ending != 0 ? TH02_SCENE_ENDING_REGISTRATION
                                             : TH02_SCENE_REGISTRATION;
        CHECK_EQ_INT(p_Program->scenes.scene, scene);
        CHECK(M_Step(&input));
        CHECK_EQ_INT(p_Program->scenes.scene, scene);
        TH02_VIEW view;
        Th02_View(&view);
        CHECK_EQ_INT(view.ending_program, ending != 0);
        CHECK(!p_Program->scenes.save_scores);
    }
    g_GameDesc.shutdown();
}

TEST(registration_snapshots_each_input_date)
{
    M_Begin();
    m_Test.registration =
        (HIGH_SCORE_ENTRY_EVENTS) { HIGH_SCORE_ENTRY_FRAME, false };
    p_Program->scenes.session.rank = 1;
    p_Program->scenes.session.shot_type = 0;
    p_Program->scenes.session.score = 34567;
    TH02_INPUT input = { .date = 20261231 };
    M_Registration(&input, true);
    HIGH_SCORE_RECORD *const records =
        p_Program->scenes.scores.sections[1].records;
    CHECK_EQ_INT(records[0].year, 2026);
    CHECK_EQ_INT(records[0].month, 12);
    CHECK_EQ_INT(records[0].day, 31);
    input.date = 20270101;
    CHECK(M_Step(&input));
    CHECK_EQ_INT(records[0].year, 2026);
    CHECK_EQ_INT(records[0].month, 12);
    CHECK_EQ_INT(records[0].day, 31);
    p_Program->scenes.session.score++;
    M_Registration(&input, false);
    CHECK_EQ_INT(records[0].year, 2027);
    CHECK_EQ_INT(records[0].month, 1);
    CHECK_EQ_INT(records[0].day, 1);
    CHECK_EQ_INT(records[1].year, 2026);
    CHECK_EQ_INT(records[1].month, 12);
    CHECK_EQ_INT(records[1].day, 31);
    g_GameDesc.shutdown();
}

TEST(main_score_converts_at_registration_and_stays_encoded_through_continue)
{
    M_Begin();
    p_Program->scenes.scene = TH02_SCENE_GAME_OVER;
    p_Program->scenes.session.rank = 1;
    p_Program->scenes.session.continues_used = 2;
    p_Program->scenes.session.score = 123452;
    m_Score.value = 12345;
    const TH02_INPUT input = {};
    CHECK(M_Step(&input));
    CHECK_EQ_INT(m_Score.value, 12345);
    m_Test.registration =
        (HIGH_SCORE_ENTRY_EVENTS) { HIGH_SCORE_ENTRY_FRAME, false };
    M_Registration(&input, false);
    CHECK_EQ_INT(m_Score.value, 123452);
    CHECK(M_Step(&input));
    CHECK_EQ_INT(m_Score.value, 123452);
    m_Test.registration.result = HIGH_SCORE_ENTRY_COMPLETE;
    CHECK(M_Step(&input));
    CHECK_EQ_INT(p_Program->scenes.scene, TH02_SCENE_GAME_OVER);
    CHECK_EQ_INT(m_Score.value, 123452);
    CHECK(M_Step(&input));
    CHECK_EQ_INT(m_Score.value, 123452);
    g_GameDesc.shutdown();
}

TEST(quitting_tick_flushes_scores_and_coalesces_settings_requests)
{
    M_Begin();
    p_Program->scenes.scene = TH02_SCENE_OPENING;
    m_Test.quit = true;
    p_Program->scenes.save_scores = true;
    M_SaveSettings(nullptr);
    M_SaveSettings(nullptr);
    const TH02_INPUT input = {};
    CHECK(!g_GameDesc.tick(&input));
    CHECK(strcmp(m_Test.events, "OST") == 0);
    CHECK(!p_Program->scenes.save_scores && !p_Program->scenes.save_settings);
    CHECK(!Th02_Failed());
    g_GameDesc.shutdown();
    CHECK(strcmp(m_Test.events, "OST") == 0);
}

TEST(save_and_audio_failures_stop_future_ticks_without_losing_the_final_save)
{
    for (unsigned audio = 0; audio < 2; audio++) {
        M_Begin();
        m_Test.fail_save = audio == 0;
        m_Test.fail_audio = audio != 0;
        const TH02_INPUT input = {};
        CHECK(!M_Step(&input));
        CHECK(strcmp(m_Test.events, audio != 0 ? "RCGAS" : "RCGAPS") == 0);
        const size_t events = m_Test.count;
        CHECK(Th02_Failed());
        CHECK(!M_Step(&input));
        CHECK_EQ_INT(m_Test.count, events);
        g_GameDesc.shutdown();
        CHECK(Th02_Failed());
        CHECK_EQ_INT(m_Test.count, events);
    }
}

TEST(shutdown_drains_pending_saves_once)
{
    M_Begin();
    p_Program->scenes.save_scores = p_Program->scenes.save_settings = true;
    g_GameDesc.shutdown();
    CHECK(strcmp(m_Test.events, "ST") == 0);
    CHECK(!Th02_Failed());
    g_GameDesc.shutdown();
    CHECK(strcmp(m_Test.events, "ST") == 0);
}

TEST(unavailable_scene_request_stops_without_advancing_another_tick)
{
    M_Begin();
    p_Program->scenes.scene = TH02_SCENE_OPENING;
    m_Test.action = MAIN_MENU_SCORES;
    const TH02_INPUT input = {};
    CHECK(!M_Step(&input));
    CHECK(Th02_Failed());
    CHECK(strcmp(m_Test.events, "O") == 0);
    CHECK(!M_Step(&input));
    CHECK(strcmp(m_Test.events, "O") == 0);
    g_GameDesc.shutdown();
}

TEST(replay_view_copies_root_values_without_advancing_the_game)
{
    M_Begin();
    p_Program->scenes.session.frame = -123456;
    p_Program->scenes.session.stage = 5;
    p_Program->scenes.session.reduce_effects = true;
    const TH02_SESSION session = p_Program->scenes.session;
    const VSYNC_COUNTERS counters = g_VSync;
    for (TH02_SCENE scene = TH02_SCENE_OPENING;
         scene <= TH02_SCENE_ENDING_REGISTRATION; scene++) {
        p_Program->scenes.scene = scene;
        TH02_VIEW view;
        Th02_View(&view);
        CHECK_EQ_INT(view.scene, scene);
        CHECK_EQ_INT(view.frame, -123456);
        CHECK_EQ_INT(view.stage, 5);
        CHECK(view.reduce_effects);
        CHECK(
            view.ending_program
            == (scene == TH02_SCENE_ENDING
                || scene == TH02_SCENE_ENDING_REGISTRATION));
        CHECK(memcmp(&counters, &g_VSync, sizeof(counters)) == 0);
        CHECK(
            memcmp(&session, &p_Program->scenes.session, sizeof(session)) == 0);
    }
    g_GameDesc.shutdown();
}

TEST(ending_handoff_retains_mima_delta_and_commits_only_the_extra_delta)
{
    for (unsigned extra = 0; extra < 2; extra++) {
        for (unsigned continued = 0; continued < 2; continued++) {
            M_Begin();
            memset(
                &p_Program->scenes.session, 0,
                sizeof(p_Program->scenes.session));
            p_Program->scenes.session.stage = 127;
            p_Program->scenes.session.rank =
                extra != 0 ? SESSION_EXTRA_RANK : 1;
            p_Program->scenes.session.shot_type = 1;
            p_Program->scenes.session.continues_used = continued;
            Score_Init();
            m_Score.value = 12345;
            m_Score.delta = 48;
            m_Score.transferred = 19;
            m_Score.lives = 2;
            m_Score.bombs = 3;
            M_Ending(extra != 0);
            CHECK_EQ_INT(p_Program->scenes.scene, TH02_SCENE_ENDING);
            CHECK(strcmp(m_Test.events, "E") == 0);
            CHECK_EQ_INT(m_Score.value, extra != 0 ? 12393 : 12345);
            CHECK_EQ_INT(m_Score.delta, extra != 0 ? 0 : 48);
            CHECK_EQ_INT(m_Score.transferred, extra != 0 ? 0 : 19);
            CHECK_EQ_INT(p_Program->scenes.session.lives, 2);
            CHECK_EQ_INT(p_Program->scenes.session.bombs, 3);
            CHECK_EQ_INT(
                p_Program->scenes.save_scores, extra == 0 && continued == 0);
            HIGH_SCORE_CLEARS clears;
            HighScores_GetClears(&p_Program->scenes.scores, &clears);
            CHECK_EQ_INT(clears.game[1], extra == 0 && continued == 0);
            g_GameDesc.shutdown();
        }
    }
}

TEST(ending_reads_enabled_driver_measures_before_each_audio_block)
{
    for (unsigned enabled = 0; enabled < 2; enabled++) {
        M_Begin();
        p_Program->scenes.scene = TH02_SCENE_ENDING;
        p_Program->audio.sound.active = enabled != 0;
        m_Test.advance_measure = true;
        m_Test.driver_measure = UINT16_MAX;
        const TH02_INPUT input = {};
        CHECK(M_Step(&input));
        CHECK_EQ_INT(m_Test.ending_music, enabled != 0);
        CHECK_EQ_INT(m_Test.ending_measure, enabled != 0 ? UINT16_MAX : 0);
        CHECK_EQ_INT(m_Test.driver_measure, 0);
        CHECK(strcmp(m_Test.events, enabled != 0 ? "MUAP" : "UAP") == 0);
        CHECK(M_Step(&input));
        CHECK_EQ_INT(m_Test.ending_measure, 0);
        CHECK_EQ_INT(m_Test.driver_measure, 1);
        CHECK(strcmp(m_Test.events, enabled != 0 ? "MUAPMUAP" : "UAPUAP") == 0);
        CHECK_EQ_INT(p_Program->scenes.scene, TH02_SCENE_ENDING);
        g_GameDesc.shutdown();
    }
}

TEST(ending_clock_advances_before_measure_and_keeps_fractional_samples)
{
    M_Begin();
    p_Program->scenes.scene = TH02_SCENE_ENDING;
    p_Program->audio.sound.active = true;
    m_Test.clocked = m_Test.advance_measure = true;
    const uint64_t origin = UINT64_C(5000000000);
    const TH02_CLOCK_SAMPLE clocks[] = {
        { TH02_CLOCK_FRAME, origin + 1000 },
        { TH02_CLOCK_ORIGIN, origin },
        { TH02_CLOCK_MEASURE, origin + 1500 },
    };
    const TH02_INPUT input = { .clocks = clocks,
                               .clock_count = ARRAY_SIZE(clocks) };
    CHECK(M_Step(&input));
    CHECK_EQ_STR(m_Test.events, "AAMUP");
    CHECK_EQ_INT(m_Test.ending_measure, 1);
    CHECK_EQ_INT(m_Test.audio_samples, 72);
    m_Test.count = 0;
    memset(m_Test.events, 0, sizeof(m_Test.events));
    const TH02_CLOCK_SAMPLE next[] = { { TH02_CLOCK_FRAME, origin + 2400 } };
    const TH02_INPUT next_input = { .clocks = next,
                                    .clock_count = ARRAY_SIZE(next) };
    CHECK(M_Step(&next_input));
    CHECK_EQ_STR(m_Test.events, "AMUP");
    CHECK_EQ_INT(m_Test.audio_samples, 115);
    CHECK_EQ_INT(p_Program->audio.music_wait.phase, 115 * 832);
    g_GameDesc.shutdown();
}

TEST(ending_clock_requires_its_recorded_origin)
{
    M_Begin();
    p_Program->scenes.scene = TH02_SCENE_ENDING;
    const TH02_CLOCK_SAMPLE clocks[] = { { TH02_CLOCK_FRAME, 1000 } };
    const TH02_INPUT input = { .clocks = clocks,
                               .clock_count = ARRAY_SIZE(clocks) };
    CHECK(!M_Step(&input));
    CHECK(Th02_Failed());
    CHECK_EQ_INT(m_Test.audio_samples, 0);
    g_GameDesc.shutdown();
}

TEST(frame_clock_advances_fades_before_scene_logic)
{
    M_Begin();
    p_Program->scenes.scene = TH02_SCENE_OPENING;
    m_Test.clocked = true;
    p_Program->audio.music_wait.volume = 240;
    p_Program->audio.music_wait.speed = 15;
    p_Program->audio.music_wait.timer_a = 7;
    const TH02_CLOCK_SAMPLE clocks[] = {
        { TH02_CLOCK_FRAME, 18500 },
        { TH02_CLOCK_ORIGIN, 0 },
    };
    const TH02_INPUT input = { .clocks = clocks,
                               .clock_count = ARRAY_SIZE(clocks) };
    CHECK(M_Step(&input));
    CHECK(m_Test.entry_fade_ready);
    CHECK_EQ_INT(m_Test.audio_samples, 888);
    CHECK_EQ_STR(m_Test.events, "AOP");
    g_GameDesc.shutdown();
}

TEST(ending_completion_enters_registration_in_the_same_tick)
{
    M_Begin();
    memset(&p_Program->scenes.session, 0, sizeof(p_Program->scenes.session));
    p_Program->scenes.session.stage = 127;
    p_Program->scenes.session.rank = 1;
    p_Program->scenes.session.score = 12345;
    p_Program->scenes.scene = TH02_SCENE_ENDING;
    p_Program->audio.sound.active = false;
    m_Test.ending_done = true;
    m_Test.registration.result = HIGH_SCORE_ENTRY_FRAME;
    m_Test.registration.save_section = false;
    const TH02_INPUT input = {};
    CHECK(M_Step(&input));
    CHECK(strcmp(m_Test.events, "URAP") == 0);
    CHECK_EQ_INT(p_Program->scenes.scene, TH02_SCENE_ENDING_REGISTRATION);
    TH02_VIEW view;
    Th02_View(&view);
    CHECK(view.ending_program);
    CHECK(!p_Program->scenes.save_scores);
    g_GameDesc.shutdown();
}

TEST(ending_registration_starts_the_opening_before_the_next_tick)
{
    M_Begin();
    p_Program->scenes.scene = TH02_SCENE_ENDING_REGISTRATION;
    p_Program->scenes.session.demo_num = 0;
    m_Test.registration =
        (HIGH_SCORE_ENTRY_EVENTS) { HIGH_SCORE_ENTRY_COMPLETE, false };
    const TH02_INPUT input = {};
    CHECK(M_Step(&input));
    CHECK(strcmp(m_Test.events, "ROAP") == 0);
    CHECK_EQ_INT(p_Program->scenes.scene, TH02_SCENE_OPENING);
    CHECK(!p_Program->scenes.save_scores);
    g_GameDesc.shutdown();
}

TEST(extra_completion_retains_clear_bits_and_saves_once_at_tick_end)
{
    for (uint8_t shot = 0; shot < 3; shot++) {
        for (unsigned failure = 0; failure < 2; failure++) {
            M_Begin();
            memset(
                &p_Program->scenes.session, 0,
                sizeof(p_Program->scenes.session));
            p_Program->scenes.session.stage = 5;
            p_Program->scenes.session.rank = SESSION_EXTRA_RANK;
            p_Program->scenes.session.shot_type = shot;
            p_Program->scenes.scores.sections[3].cleared =
                (int16_t)(0x120u | (7u ^ (1u << shot)));
            p_Program->scenes.scores.sections[4].cleared = 0x3456;
            Score_Init();
            m_Score.value = 500;
            m_Score.delta = 123;
            m_Score.transferred = 17;
            p_Program->scenes.scene = TH02_SCENE_BONUS;
            p_Program->audio.sound.active = false;
            m_Test.bonus_done = true;
            m_Test.fail_save = failure != 0;
            const TH02_INPUT input = {};
            CHECK_EQ_INT(g_GameDesc.tick(&input), failure == 0);
            CHECK(strcmp(m_Test.events, "BEAPS") == 0);
            CHECK(!p_Program->scenes.save_scores);
            CHECK_EQ_INT(m_Test.written.sections[3].cleared, 0x127);
            CHECK_EQ_INT(m_Test.written.sections[4].cleared, 0x3456);
            CHECK_EQ_INT(p_Program->scenes.session.stage, 127);
            CHECK_EQ_INT(p_Program->scenes.scene, TH02_SCENE_ENDING);
            CHECK_EQ_INT(m_Score.value, 500);
            CHECK_EQ_INT(m_Score.delta, 0);
            CHECK_EQ_INT(m_Score.transferred, 0);
            CHECK_EQ_INT(Th02_Failed(), failure != 0);
            if (failure != 0) {
                const size_t events = m_Test.count;
                CHECK(!M_Step(&input));
                CHECK_EQ_INT(m_Test.count, events);
            } else {
                CHECK(M_Step(&input));
                CHECK(strcmp(m_Test.events, "BEAPSUAP") == 0);
            }
            g_GameDesc.shutdown();
        }
    }
}

static void M_EndingStart(void *const context, TH02_SESSION *const session)
{
    M_Event('H');
    session->stage = 127;
    session->rank = *(const int8_t *)context;
    session->shot_type = 2;
    session->score = 12345;
    session->continues_used = 3;
}

TEST(replay_start_enters_the_ending_with_its_resident_values)
{
    const int8_t ranks[] = { 1, SESSION_EXTRA_RANK };
    for (size_t i = 0; i < ARRAY_SIZE(ranks); i++) {
        int8_t rank = ranks[i];
        M_Begin();
        p_Program->scenes.scene = TH02_SCENE_OPENING;
        m_Test.action = MAIN_MENU_START;
        p_Program->audio.sound.active = false;
        m_Score.value = 999;
        g_Th02Hooks.on_start = M_EndingStart;
        g_Th02Hooks.arg = &rank;
        const TH02_INPUT input = {};
        CHECK(M_Step(&input));
        CHECK(strcmp(m_Test.events, "OHEAP") == 0);
        CHECK_EQ_INT(p_Program->scenes.scene, TH02_SCENE_ENDING);
        CHECK_EQ_INT(p_Program->scenes.session.rank, ranks[i]);
        CHECK_EQ_INT(p_Program->scenes.session.score, 12345);
        CHECK_EQ_INT(p_Program->scenes.session.continues_used, 3);
        CHECK_EQ_INT(m_Score.value, 999);
        CHECK(!p_Program->scenes.save_scores);
        memset(&g_Th02Hooks, 0, sizeof(g_Th02Hooks));
        g_GameDesc.shutdown();
    }
}
