#include "th01/gameplay/private.h"
#include "th01/input.h"
#include "th01/palette.h"
#include "th01/player.h"
#include "th01/sprite.h"
#include "th01/vsync_irq.h"

typedef enum {
    GP_PAUSE_MENU,
    GP_PAUSE_DELAY,
    GP_PAUSE_QUIT_CONFIRM,
} GAMEPLAY_PAUSE_PHASE;

static GAMEPLAY_PAUSE_PHASE m_PausePhase;

static void M_PauseDraw(const bool quit)
{
    Gameplay_Draw(
        quit ? "本当に終了しちゃうの？" : "ＰＡＵＳＥ", quit ? 224 : 272, 128,
        7, GAME_TEXT_BLACK, false);
    Gameplay_Draw(
        quit ? "　うそですぅ　　はいっ" : "　再開　　終了", quit ? 224 : 256,
        144, 7, GAME_TEXT_BLACK, false);
    Gameplay_Draw("●", quit ? 224 : 256, 144, 7, GAME_TEXT_NORMAL, false);
    g_Gameplay.menu_selection = 0;
    GameInput_ResetMenu();
    g_Gameplay.waiting = false;
}

static bool M_PausePoll(const TH01_INPUT *const raw, const bool quit)
{
    for (;;) {
        if (g_Gameplay.waiting && !VSyncIrq_DelayReady(1)) {
            return false;
        }
        g_Gameplay.waiting = false;
        if (g_GameInput->paused) {
            Gameplay_Sense(raw);
            if (g_Player.is_hit) {
                GameplayEnd_OpenMenu();
                return true;
            }
            if (g_GameInput->horizontal == GAME_INPUT_LEFT
                || g_GameInput->horizontal == GAME_INPUT_RIGHT) {
                const int32_t left = quit ? 224 : 256;
                const int32_t second = quit ? 336 : 320;
                const bool select_second =
                    g_GameInput->horizontal == GAME_INPUT_RIGHT;
                Sprite_Restore16(
                    (int16_t)(select_second ? left : second), 144, 16, 16);
                Gameplay_Draw(
                    "●", select_second ? second : left, 144, 7,
                    GAME_TEXT_NORMAL, false);
                g_Gameplay.menu_selection = select_second;
            }
            if (!g_GameInput->shot && !g_GameInput->ok) {
                g_Gameplay.waiting = true;
                if (!VSyncIrq_DelayReady(1)) {
                    return false;
                }
                g_Gameplay.waiting = false;
                continue;
            }
        }
        if (quit) {
            if (g_Gameplay.menu_selection != 0) {
                Session_CountContinue();
                GameplayEnd_OpenMenu();
            } else {
                Palette_Set(&g_Palette, &g_StagePalette);
                GameInput_ResetGame();
                Sprite_Restore16(232, 128, 176, 32);
                GameplayLife_Resume();
            }
        } else {
            Sprite_Restore16(276, 128, 80, 16);
            Sprite_Restore16(256, 144, 112, 16);
            m_PausePhase = GP_PAUSE_DELAY;
        }
        return true;
    }
}

void GameplayPause_Begin(void)
{
    M_PauseDraw(false);
    PALETTE dark = g_Palette.saved;
    for (int32_t i = 0; i < 16; i++) {
        if (i == 7) {
            continue;
        }
        for (int32_t c = 0; c < 3; c++) {
            dark.colors[i][c] = (int8_t)(dark.colors[i][c] * 40 / 100);
        }
    }
    Palette_Set(&g_Palette, &dark);
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_PAUSE;
    m_PausePhase = GP_PAUSE_MENU;
}

bool GameplayPause_Step(const TH01_INPUT *const raw)
{
    switch (m_PausePhase) {
    case GP_PAUSE_MENU:
        return M_PausePoll(raw, false);
    case GP_PAUSE_DELAY:
        if (!VSyncIrq_DelayReady(20)) {
            return false;
        }
        GameInput_ResetMenu();
        if (g_Gameplay.menu_selection != 0 && g_GameInput->paused) {
            M_PauseDraw(true);
            m_PausePhase = GP_PAUSE_QUIT_CONFIRM;
        } else {
            Palette_Set(&g_Palette, &g_StagePalette);
            GameInput_ResetGame();
            Sprite_Restore16(232, 128, 176, 32);
            GameplayLife_Resume();
        }
        return true;
    case GP_PAUSE_QUIT_CONFIRM:
        return M_PausePoll(raw, true);
    }
    return true;
}
