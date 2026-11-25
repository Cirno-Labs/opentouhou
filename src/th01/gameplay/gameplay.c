#include "th01/gameplay.h"
#include "th01/gameplay/private.h"
#include "th01/boss_entity.h"
#include "th01/boss_strategy.h"
#include "th01/hud_hp.h"
#include "th01/missiles.h"
#include "th01/particles.h"
#include "th01/stage_objects.h"
#include "th01/tram.h"
#include "th01/invincibility.h"
#include "th01/items.h"
#include "th01/miss.h"
#include "th01/sound.h"
#include "th01/stage.h"
#include "th01/rng.h"
#include "th01/hooks.h"
#include "th01/scale2x.h"

#include "pc98/zun/master_rng.h"
#include "core/base/assert.h"

#include <string.h>

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

GAMEPLAY g_Gameplay;
const GAMEPLAY_ASSETS *g_GameplayAssets;

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
    Sprite_PtnPut(&g_GameplayAssets->miko, 0, -1, g_Player.left, PLAYER_TOP);
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
    g_GameInput.horizontal = 0;
    g_GameInput.shot = false;
    g_GameInput.ok = false;
    g_GameInput.paused = false;
    Hud_ScoreAndCombo();
    g_GameInput.bomb_frame = 60;
    Stage_UpdateObstacles(Gameplay_StageSprites(), true);
    const BOSS_STRATEGY *const boss = Gameplay_Boss();
    const int32_t song = boss != nullptr ? boss->song : Session_Scene();
    if (!g_Gameplay.bgm_started && song >= 0) {
        Sound_Load((size_t)song);
        Sound_Play();
        g_Gameplay.bgm_started = true;
    }
    GameInput_ResetGame();
    if (g_Player.invincibility_time > 1) {
        g_Player.invincible = true;
    }
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
    g_GameInput.shot = false;
    g_Stage.timer_initialized = true;
    g_Rng = g_Session.frame_rand;
    g_GameInput.bomb_frame = GAME_INPUT_BOMB_WINDOW;
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
            (int16_t)g_Player.bomb_frame);
        if (result == BOMB_UPDATE_WAIT) {
            m_LifePhase = GP_LIFE_PLAYER_BOMB;
            return;
        }
        Player_FinishBombUpdate(
            &g_Gameplay.player_bomb, &sprites, result == BOMB_UPDATE_DONE);
    }
    m_LifePhase = after;
}

static GAMEPLAY_REQUEST M_Run(
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    for (;;) {
        bool next = false;
        switch (g_Gameplay.sequence) {
        case GAMEPLAY_SEQUENCE_STAGE:
            next = GameplayStage_Step(host_tick);
            break;
        case GAMEPLAY_SEQUENCE_LIFE:
            next = GameplayLife_Step(raw, host_tick);
            break;
        case GAMEPLAY_SEQUENCE_PAUSE:
            next = GameplayPause_Step(raw);
            break;
        case GAMEPLAY_SEQUENCE_EXIT:
            next = GameplayExit_Step(raw, host_tick);
            break;
        case GAMEPLAY_SEQUENCE_END:
            next = GameplayEnd_Step(raw, host_tick);
            break;
        case GAMEPLAY_SEQUENCE_TERMINAL:
            break;
        }
        if (!next) {
            return g_Gameplay.request;
        }
    }
}

const PTN *Gameplay_StageSprites(void)
{
    const GAMEPLAY_ASSETS *const a = g_GameplayAssets;
    return g_Gameplay.boss_stage ? &a->boss_stage : &a->stage;
}

void Gameplay_SyncSession(void)
{
    g_Session.score = g_Stage.score;
    g_Session.bombs = g_Player.remaining_bombs;
    g_Session.pellet_speed = g_Pellets.speed;
}

const BOSS_STRATEGY *Gameplay_Boss(void)
{
    return g_Stage.boss == BOSS_NONE ? nullptr : BossStrategy_Get(g_Stage.boss);
}

const void *Gameplay_BossAssets(void)
{
    return g_GameplayAssets->bosses[g_Stage.boss];
}

BOSS_CONTEXT Gameplay_BossContext(
    ROUTE_SELECT_ASSETS *const route, const TH01_INPUT *const raw,
    const uint64_t host_tick)
{
    const GAMEPLAY_ASSETS *const a = g_GameplayAssets;
    *route = (ROUTE_SELECT_ASSETS) { .font = &a->route_font,
                                     .stage = Gameplay_StageSprites(),
                                     .makai = &a->scenes[1],
                                     .jigoku = &a->scenes[2] };
    return (BOSS_CONTEXT) { .miko = &a->miko,
                            .stage = Gameplay_StageSprites(),
                            .route = route,
                            .raw = raw,
                            .host_tick = host_tick };
}

bool Gameplay_BossCall(
    bool (*const call)(const void *assets, const BOSS_CONTEXT *ctx),
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    ROUTE_SELECT_ASSETS route;
    const BOSS_CONTEXT ctx = Gameplay_BossContext(&route, raw, host_tick);
    return call(Gameplay_BossAssets(), &ctx);
}

void Gameplay_CopyPage(const int32_t from, const int32_t to)
{
    memcpy(g_Video.pages[to], g_Video.pages[from], VIDEO_PAGE_SIZE);
}

void Gameplay_Sense(const TH01_INPUT *const raw)
{
    if (GameInput_SenseGame(raw, false)) {
        g_Player.is_hit = true;
    }
}

void Gameplay_Draw(
    const char *const text, const int32_t left, const int32_t top,
    const uint8_t color, const GAME_TEXT_WEIGHT weight, const bool clear)
{
    const GAME_TEXT_STYLE style = { .weight = weight,
                                    .clear_background = clear };
    GameText_Draw(&g_GameplayAssets->menu_font, text, left, top, color, &style);
}

void Gameplay_ContinueUse(void)
{
    g_Session.continues_total =
        (int32_t)((uint32_t)g_Session.continues_total + 1);
    const int16_t scene = g_Session.stage_id / 5;
    g_Session.continues_per_scene[scene]++;
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
            || g_GameInput.shot) {
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
        g_Player.bomb_frame++;
        m_LifePhase = GP_LIFE_WAIT_SHOT;
        return true;
    case GP_LIFE_FRAME_BEGIN:
        if (g_Player.is_hit) {
            g_Stage.timer_initialized = false;
            GameplayExit_Begin();
            return true;
        }
        g_Session.frame_rand++;
        {
            const int32_t period =
                1800 - g_Session.lives * 200 - g_Player.remaining_bombs * 50;
            if (period != 0 && g_Session.frame_rand % (uint32_t)period == 0) {
                Pellets_SpeedRaise(1);
            }
        }
        Gameplay_Sense(raw);
        if (g_Player.invincibility_time > 1) {
            g_Player.invincibility_time--;
            g_Player.invincible = true;
        } else if (g_Player.invincibility_time == 1) {
            g_Player.invincible = false;
            g_Player.invincibility_time = 0;
        }
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
        g_Player.bomb_frame++;
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
        if (g_GameInput.paused) {
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
                g_Stage.score += g_Pellets.destroy_score_delta;
                Hud_ScoreAndCombo();
                g_Pellets.destroy_score_delta = 0;
            }
            m_LifePhase = GP_LIFE_FRAME_BEGIN;
        }
        return true;
    }
    return true;
}

void Gameplay_Init(
    const ALLOCATOR *const allocator, const GAMEPLAY_ASSETS *const assets)
{
    g_GameplayAssets = assets;
    memset(&g_Gameplay, 0, sizeof(g_Gameplay));
    g_Stage.frame = 0;
    g_Stage.boss = BOSS_NONE;
    g_Gameplay.wait_for_shot = true;
    GameplayStage_Begin();
    StageObjects_Prepare(allocator);
    Hud_Init(allocator);
    HudHp_Init(allocator);
    Tram_Init(allocator);
    Palette_Init(&g_Palette);
    Hud_Load(&assets->hud, Gameplay_StageSprites(), &assets->hud_font);
}

void Gameplay_Reset(void)
{
    memset(&g_Gameplay, 0, sizeof(g_Gameplay));
    g_Stage.frame = 0;
    g_Stage.boss = BOSS_NONE;
    g_Gameplay.wait_for_shot = true;
    GameplayStage_Begin();
    Palette_Init(&g_Palette);
    memset(&g_StagePalette, 0, sizeof(g_StagePalette));
    Boss_Init();
    Player_Init();
    Orb_Init();
    Shots_Init();
    Pellets_Init();
    memset(&g_StageObjects, 0, sizeof(g_StageObjects));
    memset(&g_Stage, 0, sizeof(g_Stage));
    g_Stage.first_stage = true;
    StageOrb_Init();
    Items_Init();
    Missiles_Init();
    ShootoutLasers_Init();
    Particles_Reset();
    Bomb_Init();
    Miss_Init();
    Scene_Init();
    Bonus_Init();
    Hud_Reset();
    HudHp_Reset();
    VSyncIrq_Reset();
    GameInput_Init();
    StageTimer_Init();
    VSyncIrq_SwitchProgram();
    Tram_Clear();
    Tram_SetVisible(true);
    BossEntity_Init();
    for (BOSS boss = BOSS_SINGYOKU; boss < BOSS_COUNT; boss++) {
        BossStrategy_Get(boss)->init();
    }
    g_Session.high_score = g_Session.table_high_score;
    g_Stage.score = g_Session.score;
    g_Player.remaining_bombs = g_Session.bombs;
    g_Player.left = 304;
    g_Pellets.speed = g_Session.pellet_speed;
    Extend_SyncResidentLives(g_Session.lives);
    Extend_ResetNext(g_Session.score);
    g_Rng = g_Session.frame_rand;
    g_Stage.flip_cycle = (uint8_t)(MasterRng_Next(&g_Rng) % 60);
    g_Gameplay.boss_stage = g_Session.stage_id % 5 == 4;
    Scene_Activate(&g_GameplayAssets->scenes[Session_Scene()]);
    Video_SelectDrawingPage(0);
    Video_SelectVisiblePage(0);
    Video_SetScroll(0);
    Video_Clear(0);
    Hud_Load(
        &g_GameplayAssets->hud, Gameplay_StageSprites(),
        &g_GameplayAssets->hud_font);
}

void Gameplay_ResumeAfterScore(
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    ASSERT(g_Gameplay.request == GAMEPLAY_REQUEST_SCORE);
    g_Gameplay.request = GAMEPLAY_REQUEST_NONE;
    GameplayEnd_BeginContinue();
    M_Run(raw, host_tick);
}

void Gameplay_Begin(const TH01_INPUT *const raw, const uint64_t host_tick)
{
    Gameplay_Reset();
    Sound_Load(11);
    Sound_Play();
    M_Run(raw, host_tick);
}

GAMEPLAY_REQUEST Gameplay_Tick(
    const TH01_INPUT *const raw, const bool reference_edge,
    const uint64_t host_tick)
{
    if (reference_edge && GameplayEnd_Switching()) {
        VSyncIrq_PollEdge();
    } else if (reference_edge) {
        VSyncIrq_Edge();
    }
    return M_Run(raw, host_tick);
}

void Gameplay_Close(void)
{
    memset(&g_Gameplay, 0, sizeof(g_Gameplay));
    g_Stage.frame = 0;
    g_Stage.boss = BOSS_NONE;
    g_GameplayAssets = nullptr;
}
