#include "th01/stage.h"

#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    ARENA arena;
    STAGE_CONTEXT context;
    STAGE_OBJECTS objects;
    STAGE_GAMEPLAY game;
    ORB orb;
    PLAYER player;
    VIDEO video;
    PTN sprites;
    uint32_t rng;
    int32_t pellets, updates;
    STAGE_PELLET_SPAWN last;
} CASE;

static void M_Pellet(void *const user, const STAGE_PELLET_SPAWN spawn)
{
    CASE *const state = user;
    state->pellets++;
    state->last = spawn;
    MasterRng_Next(&state->rng);
}

static void M_Update(void *const user)
{
    CASE *const state = user;
    state->updates++;
    MasterRng_Next(&state->rng);
}

static void M_Init(CASE *const state)
{
    *state = (CASE) {
        .rng = 1,
        .orb = { .left = 500, .top = 350, .velocity_y = 2, .force = -8 },
        .player = { .left = 0 }
    };
    Arena_Init(&state->arena, "stage obstacles", 2 * 1024 * 1024);
    Video_Init(&state->video, Arena_Allocator(&state->arena));
    memset(state->video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(state->video.pages[1], 2, VIDEO_PAGE_SIZE);
    state->sprites =
        (PTN) { .count = 34, .pixels = Arena_Alloc(&state->arena, 34 * 1024) };
    memset(state->sprites.pixels, PTN_TRANSPARENT, 34 * 1024);
    for (int32_t i = 0; i < 34; i++) {
        state->sprites.pixels[i * 1024] = i % 15;
    }
    state->objects.obstacles =
        Arena_AllocZ(&state->arena, 200 * sizeof(STAGE_OBSTACLE));
    state->context = (STAGE_CONTEXT) {
        .objects = &state->objects,
        .gameplay = &state->game,
        .orb = &state->orb,
        .player = &state->player,
        .rng = &state->rng,
        .video = &state->video,
        .sprites = &state->sprites,
        .difficulty = SETTINGS_DIFFICULTY_NORMAL,
        .pellet_group = M_Pellet,
        .pellets_update = M_Update,
        .user = state,
    };
}

TEST(bumpers_teleport_and_only_reverse_speed_four_even_on_reset)
{
    for (int32_t speed = 0; speed < 5; speed++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            for (int32_t dy = -1; dy <= 1; dy++) {
                CASE state;
                M_Init(&state);
                state.objects.obstacle_count = 1;
                state.objects.obstacles[0] = (STAGE_OBSTACLE) {
                    .left = 96, .top = 128, .type = STAGE_DATA_BUMPER
                };
                state.orb.left = (int16_t)(96 + dx);
                state.orb.top = (int16_t)(128 + dy);
                state.orb.velocity_x = speed;
                Stage_UpdateObstacles(&state.context, true);
                CHECK_EQ_INT(state.orb.top, 128 + (dy <= 0 ? -24 : 24));
                CHECK_EQ_INT(state.objects.obstacles[0].frame, 0);
                CHECK_EQ_INT(
                    state.orb.velocity_x,
                    speed == ORB_X_RIGHT_4 && dx < 0      ? ORB_X_LEFT_4
                        : speed == ORB_X_LEFT_4 && dx > 0 ? ORB_X_RIGHT_4
                                                          : speed);
                CHECK_EQ_INT(state.orb.force_frame, 0);
                CHECK(state.orb.force == -1.56);
                Arena_Free(&state.arena);
            }
        }
    }
}

TEST(bar_boundaries_shared_block_and_ordered_release)
{
    const STAGE_DATA_OBSTACLE types[] = { STAGE_DATA_BAR_TOP,
                                          STAGE_DATA_BAR_BOTTOM,
                                          STAGE_DATA_BAR_LEFT,
                                          STAGE_DATA_BAR_RIGHT };
    for (int32_t type = 0; type < 4; type++) {
        for (int32_t dx = -33; dx <= 33; dx++) {
            for (int32_t dy = -33; dy <= 33; dy++) {
                CASE state;
                M_Init(&state);
                state.objects.obstacle_count = 1;
                state.objects.obstacles[0] = (STAGE_OBSTACLE) {
                    .left = 96, .top = 128, .type = types[type]
                };
                state.orb.left = (int16_t)(96 + dx);
                state.orb.top = (int16_t)(128 + dy);
                state.orb.velocity_x = ORB_X_LEFT_4;
                const bool collision = type == 0
                    ? dx > -32 && dx < 32 && dy < 0 && dy >= -32
                    : type == 1 ? dx > -32 && dx < 32 && dy > 0 && dy <= 32
                    : type == 2 ? dy > -32 && dy < 32 && dx >= -32 && dx <= 0
                                : dy > -32 && dy < 32 && dx >= 0 && dx <= 32;
                Stage_UpdateObstacles(&state.context, false);
                CHECK_EQ_INT(
                    state.objects.obstacles[0].frame, collision ? 2 : 0);
                CHECK_EQ_INT(
                    state.objects.vertical_bar_active, collision && type >= 2);
                CHECK_EQ_INT(
                    state.orb.velocity_x,
                    collision && type >= 2 ? ORB_X_RIGHT_4 : ORB_X_LEFT_4);
                Arena_Free(&state.arena);
            }
        }
    }
    CASE state;
    M_Init(&state);
    state.objects.obstacle_count = 2;
    state.objects.obstacles[0] = (STAGE_OBSTACLE) {
        .left = 96, .top = 128, .type = STAGE_DATA_BAR_RIGHT
    };
    state.objects.obstacles[1] = state.objects.obstacles[0];
    state.orb.left = 100;
    state.orb.top = 128;
    state.orb.velocity_x = ORB_X_LEFT_4;
    Stage_UpdateObstacles(&state.context, false);
    CHECK_EQ_INT(state.orb.velocity_x, ORB_X_RIGHT_4);
    CHECK_EQ_INT(state.objects.obstacles[0].frame, 2);
    CHECK_EQ_INT(state.objects.obstacles[1].frame, 0);
    for (int32_t frame = 0; frame < 6; frame++) {
        Stage_UpdateObstacles(&state.context, false);
    }
    CHECK_EQ_INT(state.objects.obstacles[0].frame, 0);
    CHECK_EQ_INT(state.objects.obstacles[1].frame, 2);
    CHECK_EQ_INT(state.orb.velocity_x, ORB_X_LEFT_4);
    Stage_UpdateObstacles(&state.context, true);
    CHECK_EQ_INT(state.objects.obstacles[0].frame, 0);
    CHECK_EQ_INT(state.objects.obstacles[1].frame, 0);
    CHECK(state.objects.vertical_bar_active);
    Arena_Free(&state.arena);
}

TEST(turret_types_rank_intervals_callback_rng_and_reset_order)
{
    const int32_t groups[] = { 12, 29, 21, 23, 25, 27 };
    for (int32_t type = 2; type <= 13; type++) {
        for (int32_t rank = 0; rank < 4; rank++) {
            CASE state;
            M_Init(&state);
            state.context.difficulty = rank;
            state.objects.obstacle_count = 1;
            state.objects.obstacles[0] =
                (STAGE_OBSTACLE) { .left = 96, .top = 128, .type = type };
            const size_t used = state.arena.used;
            const int32_t interval = type < 8 ? 200 : 100;
            const uint32_t initial_rng = state.rng;
            for (int32_t frame = 1; frame <= interval + 17; frame++) {
                Stage_UpdateObstacles(&state.context, false);
                CHECK_EQ_INT(
                    state.pellets, rank != 0 && frame >= interval + 10);
                if (rank != 0 && frame == interval) {
                    CHECK_EQ_INT(state.objects.obstacles[0].turret_flag, 1);
                    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 96, 128), 9);
                    CHECK_EQ_INT(Video_ReadPixel(&state.video, 1, 96, 128), 9);
                }
            }
            CHECK_EQ_INT(state.objects.obstacles[0].frame, 0);
            CHECK_EQ_INT(state.objects.obstacles[0].turret_flag, 0);
            CHECK_EQ_INT(state.arena.used, used);
            uint32_t expected = initial_rng;
            if (rank != 0) {
                MasterRng_Next(&expected);
                CHECK_EQ_INT(state.last.group, groups[(type - 2) % 6]);
                CHECK_EQ_INT(state.last.left, 108);
                CHECK_EQ_INT(state.last.top, 140);
                CHECK_EQ_INT(state.last.speed, 56);
            }
            CHECK_EQ_INT(state.rng, expected);
            state.objects.obstacles[0].frame = (int16_t)(interval + 9);
            state.objects.obstacles[0].turret_flag = 2;
            Stage_UpdateObstacles(&state.context, true);
            CHECK_EQ_INT(state.pellets, rank == 0 ? 1 : 2);
            CHECK_EQ_INT(state.objects.obstacles[0].frame, 0);
            CHECK_EQ_INT(state.objects.obstacles[0].turret_flag, 0);
            Arena_Free(&state.arena);
        }
    }
}

static int32_t M_Destination(
    CASE *const state, const int32_t entered, uint32_t *const rng)
{
    int32_t slot = 0;
    int32_t loops = 0;
    for (;;) {
        if (state->objects.obstacles[slot].type == STAGE_DATA_PORTAL
            && MasterRng_Next(rng) % 4 == 0 && slot != entered) {
            return slot;
        }
        slot++;
        if (loops > 15) {
            return entered;
        }
        if (slot >= state->objects.obstacle_count) {
            slot = 0;
            loops++;
        }
    }
}

TEST(portal_counts_destinations_rng_and_exact_keyframes)
{
    for (int32_t count = 1; count <= 4; count++) {
        for (uint32_t seed = 1; seed < 40; seed++) {
            CASE state;
            M_Init(&state);
            state.rng = seed;
            state.objects.obstacle_count = count * 2;
            for (int32_t i = 0; i < count * 2; i++) {
                state.objects.obstacles[i] = (STAGE_OBSTACLE) {
                    .left = (int16_t)(i * 64),
                    .top = 128,
                    .type = i % 2 == 0 ? STAGE_DATA_PORTAL : STAGE_DATA_BUMPER,
                };
            }
            state.orb.left = 0;
            state.orb.top = 128;
            uint32_t expected = seed;
            const int32_t destination = M_Destination(&state, 0, &expected);
            const uint32_t selected_rng = expected;
            const int32_t velocity = MasterRng_Next(&expected) % 5;
            const int32_t force = MasterRng_Next(&expected) % 19 - 9;
            const size_t used = state.arena.used;
            for (int32_t frame = 1; frame <= 60; frame++) {
                if (frame == 41) {
                    state.orb.left = 500;
                    state.orb.top = 350;
                }
                Stage_UpdateObstacles(&state.context, false);
                CHECK_EQ_INT(
                    state.objects.obstacles[0].frame, frame == 60 ? 0 : frame);
                CHECK_EQ_INT(state.orb.in_portal, frame < 40);
                CHECK_EQ_INT(state.objects.portal_active, frame < 60);
                CHECK_EQ_INT(
                    state.rng,
                    frame < 20       ? seed
                        : frame < 40 ? selected_rng
                                     : expected);
                if (frame >= 20) {
                    CHECK_EQ_INT(
                        state.objects.destination_left, destination * 64);
                }
                if (frame == 40) {
                    CHECK_EQ_INT(state.orb.left, destination * 64);
                    CHECK_EQ_INT(state.orb.top, 128);
                    CHECK_EQ_INT(state.orb.velocity_x, velocity);
                    CHECK(state.orb.force == force);
                }
                CHECK_EQ_INT(state.arena.used, used);
            }
            Arena_Free(&state.arena);
        }
    }
}

TEST(portal_reset_keeps_overlay_artifact_and_can_enter_later_portal)
{
    CASE state;
    M_Init(&state);
    state.objects.obstacle_count = 2;
    state.objects.obstacles[0] =
        (STAGE_OBSTACLE) { .left = 96, .top = 128, .type = STAGE_DATA_PORTAL };
    state.objects.obstacles[1] =
        (STAGE_OBSTACLE) { .left = 96, .top = 128, .type = STAGE_DATA_PORTAL };
    state.objects.destination_left = 96;
    state.objects.destination_top = 128;
    state.orb.left = 96;
    state.orb.top = 128;
    state.sprites.pixels[30 * 1024 + 1] = 5;
    Stage_UpdateObstacles(&state.context, true);
    CHECK(!state.orb.in_portal);
    CHECK(!state.objects.portal_active);
    CHECK_EQ_INT(state.objects.obstacles[0].frame, 0);
    CHECK_EQ_INT(state.objects.obstacles[1].frame, 0);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 97, 128), 5);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 96, 128), 14);
    CHECK_EQ_INT(state.video.drawing_page, 0);
    Arena_Free(&state.arena);
}

TEST(orb_composite_reuses_ready_count_and_never_replays_motion)
{
    const int16_t counts[] = { 0, 1, 7, -1 };
    for (int32_t variant = 0; variant < 4; variant++) {
        CASE state;
        M_Init(&state);
        state.context.stage_id = 4;
        state.orb.previous_left = 499;
        state.orb.previous_top = 349;
        STAGE_ORB_COMPOSITE composite = {};
        FRAME_VSYNC vsync = { .count1 = counts[variant], .count2 = 44 };
        CHECK_EQ_INT(
            Stage_BeginOrbComposite(&composite, &state.context, &vsync),
            variant != 0);
        CHECK_EQ_INT(state.updates, variant != 0);
        CHECK_EQ_INT(state.orb.frames_outside_portal, 1);
        if (variant == 0) {
            const ORB orb = state.orb;
            CHECK(
                !Stage_ResumeOrbComposite(&composite, &state.context, &vsync));
            CHECK(memcmp(&orb, &state.orb, sizeof(ORB)) == 0);
            CHECK_EQ_INT(state.updates, 0);
            Frame_VSync(&vsync);
            CHECK(Stage_ResumeOrbComposite(&composite, &state.context, &vsync));
        }
        CHECK_EQ_INT(state.updates, 1);
        CHECK_EQ_INT(vsync.count1, 0);
        CHECK_EQ_INT(vsync.count2, variant == 0 ? 45 : 44);
        CHECK_EQ_INT(state.orb.previous_left, state.orb.left);
        CHECK_EQ_INT(state.orb.previous_top, state.orb.top);
        CHECK(!state.player.is_hit);
        Arena_Free(&state.arena);
    }
}

TEST(turret_callbacks_consume_rng_before_the_following_portal)
{
    CASE state;
    M_Init(&state);
    state.objects.obstacle_count = 2;
    state.objects.obstacles[0] = (STAGE_OBSTACLE) {
        .left = 0,
        .top = 128,
        .type = STAGE_DATA_TURRET_SLOW_1_AIMED,
        .frame = 209,
        .turret_flag = 2,
    };
    state.objects.obstacles[1] = (STAGE_OBSTACLE) {
        .left = 128,
        .top = 128,
        .type = STAGE_DATA_PORTAL,
        .frame = 19,
    };
    state.objects.portal_active = true;
    state.objects.entered_portal = 1;
    state.orb.in_portal = true;
    uint32_t expected = state.rng;
    MasterRng_Next(&expected);
    CHECK_EQ_INT(M_Destination(&state, 1, &expected), 1);
    Stage_UpdateObstacles(&state.context, false);
    CHECK_EQ_INT(state.pellets, 1);
    CHECK_EQ_INT(state.rng, expected);
    CHECK_EQ_INT(state.objects.destination_left, 128);
    Arena_Free(&state.arena);
}

TEST(portal_unblock_allows_later_destination_reentry_in_the_same_call)
{
    CASE state;
    M_Init(&state);
    state.objects.obstacle_count = 2;
    state.objects.obstacles[0] = (STAGE_OBSTACLE) {
        .left = 0,
        .top = 128,
        .type = STAGE_DATA_PORTAL,
        .frame = 59,
    };
    state.objects.obstacles[1] = (STAGE_OBSTACLE) {
        .left = 128,
        .top = 128,
        .type = STAGE_DATA_PORTAL,
    };
    state.objects.portal_active = true;
    state.orb.left = 128;
    state.orb.top = 128;
    Stage_UpdateObstacles(&state.context, false);
    CHECK_EQ_INT(state.objects.obstacles[0].frame, 0);
    CHECK_EQ_INT(state.objects.obstacles[1].frame, 1);
    CHECK_EQ_INT(state.objects.entered_portal, 1);
    CHECK(state.orb.in_portal);
    CHECK(state.objects.portal_active);
    Arena_Free(&state.arena);
}

TEST(card_completion_sets_hit_after_lethal_test_and_suppresses_orb_put)
{
    CASE state;
    M_Init(&state);
    state.objects.card_count = 1;
    state.objects.cards = Arena_AllocZ(&state.arena, sizeof(STAGE_CARD));
    state.objects.cards[0] = (STAGE_CARD) {
        .left = 0,
        .top = 64,
        .flag = STAGE_CARD_FLIPPING,
        .flip_frame = 29,
    };
    for (int32_t image = 3; image < 7; image++) {
        memset(state.sprites.pixels + image * 1024, 3, 1024);
    }
    state.orb.previous_left = 500;
    state.orb.previous_top = 350;
    FRAME_VSYNC vsync = { .count1 = -1 };
    STAGE_ORB_COMPOSITE composite = {};
    CHECK(Stage_BeginOrbComposite(&composite, &state.context, &vsync));
    CHECK(state.player.is_hit);
    CHECK(state.game.cleared);
    CHECK_EQ_INT(state.objects.cards[0].flag, STAGE_CARD_REMOVED);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 500, 342), 6);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 500, 350), 2);
    CHECK_EQ_INT(state.updates, 1);
    Arena_Free(&state.arena);
}
