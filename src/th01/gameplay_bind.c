#include "th01/gameplay_internal.h"

#include "core/base/assert.h"

static void M_Shots(void *const user, const SHOTS_ACTION action)
{
    GAMEPLAY *const g = user;
    if (action.kind == SHOTS_ACTION_EFFECT) {
        Sound_Effect(action.effect);
    } else {
        Shots_RenderAction(g->bindings.stage.sprites, action);
    }
}

static void M_Player(void *const user, const PLAYER_ACTION action)
{
    GAMEPLAY *const g = user;
    switch (action.kind) {
    case PLAYER_ACTION_EFFECT:
        Sound_Effect(action.effect);
        break;
    case PLAYER_ACTION_SHOT:
        M_Shots(g, action.shot);
        break;
    case PLAYER_ACTION_HUD_BOMBS:
        Hud_BombsPut(action.previous_bombs);
        break;
    case PLAYER_ACTION_PELLET_SPEED_RAISE:
        Pellets_SpeedRaise((int16_t)(action.pellet_speed_delta * 40));
        break;
    default:
        Player_RenderAction(
            &g->bindings.player_sprites, g->bindings.stage.sprites, action);
        break;
    }
}

static void M_Pellets(void *const user)
{
    GAMEPLAY *const g = user;
    Pellets_UpdateAndRender(&g->bindings.pellet);
}

void Gameplay_Bind(GAMEPLAY *const g)
{
    const GAMEPLAY_ASSETS *const a = g->storage.assets;
    const PTN *const stage = g->world.boss_stage ? &a->boss_stage : &a->stage;
    Hud_Load(&a->hud, stage, &a->hud_font);
    g->bindings.player_sprites = (PLAYER_SPRITES) { .miko = &a->miko,
                                                    .miko_ac = &a->miko_ac,
                                                    .miko_ac2 = &a->miko_ac2 };
    g->bindings.shots_output = (SHOTS_OUTPUT) { .action = M_Shots, .user = g };
    g->bindings.player_output =
        (PLAYER_OUTPUT) { .action = M_Player, .user = g };
    g->bindings.stage = (STAGE_CONTEXT) {
        .sprites = stage,
        .font = &a->hud_font,
        .stage_id = g_Session.stage_id,
        .stage_num = (int16_t)(g_Session.stage_id + 1),
        .difficulty = (SETTINGS_DIFFICULTY)g_Session.settings.difficulty,
        .pellets_update = M_Pellets,
        .user = g,
    };
    g->bindings.item = (ITEM_CONTEXT) {
        .point_value = &g_Session.point_value,
        .sprites = stage,
        .font = &a->item_font,
    };
    g->bindings.pellet = (PELLET_CONTEXT) {
        .assets = a->pellets,
        .shots_output = g->bindings.shots_output,
    };
    g->bindings.bombing = (BOMB_CONTEXT) {
        .palette = &g->world.palette,
        .miko = &a->miko,
        .assets = &a->bomb,
        .stage_palette = &g->world.stage_palette,
    };
    g->bindings.missing = (MISS_CONTEXT) {
        .miko = &a->miko,
        .stage = stage,
        .difficulty = g_Session.settings.difficulty,
        .remaining_lives = g_Session.lives,
        .extra_bombs = 1,
    };
    g->bindings.route_assets =
        (ROUTE_SELECT_ASSETS) { .font = &a->route_font,
                                .stage = stage,
                                .makai = &a->scenes[1],
                                .jigoku = &a->scenes[2] };
    g->bindings.route = (ROUTE_SELECT_CONTEXT) {
        .palette = &g->world.palette,
        .assets = &g->bindings.route_assets,
        .route = &g_Session.route,
        .stage_cleared = &g_Stage.cleared,
    };
    g->bindings.hit = (BOSS_HIT_CONTEXT) {
        .palette = &g->world.palette,
        .shots_output = g->bindings.shots_output,
        .test_damage = &g->world.test_damage,
        .frame_rand = g_Session.frame_rand,
    };
    g->bindings.c05 = (B05_CONTEXT) { .assets = &a->b05,
                                      .hit = &g->bindings.hit,
                                      .stage_palette = &g->world.stage_palette,
                                      .miko = &a->miko,
                                      .route = &g->bindings.route };
    g->bindings.c10j =
        (B10J_CONTEXT) { .assets = &a->b10j,
                         .hit = &g->bindings.hit,
                         .stage_palette = &g->world.stage_palette,
                         .miko = &a->miko,
                         .stage_cleared = &g_Stage.cleared };
    g->bindings.c10m =
        (B10M_CONTEXT) { .assets = &a->b10m,
                         .hit = &g->bindings.hit,
                         .stage_palette = &g->world.stage_palette,
                         .miko = &a->miko,
                         .stage_cleared = &g_Stage.cleared };
    g->bindings.c15j =
        (B15J_CONTEXT) { .assets = &a->b15j,
                         .hit = &g->bindings.hit,
                         .stage_palette = &g->world.stage_palette,
                         .miko = &a->miko,
                         .stage_cleared = &g_Stage.cleared };
    g->bindings.c15m = (B15M_CONTEXT) { .assets = &a->b15m,
                                        .hit = &g->bindings.hit,
                                        .miko = &a->miko,
                                        .stage_cleared = &g_Stage.cleared };
    g->bindings.c20j =
        (B20J_CONTEXT) { .assets = &a->b20j,
                         .hit = &g->bindings.hit,
                         .game_cleared = &g->world.game_cleared };
    g->bindings.c20m =
        (B20M_CONTEXT) { .assets = &a->b20m,
                         .hit = &g->bindings.hit,
                         .first_stage = &g->world.first_stage,
                         .timer_initialized = &g->world.timer_initialized,
                         .game_cleared = &g->world.game_cleared };
    g->bindings.bonus_context = (BONUS_CONTEXT) {
        .stage = a->bonus,
        .font = &a->bonus_font,
        .stage_palette = &g->world.stage_palette,
        .palette = &g->world.palette,
        .score_bonus = &g->world.score_bonus,
        .bonus_per_stage = g_Session.bonus_per_stage,
    };
    g->bindings.total_context = (BONUS_TOTAL_CONTEXT) {
        .common = &g->bindings.bonus_context,
        .assets = &a->total,
        .shots_output = g->bindings.shots_output,
        .continues_per_scene = g_Session.continues_per_scene,
        .continues_total = &g_Session.continues_total,
    };
}
