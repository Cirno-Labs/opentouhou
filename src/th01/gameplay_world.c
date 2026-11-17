#include "th01/gameplay_internal.h"

#include "core/base/assert.h"

#include <string.h>

RESULT Gameplay_Prepare(
    const ALLOCATOR *const allocator, const GAMEPLAY_ASSETS *const assets,
    VIDEO *const video, SESSION *const session, GAMEPLAY **const out)
{
    *out = nullptr;
    GAMEPLAY *const g = Allocator_AllocZ(allocator, sizeof(*g));
    g->assets = assets;
    g->video = video;
    g->session = session;
    StageObjects_Prepare(&g->storage, allocator);
    Hud_Init(&g->hud, allocator);
    HudHp_Init(&g->health, allocator);
    Tram_Init(&g->tram, allocator);
    B10J_Init(&g->b10j, allocator);
    B15M_Init(&g->b15m, allocator);
    B20M_Init(&g->b20m, allocator);
    MUST(MdrvSynth_Init(
        assets->songs, GAMEPLAY_SONG_COUNT, &assets->effects, allocator,
        nullptr, nullptr, &g->audio));
    Gameplay_Bind(g);
    MUST(B05_ValidateContext(&g->c05));
    MUST(B10J_ValidateContext(&g->c10j));
    MUST(B10M_ValidateContext(&g->c10m));
    MUST(B15J_ValidateContext(&g->c15j));
    MUST(B15M_ValidateContext(&g->c15m));
    MUST(B20J_ValidateContext(&g->c20j));
    MUST(B20M_ValidateContext(&g->c20m));
    *out = g;
    return OK;
}

void Gameplay_ResetWorld(GAMEPLAY *const g)
{
    const HUD hud = g->hud;
    const HUD_HP health = g->health;
    *g = (GAMEPLAY) {
        .assets = g->assets,
        .video = g->video,
        .session = g->session,
        .audio = g->audio,
        .song = g->song,
        .host_tick = g->host_tick,
        .storage = g->storage,
        .tram = g->tram,
        .hud = { .lives = hud.lives,
                 .bombs = hud.bombs,
                 .numbers = hud.numbers,
                 .stage = hud.stage,
                 .timer = hud.timer },
        .health = { .backgrounds = health.backgrounds },
        .b10j = { .background = g->b10j.background,
                  .meteor_active = true,
                  .spread_interval = 4,
                  .spread_speed = 8 },
        .b15m = { .background = g->b15m.background },
        .b20m = { .wand_background = g->b20m.wand_background,
                  .ray_previous_x = 999,
                  .ray_previous_y = 999,
                  .flake_state = -1 },
        .first_stage = true,
        .wait_for_shot = true,
        .phase = GP_STAGE_BEGIN,
    };
    B20J_Init(&g->b20j);
    Palette_Init(&g->palette);
    Frame_SwitchProgram(&g->vsync);
    Tram_Clear(&g->tram);
    Tram_SetVisible(&g->tram, true);
    g->session->high_score = g->session->table_high_score;
    g->metrics.score = g->session->score;
    g->player.remaining_bombs = g->session->bombs;
    g->player.left = 304;
    g->pellet_shared.speed = g->session->pellet_speed;
    g->retained_lives = (int8_t)g->session->lives;
    g->next_extend = Extend_NextForScore(g->session->score);
    g->rng = g->session->frame_rand;
    g->metrics.flip_cycle = (uint8_t)(Math_Random(&g->rng) % 60);
    g->boss_stage = g->session->stage_id % 5 == 4;
    Scene_Activate(&g->scene, &g->assets->scenes[Session_Scene(g->session)]);
    Video_SelectDrawingPage(g->video, 0);
    Video_SelectVisiblePage(g->video, 0);
    Video_SetScroll(g->video, 0);
    Video_Clear(g->video, 0);
    Gameplay_Bind(g);
}

void Gameplay_ResetInput(GAMEPLAY *const g)
{
    GameInput_ResetGame(&g->input);
}

void Gameplay_DefaultActors(GAMEPLAY *const g)
{
    Sprite_PtnPut(
        g->video, &g->assets->miko, 0, -1, g->player.left, PLAYER_TOP);
    Orb_RenderAction(
        g->video, g->stage.sprites,
        (ORB_ACTION) {
            .kind = ORB_ACTION_PUT,
            .left = g->orb.left,
            .top = g->orb.top,
            .image = 0,
        });
}

void Gameplay_SyncSession(GAMEPLAY *const g)
{
    g->session->score = g->metrics.score;
    g->session->bombs = g->player.remaining_bombs;
    g->session->pellet_speed = g->pellet_shared.speed;
}

void Gameplay_BossSetup(GAMEPLAY *const g)
{
    switch (g->boss) {
    case 1:
        B05_Setup(&g->b05, &g->c05);
        break;
    case 2:
        B10M_Setup(&g->b10m, &g->c10m);
        break;
    case 3:
        B10J_BeginSetup(&g->b10j, &g->c10j, &g->vsync);
        break;
    case 4:
        B15J_Setup(&g->b15j, &g->c15j);
        break;
    case 5:
        B15M_Setup(&g->b15m, &g->c15m);
        break;
    case 6:
        B20M_Setup(&g->b20m, &g->c20m);
        break;
    case 7:
        B20J_Setup(&g->b20j, &g->c20j);
        break;
    default:
        break;
    }
}

bool Gameplay_BossTick(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    switch (g->boss) {
    case 1:
        return B05_UpdateAndRender(&g->b05, &g->c05, raw, &g->vsync);
    case 2:
        return B10M_UpdateAndRender(&g->b10m, &g->c10m, &g->vsync);
    case 3:
        return B10J_UpdateAndRender(&g->b10j, &g->c10j, &g->vsync);
    case 4:
        return B15J_UpdateAndRender(&g->b15j, &g->c15j, &g->vsync);
    case 5:
        return B15M_UpdateAndRender(
            &g->b15m, &g->c15m, &g->vsync, g->host_tick);
    case 6:
        return B20M_UpdateAndRender(
            &g->b20m, &g->c20m, &g->vsync, g->host_tick);
    case 7:
        return B20J_UpdateAndRender(&g->b20j, &g->c20j, &g->vsync);
    default:
        return true;
    }
}

const TRAM *Gameplay_TextLayer(const GAMEPLAY *const g)
{
    return &g->tram;
}

void Gameplay_AdvanceAudio(GAMEPLAY *const g)
{
    ASSERT(Result_Ignore(MdrvSynth_Render(g->audio, nullptr, 800)));
}

void Gameplay_OpeningAudio(GAMEPLAY *const g, const OPENING_EVENTS events)
{
    switch (events.audio) {
    case OPENING_AUDIO_NONE:
        break;
    case OPENING_AUDIO_STOP:
        MdrvSynth_Stop(g->audio);
        break;
    case OPENING_AUDIO_FADE:
        MdrvSynth_Fade(g->audio, 1);
        break;
    case OPENING_AUDIO_PLAY: {
        static const uint8_t songs[OPENING_TRACK_COUNT] = {
            15, 16, 0, 1, 2, 3, 4, 5, 7, 8, 9, 13, 14, 12, 17,
        };
        ASSERT(events.track >= 0 && events.track < OPENING_TRACK_COUNT);
        g->song = songs[events.track];
        ASSERT(Result_Ignore(MdrvSynth_Play(g->audio, g->song)));
        break;
    }
    }
}

GAMEPLAY_SCORE_CONTEXT Gameplay_ScoreContext(GAMEPLAY *const g)
{
    return (GAMEPLAY_SCORE_CONTEXT) { .vsync = &g->vsync, .input = &g->input };
}
