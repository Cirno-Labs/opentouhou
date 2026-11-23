#include "th06/ascii/pause_menu.h"

#include "th06/ascii/text.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"

#include "core/base/color.h"

#define M_HIGHLIGHT COLOR_ARGB(0xff, 0xff, 0x80, 0x80)
#define M_DIM COLOR_ARGB(0x80, 0x80, 0x80, 0x80)

// The pause menu shows a title, resume and quit, then a question, yes and
// no. The continue menu shows two lines, yes and no, and then draws the
// number of continues left in sprite 4.
#define M_PAUSE_SPRITE_COUNT ASCII_MENU_SPRITE_COUNT
#define M_PAUSE_MAIN_COUNT 3
#define M_PAUSE_RESUME_SPRITE 1
#define M_PAUSE_YES_SPRITE 4
#define M_CONTINUE_SPRITE_COUNT 4
#define M_CONTINUE_YES_SPRITE 2
#define M_CONTINUE_COUNTER_SPRITE 4

typedef enum {
    M_PAUSE_OPEN = 0,
    M_PAUSE_ON_RESUME = 1,
    M_PAUSE_ON_QUIT = 2,
    M_PAUSE_RESUMING = 3,
    M_PAUSE_ON_YES = 4,
    M_PAUSE_ON_NO = 5,
    M_PAUSE_QUITTING = 6,
} M_PAUSE_STATE;

typedef enum {
    M_CONTINUE_OPEN = 0,
    M_CONTINUE_ON_YES = 1,
    M_CONTINUE_ON_NO = 2,
    M_CONTINUE_CONTINUING = 3,
    M_CONTINUE_GIVING_UP = 4,
} M_CONTINUE_STATE;

static const VEC3 m_Raised = { -4.0f, -4.0f, 0.0f };

static void M_SetViewport(void)
{
    VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    viewport->x = (float)(uint32_t)g_GameManager.play_area.x;
    viewport->y = (float)(uint32_t)g_GameManager.play_area.y;
    viewport->width = (float)(uint32_t)g_GameManager.play_area.width;
    viewport->height = (float)(uint32_t)g_GameManager.play_area.height;
}

static void M_InterruptShown(ANM_VM *const sprites, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (sprites[i].shown) {
            sprites[i].pending_interrupt = 2;
        }
    }
}

static void M_Interrupt(
    ANM_VM *const sprites, const int32_t first, const int32_t end,
    const int16_t interrupt)
{
    for (int32_t i = first; i < end; i++) {
        sprites[i].pending_interrupt = interrupt;
    }
}

static void M_Hide(ANM_VM *const sprites, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        sprites[i].shown = false;
    }
}

// Highlights sprites[first] or sprites[first + 1], and dims the other.
static void M_Highlight(
    ANM_VM *const sprites, const int32_t first, const bool on_first)
{
    ANM_VM *const on = &sprites[on_first ? first : first + 1];
    ANM_VM *const off = &sprites[on_first ? first + 1 : first];
    on->color = M_HIGHLIGHT;
    on->scale_y = 1.7f;
    on->scale_x = 1.7f;
    on->pos_offset = m_Raised;
    off->color = M_DIM;
    off->scale_y = 1.5f;
    off->scale_x = 1.5f;
    off->pos_offset = (VEC3) {};
}

static bool M_Moved(void)
{
    return Buttons_WasPressed(TH06_BUTTON_up)
        || Buttons_WasPressed(TH06_BUTTON_down);
}

static void M_StartBackground(ANM_VM *const background)
{
    Anm_SetPendingCopy(3, (GFX_RECT) { 32, 16, 384, 448 });
    Anm_StartScript(background, 0x718);
    background->pos = (VEC3) { 32.0f, 16.0f, 0.0f };
}

static void M_OpenPause(PAUSE_MENU *const menu)
{
    for (int32_t i = 0; i < M_PAUSE_SPRITE_COUNT; i++) {
        Anm_StartScript(&menu->sprites[i], i + 2);
    }
    M_Interrupt(menu->sprites, 0, M_PAUSE_MAIN_COUNT, 1);
    menu->state++;
    menu->state_frames = 0;
    if (g_AsciiText.capture_background) {
        M_StartBackground(&menu->background);
    }
}

static void M_OnResume(PAUSE_MENU *const menu)
{
    M_Highlight(menu->sprites, M_PAUSE_RESUME_SPRITE, true);
    if (menu->state_frames < 4) {
        return;
    }
    if (M_Moved()) {
        menu->state = M_PAUSE_ON_QUIT;
    }
    if (Buttons_WasPressed(TH06_BUTTON_shoot)) {
        M_Interrupt(menu->sprites, 0, M_PAUSE_MAIN_COUNT, 2);
        menu->state = M_PAUSE_RESUMING;
        menu->state_frames = 0;
        menu->background.pending_interrupt = 1;
    }
}

static void M_OnQuit(PAUSE_MENU *const menu)
{
    M_Highlight(menu->sprites, M_PAUSE_RESUME_SPRITE, false);
    if (menu->state_frames < 4) {
        return;
    }
    if (M_Moved()) {
        menu->state = M_PAUSE_ON_RESUME;
    }
    if (Buttons_WasPressed(TH06_BUTTON_shoot)) {
        M_Interrupt(menu->sprites, 0, M_PAUSE_MAIN_COUNT, 2);
        M_Interrupt(menu->sprites, M_PAUSE_MAIN_COUNT, M_PAUSE_SPRITE_COUNT, 1);
        menu->state = M_PAUSE_ON_NO;
        menu->state_frames = 0;
    }
}

static void M_OnYes(PAUSE_MENU *const menu)
{
    M_Highlight(menu->sprites, M_PAUSE_YES_SPRITE, true);
    if (menu->state_frames < 4) {
        return;
    }
    if (M_Moved()) {
        menu->state = M_PAUSE_ON_NO;
    }
    if (Buttons_WasPressed(TH06_BUTTON_shoot)) {
        M_Interrupt(menu->sprites, M_PAUSE_MAIN_COUNT, M_PAUSE_SPRITE_COUNT, 2);
        menu->state = M_PAUSE_QUITTING;
        menu->state_frames = 0;
    }
}

static void M_OnNo(PAUSE_MENU *const menu)
{
    M_Highlight(menu->sprites, M_PAUSE_YES_SPRITE, false);
    if (menu->state_frames < 4) {
        return;
    }
    if (M_Moved()) {
        menu->state = M_PAUSE_ON_YES;
    }
    if (Buttons_WasPressed(TH06_BUTTON_shoot)) {
        M_Interrupt(menu->sprites, 0, M_PAUSE_MAIN_COUNT, 1);
        M_Interrupt(menu->sprites, M_PAUSE_MAIN_COUNT, M_PAUSE_SPRITE_COUNT, 2);
        menu->state = M_PAUSE_ON_QUIT;
        menu->state_frames = 0;
    }
}

static void M_ClosePause(PAUSE_MENU *const menu, const bool quit)
{
    if (menu->state_frames < 20) {
        return;
    }
    menu->state = M_PAUSE_OPEN;
    g_GameManager.mode.paused = 0;
    if (quit) {
        Scene_Request(SCENE_TITLE);
    }
    M_Hide(menu->sprites, M_PAUSE_SPRITE_COUNT);
}

void PauseMenu_Update(PAUSE_MENU *const menu)
{
    if (Buttons_WasPressed(TH06_BUTTON_menu)) {
        menu->state = M_PAUSE_RESUMING;
        M_InterruptShown(menu->sprites, M_PAUSE_SPRITE_COUNT);
        menu->state_frames = 0;
        menu->background.pending_interrupt = 1;
    }
    if (Buttons_WasPressed(TH06_BUTTON_quit)) {
        menu->state = M_PAUSE_QUITTING;
        M_InterruptShown(menu->sprites, M_PAUSE_SPRITE_COUNT);
        menu->state_frames = 0;
    }
    switch (menu->state) {
    case M_PAUSE_OPEN:
        M_OpenPause(menu);
        M_OnResume(menu);
        break;
    case M_PAUSE_ON_RESUME:
        M_OnResume(menu);
        break;
    case M_PAUSE_ON_QUIT:
        M_OnQuit(menu);
        break;
    case M_PAUSE_RESUMING:
        M_ClosePause(menu, false);
        break;
    case M_PAUSE_ON_YES:
        M_OnYes(menu);
        break;
    case M_PAUSE_ON_NO:
        M_OnNo(menu);
        break;
    case M_PAUSE_QUITTING:
        M_ClosePause(menu, true);
        break;
    }
    for (int32_t i = 0; i < M_PAUSE_SPRITE_COUNT; i++) {
        Anm_Step(&menu->sprites[i]);
    }
    if (g_AsciiText.capture_background) {
        Anm_Step(&menu->background);
    }
    menu->state_frames++;
}

void PauseMenu_DrawPauseMenu(PAUSE_MENU *const menu)
{
    if (g_GameManager.mode.paused != 0) {
        M_SetViewport();
        ANM_DRAW draw = Anm_ScreenDraw();
        draw.interpolate = true;
        draw.part = ASCII_MENU_SPRITE_COUNT;
        draw.view =
            Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
        if (g_AsciiText.capture_background && menu->state != M_PAUSE_OPEN) {
            ANM_VM background = menu->background;
            background.z_write_off = true;
            Anm_DrawUnrotated(&background, &draw);
        }
        for (int32_t i = 0; i < ASCII_MENU_SPRITE_COUNT; i++) {
            if (menu->sprites[i].shown) {
                draw.part = (uint32_t)i;
                Anm_DrawUnrotated(&menu->sprites[i], &draw);
            }
        }
    }
}

// Starts the sprites on the first calc. Returns false once 8 calcs have
// passed; otherwise moves the cursor to no, which retail draws as yes on
// that calc.
static bool M_OpenContinue(PAUSE_MENU *const menu)
{
    if (menu->state_frames == 0) {
        for (int32_t i = 0; i < M_CONTINUE_SPRITE_COUNT; i++) {
            Anm_StartScript(&menu->sprites[i], i < 2 ? i + 8 : i + 4);
            menu->sprites[i].pending_interrupt = 1;
        }
        if (g_AsciiText.capture_background) {
            M_StartBackground(&menu->background);
        }
    }
    if (menu->state_frames > 8) {
        return false;
    }
    menu->state += 2;
    menu->state_frames = 0;
    return true;
}

static void M_OnContinue(PAUSE_MENU *const menu)
{
    M_Highlight(menu->sprites, M_CONTINUE_YES_SPRITE, true);
    if (menu->state_frames < 4) {
        return;
    }
    if (M_Moved()) {
        menu->state = M_CONTINUE_ON_NO;
    }
    if (Buttons_WasPressed(TH06_BUTTON_shoot)) {
        M_Interrupt(menu->sprites, 0, M_CONTINUE_SPRITE_COUNT, 2);
        menu->state = M_CONTINUE_CONTINUING;
        menu->background.pending_interrupt = 1;
        menu->state_frames = 0;
    }
}

static void M_OnGiveUp(PAUSE_MENU *const menu)
{
    M_Highlight(menu->sprites, M_CONTINUE_YES_SPRITE, false);
    if (menu->state_frames < 30) {
        return;
    }
    if (M_Moved()) {
        menu->state = M_CONTINUE_ON_YES;
    }
    if (Buttons_WasPressed(TH06_BUTTON_shoot)) {
        M_Interrupt(menu->sprites, 0, M_CONTINUE_SPRITE_COUNT, 2);
        menu->state = M_CONTINUE_GIVING_UP;
        menu->state_frames = 0;
    }
}

static void M_GiveUp(PAUSE_MENU *const menu)
{
    menu->state = M_CONTINUE_OPEN;
    menu->state_frames = 0;
    g_GameManager.mode.in_continue_menu = false;
    Scene_Request(SCENE_GAME_RESULT);
    M_Hide(menu->sprites, M_CONTINUE_SPRITE_COUNT);
    g_GameManager.score.shown = g_GameManager.score.value;
}

static void M_Continue(PAUSE_MENU *const menu)
{
    menu->state = M_CONTINUE_OPEN;
    menu->state_frames = 0;
    g_GameManager.mode.in_continue_menu = false;
    M_Hide(menu->sprites, M_CONTINUE_SPRITE_COUNT);
    g_GameManager.stats.continues_used++;
    g_GameManager.score.shown = g_GameManager.stats.continues_used;
    g_GameManager.score.step = 0;
    g_GameManager.score.value = g_GameManager.score.shown;
    g_GameManager.player.lives = (int8_t)g_GameManager.run.start_lives;
    g_GameManager.player.bombs = (int8_t)g_GameManager.run.start_bombs;
    g_GameManager.player.graze = 0;
    g_GameManager.player.point_items = 0;
    g_GameManager.player.power = 0;
    g_GameManager.score.next_extend = 0;
}

void PauseMenu_UpdateContinueMenu(PAUSE_MENU *const menu)
{
    if (g_GameManager.mode.in_practice) {
        g_GameManager.mode.in_continue_menu = false;
        g_GameManager.score.shown = g_GameManager.score.value;
        Scene_Request(SCENE_GAME_RESULT);
        return;
    }
    if (g_GameManager.mode.in_replay) {
        g_GameManager.mode.in_continue_menu = false;
        Scene_Request(SCENE_REPLAY_END);
        g_GameManager.score.shown = g_GameManager.score.value;
        return;
    }
    if (g_GameManager.stats.continues_used >= 3
        || g_GameManager.run.difficulty >= 4) {
        g_GameManager.mode.in_continue_menu = false;
        Scene_Request(SCENE_GAME_RESULT);
        g_GameManager.score.shown = g_GameManager.score.value;
        return;
    }
    switch (menu->state) {
    case M_CONTINUE_OPEN:
        if (!M_OpenContinue(menu)) {
            break;
        }
        M_OnContinue(menu);
        break;
    case M_CONTINUE_ON_YES:
        M_OnContinue(menu);
        break;
    case M_CONTINUE_ON_NO:
        M_OnGiveUp(menu);
        break;
    case M_CONTINUE_GIVING_UP:
        if (menu->state_frames >= 20) {
            M_GiveUp(menu);
            return;
        }
        break;
    case M_CONTINUE_CONTINUING:
        if (menu->state_frames >= 30) {
            M_Continue(menu);
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

void PauseMenu_DrawContinueMenu(PAUSE_MENU *const menu)
{
    if (g_GameManager.mode.in_continue_menu) {
        M_SetViewport();
        ANM_DRAW draw = Anm_ScreenDraw();
        draw.interpolate = true;
        draw.part = ASCII_MENU_SPRITE_COUNT;
        draw.view =
            Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
        if (g_AsciiText.capture_background
            && (menu->state != M_CONTINUE_OPEN || menu->state_frames > 2)) {
            Anm_DrawUnrotated(&menu->background, &draw);
        }
        if (menu->state == M_CONTINUE_ON_YES
            || menu->state == M_CONTINUE_ON_NO) {
            ANM_VM *const shown = &menu->sprites[M_CONTINUE_COUNTER_SPRITE];
            *shown = menu->sprites[1];
            shown->pos.x += 8.0f * shown->scale_x;
            Anm_SetSprite(shown, 0x1e - g_GameManager.stats.continues_used);
            ANM_VM counter = *shown;
            counter.presentation_generation =
                menu->sprites[1].presentation_generation;
            draw.part = M_CONTINUE_COUNTER_SPRITE;
            Anm_DrawUnrotated(&counter, &draw);
        }
        for (int32_t i = 0; i < M_CONTINUE_SPRITE_COUNT; i++) {
            if (menu->sprites[i].shown) {
                draw.part = (uint32_t)i;
                Anm_DrawUnrotated(&menu->sprites[i], &draw);
            }
        }
    }
}
