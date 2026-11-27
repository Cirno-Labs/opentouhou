#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_sprites.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/sparks.c"
#include "th02/sparks.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

TEST(loads_pre_shifted_mask_words_from_sparks_sheet)
{
    VFS *const vfs = Th02Test_Sprites();
    SPARK_IMAGES images;
    CHECK(IS_OK(SparkImages_Load(vfs, &images)));
    CHECK_EQ_INT(images.rows[0][0][0], 0x0010);
    CHECK_EQ_INT(images.rows[7][7][7], 0x7800);
    Vfs_Destroy(vfs);
}

TEST(missing_sparks_sheet_leaves_images_empty)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    SPARK_IMAGES images;
    memset(&images, 0xa5, sizeof(images));
    CHECK(!Result_Ignore(SparkImages_Load(vfs, &images)));
    for (size_t i = 0; i < sizeof(images); i++) {
        CHECK_EQ_INT(((uint8_t *)&images)[i], 0);
    }
    Vfs_Destroy(vfs);
}

TEST(update_wraps_age_and_words_before_removal_and_preserves_other_page)
{
    ARENA arena;
    Arena_Init(&arena, "spark update", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    TestClip_Set(0, 0, 639, 399);
    Grcg_Init();

    SPARK_IMAGES images = {};
    memset(&m_Sparks, 0xa5, sizeof(m_Sparks));
    Sparks_Init();
    CHECK_EQ_INT(m_Sparks.next, 0);
    CHECK_EQ_INT(m_Sparks.acceleration_x, 0);
    CHECK_EQ_INT(m_Sparks.sprite_interval, 0);
    CHECK_EQ_INT(m_Sparks.age_limit, 0);
    CHECK_EQ_INT(m_Sparks.slots[SPARK_COUNT - 1].state, 0);
    m_Sparks.next = 0xa53f;
    m_Sparks.acceleration_x = 2;
    m_Sparks.age_limit = 48;
    m_Sparks.slots[0] = (SPARK) {
        .state = ENTITY_ACTIVE,
        .age = 255,
        .position = { { 1600, 1600 }, { 3200, 3200 } },
    };
    m_Sparks.slots[1] = m_Sparks.slots[0];
    m_Sparks.slots[1].age = 48;
    m_Sparks.slots[2] = (SPARK) {
        .state = ENTITY_ACTIVE,
        .age = 255,
        .position = { { INT16_MAX, 1600 }, { 3200, 3200 } },
        .velocity = { 1, INT16_MAX },
    };
    Sparks_Update((PLAYFIELD_PAGES) { 0, 0, 0 }, &images);
    CHECK_EQ_INT(m_Sparks.slots[0].age, 0);
    CHECK_EQ_INT(m_Sparks.slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Sparks.slots[0].velocity.x, 2);
    CHECK_EQ_INT(m_Sparks.slots[0].velocity.y, 1);
    CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 100], 14);
    CHECK_EQ_INT(m_Sparks.slots[1].age, 49);
    CHECK_EQ_INT(m_Sparks.slots[1].state, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Sparks.slots[1].velocity.y, 1);
    CHECK_EQ_INT(m_Sparks.slots[2].age, 0);
    CHECK_EQ_INT(m_Sparks.slots[2].position[0].x, INT16_MIN);
    CHECK_EQ_INT(m_Sparks.slots[2].velocity.y, INT16_MIN);
    CHECK_EQ_INT(m_Sparks.slots[2].state, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Sparks.slots[2].position[1].y, 3200);
    CHECK_EQ_INT(m_Sparks.next, 0xa53f);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 0);
    for (size_t i = 1; i < 4; i++) {
        CHECK_EQ_INT(g_Grcg->tiles[i], 255);
    }
    Arena_Free(&arena);
}

TEST(sprite_masks_ignore_clip_and_roll_while_dots_obey_clip)
{
    ARENA arena;
    Arena_Init(&arena, "spark drawing", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Video_AccessPage(1);
    Video_SetSplit(100, 200, 300);
    TestClip_Set(0, 0, 10, 10);
    Grcg_Init();

    SPARK_IMAGES images = {};
    images.rows[3][1][0] = 0x0240;
    images.rows[3][1][7] = 0x0080;
    Sparks_Init();
    m_Sparks.age_limit = 48;
    m_Sparks.slots[0] = (SPARK) {
        .state = ENTITY_ACTIVE,
        .position = { { 99 * 16, 100 * 16 } },
        .drawing = 255,
    };
    m_Sparks.slots[1] = (SPARK) {
        .state = ENTITY_ACTIVE,
        .position = { { 100 * 16, 100 * 16 } },
    };
    Sparks_Update((PLAYFIELD_PAGES) { 0, 0, 299 }, &images);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 97], 14);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 110], 14);
    CHECK_EQ_INT(g_Video.pages[1][6 * 640 + 96], 14);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 100], 0);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 97], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 100);
    CHECK_EQ_INT(g_Video.first_address, 200);
    CHECK_EQ_INT(g_Video.second_address, 300);
    Arena_Free(&arena);
}

TEST(cleanup_marks_different_footprints_and_copies_opposite_front)
{
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_BLACK;
    Sparks_Init();
    m_Sparks.slots[0] = (SPARK) {
        .state = ENTITY_ACTIVE,
        .position = { { 47 * 16, 31 * 16 }, { 100 * 16, 100 * 16 } },
    };
    m_Sparks.slots[1] = m_Sparks.slots[0];
    m_Sparks.slots[1].state = ENTITY_REMOVE;
    m_Sparks.slots[1].drawing = 14;
    Sparks_Invalidate((PLAYFIELD_PAGES) { 0, 0, 0 });
    CHECK_EQ_INT(m_Sparks.slots[0].position[1].x, 47 * 16);
    CHECK_EQ_INT(m_Sparks.slots[0].position[0].y, 31 * 16);
    CHECK_EQ_INT(m_Sparks.slots[1].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Sparks.slots[1].position[1].x, 100 * 16);
    CHECK(p_Background->cache.dirty[0][1] != 0);
    CHECK(p_Background->cache.dirty[1][2] != 0);
}

TEST(spawn_overwrites_live_ring_slots_and_keeps_other_page_and_defaults)
{
    Sparks_Init();
    m_Sparks.next = 63;
    m_Sparks.acceleration_x = -9;
    m_Sparks.age_limit = 48;
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        m_Sparks.slots[i] = (SPARK) {
            .state = ENTITY_ACTIVE,
            .age = 42,
            .position = { { 11, 22 }, { 33, 44 } },
            .drawing = 255,
            .reserved_1 = 17,
            .angle = 64,
            .base_speed = 63,
            .reserved_2 = 29,
            .preferred = 14,
        };
    }
    Sparks_Add(1, -2049, 801, INT16_MAX, 2, false);
    CHECK_EQ_INT(m_Sparks.next, 1);
    CHECK_EQ_INT(m_Sparks.acceleration_x, -9);
    CHECK_EQ_INT(m_Sparks.age_limit, 48);
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        const SPARK *const spark = &m_Sparks.slots[i];
        CHECK_EQ_INT(spark->position[0].x, 11);
        CHECK_EQ_INT(spark->position[0].y, 22);
        CHECK_EQ_INT(spark->reserved_1, 17);
        CHECK_EQ_INT(spark->reserved_2, 29);
        CHECK_EQ_INT(spark->angle, 64);
        CHECK_EQ_INT(spark->base_speed, 63);
        CHECK_EQ_INT(spark->preferred, 14);
        if (i == 0 || i == 63) {
            CHECK_EQ_INT(spark->state, ENTITY_ACTIVE);
            CHECK_EQ_INT(spark->age, 0);
            CHECK_EQ_INT(spark->position[1].x, 32752);
            CHECK_EQ_INT(spark->position[1].y, 6416);
            CHECK_EQ_INT(spark->drawing, 14);
            CHECK_EQ_INT(spark->velocity.x, 0);
            CHECK_EQ_INT(spark->velocity.y, -32706);
        } else {
            CHECK_EQ_INT(spark->age, 42);
            CHECK_EQ_INT(spark->position[1].x, 33);
            CHECK_EQ_INT(spark->drawing, 255);
        }
    }
}

TEST(sprite_burst_wraps_angle_numerator_and_nonpositive_counts_only_move_cursor)
{
    Sparks_Init();
    m_Sparks.next = 63;
    Sparks_Add(0, 100, 400, 256, 130, true);
    CHECK_EQ_INT(m_Sparks.next, 1);
    CHECK_EQ_INT(m_Sparks.slots[63].position[0].x, 1600);
    CHECK_EQ_INT(m_Sparks.slots[63].position[0].y, 0);
    CHECK_EQ_INT(m_Sparks.slots[63].drawing, 14);
    CHECK_EQ_INT(m_Sparks.slots[63].velocity.x, 255);
    CHECK_EQ_INT(m_Sparks.slots[63].velocity.y, 25);
    const SPARKS before = m_Sparks;
    Sparks_Add(1, 0, 0, 0, 0, true);
    CHECK(memcmp(&m_Sparks, &before, sizeof(m_Sparks)) == 0);
    Sparks_Add(1, 0, 0, 0, -2, true);
    CHECK_EQ_INT(m_Sparks.next, 63);
    CHECK(memcmp(m_Sparks.slots, before.slots, sizeof(m_Sparks.slots)) == 0);
}
