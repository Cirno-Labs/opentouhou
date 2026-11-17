#include "th01/gameplay_internal.h"

#include "core/base/assert.h"

void Gameplay_Effect(void *const user, const int16_t number)
{
    GAMEPLAY *const g = user;
    if (g->session->settings.music != SETTINGS_MUSIC_OFF) {
        ASSERT(Result_Ignore(MdrvSynth_EffectPlay(g->audio, (uint16_t)number)));
    }
}

void Gameplay_Play(GAMEPLAY *const g, const size_t song)
{
    g->song = song;
    if (g->session->settings.music != SETTINGS_MUSIC_OFF) {
        ASSERT(Result_Ignore(MdrvSynth_Play(g->audio, song)));
    }
}

void Gameplay_Fade(void *const user)
{
    GAMEPLAY *const g = user;
    if (g->session->settings.music != SETTINGS_MUSIC_OFF) {
        MdrvSynth_Fade(g->audio, 1);
    }
}

void Gameplay_Stop(GAMEPLAY *const g)
{
    if (g->session->settings.music != SETTINGS_MUSIC_OFF) {
        MdrvSynth_Stop(g->audio);
    }
}

static void M_HudScore(void *const user)
{
    GAMEPLAY *const g = user;
    Hud_ScoreAndCombo(&g->hud, &g->display);
}

static void M_HudBombs(void *const user, const int16_t previous)
{
    GAMEPLAY *const g = user;
    Hud_BombsPut(&g->hud, &g->display, previous);
}

static void M_HudLives(void *const user, const int16_t previous)
{
    GAMEPLAY *const g = user;
    Hud_LivesPut(&g->hud, &g->display, previous);
}

static void M_Decay(void *const user)
{
    GAMEPLAY *const g = user;
    Pellets_Decay(&g->pellets, &g->pellet_shared);
}

static void M_TimerExtend(void *const user)
{
    GAMEPLAY *const g = user;
    Timer_ExtendAndPut(&g->timer, &g->timing);
}

static void M_SpeedLower(
    void *const user, const float minimum, const float delta)
{
    GAMEPLAY *const g = user;
    Pellets_SpeedLower(
        &g->pellet_shared, (int16_t)(minimum * 40), (int16_t)(delta * 40));
}

static void M_Shots(void *const user, const SHOTS_ACTION action)
{
    GAMEPLAY *const g = user;
    if (action.kind == SHOTS_ACTION_EFFECT) {
        Gameplay_Effect(g, action.effect);
    } else {
        Shots_RenderAction(g->video, g->stage.sprites, action);
    }
}

static void M_Player(void *const user, const PLAYER_ACTION action)
{
    GAMEPLAY *const g = user;
    switch (action.kind) {
    case PLAYER_ACTION_EFFECT:
        Gameplay_Effect(g, action.effect);
        break;
    case PLAYER_ACTION_SHOT:
        M_Shots(g, action.shot);
        break;
    case PLAYER_ACTION_HUD_BOMBS:
        M_HudBombs(g, action.previous_bombs);
        break;
    case PLAYER_ACTION_PELLET_SPEED_RAISE:
        Pellets_SpeedRaise(
            &g->pellet_shared, (int16_t)(action.pellet_speed_delta * 40));
        break;
    default:
        Player_RenderAction(
            g->video, &g->player_sprites, g->stage.sprites, action);
        break;
    }
}

static void M_ItemAdd(void *const user, const int32_t slot, const bool bomb)
{
    GAMEPLAY *const g = user;
    Items_Add(
        &g->items, g->objects.cards[slot].left, g->objects.cards[slot].top,
        bomb);
}

static void M_PelletAdd(void *const user, const STAGE_PELLET_SPAWN spawn)
{
    GAMEPLAY *const g = user;
    Pellets_AddGroup(
        &g->pellets, &g->pellet_shared, &g->player, &g->rng,
        g->session->settings.difficulty, spawn.left, spawn.top,
        (PELLET_GROUP)spawn.group, spawn.speed);
}

static void M_Pellets(void *const user)
{
    GAMEPLAY *const g = user;
    Pellets_UpdateAndRender(&g->pellets, &g->pellet);
}

static void M_AudioJ(
    void *const user, const B20J_AUDIO_ACTION action,
    const B20J_ASSETS *const assets [[maybe_unused]])
{
    GAMEPLAY *const g = user;
    switch (action) {
    case B20J_AUDIO_LOAD_MUSIC:
        g->song = 12;
        break;
    case B20J_AUDIO_LOAD_EFFECTS:
        break;
    case B20J_AUDIO_PLAY:
        Gameplay_Play(g, g->song);
        break;
    case B20J_AUDIO_STOP:
        Gameplay_Stop(g);
        break;
    case B20J_AUDIO_FADE:
        Gameplay_Fade(g);
        break;
    }
}

static void M_AudioM(
    void *const user, const B20M_AUDIO_ACTION action,
    const B20M_ASSETS *const assets [[maybe_unused]])
{
    GAMEPLAY *const g = user;
    switch (action) {
    case B20M_AUDIO_LOAD_MUSIC:
        g->song = 13;
        break;
    case B20M_AUDIO_LOAD_SECOND_MUSIC:
        g->song = 14;
        break;
    case B20M_AUDIO_LOAD_EFFECTS:
        break;
    case B20M_AUDIO_PLAY:
        Gameplay_Play(g, g->song);
        break;
    case B20M_AUDIO_STOP:
        Gameplay_Stop(g);
        break;
    case B20M_AUDIO_FADE:
        Gameplay_Fade(g);
        break;
    }
}

void Gameplay_Bind(GAMEPLAY *const g)
{
    const GAMEPLAY_ASSETS *const a = g->assets;
    SESSION *const s = g->session;
    const PTN *const stage = g->boss_stage ? &a->boss_stage : &a->stage;
    g->player_sprites =
        (PLAYER_SPRITES) { &a->miko, &a->miko_ac, &a->miko_ac2 };
    g->shots_output = (SHOTS_OUTPUT) { M_Shots, g };
    g->player_output = (PLAYER_OUTPUT) { .action = M_Player, .user = g };
    g->display = (HUD_CONTEXT) {
        .video = g->video,
        .mask = &a->hud,
        .sprites = stage,
        .font = &a->hud_font,
        .gameplay = &g->metrics,
        .player = &g->player,
        .high_score = &s->high_score,
        .remaining_lives = &s->lives,
        .stage_number = (uint16_t)(s->stage_id + 1),
        .rank = (SETTINGS_DIFFICULTY)s->settings.difficulty,
    };
    g->extend = (EXTEND_CONTEXT) {
        .hud = &g->hud,
        .display = &g->display,
        .resident_lives = &g->retained_lives,
        .next = &g->next_extend,
        .shared = &g->pellet_shared,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->stage = (STAGE_CONTEXT) {
        .objects = &g->objects,
        .gameplay = &g->metrics,
        .orb = &g->orb,
        .player = &g->player,
        .rng = &g->rng,
        .video = g->video,
        .sprites = stage,
        .font = &a->hud_font,
        .stage_id = s->stage_id,
        .stage_num = (int16_t)(s->stage_id + 1),
        .difficulty = (SETTINGS_DIFFICULTY)s->settings.difficulty,
        .hud_score_and_combo = M_HudScore,
        .item_add = M_ItemAdd,
        .effect = Gameplay_Effect,
        .pellet_group = M_PelletAdd,
        .pellets_update = M_Pellets,
        .user = g,
    };
    g->item = (ITEM_CONTEXT) {
        .player = &g->player,
        .gameplay = &g->metrics,
        .point_value = &s->point_value,
        .video = g->video,
        .sprites = stage,
        .font = &a->item_font,
        .hud_bomb = M_HudBombs,
        .hud_score_and_combo = M_HudScore,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->pellet = (PELLET_CONTEXT) {
        .video = g->video,
        .assets = a->pellets,
        .player = &g->player,
        .orb = &g->orb,
        .shots = &g->shots,
        .rng = &g->rng,
        .shared = &g->pellet_shared,
        .shots_output = g->shots_output,
    };
    g->bombing = (BOMB_CONTEXT) {
        .video = g->video,
        .palette = &g->palette,
        .player = &g->player,
        .miko = &a->miko,
        .assets = &a->bomb,
        .stage_palette = &g->stage_palette,
        .pellets_decay = M_Decay,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->missing = (MISS_CONTEXT) {
        .video = g->video,
        .player = &g->player,
        .input = &g->input,
        .orb = &g->orb,
        .shots = &g->shots,
        .rng = &g->rng,
        .miko = &a->miko,
        .stage = stage,
        .difficulty = s->settings.difficulty,
        .remaining_lives = s->lives,
        .extra_bombs = 1,
        .pellets_decay = M_Decay,
        .hud_lives = M_HudLives,
        .hud_bombs = M_HudBombs,
        .timer_extend = M_TimerExtend,
        .pellet_speed_lower = M_SpeedLower,
        .user = g,
    };
    g->timing = (TIMER_CONTEXT) {
        .hud = &g->hud,
        .display = &g->display,
        .tram = &g->tram,
        .vsync = &g->vsync,
        .pellets = &g->pellets,
        .shared = &g->pellet_shared,
        .rng = &g->rng,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->route_assets = (ROUTE_SELECT_ASSETS) {
        &a->route_font,
        stage,
        &a->scenes[1],
        &a->scenes[2],
    };
    g->route = (ROUTE_SELECT_CONTEXT) {
        .video = g->video,
        .palette = &g->palette,
        .assets = &g->route_assets,
        .input = &g->input,
        .player = &g->player,
        .scene = &g->scene,
        .route = &s->route,
        .stage_cleared = &g->metrics.cleared,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->hit = (BOSS_HIT_CONTEXT) {
        .palette = &g->palette,
        .hud = &g->hud,
        .display = &g->display,
        .health = &g->health,
        .shots = &g->shots,
        .shots_output = g->shots_output,
        .orb = &g->orb,
        .test_damage = &g->test_damage,
        .frame_rand = s->frame_rand,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->c05 = (B05_CONTEXT) { .assets = &a->b05,
                             .sprites = &g->sprites,
                             .hit = &g->hit,
                             .stage_palette = &g->stage_palette,
                             .particles = &g->particles,
                             .pellets = &g->pellets,
                             .pellet_shared = &g->pellet_shared,
                             .rng = &g->rng,
                             .miko = &a->miko,
                             .route = &g->route,
                             .fade_music = Gameplay_Fade };
    g->c10j = (B10J_CONTEXT) { .assets = &a->b10j,
                               .sprites = &g->sprites,
                               .hit = &g->hit,
                               .stage_palette = &g->stage_palette,
                               .particles = &g->particles,
                               .pellets = &g->pellets,
                               .pellet_shared = &g->pellet_shared,
                               .missiles = &g->missiles,
                               .lasers = &g->lasers,
                               .rng = &g->rng,
                               .miko = &a->miko,
                               .scene = &g->scene,
                               .stage_cleared = &g->metrics.cleared,
                               .fade_music = Gameplay_Fade };
    g->c10m = (B10M_CONTEXT) { .assets = &a->b10m,
                               .sprites = &g->sprites,
                               .hit = &g->hit,
                               .stage_palette = &g->stage_palette,
                               .particles = &g->particles,
                               .pellets = &g->pellets,
                               .pellet_shared = &g->pellet_shared,
                               .missiles = &g->missiles,
                               .lasers = &g->lasers,
                               .rng = &g->rng,
                               .miko = &a->miko,
                               .scene = &g->scene,
                               .stage_cleared = &g->metrics.cleared,
                               .fade_music = Gameplay_Fade };
    g->c15j = (B15J_CONTEXT) { .assets = &a->b15j,
                               .sprites = &g->sprites,
                               .hit = &g->hit,
                               .stage_palette = &g->stage_palette,
                               .particles = &g->particles,
                               .pellets = &g->pellets,
                               .pellet_shared = &g->pellet_shared,
                               .lasers = &g->lasers,
                               .rng = &g->rng,
                               .miko = &a->miko,
                               .scene = &g->scene,
                               .stage_cleared = &g->metrics.cleared,
                               .fade_music = Gameplay_Fade };
    g->c15m = (B15M_CONTEXT) { .assets = &a->b15m,
                               .sprites = &g->sprites,
                               .hit = &g->hit,
                               .particles = &g->particles,
                               .pellets = &g->pellets,
                               .pellet_shared = &g->pellet_shared,
                               .missiles = &g->missiles,
                               .lasers = &g->lasers,
                               .rng = &g->rng,
                               .miko = &a->miko,
                               .scene = &g->scene,
                               .stage_cleared = &g->metrics.cleared,
                               .fade_music = Gameplay_Fade };
    g->c20j = (B20J_CONTEXT) { .assets = &a->b20j,
                               .sprites = &g->sprites,
                               .hit = &g->hit,
                               .pellets = &g->pellets,
                               .pellet_shared = &g->pellet_shared,
                               .lasers = &g->lasers,
                               .objects = &g->objects,
                               .tram = &g->tram,
                               .rng = &g->rng,
                               .game_cleared = &g->game_cleared,
                               .audio = M_AudioJ };
    g->c20m = (B20M_CONTEXT) { .assets = &a->b20m,
                               .sprites = &g->sprites,
                               .hit = &g->hit,
                               .pellets = &g->pellets,
                               .pellet_shared = &g->pellet_shared,
                               .lasers = &g->lasers,
                               .objects = &g->objects,
                               .tram = &g->tram,
                               .input = &g->input,
                               .timer = &g->timer,
                               .first_stage = &g->first_stage,
                               .timer_initialized = &g->timer_initialized,
                               .rng = &g->rng,
                               .game_cleared = &g->game_cleared,
                               .audio = M_AudioM };
    g->bonus_context = (BONUS_CONTEXT) {
        .stage = a->bonus,
        .font = &a->bonus_font,
        .stage_palette = &g->stage_palette,
        .palette = &g->palette,
        .hud = &g->hud,
        .display = &g->display,
        .extend = &g->extend,
        .timer = &g->timer,
        .input = &g->input,
        .score_bonus = &g->score_bonus,
        .bonus_per_stage = s->bonus_per_stage,
    };
    g->total_context = (BONUS_TOTAL_CONTEXT) {
        .common = &g->bonus_context,
        .assets = &a->total,
        .shots = &g->shots,
        .shots_output = g->shots_output,
        .pellets = &g->pellets,
        .continues_per_scene = s->continues_per_scene,
        .continues_total = &s->continues_total,
        .effect = Gameplay_Effect,
        .user = g,
    };
}
