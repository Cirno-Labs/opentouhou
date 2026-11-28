#include "tests/test.h"
#include "tests/th01/th01_video.h"
#include "th01/orb.c"
#include "th01/orb.h"
#include "th01/rng.h"

#include "core/base/memory.h"

#include <string.h>

static ORB M_Orb(void)
{
    memset(&m_Orb, 0, sizeof(m_Orb));
    Orb_ResetFlight();
    return m_Orb;
}

TEST(reset_retains_rotation_portal_and_last_velocity)
{
    memset(&m_Orb, 0, sizeof(m_Orb));
    m_Orb.rotation_frame = 7;
    m_Orb.in_portal = true;
    m_Orb.frames_outside_portal = 123;
    m_Orb.velocity_y = 12.5;
    Orb_ResetFlight();
    CHECK_EQ_INT(m_Orb.left, 600);
    CHECK_EQ_INT(m_Orb.top, 280);
    CHECK_EQ_INT(m_Orb.previous_left, 600);
    CHECK_EQ_INT(m_Orb.previous_top, 280);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_LEFT_4);
    CHECK(m_Orb.force == -8.0);
    CHECK_EQ_INT(m_Orb.force_frame, 0);
    CHECK_EQ_INT(m_Orb.rotation_frame, 7);
    CHECK_EQ_INT(m_Orb.frames_outside_portal, 123);
    CHECK(m_Orb.in_portal);
    CHECK(m_Orb.velocity_y == 12.5);
}

TEST(gravity_keeps_stored_clamp_separate_from_extended_displacement)
{
    m_Orb = M_Orb();
    m_Orb.force = -0x1.0000000000001p0;
    m_Orb.force_frame = 25;
    CHECK_EQ_INT(Orb_UpdateVelocityY(), 3);
    CHECK(m_Orb.velocity_y == 4.0);
    m_Orb.force = 0;
    m_Orb.force_frame = 100;
    CHECK_EQ_INT(Orb_UpdateVelocityY(), 20);
    CHECK(m_Orb.velocity_y == 16.0);
    m_Orb.force_frame = -100;
    CHECK_EQ_INT(Orb_UpdateVelocityY(), -20);
    CHECK(m_Orb.velocity_y == -16.0);
    m_Orb.force_frame = -9;
    CHECK_EQ_INT(Orb_UpdateVelocityY(), -1);
    m_Orb.force_frame = INT16_MAX;
    Orb_AdvanceForceFrame();
    CHECK_EQ_INT(m_Orb.force_frame, INT16_MIN);
    CHECK_EQ_INT(Orb_UpdateVelocityY(), -6553);
}

TEST(force_modes_keep_rounding_frame_and_immediate_rules)
{
    m_Orb = M_Orb();
    g_Rng = 42;
    m_Orb.velocity_y = 0x1.3fd7025f9f050p1;
    m_Orb.force_frame = 16;
    Orb_NewForce(ORB_FORCE_SURFACE, 123.0);
    CHECK(m_Orb.force == -0x1.f2f34124876e4p0);
    CHECK_EQ_INT(m_Orb.force_frame, 0);
    CHECK_EQ_INT(g_Rng, 42);
    m_Orb.velocity_y = -3.25;
    m_Orb.force_frame = -7;
    Orb_NewForce(ORB_FORCE_TOP, 123.0);
    CHECK(m_Orb.force == 4.25);
    m_Orb.velocity_y = 3.0;
    Orb_NewForce(ORB_FORCE_SHOT, 123.0);
    CHECK(m_Orb.force == -8.5);
    Orb_NewForce(ORB_FORCE_IMMEDIATE, -12.25);
    CHECK(m_Orb.force == -12.25);
    Orb_ReflectY();
    CHECK(m_Orb.force == -3.0);
    Orb_ReflectY();
    CHECK(m_Orb.force == -3.0);
}

TEST(top_force_preserves_the_sign_of_zero)
{
    m_Orb = M_Orb();
    g_Rng = 1;
    uint64_t bits;
    m_Orb.velocity_y = 0.0;
    Orb_NewForce(ORB_FORCE_TOP, 0.0);
    memcpy(&bits, &m_Orb.force, sizeof(bits));
    CHECK(bits == (UINT64_C(1) << 63));
    m_Orb.velocity_y = -0.0;
    Orb_NewForce(ORB_FORCE_TOP, 0.0);
    memcpy(&bits, &m_Orb.force, sizeof(bits));
    CHECK_EQ_INT(bits, 0);
}

TEST(surface_rng_draws_twice_and_second_can_override_left)
{
    static const struct {
        uint32_t seed;
        uint32_t after;
        ORB_VELOCITY_X velocity;
    } cases[] = {
        { 0, 22695478, ORB_X_LEFT_4 },
        { 1, 2156045615, ORB_X_STILL },
        { 4, 4261128730, ORB_X_RIGHT_4 },
        { 1663, 140986813, ORB_X_RIGHT_4 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        m_Orb = M_Orb();
        m_Orb.velocity_x = ORB_X_STILL;
        m_Orb.force_frame = 16;
        g_Rng = cases[i].seed;
        Orb_NewForce(ORB_FORCE_SURFACE, 0.0);
        CHECK_EQ_INT(m_Orb.velocity_x, cases[i].velocity);
        CHECK_EQ_INT(g_Rng, cases[i].after);
    }
    m_Orb = M_Orb();
    m_Orb.velocity_x = ORB_X_STILL;
    m_Orb.force_frame = 17;
    g_Rng = 1663;
    Orb_NewForce(ORB_FORCE_SURFACE, 0.0);
    CHECK_EQ_INT(g_Rng, 1663);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_STILL);
}

TEST(wall_reversal_uses_stored_velocity_and_keeps_overshoot)
{
    m_Orb = M_Orb();
    m_Orb.left = 2;
    m_Orb.velocity_x = ORB_X_LEFT_8;
    Orb_MoveX(ORB_X_LEFT_4);
    CHECK_EQ_INT(m_Orb.left, -2);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_RIGHT_8);
    Orb_MoveX(ORB_X_RIGHT_8);
    CHECK_EQ_INT(m_Orb.left, 6);
    m_Orb.left = 607;
    Orb_MoveX(ORB_X_RIGHT_8);
    CHECK_EQ_INT(m_Orb.left, 615);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_LEFT_8);
    m_Orb.left = 2;
    m_Orb.velocity_x = ORB_X_RIGHT_4;
    Orb_MoveX(ORB_X_LEFT_8);
    CHECK_EQ_INT(m_Orb.left, -6);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_RIGHT_4);
    m_Orb.left = 0;
    m_Orb.velocity_x = ORB_X_LEFT_4;
    Orb_MoveX(ORB_X_STILL);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_RIGHT_4);
    Orb_ReflectX();
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_LEFT_4);
    Orb_ReflectX();
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_RIGHT_4);
}

TEST(flight_sequence_applies_force_age_before_motion_and_wraps_last_image)
{
    static const int16_t tops[] = { 272, 264, 256, 248, 241, 234,
                                    227, 220, 213, 207, 201, 195 };
    static const int16_t images[] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 0, 0, 0 };
    m_Orb = M_Orb();
    g_Rng = 1;
    int16_t combo = 7;
    for (size_t i = 0; i < sizeof(tops) / sizeof(*tops); i++) {
        Orb_AdvanceForceFrame();
        Orb_AdvanceMotion(&combo);
        CHECK_EQ_INT(m_Orb.top, tops[i]);
        CHECK_EQ_INT(m_Orb.left, 596 - (int32_t)i * 4);
        CHECK_EQ_INT(Orb_Image(), images[i]);
    }
    CHECK_EQ_INT(combo, 7);
    CHECK_EQ_INT(g_Rng, 1);
    m_Orb.left = 1;
    m_Orb.rotation_frame = 0;
    Orb_AdvanceMotion(&combo);
    CHECK_EQ_INT(m_Orb.left, -3);
    CHECK_EQ_INT(m_Orb.rotation_frame, 9);
    CHECK_EQ_INT(Orb_Image(), 3);
}

TEST(boundary_bounces_use_strict_tests_and_reset_combo_only_at_bottom)
{
    m_Orb = M_Orb();
    g_Rng = 1;
    int16_t combo = 7;
    m_Orb.top = 367;
    m_Orb.force = 1;
    Orb_AdvanceMotion(&combo);
    CHECK_EQ_INT(m_Orb.top, 368);
    CHECK_EQ_INT(combo, 7);
    CHECK(m_Orb.force == 1.0);
    m_Orb.force_frame = 100;
    Orb_AdvanceMotion(&combo);
    CHECK_EQ_INT(m_Orb.top, 368);
    CHECK_EQ_INT(combo, 0);
    CHECK(m_Orb.velocity_y == 16.0);
    CHECK(m_Orb.force == -12.48);
    CHECK_EQ_INT(m_Orb.force_frame, 0);
    m_Orb.top = 65;
    m_Orb.force = -2;
    m_Orb.force_frame = 3;
    combo = 9;
    Orb_AdvanceMotion(&combo);
    CHECK_EQ_INT(m_Orb.top, 64);
    CHECK(m_Orb.force == 2.0);
    CHECK_EQ_INT(combo, 9);
}

TEST(portal_suspends_motion_but_preserves_main_loop_force_age)
{
    m_Orb = M_Orb();
    m_Orb.in_portal = true;
    m_Orb.frames_outside_portal = INT16_MAX;
    g_Rng = 1;
    int16_t combo = 7;
    Orb_AdvanceForceFrame();
    Orb_AdvanceMotion(&combo);
    CHECK_EQ_INT(m_Orb.left, 600);
    CHECK_EQ_INT(m_Orb.top, 280);
    CHECK_EQ_INT(m_Orb.force_frame, 1);
    CHECK_EQ_INT(m_Orb.rotation_frame, 0);
    CHECK_EQ_INT(m_Orb.frames_outside_portal, INT16_MAX);
    m_Orb.in_portal = false;
    Orb_AdvanceMotion(&combo);
    CHECK_EQ_INT(m_Orb.frames_outside_portal, INT16_MIN);
}

TEST(player_contact_uses_strict_deltas_and_both_invincibility_flags)
{
    m_Orb = M_Orb();
    g_Rng = 1;
    ORB_PLAYER player = { .left = m_Orb.left, .top = m_Orb.top };
    CHECK(Orb_PlayerHitTest(&player, 0));
    player.left += 24;
    CHECK(!Orb_PlayerHitTest(&player, 0));
    player.left--;
    player.top += 15;
    CHECK(Orb_PlayerHitTest(&player, 0));
    player.top++;
    CHECK(!Orb_PlayerHitTest(&player, 0));
    player.top--;
    player.invincible = true;
    CHECK(!Orb_PlayerHitTest(&player, 0));
    player.invincible = false;
    player.invincible_against_orb = true;
    CHECK(!Orb_PlayerHitTest(&player, 0));
    CHECK_EQ_INT(g_Rng, 1);
}

TEST(player_repulsion_keeps_center_rng_bug_and_enum_order)
{
    m_Orb = M_Orb();
    g_Rng = 5;
    ORB_PLAYER player = { .left = m_Orb.left,
                          .top = m_Orb.top,
                          .invincible = true };
    CHECK(!Orb_PlayerHitTest(&player, 3));
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_LEFT_4);
    CHECK(m_Orb.force == -12.0);
    CHECK_EQ_INT(g_Rng, 2099511571);
    g_Rng = 1;
    CHECK(!Orb_PlayerHitTest(&player, 1));
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_STILL);
    CHECK_EQ_INT(g_Rng, 2156045615);
    static const ORB_VELOCITY_X speeds[] = {
        ORB_X_STILL, ORB_X_LEFT_4, ORB_X_RIGHT_4, ORB_X_RIGHT_8, ORB_X_LEFT_8,
    };
    for (int16_t i = 0; i < 5; i++) {
        CHECK(!Orb_PlayerHitTest(&player, (int16_t)(100 + i)));
        CHECK_EQ_INT(m_Orb.velocity_x, speeds[i]);
        CHECK(m_Orb.force == -10.0);
    }
    const uint32_t after = g_Rng;
    player.left += 40;
    CHECK(!Orb_PlayerHitTest(&player, 3));
    CHECK_EQ_INT(g_Rng, after);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_LEFT_8);
    player.left--;
    player.top += 31;
    CHECK(!Orb_PlayerHitTest(&player, 25));
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_LEFT_4);
    CHECK(m_Orb.force == -1.0);
}

TEST(render_restores_current_sprite_mask_at_quantized_previous_position)
{
    static uint8_t
        pixels[(ORB_FIRST_IMAGE + ORB_IMAGE_COUNT) * PTN_WIDTH * PTN_HEIGHT];
    Video_Init(&g_HeapAllocator);
    Video_SelectDrawingPage(1);
    Video_SelectVisiblePage(1);
    PTN sprites = { .count = ORB_FIRST_IMAGE + ORB_IMAGE_COUNT,
                    .pixels = pixels };
    memset(g_Video->pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video->pages[1], 9, VIDEO_PAGE_SIZE);
    memset(pixels, PTN_TRANSPARENT, sizeof(pixels));
    pixels[(ORB_FIRST_IMAGE + 1) * PTN_WIDTH * PTN_HEIGHT] = 2;
    pixels[(ORB_FIRST_IMAGE + 1) * PTN_WIDTH * PTN_HEIGHT + 31] = 0;
    Orb_Erase(&sprites, 19, 70, 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 70), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 47, 70), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 17, 70), 6);
    CHECK_EQ_INT(g_Video->drawing_page, 0);
    CHECK_EQ_INT(g_Video->visible_page, 1);
    Orb_Put(&sprites, 19, 70, 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 70), 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 47, 70), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 17, 70), 6);
    CHECK_EQ_INT(Th01Test_Pixel(1, 16, 70), 9);
    Mem_Free(g_Video->pages[0]);
}

TEST(render_wall_overshoot_wraps_rows_within_visible_page)
{
    static uint8_t
        pixels[(ORB_FIRST_IMAGE + ORB_IMAGE_COUNT) * PTN_WIDTH * PTN_HEIGHT];
    Video_Init(&g_HeapAllocator);
    PTN sprites = { .count = ORB_FIRST_IMAGE + ORB_IMAGE_COUNT,
                    .pixels = pixels };
    memset(g_Video->pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video->pages[1], 9, VIDEO_PAGE_SIZE);
    memset(pixels, PTN_TRANSPARENT, sizeof(pixels));
    pixels[ORB_FIRST_IMAGE * PTN_WIDTH * PTN_HEIGHT] = 2;
    pixels[ORB_FIRST_IMAGE * PTN_WIDTH * PTN_HEIGHT + 31] = 3;
    Orb_Put(&sprites, -4, 64, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 632, 63), 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 23, 64), 3);
    Orb_Erase(&sprites, -4, 64, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 632, 63), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 23, 64), 9);
    Orb_Put(&sprites, 612, 64, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 608, 64), 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 64), 3);
    Orb_Put(&sprites, 616, 64, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 616, 64), 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 7, 65), 3);
    Orb_Put(&sprites, -8, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 23, 0), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 632, 399), 6);
    Mem_Free(g_Video->pages[0]);
}
