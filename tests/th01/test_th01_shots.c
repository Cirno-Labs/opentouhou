#include "tests/test.h"
#include "th01/orb_private.h"
#include "th01/rng.h"
#include "th01/shots_private.h"

#include "core/base/memory.h"

#include <string.h>

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

TEST(spawn_uses_first_free_slot)
{
    memset(p_Shots, 0, sizeof(*p_Shots));
    CHECK(!Shots_Add(-1, 368));
    CHECK(!Shots_Add(640, 368));
    p_Shots->slots[0].decay_frame = 3;
    for (int32_t i = 1; i < 8; i++) {
        CHECK(Shots_Add(i == 7 ? 639 : 0, 368));
        CHECK(p_Shots->slots[i].moving);
    }
    CHECK(!Shots_Add(320, 368));
    p_Shots->slots[3].moving = false;
    CHECK(Shots_Add(16, -3));
    CHECK_EQ_INT(p_Shots->slots[3].left, 16);
    CHECK_EQ_INT(p_Shots->slots[3].top, -3);
}

TEST(orb_collision_has_strict_bounds_and_extra_motion)
{
    g_Rng = 1;
    for (int16_t dx = -17; dx <= 33; dx++) {
        for (int16_t dy = -17; dy <= 33; dy++) {
            memset(p_Shots, 0, sizeof(*p_Shots));
            p_Shots->slots[0] = (SHOT) { 300 + dx, 200 + dy, true, 0 };
            memset(p_Orb, 0, sizeof(*p_Orb));
            p_Orb->left = 300;
            p_Orb->top = 200;
            p_Orb->force_frame = 90;
            p_Orb->rotation_frame = 7;
            const bool expected = dx > -16 && dx < 32 && dy > -16 && dy < 32;
            CHECK(Shots_HitTestOrb(0) == expected);
            CHECK(p_Shots->slots[0].moving == !expected);
            CHECK_EQ_INT(p_Orb->rotation_frame, 7);
            CHECK_EQ_INT(p_Orb->force_frame, expected ? 0 : 90);
            if (expected) {
                CHECK_EQ_INT(p_Orb->left, dx > 8 ? 296 : dx == 8 ? 300 : 304);
                CHECK(p_Orb->top < 200);
            }
        }
    }
    CHECK_EQ_INT(g_Rng, 1);
}

TEST(update_tests_orb_before_top_clip_and_skips_portals)
{
    Video_Init(&g_HeapAllocator);
    uint8_t pixels[2048] = {};
    PTN sprites = { .count = 2, .pixels = pixels };
    g_Rng = 1;
    memset(p_Shots, 0, sizeof(*p_Shots));
    p_Shots->slots[0] = (SHOT) { 300, 70, true, 0 };
    p_Shots->slots[1] = (SHOT) { 400, 76, true, 0 };
    memset(p_Orb, 0, sizeof(*p_Orb));
    p_Orb->left = 300;
    p_Orb->top = 64;
    Shots_Update(&sprites);
    CHECK(!p_Shots->slots[0].moving);
    CHECK(p_Shots->slots[1].moving);
    CHECK_EQ_INT(p_Shots->slots[1].top, 64);
    p_Shots->slots[0] = (SHOT) { 300, 70, true, 0 };
    memset(p_Orb, 0, sizeof(*p_Orb));
    p_Orb->left = 300;
    p_Orb->top = 64;
    p_Orb->in_portal = true;
    Shots_Update(&sprites);
    CHECK_EQ_INT(p_Orb->left, 300);
    CHECK_EQ_INT(p_Orb->top, 64);
    CHECK(!p_Shots->slots[0].moving);
    Mem_Free(g_Video->pages[0]);
}

TEST(pellet_margins_are_inclusive_and_first_hit_wins)
{
    Video_Init(&g_HeapAllocator);
    for (int16_t dx = -15; dx <= 7; dx++) {
        for (int16_t dy = -15; dy <= 7; dy++) {
            memset(p_Shots, 0, sizeof(*p_Shots));
            p_Shots->slots[0] = (SHOT) { 300 + dx, 200 + dy, true, 0 };
            p_Shots->slots[1] = (SHOT) { 300 + dx, 200 + dy, true, 0 };
            const bool expected = dx >= -14 && dx <= 6 && dy >= -14 && dy <= 6;
            CHECK(Shots_HitTestPellet(300, 200) == expected);
            CHECK(p_Shots->slots[1].moving);
            if (expected) {
                CHECK(!p_Shots->slots[0].moving);
                CHECK_EQ_INT(p_Shots->slots[0].decay_frame, 1);
            }
        }
    }
    Mem_Free(g_Video->pages[0]);
}

TEST(boss_inclusive_hit_and_decay_image_sequence)
{
    Video_Init(&g_HeapAllocator);
    uint8_t pixels[2048] = {};
    PTN sprites = { .count = 2, .pixels = pixels };
    memset(p_Shots, 0, sizeof(*p_Shots));
    p_Shots->slots[0] = (SHOT) { 340, 220, true, 0 };
    CHECK(Shots_HitTestBoss(300, 200, 40, 20));
    CHECK(!p_Shots->slots[0].moving);
    CHECK_EQ_INT(p_Shots->slots[0].decay_frame, 1);
    memset(p_Orb, 0, sizeof(*p_Orb));
    p_Orb->in_portal = true;
    g_Rng = 1;
    for (int32_t frame = 1; frame <= 7; frame++) {
        Shots_Update(&sprites);
        CHECK_EQ_INT(p_Shots->slots[0].decay_frame, frame == 7 ? 0 : frame + 1);
        CHECK(!p_Shots->slots[0].moving);
    }
    CHECK_EQ_INT(p_Shots->slots[0].decay_frame, 0);
    CHECK(!p_Shots->slots[0].moving);
    Mem_Free(g_Video->pages[0]);
}

TEST(reset_erases_moving_and_decaying_in_slot_order)
{
    Video_Init(&g_HeapAllocator);
    memset(p_Shots, 0, sizeof(*p_Shots));
    p_Shots->slots[0] = (SHOT) { 10, 100, true, 0 };
    p_Shots->slots[1] = (SHOT) { 20, 200, false, 3 };
    p_Shots->slots[2] = (SHOT) { 30, 300, true, 0 };
    Shots_EraseAndReset();
    for (int32_t i = 0; i < 3; i++) {
        CHECK(!p_Shots->slots[i].moving);
        CHECK_EQ_INT(p_Shots->slots[i].decay_frame, 0);
    }
    Mem_Free(g_Video->pages[0]);
}
