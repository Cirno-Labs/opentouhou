#include "th01/scene.h"

#include "core/base/arena.h"

#include "test.h"

#define M_SIZE (22 + 5 * 250)

TEST(scene_names_come_from_requested_id_not_header)
{
    ARENA arena;
    Arena_Init(&arena, "scene", 65536);
    uint8_t bytes[M_SIZE] = {};
    memset(bytes, 0xff, 22);
    bytes[22 + 4 * 250] = 8;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    SCENE_DATA scene;
    CHECK(Result_Ignore(
        Scene_LoadReader(&reader, 2, Arena_Allocator(&arena), &scene)));
    CHECK_EQ_STR(scene.background, "ST2.GRP");
    CHECK_EQ_STR(scene.music, "ST2.MDT");
    CHECK_EQ_INT(scene.id, 2);
    CHECK(scene.layouts.stages[4].cells[0][0].card);
    CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(cached_activation_borrows_independent_layouts)
{
    ARENA arena;
    Arena_Init(&arena, "scenes", 128 * 1024);
    SCENE_DATA cache[SCENE_COUNT];
    for (uint8_t id = 0; id < SCENE_COUNT; id++) {
        uint8_t bytes[M_SIZE] = {};
        bytes[22 + id / 4] = (uint8_t)(8 >> (id % 4));
        READER reader;
        Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
        CHECK(Result_Ignore(Scene_LoadReader(
            &reader, id, Arena_Allocator(&arena), &cache[id])));
        Reader_Close(&reader);
    }
    const size_t used = arena.used;
    SCENE_STATE state = {};
    const uint8_t ids[] = { 1, 2, 1, 6, 0 };
    for (size_t i = 0; i < sizeof(ids); i++) {
        const uint8_t id = ids[i];
        Scene_Activate(&state, &cache[id]);
        CHECK(state.active == &cache[id]);
        CHECK_EQ_INT(state.active->background[2], '0' + id);
        for (int32_t column = 0; column < 20; column++) {
            CHECK(
                state.active->layouts.stages[0].cells[0][column].card
                == (column == id));
        }
    }
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(invalid_or_truncated_scenes_allocate_nothing)
{
    ARENA arena;
    Arena_Init(&arena, "scene", 65536);
    uint8_t bytes[M_SIZE] = {};
    READER reader;
    Reader_OpenBuffer(&reader, bytes, 22, nullptr);
    SCENE_DATA out = { .id = 6 };
    CHECK(!Result_Ignore(
        Scene_LoadReader(&reader, 1, Arena_Allocator(&arena), &out)));
    CHECK(out.layouts.stages == nullptr);
    CHECK_EQ_INT(out.id, 0);
    CHECK_EQ_INT(arena.used, 0);
    Reader_Close(&reader);
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(!Result_Ignore(
        Scene_LoadReader(&reader, 7, Arena_Allocator(&arena), &out)));
    CHECK_EQ_INT(arena.used, 0);
    Reader_Close(&reader);
    Arena_Free(&arena);
}
