#include "../../src/th01/input.c"
#include "../../src/th01/player.c"
#include "../../src/th01/orb.c"
#include "th01/player.h"
#include "th01/bomb.h"
#include "th01/hud.h"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/shots.h"

#include <string.h>

#include "core/base/arena.h"

#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

typedef struct {
    ARENA arena;
    PTN miko;
    BOS miko_ac;
    BOS miko_ac2;
    BOMB_ASSETS bomb_assets;
    PLAYER_SPRITES sprites;
    PLAYER_BOMB_CONTINUATION continuation;
    bool bomb_active;
} CASE;

// Fills state in place: its sprite table points into state's own fields.
static void M_Case(CASE *const state)
{
    memset(state, 0, sizeof(*state));
    Th01Test_ResetWorld();
    Arena_Init(&state->arena, "player", 256 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&state->arena);
    Video_Init(&g_HeapAllocator);
    Hud_Init(a);
    state->miko =
        (PTN) { .count = 32, .pixels = Allocator_Alloc(a, 32 * 1024) };
    memset(state->miko.pixels, PTN_TRANSPARENT, 32 * 1024);
    state->miko_ac = (BOS) { 48, 48, 28, Allocator_AllocZ(a, 28 * 2304),
                             Allocator_AllocZ(a, 28 * 2304) };
    state->miko_ac2 = (BOS) { 48, 32, 6, Allocator_AllocZ(a, 6 * 1536),
                              Allocator_AllocZ(a, 6 * 1536) };
    state->sprites = (PLAYER_SPRITES) { .miko = &state->miko,
                                        .miko_ac = &state->miko_ac,
                                        .miko_ac2 = &state->miko_ac2 };
    for (int32_t i = 0; i < 2; i++) {
        const int32_t count = i == 0 ? 8 : 1;
        state->bomb_assets.kuji[i] =
            (GRC) { 32, 32, count, Allocator_Alloc(a, count * 1024) };
    }
    memset(&m_Player, 0, sizeof(m_Player));
    m_Player.left = 304;
    m_Player.remaining_bombs = 3;
    memset(&m_Orb, 0, sizeof(m_Orb));
    m_Orb.left = 304;
    m_Orb.top = 368;
    g_Rng = 1;
    GameInput_Init();
    Player_Reset(SETTINGS_DIFFICULTY_NORMAL);
}

// Runs one update through the real entry points. While a bomb operation is
// pending, one call advances it by one real VSync, as gameplay.c does.
static void M_Tick(CASE *const state)
{
    BOMB_UPDATE result;
    if (!state->bomb_active) {
        if (!Player_BeginUpdate(&state->continuation, &state->sprites)) {
            return;
        }
        result = Bomb_BeginUpdate(
            &state->miko, &state->bomb_assets, (int16_t)m_Player.bomb_frame);
    } else {
        VSyncIrq_Edge();
        result = Bomb_ResumeUpdate();
    }
    if (result == BOMB_UPDATE_WAIT) {
        state->bomb_active = true;
        return;
    }
    state->bomb_active = false;
    Player_FinishBombUpdate(
        &state->continuation, &state->sprites, result == BOMB_UPDATE_DONE);
}

TEST(reset_preserves_position_hits_and_shared_counters)
{
    CASE state;
    M_Case(&state);
    m_Player.left = 607;
    m_Player.is_hit = true;
    m_Player.invincible = true;
    m_Player.invincibility_time = 91;
    m_Player.bomb_frame = UINT32_MAX;
    m_Player.bomb_damaging = true;
    m_GameInput.bomb = true;
    m_GameInput.shot = true;
    m_GameInput.strike = true;
    const int8_t expected[] = { 15, 12, 10, 8, 12 };
    for (int32_t rank = 0; rank < 5; rank++) {
        Player_Reset(rank);
        CHECK_EQ_INT(m_Player.deflect_duration, expected[rank]);
        CHECK_EQ_INT(m_Player.left, 607);
        CHECK(m_Player.is_hit);
        CHECK(m_Player.invincible);
        CHECK_EQ_INT(m_Player.invincibility_time, 91);
        CHECK_EQ_INT(m_Player.bomb_frame, UINT32_MAX);
        CHECK_EQ_INT(m_Player.remaining_bombs, 3);
        CHECK_EQ_INT(m_Player.submode, -1);
        CHECK_EQ_INT(m_Player.dash_direction, 1);
        CHECK(!m_Player.bomb_damaging);
        CHECK(!g_GameInput->bomb);
        CHECK(g_GameInput->shot);
        CHECK(g_GameInput->strike);
    }
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(stationary_draw_gap_and_simultaneous_actions)
{
    CASE state;
    M_Case(&state);
    M_Tick(&state);
    M_Tick(&state);
    m_GameInput.shot = true;
    m_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_SWING_OR_SLIDE);
    CHECK(m_Player.deflecting);
    CHECK(m_Player.invincible_against_orb);
    CHECK(!g_Shots->slots[0].moving);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(normal_shots_spawn_on_following_updates)
{
    CASE state;
    M_Case(&state);
    m_GameInput.shot = true;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_SHOOT);
    CHECK_EQ_INT(m_Player.mode_frame, 0);
    CHECK(!g_Shots->slots[0].moving);
    M_Tick(&state);
    CHECK(g_Shots->slots[0].moving);
    CHECK_EQ_INT(g_Shots->slots[0].left, 312);
    CHECK_EQ_INT(g_Shots->slots[0].top, 368);
    CHECK(g_GameInput->shot);
    M_Tick(&state);
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_REGULAR);
    CHECK(!g_GameInput->shot);
    CHECK_EQ_INT(m_Player.submode, GAME_INPUT_BOTH);
    M_Tick(&state);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
    M_Case(&state);
    m_GameInput.horizontal = GAME_INPUT_RIGHT;
    m_GameInput.shot = true;
    M_Tick(&state);
    M_Tick(&state);
    CHECK(!g_Shots->slots[0].moving);
    M_Tick(&state);
    CHECK(g_Shots->slots[0].moving);
    CHECK_EQ_INT(g_Shots->slots[0].left, 324);
    CHECK_EQ_INT(m_Player.left, 316);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_REGULAR);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(swing_retains_invisible_frames_and_deflection_window)
{
    CASE state;
    M_Case(&state);
    m_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode_frame, 0);
    for (int32_t frame = 0; frame < 3; frame++) {
        M_Tick(&state);
    }
    M_Tick(&state);
    while (m_Player.mode_frame <= 12) {
        M_Tick(&state);
    }
    CHECK(!m_Player.deflecting);
    while (m_Player.mode == PLAYER_MODE_SWING_OR_SLIDE) {
        M_Tick(&state);
    }
    CHECK(!m_Player.invincible_against_orb);
    CHECK(!g_GameInput->strike);
    M_Tick(&state);
    M_Tick(&state);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(slide_requires_release_and_strike_has_combo_priority)
{
    CASE state;
    M_Case(&state);
    m_GameInput.horizontal = GAME_INPUT_RIGHT;
    m_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.left, 308);
    for (int32_t frame = 0; frame < 6; frame++) {
        M_Tick(&state);
    }
    CHECK(!m_Player.combo_enabled);
    m_GameInput.strike = false;
    M_Tick(&state);
    CHECK(m_Player.combo_enabled);
    while (m_Player.mode_frame < 12) {
        M_Tick(&state);
    }
    m_GameInput.horizontal = GAME_INPUT_LEFT;
    m_GameInput.strike = true;
    m_GameInput.shot = true;
    m_Orb.force_frame = 73;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_SPECIAL_FIRST);
    CHECK_EQ_INT(m_Player.submode, 2);
    CHECK_EQ_INT(m_Player.mode_frame, 0);
    CHECK_EQ_INT(m_Orb.force_frame, 73);
    CHECK(m_Player.deflecting);
    CHECK(!m_Player.sliding);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(all_special_attacks_use_source_duration_and_movement)
{
    for (int8_t special = 0; special < 8; special++) {
        CASE state;
        M_Case(&state);
        m_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
        m_Player.mode_frame = 0;
        m_Player.submode = special;
        m_Player.invincible_against_orb = true;
        m_Player.deflecting = true;
        const int32_t duration = special < 4 ? 28 : special < 6 ? 13 : 19;
        const int32_t direction = (special & 1) == 0 ? 1 : -1;
        const int32_t speed = special < 2 ? 2
            : special < 4                 ? 0
            : special < 6                 ? 6
                                          : 0;
        for (int32_t frame = 0; frame <= duration; frame++) {
            M_Tick(&state);
            CHECK_EQ_INT(m_Player.left, 304 + speed * direction * (frame + 1));
            if (frame < duration) {
                CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_SPECIAL_FIRST);
            }
        }
        CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_REGULAR);
        CHECK_EQ_INT(m_Player.mode_frame, 0);
        CHECK(!m_Player.invincible_against_orb);
        CHECK_EQ_INT(m_Player.submode, special);
        M_Tick(&state);
        Mem_Free(g_Video.pages[0]);
        Arena_Free(&state.arena);
    }
}

TEST(shot_combo_spawns_seven_source_offsets_after_drawing)
{
    CASE state;
    M_Case(&state);
    m_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
    m_Player.submode = 6;
    m_Player.deflecting = true;
    const int16_t offsets[] = { -8, 8, 0, -16, 0, 16, 0 };
    int32_t count = 0;
    for (int32_t frame = 0; frame <= 19; frame++) {
        M_Tick(&state);
        if (frame >= 1 && frame % 3 == 1) {
            CHECK_EQ_INT(g_Shots->slots[count].left, 312 + offsets[count]);
            count++;
        }
        if (frame >= 4) {
            CHECK(!m_Player.deflecting);
        }
    }
    CHECK_EQ_INT(count, 7);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(combo_continuation_stops_after_third_attack)
{
    CASE state;
    M_Case(&state);
    m_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
    m_Player.submode = 0;
    m_Player.mode_frame = 28;
    m_Player.combo_enabled = true;
    m_GameInput.strike = true;
    m_GameInput.horizontal = GAME_INPUT_LEFT;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_SPECIAL_SECOND);
    CHECK_EQ_INT(m_Player.submode, 2);
    CHECK_EQ_INT(m_Player.mode_frame, 0);
    m_Player.mode_frame = 28;
    m_Player.combo_enabled = true;
    m_GameInput.strike = false;
    m_GameInput.shot = true;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_SPECIAL_THIRD);
    CHECK_EQ_INT(m_Player.submode, 6);
    m_Player.mode_frame = 19;
    m_Player.combo_enabled = true;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_REGULAR);
    CHECK_EQ_INT(m_Player.submode, 6);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(bomb_boundary_preserves_deferred_start_and_operation_order)
{
    CASE state;
    M_Case(&state);
    m_GameInput.bomb = true;
    m_GameInput.shot = true;
    m_GameInput.strike = true;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.bomb_flag, 2);
    CHECK_EQ_INT(m_Player.remaining_bombs, 3);
    m_Orb.force_frame = 22;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.remaining_bombs, 2);
    CHECK(m_Player.bombing);
    CHECK(m_Player.invincible);
    for (int32_t i = 0; i < 200 && m_Player.bombing; i++) {
        M_Tick(&state);
        if (!state.bomb_active) {
            m_Player.bomb_frame++;
        }
    }
    CHECK(!m_Player.bombing);
    CHECK_EQ_INT(m_Player.bomb_flag, PLAYER_BOMB_NONE);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}

TEST(bomb_exclusion_checks_only_first_special_mode)
{
    for (int8_t mode = PLAYER_MODE_SPECIAL_FIRST;
         mode <= PLAYER_MODE_SPECIAL_THIRD; mode++) {
        CASE state;
        M_Case(&state);
        m_Player.mode = mode;
        m_Player.submode = 2;
        m_GameInput.bomb = true;
        M_Tick(&state);
        CHECK_EQ_INT(
            m_Player.bomb_flag, mode == PLAYER_MODE_SPECIAL_FIRST ? 0 : 2);
        CHECK(g_GameInput->bomb == (mode == PLAYER_MODE_SPECIAL_FIRST));
        CHECK_EQ_INT(m_Player.dash_cycle, 1);
        Mem_Free(g_Video.pages[0]);
        Arena_Free(&state.arena);
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
    CASE state;
    M_Case(&state);
    m_Player.mode = PLAYER_MODE_SHOOT;
    m_Player.mode_frame = 127;
    m_Player.submode = GAME_INPUT_BOTH;
    M_Tick(&state);
    CHECK_EQ_INT(m_Player.mode_frame, -128);
    m_Player.left = INT16_MAX;
    Player_MoveAndClamp(1);
    CHECK_EQ_INT(m_Player.left, 0);
    m_Player.left = 607;
    Player_MoveAndClamp(6);
    CHECK_EQ_INT(m_Player.left, 608);
    Mem_Free(g_Video.pages[0]);
    Arena_Free(&state.arena);
}
