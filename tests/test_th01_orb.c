#include "th01/orb.h"

#include "test.h"

static ORB M_Orb(void)
{
    ORB orb = { 0 };
    Orb_ResetFlight(&orb);
    return orb;
}

TEST(reset_retains_rotation_portal_and_last_velocity)
{
    ORB orb = { .rotation_frame = 7,
                .in_portal = true,
                .frames_outside_portal = 123,
                .velocity_y = 12.5 };
    Orb_ResetFlight(&orb);
    CHECK_EQ_INT(orb.left, 600);
    CHECK_EQ_INT(orb.top, 280);
    CHECK_EQ_INT(orb.previous_left, 600);
    CHECK_EQ_INT(orb.previous_top, 280);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_LEFT_4);
    CHECK(orb.force == -8.0);
    CHECK_EQ_INT(orb.force_frame, 0);
    CHECK_EQ_INT(orb.rotation_frame, 7);
    CHECK_EQ_INT(orb.frames_outside_portal, 123);
    CHECK(orb.in_portal);
    CHECK(orb.velocity_y == 12.5);
}

TEST(gravity_keeps_stored_clamp_separate_from_extended_displacement)
{
    ORB orb = M_Orb();
    orb.force = -0x1.0000000000001p0;
    orb.force_frame = 25;
    CHECK_EQ_INT(Orb_UpdateVelocityY(&orb), 3);
    CHECK(orb.velocity_y == 4.0);
    orb.force = 0;
    orb.force_frame = 100;
    CHECK_EQ_INT(Orb_UpdateVelocityY(&orb), 20);
    CHECK(orb.velocity_y == 16.0);
    orb.force_frame = -100;
    CHECK_EQ_INT(Orb_UpdateVelocityY(&orb), -20);
    CHECK(orb.velocity_y == -16.0);
    orb.force_frame = -9;
    CHECK_EQ_INT(Orb_UpdateVelocityY(&orb), -1);
    orb.force_frame = INT16_MAX;
    Orb_AdvanceForceFrame(&orb);
    CHECK_EQ_INT(orb.force_frame, INT16_MIN);
    CHECK_EQ_INT(Orb_UpdateVelocityY(&orb), -6553);
}

TEST(force_modes_keep_rounding_frame_and_immediate_rules)
{
    ORB orb = M_Orb();
    uint32_t rng = 42;
    orb.velocity_y = 0x1.3fd7025f9f050p1;
    orb.force_frame = 16;
    Orb_NewForce(&orb, ORB_FORCE_SURFACE, 123.0, &rng);
    CHECK(orb.force == -0x1.f2f34124876e4p0);
    CHECK_EQ_INT(orb.force_frame, 0);
    CHECK_EQ_INT(rng, 42);
    orb.velocity_y = -3.25;
    orb.force_frame = -7;
    Orb_NewForce(&orb, ORB_FORCE_TOP, 123.0, &rng);
    CHECK(orb.force == 4.25);
    orb.velocity_y = 3.0;
    Orb_NewForce(&orb, ORB_FORCE_SHOT, 123.0, &rng);
    CHECK(orb.force == -8.5);
    Orb_NewForce(&orb, ORB_FORCE_IMMEDIATE, -12.25, &rng);
    CHECK(orb.force == -12.25);
    Orb_ReflectY(&orb, &rng);
    CHECK(orb.force == -3.0);
    Orb_ReflectY(&orb, &rng);
    CHECK(orb.force == -3.0);
}

TEST(top_force_preserves_the_sign_of_zero)
{
    ORB orb = M_Orb();
    uint32_t rng = 1;
    uint64_t bits;
    orb.velocity_y = 0.0;
    Orb_NewForce(&orb, ORB_FORCE_TOP, 0.0, &rng);
    memcpy(&bits, &orb.force, sizeof(bits));
    CHECK(bits == (UINT64_C(1) << 63));
    orb.velocity_y = -0.0;
    Orb_NewForce(&orb, ORB_FORCE_TOP, 0.0, &rng);
    memcpy(&bits, &orb.force, sizeof(bits));
    CHECK_EQ_INT(bits, 0);
}

TEST(surface_rng_draws_twice_and_second_can_override_left)
{
    static const struct {
        uint32_t seed, after;
        ORB_VELOCITY_X velocity;
    } cases[] = {
        { 0, 22695478, ORB_X_LEFT_4 },
        { 1, 2156045615, ORB_X_STILL },
        { 4, 4261128730, ORB_X_RIGHT_4 },
        { 1663, 140986813, ORB_X_RIGHT_4 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        ORB orb = M_Orb();
        orb.velocity_x = ORB_X_STILL;
        orb.force_frame = 16;
        uint32_t rng = cases[i].seed;
        Orb_NewForce(&orb, ORB_FORCE_SURFACE, 0.0, &rng);
        CHECK_EQ_INT(orb.velocity_x, cases[i].velocity);
        CHECK_EQ_INT(rng, cases[i].after);
    }
    ORB orb = M_Orb();
    orb.velocity_x = ORB_X_STILL;
    orb.force_frame = 17;
    uint32_t rng = 1663;
    Orb_NewForce(&orb, ORB_FORCE_SURFACE, 0.0, &rng);
    CHECK_EQ_INT(rng, 1663);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_STILL);
}

TEST(wall_reversal_uses_stored_velocity_and_keeps_overshoot)
{
    ORB orb = M_Orb();
    orb.left = 2;
    orb.velocity_x = ORB_X_LEFT_8;
    Orb_MoveX(&orb, ORB_X_LEFT_4);
    CHECK_EQ_INT(orb.left, -2);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_RIGHT_8);
    Orb_MoveX(&orb, ORB_X_RIGHT_8);
    CHECK_EQ_INT(orb.left, 6);
    orb.left = 607;
    Orb_MoveX(&orb, ORB_X_RIGHT_8);
    CHECK_EQ_INT(orb.left, 615);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_LEFT_8);
    orb.left = 2;
    orb.velocity_x = ORB_X_RIGHT_4;
    Orb_MoveX(&orb, ORB_X_LEFT_8);
    CHECK_EQ_INT(orb.left, -6);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_RIGHT_4);
    orb.left = 0;
    orb.velocity_x = ORB_X_LEFT_4;
    Orb_MoveX(&orb, ORB_X_STILL);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_RIGHT_4);
    Orb_ReflectX(&orb);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_LEFT_4);
    Orb_ReflectX(&orb);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_RIGHT_4);
}

TEST(flight_sequence_applies_force_age_before_motion_and_wraps_last_image)
{
    static const int16_t tops[] = { 272, 264, 256, 248, 241, 234,
                                    227, 220, 213, 207, 201, 195 };
    static const int16_t images[] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 0, 0, 0 };
    ORB orb = M_Orb();
    uint32_t rng = 1;
    int16_t combo = 7;
    for (size_t i = 0; i < sizeof(tops) / sizeof(*tops); i++) {
        Orb_AdvanceForceFrame(&orb);
        Orb_AdvanceMotion(&orb, &rng, &combo);
        CHECK_EQ_INT(orb.top, tops[i]);
        CHECK_EQ_INT(orb.left, 596 - (int32_t)i * 4);
        CHECK_EQ_INT(Orb_Image(&orb), images[i]);
    }
    CHECK_EQ_INT(combo, 7);
    CHECK_EQ_INT(rng, 1);
    orb.left = 1;
    orb.rotation_frame = 0;
    Orb_AdvanceMotion(&orb, &rng, &combo);
    CHECK_EQ_INT(orb.left, -3);
    CHECK_EQ_INT(orb.rotation_frame, 9);
    CHECK_EQ_INT(Orb_Image(&orb), 3);
}

TEST(boundary_bounces_use_strict_tests_and_reset_combo_only_at_bottom)
{
    ORB orb = M_Orb();
    uint32_t rng = 1;
    int16_t combo = 7;
    orb.top = 367;
    orb.force = 1;
    Orb_AdvanceMotion(&orb, &rng, &combo);
    CHECK_EQ_INT(orb.top, 368);
    CHECK_EQ_INT(combo, 7);
    CHECK(orb.force == 1.0);
    orb.force_frame = 100;
    Orb_AdvanceMotion(&orb, &rng, &combo);
    CHECK_EQ_INT(orb.top, 368);
    CHECK_EQ_INT(combo, 0);
    CHECK(orb.velocity_y == 16.0);
    CHECK(orb.force == -12.48);
    CHECK_EQ_INT(orb.force_frame, 0);
    orb.top = 65;
    orb.force = -2;
    orb.force_frame = 3;
    combo = 9;
    Orb_AdvanceMotion(&orb, &rng, &combo);
    CHECK_EQ_INT(orb.top, 64);
    CHECK(orb.force == 2.0);
    CHECK_EQ_INT(combo, 9);
}

TEST(portal_suspends_motion_but_preserves_main_loop_force_age)
{
    ORB orb = M_Orb();
    orb.in_portal = true;
    orb.frames_outside_portal = INT16_MAX;
    uint32_t rng = 1;
    int16_t combo = 7;
    Orb_AdvanceForceFrame(&orb);
    Orb_AdvanceMotion(&orb, &rng, &combo);
    CHECK_EQ_INT(orb.left, 600);
    CHECK_EQ_INT(orb.top, 280);
    CHECK_EQ_INT(orb.force_frame, 1);
    CHECK_EQ_INT(orb.rotation_frame, 0);
    CHECK_EQ_INT(orb.frames_outside_portal, INT16_MAX);
    orb.in_portal = false;
    Orb_AdvanceMotion(&orb, &rng, &combo);
    CHECK_EQ_INT(orb.frames_outside_portal, INT16_MIN);
}

TEST(composite_preserves_source_order_and_erases_with_the_new_image)
{
    static const ORB_ACTION_KIND order[] = {
        ORB_ACTION_DELAY_ONE,      ORB_ACTION_PELLETS,
        ORB_ACTION_CARDS_HITTEST,  ORB_ACTION_OBSTACLES,
        ORB_ACTION_PLAYER_HITTEST, ORB_ACTION_ERASE,
        ORB_ACTION_CARDS_RENDER,   ORB_ACTION_PUT,
        ORB_ACTION_DONE,
    };
    ORB orb = M_Orb();
    orb.rotation_frame = 2;
    uint32_t rng = 1;
    int16_t combo = 7;
    ORB_COMPOSITE composite = Orb_BeginComposite(&orb, 0, &rng, &combo);
    for (size_t i = 0; i < sizeof(order) / sizeof(*order); i++) {
        const ORB_ACTION action = Orb_NextAction(&orb, &composite, false);
        CHECK_EQ_INT(action.kind, order[i]);
        if (action.kind == ORB_ACTION_ERASE) {
            CHECK_EQ_INT(action.left, 600);
            CHECK_EQ_INT(action.top, 280);
            CHECK_EQ_INT(action.image, 1);
        }
        if (action.kind == ORB_ACTION_PUT) {
            CHECK_EQ_INT(action.left, 596);
            CHECK_EQ_INT(action.top, 272);
        }
    }
    CHECK_EQ_INT(orb.previous_left, 596);
    CHECK_EQ_INT(orb.previous_top, 272);
}

TEST(composite_reads_portal_changes_after_obstacles_and_updates_previous)
{
    ORB orb = M_Orb();
    uint32_t rng = 1;
    int16_t combo = 7;
    ORB_COMPOSITE composite = Orb_BeginComposite(&orb, 0, &rng, &combo);
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, false).kind, ORB_ACTION_DELAY_ONE);
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, false).kind, ORB_ACTION_PELLETS);
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, false).kind, ORB_ACTION_CARDS_HITTEST);
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, false).kind, ORB_ACTION_OBSTACLES);
    orb.in_portal = true;
    orb.left = 200;
    orb.top = 100;
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, false).kind, ORB_ACTION_CARDS_RENDER);
    CHECK_EQ_INT(Orb_NextAction(&orb, &composite, false).kind, ORB_ACTION_DONE);
    CHECK_EQ_INT(orb.previous_left, 200);
    CHECK_EQ_INT(orb.previous_top, 100);
    composite = Orb_BeginComposite(&orb, 9, &rng, &combo);
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, true).kind, ORB_ACTION_DELAY_ONE);
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, true).kind, ORB_ACTION_PELLETS);
    orb.in_portal = false;
    CHECK_EQ_INT(
        Orb_NextAction(&orb, &composite, true).kind, ORB_ACTION_PLAYER_HITTEST);
    CHECK_EQ_INT(Orb_NextAction(&orb, &composite, true).kind, ORB_ACTION_ERASE);
    CHECK_EQ_INT(Orb_NextAction(&orb, &composite, true).kind, ORB_ACTION_DONE);
}

TEST(player_contact_uses_strict_deltas_and_both_invincibility_flags)
{
    ORB orb = M_Orb();
    uint32_t rng = 1;
    ORB_PLAYER player = { .left = orb.left, .top = orb.top };
    CHECK(Orb_PlayerHitTest(&orb, &player, 0, &rng));
    player.left += 24;
    CHECK(!Orb_PlayerHitTest(&orb, &player, 0, &rng));
    player.left--;
    player.top += 15;
    CHECK(Orb_PlayerHitTest(&orb, &player, 0, &rng));
    player.top++;
    CHECK(!Orb_PlayerHitTest(&orb, &player, 0, &rng));
    player.top--;
    player.invincible = true;
    CHECK(!Orb_PlayerHitTest(&orb, &player, 0, &rng));
    player.invincible = false;
    player.invincible_against_orb = true;
    CHECK(!Orb_PlayerHitTest(&orb, &player, 0, &rng));
    CHECK_EQ_INT(rng, 1);
}

TEST(player_repulsion_keeps_center_rng_bug_and_enum_order)
{
    ORB orb = M_Orb();
    uint32_t rng = 5;
    ORB_PLAYER player = { .left = orb.left,
                          .top = orb.top,
                          .invincible = true };
    CHECK(!Orb_PlayerHitTest(&orb, &player, 3, &rng));
    CHECK_EQ_INT(orb.velocity_x, ORB_X_LEFT_4);
    CHECK(orb.force == -12.0);
    CHECK_EQ_INT(rng, 2099511571);
    rng = 1;
    CHECK(!Orb_PlayerHitTest(&orb, &player, 1, &rng));
    CHECK_EQ_INT(orb.velocity_x, ORB_X_STILL);
    CHECK_EQ_INT(rng, 2156045615);
    static const ORB_VELOCITY_X speeds[] = {
        ORB_X_STILL, ORB_X_LEFT_4, ORB_X_RIGHT_4, ORB_X_RIGHT_8, ORB_X_LEFT_8,
    };
    for (int16_t i = 0; i < 5; i++) {
        CHECK(!Orb_PlayerHitTest(&orb, &player, (int16_t)(100 + i), &rng));
        CHECK_EQ_INT(orb.velocity_x, speeds[i]);
        CHECK(orb.force == -10.0);
    }
    const uint32_t after = rng;
    player.left += 40;
    CHECK(!Orb_PlayerHitTest(&orb, &player, 3, &rng));
    CHECK_EQ_INT(rng, after);
    CHECK_EQ_INT(orb.velocity_x, ORB_X_LEFT_8);
    player.left--;
    player.top += 31;
    CHECK(!Orb_PlayerHitTest(&orb, &player, 25, &rng));
    CHECK_EQ_INT(orb.velocity_x, ORB_X_LEFT_4);
    CHECK(orb.force == -1.0);
}

TEST(render_restores_current_sprite_mask_at_quantized_previous_position)
{
    static uint8_t pages[2][VIDEO_PAGE_SIZE];
    static uint8_t
        pixels[(ORB_FIRST_IMAGE + ORB_IMAGE_COUNT) * PTN_WIDTH * PTN_HEIGHT];
    VIDEO video = { .pages = { pages[0], pages[1] },
                    .drawing_page = 1,
                    .visible_page = 1 };
    PTN sprites = { .count = ORB_FIRST_IMAGE + ORB_IMAGE_COUNT,
                    .pixels = pixels };
    memset(pages[0], 6, VIDEO_PAGE_SIZE);
    memset(pages[1], 9, VIDEO_PAGE_SIZE);
    memset(pixels, PTN_TRANSPARENT, sizeof(pixels));
    pixels[(ORB_FIRST_IMAGE + 1) * PTN_WIDTH * PTN_HEIGHT] = 2;
    pixels[(ORB_FIRST_IMAGE + 1) * PTN_WIDTH * PTN_HEIGHT + 31] = 0;
    const ORB_ACTION action = { ORB_ACTION_ERASE, 19, 70, 1 };
    Orb_RenderAction(&video, &sprites, action);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 16, 70), 9);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 47, 70), 9);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 17, 70), 6);
    CHECK_EQ_INT(video.drawing_page, 0);
    CHECK_EQ_INT(video.visible_page, 1);
    Orb_RenderAction(
        &video, &sprites, (ORB_ACTION) { ORB_ACTION_PUT, 19, 70, 1 });
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 16, 70), 2);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 47, 70), 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 17, 70), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 1, 16, 70), 9);
}

TEST(render_wall_overshoot_wraps_rows_within_visible_page)
{
    static uint8_t pages[2][VIDEO_PAGE_SIZE];
    static uint8_t
        pixels[(ORB_FIRST_IMAGE + ORB_IMAGE_COUNT) * PTN_WIDTH * PTN_HEIGHT];
    VIDEO video = { .pages = { pages[0], pages[1] } };
    PTN sprites = { .count = ORB_FIRST_IMAGE + ORB_IMAGE_COUNT,
                    .pixels = pixels };
    memset(pages[0], 6, VIDEO_PAGE_SIZE);
    memset(pages[1], 9, VIDEO_PAGE_SIZE);
    memset(pixels, PTN_TRANSPARENT, sizeof(pixels));
    pixels[ORB_FIRST_IMAGE * PTN_WIDTH * PTN_HEIGHT] = 2;
    pixels[ORB_FIRST_IMAGE * PTN_WIDTH * PTN_HEIGHT + 31] = 3;
    Orb_RenderAction(
        &video, &sprites, (ORB_ACTION) { ORB_ACTION_PUT, -4, 64, 0 });
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 632, 63), 2);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 23, 64), 3);
    Orb_RenderAction(
        &video, &sprites, (ORB_ACTION) { ORB_ACTION_ERASE, -4, 64, 0 });
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 632, 63), 9);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 23, 64), 9);
    Orb_RenderAction(
        &video, &sprites, (ORB_ACTION) { ORB_ACTION_PUT, 612, 64, 0 });
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 608, 64), 2);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 639, 64), 3);
    Orb_RenderAction(
        &video, &sprites, (ORB_ACTION) { ORB_ACTION_PUT, 616, 64, 0 });
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 616, 64), 2);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 7, 65), 3);
    Orb_RenderAction(
        &video, &sprites, (ORB_ACTION) { ORB_ACTION_PUT, -8, 0, 0 });
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 23, 0), 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 632, 399), 6);
}
