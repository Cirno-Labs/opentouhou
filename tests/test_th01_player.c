#include "th01/player.h"
#include "th01/shots.h"
#include "th01/rng.h"
#include "th01/pellets.h"
#include "th01/orb.h"

#include <string.h>

#include "test.h"

typedef struct {
    PLAYER_ACTION actions[256];
    int32_t action_count, bomb_calls;
    int16_t bomb_argument;
    bool bomb_done;
} CASE;

static void M_Action(void *const user, const PLAYER_ACTION action)
{
    CASE *const state = user;
    state->actions[state->action_count++] = action;
    if (action.kind == PLAYER_ACTION_HUD_BOMBS) {
        CHECK_EQ_INT(g_Player.remaining_bombs, action.previous_bombs - 1);
        CHECK(g_Player.bombing);
        CHECK(g_Player.deflecting);
        CHECK_EQ_INT(g_Orb.force_frame, 22);
    }
}

static bool M_Bomb(void *const user, const int16_t frame)
{
    CASE *const state = user;
    state->bomb_calls++;
    state->bomb_argument = frame;
    CHECK_EQ_INT(g_Orb.force_frame, 0);
    CHECK(g_Orb.force == -13);
    CHECK(g_GameInput.shot);
    CHECK(g_GameInput.strike);
    g_Player.invincible = true;
    g_Player.invincibility_time = state->bomb_done ? 100 : 0;
    g_Player.bomb_damaging = !state->bomb_done;
    return state->bomb_done;
}

static CASE M_Case(void)
{
    CASE state = {};
    Player_Init();
    Orb_Init();
    Shots_Init();
    Pellets_Init();
    g_Rng = 0;
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.left = 304;
    g_Player.remaining_bombs = 3;
    memset(&g_Orb, 0, sizeof(g_Orb));
    g_Orb.left = 304;
    g_Orb.top = 368;
    g_Rng = 1;
    GameInput_Init();
    Player_Reset(SETTINGS_DIFFICULTY_NORMAL);
    return state;
}

static void M_Tick(CASE *const state)
{
    state->action_count = 0;
    Player_UpdateAndRender((PLAYER_OUTPUT) { M_Action, M_Bomb, state });
}

TEST(reset_preserves_position_hits_and_shared_counters)
{
    CASE state = M_Case();
    g_Player.left = 607;
    g_Player.is_hit = true;
    g_Player.invincible = true;
    g_Player.invincibility_time = 91;
    g_Player.bomb_frame = UINT32_MAX;
    g_Player.bomb_damaging = true;
    g_GameInput.bomb = true;
    g_GameInput.shot = true;
    g_GameInput.strike = true;
    const int8_t expected[] = { 15, 12, 10, 8, 12 };
    for (int32_t rank = 0; rank < 5; rank++) {
        Player_Reset(rank);
        CHECK_EQ_INT(g_Player.deflect_duration, expected[rank]);
        CHECK_EQ_INT(g_Player.left, 607);
        CHECK(g_Player.is_hit);
        CHECK(g_Player.invincible);
        CHECK_EQ_INT(g_Player.invincibility_time, 91);
        CHECK_EQ_INT(g_Player.bomb_frame, UINT32_MAX);
        CHECK_EQ_INT(g_Player.remaining_bombs, 3);
        CHECK_EQ_INT(g_Player.submode, -1);
        CHECK_EQ_INT(g_Player.dash_direction, 1);
        CHECK(!g_Player.bomb_damaging);
        CHECK(!g_GameInput.bomb);
        CHECK(g_GameInput.shot);
        CHECK(g_GameInput.strike);
        CHECK_EQ_INT(state.action_count, 0);
    }
}

TEST(stationary_draw_gap_and_simultaneous_actions)
{
    CASE state = M_Case();
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 0);
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 1);
    CHECK_EQ_INT(state.actions[0].image, 0);
    g_GameInput.shot = true;
    g_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 4);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_PTN_PUT);
    CHECK_EQ_INT(state.actions[1].kind, PLAYER_ACTION_PTN_ERASE);
    CHECK_EQ_INT(state.actions[2].kind, PLAYER_ACTION_PTN_PUT);
    CHECK_EQ_INT(state.actions[2].image, 3);
    CHECK_EQ_INT(state.actions[3].kind, PLAYER_ACTION_PTN_ERASE);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_SWING_OR_SLIDE);
    CHECK(g_Player.deflecting);
    CHECK(g_Player.invincible_against_orb);
    CHECK(!g_Shots.slots[0].moving);
}

TEST(normal_shots_spawn_on_following_updates)
{
    CASE state = M_Case();
    g_GameInput.shot = true;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_SHOOT);
    CHECK_EQ_INT(g_Player.mode_frame, 0);
    CHECK(!g_Shots.slots[0].moving);
    M_Tick(&state);
    CHECK(g_Shots.slots[0].moving);
    CHECK_EQ_INT(g_Shots.slots[0].left, 312);
    CHECK_EQ_INT(g_Shots.slots[0].top, 368);
    CHECK(g_GameInput.shot);
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 0);
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_REGULAR);
    CHECK(!g_GameInput.shot);
    CHECK_EQ_INT(g_Player.submode, GAME_INPUT_BOTH);
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 0);
    state = M_Case();
    g_GameInput.horizontal = GAME_INPUT_RIGHT;
    g_GameInput.shot = true;
    M_Tick(&state);
    M_Tick(&state);
    CHECK(!g_Shots.slots[0].moving);
    M_Tick(&state);
    CHECK(g_Shots.slots[0].moving);
    CHECK_EQ_INT(g_Shots.slots[0].left, 324);
    CHECK_EQ_INT(g_Player.left, 316);
    CHECK_EQ_INT(state.actions[1].image, 15);
    CHECK_EQ_INT(state.actions[2].kind, PLAYER_ACTION_SHOT);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_REGULAR);
}

TEST(swing_retains_invisible_frames_and_deflection_window)
{
    CASE state = M_Case();
    g_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode_frame, 0);
    for (int32_t frame = 0; frame < 3; frame++) {
        M_Tick(&state);
        CHECK_EQ_INT(state.action_count, 0);
    }
    M_Tick(&state);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_48_OVERLAP);
    CHECK_EQ_INT(state.actions[0].image, 1);
    CHECK_EQ_INT(state.actions[0].previous_image, 0);
    while (g_Player.mode_frame <= 12) {
        M_Tick(&state);
    }
    CHECK(!g_Player.deflecting);
    while (g_Player.mode == PLAYER_MODE_SWING_OR_SLIDE) {
        M_Tick(&state);
    }
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_48_ERASE);
    CHECK_EQ_INT(state.actions[0].image, 7);
    CHECK(!g_Player.invincible_against_orb);
    CHECK(!g_GameInput.strike);
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 0);
    M_Tick(&state);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_PTN_PUT);
}

TEST(slide_requires_release_and_strike_has_combo_priority)
{
    CASE state = M_Case();
    g_GameInput.horizontal = GAME_INPUT_RIGHT;
    g_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.left, 308);
    CHECK_EQ_INT(state.actions[state.action_count - 1].image, 3);
    for (int32_t frame = 0; frame < 6; frame++) {
        M_Tick(&state);
    }
    CHECK(!g_Player.combo_enabled);
    g_GameInput.strike = false;
    M_Tick(&state);
    CHECK(g_Player.combo_enabled);
    while (g_Player.mode_frame < 12) {
        M_Tick(&state);
    }
    g_GameInput.horizontal = GAME_INPUT_LEFT;
    g_GameInput.strike = true;
    g_GameInput.shot = true;
    g_Orb.force_frame = 73;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_SPECIAL_FIRST);
    CHECK_EQ_INT(g_Player.submode, 2);
    CHECK_EQ_INT(g_Player.mode_frame, 0);
    CHECK_EQ_INT(g_Orb.force_frame, 73);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_32_OVERLAP);
    CHECK_EQ_INT(state.actions[1].kind, PLAYER_ACTION_BOS_32_ERASE);
    CHECK_EQ_INT(state.actions[2].kind, PLAYER_ACTION_EFFECT);
    CHECK(g_Player.deflecting);
    CHECK(!g_Player.sliding);
}

TEST(all_special_attacks_use_source_images_duration_and_movement)
{
    for (int8_t special = 0; special < 8; special++) {
        CASE state = M_Case();
        g_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
        g_Player.mode_frame = 0;
        g_Player.submode = special;
        g_Player.invincible_against_orb = true;
        g_Player.deflecting = true;
        const int32_t duration = special < 4 ? 28 : special < 6 ? 13 : 19;
        const int32_t direction = (special & 1) == 0 ? 1 : -1;
        const int32_t speed = special < 2 ? 2
            : special < 4                 ? 0
            : special < 6                 ? 6
                                          : 0;
        for (int32_t frame = 0; frame <= duration; frame++) {
            M_Tick(&state);
            CHECK_EQ_INT(g_Player.left, 304 + speed * direction * (frame + 1));
            const int32_t image = special < 4 ? ((special & 1) == 0 ? 14 : 8)
                    + (frame < 4 ? 0 : (frame - 7) / 4)
                : special < 6 ? ((special & 1) == 0 ? 5 : 4)
                              : ((special & 1) == 0 ? 24 : 20) + frame / 6;
            CHECK_EQ_INT(state.actions[0].image, image);
            if (frame < duration) {
                CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_SPECIAL_FIRST);
            }
        }
        CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_REGULAR);
        CHECK_EQ_INT(g_Player.mode_frame, 0);
        CHECK(!g_Player.invincible_against_orb);
        CHECK_EQ_INT(g_Player.submode, special);
        CHECK_EQ_INT(
            state.actions[state.action_count - 1].kind,
            special == 4 || special == 5 ? PLAYER_ACTION_BOS_32_ERASE
                                         : PLAYER_ACTION_BOS_48_ERASE);
        M_Tick(&state);
        CHECK_EQ_INT(state.action_count, special == 0 ? 1 : 0);
    }
}

TEST(shot_combo_spawns_seven_source_offsets_after_drawing)
{
    CASE state = M_Case();
    g_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
    g_Player.submode = 6;
    g_Player.deflecting = true;
    const int16_t offsets[] = { -8, 8, 0, -16, 0, 16, 0 };
    int32_t count = 0;
    for (int32_t frame = 0; frame <= 19; frame++) {
        M_Tick(&state);
        CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_48_OVERLAP);
        if (frame >= 1 && frame % 3 == 1) {
            CHECK_EQ_INT(state.actions[1].kind, PLAYER_ACTION_SHOT);
            CHECK_EQ_INT(state.actions[1].shot.effect, 1);
            CHECK_EQ_INT(g_Shots.slots[count].left, 312 + offsets[count]);
            count++;
        }
        if (frame >= 4) {
            CHECK(!g_Player.deflecting);
        }
    }
    CHECK_EQ_INT(count, 7);
}

TEST(combo_continuation_stops_after_third_attack)
{
    CASE state = M_Case();
    g_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
    g_Player.submode = 0;
    g_Player.mode_frame = 28;
    g_Player.combo_enabled = true;
    g_GameInput.strike = true;
    g_GameInput.horizontal = GAME_INPUT_LEFT;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_SPECIAL_SECOND);
    CHECK_EQ_INT(g_Player.submode, 2);
    CHECK_EQ_INT(g_Player.mode_frame, 0);
    g_Player.mode_frame = 28;
    g_Player.combo_enabled = true;
    g_GameInput.strike = false;
    g_GameInput.shot = true;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_SPECIAL_THIRD);
    CHECK_EQ_INT(g_Player.submode, 6);
    g_Player.mode_frame = 19;
    g_Player.combo_enabled = true;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_REGULAR);
    CHECK_EQ_INT(g_Player.submode, 6);
}

TEST(bomb_boundary_preserves_deferred_start_and_operation_order)
{
    CASE state = M_Case();
    g_GameInput.bomb = true;
    g_GameInput.shot = true;
    g_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.bomb_flag, 2);
    CHECK_EQ_INT(state.action_count, 1);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_PELLET_SPEED_RAISE);
    CHECK(state.actions[0].pellet_speed_delta == 0.025f);
    CHECK_EQ_INT(state.bomb_calls, 0);
    CHECK_EQ_INT(g_Player.remaining_bombs, 3);
    g_Orb.force_frame = 22;
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 1);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_HUD_BOMBS);
    CHECK_EQ_INT(state.actions[0].previous_bombs, 3);
    CHECK_EQ_INT(state.bomb_calls, 1);
    CHECK_EQ_INT(state.bomb_argument, 0);
    CHECK_EQ_INT(g_Player.bomb_frame, 0);
    CHECK(!g_GameInput.shot);
    CHECK(!g_GameInput.strike);
    CHECK(g_Player.invincible);
    CHECK(g_Player.bomb_damaging);
    g_Player.bomb_frame = 0x8000;
    g_GameInput.shot = true;
    g_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.bomb_argument, INT16_MIN);
    CHECK_EQ_INT(state.actions[0].image, 3);
    g_Player.bomb_frame = 140;
    g_GameInput.shot = true;
    g_GameInput.strike = true;
    state.bomb_done = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.actions[0].image, 0);
    CHECK_EQ_INT(g_Player.bomb_flag, 0);
    CHECK(!g_Player.deflecting);
    CHECK(!g_Player.bombing);
    CHECK_EQ_INT(g_Player.invincibility_time, 100);
}

TEST(bomb_exclusion_checks_only_first_special_mode)
{
    for (int8_t mode = PLAYER_MODE_SPECIAL_FIRST;
         mode <= PLAYER_MODE_SPECIAL_THIRD; mode++) {
        CASE state = M_Case();
        g_Player.mode = mode;
        g_Player.submode = 2;
        g_GameInput.bomb = true;
        M_Tick(&state);
        CHECK_EQ_INT(
            g_Player.bomb_flag, mode == PLAYER_MODE_SPECIAL_FIRST ? 0 : 2);
        CHECK(g_GameInput.bomb == (mode == PLAYER_MODE_SPECIAL_FIRST));
        CHECK_EQ_INT(g_Player.dash_cycle, 1);
    }
}

TEST(actor_validates_required_sprites_and_wraps_counters)
{
    uint8_t pixel = 0;
    PTN miko = { .count = 16, .pixels = &pixel };
    BOS large = { 48, 48, 28, &pixel, &pixel };
    BOS small = { 48, 32, 6, &pixel, &pixel };
    const PLAYER_SPRITES sprites = { &miko, &large, &small };
    CHECK(IS_OK(Player_ValidateSprites(&sprites)));
    large.count = 27;
    CHECK(!Result_Ignore(Player_ValidateSprites(&sprites)));
    large.count = 28;
    small.height = 48;
    CHECK(!Result_Ignore(Player_ValidateSprites(&sprites)));
    small.height = 32;
    miko.count = 15;
    CHECK(!Result_Ignore(Player_ValidateSprites(&sprites)));
    CASE state = M_Case();
    g_Player.mode = PLAYER_MODE_SHOOT;
    g_Player.mode_frame = 127;
    g_Player.submode = GAME_INPUT_BOTH;
    M_Tick(&state);
    CHECK_EQ_INT(g_Player.mode_frame, -128);
    g_Player.left = INT16_MAX;
    Player_MoveAndClamp(1);
    CHECK_EQ_INT(g_Player.left, 0);
    g_Player.left = 607;
    Player_MoveAndClamp(6);
    CHECK_EQ_INT(g_Player.left, 608);
}
