#include "th01/shots.h"

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
    SHOTS shots = {};
    const SHOTS_OUTPUT out = M_Output();
    CHECK(!Shots_Add(&shots, -1, 368, out));
    CHECK(!Shots_Add(&shots, 640, 368, out));
    shots.slots[0].decay_frame = 3;
    for (int32_t i = 1; i < 8; i++) {
        CHECK(Shots_Add(&shots, i == 7 ? 639 : 0, 368, out));
        CHECK(shots.slots[i].moving);
    }
    CHECK(!Shots_Add(&shots, 320, 368, out));
    CHECK_EQ_INT(m_Count, 7);
    for (int32_t i = 0; i < m_Count; i++) {
        CHECK_EQ_INT(m_Actions[i].kind, SHOTS_ACTION_EFFECT);
        CHECK_EQ_INT(m_Actions[i].effect, 1);
    }
    shots.slots[3].moving = false;
    CHECK(Shots_Add(&shots, 16, -3, out));
    CHECK_EQ_INT(shots.slots[3].left, 16);
    CHECK_EQ_INT(shots.slots[3].top, -3);
}

TEST(orb_collision_has_strict_bounds_and_extra_motion)
{
    uint32_t rng = 1;
    for (int16_t dx = -17; dx <= 33; dx++) {
        for (int16_t dy = -17; dy <= 33; dy++) {
            SHOTS shots = { .slots = { { 300 + dx, 200 + dy, true, 0 } } };
            ORB orb = {
                .left = 300, .top = 200, .force_frame = 90, .rotation_frame = 7
            };
            const bool expected = dx > -16 && dx < 32 && dy > -16 && dy < 32;
            CHECK(Shots_HitTestOrb(&shots, 0, &orb, &rng) == expected);
            CHECK(shots.slots[0].moving == !expected);
            CHECK_EQ_INT(orb.rotation_frame, 7);
            CHECK_EQ_INT(orb.force_frame, expected ? 0 : 90);
            if (expected) {
                CHECK_EQ_INT(orb.left, dx > 8 ? 296 : dx == 8 ? 300 : 304);
                CHECK(orb.top < 200);
            }
        }
    }
    CHECK_EQ_INT(rng, 1);
}

TEST(update_tests_orb_before_top_clip_and_skips_portals)
{
    uint32_t rng = 1;
    SHOTS shots = { .slots = { { 300, 70, true, 0 }, { 400, 76, true, 0 } } };
    ORB orb = { .left = 300, .top = 64 };
    SHOTS_OUTPUT out = M_Output();
    Shots_UpdateAndRender(&shots, &orb, &rng, out);
    CHECK(!shots.slots[0].moving);
    CHECK(shots.slots[1].moving);
    CHECK_EQ_INT(m_Count, 3);
    CHECK_EQ_INT(m_Actions[0].kind, SHOTS_ACTION_RESTORE);
    CHECK_EQ_INT(m_Actions[0].top, 70);
    CHECK_EQ_INT(m_Actions[1].kind, SHOTS_ACTION_RESTORE);
    CHECK_EQ_INT(m_Actions[2].kind, SHOTS_ACTION_PUT);
    CHECK_EQ_INT(m_Actions[2].top, 64);
    shots.slots[0] = (SHOT) { 300, 70, true, 0 };
    orb = (ORB) { .left = 300, .top = 64, .in_portal = true };
    out = M_Output();
    Shots_UpdateAndRender(&shots, &orb, &rng, out);
    CHECK_EQ_INT(orb.left, 300);
    CHECK_EQ_INT(orb.top, 64);
    CHECK(!shots.slots[0].moving);
}

TEST(pellet_margins_are_inclusive_and_first_hit_wins)
{
    for (int16_t dx = -15; dx <= 7; dx++) {
        for (int16_t dy = -15; dy <= 7; dy++) {
            SHOTS shots = { .slots = { { 300 + dx, 200 + dy, true, 0 },
                                       { 300 + dx, 200 + dy, true, 0 } } };
            const SHOTS_OUTPUT out = M_Output();
            const bool expected = dx >= -14 && dx <= 6 && dy >= -14 && dy <= 6;
            CHECK(Shots_HitTestPellet(&shots, 300, 200, out) == expected);
            CHECK(shots.slots[1].moving);
            CHECK_EQ_INT(m_Count, expected ? 2 : 0);
            if (expected) {
                CHECK_EQ_INT(m_Actions[0].kind, SHOTS_ACTION_RESTORE);
                CHECK_EQ_INT(m_Actions[1].effect, 16);
                CHECK_EQ_INT(shots.slots[0].decay_frame, 1);
            }
        }
    }
}

TEST(boss_inclusive_hit_and_decay_image_sequence)
{
    SHOTS shots = { .slots = { { 340, 220, true, 0 } } };
    SHOTS_OUTPUT out = M_Output();
    CHECK(Shots_HitTestBoss(&shots, 300, 200, 40, 20, out));
    ORB orb = { .in_portal = true };
    uint32_t rng = 1;
    for (int32_t frame = 1; frame <= 7; frame++) {
        out = M_Output();
        Shots_UpdateAndRender(&shots, &orb, &rng, out);
        CHECK_EQ_INT(m_Count, frame == 7 ? 1 : 2);
        CHECK_EQ_INT(m_Actions[0].kind, SHOTS_ACTION_ERASE);
        CHECK_EQ_INT(m_Actions[0].quarter, frame < 4 ? 1 : 2);
        if (frame != 7) {
            CHECK_EQ_INT(m_Actions[1].quarter, frame < 3 ? 1 : 2);
        }
    }
    CHECK_EQ_INT(shots.slots[0].decay_frame, 0);
    CHECK(!shots.slots[0].moving);
}

TEST(reset_erases_moving_and_decaying_in_slot_order)
{
    SHOTS shots = { .slots = { { 10, 100, true, 0 },
                               { 20, 200, false, 3 },
                               { 30, 300, true, 0 } } };
    const SHOTS_OUTPUT out = M_Output();
    Shots_EraseAndReset(&shots, out);
    CHECK_EQ_INT(m_Count, 3);
    for (int32_t i = 0; i < 3; i++) {
        CHECK_EQ_INT(m_Actions[i].kind, SHOTS_ACTION_RESTORE);
        CHECK_EQ_INT(m_Actions[i].left, (i + 1) * 10);
        CHECK(!shots.slots[i].moving);
        CHECK_EQ_INT(shots.slots[i].decay_frame, 0);
    }
}
