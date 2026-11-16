#include "th01/pellets.h"

#include "test.h"

#include <string.h>

typedef struct {
    PELLETS pellets;
    PELLET_SHARED shared;
    PLAYER player;
    ORB orb;
    SHOTS shots;
    VIDEO video;
    PELLET_ASSETS assets;
    PELLET_CONTEXT context;
    uint32_t rng;
    SHOTS_ACTION actions[64];
    int32_t count;
} FIXTURE;

static void M_Action(void *const user, const SHOTS_ACTION action)
{
    FIXTURE *const f = user;
    CHECK(f->count < 64);
    f->actions[f->count++] = action;
}

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Video_Init(&f->video, &g_HeapAllocator);
    f->orb = (ORB) { .left = 500, .top = 100, .in_portal = true };
    f->player.left = 300;
    f->rng = 1;
    f->context =
        (PELLET_CONTEXT) { &f->video, &f->assets, &f->player, &f->orb,
                           &f->shots, &f->rng,    &f->shared, { M_Action, f } };
}

static void M_Free(FIXTURE *const f)
{
    Mem_Free(f->video.pages[0]);
}

static void M_Add(FIXTURE *const f, const int16_t left, const int16_t top)
{
    Pellets_AddSingle(
        &f->pellets, &f->shared, 0, left, top, 64, 16, PELLET_REGULAR, 0, 0, 0);
}

TEST(all_groups_have_source_counts_angles_and_rng)
{
    PLAYER player = { .left = 300 };
    PELLET_SHARED shared = {};
    for (int32_t group = 1; group <= 30; group++) {
        PELLETS pellets = {};
        uint32_t rng = 1;
        Pellets_AddGroup(
            &pellets, &shared, &player, &rng, 0, 300, 100, (PELLET_GROUP)group,
            64);
        const int32_t count = group == 1 || group == 12 || group >= 29 ? 1
            : group < 12                                               ? 0
                         : 2 + ((group - 13) % 8) / 2;
        CHECK_EQ_INT(pellets.alive_count_excluding_clouds_after_reset, count);
        for (int32_t i = 0; i < 100; i++) {
            CHECK(pellets.slots[i].moving == (i < count));
        }
        if (group < 29) {
            CHECK_EQ_INT(rng, 1);
        } else {
            uint32_t expected_rng = 1;
            Math_Random(&expected_rng);
            CHECK_EQ_INT(rng, expected_rng);
        }
    }
    const int32_t angles[4] = { 72, 56, 88, 40 };
    PELLETS pellets = {};
    uint32_t rng = 1;
    Pellets_AddGroup(
        &pellets, &shared, &player, &rng, 0, 300, 100, PELLET_GROUP_FOUR_WIDE,
        64);
    for (int32_t i = 0; i < 4; i++) {
        const MATH_VECTOR v = Math_Vector(64, angles[i]);
        CHECK_EQ_INT(pellets.slots[i].velocity.x, v.x);
        CHECK_EQ_INT(pellets.slots[i].velocity.y, v.y);
    }
}

TEST(spawn_reuses_selective_fields_and_obeys_real_pool)
{
    PELLETS pellets = {};
    PELLET_SHARED shared = {};
    PLAYER player = {};
    uint32_t rng = 1;
    pellets.slots[0].angle = 96;
    pellets.slots[0].sling_direction = 2;
    pellets.slots[0].not_rendered = true;
    Pellets_AddSingle(
        &pellets, &shared, 0, -10, 40, 16, 0, PELLET_SLING_AIMED, 32, 0, 0);
    CHECK(pellets.slots[0].moving);
    CHECK(pellets.slots[0].not_rendered);
    CHECK_EQ_INT(pellets.slots[0].angle, 96);
    CHECK_EQ_INT(pellets.slots[0].sling_direction, 2);
    CHECK_EQ_INT(pellets.slots[0].previous_left, -1);
    Pellets_ResetMoving(&pellets);
    Pellets_AddGroup(
        &pellets, &shared, &player, &rng, 0, 100, 100, PELLET_GROUP_SINGLE, 16);
    CHECK_EQ_INT(pellets.slots[0].motion_type, PELLET_SLING_AIMED);
    CHECK_EQ_INT(pellets.slots[0].speed, 32);
    for (int32_t i = 0; i < 100; i++) {
        pellets.slots[i].moving = true;
    }
    pellets.alive_count_excluding_clouds_after_reset = 0;
    Pellets_AddGroup(
        &pellets, &shared, &player, &rng, 0, 100, 100, PELLET_GROUP_RANDOM_WIDE,
        16);
    CHECK_EQ_INT(rng, 1);
    pellets.slots[99].moving = false;
    pellets.alive_count_excluding_clouds_after_reset = 100;
    Pellets_AddGroup(
        &pellets, &shared, &player, &rng, 0, 100, 100, PELLET_GROUP_RANDOM_WIDE,
        16);
    CHECK(pellets.slots[99].moving);
    CHECK_EQ_INT(pellets.alive_count_excluding_clouds_after_reset, 101);
}

TEST(cloud_reset_retains_delay_and_activation_moves_same_tick)
{
    FIXTURE f;
    M_Init(&f);
    f.pellets.spawn_with_cloud = true;
    M_Add(&f, 100, 100);
    Pellets_ResetMoving(&f.pellets);
    CHECK_EQ_INT(f.pellets.alive_count_excluding_clouds_after_reset, 0);
    PELLET *const p = &f.pellets.slots[0];
    for (int32_t i = 2; i <= 8; i++) {
        Pellets_UpdateAndRender(&f.pellets, &f.context);
        CHECK_EQ_INT(p->cloud_frame, i);
        CHECK(!p->moving);
        CHECK_EQ_INT(p->age, 0);
    }
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK(p->moving);
    CHECK_EQ_INT(p->cloud_frame, 0);
    CHECK_EQ_INT(p->top, 101 * 16);
    CHECK_EQ_INT(p->age, 1);
    M_Free(&f);
}

TEST(shot_hit_then_clip_decrements_twice_and_updates_shots_once)
{
    FIXTURE f;
    M_Init(&f);
    M_Add(&f, 100, 399);
    f.shots.slots[0] = (SHOT) { .left = 100, .top = 399, .moving = true };
    f.shots.slots[1] = (SHOT) { .left = 200, .top = 200, .moving = true };
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK(!f.pellets.slots[0].moving);
    CHECK_EQ_INT(f.pellets.alive_count_excluding_clouds_after_reset, -1);
    CHECK_EQ_INT(f.shots.slots[0].decay_frame, 2);
    CHECK_EQ_INT(f.shots.slots[1].top, 188);
    CHECK_EQ_INT(f.actions[0].kind, SHOTS_ACTION_RESTORE);
    CHECK_EQ_INT(f.actions[1].kind, SHOTS_ACTION_EFFECT);
    CHECK_EQ_INT(f.actions[1].effect, 16);
    M_Free(&f);
}

TEST(interlace_skips_orb_but_not_movement_age_or_lethal_hits)
{
    FIXTURE f;
    M_Init(&f);
    f.pellets.interlace = true;
    M_Add(&f, 310, 368);
    f.orb.left = 310;
    f.orb.top = 369;
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK(f.player.is_hit);
    CHECK(!f.pellets.slots[0].moving);
    CHECK_EQ_INT(f.pellets.slots[0].age, 1);
    CHECK_EQ_INT(f.shared.destroy_score_delta, 0);
    M_Free(&f);
}

TEST(stale_idle_shot_can_suppress_orb_and_deflection)
{
    FIXTURE f;
    M_Init(&f);
    M_Add(&f, 100, 100);
    f.shots.slots[0] = (SHOT) { .left = 115, .top = 101 };
    f.orb.left = 100;
    f.orb.top = 101;
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK(f.pellets.slots[0].not_rendered);
    CHECK_EQ_INT(f.pellets.slots[0].decay_frame, 0);
    CHECK_EQ_INT(f.shared.destroy_score_delta, 0);
    f.shots.slots[0].top = 0;
    f.pellets.slots[0].velocity = (MATH_VECTOR) { -3, -3 };
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK_EQ_INT(f.pellets.slots[0].decay_frame, 2);
    CHECK_EQ_INT(f.pellets.slots[0].velocity.x, -2);
    CHECK_EQ_INT(f.shared.destroy_score_delta, 10);
    M_Free(&f);
}

TEST(deflection_is_invisible_and_decay_ends_after_frame_21)
{
    FIXTURE f;
    M_Init(&f);
    M_Add(&f, 299, 350);
    f.player.deflecting = true;
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK(f.pellets.slots[0].not_rendered);
    CHECK_EQ_INT(f.pellets.slots[0].velocity.x, -128);
    CHECK_EQ_INT(f.pellets.slots[0].velocity.y, 0);
    f.pellets.slots[0].velocity = (MATH_VECTOR) { 7, -7 };
    f.shared.destroy_score_delta = 65530;
    Pellets_Decay(&f.pellets, &f.shared);
    CHECK_EQ_INT(f.pellets.slots[0].velocity.x, 4);
    CHECK_EQ_INT(f.pellets.slots[0].velocity.y, -4);
    CHECK_EQ_INT(f.shared.destroy_score_delta, 4);
    for (int32_t i = 2; i <= 21; i++) {
        Pellets_UpdateAndRender(&f.pellets, &f.context);
        CHECK(f.pellets.slots[0].moving);
        CHECK_EQ_INT(f.pellets.slots[0].decay_frame, i);
    }
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK(!f.pellets.slots[0].moving);
    CHECK_EQ_INT(f.pellets.slots[0].decay_frame, 0);
    M_Free(&f);
}

TEST(sling_retains_direction_and_retargets_only_after_full_turn)
{
    FIXTURE f;
    M_Init(&f);
    Pellets_AddSingle(
        &f.pellets, &f.shared, 0, 300, 180, 0, 32, PELLET_SLING_AIMED, 16, 0,
        0);
    PELLET *const p = &f.pellets.slots[0];
    p->sling_direction = 2;
    p->angle = 248;
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    const MATH_VECTOR v = Math_Vector(16, 248);
    CHECK_EQ_INT(p->velocity.x, v.y);
    CHECK_EQ_INT(p->velocity.y, v.x);
    CHECK_EQ_INT(p->angle, 256);
    CHECK_EQ_INT(p->from_group, 0);
    Pellets_UpdateAndRender(&f.pellets, &f.context);
    CHECK_EQ_INT(p->angle, 0);
    CHECK_EQ_INT(p->from_group, PELLET_GROUP_SINGLE_AIMED);
    CHECK_EQ_INT(f.rng, 1);
    M_Free(&f);
}

TEST(speed_tuning_wraps_product_and_helpers_keep_asymmetric_cap)
{
    PELLETS pellets = {};
    PELLET_SHARED shared = { .speed = 20 };
    Pellets_AddSingle(
        &pellets, &shared, 3, 100, 100, 0, 2000, PELLET_REGULAR, 0, 0, 0);
    CHECK_EQ_INT(pellets.slots[0].velocity.x, 1389);
    shared.speed = 20;
    Pellets_SpeedLower(&shared, 5, -3);
    CHECK_EQ_INT(shared.speed, 5);
    Pellets_SpeedLower(&shared, 5, -30);
    CHECK_EQ_INT(shared.speed, -15);
    Pellets_SpeedRaise(&shared, 40);
    CHECK_EQ_INT(shared.speed, 20);
}
