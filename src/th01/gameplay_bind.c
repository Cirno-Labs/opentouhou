#include "th01/gameplay_internal.h"

#include "core/base/assert.h"

void Gameplay_Effect(void *const user, const int16_t number)
{
    GAMEPLAY *const g = user;
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Effect(g->storage.audio, (uint16_t)number);
    }
}

void Gameplay_Play(GAMEPLAY *const g, const size_t song)
{
    g->storage.song = song;
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Play(g->storage.audio, song);
    }
}

void Gameplay_Fade(void *const user)
{
    GAMEPLAY *const g = user;
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Fade(g->storage.audio, 1);
    }
}

void Gameplay_Stop(GAMEPLAY *const g)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Stop(g->storage.audio);
    }
}

static void M_HudScore(void *const user)
{
    GAMEPLAY *const g = user;
    Hud_ScoreAndCombo(&g->world.hud, &g->bindings.display);
}

static void M_HudBombs(void *const user, const int16_t previous)
{
    GAMEPLAY *const g = user;
    Hud_BombsPut(&g->world.hud, &g->bindings.display, previous);
}

static void M_HudLives(void *const user, const int16_t previous)
{
    GAMEPLAY *const g = user;
    Hud_LivesPut(&g->world.hud, &g->bindings.display, previous);
}

static void M_Decay(void *const user)
{
    GAMEPLAY *const g = user;
    Pellets_Decay(&g->world.pellets, &g->world.pellet_shared);
}

static void M_TimerExtend(void *const user)
{
    GAMEPLAY *const g = user;
    StageTimer_ExtendAndPut(&g->bindings.timing);
}

static void M_SpeedLower(
    void *const user, const float minimum, const float delta)
{
    GAMEPLAY *const g = user;
    Pellets_SpeedLower(
        &g->world.pellet_shared, (int16_t)(minimum * 40),
        (int16_t)(delta * 40));
}

static void M_Shots(void *const user, const SHOTS_ACTION action)
{
    GAMEPLAY *const g = user;
    if (action.kind == SHOTS_ACTION_EFFECT) {
        Gameplay_Effect(g, action.effect);
    } else {
        Shots_RenderAction(g->bindings.stage.sprites, action);
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
            &g->world.pellet_shared, (int16_t)(action.pellet_speed_delta * 40));
        break;
    default:
        Player_RenderAction(
            &g->bindings.player_sprites, g->bindings.stage.sprites, action);
        break;
    }
}

static void M_ItemAdd(void *const user, const int32_t slot, const bool bomb)
{
    GAMEPLAY *const g = user;
    Items_Add(
        &g->world.items, g->world.objects.cards[slot].left,
        g->world.objects.cards[slot].top, bomb);
}

static void M_PelletAdd(void *const user, const STAGE_PELLET_SPAWN spawn)
{
    GAMEPLAY *const g = user;
    Pellets_AddGroup(
        &g->world.pellets, &g->world.pellet_shared, &g->world.player,
        &g->world.rng, g_Session.settings.difficulty, spawn.left, spawn.top,
        (PELLET_GROUP)spawn.group, spawn.speed);
}

static void M_Pellets(void *const user)
{
    GAMEPLAY *const g = user;
    Pellets_UpdateAndRender(&g->world.pellets, &g->bindings.pellet);
}

static void M_AudioJ(void *const user, const B20J_AUDIO_ACTION action)
{
    GAMEPLAY *const g = user;
    switch (action) {
    case B20J_AUDIO_LOAD_MUSIC:
        g->storage.song = 12;
        break;
    case B20J_AUDIO_PLAY:
        Gameplay_Play(g, g->storage.song);
        break;
    case B20J_AUDIO_STOP:
        Gameplay_Stop(g);
        break;
    case B20J_AUDIO_FADE:
        Gameplay_Fade(g);
        break;
    }
}

static void M_AudioM(void *const user, const B20M_AUDIO_ACTION action)
{
    GAMEPLAY *const g = user;
    switch (action) {
    case B20M_AUDIO_LOAD_MUSIC:
        g->storage.song = 13;
        break;
    case B20M_AUDIO_LOAD_SECOND_MUSIC:
        g->storage.song = 14;
        break;
    case B20M_AUDIO_PLAY:
        Gameplay_Play(g, g->storage.song);
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
    const GAMEPLAY_ASSETS *const a = g->storage.assets;
    const PTN *const stage = g->world.boss_stage ? &a->boss_stage : &a->stage;
    g->bindings.player_sprites = (PLAYER_SPRITES) { .miko = &a->miko,
                                                    .miko_ac = &a->miko_ac,
                                                    .miko_ac2 = &a->miko_ac2 };
    g->bindings.shots_output = (SHOTS_OUTPUT) { .action = M_Shots, .user = g };
    g->bindings.player_output =
        (PLAYER_OUTPUT) { .action = M_Player, .user = g };
    g->bindings.display = (HUD_CONTEXT) {
        .mask = &a->hud,
        .sprites = stage,
        .font = &a->hud_font,
        .gameplay = &g->world.metrics,
        .player = &g->world.player,
        .high_score = &g_Session.high_score,
        .remaining_lives = &g_Session.lives,
        .stage_number = (uint16_t)(g_Session.stage_id + 1),
        .rank = (SETTINGS_DIFFICULTY)g_Session.settings.difficulty,
    };
    g->bindings.extend = (EXTEND_CONTEXT) {
        .hud = &g->world.hud,
        .display = &g->bindings.display,
        .resident_lives = &g->world.retained_lives,
        .next = &g->world.next_extend,
        .shared = &g->world.pellet_shared,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->bindings.stage = (STAGE_CONTEXT) {
        .objects = &g->world.objects,
        .gameplay = &g->world.metrics,
        .orb = &g->world.orb,
        .player = &g->world.player,
        .rng = &g->world.rng,
        .sprites = stage,
        .font = &a->hud_font,
        .stage_id = g_Session.stage_id,
        .stage_num = (int16_t)(g_Session.stage_id + 1),
        .difficulty = (SETTINGS_DIFFICULTY)g_Session.settings.difficulty,
        .hud_score_and_combo = M_HudScore,
        .item_add = M_ItemAdd,
        .effect = Gameplay_Effect,
        .pellet_group = M_PelletAdd,
        .pellets_update = M_Pellets,
        .user = g,
    };
    g->bindings.item = (ITEM_CONTEXT) {
        .player = &g->world.player,
        .gameplay = &g->world.metrics,
        .point_value = &g_Session.point_value,
        .sprites = stage,
        .font = &a->item_font,
        .hud_bomb = M_HudBombs,
        .hud_score_and_combo = M_HudScore,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->bindings.pellet = (PELLET_CONTEXT) {
        .assets = a->pellets,
        .player = &g->world.player,
        .orb = &g->world.orb,
        .shots = &g->world.shots,
        .rng = &g->world.rng,
        .shared = &g->world.pellet_shared,
        .shots_output = g->bindings.shots_output,
    };
    g->bindings.bombing = (BOMB_CONTEXT) {
        .palette = &g->world.palette,
        .player = &g->world.player,
        .miko = &a->miko,
        .assets = &a->bomb,
        .stage_palette = &g->world.stage_palette,
        .pellets_decay = M_Decay,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->bindings.missing = (MISS_CONTEXT) {
        .player = &g->world.player,
        .orb = &g->world.orb,
        .shots = &g->world.shots,
        .rng = &g->world.rng,
        .miko = &a->miko,
        .stage = stage,
        .difficulty = g_Session.settings.difficulty,
        .remaining_lives = g_Session.lives,
        .extra_bombs = 1,
        .pellets_decay = M_Decay,
        .hud_lives = M_HudLives,
        .hud_bombs = M_HudBombs,
        .timer_extend = M_TimerExtend,
        .pellet_speed_lower = M_SpeedLower,
        .user = g,
    };
    g->bindings.timing = (TIMER_CONTEXT) {
        .hud = &g->world.hud,
        .display = &g->bindings.display,
        .pellets = &g->world.pellets,
        .shared = &g->world.pellet_shared,
        .rng = &g->world.rng,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->bindings.route_assets =
        (ROUTE_SELECT_ASSETS) { .font = &a->route_font,
                                .stage = stage,
                                .makai = &a->scenes[1],
                                .jigoku = &a->scenes[2] };
    g->bindings.route = (ROUTE_SELECT_CONTEXT) {
        .palette = &g->world.palette,
        .assets = &g->bindings.route_assets,
        .player = &g->world.player,
        .scene = &g->world.scene,
        .route = &g_Session.route,
        .stage_cleared = &g->world.metrics.cleared,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->bindings.hit = (BOSS_HIT_CONTEXT) {
        .palette = &g->world.palette,
        .hud = &g->world.hud,
        .display = &g->bindings.display,
        .health = &g->world.health,
        .shots = &g->world.shots,
        .shots_output = g->bindings.shots_output,
        .orb = &g->world.orb,
        .test_damage = &g->world.test_damage,
        .frame_rand = g_Session.frame_rand,
        .effect = Gameplay_Effect,
        .user = g,
    };
    g->bindings.c05 = (B05_CONTEXT) { .assets = &a->b05,
                                      .sprites = &g->world.sprites,
                                      .hit = &g->bindings.hit,
                                      .stage_palette = &g->world.stage_palette,
                                      .particles = &g->world.particles,
                                      .pellets = &g->world.pellets,
                                      .pellet_shared = &g->world.pellet_shared,
                                      .rng = &g->world.rng,
                                      .miko = &a->miko,
                                      .route = &g->bindings.route,
                                      .fade_music = Gameplay_Fade };
    g->bindings.c10j =
        (B10J_CONTEXT) { .assets = &a->b10j,
                         .sprites = &g->world.sprites,
                         .hit = &g->bindings.hit,
                         .stage_palette = &g->world.stage_palette,
                         .particles = &g->world.particles,
                         .pellets = &g->world.pellets,
                         .pellet_shared = &g->world.pellet_shared,
                         .missiles = &g->world.missiles,
                         .lasers = &g->world.lasers,
                         .rng = &g->world.rng,
                         .miko = &a->miko,
                         .scene = &g->world.scene,
                         .stage_cleared = &g->world.metrics.cleared,
                         .fade_music = Gameplay_Fade };
    g->bindings.c10m =
        (B10M_CONTEXT) { .assets = &a->b10m,
                         .sprites = &g->world.sprites,
                         .hit = &g->bindings.hit,
                         .stage_palette = &g->world.stage_palette,
                         .particles = &g->world.particles,
                         .pellets = &g->world.pellets,
                         .pellet_shared = &g->world.pellet_shared,
                         .missiles = &g->world.missiles,
                         .lasers = &g->world.lasers,
                         .rng = &g->world.rng,
                         .miko = &a->miko,
                         .scene = &g->world.scene,
                         .stage_cleared = &g->world.metrics.cleared,
                         .fade_music = Gameplay_Fade };
    g->bindings.c15j =
        (B15J_CONTEXT) { .assets = &a->b15j,
                         .sprites = &g->world.sprites,
                         .hit = &g->bindings.hit,
                         .stage_palette = &g->world.stage_palette,
                         .particles = &g->world.particles,
                         .pellets = &g->world.pellets,
                         .pellet_shared = &g->world.pellet_shared,
                         .lasers = &g->world.lasers,
                         .rng = &g->world.rng,
                         .miko = &a->miko,
                         .scene = &g->world.scene,
                         .stage_cleared = &g->world.metrics.cleared,
                         .fade_music = Gameplay_Fade };
    g->bindings.c15m =
        (B15M_CONTEXT) { .assets = &a->b15m,
                         .sprites = &g->world.sprites,
                         .hit = &g->bindings.hit,
                         .particles = &g->world.particles,
                         .pellets = &g->world.pellets,
                         .pellet_shared = &g->world.pellet_shared,
                         .missiles = &g->world.missiles,
                         .lasers = &g->world.lasers,
                         .rng = &g->world.rng,
                         .miko = &a->miko,
                         .scene = &g->world.scene,
                         .stage_cleared = &g->world.metrics.cleared,
                         .fade_music = Gameplay_Fade };
    g->bindings.c20j =
        (B20J_CONTEXT) { .assets = &a->b20j,
                         .sprites = &g->world.sprites,
                         .hit = &g->bindings.hit,
                         .pellets = &g->world.pellets,
                         .pellet_shared = &g->world.pellet_shared,
                         .lasers = &g->world.lasers,
                         .objects = &g->world.objects,
                         .rng = &g->world.rng,
                         .game_cleared = &g->world.game_cleared,
                         .audio = M_AudioJ };
    g->bindings.c20m =
        (B20M_CONTEXT) { .assets = &a->b20m,
                         .sprites = &g->world.sprites,
                         .hit = &g->bindings.hit,
                         .pellets = &g->world.pellets,
                         .pellet_shared = &g->world.pellet_shared,
                         .lasers = &g->world.lasers,
                         .objects = &g->world.objects,
                         .first_stage = &g->world.first_stage,
                         .timer_initialized = &g->world.timer_initialized,
                         .rng = &g->world.rng,
                         .game_cleared = &g->world.game_cleared,
                         .audio = M_AudioM };
    g->bindings.bonus_context = (BONUS_CONTEXT) {
        .stage = a->bonus,
        .font = &a->bonus_font,
        .stage_palette = &g->world.stage_palette,
        .palette = &g->world.palette,
        .hud = &g->world.hud,
        .display = &g->bindings.display,
        .extend = &g->bindings.extend,
        .score_bonus = &g->world.score_bonus,
        .bonus_per_stage = g_Session.bonus_per_stage,
    };
    g->bindings.total_context = (BONUS_TOTAL_CONTEXT) {
        .common = &g->bindings.bonus_context,
        .assets = &a->total,
        .shots = &g->world.shots,
        .shots_output = g->bindings.shots_output,
        .pellets = &g->world.pellets,
        .continues_per_scene = g_Session.continues_per_scene,
        .continues_total = &g_Session.continues_total,
        .effect = Gameplay_Effect,
        .user = g,
    };
}
