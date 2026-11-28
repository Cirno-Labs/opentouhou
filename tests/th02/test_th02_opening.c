#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_high_score_view.c"
#include "tests/th02/th02_menu.c"
#include "tests/th02/th02_music_room.c"
#include "tests/th02/th02_palette_fixture.h"
#include "th02/high_score_view/private.h"
#include "th02/music_room/private.h"
#include "th02/opening/private.h"
#include "th02/title_private.h"

#include "core/base/arena.h"

#include <string.h>

typedef struct {
    ARENA arena;

    GAIJI gaiji;

    TH02_SESSION session;
    TH02_SETTINGS settings;
    HIGH_SCORES scores;
    OPENING_ASSETS assets;
    OPTIONS_MENU_SOUND sound;
    TITLE_AUDIO_REQUEST audio[16];
    size_t audio_count;
    MUSIC_ROOM_AUDIO_REQUEST music_audio[6];
    size_t music_audio_count;
    size_t saves;
} FIXTURE;

static void M_SaveSettings(void *const context)
{
    FIXTURE *const fixture = context;
    fixture->saves++;
    CHECK_EQ_INT(fixture->session.shot_type, 2);
}

static void M_MusicAudio(
    void *const context, const MUSIC_ROOM_AUDIO_REQUEST *const request)
{
    FIXTURE *const fixture = context;
    CHECK(fixture->music_audio_count < ARRAY_SIZE(fixture->music_audio));
    fixture->music_audio[fixture->music_audio_count++] = *request;
}

static void M_RecordAudio(
    void *const context, const TITLE_AUDIO_REQUEST *const request)
{
    FIXTURE *const fixture = context;
    CHECK(fixture->audio_count < ARRAY_SIZE(fixture->audio));
    fixture->audio[fixture->audio_count++] = *request;
    if (request->action == TITLE_FORCE_EFFECT && request->effect == 6) {
        CHECK_EQ_INT(p_TitleFlash->frame, 0);
        CHECK_EQ_INT(p_Title->phase, TITLE_FLASHING);
    }
}

static void M_Begin(FIXTURE *const fixture, const HIGH_SCORES *const scores)
{
    const OPENING_AUDIO audio = { .title = M_RecordAudio, .context = fixture };
    Opening_Begin(
        &fixture->assets, &fixture->session, &fixture->settings, scores,
        &fixture->sound, nullptr, &audio, 1, &g_TestMusicRoomRender,
        &g_TestHighScoreViewRender, &g_ShotSelectRender, &g_MainMenuRender);
}

static void M_Init(FIXTURE *const fixture)
{
    memset(p_Palette, 0, sizeof((*p_Palette)));
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "opening scene", 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&fixture->arena);
    Video_Init(allocator);
    Tram_Init();
    Clip_Reset();
    Grcg_Init();
    Palette_Init();
    fixture->settings = (TH02_SETTINGS) { .difficulty = TH02_DIFFICULTY_NORMAL,
                                          .music = TH02_MUSIC_FM,
                                          .bombs = 3,
                                          .lives = 2 };
    Session_Init(&fixture->session);
    fixture->assets.logo = (BFNT) { .width = 64, .height = 16, .count = 16 };
    fixture->assets.logo.pixels = Allocator_AllocZ(allocator, 64 * 16 * 16);
    static const uint8_t pixels[16] = {};
    const TITLE_IMAGE image = { .view = { pixels, 16, 1 } };
    fixture->assets.title.menu_background =
        fixture->assets.title.title_background = image;
    for (size_t i = 0; i < 3; i++) {
        fixture->assets.title.flash[i] = image.view;
    }
    fixture->assets.title.glyphs = &fixture->gaiji;
    fixture->sound = (OPTIONS_MENU_SOUND) { .fm_available = true,
                                            .fm_possible = true,
                                            .active = true };
    TestHighScoreView_Observe(nullptr);
    TestMusicRoom_Observe(nullptr);
    M_Begin(fixture, nullptr);
}

static MAIN_MENU_EVENTS M_Edge(FIXTURE *const fixture, const uint16_t keys)
{
    VSyncIrq_VSync();
    return Opening_Update(true, keys, false).menu;
}

static void M_Menu(FIXTURE *const fixture)
{
    for (size_t i = 0; i < 250 && p_Opening->program.phase != OPENING_MENU;
         i++) {
        VSyncIrq_VSync();
        const MAIN_MENU_EVENTS events =
            Opening_Update(true, 0, p_Opening->program.phase == OPENING_LOGO)
                .menu;
        CHECK_EQ_INT(events.action, MAIN_MENU_NONE);
    }
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MENU);
}

TEST(logo_title_and_menu_follow_the_clock_and_emit_audio_before_drawing)
{
    FIXTURE fixture;
    M_Init(&fixture);
    VSYNC_CLOCK clock = {};
    uint64_t ticks = 0;
    while (p_Opening->program.phase != OPENING_MENU && ticks < 800) {
        ticks++;
        const bool edge = VSync_Advance(&clock);
        if (edge) {
            VSyncIrq_VSync();
        }
        const OPENING_EVENTS opening = Opening_Update(edge, 0, false);
        const MAIN_MENU_EVENTS events = opening.menu;
        CHECK_EQ_INT(
            opening.hide_text, p_Opening->program.phase == OPENING_LOGO);
        CHECK_EQ_INT(events.action, MAIN_MENU_NONE);
        CHECK(!events.quit);
    }
    CHECK_EQ_INT(ticks, 693);
    CHECK_EQ_INT(g_VSync.edges, 651);
    CHECK_EQ_INT(g_VSync.count2, 189);
    CHECK_EQ_INT(fixture.session.frame, 1);
    CHECK_EQ_INT(fixture.audio_count, 5);
    CHECK_EQ_INT(fixture.audio[0].action, TITLE_STOP_SONG);
    for (size_t i = 1; i < fixture.audio_count; i++) {
        CHECK_EQ_INT(fixture.audio[i].action, i - 1);
    }
    CHECK_EQ_INT(fixture.audio[3].effect, 6);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 0);
    CHECK(p_Opening->program.wait.kind == WAIT_DELAY);
    CHECK_EQ_INT(g_VSync.count1, 0);
    Arena_Free(&fixture.arena);
}

TEST(logo_skip_polls_after_two_waits_and_quit_observes_its_last_wait)
{
    FIXTURE fixture;
    M_Init(&fixture);
    Opening_Update(false, 0, true);
    Opening_Update(false, 0, false);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_LOGO);
    VSyncIrq_VSync();
    Opening_Update(true, 0, true);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_LOGO);
    M_Edge(&fixture, 0);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_LOGO);
    M_Menu(&fixture);
    const MAIN_MENU_EVENTS pending = M_Edge(&fixture, GAME_KEY_CANCEL);
    CHECK(pending.wait_frame);
    CHECK(!pending.quit);
    CHECK(p_Opening->program.quit_pending);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MENU);
    CHECK(!Opening_Update(false, 0, false).menu.quit);
    CHECK(M_Edge(&fixture, 0).quit);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_DONE);
    CHECK(Opening_Update(false, 0, false).menu.quit);
    Arena_Free(&fixture.arena);
}

TEST(logo_skip_resets_the_visible_page_for_title)
{
    FIXTURE fixture;
    M_Init(&fixture);
    for (size_t i = 0; i < 3; i++) {
        M_Edge(&fixture, 0);
    }
    VSyncIrq_VSync();
    Opening_Update(true, 0, true);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_TITLE);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&fixture.arena);
}

TEST(logo_palette_advances_before_the_next_wait)
{
    FIXTURE fixture;
    M_Init(&fixture);
    fixture.assets.logo.palette[1] = (GFX_COLOR) { 255, 255, 255, 255 };
    for (size_t i = 0; i < 6; i++) {
        M_Edge(&fixture, 0);
    }
    CHECK_EQ_INT(g_Video.palette[1].red, 17);
    Opening_Update(false, 0, false);
    CHECK_EQ_INT(g_Video.palette[1].red, 17);
    Arena_Free(&fixture.arena);
}

TEST(external_menu_actions_suspend_once_and_resume_before_the_outer_wait)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Menu(&fixture);
    M_Edge(&fixture, GAME_KEY_DOWN);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 2);
    M_Edge(&fixture, 0);
    const int32_t before = fixture.session.frame;
    CHECK_EQ_INT(M_Edge(&fixture, GAME_KEY_OK).action, MAIN_MENU_SCORES);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_SUSPENDED);
    CHECK_EQ_INT(M_Edge(&fixture, 0).action, MAIN_MENU_NONE);
    CHECK_EQ_INT(fixture.session.frame, before);
    const MAIN_MENU_EVENTS resumed = Opening_Resume(0);
    CHECK(resumed.wait_frame);
    CHECK_EQ_INT(fixture.session.frame, before + 1);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MENU);
    CHECK(!p_MainMenu->initialized);
    Opening_Update(false, 0, false);
    CHECK(!p_MainMenu->initialized);
    M_Edge(&fixture, 0);
    CHECK(p_MainMenu->initialized);
    CHECK_EQ_INT(fixture.session.frame, before + 2);
    CHECK_EQ_INT(Opening_Resume(0).action, MAIN_MENU_NONE);
    Arena_Free(&fixture.arena);
}

TEST(options_enter_after_the_menu_wait_and_return_after_their_own_wait)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Menu(&fixture);
    M_Edge(&fixture, GAME_KEY_DOWN);
    M_Edge(&fixture, 0);
    M_Edge(&fixture, GAME_KEY_DOWN);
    M_Edge(&fixture, 0);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 3);
    CHECK_EQ_INT(M_Edge(&fixture, GAME_KEY_OK).action, MAIN_MENU_NONE);
    CHECK(p_MainMenu->nav.in_option);
    CHECK(!MainMenu_Sample().options_ready);
    Opening_Update(false, 0, false);
    CHECK(!MainMenu_Sample().options_ready);
    M_Edge(&fixture, GAME_KEY_OK);
    CHECK(MainMenu_Sample().options_ready);
    CHECK(!MainMenu_Sample().options_input);
    M_Edge(&fixture, 0);
    M_Edge(&fixture, GAME_KEY_RIGHT);
    CHECK_EQ_INT(fixture.settings.difficulty, TH02_DIFFICULTY_HARD);
    M_Edge(&fixture, 0);
    M_Edge(&fixture, GAME_KEY_CANCEL);
    CHECK(!p_MainMenu->nav.in_option);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 3);
    CHECK(!p_MainMenu->initialized);
    Opening_Update(false, 0, false);
    CHECK(!p_MainMenu->initialized);
    M_Edge(&fixture, 0);
    CHECK(p_MainMenu->initialized);
    CHECK_EQ_INT(fixture.settings.difficulty, TH02_DIFFICULTY_HARD);
    Arena_Free(&fixture.arena);
}

TEST(score_viewer_holds_twenty_frames_then_resumes_with_zero_keys)
{
    FIXTURE fixture;
    M_Init(&fixture);
    p_Opening->scores = &fixture.scores;
    M_Menu(&fixture);
    M_Edge(&fixture, GAME_KEY_DOWN);
    M_Edge(&fixture, 0);
    const int32_t before = fixture.session.frame;
    CHECK_EQ_INT(M_Edge(&fixture, GAME_KEY_OK).action, MAIN_MENU_NONE);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_SCORES);
    CHECK_EQ_INT(p_Viewer->frame, 1);
    CHECK(!p_Viewer->input_allowed);
    CHECK_EQ_INT(fixture.session.frame, before);
    Opening_Update(false, 0, false);
    CHECK(!p_Viewer->input_allowed);
    M_Edge(&fixture, 0);
    CHECK(p_Viewer->input_allowed);
    M_Edge(&fixture, GAME_KEY_CANCEL);
    CHECK_EQ_INT(p_Viewer->phase, HIGH_SCORE_VIEW_HOLD);
    CHECK_EQ_INT(p_Opening->program.wait.count, 20);
    for (int frame = 0; frame < 19; frame++) {
        CHECK(!M_Edge(&fixture, GAME_KEY_CANCEL).quit);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_SCORES);
        CHECK_EQ_INT(fixture.session.frame, before);
    }
    CHECK(!M_Edge(&fixture, GAME_KEY_CANCEL).quit);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MENU);
    CHECK_EQ_INT(p_Viewer->phase, HIGH_SCORE_VIEW_DONE);
    CHECK_EQ_INT(fixture.session.frame, before + 1);
    CHECK(!p_MainMenu->initialized);
    CHECK(!p_MainMenu->suspended);
    CHECK(!p_MainMenu->quit);
    M_Edge(&fixture, GAME_KEY_CANCEL);
    CHECK(p_MainMenu->initialized);
    CHECK(!p_MainMenu->input_allowed);
    CHECK(!p_MainMenu->quit);
    M_Edge(&fixture, 0);
    CHECK(p_MainMenu->input_allowed);
    CHECK(M_Edge(&fixture, GAME_KEY_CANCEL).wait_frame);
    CHECK(M_Edge(&fixture, 0).quit);
    Arena_Free(&fixture.arena);
}

TEST(music_room_waits_for_release_and_keeps_playing_track_across_visits)
{
    FIXTURE fixture;
    M_Init(&fixture);
    fixture.assets.music.background.view.pixels = fixture.assets.logo.pixels;

    p_Opening->audio.music = M_MusicAudio;
    fixture.sound.midi_possible = true;
    fixture.sound.midi_active = true;
    M_Menu(&fixture);
    for (int i = 0; i < 3; i++) {
        M_Edge(&fixture, GAME_KEY_DOWN);
        M_Edge(&fixture, 0);
    }
    CHECK_EQ_INT(p_MainMenu->nav.selection, 4);
    const int32_t before = fixture.session.frame;
    CHECK_EQ_INT(M_Edge(&fixture, GAME_KEY_OK).action, MAIN_MENU_NONE);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MUSIC);
    CHECK_EQ_INT(p_Room->phase, MUSIC_ROOM_RELEASE);
    CHECK_EQ_INT(p_Opening->program.wait.count, 1);
    CHECK_EQ_INT(fixture.music_audio_count, 0);
    CHECK_EQ_INT(fixture.session.frame, before);
    Opening_Update(false, 0, false);
    CHECK_EQ_INT(p_Room->phase, MUSIC_ROOM_RELEASE);
    M_Edge(&fixture, 0);
    M_Edge(&fixture, GAME_KEY_DOWN);
    CHECK_EQ_INT(p_Room->selection, 1);
    M_Edge(&fixture, 0);
    M_Edge(&fixture, GAME_KEY_OK);
    CHECK_EQ_INT(p_Room->playing, 1);
    CHECK_EQ_INT(fixture.music_audio_count, 3);
    CHECK_EQ_INT(fixture.music_audio[0].action, MUSIC_ROOM_LOAD);
    CHECK(fixture.music_audio[0].midi);
    CHECK(!fixture.music_audio[1].midi);
    CHECK_EQ_INT(fixture.music_audio[2].action, MUSIC_ROOM_PLAY);
    CHECK(fixture.music_audio[2].midi);
    CHECK_EQ_INT(fixture.music_audio[2].track, 1);
    CHECK_EQ_INT(p_Room->phase, MUSIC_ROOM_COMMENT);
    M_Edge(&fixture, 0);
    M_Edge(&fixture, GAME_KEY_CANCEL);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MUSIC);
    CHECK_EQ_INT(fixture.session.frame, before);
    M_Edge(&fixture, 0);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MENU);
    CHECK_EQ_INT(fixture.session.frame, before + 1);
    CHECK(!p_MainMenu->initialized);
    CHECK(!p_MainMenu->quit);
    M_Edge(&fixture, 0);
    fixture.sound.midi_active = false;
    M_Edge(&fixture, GAME_KEY_OK);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_MUSIC);
    CHECK_EQ_INT(p_Room->selection, 1);
    CHECK(!p_Room->midi_active);
    CHECK_EQ_INT(fixture.music_audio_count, 3);
    Arena_Free(&fixture.arena);
}

TEST(start_and_extra_apply_the_delay_before_saving_and_observe_both_fades)
{
    for (int extra = 0; extra < 2; extra++) {
        FIXTURE fixture;
        M_Init(&fixture);
        HighScores_Defaults(&fixture.scores);
        M_Begin(&fixture, &fixture.scores);
        for (uint8_t shot = 0; shot < 3; shot++) {
            CHECK_EQ_INT(
                HighScores_RecordClear(&fixture.scores, shot, false, 0), shot);
        }
        M_Menu(&fixture);
        if (extra != 0) {
            M_Edge(&fixture, GAME_KEY_DOWN);
            M_Edge(&fixture, 0);
        }
        const SHOT_SELECT_ASSETS assets = {};
        p_Opening->shots = &assets;
        p_Opening->audio.save_settings = M_SaveSettings;
        fixture.session.stage = 7;
        fixture.session.score = 123;
        fixture.session.shot_type = 2;
        const int32_t frame = fixture.session.frame;
        CHECK_EQ_INT(M_Edge(&fixture, GAME_KEY_OK).action, MAIN_MENU_NONE);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_START);
        CHECK_EQ_INT(fixture.audio[fixture.audio_count - 1].effect, 13);
        CHECK_EQ_INT(fixture.session.stage, 7);
        CHECK_EQ_INT(p_Opening->program.wait.count, 20);
        for (int i = 0; i < 19; i++) {
            M_Edge(&fixture, GAME_KEY_OK);
            CHECK_EQ_INT(p_Opening->program.phase, OPENING_START);
            CHECK_EQ_INT(fixture.saves, 0);
        }
        M_Edge(&fixture, 0);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_SHOT_FADE_IN);
        CHECK_EQ_INT(fixture.saves, 1);
        CHECK_EQ_INT(fixture.session.stage, extra != 0 ? 5 : 0);
        CHECK_EQ_INT(fixture.session.rank, extra != 0 ? 4 : 1);
        CHECK_EQ_INT(fixture.session.score, 0);
        CHECK_EQ_INT(fixture.session.shot_type, 1);
        CHECK_EQ_INT(fixture.session.bombs, extra != 0 ? 1 : 3);
        CHECK_EQ_INT(fixture.session.lives, 2);
        for (int i = 0; i < 34; i++) {
            M_Edge(&fixture, 0);
            CHECK_EQ_INT(p_Opening->program.phase, OPENING_SHOT_FADE_IN);
        }
        M_Edge(&fixture, 0);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_SHOTS);
        CHECK_EQ_INT(p_Palette->tone, 100);
        M_Edge(&fixture, GAME_KEY_OK);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_SHOT_FADE_OUT);
        CHECK_EQ_INT(fixture.session.frame, frame);
        for (int i = 0; i < 17; i++) {
            CHECK_EQ_INT(M_Edge(&fixture, 0).action, MAIN_MENU_NONE);
            CHECK_EQ_INT(p_Opening->program.phase, OPENING_SHOT_FADE_OUT);
        }
        CHECK_EQ_INT(
            M_Edge(&fixture, 0).action,
            extra != 0 ? MAIN_MENU_EXTRA : MAIN_MENU_START);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_SUSPENDED);
        CHECK_EQ_INT(p_Palette->tone, 0);
        CHECK_EQ_INT(fixture.saves, 1);
        Arena_Free(&fixture.arena);
    }
}

TEST(program_return_uses_cached_title_without_replaying_launcher_logo)
{
    FIXTURE f;
    M_Init(&f);
    f.sound.midi_possible = true;
    f.sound.midi_active = true;
    p_Opening->logo = nullptr;
    g_VSync.edges = 735;
    g_VSync.count1 = 900;
    g_VSync.count2 = 901;
    g_Video.pages[0][0] = 7;
    g_Video.pages[1][0] = 8;
    Video_ShowPage(1);
    Video_AccessPage(1);
    Video_SetSplit(23, 15080, 8);
    Title_Begin(&f.assets.title, 0, true, true);
    Opening_BeginProgram();
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_TITLE);
    CHECK_EQ_INT(p_Title->phase, TITLE_PREPARE);
    CHECK(p_Title->midi_possible);
    CHECK(p_Title->midi_active);
    CHECK(p_Title->assets == &f.assets.title);
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, 0);
    CHECK_EQ_INT(g_VSync.edges, 735);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 400);
    CHECK_EQ_INT(g_Video.first_address, 0);
    CHECK_EQ_INT(g_Video.second_address, 0);
    CHECK_EQ_INT(g_Video.pages[0][0], 7);
    CHECK_EQ_INT(g_Video.pages[1][0], 8);
    CHECK_EQ_INT(f.audio_count, 1);
    CHECK_EQ_INT(f.audio[0].action, TITLE_STOP_SONG);
    Arena_Free(&f.arena);
}

TEST(
    demo_return_times_out_scores_before_title_without_resetting_program_counters)
{
    FIXTURE f;
    M_Init(&f);
    p_Opening->scores = &f.scores;
    Opening_DemoStarted(2);
    g_VSync.edges = 400;
    Palette_SetTone(0);
    Opening_BeginDemoReturn(0);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_RETURN_SCORES);
    CHECK_EQ_INT(p_Viewer->duration, 350);
    CHECK_EQ_INT(p_Viewer->frame, 1);
    CHECK_EQ_INT(p_Palette->tone, 100);
    CHECK_EQ_INT(f.audio_count, 0);
    for (unsigned edge = 0; edge < 370; edge++) {
        M_Edge(&f, 0);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_RETURN_SCORES);
        CHECK_EQ_INT(f.audio_count, 0);
    }
    M_Edge(&f, 0);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_TITLE);
    CHECK_EQ_INT(p_Title->door_step, 1);
    CHECK_EQ_INT(g_VSync.count2, 371);
    CHECK_EQ_INT(g_VSync.edges, 771);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(f.audio_count, 1);
    M_Menu(&f);
    CHECK_EQ_INT(p_MainMenu->demo, 3);
    CHECK_EQ_INT(p_Title->demo_num, 0);
    CHECK(!p_MainMenu->quit);
    CHECK_EQ_INT(f.audio_count, 2);
    CHECK_EQ_INT(f.audio[0].action, TITLE_LOAD_EFFECTS);
    CHECK_EQ_INT(f.audio[1].action, TITLE_FORCE_EFFECT);
    Arena_Free(&f.arena);
}

TEST(demo_return_keeps_held_keys_locked_until_release_and_discards_exit_key)
{
    FIXTURE f;
    M_Init(&f);
    p_Opening->scores = &f.scores;
    Opening_DemoStarted(1);
    Opening_BeginDemoReturn(GAME_KEY_CANCEL);
    for (unsigned edge = 0; edge < 3; edge++) {
        M_Edge(&f, GAME_KEY_CANCEL);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_RETURN_SCORES);
        CHECK(!p_Viewer->input_allowed);
    }
    M_Edge(&f, 0);
    CHECK(p_Viewer->input_allowed);
    M_Edge(&f, GAME_KEY_CANCEL);
    CHECK_EQ_INT(p_Viewer->phase, HIGH_SCORE_VIEW_HOLD);
    CHECK_EQ_INT(f.audio_count, 0);
    for (unsigned edge = 0; edge < 19; edge++) {
        M_Edge(&f, GAME_KEY_CANCEL);
        CHECK_EQ_INT(p_Opening->program.phase, OPENING_RETURN_SCORES);
    }
    M_Edge(&f, GAME_KEY_CANCEL);
    CHECK_EQ_INT(p_Opening->program.phase, OPENING_TITLE);
    CHECK_EQ_INT(g_VSync.count2, 25);
    M_Menu(&f);
    CHECK(!p_MainMenu->quit);
    CHECK_EQ_INT(p_Title->demo_num, 0);
    Arena_Free(&f.arena);
}

TEST(program_return_refreshes_unlock_from_exact_score_markers)
{
    FIXTURE f;
    M_Init(&f);
    HighScores_Defaults(&f.scores);
    M_Begin(&f, &f.scores);
    CHECK_EQ_INT(HighScores_RecordClear(&f.scores, 0, false, 0), 0);
    CHECK_EQ_INT(HighScores_RecordClear(&f.scores, 1, false, 0), 1);
    CHECK_EQ_INT(HighScores_RecordClear(&f.scores, 2, false, 1), -1);
    M_Menu(&f);
    M_Edge(&f, 0);
    M_Edge(&f, GAME_KEY_DOWN);
    CHECK_EQ_INT(MainMenu_Sample().selection, 2);
    CHECK_EQ_INT(HighScores_RecordClear(&f.scores, 2, false, 0), 2);
    M_Edge(&f, 0);
    M_Edge(&f, GAME_KEY_UP);
    CHECK_EQ_INT(MainMenu_Sample().selection, 0);
    Opening_BeginProgram();
    M_Menu(&f);
    M_Edge(&f, 0);
    M_Edge(&f, GAME_KEY_DOWN);
    CHECK_EQ_INT(MainMenu_Sample().selection, 1);
    for (size_t shot = 0; shot < 3; shot++) {
        CHECK(p_Opening->clears.game[shot]);
        CHECK(!p_Opening->clears.extra[shot]);
    }
    M_Edge(&f, 0);
    CHECK_EQ_INT(M_Edge(&f, GAME_KEY_OK).action, MAIN_MENU_EXTRA);
    CHECK_EQ_INT(HighScores_RecordClear(&f.scores, 1, true, 0), 3);
    f.scores.sections[1].cleared++;
    Opening_BeginProgram();
    M_Menu(&f);
    M_Edge(&f, 0);
    M_Edge(&f, GAME_KEY_DOWN);
    CHECK_EQ_INT(MainMenu_Sample().selection, 2);
    CHECK(!p_Opening->clears.game[1]);
    CHECK(p_Opening->clears.extra[1]);
    Arena_Free(&f.arena);
}
