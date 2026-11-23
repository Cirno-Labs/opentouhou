#include "th07/ascii/pause_menu.h"

#include "th07/anm/texture.h"
#include "th07/ascii/text.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/play_time.h"
#include "th07/player.h"
#include "th07/scene.h"
#include "th07/sound.h"

#include "core/base/color.h"
#include "core/gfx.h"
#include "core/math/x87.h"

#define M_HIGHLIGHT COLOR_ARGB(0xff, 0xff, 0x80, 0x80)
#define M_DIM COLOR_ARGB(0x80, 0x80, 0x80, 0x80)
#define M_PAUSE_HIGHLIGHT COLOR_WHITE
#define M_PAUSE_DIM COLOR_ARGB(0x80, 0x30, 0x30, 0x30)

static const VEC3 m_Raised = { -4.0f, -4.0f, 0.0f };

static void M_SetViewport(void)
{
    VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    viewport->x = (float)(uint32_t)g_GameManager.play_area.x;
    viewport->y = (float)(uint32_t)g_GameManager.play_area.y;
    viewport->width = (float)(uint32_t)g_GameManager.play_area.width;
    viewport->height = (float)(uint32_t)g_GameManager.play_area.height;
}

static void M_Interrupt(
    ANM_VM *const sprites, const int32_t first, const int32_t end,
    const int16_t interrupt)
{
    for (int32_t i = first; i < end; i++) {
        sprites[i].pending_interrupt = interrupt;
    }
}

static void M_InterruptShown(ANM_VM *const sprites, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (sprites[i].shown) {
            sprites[i].pending_interrupt = 2;
        }
    }
}

static void M_Hide(ANM_VM *const sprites, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        sprites[i].shown = false;
    }
}

// Returns false when a copy is already pending.
static bool M_StartBackground(ANM_VM *const background)
{
    Anm_StartScript(background, 0x724);
    const ANM_SPRITE *const sprite = background->sprite_data;
    GFX_RECT dest = {};
    if (sprite != nullptr) {
        // Retail passes the sprite's height as the width and its width as
        // the height.
        dest = (GFX_RECT) { (int32_t)sprite->x0, (int32_t)sprite->y0,
                            (int32_t)sprite->height, (int32_t)sprite->width };
    }
    if (Gfx_IsStarted()
        && !Anm_SetPendingCopy(4, (GFX_RECT) { 32, 16, 384, 448 }, dest)) {
        return false;
    }
    background->pos = (VEC3) { 32.0f, 16.0f, 0.0f };
    return true;
}

static void M_ClosePause(PAUSE_MENU *const menu)
{
    menu->state = 0;
    g_GameManager.mode.paused = 0;
    M_Hide(menu->sprites, ASCII_PAUSE_SPRITE_COUNT);
}

static void M_HighlightPauseLine(PAUSE_MENU *const menu, const int32_t line)
{
    for (int32_t i = 1; i < 4; i++) {
        menu->sprites[i].color = i == line ? M_PAUSE_HIGHLIGHT : M_PAUSE_DIM;
        menu->sprites[i].pos_offset = i == line ? m_Raised : (VEC3) {};
    }
}

static void M_HighlightConfirm(
    ANM_VM *const sprites, const int32_t yes, const bool on_yes)
{
    sprites[yes].color = on_yes ? M_HIGHLIGHT : M_DIM;
    sprites[yes + 1].color = on_yes ? M_DIM : M_HIGHLIGHT;
    sprites[yes].pos_offset = on_yes ? m_Raised : (VEC3) {};
    sprites[yes + 1].pos_offset = on_yes ? (VEC3) {} : m_Raised;
}

static bool M_Confirmed(void)
{
    return Buttons_WasPressed(TH07_BUTTON_ok);
}

static void M_PlayMove(void)
{
    Sound_Queue(SOUND_PLAYER_SHOT);
}

// States: 1 to 3 choose resume, quit or retry; 5 and 6 confirm quitting
// (yes, no) and 7 and 8 retrying; 4, 9 and 10 close the menu into play,
// the title or a retry 20 frames later.
void PauseMenu_UpdatePauseMenu(PAUSE_MENU *const menu)
{
    const bool in_replay = g_GameManager.mode.in_replay;
    if (Buttons_WasPressed(8) && menu->state != 4) {
        Sound_Queue(SOUND_OK);
        menu->state = 4;
        M_InterruptShown(menu->sprites, ASCII_PAUSE_SPRITE_COUNT);
        menu->state_frames = 0;
        menu->background.pending_interrupt = 1;
    }
    if (Buttons_WasPressed(TH07_BUTTON_quit) && menu->state != 9) {
        Sound_Queue(SOUND_OK);
        menu->state = 9;
        M_InterruptShown(menu->sprites, ASCII_PAUSE_SPRITE_COUNT);
        menu->state_frames = 0;
    }
    // Retail tests for state 9 here, so a new press restarts the wait of
    // state 10.
    if (!in_replay && Buttons_WasPressed(TH07_BUTTON_retry)
        && menu->state != 9) {
        Sound_Queue(SOUND_OK);
        menu->state = 10;
        M_InterruptShown(menu->sprites, ASCII_PAUSE_SPRITE_COUNT);
        menu->state_frames = 0;
    }
    switch (menu->state) {
    case 0:
        for (int32_t i = 0; i < ASCII_PAUSE_SPRITE_COUNT; i++) {
            Anm_StartScript(&menu->sprites[i], i + 0xfe);
        }
        M_Interrupt(menu->sprites, 0, 4, 1);
        Anm_SetSprite(&menu->sprites[7], g_GameManager.run.difficulty + 0x10d);
        if (!g_GameManager.mode.in_practice) {
            menu->sprites[8].shown = false;
        }
        if (!g_GameManager.run.score_disabled) {
            menu->sprites[9].shown = false;
        }
        if (in_replay) {
            menu->sprites[3].pc = -1;
        }
        menu->state++;
        menu->state_frames = 0;
        if (g_AsciiText.capture_background
            && !M_StartBackground(&menu->background)) {
            menu->state = 0;
            return;
        }
        [[fallthrough]];
    case 1:
        M_HighlightPauseLine(menu, 1);
        if (menu->state_frames >= 4) {
            if (!in_replay) {
                if (Buttons_WasPressed(TH07_BUTTON_up)) {
                    menu->state = 3;
                    M_PlayMove();
                }
            } else if (Buttons_WasPressed(TH07_BUTTON_up)) {
                menu->state = 2;
                M_PlayMove();
            }
            if (Buttons_WasPressed(TH07_BUTTON_down)) {
                menu->state = 2;
                M_PlayMove();
            }
            if (M_Confirmed()) {
                Sound_Queue(SOUND_OK);
                M_Interrupt(menu->sprites, 0, 4, 2);
                menu->state = 4;
                menu->state_frames = 0;
                menu->background.pending_interrupt = 1;
            }
        }
        break;
    case 2:
        M_HighlightPauseLine(menu, 2);
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(TH07_BUTTON_up)) {
                menu->state = 1;
                M_PlayMove();
            }
            if (in_replay) {
                if (Buttons_WasPressed(TH07_BUTTON_down)) {
                    menu->state = 1;
                    M_PlayMove();
                }
            } else if (Buttons_WasPressed(TH07_BUTTON_down)) {
                menu->state = 3;
                M_PlayMove();
            }
            if (M_Confirmed()) {
                Sound_Queue(SOUND_OK);
                M_Interrupt(menu->sprites, 0, 4, 2);
                M_Interrupt(menu->sprites, 4, 7, 1);
                menu->state = 6;
                menu->state_frames = 0;
            }
        }
        break;
    case 3:
        M_HighlightPauseLine(menu, 3);
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(TH07_BUTTON_up)) {
                menu->state = 2;
                M_PlayMove();
            }
            if (Buttons_WasPressed(TH07_BUTTON_down)) {
                menu->state = 1;
                M_PlayMove();
            }
            if (M_Confirmed()) {
                Sound_Queue(SOUND_OK);
                M_Interrupt(menu->sprites, 0, 4, 2);
                M_Interrupt(menu->sprites, 4, 7, 1);
                menu->state = 8;
                menu->state_frames = 0;
            }
        }
        break;
    case 4:
        if (menu->state_frames >= 20) {
            M_ClosePause(menu);
            if (g_GameManager.run.stage != 6 || g_Gui.frames >= 300) {
                Sound_ResumeMusic();
            }
            PlayTime_Restart();
        }
        break;
    case 5:
    case 7:
        M_HighlightConfirm(menu->sprites, 5, true);
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(TH07_BUTTON_up)
                || Buttons_WasPressed(TH07_BUTTON_down)) {
                menu->state = menu->state == 5 ? 6 : 8;
                M_PlayMove();
            }
            if (M_Confirmed()) {
                Sound_Queue(SOUND_OK);
                M_Interrupt(menu->sprites, 4, 7, 2);
                menu->state = menu->state == 5 ? 9 : 10;
                menu->state_frames = 0;
            }
        }
        break;
    case 6:
    case 8:
        M_HighlightConfirm(menu->sprites, 5, false);
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(TH07_BUTTON_up)
                || Buttons_WasPressed(TH07_BUTTON_down)) {
                menu->state = menu->state == 6 ? 5 : 7;
                M_PlayMove();
            }
            if (M_Confirmed()) {
                Sound_Queue(SOUND_OK);
                M_Interrupt(menu->sprites, 0, 4, 1);
                M_Interrupt(menu->sprites, 4, 7, 2);
                menu->state = menu->state == 6 ? 2 : 3;
                menu->state_frames = 0;
            }
        }
        break;
    case 9:
        if (menu->state_frames >= 20) {
            M_ClosePause(menu);
            Scene_Request(SCENE_TITLE);
            PlayTime_Restart();
        }
        break;
    case 10:
        if (menu->state_frames >= 20) {
            M_ClosePause(menu);
            Scene_Request(SCENE_RETRY);
            PlayTime_Restart();
        }
        break;
    }
    for (int32_t i = 0; i < ASCII_PAUSE_SPRITE_COUNT; i++) {
        Anm_Step(&menu->sprites[i]);
    }
    if (g_AsciiText.capture_background) {
        Anm_Step(&menu->background);
    }
    menu->state_frames++;
}

void PauseMenu_DrawPauseMenu(PAUSE_MENU *const menu)
{
    if (g_GameManager.mode.paused == 0) {
        return;
    }
    M_SetViewport();
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.interpolate = true;
    draw.part = ASCII_PAUSE_SPRITE_COUNT;
    draw.view =
        Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
    if (g_AsciiText.capture_background && menu->state != 0) {
        ANM_VM background = menu->background;
        background.z_write_off = true;
        Anm_DrawUnrotated(&background, &draw);
    }
    for (int32_t i = 0; i < ASCII_PAUSE_SPRITE_COUNT; i++) {
        if (menu->sprites[i].shown) {
            draw.part = (uint32_t)i;
            Anm_DrawUnrotated(&menu->sprites[i], &draw);
        }
    }
}

static void M_LeaveContinue(const SCENE scene)
{
    g_GameManager.mode.in_continue_menu = false;
    Scene_Request(scene);
    g_GameManager.score.shown = g_GameManager.score.value;
}

static void M_CountUp(int32_t *const count)
{
    if (*count < 999999) {
        *count = *count + 1;
    }
}

static void M_Continue(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->stats.continues_used++;
    gm->score.shown = gm->stats.continues_used;
    gm->score.step = 0;
    gm->score.value = gm->score.shown;
    gm->player.lives = (float)gm->run.start_lives;
    GameManager_DrawChecksumKeys();
    gm->player.bombs = (float)X87_Ftol(g_Player.shot_type.bombs);
    gm->player.graze = 0;
    gm->player.point_items = 0;
    gm->player.point_items_total = 0;
    gm->player.power = 0.0f;
    GameManager_DrawChecksumKeys();
    gm->player.extends = 0;
    gm->player.next_extend_items = 50;
    gm->cherry.value = gm->cherry.offset;
    const int32_t shot = gm->run.shot_index + 1;
    M_CountUp(&gm->records.play_stats.counts[gm->run.difficulty].plays[0]);
    M_CountUp(&gm->records.play_stats.counts[6].plays[0]);
    M_CountUp(&gm->records.play_stats.counts[gm->run.difficulty].plays[shot]);
    M_CountUp(&gm->records.play_stats.counts[6].plays[shot]);
    M_CountUp(&gm->records.play_stats.counts[gm->run.difficulty].continues);
    M_CountUp(&gm->records.play_stats.counts[6].continues);
    Sound_ResumeMusic();
    PlayTime_Restart();
}

// States: 1 continues and 2 gives up, after 30 frames; 3 and 4 close the
// menu into play or the result screen.
void PauseMenu_UpdateContinueMenu(CONTINUE_MENU *const menu)
{
    if (g_GameManager.mode.in_practice) {
        M_LeaveContinue(SCENE_GAME_RESULT);
        return;
    }
    if (g_GameManager.mode.in_replay) {
        M_LeaveContinue(SCENE_REPLAY_END);
        return;
    }
    if (g_GameManager.stats.continues_used >= g_GameManager.run.max_continues
        || g_GameManager.run.difficulty >= 4) {
        M_LeaveContinue(SCENE_GAME_RESULT);
        return;
    }
    switch (menu->state) {
    case 0:
        if (menu->state_frames == 0) {
            Sound_PauseMusic();
            for (int32_t i = 0; i < 4; i++) {
                Anm_StartScript(&menu->sprites[i], i + 0x108);
                menu->sprites[i].pending_interrupt = 1;
            }
            Anm_StartScript(&menu->sprites[4], 0x10c);
            Anm_SetSprite(
                &menu->sprites[4],
                g_GameManager.run.max_continues + 0x106
                    - g_GameManager.stats.continues_used);
            menu->sprites[4].pending_interrupt = 1;
            if (g_AsciiText.capture_background
                && !M_StartBackground(&menu->background)) {
                menu->state = 0;
                return;
            }
            PlayTime_Add();
        }
        if (menu->state_frames > 8) {
            break;
        }
        // Retail moves to state 2 but runs state 1's frame first.
        menu->state += 2;
        menu->state_frames = 0;
        [[fallthrough]];
    case 1:
        M_HighlightConfirm(menu->sprites, 2, true);
        if (menu->state_frames >= 4) {
            if (Buttons_WasPressed(TH07_BUTTON_up)
                || Buttons_WasPressed(TH07_BUTTON_down)) {
                menu->state = 2;
                M_PlayMove();
            }
            if (M_Confirmed()) {
                Sound_Queue(SOUND_OK);
                M_Interrupt(menu->sprites, 0, 5, 2);
                menu->state = 3;
                menu->background.pending_interrupt = 1;
                menu->state_frames = 0;
            }
        }
        break;
    case 2:
        M_HighlightConfirm(menu->sprites, 2, false);
        if (menu->state_frames >= 30) {
            if (Buttons_WasPressed(TH07_BUTTON_up)
                || Buttons_WasPressed(TH07_BUTTON_down)) {
                menu->state = 1;
                M_PlayMove();
            }
            if (M_Confirmed()) {
                Sound_Queue(SOUND_OK);
                M_Interrupt(menu->sprites, 0, 5, 2);
                menu->state = 4;
                menu->state_frames = 0;
            }
        }
        break;
    case 4:
        if (menu->state_frames >= 20) {
            menu->state = 0;
            menu->state_frames = 0;
            g_GameManager.mode.in_continue_menu = false;
            Scene_Request(SCENE_GAME_RESULT);
            M_Hide(menu->sprites, 5);
            g_GameManager.score.shown = g_GameManager.score.value;
            PlayTime_Restart();
            return;
        }
        break;
    case 3:
        if (menu->state_frames >= 30) {
            menu->state = 0;
            menu->state_frames = 0;
            g_GameManager.mode.in_continue_menu = false;
            M_Hide(menu->sprites, 5);
            M_Continue();
            return;
        }
        break;
    }
    for (int32_t i = 0; i < 5; i++) {
        Anm_Step(&menu->sprites[i]);
    }
    if (g_AsciiText.capture_background) {
        Anm_Step(&menu->background);
    }
    menu->state_frames++;
}

void PauseMenu_DrawContinueMenu(CONTINUE_MENU *const menu)
{
    if (!g_GameManager.mode.in_continue_menu) {
        return;
    }
    M_SetViewport();
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.interpolate = true;
    draw.part = ASCII_CONTINUE_SPRITE_COUNT;
    draw.view =
        Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
    if (g_AsciiText.capture_background
        && (menu->state != 0 || menu->state_frames > 2)) {
        Anm_DrawUnrotated(&menu->background, &draw);
    }
    // Retail draws the counter here and again in the loop below.
    if (menu->state == 1 || menu->state == 2) {
        draw.part = ASCII_CONTINUE_SPRITE_COUNT + 1;
        Anm_DrawUnrotated(&menu->sprites[4], &draw);
    }
    for (int32_t i = 0; i < 5; i++) {
        if (menu->sprites[i].shown) {
            draw.part = (uint32_t)i;
            Anm_DrawUnrotated(&menu->sprites[i], &draw);
        }
    }
}
