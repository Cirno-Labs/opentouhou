#include "th01/gameplay/private.h"
#include "th01/bomb.h"
#include "th01/extend.h"
#include "th01/hud.h"
#include "th01/input.h"
#include "th01/invincibility.h"
#include "th01/items.h"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/player.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/shootout_lasers.h"
#include "th01/sound.h"
#include "th01/sprite.h"
#include "th01/stage.h"
#include "th01/timer.h"
#include "th01/vsync_irq.h"

typedef enum {
    GP_LIFE_BEGIN,
    GP_LIFE_BOSS_BEGIN,
    GP_LIFE_WAIT_SHOT,
    GP_LIFE_WAIT_SHOT_DELAY,
    GP_LIFE_FRAME_BEGIN,
    GP_LIFE_PLAYER_BOMB,
    GP_LIFE_ITEMS,
    GP_LIFE_TIMER,
    GP_LIFE_BOSS,
    GP_LIFE_ORB,
    GP_LIFE_AFTER_ORB,
} GAMEPLAY_LIFE_PHASE;

static GAMEPLAY_LIFE_PHASE m_LifePhase, m_AfterPlayer;

static PLAYER_SPRITES M_PlayerSprites(void)
{
    const GAMEPLAY_ASSETS *const a = g_GameplayAssets;
    return (PLAYER_SPRITES) {
        .miko = &a->miko,
        .miko_ac = &a->miko_ac,
        .miko_ac2 = &a->miko_ac2,
    };
}

static void M_DefaultActors(void)
{
    Sprite_PtnPut(&g_GameplayAssets->miko, 0, -1, g_Player->left, PLAYER_TOP);
    Orb_Put(Gameplay_StageSprites(), g_Orb.left, g_Orb.top, 0);
}

static bool M_BossTick(const TH01_INPUT *const raw, const uint64_t host_tick)
{
    const BOSS_STRATEGY *const boss = Gameplay_Boss();
    return boss == nullptr || Gameplay_BossCall(boss->update, raw, host_tick);
}

static void M_LifeBegin(const TH01_INPUT *const raw, const uint64_t host_tick)
{
    Player_Reset(g_Session.settings.difficulty);
    M_DefaultActors();
    GameInput_ResetMenu();
    GameInput_Consume(GAME_INPUT_PRESS_PAUSE);
    Hud_ScoreAndCombo();
    GameInput_DisableBomb(60);
    Stage_UpdateObstacles(Gameplay_StageSprites(), true);
    const BOSS_STRATEGY *const boss = Gameplay_Boss();
    const int32_t song = boss != nullptr ? boss->song : Session_Scene();
    if (!g_Gameplay.bgm_started && song >= 0) {
        Sound_Load((size_t)song);
        Sound_Play();
        g_Gameplay.bgm_started = true;
    }
    GameInput_ResetGame();
    Player_ResumeInvincibility();
    if (boss != nullptr && boss->update_on_life_begin) {
        m_LifePhase = GP_LIFE_BOSS_BEGIN;
    } else {
        if (boss != nullptr && boss->begin_life != nullptr) {
            boss->begin_life();
        }
        m_LifePhase = GP_LIFE_WAIT_SHOT;
    }
    if (m_LifePhase == GP_LIFE_BOSS_BEGIN && M_BossTick(raw, host_tick)) {
        m_LifePhase = GP_LIFE_WAIT_SHOT;
    }
}

static void M_StartFrames(void)
{
    g_Gameplay.wait_for_shot = false;
    GameInput_Consume(GAME_INPUT_PRESS_SHOT);
    g_Stage.timer_initialized = true;
    Rng_Seed(g_Session.frame_rand);
    GameInput_OpenBombWindow();
    g_Stage.first_stage = false;
    m_LifePhase = GP_LIFE_FRAME_BEGIN;
}

static void M_PlayerBegin(const GAMEPLAY_LIFE_PHASE after)
{
    m_AfterPlayer = after;
    const PLAYER_SPRITES sprites = M_PlayerSprites();
    if (Player_BeginUpdate(&g_Gameplay.player_bomb, &sprites)) {
        const BOMB_UPDATE result = Bomb_BeginUpdate(
            &g_GameplayAssets->miko, &g_GameplayAssets->bomb,
            (int16_t)g_Player->bomb_frame);
        if (result == BOMB_UPDATE_WAIT) {
            m_LifePhase = GP_LIFE_PLAYER_BOMB;
            return;
        }
        Player_FinishBombUpdate(
            &g_Gameplay.player_bomb, &sprites, result == BOMB_UPDATE_DONE);
    }
    m_LifePhase = after;
}

void GameplayLife_Begin(void)
{
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_LIFE;
    m_LifePhase = GP_LIFE_BEGIN;
}

void GameplayLife_Resume(void)
{
    g_Gameplay.sequence = GAMEPLAY_SEQUENCE_LIFE;
    m_LifePhase = GP_LIFE_AFTER_ORB;
}

bool GameplayLife_Step(const TH01_INPUT *const raw, const uint64_t host_tick)
{
    switch (m_LifePhase) {
    case GP_LIFE_BEGIN:
        M_LifeBegin(raw, host_tick);
        return true;
    case GP_LIFE_BOSS_BEGIN:
        if (!M_BossTick(raw, host_tick)) {
            return false;
        }
        m_LifePhase = GP_LIFE_WAIT_SHOT;
        return true;
    case GP_LIFE_WAIT_SHOT:
        if (g_Stage.boss != BOSS_NONE || !g_Gameplay.wait_for_shot
            || g_GameInput->shot) {
            M_StartFrames();
        } else {
            Gameplay_Sense(raw);
            M_PlayerBegin(GP_LIFE_WAIT_SHOT_DELAY);
        }
        return true;
    case GP_LIFE_WAIT_SHOT_DELAY:
        if (!VSyncIrq_DelayReady(1)) {
            return false;
        }
        Player_AdvanceBombFrame();
        m_LifePhase = GP_LIFE_WAIT_SHOT;
        return true;
    case GP_LIFE_FRAME_BEGIN:
        if (g_Player->is_hit) {
            g_Stage.timer_initialized = false;
            GameplayExit_Begin();
            return true;
        }
        g_Session.frame_rand++;
        {
            const int32_t period =
                1800 - g_Session.lives * 200 - g_Player->remaining_bombs * 50;
            if (period != 0 && g_Session.frame_rand % (uint32_t)period == 0) {
                Pellets_SpeedRaise(1);
            }
        }
        Gameplay_Sense(raw);
        Player_TickInvincibility();
        M_PlayerBegin(GP_LIFE_ITEMS);
        return true;
    case GP_LIFE_PLAYER_BOMB: {
        const BOMB_UPDATE result = Bomb_ResumeUpdate();
        if (result == BOMB_UPDATE_WAIT) {
            return false;
        }
        const PLAYER_SPRITES sprites = M_PlayerSprites();
        Player_FinishBombUpdate(
            &g_Gameplay.player_bomb, &sprites, result == BOMB_UPDATE_DONE);
        m_LifePhase = m_AfterPlayer;
        return true;
    }
    case GP_LIFE_ITEMS:
        Items_UpdateAndRender(
            Gameplay_StageSprites(), &g_GameplayAssets->item_font);
        g_Stage.frame++;
        Orb_AdvanceForceFrame();
        Player_AdvanceBombFrame();
        GameInput_AdvanceBombFrame();
        m_LifePhase =
            (g_Session.frame_rand & 3) == 0 ? GP_LIFE_TIMER : GP_LIFE_BOSS;
        return true;
    case GP_LIFE_TIMER:
        if (!StageTimer_TickAndPut()) {
            return false;
        }
        m_LifePhase = GP_LIFE_BOSS;
        return true;
    case GP_LIFE_BOSS:
        if (!g_Gameplay.waiting) {
            Invincibility_UpdateAndRender(
                &g_Gameplay.invincibility, &g_GameplayAssets->invincibility);
            g_Gameplay.waiting = true;
        }
        if (!M_BossTick(raw, host_tick)) {
            return false;
        }
        g_Gameplay.waiting = false;
        ShootoutLasers_UpdateAll(g_GameplayAssets->lasers);
        m_LifePhase = GP_LIFE_ORB;
        return Stage_BeginOrbComposite(
            Gameplay_StageSprites(), &g_GameplayAssets->hud_font,
            g_GameplayAssets->pellets);
    case GP_LIFE_ORB:
        if (!Stage_ResumeOrbComposite(
                Gameplay_StageSprites(), &g_GameplayAssets->hud_font,
                g_GameplayAssets->pellets)) {
            return false;
        }
        if (g_GameInput->paused) {
            GameplayPause_Begin();
        } else {
            m_LifePhase = GP_LIFE_AFTER_ORB;
        }
        return true;
    case GP_LIFE_AFTER_ORB:
        Extend_UpdateAndRender();
        if (g_Stage.game_cleared) {
            GameplayEnd_BeginVictory();
        } else {
            if (g_Pellets.destroy_score_delta != 0) {
                Stage_AddScore(g_Pellets.destroy_score_delta);
                Hud_ScoreAndCombo();
                g_Pellets.destroy_score_delta = 0;
            }
            m_LifePhase = GP_LIFE_FRAME_BEGIN;
        }
        return true;
    }
    return true;
}
