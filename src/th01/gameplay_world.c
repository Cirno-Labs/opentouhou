#include "th01/gameplay_internal.h"

#include "core/base/assert.h"

#include <string.h>

RESULT Gameplay_Prepare(
    const ALLOCATOR *const allocator, const GAMEPLAY_ASSETS *const assets,
    VIDEO *const video, SESSION *const session, GAMEPLAY **const out)
{
    *out = nullptr;
    GAMEPLAY *const g = Allocator_AllocZ(allocator, sizeof(*g));
    g->storage.assets = assets;
    g->storage.video = video;
    g->storage.session = session;
    StageObjects_Prepare(&g->storage.objects, allocator);
    Hud_Init(&g->world.hud, allocator);
    HudHp_Init(&g->world.health, allocator);
    Tram_Init(&g->world.tram, allocator);
    B10J_Init(&g->world.b10j, allocator);
    B15M_Init(&g->world.b15m, allocator);
    B20M_Init(&g->world.b20m, allocator);
    MUST(MdrvSynth_Init(
        assets->songs, GAMEPLAY_SONG_COUNT, &assets->effects, allocator,
        nullptr, nullptr, &g->storage.audio));
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
    MUST(B05_ValidateContext(&g->bindings.c05));
    MUST(B10J_ValidateContext(&g->bindings.c10j));
    MUST(B10M_ValidateContext(&g->bindings.c10m));
    MUST(B15J_ValidateContext(&g->bindings.c15j));
    MUST(B15M_ValidateContext(&g->bindings.c15m));
    MUST(B20J_ValidateContext(&g->bindings.c20j));
    MUST(B20M_ValidateContext(&g->bindings.c20m));
    *out = g;
    return OK;
}

void Gameplay_ResetWorld(GAMEPLAY *const g)
{
    memcpy(&g->world, g->storage.initial, sizeof(g->world));
    Frame_SwitchProgram(&g->world.vsync);
    Tram_Clear(&g->world.tram);
    Tram_SetVisible(&g->world.tram, true);
    g->storage.session->high_score = g->storage.session->table_high_score;
    g->world.metrics.score = g->storage.session->score;
    g->world.player.remaining_bombs = g->storage.session->bombs;
    g->world.player.left = 304;
    g->world.pellet_shared.speed = g->storage.session->pellet_speed;
    g->world.retained_lives = (int8_t)g->storage.session->lives;
    g->world.next_extend = Extend_NextForScore(g->storage.session->score);
    g->world.rng = g->storage.session->frame_rand;
    g->world.metrics.flip_cycle = (uint8_t)(Math_Random(&g->world.rng) % 60);
    g->world.boss_stage = g->storage.session->stage_id % 5 == 4;
    Scene_Activate(
        &g->world.scene,
        &g->storage.assets->scenes[Session_Scene(g->storage.session)]);
    Video_SelectDrawingPage(g->storage.video, 0);
    Video_SelectVisiblePage(g->storage.video, 0);
    Video_SetScroll(g->storage.video, 0);
    Video_Clear(g->storage.video, 0);
    Gameplay_Bind(g);
}

void Gameplay_ResetInput(GAMEPLAY *const g)
{
    GameInput_ResetGame(&g->world.input);
}

void Gameplay_DefaultActors(GAMEPLAY *const g)
{
    Sprite_PtnPut(
        g->storage.video, &g->storage.assets->miko, 0, -1, g->world.player.left,
        PLAYER_TOP);
    Orb_RenderAction(
        g->storage.video, g->bindings.stage.sprites,
        (ORB_ACTION) {
            .kind = ORB_ACTION_PUT,
            .left = g->world.orb.left,
            .top = g->world.orb.top,
            .image = 0,
        });
}

void Gameplay_SyncSession(GAMEPLAY *const g)
{
    g->storage.session->score = g->world.metrics.score;
    g->storage.session->bombs = g->world.player.remaining_bombs;
    g->storage.session->pellet_speed = g->world.pellet_shared.speed;
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
        B10J_BeginSetup(&g->world.b10j, &g->bindings.c10j, &g->world.vsync);
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
}

bool Gameplay_BossTick(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    switch (g->world.boss) {
    case 1:
        return B05_UpdateAndRender(
            &g->world.b05, &g->bindings.c05, raw, &g->world.vsync);
    case 2:
        return B10M_UpdateAndRender(
            &g->world.b10m, &g->bindings.c10m, &g->world.vsync);
    case 3:
        return B10J_UpdateAndRender(
            &g->world.b10j, &g->bindings.c10j, &g->world.vsync);
    case 4:
        return B15J_UpdateAndRender(
            &g->world.b15j, &g->bindings.c15j, &g->world.vsync);
    case 5:
        return B15M_UpdateAndRender(
            &g->world.b15m, &g->bindings.c15m, &g->world.vsync,
            g->storage.host_tick);
    case 6:
        return B20M_UpdateAndRender(
            &g->world.b20m, &g->bindings.c20m, &g->world.vsync,
            g->storage.host_tick);
    case 7:
        return B20J_UpdateAndRender(
            &g->world.b20j, &g->bindings.c20j, &g->world.vsync);
    default:
        return true;
    }
}

const TRAM *Gameplay_TextLayer(const GAMEPLAY *const g)
{
    return &g->world.tram;
}

void Gameplay_AdvanceAudio(GAMEPLAY *const g)
{
    Gameplay_RequireAudio(
        GameAudio_Render(g->storage.audio_output, g->storage.audio, 800));
}

void Gameplay_OpeningAudio(GAMEPLAY *const g, const OPENING_EVENTS events)
{
    switch (events.audio) {
    case OPENING_AUDIO_NONE:
        break;
    case OPENING_AUDIO_STOP:
        MdrvSynth_Stop(g->storage.audio);
        break;
    case OPENING_AUDIO_FADE:
        MdrvSynth_Fade(g->storage.audio, 1);
        break;
    case OPENING_AUDIO_PLAY: {
        static const uint8_t songs[OPENING_TRACK_COUNT] = {
            15, 16, 0, 1, 2, 3, 4, 5, 7, 8, 9, 13, 14, 12, 17,
        };
        ASSERT(events.track >= 0 && events.track < OPENING_TRACK_COUNT);
        g->storage.song = songs[events.track];
        Gameplay_RequireAudio(
            MdrvSynth_Play(g->storage.audio, g->storage.song));
        break;
    }
    }
}

GAMEPLAY_SCORE_CONTEXT Gameplay_ScoreContext(GAMEPLAY *const g)
{
    return (GAMEPLAY_SCORE_CONTEXT) { .vsync = &g->world.vsync,
                                      .input = &g->world.input };
}

void Gameplay_BindAudio(GAMEPLAY *const g, GAME_AUDIO *const output)
{
    g->storage.audio_output = output;
}

void Gameplay_RequireAudio(const RESULT result)
{
    if (!IS_OK(result)) {
        Result_Log(Result_Prefix(result, "TH01 audio"), LOG_LEVEL_ERROR);
        LOG_FATAL("TH01 audio playback failed");
    }
}
