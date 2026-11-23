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

// The pause menu shows a title and the lines resume, quit and retry, then
// a question with yes and no, then the difficulty and the practice and
// unscored marks. The continue menu shows two lines, yes and no, and the
// number of continues left.
#define M_PAUSE_RESUME_LINE 1
#define M_PAUSE_QUIT_LINE 2
#define M_PAUSE_RETRY_LINE 3
#define M_PAUSE_LINE_END 4
#define M_PAUSE_YES_SPRITE 5
#define M_PAUSE_CONFIRM_END 7
#define M_PAUSE_DIFFICULTY_SPRITE 7
#define M_PAUSE_PRACTICE_SPRITE 8
#define M_PAUSE_UNSCORED_SPRITE 9
#define M_CONTINUE_YES_SPRITE 2
#define M_CONTINUE_COUNTER_SPRITE 4
#define M_CONTINUE_SPRITE_COUNT 5

typedef enum {
    M_PAUSE_OPEN = 0,
    M_PAUSE_ON_RESUME = 1,
    M_PAUSE_ON_QUIT = 2,
    M_PAUSE_ON_RETRY = 3,
    M_PAUSE_RESUMING = 4,
    M_PAUSE_QUIT_YES = 5,
    M_PAUSE_QUIT_NO = 6,
    M_PAUSE_RETRY_YES = 7,
    M_PAUSE_RETRY_NO = 8,
    M_PAUSE_QUITTING = 9,
    M_PAUSE_RETRYING = 10,
} M_PAUSE_STATE;

typedef enum {
    M_CONTINUE_OPEN = 0,
    M_CONTINUE_ON_YES = 1,
    M_CONTINUE_ON_NO = 2,
    M_CONTINUE_CONTINUING = 3,
    M_CONTINUE_GIVING_UP = 4,
} M_CONTINUE_STATE;

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

static void M_StartClose(PAUSE_MENU *const menu, const M_PAUSE_STATE state)
{
    Sound_Queue(SOUND_OK);
    menu->state = state;
    M_InterruptShown(menu->sprites, ASCII_PAUSE_SPRITE_COUNT);
    menu->state_frames = 0;
}

// Returns false when the background copy has to wait for the next calc.
static bool M_OpenPause(PAUSE_MENU *const menu)
{
    for (int32_t i = 0; i < ASCII_PAUSE_SPRITE_COUNT; i++) {
        Anm_StartScript(&menu->sprites[i], i + 0xfe);
    }
    M_Interrupt(menu->sprites, 0, M_PAUSE_LINE_END, 1);
    Anm_SetSprite(
        &menu->sprites[M_PAUSE_DIFFICULTY_SPRITE],
        g_GameManager.run.difficulty + 0x10d);
    if (!g_GameManager.mode.in_practice) {
        menu->sprites[M_PAUSE_PRACTICE_SPRITE].shown = false;
    }
    if (!g_GameManager.run.score_disabled) {
        menu->sprites[M_PAUSE_UNSCORED_SPRITE].shown = false;
    }
    if (g_GameManager.mode.in_replay) {
        menu->sprites[M_PAUSE_RETRY_LINE].pc = -1;
    }
    menu->state++;
    menu->state_frames = 0;
    if (g_AsciiText.capture_background
        && !M_StartBackground(&menu->background)) {
        menu->state = M_PAUSE_OPEN;
        return false;
    }
    return true;
}

// Moves the cursor to up_state on up and to down_state on down.
static void M_MoveLine(
    PAUSE_MENU *const menu, const M_PAUSE_STATE up_state,
    const M_PAUSE_STATE down_state)
{
    if (Buttons_WasPressed(TH07_BUTTON_up)) {
        menu->state = up_state;
        M_PlayMove();
    }
    if (Buttons_WasPressed(TH07_BUTTON_down)) {
        menu->state = down_state;
        M_PlayMove();
    }
}

static void M_OnResume(PAUSE_MENU *const menu)
{
    M_HighlightPauseLine(menu, M_PAUSE_RESUME_LINE);
    if (menu->state_frames < 4) {
        return;
    }
    const bool in_replay = g_GameManager.mode.in_replay;
    M_MoveLine(
        menu, in_replay ? M_PAUSE_ON_QUIT : M_PAUSE_ON_RETRY, M_PAUSE_ON_QUIT);
    if (M_Confirmed()) {
        Sound_Queue(SOUND_OK);
        M_Interrupt(menu->sprites, 0, M_PAUSE_LINE_END, 2);
        menu->state = M_PAUSE_RESUMING;
        menu->state_frames = 0;
        menu->background.pending_interrupt = 1;
    }
}

// Opens the question of confirm_state from the line chosen.
static void M_Ask(PAUSE_MENU *const menu, const M_PAUSE_STATE confirm_state)
{
    Sound_Queue(SOUND_OK);
    M_Interrupt(menu->sprites, 0, M_PAUSE_LINE_END, 2);
    M_Interrupt(menu->sprites, M_PAUSE_LINE_END, M_PAUSE_CONFIRM_END, 1);
    menu->state = confirm_state;
    menu->state_frames = 0;
}

static void M_OnQuit(PAUSE_MENU *const menu)
{
    M_HighlightPauseLine(menu, M_PAUSE_QUIT_LINE);
    if (menu->state_frames < 4) {
        return;
    }
    const bool in_replay = g_GameManager.mode.in_replay;
    M_MoveLine(
        menu, M_PAUSE_ON_RESUME,
        in_replay ? M_PAUSE_ON_RESUME : M_PAUSE_ON_RETRY);
    if (M_Confirmed()) {
        M_Ask(menu, M_PAUSE_QUIT_NO);
    }
}

static void M_OnRetry(PAUSE_MENU *const menu)
{
    M_HighlightPauseLine(menu, M_PAUSE_RETRY_LINE);
    if (menu->state_frames < 4) {
        return;
    }
    M_MoveLine(menu, M_PAUSE_ON_QUIT, M_PAUSE_ON_RESUME);
    if (M_Confirmed()) {
        M_Ask(menu, M_PAUSE_RETRY_NO);
    }
}

// Runs yes of the question to quit, or to retry when retry is set.
static void M_OnYes(PAUSE_MENU *const menu, const bool retry)
{
    M_HighlightConfirm(menu->sprites, M_PAUSE_YES_SPRITE, true);
    if (menu->state_frames < 4) {
        return;
    }
    if (Buttons_WasPressed(TH07_BUTTON_up)
        || Buttons_WasPressed(TH07_BUTTON_down)) {
        menu->state = retry ? M_PAUSE_RETRY_NO : M_PAUSE_QUIT_NO;
        M_PlayMove();
    }
    if (M_Confirmed()) {
        Sound_Queue(SOUND_OK);
        M_Interrupt(menu->sprites, M_PAUSE_LINE_END, M_PAUSE_CONFIRM_END, 2);
        menu->state = retry ? M_PAUSE_RETRYING : M_PAUSE_QUITTING;
        menu->state_frames = 0;
    }
}

// Runs no of the question to quit, or to retry when retry is set.
static void M_OnNo(PAUSE_MENU *const menu, const bool retry)
{
    M_HighlightConfirm(menu->sprites, M_PAUSE_YES_SPRITE, false);
    if (menu->state_frames < 4) {
        return;
    }
    if (Buttons_WasPressed(TH07_BUTTON_up)
        || Buttons_WasPressed(TH07_BUTTON_down)) {
        menu->state = retry ? M_PAUSE_RETRY_YES : M_PAUSE_QUIT_YES;
        M_PlayMove();
    }
    if (M_Confirmed()) {
        Sound_Queue(SOUND_OK);
        M_Interrupt(menu->sprites, 0, M_PAUSE_LINE_END, 1);
        M_Interrupt(menu->sprites, M_PAUSE_LINE_END, M_PAUSE_CONFIRM_END, 2);
        menu->state = retry ? M_PAUSE_ON_RETRY : M_PAUSE_ON_QUIT;
        menu->state_frames = 0;
    }
}

static void M_Resume(PAUSE_MENU *const menu)
{
    if (menu->state_frames < 20) {
        return;
    }
    M_ClosePause(menu);
    if (g_GameManager.run.stage != 6 || g_Gui.frames >= 300) {
        Sound_ResumeMusic();
    }
    PlayTime_Restart();
}

static void M_Leave(PAUSE_MENU *const menu, const SCENE scene)
{
    if (menu->state_frames < 20) {
        return;
    }
    M_ClosePause(menu);
    Scene_Request(scene);
    PlayTime_Restart();
}

void PauseMenu_Update(PAUSE_MENU *const menu)
{
    if (Buttons_WasPressed(TH07_BUTTON_menu)
        && menu->state != M_PAUSE_RESUMING) {
        M_StartClose(menu, M_PAUSE_RESUMING);
        menu->background.pending_interrupt = 1;
    }
    if (Buttons_WasPressed(TH07_BUTTON_quit)
        && menu->state != M_PAUSE_QUITTING) {
        M_StartClose(menu, M_PAUSE_QUITTING);
    }
    // Retail tests for quitting here, so a new press restarts the wait of
    // retrying.
    if (!g_GameManager.mode.in_replay && Buttons_WasPressed(TH07_BUTTON_retry)
        && menu->state != M_PAUSE_QUITTING) {
        M_StartClose(menu, M_PAUSE_RETRYING);
    }
    switch (menu->state) {
    case M_PAUSE_OPEN:
        if (!M_OpenPause(menu)) {
            return;
        }
        M_OnResume(menu);
        break;
    case M_PAUSE_ON_RESUME:
        M_OnResume(menu);
        break;
    case M_PAUSE_ON_QUIT:
        M_OnQuit(menu);
        break;
    case M_PAUSE_ON_RETRY:
        M_OnRetry(menu);
        break;
    case M_PAUSE_RESUMING:
        M_Resume(menu);
        break;
    case M_PAUSE_QUIT_YES:
    case M_PAUSE_RETRY_YES:
        M_OnYes(menu, menu->state == M_PAUSE_RETRY_YES);
        break;
    case M_PAUSE_QUIT_NO:
    case M_PAUSE_RETRY_NO:
        M_OnNo(menu, menu->state == M_PAUSE_RETRY_NO);
        break;
    case M_PAUSE_QUITTING:
        M_Leave(menu, SCENE_TITLE);
        break;
    case M_PAUSE_RETRYING:
        M_Leave(menu, SCENE_RETRY);
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
    if (g_AsciiText.capture_background && menu->state != M_PAUSE_OPEN) {
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

typedef enum {
    M_OPEN_WAIT,
    M_OPEN_SHOWN,
    M_OPEN_CHOOSE,
} M_OPEN;

// Starts the sprites on the first calc. Returns M_OPEN_WAIT when the
// background copy has to wait for the next calc, M_OPEN_SHOWN once 8 calcs
// have passed, and otherwise moves the cursor to no, which retail runs as
// yes on that calc.
static M_OPEN M_OpenContinue(CONTINUE_MENU *const menu)
{
    if (menu->state_frames == 0) {
        Sound_PauseMusic();
        for (int32_t i = 0; i < M_CONTINUE_COUNTER_SPRITE; i++) {
            Anm_StartScript(&menu->sprites[i], i + 0x108);
            menu->sprites[i].pending_interrupt = 1;
        }
        ANM_VM *const counter = &menu->sprites[M_CONTINUE_COUNTER_SPRITE];
        Anm_StartScript(counter, 0x10c);
        Anm_SetSprite(
            counter,
            g_GameManager.run.max_continues + 0x106
                - g_GameManager.stats.continues_used);
        counter->pending_interrupt = 1;
        if (g_AsciiText.capture_background
            && !M_StartBackground(&menu->background)) {
            menu->state = M_CONTINUE_OPEN;
            return M_OPEN_WAIT;
        }
        PlayTime_Add();
    }
    if (menu->state_frames > 8) {
        return M_OPEN_SHOWN;
    }
    menu->state += 2;
    menu->state_frames = 0;
    return M_OPEN_CHOOSE;
}

// Runs yes, which continues, or no when on_yes is false.
static void M_OnContinueChoice(CONTINUE_MENU *const menu, const bool on_yes)
{
    M_HighlightConfirm(menu->sprites, M_CONTINUE_YES_SPRITE, on_yes);
    if (menu->state_frames < (on_yes ? 4 : 30)) {
        return;
    }
    if (Buttons_WasPressed(TH07_BUTTON_up)
        || Buttons_WasPressed(TH07_BUTTON_down)) {
        menu->state = on_yes ? M_CONTINUE_ON_NO : M_CONTINUE_ON_YES;
        M_PlayMove();
    }
    if (M_Confirmed()) {
        Sound_Queue(SOUND_OK);
        M_Interrupt(menu->sprites, 0, M_CONTINUE_SPRITE_COUNT, 2);
        menu->state = on_yes ? M_CONTINUE_CONTINUING : M_CONTINUE_GIVING_UP;
        if (on_yes) {
            menu->background.pending_interrupt = 1;
        }
        menu->state_frames = 0;
    }
}

static void M_CloseContinue(CONTINUE_MENU *const menu)
{
    menu->state = M_CONTINUE_OPEN;
    menu->state_frames = 0;
    g_GameManager.mode.in_continue_menu = false;
}

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
    case M_CONTINUE_OPEN: {
        const M_OPEN open = M_OpenContinue(menu);
        if (open == M_OPEN_WAIT) {
            return;
        }
        if (open == M_OPEN_CHOOSE) {
            M_OnContinueChoice(menu, true);
        }
        break;
    }
    case M_CONTINUE_ON_YES:
        M_OnContinueChoice(menu, true);
        break;
    case M_CONTINUE_ON_NO:
        M_OnContinueChoice(menu, false);
        break;
    case M_CONTINUE_GIVING_UP:
        if (menu->state_frames >= 20) {
            M_CloseContinue(menu);
            Scene_Request(SCENE_GAME_RESULT);
            M_Hide(menu->sprites, M_CONTINUE_SPRITE_COUNT);
            g_GameManager.score.shown = g_GameManager.score.value;
            PlayTime_Restart();
            return;
        }
        break;
    case M_CONTINUE_CONTINUING:
        if (menu->state_frames >= 30) {
            M_CloseContinue(menu);
            M_Hide(menu->sprites, M_CONTINUE_SPRITE_COUNT);
            M_Continue();
            return;
        }
        break;
    }
    for (int32_t i = 0; i < M_CONTINUE_SPRITE_COUNT; i++) {
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
        && (menu->state != M_CONTINUE_OPEN || menu->state_frames > 2)) {
        Anm_DrawUnrotated(&menu->background, &draw);
    }
    // Retail draws the counter here and again in the loop below.
    if (menu->state == M_CONTINUE_ON_YES || menu->state == M_CONTINUE_ON_NO) {
        draw.part = ASCII_CONTINUE_SPRITE_COUNT + 1;
        Anm_DrawUnrotated(&menu->sprites[M_CONTINUE_COUNTER_SPRITE], &draw);
    }
    for (int32_t i = 0; i < M_CONTINUE_SPRITE_COUNT; i++) {
        if (menu->sprites[i].shown) {
            draw.part = (uint32_t)i;
            Anm_DrawUnrotated(&menu->sprites[i], &draw);
        }
    }
}
