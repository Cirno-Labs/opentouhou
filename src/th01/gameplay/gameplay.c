#include "th01/gameplay/gameplay.h"
#include "th01/gameplay/private.h"
#include "th01/boss/entity.h"
#include "th01/boss/strategy.h"
#include "th01/hud_hp.h"
#include "th01/missiles.h"
#include "th01/particles.h"
#include "th01/stage_objects.h"
#include "th01/tram.h"
#include "th01/items.h"
#include "th01/miss.h"
#include "th01/sound.h"
#include "th01/stage.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"
#include "core/base/assert.h"

#include <string.h>

GAMEPLAY g_Gameplay;
const GAMEPLAY_ASSETS *g_GameplayAssets;

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
    Session_Store(g_Stage.score, g_Player->remaining_bombs, g_Pellets->speed);
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
        Player_Hit();
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

void Gameplay_Init(
    const ALLOCATOR *const allocator, const GAMEPLAY_ASSETS *const assets)
{
    g_GameplayAssets = assets;
    memset(&g_Gameplay, 0, sizeof(g_Gameplay));
    Stage_Leave();
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
    Stage_Leave();
    g_Gameplay.wait_for_shot = true;
    GameplayStage_Begin();
    Palette_Init(&g_Palette);
    memset(&g_StagePalette, 0, sizeof(g_StagePalette));
    Boss_Init();
    Player_Init();
    Orb_Init();
    Shots_Init();
    Pellets_Init();
    StageObjects_Clear();
    Stage_Init();
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
    Player_SetBombs(g_Session.bombs);
    Player_Center();
    Pellets_SetSpeed(g_Session.pellet_speed);
    Extend_SyncResidentLives(g_Session.lives);
    Extend_ResetNext(g_Session.score);
    Rng_Seed(g_Session.frame_rand);
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
    Stage_Leave();
    g_GameplayAssets = nullptr;
}
