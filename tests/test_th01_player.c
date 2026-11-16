#include "th01/player.h"

#include "test.h"

typedef struct {
    PLAYER player;
    GAME_INPUT input;
    ORB orb;
    SHOTS shots;
    uint32_t rng;
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
        CHECK_EQ_INT(state->player.remaining_bombs, action.previous_bombs - 1);
        CHECK(state->player.bombing);
        CHECK(state->player.deflecting);
        CHECK_EQ_INT(state->orb.force_frame, 22);
    }
}

static bool M_Bomb(void *const user, PLAYER *const player, const int16_t frame)
{
    CASE *const state = user;
    state->bomb_calls++;
    state->bomb_argument = frame;
    CHECK_EQ_INT(state->orb.force_frame, 0);
    CHECK(state->orb.force == -13);
    CHECK(state->input.shot);
    CHECK(state->input.strike);
    player->invincible = true;
    player->invincibility_time = state->bomb_done ? 100 : 0;
    player->bomb_damaging = !state->bomb_done;
    return state->bomb_done;
}

static CASE M_Case(void)
{
    CASE state = {
        .player = { .left = 304, .remaining_bombs = 3 },
        .orb = { .left = 304, .top = 368 },
        .rng = 1,
    };
    Player_Reset(&state.player, &state.input, SETTINGS_DIFFICULTY_NORMAL);
    return state;
}

static void M_Tick(CASE *const state)
{
    state->action_count = 0;
    Player_UpdateAndRender(
        &state->player, &state->input, &state->orb, &state->shots, &state->rng,
        (PLAYER_OUTPUT) { M_Action, M_Bomb, state });
}

TEST(reset_preserves_position_hits_and_shared_counters)
{
    CASE state = M_Case();
    state.player.left = 607;
    state.player.is_hit = true;
    state.player.invincible = true;
    state.player.invincibility_time = 91;
    state.player.bomb_frame = UINT32_MAX;
    state.player.bomb_damaging = true;
    state.input.bomb = true;
    state.input.shot = true;
    state.input.strike = true;
    const int8_t expected[] = { 15, 12, 10, 8, 12 };
    for (int32_t rank = 0; rank < 5; rank++) {
        Player_Reset(&state.player, &state.input, rank);
        CHECK_EQ_INT(state.player.deflect_duration, expected[rank]);
        CHECK_EQ_INT(state.player.left, 607);
        CHECK(state.player.is_hit);
        CHECK(state.player.invincible);
        CHECK_EQ_INT(state.player.invincibility_time, 91);
        CHECK_EQ_INT(state.player.bomb_frame, UINT32_MAX);
        CHECK_EQ_INT(state.player.remaining_bombs, 3);
        CHECK_EQ_INT(state.player.submode, -1);
        CHECK_EQ_INT(state.player.dash_direction, 1);
        CHECK(!state.player.bomb_damaging);
        CHECK(!state.input.bomb);
        CHECK(state.input.shot);
        CHECK(state.input.strike);
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
    state.input.shot = true;
    state.input.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 4);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_PTN_PUT);
    CHECK_EQ_INT(state.actions[1].kind, PLAYER_ACTION_PTN_ERASE);
    CHECK_EQ_INT(state.actions[2].kind, PLAYER_ACTION_PTN_PUT);
    CHECK_EQ_INT(state.actions[2].image, 3);
    CHECK_EQ_INT(state.actions[3].kind, PLAYER_ACTION_PTN_ERASE);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_SWING_OR_SLIDE);
    CHECK(state.player.deflecting);
    CHECK(state.player.invincible_against_orb);
    CHECK(!state.shots.slots[0].moving);
}

TEST(normal_shots_spawn_on_following_updates)
{
    CASE state = M_Case();
    state.input.shot = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_SHOOT);
    CHECK_EQ_INT(state.player.mode_frame, 0);
    CHECK(!state.shots.slots[0].moving);
    M_Tick(&state);
    CHECK(state.shots.slots[0].moving);
    CHECK_EQ_INT(state.shots.slots[0].left, 312);
    CHECK_EQ_INT(state.shots.slots[0].top, 368);
    CHECK(state.input.shot);
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 0);
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_REGULAR);
    CHECK(!state.input.shot);
    CHECK_EQ_INT(state.player.submode, GAME_INPUT_BOTH);
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 0);
    state = M_Case();
    state.input.horizontal = GAME_INPUT_RIGHT;
    state.input.shot = true;
    M_Tick(&state);
    M_Tick(&state);
    CHECK(!state.shots.slots[0].moving);
    M_Tick(&state);
    CHECK(state.shots.slots[0].moving);
    CHECK_EQ_INT(state.shots.slots[0].left, 324);
    CHECK_EQ_INT(state.player.left, 316);
    CHECK_EQ_INT(state.actions[1].image, 15);
    CHECK_EQ_INT(state.actions[2].kind, PLAYER_ACTION_SHOT);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_REGULAR);
}

TEST(swing_retains_invisible_frames_and_deflection_window)
{
    CASE state = M_Case();
    state.input.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode_frame, 0);
    for (int32_t frame = 0; frame < 3; frame++) {
        M_Tick(&state);
        CHECK_EQ_INT(state.action_count, 0);
    }
    M_Tick(&state);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_48_OVERLAP);
    CHECK_EQ_INT(state.actions[0].image, 1);
    CHECK_EQ_INT(state.actions[0].previous_image, 0);
    while (state.player.mode_frame <= 12) {
        M_Tick(&state);
    }
    CHECK(!state.player.deflecting);
    while (state.player.mode == PLAYER_MODE_SWING_OR_SLIDE) {
        M_Tick(&state);
    }
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_48_ERASE);
    CHECK_EQ_INT(state.actions[0].image, 7);
    CHECK(!state.player.invincible_against_orb);
    CHECK(!state.input.strike);
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 0);
    M_Tick(&state);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_PTN_PUT);
}

TEST(slide_requires_release_and_strike_has_combo_priority)
{
    CASE state = M_Case();
    state.input.horizontal = GAME_INPUT_RIGHT;
    state.input.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.left, 308);
    CHECK_EQ_INT(state.actions[state.action_count - 1].image, 3);
    for (int32_t frame = 0; frame < 6; frame++) {
        M_Tick(&state);
    }
    CHECK(!state.player.combo_enabled);
    state.input.strike = false;
    M_Tick(&state);
    CHECK(state.player.combo_enabled);
    while (state.player.mode_frame < 12) {
        M_Tick(&state);
    }
    state.input.horizontal = GAME_INPUT_LEFT;
    state.input.strike = true;
    state.input.shot = true;
    state.orb.force_frame = 73;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_SPECIAL_FIRST);
    CHECK_EQ_INT(state.player.submode, 2);
    CHECK_EQ_INT(state.player.mode_frame, 0);
    CHECK_EQ_INT(state.orb.force_frame, 73);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_32_OVERLAP);
    CHECK_EQ_INT(state.actions[1].kind, PLAYER_ACTION_BOS_32_ERASE);
    CHECK_EQ_INT(state.actions[2].kind, PLAYER_ACTION_EFFECT);
    CHECK(state.player.deflecting);
    CHECK(!state.player.sliding);
}

TEST(all_special_attacks_use_source_images_duration_and_movement)
{
    for (int8_t special = 0; special < 8; special++) {
        CASE state = M_Case();
        state.player.mode = PLAYER_MODE_SPECIAL_FIRST;
        state.player.mode_frame = 0;
        state.player.submode = special;
        state.player.invincible_against_orb = true;
        state.player.deflecting = true;
        const int32_t duration = special < 4 ? 28 : special < 6 ? 13 : 19;
        const int32_t direction = (special & 1) == 0 ? 1 : -1;
        const int32_t speed = special < 2 ? 2
            : special < 4                 ? 0
            : special < 6                 ? 6
                                          : 0;
        for (int32_t frame = 0; frame <= duration; frame++) {
            M_Tick(&state);
            CHECK_EQ_INT(
                state.player.left, 304 + speed * direction * (frame + 1));
            const int32_t image = special < 4 ? ((special & 1) == 0 ? 14 : 8)
                    + (frame < 4 ? 0 : (frame - 7) / 4)
                : special < 6 ? ((special & 1) == 0 ? 5 : 4)
                              : ((special & 1) == 0 ? 24 : 20) + frame / 6;
            CHECK_EQ_INT(state.actions[0].image, image);
            if (frame < duration) {
                CHECK_EQ_INT(state.player.mode, PLAYER_MODE_SPECIAL_FIRST);
            }
        }
        CHECK_EQ_INT(state.player.mode, PLAYER_MODE_REGULAR);
        CHECK_EQ_INT(state.player.mode_frame, 0);
        CHECK(!state.player.invincible_against_orb);
        CHECK_EQ_INT(state.player.submode, special);
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
    state.player.mode = PLAYER_MODE_SPECIAL_FIRST;
    state.player.submode = 6;
    state.player.deflecting = true;
    const int16_t offsets[] = { -8, 8, 0, -16, 0, 16, 0 };
    int32_t count = 0;
    for (int32_t frame = 0; frame <= 19; frame++) {
        M_Tick(&state);
        CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_BOS_48_OVERLAP);
        if (frame >= 1 && frame % 3 == 1) {
            CHECK_EQ_INT(state.actions[1].kind, PLAYER_ACTION_SHOT);
            CHECK_EQ_INT(state.actions[1].shot.effect, 1);
            CHECK_EQ_INT(state.shots.slots[count].left, 312 + offsets[count]);
            count++;
        }
        if (frame >= 4) {
            CHECK(!state.player.deflecting);
        }
    }
    CHECK_EQ_INT(count, 7);
}

TEST(combo_continuation_stops_after_third_attack)
{
    CASE state = M_Case();
    state.player.mode = PLAYER_MODE_SPECIAL_FIRST;
    state.player.submode = 0;
    state.player.mode_frame = 28;
    state.player.combo_enabled = true;
    state.input.strike = true;
    state.input.horizontal = GAME_INPUT_LEFT;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_SPECIAL_SECOND);
    CHECK_EQ_INT(state.player.submode, 2);
    CHECK_EQ_INT(state.player.mode_frame, 0);
    state.player.mode_frame = 28;
    state.player.combo_enabled = true;
    state.input.strike = false;
    state.input.shot = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_SPECIAL_THIRD);
    CHECK_EQ_INT(state.player.submode, 6);
    state.player.mode_frame = 19;
    state.player.combo_enabled = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_REGULAR);
    CHECK_EQ_INT(state.player.submode, 6);
}

TEST(bomb_boundary_preserves_deferred_start_and_operation_order)
{
    CASE state = M_Case();
    state.input.bomb = true;
    state.input.shot = true;
    state.input.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.bomb_flag, 2);
    CHECK_EQ_INT(state.action_count, 1);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_PELLET_SPEED_RAISE);
    CHECK(state.actions[0].pellet_speed_delta == 0.025f);
    CHECK_EQ_INT(state.bomb_calls, 0);
    CHECK_EQ_INT(state.player.remaining_bombs, 3);
    state.orb.force_frame = 22;
    M_Tick(&state);
    CHECK_EQ_INT(state.action_count, 1);
    CHECK_EQ_INT(state.actions[0].kind, PLAYER_ACTION_HUD_BOMBS);
    CHECK_EQ_INT(state.actions[0].previous_bombs, 3);
    CHECK_EQ_INT(state.bomb_calls, 1);
    CHECK_EQ_INT(state.bomb_argument, 0);
    CHECK_EQ_INT(state.player.bomb_frame, 0);
    CHECK(!state.input.shot);
    CHECK(!state.input.strike);
    CHECK(state.player.invincible);
    CHECK(state.player.bomb_damaging);
    state.player.bomb_frame = 0x8000;
    state.input.shot = true;
    state.input.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.bomb_argument, INT16_MIN);
    CHECK_EQ_INT(state.actions[0].image, 3);
    state.player.bomb_frame = 140;
    state.input.shot = true;
    state.input.strike = true;
    state.bomb_done = true;
    M_Tick(&state);
    CHECK_EQ_INT(state.actions[0].image, 0);
    CHECK_EQ_INT(state.player.bomb_flag, 0);
    CHECK(!state.player.deflecting);
    CHECK(!state.player.bombing);
    CHECK_EQ_INT(state.player.invincibility_time, 100);
}

TEST(bomb_exclusion_checks_only_first_special_mode)
{
    for (int8_t mode = PLAYER_MODE_SPECIAL_FIRST;
         mode <= PLAYER_MODE_SPECIAL_THIRD; mode++) {
        CASE state = M_Case();
        state.player.mode = mode;
        state.player.submode = 2;
        state.input.bomb = true;
        M_Tick(&state);
        CHECK_EQ_INT(
            state.player.bomb_flag, mode == PLAYER_MODE_SPECIAL_FIRST ? 0 : 2);
        CHECK(state.input.bomb == (mode == PLAYER_MODE_SPECIAL_FIRST));
        CHECK_EQ_INT(state.player.dash_cycle, 1);
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
    state.player.mode = PLAYER_MODE_SHOOT;
    state.player.mode_frame = 127;
    state.player.submode = GAME_INPUT_BOTH;
    M_Tick(&state);
    CHECK_EQ_INT(state.player.mode_frame, -128);
    state.player.left = INT16_MAX;
    Player_MoveAndClamp(&state.player, 1);
    CHECK_EQ_INT(state.player.left, 0);
    state.player.left = 607;
    Player_MoveAndClamp(&state.player, 6);
    CHECK_EQ_INT(state.player.left, 608);
}
