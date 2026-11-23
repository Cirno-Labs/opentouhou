#include "th01/bomb.h"

#include "pc98/zun/math16.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

typedef struct {
    ARENA arena;
    VIDEO video;
    PALETTE_STATE palette;
    PALETTE stage;
    PLAYER player;
    PTN miko;
    BOMB_ASSETS assets;
    BOMB bomb;
    FRAME_VSYNC vsync;
    int32_t decays, effects[10];
} CASE;

static void M_Decay(void *const user)
{
    CASE *const state = user;
    state->decays++;
    CHECK(state->player.invincible);
}

static void M_Effect(void *const user, const int16_t effect)
{
    CASE *const state = user;
    state->effects[effect]++;
}

static BOMB_CONTEXT M_Context(CASE *const state)
{
    return (BOMB_CONTEXT) {
        &state->video, &state->palette, &state->player,
        &state->miko,  &state->assets,  &state->stage,
        M_Decay,       M_Effect,        state,
    };
}

static void M_Init(CASE *const state)
{
    *state = (CASE) {};
    Arena_Init(&state->arena, "bomb", 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&state->arena);
    Video_Init(&state->video, allocator);
    Palette_Init(&state->palette);
    state->stage = state->palette.saved;
    state->player.left = 304;
    state->miko =
        (PTN) { .count = 4, .pixels = Arena_Alloc(&state->arena, 4 * 1024) };
    memset(state->miko.pixels, 15, 4 * 1024);
    for (int32_t i = 0; i < 2; i++) {
        const int32_t count = i == 0 ? 8 : 1;
        state->assets.kuji[i] =
            (GRC) { 32, 32, count, Arena_Alloc(&state->arena, count * 1024) };
        memset(state->assets.kuji[i].coverage, 1, count * 1024);
    }
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        state->video.pages[1][i] = (uint8_t)(i % 16);
    }
}

TEST(full_bomb_preserves_waits_palette_events_and_frame_ownership)
{
    CASE state;
    M_Init(&state);
    const BOMB_CONTEXT context = M_Context(&state);
    CHECK(IS_OK(Bomb_ValidateAssets(&state.assets)));
    int32_t waits = 0;
    const size_t used = state.arena.used;
    for (int16_t frame = 0; frame <= 140; frame++) {
        state.player.bomb_frame = (uint32_t)frame;
        state.player.invincible = false;
        const uint64_t edge = state.vsync.edge;
        BOMB_UPDATE result =
            Bomb_BeginUpdate(&state.bomb, &context, frame, &state.vsync);
        CHECK(state.player.invincible);
        CHECK_EQ_INT(state.player.bomb_frame, frame);
        const bool must_wait = frame == 140 || (frame > 50 && frame % 4 == 0);
        CHECK_EQ_INT(result == BOMB_UPDATE_WAIT, must_wait);
        CHECK_EQ_INT(state.vsync.edge, edge);
        if (must_wait) {
            waits++;
            const uint8_t before = state.video.pages[0][184 * 640 + 304];
            const int16_t scroll = state.video.scroll_top;
            CHECK_EQ_INT(
                Bomb_ResumeUpdate(&state.bomb, &context, &state.vsync),
                BOMB_UPDATE_WAIT);
            CHECK_EQ_INT(state.video.pages[0][184 * 640 + 304], before);
            CHECK_EQ_INT(state.video.scroll_top, scroll);
            Frame_VSync(&state.vsync);
            result = Bomb_ResumeUpdate(&state.bomb, &context, &state.vsync);
            const int16_t offsets[] = { 392, 0, 384, 0 };
            CHECK_EQ_INT(
                state.video.scroll_top,
                frame == 140 ? 0 : offsets[frame % 16 / 4]);
            CHECK(Frame_DelayReady(&state.vsync, 1));
        }
        CHECK_EQ_INT(
            result, frame == 140 ? BOMB_UPDATE_DONE : BOMB_UPDATE_RUNNING);
        CHECK_EQ_INT(state.player.bomb_damaging, frame >= 50 && frame < 140);
        if (frame == 2) {
            CHECK_EQ_INT(state.palette.saved.colors[7][0], 14);
            CHECK_EQ_INT(state.palette.saved.colors[7][1], 14);
            CHECK_EQ_INT(state.palette.saved.colors[7][2], 15);
            CHECK_EQ_INT(state.bomb.left[0], Math16_PolarX(304, 252, 2));
            CHECK_EQ_INT(state.bomb.top[9], Math16_PolarY(184, 162, -2));
        }
        if (frame == 40) {
            CHECK_EQ_INT(state.bomb.tint_offset, -9);
        }
    }
    CHECK_EQ_INT(waits, 23);
    CHECK_EQ_INT(state.arena.used, used);
    CHECK_EQ_INT(state.decays, 5);
    CHECK_EQ_INT(state.effects[8], 1);
    CHECK_EQ_INT(state.effects[9], 8);
    CHECK_EQ_INT(state.player.invincibility_time, 100);
    CHECK(
        memcmp(state.video.pages[0], state.video.pages[1], VIDEO_PAGE_SIZE)
        == 0);
    CHECK(memcmp(&state.palette.saved, &state.stage, sizeof(PALETTE)) == 0);
    const int16_t previous_x = state.bomb.left[0];
    const int16_t previous_y = state.bomb.top[0];
    Bomb_BeginUpdate(&state.bomb, &context, 0, &state.vsync);
    CHECK_EQ_INT(state.bomb.left[0], previous_x);
    CHECK_EQ_INT(state.bomb.top[0], previous_y);
    CHECK_EQ_INT(state.decays, 6);
    Arena_Free(&state.arena);
}

TEST(damage_uses_full_unsigned_frame_and_last_life)
{
    PLAYER player = { .bomb_damaging = true };
    CHECK(Bomb_DealsDamage(&player, 0, 0));
    CHECK(Bomb_DealsDamage(&player, 0, 65550));
    CHECK(!Bomb_DealsDamage(&player, 0, 65536));
    CHECK(!Bomb_DealsDamage(&player, 1, 100));
    CHECK(!Bomb_DealsDamage(&player, -1, 100));
    player.bomb_damaging = false;
    CHECK(!Bomb_DealsDamage(&player, 0, 100));
}

TEST(square_inversion_rejects_edges_and_toggles_only_one_plane)
{
    CASE state;
    M_Init(&state);
    memset(state.video.pages[0], 6, VIDEO_PAGE_SIZE);
    CHECK(!Video_Invert32(&state.video, -1, 0, 1));
    CHECK(!Video_Invert32(&state.video, 0, -1, 1));
    CHECK(!Video_Invert32(&state.video, 608, 0, 1));
    CHECK(!Video_Invert32(&state.video, 0, 368, 1));
    CHECK(Video_Invert32(&state.video, 607, 367, 1));
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 600, 367), 7);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 631, 398), 7);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 632, 398), 6);
    CHECK(Video_Invert32(&state.video, 607, 367, 2));
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 600, 367), 5);
    CHECK_EQ_INT(
        Th01Test_Pixel(&state.video, 1, 600, 367), (367 * 640 + 600) % 16);
    Video_SetScroll(&state.video, 400);
    CHECK_EQ_INT(state.video.scroll_top, 0);
    Video_SetScroll(&state.video, -8);
    CHECK_EQ_INT(state.video.scroll_top, 392);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 600, 367), 5);
    Arena_Free(&state.arena);
}

typedef struct {
    CASE *state;
    int32_t hud, draws;
} PLAYER_CASE;

static void M_PlayerAction(void *const user, const PLAYER_ACTION action)
{
    PLAYER_CASE *const actor = user;
    if (action.kind == PLAYER_ACTION_HUD_BOMBS) {
        actor->hud++;
    } else {
        CHECK_EQ_INT(action.kind, PLAYER_ACTION_PTN_PUT);
        actor->draws++;
        Player_RenderAction(
            &actor->state->video,
            &(PLAYER_SPRITES) { .miko = &actor->state->miko }, nullptr, action);
    }
}

TEST(player_continuation_spans_real_bomb_scroll_wait)
{
    CASE state;
    M_Init(&state);
    state.player.remaining_bombs = 2;
    state.player.bomb_flag = PLAYER_BOMB_START;
    GAME_INPUT input = { .shot = true, .strike = true, .bomb = true };
    ORB orb = { .left = 500, .top = 100 };
    SHOTS shots = {};
    uint32_t rng = 1;
    PLAYER_CASE actor = { .state = &state };
    const PLAYER_OUTPUT output = { .action = M_PlayerAction, .user = &actor };
    PLAYER_BOMB_CONTINUATION continuation = {};
    const BOMB_CONTEXT context = M_Context(&state);
    CHECK(Player_BeginUpdate(
        &continuation, &state.player, &input, &orb, &shots, &rng, output));
    CHECK_EQ_INT(actor.hud, 1);
    CHECK_EQ_INT(state.player.remaining_bombs, 1);
    CHECK_EQ_INT(
        Bomb_BeginUpdate(&state.bomb, &context, 0, &state.vsync),
        BOMB_UPDATE_RUNNING);
    Player_FinishBombUpdate(&continuation, false);
    state.player.bomb_frame = 64;
    input.shot = input.strike = true;
    CHECK(Player_BeginUpdate(
        &continuation, &state.player, &input, &orb, &shots, &rng, output));
    const int8_t dash = state.player.dash_cycle;
    CHECK_EQ_INT(
        Bomb_BeginUpdate(&state.bomb, &context, 64, &state.vsync),
        BOMB_UPDATE_WAIT);
    CHECK_EQ_INT(
        Bomb_ResumeUpdate(&state.bomb, &context, &state.vsync),
        BOMB_UPDATE_WAIT);
    CHECK(continuation.pending);
    CHECK(input.shot && input.strike);
    CHECK_EQ_INT(actor.draws, 0);
    Frame_VSync(&state.vsync);
    CHECK_EQ_INT(
        Bomb_ResumeUpdate(&state.bomb, &context, &state.vsync),
        BOMB_UPDATE_RUNNING);
    Player_FinishBombUpdate(&continuation, false);
    CHECK(!continuation.pending);
    CHECK_EQ_INT(state.player.dash_cycle, dash);
    CHECK_EQ_INT(actor.hud, 1);
    CHECK_EQ_INT(actor.draws, 1);
    CHECK_EQ_INT(state.player.bomb_frame, 64);
    CHECK(!input.shot && !input.strike);
    CHECK(Frame_DelayReady(&state.vsync, 1));
    Arena_Free(&state.arena);
}
