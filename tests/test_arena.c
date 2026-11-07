#include "core/base/arena.h"

#include "test.h"

#include <stdint.h>

typedef struct NODE {
    struct NODE *next;
    int32_t value;
} NODE;

static NODE *M_Push(ARENA *const arena, NODE *const head, const int32_t value)
{
    NODE *const node = Arena_Alloc(arena, sizeof(NODE));
    node->next = head;
    node->value = value;
    return node;
}

TEST(blocks_are_aligned_and_apart)
{
    ARENA arena;
    Arena_Init(&arena, "test", 4096);
    uint8_t *const a = Arena_Alloc(&arena, 3);
    uint8_t *const b = Arena_Alloc(&arena, 100);
    memset(a, 0xAA, 3);
    memset(b, 0xBB, 100);
    CHECK((uintptr_t)a % alignof(max_align_t) == 0);
    CHECK((uintptr_t)b % alignof(max_align_t) == 0);
    CHECK(b >= a + 3);
    CHECK_EQ_INT(a[2], 0xAA);
    Arena_Free(&arena);
}

TEST(alloc_arr_returns_count_elements)
{
    ARENA arena;
    Arena_Init(&arena, "test", 4096);
    int32_t *const values = Arena_AllocArr(&arena, 10, sizeof(int32_t));
    CHECK_EQ_INT(arena.used, 10 * sizeof(int32_t));
    int32_t *const zeroed = Arena_AllocArrZ(&arena, 10, sizeof(int32_t));
    CHECK((char *)zeroed >= (char *)(values + 10));
    CHECK_EQ_INT(zeroed[0] | zeroed[9], 0);
    Arena_Free(&arena);
}

TEST(the_whole_capacity_can_be_used)
{
    ARENA arena;
    Arena_Init(&arena, "test", 256);
    CHECK(Arena_Alloc(&arena, 256) == arena.base);
    CHECK_EQ_INT(arena.used, 256);
    Arena_Free(&arena);
}

TEST(reset_starts_again_and_alloc_z_zeroes)
{
    ARENA arena;
    Arena_Init(&arena, "test", 4096);
    uint8_t *const first = Arena_Alloc(&arena, 64);
    memset(first, 0xAB, 64);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    uint8_t *const again = Arena_AllocZ(&arena, 64);
    CHECK(again == first);
    CHECK_EQ_INT(again[0] | again[63], 0);
    Arena_Free(&arena);
}

TEST(restore_brings_back_contents_and_pointers)
{
    ARENA arena;
    Arena_Init(&arena, "sim", 1 << 20);
    NODE *head = nullptr;
    for (int32_t i = 0; i < 100; i++) {
        head = M_Push(&arena, head, i);
    }
    NODE *const saved_head = head;
    const size_t saved_used = arena.used;

    ARENA_SNAPSHOT snapshot = {};
    Arena_Save(&arena, &snapshot);
    CHECK_EQ_INT(snapshot.size, saved_used);

    for (NODE *node = head; node != nullptr; node = node->next) {
        node->value = -1;
    }
    head->next->next = nullptr;
    NODE *const later = M_Push(&arena, head, 1000);

    Arena_Restore(&arena, &snapshot);
    CHECK_EQ_INT(arena.used, saved_used);
    int32_t expected = 99;
    for (NODE *node = saved_head; node != nullptr; node = node->next) {
        CHECK_EQ_INT(node->value, expected);
        expected--;
    }
    CHECK_EQ_INT(expected, -1);

    NODE *const reused = Arena_AllocZ(&arena, sizeof(NODE));
    CHECK(reused == later);
    CHECK(reused->next == nullptr);
    CHECK_EQ_INT(reused->value, 0);

    Arena_FreeSnapshot(&snapshot);
    CHECK(snapshot.data == nullptr);
    Arena_Free(&arena);
}

TEST(a_snapshot_restores_more_than_once)
{
    ARENA arena;
    Arena_Init(&arena, "sim", 4096);
    int32_t *const counter = Arena_Alloc(&arena, sizeof(int32_t));
    *counter = 5;
    ARENA_SNAPSHOT snapshot = {};
    Arena_Save(&arena, &snapshot);
    for (int i = 0; i < 3; i++) {
        *counter += 10;
        Arena_Restore(&arena, &snapshot);
        CHECK_EQ_INT(*counter, 5);
    }
    Arena_FreeSnapshot(&snapshot);
    Arena_Free(&arena);
}

TEST(saving_again_reuses_the_snapshot)
{
    ARENA arena;
    Arena_Init(&arena, "sim", 4096);
    int32_t *const value = Arena_Alloc(&arena, sizeof(int32_t));
    ARENA_SNAPSHOT snapshot = {};

    *value = 1;
    Arena_Save(&arena, &snapshot);
    *value = 2;
    Arena_Save(&arena, &snapshot);
    *value = 3;
    Arena_Restore(&arena, &snapshot);
    CHECK_EQ_INT(*value, 2);

    Arena_Reset(&arena);
    Arena_Save(&arena, &snapshot);
    CHECK_EQ_INT(snapshot.size, 0);
    CHECK(snapshot.capacity >= sizeof(int32_t));
    Arena_FreeSnapshot(&snapshot);
    Arena_Free(&arena);
}

TEST(the_allocator_takes_blocks_from_the_arena)
{
    ARENA arena;
    Arena_Init(&arena, "test", 4096);
    const ALLOCATOR *const allocator = Arena_Allocator(&arena);

    char *block = Allocator_Alloc(allocator, 8);
    CHECK(block == (char *)arena.base);
    memcpy(block, "abcdefg", 8);
    char *const grown = Allocator_Realloc(allocator, block, 8, 64);
    CHECK(grown > block);
    CHECK_EQ_STR(grown, "abcdefg");
    CHECK(Allocator_Realloc(allocator, grown, 64, 16) == grown);

    const size_t used = arena.used;
    Allocator_Free(allocator, grown, 64);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}
