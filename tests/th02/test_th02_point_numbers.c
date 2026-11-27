#include "../../src/th02/point_numbers.c"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/point_numbers.h"

#include "core/base/arena.h"

#include "th02_sprites.h"
#include "test.h"

TEST(font_is_owned_and_copied_from_pointnum_sheet)
{
    VFS *const vfs = Th02Test_Sprites();
    POINT_NUMBER_FONT font;
    CHECK(IS_OK(PointNumberFont_Load(vfs, &font)));
    static const uint8_t glyph0[8] = { 0, 56, 68, 130, 130, 130, 68, 56 };
    CHECK(memcmp(font.rows[0], glyph0, sizeof(glyph0)) == 0);
    static const uint8_t blank[8] = {};
    CHECK(memcmp(font.rows[12], blank, sizeof(blank)) == 0);
    Vfs_Destroy(vfs);
}

TEST(missing_pointnum_sheet_leaves_font_empty)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    POINT_NUMBER_FONT font;
    memset(&font, 0xa5, sizeof(font));
    CHECK(!Result_Ignore(PointNumberFont_Load(vfs, &font)));
    for (size_t i = 0; i < sizeof(font); i++) {
        CHECK_EQ_INT(((uint8_t *)&font)[i], 0);
    }
    Vfs_Destroy(vfs);
}

TEST(rank_reset_preserves_record_payloads_and_unknown_multiplier_state)
{
    memset(&m_PointNumbers, 0x5a, sizeof(m_PointNumbers));
    PointNumbers_Init();
    CHECK_EQ_INT(m_PointNumbers.color, 0);
    CHECK_EQ_INT(m_PointNumbers.operation, 0);
    CHECK_EQ_INT(m_PointNumbers.operand, 0);
    for (size_t i = 0; i < POINT_NUMBER_COUNT; i++) {
        CHECK_EQ_INT(m_PointNumbers.slots[i].state, ENTITY_FREE);
        CHECK_EQ_INT(m_PointNumbers.slots[i].left, 0);
        CHECK_EQ_INT(m_PointNumbers.slots[i].top[0], 0);
        CHECK_EQ_INT(m_PointNumbers.slots[i].top[1], 0);
        CHECK_EQ_INT(m_PointNumbers.slots[i].value, 0);
        CHECK_EQ_INT(m_PointNumbers.slots[i].age, 0);
    }
    for (int32_t rank = 0; rank < 256; rank++) {
        memset(&m_PointNumbers, 0xa5, sizeof(m_PointNumbers));
        PointNumbers_ResetStage((uint8_t)rank);
        CHECK_EQ_INT(m_PointNumbers.color, 15);
        CHECK_EQ_INT(
            m_PointNumbers.operation,
            rank < 2       ? 12
                : rank < 5 ? 10
                           : 0xa5);
        CHECK_EQ_INT(
            m_PointNumbers.operand,
            rank < 2       ? 12
                : rank < 5 ? 1 << (rank - 1)
                           : 0xa5);
        for (size_t i = 0; i < POINT_NUMBER_COUNT; i++) {
            CHECK_EQ_INT(m_PointNumbers.slots[i].state, ENTITY_FREE);
            CHECK_EQ_INT(m_PointNumbers.slots[i].left, (int16_t)0xa5a5);
            CHECK_EQ_INT(m_PointNumbers.slots[i].top[0], (int16_t)0xa5a5);
            CHECK_EQ_INT(m_PointNumbers.slots[i].top[1], (int16_t)0xa5a5);
            CHECK_EQ_INT(m_PointNumbers.slots[i].age, 0xa5);
            CHECK_EQ_INT(m_PointNumbers.slots[i].value, 0xa5a5);
        }
    }
}

TEST(add_uses_the_first_free_slot_and_keeps_a_full_bank)
{
    memset(&m_PointNumbers, 0xa5, sizeof(m_PointNumbers));
    const POINT_NUMBERS before = m_PointNumbers;
    PointNumbers_Add(-7, 399, 6553);
    CHECK(memcmp(&m_PointNumbers, &before, sizeof(m_PointNumbers)) == 0);
    m_PointNumbers.slots[7].state = ENTITY_FREE;
    m_PointNumbers.slots[9].state = ENTITY_FREE;
    PointNumbers_Add(-7, 399, 6553);
    CHECK_EQ_INT(m_PointNumbers.slots[7].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_PointNumbers.slots[7].age, 0);
    CHECK_EQ_INT(m_PointNumbers.slots[7].left, -7);
    CHECK_EQ_INT(m_PointNumbers.slots[7].top[0], 399);
    CHECK_EQ_INT(m_PointNumbers.slots[7].top[1], 399);
    CHECK_EQ_INT(m_PointNumbers.slots[7].value, 6553);
    CHECK_EQ_INT(m_PointNumbers.slots[9].state, ENTITY_FREE);
    CHECK_EQ_INT(m_PointNumbers.color, 0xa5);
}

TEST(cleanup_keeps_removed_tops_and_copies_other_nonfree_flags)
{
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_DISABLED;
    PointNumbers_Init();
    m_PointNumbers.slots[0] =
        (POINT_NUMBER) { .top = { 100, 200 }, .state = ENTITY_REMOVE };
    m_PointNumbers.slots[1] =
        (POINT_NUMBER) { .top = { 101, 201 }, .state = ENTITY_ACTIVE };
    m_PointNumbers.slots[2] =
        (POINT_NUMBER) { .top = { 102, 202 }, .state = 255 };
    PointNumbers_Invalidate((PLAYFIELD_PAGES) { 0, 1, 399 });
    CHECK_EQ_INT(m_PointNumbers.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_PointNumbers.slots[0].top[0], 100);
    CHECK_EQ_INT(m_PointNumbers.slots[1].top[0], 201);
    CHECK_EQ_INT(m_PointNumbers.slots[2].top[0], 202);
    CHECK_EQ_INT(m_PointNumbers.slots[2].state, 255);
}

TEST(update_places_digits_then_zero_and_multiplier_and_preserves_video_state)
{
    ARENA arena;
    Arena_Init(&arena, "point numbers", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Video_AccessPage(1);
    Video_SetSplit(100, 80, 160);
    memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
    Grcg_Init();

    POINT_NUMBER_FONT font = {};
    font.rows[0][0] = 0x80;
    font.rows[2][0] = 0x40;
    font.rows[10][0] = 0x20;
    PointNumbers_Init();
    m_PointNumbers.color = 0x12;
    m_PointNumbers.operation = 10;
    m_PointNumbers.operand = 2;
    PointNumbers_Add(100, 399, 12);
    m_PointNumbers.slots[0].age = 6;
    PointNumbers_Update((PLAYFIELD_PAGES) { 1, 1, 0 }, &font);
    CHECK_EQ_INT(m_PointNumbers.slots[0].age, 7);
    CHECK_EQ_INT(m_PointNumbers.slots[0].top[1], 399);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 125], 2);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 132], 2);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 142], 2);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 149], 2);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 124], 3);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 100);
    CHECK_EQ_INT(g_Video.first_address, 80);
    CHECK_EQ_INT(g_Video.second_address, 160);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 0);
    CHECK_EQ_INT(g_Grcg->tiles[1], 255);
    PointNumbers_Update((PLAYFIELD_PAGES) { 1, 1, 0 }, &font);
    CHECK_EQ_INT(m_PointNumbers.slots[0].top[1], 398);
    m_PointNumbers.slots[0].age = 25;
    m_PointNumbers.slots[0].top[1] = INT16_MIN;
    PointNumbers_Update((PLAYFIELD_PAGES) { 1, 1, 0 }, &font);
    CHECK_EQ_INT(m_PointNumbers.slots[0].top[1], INT16_MAX);
    CHECK_EQ_INT(m_PointNumbers.slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(m_PointNumbers.slots[0].age, 25);
    Arena_Free(&arena);
}
