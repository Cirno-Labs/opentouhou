#include "th01/shots.h"
#include "th01/rng.h"

#include <string.h>

#include "test.h"

static SHOTS_ACTION m_Actions[64];
static int32_t m_Count;

static void M_Action(void *user, const SHOTS_ACTION action)
{
    int32_t *const count = user;
    m_Actions[(*count)++] = action;
}

static SHOTS_OUTPUT M_Output(void)
{
    m_Count = 0;
    return (SHOTS_OUTPUT) { M_Action, &m_Count };
}

TEST(requires_shot_image_before_ticks)
{
    uint8_t pixels[2048] = {};
    PTN sprites = { .count = 1, .pixels = pixels };
    CHECK(!Result_Ignore(Shots_ValidateSprites(&sprites)));
    sprites.count = 2;
    CHECK(IS_OK(Shots_ValidateSprites(&sprites)));
    sprites.pixels = nullptr;
    CHECK(!Result_Ignore(Shots_ValidateSprites(&sprites)));
}

TEST(spawn_uses_first_free_slot_and_only_success_plays_effect)
{
    memset(&g_Shots, 0, sizeof(g_Shots));
    const SHOTS_OUTPUT out = M_Output();
    CHECK(!Shots_Add(-1, 368, out));
    CHECK(!Shots_Add(640, 368, out));
    g_Shots.slots[0].decay_frame = 3;
    for (int32_t i = 1; i < 8; i++) {
        CHECK(Shots_Add(i == 7 ? 639 : 0, 368, out));
        CHECK(g_Shots.slots[i].moving);
    }
    CHECK(!Shots_Add(320, 368, out));
    CHECK_EQ_INT(m_Count, 7);
    for (int32_t i = 0; i < m_Count; i++) {
        CHECK_EQ_INT(m_Actions[i].kind, SHOTS_ACTION_EFFECT);
        CHECK_EQ_INT(m_Actions[i].effect, 1);
    }
    g_Shots.slots[3].moving = false;
    CHECK(Shots_Add(16, -3, out));
    CHECK_EQ_INT(g_Shots.slots[3].left, 16);
    CHECK_EQ_INT(g_Shots.slots[3].top, -3);
}

TEST(orb_collision_has_strict_bounds_and_extra_motion)
{
    g_Rng = 1;
    for (int16_t dx = -17; dx <= 33; dx++) {
        for (int16_t dy = -17; dy <= 33; dy++) {
            memset(&g_Shots, 0, sizeof(g_Shots));
            g_Shots.slots[0] = (SHOT) { 300 + dx, 200 + dy, true, 0 };
            memset(&g_Orb, 0, sizeof(g_Orb));
            g_Orb.left = 300;
            g_Orb.top = 200;
            g_Orb.force_frame = 90;
            g_Orb.rotation_frame = 7;
            const bool expected = dx > -16 && dx < 32 && dy > -16 && dy < 32;
            CHECK(Shots_HitTestOrb(0) == expected);
            CHECK(g_Shots.slots[0].moving == !expected);
            CHECK_EQ_INT(g_Orb.rotation_frame, 7);
            CHECK_EQ_INT(g_Orb.force_frame, expected ? 0 : 90);
            if (expected) {
                CHECK_EQ_INT(g_Orb.left, dx > 8 ? 296 : dx == 8 ? 300 : 304);
                CHECK(g_Orb.top < 200);
            }
        }
    }
    CHECK_EQ_INT(g_Rng, 1);
}

TEST(update_tests_orb_before_top_clip_and_skips_portals)
{
    g_Rng = 1;
    memset(&g_Shots, 0, sizeof(g_Shots));
    g_Shots.slots[0] = (SHOT) { 300, 70, true, 0 };
    g_Shots.slots[1] = (SHOT) { 400, 76, true, 0 };
    memset(&g_Orb, 0, sizeof(g_Orb));
    g_Orb.left = 300;
    g_Orb.top = 64;
    SHOTS_OUTPUT out = M_Output();
    Shots_UpdateAndRender(out);
    CHECK(!g_Shots.slots[0].moving);
    CHECK(g_Shots.slots[1].moving);
    CHECK_EQ_INT(m_Count, 3);
    CHECK_EQ_INT(m_Actions[0].kind, SHOTS_ACTION_RESTORE);
    CHECK_EQ_INT(m_Actions[0].top, 70);
    CHECK_EQ_INT(m_Actions[1].kind, SHOTS_ACTION_RESTORE);
    CHECK_EQ_INT(m_Actions[2].kind, SHOTS_ACTION_PUT);
    CHECK_EQ_INT(m_Actions[2].top, 64);
    g_Shots.slots[0] = (SHOT) { 300, 70, true, 0 };
    memset(&g_Orb, 0, sizeof(g_Orb));
    g_Orb.left = 300;
    g_Orb.top = 64;
    g_Orb.in_portal = true;
    out = M_Output();
    Shots_UpdateAndRender(out);
    CHECK_EQ_INT(g_Orb.left, 300);
    CHECK_EQ_INT(g_Orb.top, 64);
    CHECK(!g_Shots.slots[0].moving);
}

TEST(pellet_margins_are_inclusive_and_first_hit_wins)
{
    for (int16_t dx = -15; dx <= 7; dx++) {
        for (int16_t dy = -15; dy <= 7; dy++) {
            memset(&g_Shots, 0, sizeof(g_Shots));
            g_Shots.slots[0] = (SHOT) { 300 + dx, 200 + dy, true, 0 };
            g_Shots.slots[1] = (SHOT) { 300 + dx, 200 + dy, true, 0 };
            const SHOTS_OUTPUT out = M_Output();
            const bool expected = dx >= -14 && dx <= 6 && dy >= -14 && dy <= 6;
            CHECK(Shots_HitTestPellet(300, 200, out) == expected);
            CHECK(g_Shots.slots[1].moving);
            CHECK_EQ_INT(m_Count, expected ? 2 : 0);
            if (expected) {
                CHECK_EQ_INT(m_Actions[0].kind, SHOTS_ACTION_RESTORE);
                CHECK_EQ_INT(m_Actions[1].effect, 16);
                CHECK_EQ_INT(g_Shots.slots[0].decay_frame, 1);
            }
        }
    }
}

TEST(boss_inclusive_hit_and_decay_image_sequence)
{
    memset(&g_Shots, 0, sizeof(g_Shots));
    g_Shots.slots[0] = (SHOT) { 340, 220, true, 0 };
    SHOTS_OUTPUT out = M_Output();
    CHECK(Shots_HitTestBoss(300, 200, 40, 20, out));
    memset(&g_Orb, 0, sizeof(g_Orb));
    g_Orb.in_portal = true;
    g_Rng = 1;
    for (int32_t frame = 1; frame <= 7; frame++) {
        out = M_Output();
        Shots_UpdateAndRender(out);
        CHECK_EQ_INT(m_Count, frame == 7 ? 1 : 2);
        CHECK_EQ_INT(m_Actions[0].kind, SHOTS_ACTION_ERASE);
        CHECK_EQ_INT(m_Actions[0].quarter, frame < 4 ? 1 : 2);
        if (frame != 7) {
            CHECK_EQ_INT(m_Actions[1].quarter, frame < 3 ? 1 : 2);
        }
    }
    CHECK_EQ_INT(g_Shots.slots[0].decay_frame, 0);
    CHECK(!g_Shots.slots[0].moving);
}

TEST(reset_erases_moving_and_decaying_in_slot_order)
{
    memset(&g_Shots, 0, sizeof(g_Shots));
    g_Shots.slots[0] = (SHOT) { 10, 100, true, 0 };
    g_Shots.slots[1] = (SHOT) { 20, 200, false, 3 };
    g_Shots.slots[2] = (SHOT) { 30, 300, true, 0 };
    const SHOTS_OUTPUT out = M_Output();
    Shots_EraseAndReset(out);
    CHECK_EQ_INT(m_Count, 3);
    for (int32_t i = 0; i < 3; i++) {
        CHECK_EQ_INT(m_Actions[i].kind, SHOTS_ACTION_RESTORE);
        CHECK_EQ_INT(m_Actions[i].left, (i + 1) * 10);
        CHECK(!g_Shots.slots[i].moving);
        CHECK_EQ_INT(g_Shots.slots[i].decay_frame, 0);
    }
}
