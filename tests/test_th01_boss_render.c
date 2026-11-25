#include "th01/boss/entity.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

#include <string.h>

TEST(entity_word_guard_and_animation_raw_rgb_differ)
{
    ARENA arena;
    Arena_Init(&arena, "boss render", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[1], 1, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(1);
    uint8_t pixels[16] = {};
    uint8_t coverage[16] = {};
    pixels[7] = 4;
    BOS sprites = { 16, 1, 1, pixels, coverage };
    BossEntity_Init();
    BOSS_ENTITY entity = {};
    BOSS_ANIMATION animation = {};
    CHECK(IS_OK(BossEntity_Bind(&entity, 0, &sprites)));
    CHECK(IS_OK(BossAnimation_Bind(&animation, 0, &sprites)));
    BossEntity_Put(&entity, 0, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(1, 7, 0), 1);
    BossAnimation_Put(&animation);
    CHECK_EQ_INT(Th01Test_Pixel(1, 7, 0), 5);
    coverage[0] = 1;
    pixels[0] = 10;
    BossEntity_Put(&entity, 0, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(1, 0, 0), 10);
    CHECK_EQ_INT(Th01Test_Pixel(1, 7, 0), 5);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    Arena_Free(&arena);
}

TEST(full_put_and_restore_have_different_right_edge_guards)
{
    ARENA arena;
    Arena_Init(&arena, "boss edge", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    uint8_t pixels[16], coverage[16];
    memset(pixels, 10, sizeof(pixels));
    memset(coverage, 1, sizeof(coverage));
    const BOS sprites = { 16, 1, 1, pixels, coverage };
    BossEntity_Init();
    BOSS_ENTITY entity = {};
    CHECK(IS_OK(BossEntity_Bind(&entity, 0, &sprites)));
    BossEntity_Put(&entity, 639, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 632, 0), 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 1), 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 1), 0);
    Video_SelectDrawingPage(1);
    BossEntity_RestorePut(&entity, 639, 0, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(Th01Test_Pixel(1, 639, 0), 0);
    BossEntity_Erase(&entity, 639, 0, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 0), 10);
    Arena_Free(&arena);
}

TEST(restored_line_keeps_signed_foreground_shift)
{
    ARENA arena;
    Arena_Init(&arena, "boss line", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 2, VIDEO_PAGE_SIZE);
    uint8_t pixels[16] = {};
    uint8_t coverage[16];
    memset(coverage, 1, sizeof(coverage));
    pixels[8] = 1;
    const BOS sprites = { 16, 1, 1, pixels, coverage };
    BossEntity_Init();
    BOSS_ENTITY entity = {};
    CHECK(IS_OK(BossEntity_Bind(&entity, 0, &sprites)));
    BossEntity_RestorePutLine(&entity, 1, 0, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 0), 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 9, 0), 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 7, 0), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 0), 2);
    Arena_Free(&arena);
}

TEST(locked_movement_includes_a_separate_unlock_call)
{
    ARENA arena;
    Arena_Init(&arena, "boss lock", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    const uint8_t pixels[16] = {};
    const uint8_t coverage[16] = {};
    const BOS sprites = { 16, 1, 1, pixels, coverage };
    BossEntity_Init();
    BOSS_ENTITY entity = {};
    CHECK(IS_OK(BossEntity_Bind(&entity, 0, &sprites)));
    BossEntity_SetPosition(
        &entity, 100, 100, (BOSS_BOUNDS) { 0, 600, 64, 300 });
    BossEntity_LockedMovePut(&entity, 2, 3, 3, false);
    CHECK_EQ_INT(entity.left, 102);
    CHECK_EQ_INT(entity.lock_frame, 1);
    for (int32_t i = 0; i < 3; i++) {
        BossEntity_LockedMovePut(&entity, 2, 3, 3, false);
        CHECK_EQ_INT(entity.left, 102);
    }
    CHECK_EQ_INT(entity.lock_frame, 0);
    BossEntity_LockedMovePut(&entity, 2, 3, 3, false);
    CHECK_EQ_INT(entity.left, 104);
    CHECK_EQ_INT(entity.previous_top, 103);
    Arena_Free(&arena);
}
