#include "th01/gameplay_internal.h"

#include "pc98/zun/master_rng.h"
#include "th01/hooks.h"

#include "core/base/assert.h"

#include <string.h>

TH01_HOOKS g_Th01Hooks;

RESULT Gameplay_Prepare(
    const ALLOCATOR *const allocator, const GAMEPLAY_ASSETS *const assets,
    GAMEPLAY **const out)
{
    *out = nullptr;
    GAMEPLAY *const g = Allocator_AllocZ(allocator, sizeof(*g));
    g->storage.assets = assets;
    StageObjects_Prepare(&g->storage.objects, allocator);
    Hud_Init(&g->world.hud, allocator);
    HudHp_Init(&g->world.health, allocator);
    Tram_Init(allocator);
    B10J_Init(&g->world.b10j, allocator);
    B15M_Init(&g->world.b15m, allocator);
    B20M_Init(&g->world.b20m, allocator);
    MUST(Mdrv_Init(
        assets->songs, GAMEPLAY_SONG_COUNT, &assets->effects, 8, 8, allocator,
        &g->storage.audio));
    B20J_Init(&g->world.b20j);
    Palette_Init(&g->world.palette);
    g->world.first_stage = true;
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
    Frame_Reset();
    GameInput_Init();
    StageTimer_Init();
    Frame_SwitchProgram();
    Tram_Clear();
    Tram_SetVisible(true);
    g_Session.high_score = g_Session.table_high_score;
    g->world.metrics.score = g_Session.score;
    g->world.player.remaining_bombs = g_Session.bombs;
    g->world.player.left = 304;
    g->world.pellet_shared.speed = g_Session.pellet_speed;
    g->world.retained_lives = (int8_t)g_Session.lives;
    g->world.next_extend = Extend_NextForScore(g_Session.score);
    g->world.rng = g_Session.frame_rand;
    g->world.metrics.flip_cycle = (uint8_t)(MasterRng_Next(&g->world.rng) % 60);
    g->world.boss_stage = g_Session.stage_id % 5 == 4;
    Scene_Activate(
        &g->world.scene, &g->storage.assets->scenes[Session_Scene()]);
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
    Sprite_PtnPut(
        &g->storage.assets->miko, 0, -1, g->world.player.left, PLAYER_TOP);
    Orb_RenderAction(
        g->bindings.stage.sprites,
        (ORB_ACTION) {
            .kind = ORB_ACTION_PUT,
            .left = g->world.orb.left,
            .top = g->world.orb.top,
            .image = 0,
        });
}

void Gameplay_SyncSession(GAMEPLAY *const g)
{
    g_Session.score = g->world.metrics.score;
    g_Session.bombs = g->world.player.remaining_bombs;
    g_Session.pellet_speed = g->world.pellet_shared.speed;
}

void Gameplay_BossSetup(GAMEPLAY *const g)
{
    switch (g->world.boss) {
    case 1:
        B05_Setup(&g->world.b05, &g->bindings.c05);
        break;
    case 2:
        B10M_Setup(&g->world.b10m, &g->bindings.c10m);
        break;
    case 3:
        B10J_BeginSetup(&g->world.b10j, &g->bindings.c10j);
        break;
    case 4:
        B15J_Setup(&g->world.b15j, &g->bindings.c15j);
        break;
    case 5:
        B15M_Setup(&g->world.b15m, &g->bindings.c15m);
        break;
    case 6:
        B20M_Setup(&g->world.b20m, &g->bindings.c20m);
        break;
    case 7:
        B20J_Setup(&g->world.b20j, &g->bindings.c20j);
        break;
    default:
        break;
    }
    if (g->world.boss != 0 && g->world.boss != 3) {
        Gameplay_HookBossHp(g);
    }
}

void Gameplay_HookBossHp(GAMEPLAY *const g)
{
    if (g_Th01Hooks.on_boss == nullptr) {
        return;
    }
    int16_t *const hp[] = {
        nullptr,           &g->world.b05.hp,  &g->world.b10m.hp,
        &g->world.b10j.hp, &g->world.b15j.hp, &g->world.b15m.hp,
        &g->world.b20m.hp, &g->world.b20j.hp,
    };
    g_Th01Hooks.on_boss(g_Th01Hooks.arg, hp[g->world.boss]);
}

bool Gameplay_BossTick(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    switch (g->world.boss) {
    case 1:
        return B05_UpdateAndRender(&g->world.b05, &g->bindings.c05, raw);
    case 2:
        return B10M_UpdateAndRender(&g->world.b10m, &g->bindings.c10m);
    case 3:
        return B10J_UpdateAndRender(&g->world.b10j, &g->bindings.c10j);
    case 4:
        return B15J_UpdateAndRender(&g->world.b15j, &g->bindings.c15j);
    case 5:
        return B15M_UpdateAndRender(
            &g->world.b15m, &g->bindings.c15m, g->storage.host_tick);
    case 6:
        return B20M_UpdateAndRender(
            &g->world.b20m, &g->bindings.c20m, g->storage.host_tick);
    case 7:
        return B20J_UpdateAndRender(&g->world.b20j, &g->bindings.c20j);
    default:
        return true;
    }
}

void Gameplay_AdvanceAudio(GAMEPLAY *const g)
{
    GameAudio_Render(g->storage.audio_output, g->storage.audio, 800);
}

void Gameplay_OpeningAudio(GAMEPLAY *const g, const OPENING_EVENTS events)
{
    switch (events.audio) {
    case OPENING_AUDIO_NONE:
        break;
    case OPENING_AUDIO_STOP:
        Mdrv_Stop(g->storage.audio);
        break;
    case OPENING_AUDIO_FADE:
        Mdrv_Fade(g->storage.audio, 1);
        break;
    case OPENING_AUDIO_PLAY: {
        static const uint8_t songs[OPENING_TRACK_COUNT] = {
            15, 16, 0, 1, 2, 3, 4, 5, 7, 8, 9, 13, 14, 12, 17,
        };
        ASSERT(events.track >= 0 && events.track < OPENING_TRACK_COUNT);
        g->storage.song = songs[events.track];
        Mdrv_Play(g->storage.audio, g->storage.song);
        break;
    }
    }
}

void Gameplay_BindAudio(GAMEPLAY *const g, GAME_AUDIO *const output)
{
    g->storage.audio_output = output;
}
