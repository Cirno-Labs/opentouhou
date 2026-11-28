#include "tests/test.h"
#include "tests/th01/th01_video.h"
#include "tests/th01/th01_world.h"
#include "th01/missiles.c"
#include "th01/missiles.h"
#include "th01/player.c"

#include "core/base/memory.h"

#include <string.h>

TEST(pool_and_spawn_history)
{
    Th01Test_ResetWorld();
    memset(&m_Missiles, 0, sizeof(m_Missiles));
    m_Missiles.slots[0].previous_top = 123;
    m_Missiles.slots[0].unused = 456;
    for (int32_t i = 0; i < 50; i++) {
        CHECK(Missiles_Add(100, 200, -1.99, 0.99, -7));
        CHECK_EQ_INT(m_Missiles.slots[i].left, 1600);
        CHECK_EQ_INT(m_Missiles.slots[i].velocity.x, -31);
        CHECK_EQ_INT(m_Missiles.slots[i].velocity.y, 15);
        CHECK_EQ_INT(m_Missiles.slots[i].previous_left, -1);
        CHECK_EQ_INT(m_Missiles.slots[i].reserved, -7);
    }
    CHECK(!Missiles_Add(0, 0, 0, 0, 0));
    CHECK_EQ_INT(m_Missiles.slots[0].previous_top, 123);
    CHECK_EQ_INT(m_Missiles.slots[0].unused, 456);
    Missiles_ResetStage();
    CHECK_EQ_INT(m_Missiles.slots[0].left, 1600);
    for (int32_t i = 0; i < 50; i++) {
        CHECK_EQ_INT(m_Missiles.slots[i].flag, MISSILE_FREE);
    }
}

TEST(interlace_motion_and_explosion_lifetime)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    uint8_t pixels[5 * 1024] = {};
    PTN sprites = { .count = 5, .pixels = pixels };
    memset(&m_Missiles, 0, sizeof(m_Missiles));
    memset(&m_Player, 0, sizeof(m_Player));
    m_Player.left = 300;
    CHECK(Missiles_Add(623, 100, 2, 0, 0));
    m_Missiles.slots[0].previous_top = 80 * 16;
    Missiles_Update(1, &sprites);
    CHECK_EQ_INT(m_Missiles.slots[0].left, 625 * 16);
    CHECK_EQ_INT(m_Missiles.slots[0].previous_left, 624 * 16);
    CHECK_EQ_INT(m_Missiles.slots[0].previous_top, 80 * 16);
    CHECK_EQ_INT(m_Missiles.slots[0].flag, MISSILE_HIT);
    Missiles_Update(0, &sprites);
    CHECK_EQ_INT(m_Missiles.slots[0].flag, MISSILE_HIT_LAST);
    Missiles_Update(1, &sprites);
    CHECK_EQ_INT(m_Missiles.slots[0].flag, MISSILE_HIT_LAST);
    Missiles_Update(0, &sprites);
    CHECK_EQ_INT(m_Missiles.slots[0].flag, MISSILE_FREE);
    Mem_Free(g_Video->pages[0]);
}

TEST(lethal_current_coordinates_ignore_field_and_sliding)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    uint8_t pixels[5 * 1024] = {};
    PTN sprites = { .count = 5, .pixels = pixels };
    const int16_t xs[] = { 292, 293, 323, 324 };
    const int16_t ys[] = { 360, 361, 399, 400 };
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            memset(&m_Missiles, 0, sizeof(m_Missiles));
            m_Missiles.slots[0] = (MISSILE) {
                .left = xs[x] * 16,
                .top = ys[y] * 16,
                .previous_left = 100 * 16,
                .previous_top = 100 * 16,
                .flag = MISSILE_HIT,
            };
            memset(&m_Player, 0, sizeof(m_Player));
            m_Player.left = 300;
            m_Player.sliding = true;
            Missiles_Update(1, &sprites);
            CHECK(m_Player.is_hit == (x > 0 && x < 3 && y > 0 && y < 3));
            m_Player.is_hit = false;
            m_Player.invincible = true;
            Missiles_Update(1, &sprites);
            CHECK(!m_Player.is_hit);
        }
    }
    Mem_Free(g_Video->pages[0]);
}

TEST(unaligned_quarters_and_word_rounded_restore)
{
    Th01Test_ResetWorld();
    uint8_t pixels[5 * 1024];
    memset(pixels, 15, sizeof(pixels));
    pixels[0] = 3;
    pixels[15] = 5;
    PTN sprites = { .count = 5, .pixels = pixels };
    CHECK(IS_OK(Missiles_ValidateSprites(&sprites)));
    Video_Init(&g_HeapAllocator);
    memset(&m_Missiles, 0, sizeof(m_Missiles));
    memset(&m_Player, 0, sizeof(m_Player));
    m_Player.left = 0;
    m_Missiles.slots[0] = (MISSILE) {
        .left = 19 * 16,
        .top = 100 * 16,
        .previous_left = -1,
        .velocity = { 0, 0 },
        .flag = MISSILE_MOVING,
    };
    Missiles_Update(0, &sprites);
    CHECK_EQ_INT(Th01Test_Pixel(0, 19, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 34, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 100), 0);
    m_Missiles.slots[0].flag = MISSILE_HIT;
    memset(g_Video->pages[1], 9, VIDEO_PAGE_SIZE);
    Missiles_Update(0, &sprites);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 100), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 63, 115), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 64, 115), 0);
    Mem_Free(g_Video->pages[0]);
}
