#include "th01/session.h"
#include "th01/stage.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"

#include <string.h>

#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

typedef struct {
    ARENA arena;
    PTN sprites;
} CASE;

static int32_t M_Pellets(void)
{
    int32_t moving = 0;
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        moving += g_Pellets.slots[i].moving;
    }
    return moving;
}

static void M_Init(CASE *const state)
{
    *state = (CASE) {};
    Th01Test_ResetWorld();
    memset(&g_Orb, 0, sizeof(g_Orb));
    g_Orb.left = 500;
    g_Orb.top = 350;
    g_Orb.velocity_y = 2;
    g_Orb.force = -8;
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.left = 0;
    g_Rng = 1;
    Arena_Init(&state->arena, "stage obstacles", 2 * 1024 * 1024);
    Video_Init(Arena_Allocator(&state->arena));
    memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 2, VIDEO_PAGE_SIZE);
    state->sprites =
        (PTN) { .count = 34, .pixels = Arena_Alloc(&state->arena, 34 * 1024) };
    memset(state->sprites.pixels, PTN_TRANSPARENT, 34 * 1024);
    for (int32_t i = 0; i < 34; i++) {
        state->sprites.pixels[i * 1024] = i % 15;
    }
    g_StageObjects.obstacles =
        Arena_AllocZ(&state->arena, 200 * sizeof(STAGE_OBSTACLE));
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_NORMAL;
}

TEST(bumpers_teleport_and_only_reverse_speed_four_even_on_reset)
{
    for (int32_t speed = 0; speed < 5; speed++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            for (int32_t dy = -1; dy <= 1; dy++) {
                CASE state;
                M_Init(&state);
                g_StageObjects.obstacle_count = 1;
                g_StageObjects.obstacles[0] = (STAGE_OBSTACLE) {
                    .left = 96, .top = 128, .type = STAGE_DATA_BUMPER
                };
                g_Orb.left = (int16_t)(96 + dx);
                g_Orb.top = (int16_t)(128 + dy);
                g_Orb.velocity_x = speed;
                Stage_UpdateObstacles(&state.sprites, true);
                CHECK_EQ_INT(g_Orb.top, 128 + (dy <= 0 ? -24 : 24));
                CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 0);
                CHECK_EQ_INT(
                    g_Orb.velocity_x,
                    speed == ORB_X_RIGHT_4 && dx < 0      ? ORB_X_LEFT_4
                        : speed == ORB_X_LEFT_4 && dx > 0 ? ORB_X_RIGHT_4
                                                          : speed);
                CHECK_EQ_INT(g_Orb.force_frame, 0);
                CHECK(g_Orb.force == -1.56);
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
                g_StageObjects.obstacle_count = 1;
                g_StageObjects.obstacles[0] = (STAGE_OBSTACLE) {
                    .left = 96, .top = 128, .type = types[type]
                };
                g_Orb.left = (int16_t)(96 + dx);
                g_Orb.top = (int16_t)(128 + dy);
                g_Orb.velocity_x = ORB_X_LEFT_4;
                const bool collision = type == 0
                    ? dx > -32 && dx < 32 && dy < 0 && dy >= -32
                    : type == 1 ? dx > -32 && dx < 32 && dy > 0 && dy <= 32
                    : type == 2 ? dy > -32 && dy < 32 && dx >= -32 && dx <= 0
                                : dy > -32 && dy < 32 && dx >= 0 && dx <= 32;
                Stage_UpdateObstacles(&state.sprites, false);
                CHECK_EQ_INT(
                    g_StageObjects.obstacles[0].frame, collision ? 2 : 0);
                CHECK_EQ_INT(
                    g_StageObjects.vertical_bar_active, collision && type >= 2);
                CHECK_EQ_INT(
                    g_Orb.velocity_x,
                    collision && type >= 2 ? ORB_X_RIGHT_4 : ORB_X_LEFT_4);
                Arena_Free(&state.arena);
            }
        }
    }
    CASE state;
    M_Init(&state);
    g_StageObjects.obstacle_count = 2;
    g_StageObjects.obstacles[0] = (STAGE_OBSTACLE) {
        .left = 96, .top = 128, .type = STAGE_DATA_BAR_RIGHT
    };
    g_StageObjects.obstacles[1] = g_StageObjects.obstacles[0];
    g_Orb.left = 100;
    g_Orb.top = 128;
    g_Orb.velocity_x = ORB_X_LEFT_4;
    Stage_UpdateObstacles(&state.sprites, false);
    CHECK_EQ_INT(g_Orb.velocity_x, ORB_X_RIGHT_4);
    CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 2);
    CHECK_EQ_INT(g_StageObjects.obstacles[1].frame, 0);
    for (int32_t frame = 0; frame < 6; frame++) {
        Stage_UpdateObstacles(&state.sprites, false);
    }
    CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 0);
    CHECK_EQ_INT(g_StageObjects.obstacles[1].frame, 2);
    CHECK_EQ_INT(g_Orb.velocity_x, ORB_X_LEFT_4);
    Stage_UpdateObstacles(&state.sprites, true);
    CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 0);
    CHECK_EQ_INT(g_StageObjects.obstacles[1].frame, 0);
    CHECK(g_StageObjects.vertical_bar_active);
    Arena_Free(&state.arena);
}

TEST(turret_types_rank_intervals_rng_and_reset_order)
{
    for (int32_t type = 2; type <= 13; type++) {
        for (int32_t rank = 0; rank < 4; rank++) {
            CASE state;
            M_Init(&state);
            g_Session.settings.difficulty = rank;
            g_Session.settings.difficulty = rank;
            g_StageObjects.obstacle_count = 1;
            g_StageObjects.obstacles[0] =
                (STAGE_OBSTACLE) { .left = 96, .top = 128, .type = type };
            const size_t used = state.arena.used;
            const int32_t interval = type < 8 ? 200 : 100;
            const uint32_t initial_rng = g_Rng;
            for (int32_t frame = 1; frame <= interval + 17; frame++) {
                Stage_UpdateObstacles(&state.sprites, false);
                CHECK_EQ_INT(
                    M_Pellets() > 0, rank != 0 && frame >= interval + 10);
                if (rank != 0 && frame == interval) {
                    CHECK_EQ_INT(g_StageObjects.obstacles[0].turret_flag, 1);
                    CHECK_EQ_INT(Th01Test_Pixel(0, 96, 128), 9);
                    CHECK_EQ_INT(Th01Test_Pixel(1, 96, 128), 9);
                }
            }
            CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 0);
            CHECK_EQ_INT(g_StageObjects.obstacles[0].turret_flag, 0);
            CHECK_EQ_INT(state.arena.used, used);
            uint32_t expected = initial_rng;
            if (rank != 0 && (type - 2) % 6 == 1) {
                MasterRng_Next(&expected);
            }
            CHECK_EQ_INT(g_Rng, expected);
            const int32_t pellets = M_Pellets();
            g_StageObjects.obstacles[0].frame = (int16_t)(interval + 9);
            g_StageObjects.obstacles[0].turret_flag = 2;
            Stage_UpdateObstacles(&state.sprites, true);
            CHECK(M_Pellets() > pellets);
            CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 0);
            CHECK_EQ_INT(g_StageObjects.obstacles[0].turret_flag, 0);
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
        if (g_StageObjects.obstacles[slot].type == STAGE_DATA_PORTAL
            && MasterRng_Next(rng) % 4 == 0 && slot != entered) {
            return slot;
        }
        slot++;
        if (loops > 15) {
            return entered;
        }
        if (slot >= g_StageObjects.obstacle_count) {
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
            g_Rng = seed;
            g_StageObjects.obstacle_count = count * 2;
            for (int32_t i = 0; i < count * 2; i++) {
                g_StageObjects.obstacles[i] = (STAGE_OBSTACLE) {
                    .left = (int16_t)(i * 64),
                    .top = 128,
                    .type = i % 2 == 0 ? STAGE_DATA_PORTAL : STAGE_DATA_BUMPER,
                };
            }
            g_Orb.left = 0;
            g_Orb.top = 128;
            uint32_t expected = seed;
            const int32_t destination = M_Destination(&state, 0, &expected);
            const uint32_t selected_rng = expected;
            const int32_t velocity = MasterRng_Next(&expected) % 5;
            const int32_t force = MasterRng_Next(&expected) % 19 - 9;
            const size_t used = state.arena.used;
            for (int32_t frame = 1; frame <= 60; frame++) {
                if (frame == 41) {
                    g_Orb.left = 500;
                    g_Orb.top = 350;
                }
                Stage_UpdateObstacles(&state.sprites, false);
                CHECK_EQ_INT(
                    g_StageObjects.obstacles[0].frame, frame == 60 ? 0 : frame);
                CHECK_EQ_INT(g_Orb.in_portal, frame < 40);
                CHECK_EQ_INT(g_StageObjects.portal_active, frame < 60);
                CHECK_EQ_INT(
                    g_Rng,
                    frame < 20       ? seed
                        : frame < 40 ? selected_rng
                                     : expected);
                if (frame >= 20) {
                    CHECK_EQ_INT(
                        g_StageObjects.destination_left, destination * 64);
                }
                if (frame == 40) {
                    CHECK_EQ_INT(g_Orb.left, destination * 64);
                    CHECK_EQ_INT(g_Orb.top, 128);
                    CHECK_EQ_INT(g_Orb.velocity_x, velocity);
                    CHECK(g_Orb.force == force);
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
    g_StageObjects.obstacle_count = 2;
    g_StageObjects.obstacles[0] =
        (STAGE_OBSTACLE) { .left = 96, .top = 128, .type = STAGE_DATA_PORTAL };
    g_StageObjects.obstacles[1] =
        (STAGE_OBSTACLE) { .left = 96, .top = 128, .type = STAGE_DATA_PORTAL };
    g_StageObjects.destination_left = 96;
    g_StageObjects.destination_top = 128;
    g_Orb.left = 96;
    g_Orb.top = 128;
    state.sprites.pixels[30 * 1024 + 1] = 5;
    Stage_UpdateObstacles(&state.sprites, true);
    CHECK(!g_Orb.in_portal);
    CHECK(!g_StageObjects.portal_active);
    CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 0);
    CHECK_EQ_INT(g_StageObjects.obstacles[1].frame, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 97, 128), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 96, 128), 14);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&state.arena);
}

TEST(orb_composite_reuses_ready_count_and_never_replays_motion)
{
    const int16_t counts[] = { 0, 1, 7, -1 };
    for (int32_t variant = 0; variant < 4; variant++) {
        CASE state;
        M_Init(&state);
        g_Session.stage_id = 4;
        g_Orb.previous_left = 499;
        g_Orb.previous_top = 349;
        StageOrb_Init();
        VSyncIrq_Reset();
        g_VSync.count1 = counts[variant];
        g_VSync.count2 = 44;
        CHECK_EQ_INT(
            Stage_BeginOrbComposite(&state.sprites, nullptr, nullptr),
            variant != 0);
        CHECK_EQ_INT(g_Pellets.interlace_field, variant != 0);
        CHECK_EQ_INT(g_Orb.frames_outside_portal, 1);
        if (variant == 0) {
            const ORB orb = g_Orb;
            CHECK(!Stage_ResumeOrbComposite(&state.sprites, nullptr, nullptr));
            CHECK(memcmp(&orb, &g_Orb, sizeof(ORB)) == 0);
            CHECK_EQ_INT(g_Pellets.interlace_field, false);
            VSyncIrq_Edge();
            CHECK(Stage_ResumeOrbComposite(&state.sprites, nullptr, nullptr));
        }
        CHECK_EQ_INT(g_Pellets.interlace_field, true);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_VSync.count2, variant == 0 ? 45 : 44);
        CHECK_EQ_INT(g_Orb.previous_left, g_Orb.left);
        CHECK_EQ_INT(g_Orb.previous_top, g_Orb.top);
        CHECK(!g_Player.is_hit);
        Arena_Free(&state.arena);
    }
}

TEST(turret_pellets_precede_the_following_portal)
{
    CASE state;
    M_Init(&state);
    g_StageObjects.obstacle_count = 2;
    g_StageObjects.obstacles[0] = (STAGE_OBSTACLE) {
        .left = 0,
        .top = 128,
        .type = STAGE_DATA_TURRET_SLOW_1_AIMED,
        .frame = 209,
        .turret_flag = 2,
    };
    g_StageObjects.obstacles[1] = (STAGE_OBSTACLE) {
        .left = 128,
        .top = 128,
        .type = STAGE_DATA_PORTAL,
        .frame = 19,
    };
    g_StageObjects.portal_active = true;
    g_StageObjects.entered_portal = 1;
    g_Orb.in_portal = true;
    uint32_t expected = g_Rng;
    CHECK_EQ_INT(M_Destination(&state, 1, &expected), 1);
    Stage_UpdateObstacles(&state.sprites, false);
    CHECK_EQ_INT(M_Pellets(), 1);
    CHECK_EQ_INT(g_Rng, expected);
    CHECK_EQ_INT(g_StageObjects.destination_left, 128);
    Arena_Free(&state.arena);
}

TEST(portal_unblock_allows_later_destination_reentry_in_the_same_call)
{
    CASE state;
    M_Init(&state);
    g_StageObjects.obstacle_count = 2;
    g_StageObjects.obstacles[0] = (STAGE_OBSTACLE) {
        .left = 0,
        .top = 128,
        .type = STAGE_DATA_PORTAL,
        .frame = 59,
    };
    g_StageObjects.obstacles[1] = (STAGE_OBSTACLE) {
        .left = 128,
        .top = 128,
        .type = STAGE_DATA_PORTAL,
    };
    g_StageObjects.portal_active = true;
    g_Orb.left = 128;
    g_Orb.top = 128;
    Stage_UpdateObstacles(&state.sprites, false);
    CHECK_EQ_INT(g_StageObjects.obstacles[0].frame, 0);
    CHECK_EQ_INT(g_StageObjects.obstacles[1].frame, 1);
    CHECK_EQ_INT(g_StageObjects.entered_portal, 1);
    CHECK(g_Orb.in_portal);
    CHECK(g_StageObjects.portal_active);
    Arena_Free(&state.arena);
}

TEST(card_completion_sets_hit_after_lethal_test_and_suppresses_orb_put)
{
    CASE state;
    M_Init(&state);
    g_StageObjects.card_count = 1;
    g_StageObjects.cards = Arena_AllocZ(&state.arena, sizeof(STAGE_CARD));
    g_StageObjects.cards[0] = (STAGE_CARD) {
        .left = 0,
        .top = 64,
        .flag = STAGE_CARD_FLIPPING,
        .flip_frame = 29,
    };
    for (int32_t image = 3; image < 7; image++) {
        memset(state.sprites.pixels + image * 1024, 3, 1024);
    }
    g_Orb.previous_left = 500;
    g_Orb.previous_top = 350;
    VSyncIrq_Reset();
    g_VSync.count1 = -1;
    StageOrb_Init();
    CHECK(Stage_BeginOrbComposite(&state.sprites, nullptr, nullptr));
    CHECK(g_Player.is_hit);
    CHECK(g_Stage.cleared);
    CHECK_EQ_INT(g_StageObjects.cards[0].flag, STAGE_CARD_REMOVED);
    CHECK_EQ_INT(Th01Test_Pixel(0, 500, 342), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 500, 350), 2);
    CHECK_EQ_INT(g_Pellets.interlace_field, true);
    Arena_Free(&state.arena);
}
