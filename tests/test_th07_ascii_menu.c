#include "th07/ascii/pause_menu.h"

#include "th07/anm/private.h"
#include "th07/ascii/text.h"
#include "th07/game.h"
#include "th07/gui.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/game_manager.h"
#include "th07/player.h"

#include "gfx_recorder.h"
#include "test.h"
#include "th07_scene_stubs.h"

#include "core/base/color.h"
#include "core/base/memory.h"

#include "stg/rng.h"

#include <string.h>

#define I(x) { .i = (x) }
#define INSTR(op_, mask_, ...)                                                 \
    {                                                                          \
        .opcode = (op_), .param_mask = (mask_), .args = { __VA_ARGS__ }        \
    }
#define M_UP 0x10
#define M_DOWN 0x20
#define M_SHOOT 0x1
#define M_MENU 0x8
#define M_QUIT 0x200
#define M_RETRY 0x4000

static RNG m_Rng;
// Keeps the number of the last interrupt in int_vars[0].
static ANM_INSTR m_Instrs[] = {
    INSTR(3, 0, I(0)),
    INSTR(20, 0),
    INSTR(21, 0, I(1)),
    INSTR(37, 1, I(10000), I(1)),
    INSTR(20, 0),
    INSTR(21, 0, I(2)),
    INSTR(37, 1, I(10000), I(2)),
    INSTR(20, 0),
    INSTR(-1, 0),
};
static ANM_SCRIPT m_Script = { m_Instrs, 9, 0 };

static void M_Setup(void)
{
    SceneStubCalls_Reset();
    Sound_Reset();
    g_GameSpeed = 1.0f;
    Anm_Init(&g_HeapAllocator, &m_Rng);
    const ANM_SPRITE sprite = {
        .x1 = 16.0f,
        .y1 = 8.0f,
        .texture_width = 256.0f,
        .texture_height = 256.0f,
        .width = 16.0f,
        .height = 8.0f,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
    };
    for (int32_t i = 0; i < 0x120; i++) {
        Anm_SetSpriteEntry(i, &sprite);
    }
    for (int32_t i = 0xfe; i <= 0x10c; i++) {
        Anm_SetScriptEntry(i, &m_Script);
    }
    Anm_SetScriptEntry(0x724, &m_Script);
    memset(&g_AsciiText, 0, sizeof(g_AsciiText));
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    Scene_Request(SCENE_GAMEPLAY);
    g_GameManager.mode.paused = 2;
    g_GameManager.run.max_continues = 3;
    g_GameManager.run.stage = 1;
}

static void M_Teardown(void)
{
    Anm_Shutdown();
    Result_Ignore(Anm_DoPendingCopy());
}

static void M_Press(const uint16_t mask)
{
    g_Buttons.previous_word = 0;
    g_Buttons.word = mask;
}

static PAUSE_MENU *M_Pause(const int32_t state, const int32_t frames)
{
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    menu->state = state;
    menu->state_frames = frames;
    return menu;
}

static void M_PauseFrames(PAUSE_MENU *const menu, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        PauseMenu_UpdatePauseMenu(menu);
    }
}

TEST(pause_opens_ten_lines_and_runs_resume_at_once)
{
    M_Setup();
    g_GameManager.run.difficulty = 2;
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    CHECK_EQ_INT(menu->state_frames, 1);
    for (int32_t i = 0; i < ASCII_PAUSE_SPRITE_COUNT; i++) {
        CHECK_EQ_INT(menu->sprites[i].script_index, 0xfe + i);
        CHECK_EQ_INT(menu->sprites[i].int_vars[0], i < 4 ? 1 : 0);
    }
    CHECK_EQ_INT(menu->sprites[7].sprite, 0x10f);
    CHECK(!menu->sprites[8].shown);
    CHECK(!menu->sprites[9].shown);
    CHECK_EQ_INT(menu->sprites[1].color, COLOR_WHITE);
    CHECK_EQ_INT(menu->sprites[2].color, COLOR_ARGB(0x80, 0x30, 0x30, 0x30));
    CHECK_EQ_INT(menu->sprites[3].color, COLOR_ARGB(0x80, 0x30, 0x30, 0x30));
    CHECK(menu->sprites[1].pos_offset.x == -4.0f);
    CHECK(menu->sprites[3].pos_offset.x == 0.0f);
    CHECK(Anm_SetPendingCopy(4, (GFX_RECT) {}, (GFX_RECT) {}));
    M_Teardown();
}

TEST(pause_keeps_the_practice_and_score_lines_when_they_apply)
{
    M_Setup();
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.score_disabled = true;
    PauseMenu_UpdatePauseMenu(&g_AsciiText.pause_menu);
    CHECK(g_AsciiText.pause_menu.sprites[8].shown);
    CHECK(g_AsciiText.pause_menu.sprites[9].shown);
    M_Teardown();
}

TEST(pause_navigation_waits_four_frames_and_wraps_through_retry)
{
    M_Setup();
    PAUSE_MENU *const menu = M_Pause(1, 3);
    M_Press(M_UP);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 3);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_PLAYER_SHOT);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    M_Press(M_DOWN);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 3);
    CHECK_EQ_INT(menu->sprites[2].color, COLOR_WHITE);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    CHECK_EQ_INT(menu->sprites[3].color, COLOR_WHITE);
    CHECK(menu->sprites[3].pos_offset.y == -4.0f);
    M_Press(M_UP | M_DOWN);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    M_Teardown();
}

TEST(pause_in_a_replay_skips_and_freezes_retry)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->sprites[3].pc, -1);
    CHECK_EQ_INT(menu->sprites[3].pending_interrupt, 1);
    menu->state_frames = 4;
    M_Press(M_UP);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    M_Press(M_DOWN);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    M_Press(M_RETRY);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 1);
    M_Teardown();
}

TEST(pause_resume_closes_after_twenty_frames)
{
    M_Setup();
    PAUSE_MENU *const menu = M_Pause(1, 4);
    M_Press(M_SHOOT);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 4);
    CHECK_EQ_INT(menu->state_frames, 1);
    CHECK_EQ_INT(menu->background.pending_interrupt, 1);
    for (int32_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(menu->sprites[i].int_vars[0], 2);
    }
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_OK);
    M_Press(0);
    M_PauseFrames(menu, 19);
    CHECK_EQ_INT(g_GameManager.mode.paused, 2);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 0);
    CHECK_EQ_INT(g_GameManager.mode.paused, 0);
    for (int32_t i = 0; i < ASCII_PAUSE_SPRITE_COUNT; i++) {
        CHECK(!menu->sprites[i].shown);
    }
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_RESUME_MUSIC);
    CHECK_EQ_INT(g_SceneStubCalls.play_time_restart, 1);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_GAMEPLAY);
    M_Teardown();
}

TEST(pause_resume_keeps_stage_6_music_off_for_300_frames)
{
    M_Setup();
    g_GameManager.run.stage = 6;
    g_Gui.frames = 299;
    PAUSE_MENU *const menu = M_Pause(4, 20);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK(g_SoundEvents.pause != SOUND_RESUME_MUSIC);
    g_Gui.frames = 300;
    menu->state = 4;
    menu->state_frames = 20;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_RESUME_MUSIC);
    M_Teardown();
}

TEST(pause_quit_confirms_and_requests_the_title)
{
    M_Setup();
    PAUSE_MENU *const menu = M_Pause(2, 4);
    M_Press(M_SHOOT);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 6);
    for (int32_t i = 4; i < 7; i++) {
        CHECK_EQ_INT(menu->sprites[i].int_vars[0], 1);
    }
    menu->state_frames = 4;
    M_Press(M_UP);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 5);
    CHECK_EQ_INT(menu->sprites[5].color, COLOR_ARGB(0x80, 0x80, 0x80, 0x80));
    CHECK_EQ_INT(menu->sprites[6].color, COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
    M_Press(M_SHOOT);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 9);
    CHECK_EQ_INT(menu->sprites[5].color, COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
    CHECK(menu->sprites[5].pos_offset.x == -4.0f);
    M_Press(0);
    M_PauseFrames(menu, 20);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_TITLE);
    CHECK_EQ_INT(g_GameManager.mode.paused, 0);
    CHECK(g_SoundEvents.pause != SOUND_RESUME_MUSIC);
    M_Teardown();
}

TEST(pause_retry_confirms_and_requests_a_retry)
{
    M_Setup();
    PAUSE_MENU *const menu = M_Pause(3, 4);
    M_Press(M_SHOOT);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 8);
    menu->state_frames = 4;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 3);
    menu->state = 8;
    menu->state_frames = 4;
    M_Press(M_DOWN);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 7);
    M_Press(M_SHOOT);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 10);
    M_Press(0);
    M_PauseFrames(menu, 20);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_RETRY);
    M_Teardown();
}

TEST(pause_menu_quit_and_retry_buttons_act_at_once)
{
    M_Setup();
    PAUSE_MENU *const menu = M_Pause(2, 0);
    M_Press(M_MENU);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 4);
    CHECK_EQ_INT(menu->sprites[0].int_vars[0], 2);
    CHECK_EQ_INT(menu->sprites[8].int_vars[0], 0);
    M_Press(M_QUIT);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 9);
    M_Press(M_RETRY);
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 9);
    menu->state = 10;
    menu->state_frames = 10;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 10);
    CHECK_EQ_INT(menu->state_frames, 1);
    M_Teardown();
}

static uint32_t M_TexturePixel(const int32_t x, const int32_t y)
{
    const IMAGE *const image = AnmTexture_GetImage(4);
    const uint8_t *const pixel = &image->pixels[(y * image->width + x) * 4];
    return (uint32_t)pixel[0] << 24 | (uint32_t)pixel[1] << 16
        | (uint32_t)pixel[2] << 8 | pixel[3];
}

TEST(pause_capture_copies_the_play_area_scaled_to_the_sprite)
{
    M_Setup();
    GfxRecorder_Start();
    IMAGE screen = Image_Create(640, 480, &g_HeapAllocator);
    for (int32_t y = 0; y < 480; y++) {
        for (int32_t x = 0; x < 640; x++) {
            const bool play_area = x >= 32 && x < 416 && y >= 16 && y < 464;
            const uint32_t rgba = play_area ? 0xff0000ffu : 0x0000ffffu;
            for (int32_t c = 0; c < 4; c++) {
                screen.pixels[(y * 640 + x) * 4 + c] =
                    (uint8_t)(rgba >> (24 - c * 8));
            }
        }
    }
    GfxRecorder_SetScreen(&screen);
    Image_Free(&screen);
    CHECK(IS_OK(Anm_CreateTexture(4, 512, 512, ANM_TEXTURE_RGBA8)));
    g_AsciiText.capture_background = true;
    ANM_SPRITE sprite = *Anm_GetSpriteEntry(0);
    sprite.x0 = 3.0f;
    sprite.y0 = 5.0f;
    sprite.width = 400.0f;
    sprite.height = 500.0f;
    Anm_SetSpriteEntry(0, &sprite);
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK(IS_OK(Anm_DoPendingCopy()));
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 1);
    CHECK_EQ_INT(M_TexturePixel(3, 5), 0xff0000ff);
    CHECK_EQ_INT(M_TexturePixel(502, 5), 0xff0000ff);
    CHECK_EQ_INT(M_TexturePixel(3, 404), 0xff0000ff);
    CHECK_EQ_INT(M_TexturePixel(502, 404), 0xff0000ff);
    CHECK_EQ_INT(M_TexturePixel(2, 5), 0);
    CHECK_EQ_INT(M_TexturePixel(503, 5), 0);
    CHECK_EQ_INT(M_TexturePixel(3, 4), 0);
    CHECK_EQ_INT(M_TexturePixel(3, 405), 0);
    CHECK_EQ_INT(menu->background.script_index, 0x724);
    CHECK(menu->background.pos.x == 32.0f && menu->background.pos.y == 16.0f);
    M_Teardown();
    GfxRecorder_Stop();
}

TEST(pause_retries_opening_while_a_copy_is_pending)
{
    M_Setup();
    GfxRecorder_Start();
    g_AsciiText.capture_background = true;
    CHECK(Anm_SetPendingCopy(4, (GFX_RECT) {}, (GFX_RECT) {}));
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK_EQ_INT(menu->state, 0);
    CHECK_EQ_INT(menu->state_frames, 0);
    CHECK_EQ_INT(menu->sprites[0].int_vars[0], 0);
    M_Teardown();
    GfxRecorder_Stop();
}

TEST(pause_without_a_renderer_opens_with_no_copy)
{
    M_Setup();
    g_AsciiText.capture_background = true;
    PAUSE_MENU *const menu = &g_AsciiText.pause_menu;
    PauseMenu_UpdatePauseMenu(menu);
    CHECK(Anm_SetPendingCopy(4, (GFX_RECT) {}, (GFX_RECT) {}));
    CHECK_EQ_INT(menu->background.script_index, 0x724);
    M_Teardown();
}

TEST(continue_leaves_at_once_when_it_cannot_continue)
{
    M_Setup();
    CONTINUE_MENU *const menu = &g_AsciiText.continue_menu;
    g_GameManager.score.value = 1234;
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.mode.in_practice = true;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK(!g_GameManager.mode.in_continue_menu);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_GAME_RESULT);
    CHECK_EQ_INT(g_GameManager.score.shown, 1234);
    g_GameManager.mode.in_practice = false;
    g_GameManager.mode.in_replay = true;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_REPLAY_END);
    g_GameManager.mode.in_replay = false;
    g_GameManager.stats.continues_used = 3;
    Scene_Request(SCENE_GAMEPLAY);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_GAME_RESULT);
    g_GameManager.stats.continues_used = 0;
    g_GameManager.run.difficulty = 4;
    Scene_Request(SCENE_GAMEPLAY);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_GAME_RESULT);
    CHECK_EQ_INT(menu->state_frames, 0);
    M_Teardown();
}

TEST(continue_opens_on_give_up_after_a_frame_of_continue)
{
    M_Setup();
    g_GameManager.stats.continues_used = 1;
    CONTINUE_MENU *const menu = &g_AsciiText.continue_menu;
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    CHECK_EQ_INT(menu->state_frames, 1);
    CHECK_EQ_INT(menu->sprites[0].script_index, 0x108);
    CHECK_EQ_INT(menu->sprites[4].script_index, 0x10c);
    CHECK_EQ_INT(menu->sprites[4].sprite, 0x108);
    for (int32_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(menu->sprites[i].int_vars[0], 1);
    }
    CHECK_EQ_INT(menu->sprites[2].color, COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
    CHECK_EQ_INT(menu->sprites[3].color, COLOR_ARGB(0x80, 0x80, 0x80, 0x80));
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_MUSIC);
    CHECK_EQ_INT(g_SceneStubCalls.play_time_add, 1);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->sprites[3].color, COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_MUSIC);
    M_Teardown();
}

TEST(continue_give_up_waits_thirty_frames_then_shows_the_result)
{
    M_Setup();
    CONTINUE_MENU *const menu = &g_AsciiText.continue_menu;
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.score.value = 99;
    menu->state = 2;
    menu->state_frames = 29;
    M_Press(M_SHOOT);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 2);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 4);
    M_Press(0);
    for (int32_t i = 0; i < 20; i++) {
        PauseMenu_UpdateContinueMenu(menu);
    }
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_GAME_RESULT);
    CHECK(!g_GameManager.mode.in_continue_menu);
    CHECK_EQ_INT(g_GameManager.score.shown, 99);
    CHECK_EQ_INT(menu->state, 0);
    CHECK_EQ_INT(menu->state_frames, 0);
    CHECK_EQ_INT(g_SceneStubCalls.play_time_restart, 1);
    M_Teardown();
}

TEST(continue_resets_the_run_and_counts_the_continue)
{
    M_Setup();
    CONTINUE_MENU *const menu = &g_AsciiText.continue_menu;
    GAME_MANAGER *const gm = &g_GameManager;
    gm->mode.in_continue_menu = true;
    gm->stats.continues_used = 1;
    gm->score.value = 5000;
    gm->score.step = 7;
    gm->run.start_lives = 4;
    gm->run.difficulty = 2;
    gm->run.shot_index = 3;
    gm->player.graze = 10;
    gm->player.point_items = 11;
    gm->player.point_items_total = 12;
    gm->player.power = 64.0f;
    gm->player.extends = 2;
    gm->cherry.offset = 321;
    gm->cherry.value = 40000;
    gm->records.play_stats.counts[6].continues = 999999;
    g_Player.shot_type.bombs = 3.0f;
    const int32_t generated = g_Rng.generated;
    menu->state = 1;
    menu->state_frames = 4;
    M_Press(M_SHOOT);
    PauseMenu_UpdateContinueMenu(menu);
    CHECK_EQ_INT(menu->state, 3);
    CHECK_EQ_INT(menu->background.pending_interrupt, 1);
    M_Press(0);
    for (int32_t i = 0; i < 30; i++) {
        PauseMenu_UpdateContinueMenu(menu);
    }
    CHECK(!gm->mode.in_continue_menu);
    CHECK_EQ_INT(gm->stats.continues_used, 2);
    CHECK_EQ_INT(gm->score.value, 2);
    CHECK_EQ_INT(gm->score.shown, 2);
    CHECK_EQ_INT(gm->score.step, 0);
    CHECK(gm->player.lives == 4.0f);
    CHECK(gm->player.bombs == 3.0f);
    CHECK_EQ_INT(g_Rng.generated - generated, 8);
    CHECK_EQ_INT(gm->player.graze, 0);
    CHECK_EQ_INT(gm->player.point_items, 0);
    CHECK_EQ_INT(gm->player.point_items_total, 0);
    CHECK(gm->player.power == 0.0f);
    CHECK_EQ_INT(gm->player.extends, 0);
    CHECK_EQ_INT(gm->player.next_extend_items, 50);
    CHECK_EQ_INT(gm->cherry.value, 321);
    CHECK_EQ_INT(gm->records.play_stats.counts[2].plays[0], 1);
    CHECK_EQ_INT(gm->records.play_stats.counts[2].plays[4], 1);
    CHECK_EQ_INT(gm->records.play_stats.counts[6].plays[4], 1);
    CHECK_EQ_INT(gm->records.play_stats.counts[2].continues, 1);
    CHECK_EQ_INT(gm->records.play_stats.counts[6].continues, 999999);
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_RESUME_MUSIC);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_GAMEPLAY);
    M_Teardown();
}
