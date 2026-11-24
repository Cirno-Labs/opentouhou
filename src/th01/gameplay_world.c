#include "th01/gameplay_internal.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"
#include "th01/hooks.h"

#include "core/base/assert.h"

#include <string.h>

TH01_HOOKS g_Th01Hooks;

static bool m_FirstStage = true;
static bool m_TimerInitialized;
static bool m_GameCleared;

bool Gameplay_GameCleared(void)
{
    return m_GameCleared;
}

void Gameplay_SetGameCleared(void)
{
    m_GameCleared = true;
}

bool Gameplay_FirstStage(void)
{
    return m_FirstStage;
}

void Gameplay_SetFirstStage(const bool first_stage)
{
    m_FirstStage = first_stage;
}

bool Gameplay_TimerInitialized(void)
{
    return m_TimerInitialized;
}

void Gameplay_SetTimerInitialized(const bool timer_initialized)
{
    m_TimerInitialized = timer_initialized;
}

void Gameplay_ResetLiveFlags(void)
{
    m_FirstStage = true;
    m_TimerInitialized = false;
    m_GameCleared = false;
}

RESULT Gameplay_Prepare(
    const ALLOCATOR *const allocator, const GAMEPLAY_ASSETS *const assets,
    GAMEPLAY **const out)
{
    *out = nullptr;
    GAMEPLAY *const g = Allocator_AllocZ(allocator, sizeof(*g));
    g->storage.assets = assets;
    StageObjects_Prepare(allocator);
    Hud_Init(allocator);
    HudHp_Init(allocator);
    Tram_Init(allocator);
    MUST(Mdrv_Init(
        assets->songs, GAMEPLAY_SONG_COUNT, &assets->effects, 8, 8, allocator,
        &g_Sound.mdrv));
    Palette_Init(&g_Palette);
    g->world.wait_for_shot = true;
    g->world.phase = GP_STAGE_BEGIN;
    GAMEPLAY_WORLD *const initial =
        Allocator_Alloc(allocator, sizeof(*initial));
    memcpy(initial, &g->world, sizeof(*initial));
    g->storage.initial = initial;
    Gameplay_Bind(g);
    *out = g;
    return OK;
}

void Gameplay_ResetWorld(GAMEPLAY *const g)
{
    memcpy(&g->world, g->storage.initial, sizeof(g->world));
    Gameplay_ResetLiveFlags();
    Palette_Init(&g_Palette);
    memset(&g_StagePalette, 0, sizeof(g_StagePalette));
    g_TestDamage = false;
    Player_Init();
    Orb_Init();
    Shots_Init();
    Pellets_Init();
    memset(&g_StageObjects, 0, sizeof(g_StageObjects));
    memset(&g_Stage, 0, sizeof(g_Stage));
    memset(&g_OrbComposite, 0, sizeof(g_OrbComposite));
    memset(&g_Items, 0, sizeof(g_Items));
    memset(&g_Missiles, 0, sizeof(g_Missiles));
    memset(&g_ShootoutLasers, 0, sizeof(g_ShootoutLasers));
    memset(&g_Particles, 0, sizeof(g_Particles));
    memset(&g_Bomb, 0, sizeof(g_Bomb));
    memset(&g_Miss, 0, sizeof(g_Miss));
    memset(&g_Scene, 0, sizeof(g_Scene));
    memset(&g_Bonus, 0, sizeof(g_Bonus));
    memset(&g_BonusTotal, 0, sizeof(g_BonusTotal));
    Hud_Reset();
    HudHp_Reset();
    Frame_Reset();
    GameInput_Init();
    StageTimer_Init();
    Frame_SwitchProgram();
    Tram_Clear();
    Tram_SetVisible(true);
    memset(&g_BossSprites, 0, sizeof(g_BossSprites));
    B05_Init();
    B10J_Init();
    B10M_Init();
    B15J_Init();
    B15M_Init();
    B20J_Init();
    B20M_Init();
    g_Session.high_score = g_Session.table_high_score;
    g_Stage.score = g_Session.score;
    g_Player.remaining_bombs = g_Session.bombs;
    g_Player.left = 304;
    g_PelletShared.speed = g_Session.pellet_speed;
    g_Extend.resident_lives = (int8_t)g_Session.lives;
    g_Extend.next = Extend_NextForScore(g_Session.score);
    g_Rng = g_Session.frame_rand;
    g_Stage.flip_cycle = (uint8_t)(MasterRng_Next(&g_Rng) % 60);
    g->world.boss_stage = g_Session.stage_id % 5 == 4;
    Scene_Activate(&g->storage.assets->scenes[Session_Scene()]);
    Video_SelectDrawingPage(0);
    Video_SelectVisiblePage(0);
    Video_SetScroll(0);
    Video_Clear(0);
    Gameplay_Bind(g);
}

void Gameplay_ResetInput(GAMEPLAY *const g)
{
    GameInput_ResetGame();
}

void Gameplay_DefaultActors(GAMEPLAY *const g)
{
    Sprite_PtnPut(&g->storage.assets->miko, 0, -1, g_Player.left, PLAYER_TOP);
    Orb_RenderAction(
        g->bindings.stage.sprites,
        (ORB_ACTION) {
            .kind = ORB_ACTION_PUT,
            .left = g_Orb.left,
            .top = g_Orb.top,
            .image = 0,
        });
}

void Gameplay_SyncSession(GAMEPLAY *const g)
{
    g_Session.score = g_Stage.score;
    g_Session.bombs = g_Player.remaining_bombs;
    g_Session.pellet_speed = g_PelletShared.speed;
}

void Gameplay_BossSetup(GAMEPLAY *const g)
{
    switch (g->world.boss) {
    case BOSS_SINGYOKU:
        B05_Setup(&g->storage.assets->b05);
        break;
    case BOSS_YUUGENMAGAN:
        B10M_Setup(
            &g->storage.assets->b10m, g->bindings.shots_output,
            &g->storage.assets->miko);
        break;
    case BOSS_MIMA:
        B10J_BeginSetup(
            &g->storage.assets->b10j, g->bindings.shots_output,
            &g->storage.assets->miko);
        break;
    case BOSS_KIKURI:
        B15J_Setup(
            &g->storage.assets->b15j, g->bindings.shots_output,
            &g->storage.assets->miko);
        break;
    case BOSS_ELIS:
        B15M_Setup(
            &g->storage.assets->b15m, g->bindings.shots_output,
            &g->storage.assets->miko);
        break;
    case BOSS_SARIEL:
        B20M_Setup(&g->storage.assets->b20m, g->bindings.shots_output);
        break;
    case BOSS_KONNGARA:
        B20J_Setup(&g->storage.assets->b20j);
        break;
    default:
        break;
    }
    if (g->world.boss != BOSS_NONE && g->world.boss != BOSS_MIMA) {
        Gameplay_HookBossHp(g);
    }
}

void Gameplay_HookBossHp(GAMEPLAY *const g)
{
    if (g_Th01Hooks.on_boss == nullptr) {
        return;
    }
    int16_t *const hp[] = {
        [BOSS_NONE] = nullptr,           [BOSS_SINGYOKU] = &g_B05.hp,
        [BOSS_YUUGENMAGAN] = &g_B10M.hp, [BOSS_MIMA] = &g_B10J.hp,
        [BOSS_KIKURI] = &g_B15J.hp,      [BOSS_ELIS] = &g_B15M.hp,
        [BOSS_SARIEL] = &g_B20M.hp,      [BOSS_KONNGARA] = &g_B20J.hp,
    };
    g_Th01Hooks.on_boss(g_Th01Hooks.arg, hp[g->world.boss]);
}

bool Gameplay_BossTick(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    switch (g->world.boss) {
    case BOSS_SINGYOKU:
        return B05_UpdateAndRender(
            g->bindings.shots_output, &g->storage.assets->miko,
            &g->bindings.route_assets, raw);
    case BOSS_YUUGENMAGAN:
        return B10M_UpdateAndRender(
            &g->storage.assets->b10m, g->bindings.shots_output,
            &g->storage.assets->miko);
    case BOSS_MIMA:
        return B10J_UpdateAndRender(
            &g->storage.assets->b10j, g->bindings.shots_output,
            &g->storage.assets->miko);
    case BOSS_KIKURI:
        return B15J_UpdateAndRender(
            &g->storage.assets->b15j, g->bindings.shots_output,
            &g->storage.assets->miko);
    case BOSS_ELIS:
        return B15M_UpdateAndRender(
            &g->storage.assets->b15m, g->bindings.shots_output,
            &g->storage.assets->miko, g->storage.host_tick);
    case BOSS_SARIEL:
        return B20M_UpdateAndRender(
            &g->storage.assets->b20m, g->bindings.shots_output,
            g->storage.host_tick);
    case BOSS_KONNGARA:
        return B20J_UpdateAndRender(
            &g->storage.assets->b20j, g->bindings.shots_output);
    default:
        return true;
    }
}

void Gameplay_AdvanceAudio(GAMEPLAY *const g)
{
    GameAudio_Render(g->storage.audio_output, g_Sound.mdrv, 800);
}

void Gameplay_OpeningAudio(GAMEPLAY *const g, const OPENING_EVENTS events)
{
    switch (events.audio) {
    case OPENING_AUDIO_NONE:
        break;
    case OPENING_AUDIO_STOP:
        Mdrv_Stop(g_Sound.mdrv);
        break;
    case OPENING_AUDIO_FADE:
        Mdrv_Fade(g_Sound.mdrv, 1);
        break;
    case OPENING_AUDIO_PLAY: {
        static const uint8_t songs[OPENING_TRACK_COUNT] = {
            15, 16, 0, 1, 2, 3, 4, 5, 7, 8, 9, 13, 14, 12, 17,
        };
        ASSERT(events.track >= 0 && events.track < OPENING_TRACK_COUNT);
        g_Sound.song = songs[events.track];
        Mdrv_Play(g_Sound.mdrv, g_Sound.song);
        break;
    }
    }
}

void Gameplay_BindAudio(GAMEPLAY *const g, GAME_AUDIO *const output)
{
    g->storage.audio_output = output;
}
