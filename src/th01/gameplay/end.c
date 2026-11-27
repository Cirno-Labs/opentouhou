#include "th01/bonus.h"
#include "th01/extend.h"
#include "th01/gameplay/private.h"
#include "th01/input.h"
#include "th01/items.h"
#include "th01/orb.h"
#include "th01/palette.h"
#include "th01/pellets.h"
#include "th01/player.h"
#include "th01/rng.h"
#include "th01/scale2x.h"
#include "th01/session.h"
#include "th01/shots.h"
#include "th01/sound.h"
#include "th01/sprite.h"
#include "th01/stage.h"
#include "th01/stage_objects.h"
#include "th01/video.h"
#include "th01/vsync_irq.h"

#include "pc98/zun/master_rng.h"

typedef enum {
    GP_END_STAGE_BONUS,
    GP_END_TOTAL_BONUS,
    GP_END_GAMEOVER,
    GP_END_GAMEOVER_DELAY,
    GP_END_GAMEOVER_FINISH,
    GP_END_CONTINUE,
    GP_END_CONTINUE_DELAY,
    GP_END_VICTORY_DELAY,
    GP_END_SWITCH,
} GAMEPLAY_END_PHASE;

// Names the program that game_switch_binary's black-out leads to: REIIDEN
// again for a boss stage or a Continue, OP or FUUIN.
typedef enum {
    GAMEPLAY_SWITCH_STAGE,
    GAMEPLAY_SWITCH_CONTINUE,
    GAMEPLAY_SWITCH_OPENING,
    GAMEPLAY_SWITCH_ENDING,
} GAMEPLAY_SWITCH;

static GAMEPLAY_END_PHASE m_EndPhase;
static GAMEPLAY_SWITCH m_SwitchTo;

static void M_BeginSwitch(const GAMEPLAY_SWITCH to)
{
    m_SwitchTo = to;
    Palette_BeginFade(
        &g_Gameplay.switch_fade, &g_Palette, PALETTE_FADE_BLACK_OUT);
    VSyncIrq_BeginWait(&g_Gameplay.wait);
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_END;
    m_EndPhase = GP_END_SWITCH;
}

static void M_Switch(void)
{
    switch (m_SwitchTo) {
    case GAMEPLAY_SWITCH_STAGE:
        Gameplay_Reset();
        Sound_Load(11);
        Sound_Play();
        return;
    case GAMEPLAY_SWITCH_CONTINUE:
        Session_Continue(g_Pellets->speed, (int16_t)Extend_ResidentLives());
        Gameplay_Reset();
        g_Gameplay.bgm_started = true;
        return;
    case GAMEPLAY_SWITCH_OPENING:
        g_Gameplay.request = GAMEPLAY_REQUEST_OPENING;
        g_Gameplay.sequence = GAMEPLAY_SEQUENCE_TERMINAL;
        return;
    case GAMEPLAY_SWITCH_ENDING:
        g_Gameplay.request = GAMEPLAY_REQUEST_ENDING;
        g_Gameplay.sequence = GAMEPLAY_SEQUENCE_TERMINAL;
        return;
    }
}

static void M_RequestOpening(void)
{
    M_BeginSwitch(GAMEPLAY_SWITCH_OPENING);
}

static void M_AfterBonus(void)
{
    if (g_Pellets->speed < 0) {
        Pellets_SetSpeed(0);
    }
    Gameplay_SyncSession();
    if (g_Session.stage_id % 5 == 4 || g_Stage.boss != BOSS_NONE) {
        Sound_Fade();
        M_BeginSwitch(GAMEPLAY_SWITCH_STAGE);
    } else {
        Orb_LeavePortal();
        Video_SelectDrawingPage(1);
        StageObjects_PutAllBackgrounds();
        Video_SelectDrawingPage(0);
        GameplayStage_Begin();
    }
}

static void M_ContinueMenu(void)
{
    Scale2x_Copy(288, 200, 0, g_Gameplay.menu_selection == 0 ? 16 : 48, 48, 16);
    Scale2x_Copy(288, 232, 0, g_Gameplay.menu_selection == 0 ? 64 : 32, 48, 16);
}

void GameplayEnd_BeginBonus(
    const int16_t finished, const TH01_INPUT *const raw,
    const uint64_t host_tick)
{
    const GAMEPLAY_ASSETS *const a = g_GameplayAssets;
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_END;
    if (g_Stage.boss != BOSS_NONE) {
        m_EndPhase = GP_END_TOTAL_BONUS;
        if (Bonus_BeginTotal(&a->total, finished, raw, host_tick)) {
            M_AfterBonus();
        }
    } else {
        m_EndPhase = GP_END_STAGE_BONUS;
        if (Bonus_BeginStage(a->bonus, &a->bonus_font, finished, raw)) {
            M_AfterBonus();
        }
    }
}

void GameplayEnd_BeginGameover(void)
{
    Items_Reset();
    Orb_Put(Gameplay_StageSprites(), g_Orb->left, g_Orb->top, 0);
    const int16_t image = MasterRng_Next(&g_Rng) % 8 == 0 ? 23 : 22;
    Sprite_PtnPut(
        &g_GameplayAssets->miko, image, -1, g_Player->left, PLAYER_TOP);
    for (int32_t i = 0; i < 5; i++) {
        g_Gameplay.gameover_left[i] = g_Player->left;
        g_Gameplay.gameover_top[i] = PLAYER_TOP;
        g_Gameplay.gameover_clipped[i] = false;
    }
    Gameplay_CopyPage(0, 1);
    g_Gameplay.animation_frame = 0;
    VSyncIrq_BeginWait(&g_Gameplay.wait);
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_END;
    m_EndPhase = GP_END_GAMEOVER;
}

void GameplayEnd_BeginVictory(void)
{
    Gameplay_SyncSession();
    g_Session.ending =
        g_Session.route == 0 ? SESSION_END_MAKAI : SESSION_END_JIGOKU;
    if (g_Stage.score > g_Session.highest_score) {
        g_Session.highest_score = g_Stage.score;
    }
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_END;
    m_EndPhase = GP_END_VICTORY_DELAY;
}

void GameplayEnd_BeginContinue(void)
{
    Video_SelectDrawingPage(1);
    Gameplay_CopyPage(1, 0);
    const char *const lines[] = {
        "ｃｏｎｔｉｎｕｅ？　　　  ",
        "Ｙｅｓ　　  　",
        "Ｎｏ　　  　 ",
        "Ｙｅｓ　　　  ",
        "Ｎｏ　　　   ",
    };
    for (int32_t i = 0; i < 5; i++) {
        Gameplay_Draw(lines[i], 0, i * 16, i < 3 ? 7 : 1, GAME_TEXT_BOLD, true);
    }
    Video_SelectDrawingPage(0);
    Scale2x_Copy(160, 64, 0, 0, 144, 16);
    g_Gameplay.menu_selection = 0;
    M_ContinueMenu();
    GameInput_ResetGame();
    Session_CountContinue();
    if (g_Session.highest_score < g_Stage.score) {
        g_Session.highest_score = g_Stage.score;
    }
    g_Session.score = 0;
    // Restores the resident's lives; the live count stays until the re-exec.
    Extend_SyncResidentLives(g_Session.settings.lives_extra + 2);
    Pellets_SpeedLower(-2, -5);
    g_Gameplay.continue_frame = 0;
    g_Gameplay.continue_locked = g_Session.settings.fix_debouncing;
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_END;
    m_EndPhase = GP_END_CONTINUE;
}

void GameplayEnd_OpenMenu(void)
{
    Gameplay_SyncSession();
    M_RequestOpening();
}

bool GameplayEnd_Switching(void)
{
    return g_Gameplay.sequence == GAMEPLAY_SEQUENCE_END
        && m_EndPhase == GP_END_SWITCH;
}

bool GameplayEnd_Step(const TH01_INPUT *const raw, const uint64_t host_tick)
{
    const GAMEPLAY_ASSETS *const a = g_GameplayAssets;
    switch (m_EndPhase) {
    case GP_END_STAGE_BONUS:
        if (!Bonus_Resume(a->bonus, &a->bonus_font, raw)) {
            return false;
        }
        M_AfterBonus();
        return true;
    case GP_END_TOTAL_BONUS:
        if (!Bonus_ResumeTotal(&a->total, raw, host_tick)) {
            return false;
        }
        M_AfterBonus();
        return true;
    case GP_END_GAMEOVER:
        if (!VSyncIrq_WaitReady(&g_Gameplay.wait)) {
            return false;
        }
        Video_SetScroll((int16_t)(400 - g_Gameplay.animation_frame % 2 * 8));
        const int32_t frame = g_Gameplay.animation_frame;
        // Model the clipped-flag overread through the adjacent top and
        // left arrays. The caller's nonzero saved BP suppresses frame 15.
        const bool restore = frame < 5 ? !g_Gameplay.gameover_clipped[frame]
            : frame < 10               ? g_Gameplay.gameover_top[frame - 5] == 0
            : frame < 15 ? g_Gameplay.gameover_left[frame - 10] == 0
                         : false;
        if (restore) {
            for (int32_t i = 0; i < 5; i++) {
                Sprite_Restore16(
                    g_Gameplay.gameover_left[i], PLAYER_TOP, 32, 32);
            }
        }
        g_Gameplay.gameover_left[0] -= 8;
        g_Gameplay.gameover_left[1] -= 8;
        g_Gameplay.gameover_left[3] += 8;
        g_Gameplay.gameover_left[4] += 8;
        g_Gameplay.gameover_top[1] -= 8;
        g_Gameplay.gameover_top[2] -= 8;
        g_Gameplay.gameover_top[3] -= 8;
        for (int32_t i = 0; i < 5; i++) {
            if (g_Gameplay.gameover_left[i] >= 608
                || g_Gameplay.gameover_left[i] < 0) {
                g_Gameplay.gameover_clipped[i] = true;
            }
            if (!g_Gameplay.gameover_clipped[i]) {
                Sprite_PtnPut(
                    &a->miko, 20 + g_Gameplay.animation_frame % 2, -1,
                    g_Gameplay.gameover_left[i], g_Gameplay.gameover_top[i]);
            }
        }
        m_EndPhase = GP_END_GAMEOVER_DELAY;
        return true;
    case GP_END_GAMEOVER_DELAY:
        if (!VSyncIrq_DelayReady(g_Gameplay.animation_frame / 6)) {
            return false;
        }
        g_Gameplay.animation_frame++;
        VSyncIrq_BeginWait(&g_Gameplay.wait);
        m_EndPhase = g_Gameplay.animation_frame < 16 ? GP_END_GAMEOVER
                                                     : GP_END_GAMEOVER_FINISH;
        return true;
    case GP_END_GAMEOVER_FINISH:
        if (!VSyncIrq_WaitReady(&g_Gameplay.wait)) {
            return false;
        }
        Video_SetScroll(0);
        Player_SetBombs(1);
        Shots_EraseAndReset();
        Pellets_EraseAndResetMoving();
        Extend_Reset();
        Gameplay_SyncSession();
        g_Gameplay.request = GAMEPLAY_REQUEST_SCORE;
        g_Gameplay.sequence = GAMEPLAY_SEQUENCE_TERMINAL;
        return true;
    case GP_END_CONTINUE:
        Gameplay_Sense(raw);
        g_Gameplay.continue_frame++;
        m_EndPhase = GP_END_CONTINUE_DELAY;
        return true;
    case GP_END_CONTINUE_DELAY:
        if (!VSyncIrq_DelayReady(1)) {
            return false;
        }
        const bool confirm = g_GameInput->ok || g_GameInput->shot;
        g_Gameplay.continue_locked &= confirm;
        if (confirm && !g_Gameplay.continue_locked) {
            if (g_Gameplay.menu_selection == 0) {
                M_BeginSwitch(GAMEPLAY_SWITCH_CONTINUE);
            } else {
                Player_ClearHit();
                GameInput_Consume(GAME_INPUT_PRESS_PAUSE);
                Sound_Stop();
                M_RequestOpening();
            }
        } else if (g_Gameplay.continue_frame > 3000 || g_GameInput->paused) {
            Player_ClearHit();
            GameInput_Consume(GAME_INPUT_PRESS_PAUSE);
            if (g_Gameplay.continue_frame > 3000) {
                Sound_Stop();
            }
            M_RequestOpening();
        } else {
            if (g_GameInput->up) {
                g_Gameplay.menu_selection = 0;
                M_ContinueMenu();
            }
            if (g_GameInput->down) {
                g_Gameplay.menu_selection = 1;
                M_ContinueMenu();
            }
            m_EndPhase = GP_END_CONTINUE;
        }
        return true;
    case GP_END_VICTORY_DELAY:
        if (!VSyncIrq_DelayReady(120)) {
            return false;
        }
        M_BeginSwitch(GAMEPLAY_SWITCH_ENDING);
        return true;
    case GP_END_SWITCH:
        if (!VSyncIrq_WaitReady(&g_Gameplay.wait)) {
            return false;
        }
        Palette_FadeStep(&g_Gameplay.switch_fade, &g_Palette);
        if (g_Gameplay.switch_fade.remaining > 0) {
            VSyncIrq_BeginWait(&g_Gameplay.wait);
        } else {
            M_Switch();
        }
        return true;
    }
    return true;
}
