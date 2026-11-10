#include "th06/ascii/pause_menu.h"

#include "test.h"

#include "th06/ascii/text.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"

ASCII_TEXT g_AsciiText;
GAME_MANAGER g_GameManager;
BUTTONS g_Buttons;

static SCENE m_Requested;
static uint32_t m_ShownAtRequest;
static bool m_ContinueAtRequest;
static int32_t m_StepCount;
static ANM_VM *m_Steps[16];
static int32_t m_StartCount;
static ANM_VM *m_StartVms[8];
static int32_t m_StartScripts[8];
static int32_t m_SetSprite;

static void M_Reset(void)
{
    g_AsciiText = (ASCII_TEXT) {};
    g_GameManager = (GAME_MANAGER) {};
    g_Buttons = (BUTTONS) {};
    m_Requested = SCENE_NONE;
    m_ShownAtRequest = 0;
    m_ContinueAtRequest = false;
    m_StepCount = 0;
    m_StartCount = 0;
    m_SetSprite = -1;
}

static void M_ClearStepCount(void)
{
    m_StepCount = 0;
}

static void M_ShowSprites(PAUSE_MENU *const menu)
{
    for (int32_t i = 0; i < ASCII_MENU_SPRITE_COUNT; i++) {
        menu->sprites[i].shown = true;
    }
}

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
    m_ShownAtRequest = g_GameManager.score.shown;
    m_ContinueAtRequest = g_GameManager.mode.in_continue_menu;
}

void Anm_StartScript(ANM_VM *const vm, const int32_t script)
{
    m_StartVms[m_StartCount] = vm;
    m_StartScripts[m_StartCount++] = script;
    vm->script_index = (int16_t)script;
}

bool Anm_Step(ANM_VM *const vm)
{
    m_Steps[m_StepCount++ % 16] = vm;
    return false;
}

void Anm_SetSprite(ANM_VM *const vm, const int32_t sprite)
{
    m_SetSprite = sprite;
    vm->sprite = (int16_t)sprite;
}

TEST(pause_starts_all_lines_and_falls_through_to_resume)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    CHECK_EQ_INT(menu->state_frames, 1);
    CHECK_EQ_INT(m_StartCount, 6);
    CHECK_EQ_INT(m_StepCount, 6);
    for (int32_t i = 0; i < 6; i++) {
        CHECK(m_StartVms[i] == &menu->sprites[i]);
        CHECK_EQ_INT(m_StartScripts[i], i + 2);
        CHECK_EQ_INT(menu->sprites[i].pending_interrupt, i < 3 ? 1 : 0);
        CHECK(m_Steps[i] == &menu->sprites[i]);
    }
    CHECK_EQ_INT(menu->sprites[1].color, 0xffff8080);
    CHECK(menu->sprites[1].scale_x == 1.7f);
    CHECK(menu->sprites[1].pos_offset.x == -4.0f);
}

TEST(pause_capture_starts_and_steps_the_background)
{
    M_Reset();
    g_AsciiText.capture_background = true;
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(m_StartCount, 7);
    CHECK_EQ_INT(m_StartScripts[6], 0x718);
    CHECK(m_StartVms[6] == &menu->background);
    CHECK_EQ_INT(m_StepCount, 7);
    CHECK(m_Steps[6] == &menu->background);
    CHECK(menu->background.pos.x == 32.0f);
    CHECK(menu->background.pos.y == 16.0f);
}

TEST(pause_resume_waits_four_frames_for_selection_and_twenty_to_close)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    menu->state = 1;
    menu->state_frames = 3;
    g_GameManager.mode.paused = 2;
    g_Buttons.word = 1;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    CHECK_EQ_INT(menu->state_frames, 4);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 3);
    CHECK_EQ_INT(menu->state_frames, 1);
    CHECK_EQ_INT(menu->background.pending_interrupt, 1);
    for (int32_t i = 0; i < 3; i++) {
        CHECK_EQ_INT(menu->sprites[i].pending_interrupt, 2);
    }
    g_Buttons.word = 0;
    menu->state_frames = 19;
    M_ShowSprites(menu);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(g_GameManager.mode.paused, 2);
    M_ClearStepCount();
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 0);
    CHECK_EQ_INT(menu->state_frames, 21);
    CHECK_EQ_INT(g_GameManager.mode.paused, 0);
    CHECK_EQ_INT(m_StepCount, 6);
    for (int32_t i = 0; i < 6; i++) {
        CHECK(!menu->sprites[i].shown);
    }
}

TEST(pause_quit_overrides_resume_and_waits_twenty_frames)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    g_GameManager.mode.paused = 2;
    M_ShowSprites(menu);
    menu->sprites[4].shown = false;
    g_Buttons.word = 8 | 0x200;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 6);
    CHECK_EQ_INT(menu->state_frames, 1);
    CHECK_EQ_INT(menu->sprites[0].pending_interrupt, 2);
    CHECK_EQ_INT(menu->sprites[4].pending_interrupt, 0);
    CHECK_EQ_INT(menu->background.pending_interrupt, 1);
    g_Buttons.word = 0;
    menu->state_frames = 19;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(m_Requested, SCENE_NONE);
    M_ClearStepCount();
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK_EQ_INT(menu->state_frames, 21);
    CHECK_EQ_INT(m_StepCount, 6);
}

TEST(pause_quit_confirmation_can_return_to_the_quit_choice)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    menu->state = 2;
    menu->state_frames = 4;
    g_Buttons.word = 1;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 5);
    CHECK_EQ_INT(menu->state_frames, 1);
    for (int32_t i = 0; i < 6; i++) {
        CHECK_EQ_INT(menu->sprites[i].pending_interrupt, i < 3 ? 2 : 1);
    }
    menu->state_frames = 4;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    CHECK_EQ_INT(menu->state_frames, 1);
    for (int32_t i = 0; i < 6; i++) {
        CHECK_EQ_INT(menu->sprites[i].pending_interrupt, i < 3 ? 1 : 2);
    }
}

TEST(pause_quit_confirmation_can_select_yes)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    menu->state = 5;
    menu->state_frames = 4;
    g_Buttons.word = 0x10;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 4);
    g_Buttons.word = 1;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 6);
    CHECK_EQ_INT(menu->state_frames, 1);
    for (int32_t i = 3; i < 6; i++) {
        CHECK_EQ_INT(menu->sprites[i].pending_interrupt, 2);
    }
}

TEST(practice_continue_exits_before_replay_and_copies_score_before_request)
{
    M_Reset();
    g_GameManager.mode.in_practice = true;
    g_GameManager.mode.in_replay = true;
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.score.value = 77;
    g_GameManager.score.shown = 12;
    PauseMenu_UpdateContinueMenu(&g_AsciiText.continue_menu);
    CHECK_EQ_INT(m_Requested, SCENE_GAME_RESULT);
    CHECK_EQ_INT(m_ShownAtRequest, 77);
    CHECK(!m_ContinueAtRequest);
    CHECK_EQ_INT(m_StepCount, 0);
}

TEST(replay_continue_requests_replay_end_before_copying_score)
{
    M_Reset();
    g_GameManager.mode.in_replay = true;
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.score.value = 77;
    g_GameManager.score.shown = 12;
    PauseMenu_UpdateContinueMenu(&g_AsciiText.continue_menu);
    CHECK_EQ_INT(m_Requested, SCENE_REPLAY_END);
    CHECK_EQ_INT(m_ShownAtRequest, 12);
    CHECK_EQ_INT(g_GameManager.score.shown, 77);
    CHECK(!m_ContinueAtRequest);
    CHECK_EQ_INT(m_StepCount, 0);
    CHECK_EQ_INT(g_AsciiText.continue_menu.state_frames, 0);
}

TEST(extra_and_three_used_continues_go_to_results)
{
    for (int32_t scenario = 0; scenario < 2; scenario++) {
        M_Reset();
        g_GameManager.mode.in_continue_menu = true;
        g_GameManager.run.difficulty = scenario == 0 ? 4 : 1;
        g_GameManager.stats.continues_used = scenario == 0 ? 0 : 3;
        g_GameManager.score.value = 123;
        PauseMenu_UpdateContinueMenu(&g_AsciiText.continue_menu);
        CHECK_EQ_INT(m_Requested, SCENE_GAME_RESULT);
        CHECK_EQ_INT(g_GameManager.score.shown, 123);
        CHECK_EQ_INT(m_StepCount, 0);
        CHECK(!g_GameManager.mode.in_continue_menu);
    }
}

TEST(continue_starts_in_state_two_but_uses_state_one_visuals_once)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.continue_menu;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    CHECK_EQ_INT(menu->state_frames, 1);
    CHECK_EQ_INT(m_StartCount, 4);
    CHECK_EQ_INT(m_StartScripts[0], 8);
    CHECK_EQ_INT(m_StartScripts[1], 9);
    CHECK_EQ_INT(m_StartScripts[2], 6);
    CHECK_EQ_INT(m_StartScripts[3], 7);
    CHECK_EQ_INT(menu->sprites[2].color, 0xffff8080);
    CHECK_EQ_INT(menu->sprites[3].color, 0x80808080);
    CHECK_EQ_INT(m_StepCount, 4);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->sprites[2].color, 0x80808080);
    CHECK_EQ_INT(menu->sprites[3].color, 0xffff8080);
}

TEST(continue_no_waits_thirty_frames_but_yes_waits_four)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.continue_menu;
    menu->state = 2;
    menu->state_frames = 29;
    g_Buttons.word = 1;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    CHECK_EQ_INT(menu->state_frames, 30);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 4);
    CHECK_EQ_INT(menu->state_frames, 1);
    menu->state = 1;
    menu->state_frames = 3;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 3);
    CHECK_EQ_INT(menu->state_frames, 1);
    CHECK_EQ_INT(menu->background.pending_interrupt, 1);
}

TEST(continuing_restores_start_resources_and_preserves_totals_and_rank)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.continue_menu;
    menu->state = 3;
    menu->state_frames = 29;
    M_ShowSprites(menu);
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.run.start_lives = 4;
    g_GameManager.run.start_bombs = 3;
    g_GameManager.stats.continues_used = 1;
    g_GameManager.score.value = 123456;
    g_GameManager.score.shown = 100000;
    g_GameManager.score.step = 120;
    g_GameManager.score.next_extend = 3;
    g_GameManager.player.graze = 88;
    g_GameManager.player.point_items = 99;
    g_GameManager.player.power = 128;
    g_GameManager.player.graze_total = 1234;
    g_GameManager.player.point_items_total = 999;
    g_GameManager.rank.value = 18;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 3);
    CHECK_EQ_INT(menu->state_frames, 30);
    CHECK_EQ_INT(g_GameManager.stats.continues_used, 1);
    M_ClearStepCount();
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 0);
    CHECK_EQ_INT(menu->state_frames, 0);
    CHECK_EQ_INT(m_StepCount, 0);
    CHECK(!g_GameManager.mode.in_continue_menu);
    CHECK_EQ_INT(g_GameManager.stats.continues_used, 2);
    CHECK_EQ_INT(g_GameManager.score.value, 2);
    CHECK_EQ_INT(g_GameManager.score.shown, 2);
    CHECK_EQ_INT(g_GameManager.score.step, 0);
    CHECK_EQ_INT(g_GameManager.score.next_extend, 0);
    CHECK_EQ_INT(g_GameManager.player.lives, 4);
    CHECK_EQ_INT(g_GameManager.player.bombs, 3);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    CHECK_EQ_INT(g_GameManager.player.point_items, 0);
    CHECK_EQ_INT(g_GameManager.player.power, 0);
    CHECK_EQ_INT(g_GameManager.player.graze_total, 1234);
    CHECK_EQ_INT(g_GameManager.player.point_items_total, 999);
    CHECK_EQ_INT(g_GameManager.rank.value, 18);
    CHECK_EQ_INT(m_Requested, SCENE_NONE);
    for (int32_t i = 0; i < 4; i++) {
        CHECK(!menu->sprites[i].shown);
    }
}

TEST(giving_up_finishes_after_twenty_frames_without_stepping)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.continue_menu;
    menu->state = 4;
    menu->state_frames = 19;
    M_ShowSprites(menu);
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.score.value = 456;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(m_Requested, SCENE_NONE);
    CHECK_EQ_INT(menu->state_frames, 20);
    M_ClearStepCount();
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(m_Requested, SCENE_GAME_RESULT);
    CHECK_EQ_INT(menu->state_frames, 0);
    CHECK_EQ_INT(m_StepCount, 0);
    CHECK_EQ_INT(g_GameManager.score.shown, 456);
    CHECK(!g_GameManager.mode.in_continue_menu);
}

TEST(menu_draws_preserve_viewport_depth_and_copy_the_continue_digit)
{
    M_Reset();
    PAUSE_MENU *const menu = &g_AsciiText.continue_menu;
    g_GameManager.play_area = (PLAY_RECT) { 32.75f, 16.25f, 383.5f, 447.5f };
    g_GameManager.camera.viewport.min_z = 0.25f;
    g_GameManager.camera.viewport.max_z = 0.75f;
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.stats.continues_used = 2;
    menu->state = 1;
    menu->sprites[1].pos.x = 100.0f;
    menu->sprites[1].scale_x = 1.5f;
    PauseMenu_DrawContinueMenu(menu);
    CHECK(g_GameManager.camera.viewport.x == 32.0f);
    CHECK(g_GameManager.camera.viewport.y == 16.0f);
    CHECK(g_GameManager.camera.viewport.width == 383.0f);
    CHECK(g_GameManager.camera.viewport.height == 447.0f);
    CHECK(g_GameManager.camera.viewport.min_z == 0.25f);
    CHECK(g_GameManager.camera.viewport.max_z == 0.75f);
    CHECK(menu->sprites[4].pos.x == 112.0f);
    CHECK_EQ_INT(m_SetSprite, 0x1c);
    g_GameManager.play_area.x = 40.0f;
    PauseMenu_DrawPauseMenu(&g_AsciiText.pause_menu);
    CHECK(g_GameManager.camera.viewport.x == 32.0f);
    g_GameManager.mode.paused = 2;
    PauseMenu_DrawPauseMenu(&g_AsciiText.pause_menu);
    CHECK(g_GameManager.camera.viewport.x == 40.0f);
}
