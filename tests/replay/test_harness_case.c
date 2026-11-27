#include "case.h"

#include "harness_game.h"
#include "test.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#define M_ARENA_SIZE (1024 * 1024)

static char *M_Take(WRITER *const writer)
{
    Writer_WriteU8(writer, 0);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(writer, &size);
    Result_Ignore(Writer_Close(writer));
    char *const text = Str_Dup((const char *)bytes);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    return text;
}

TEST(a_manifest_round_trips)
{
    const STAGE_STEP steps[] = {
        { "files/x.rpy", "replay/slot.rpy" },
        { "files/x.cfg", "x.cfg" },
    };
    const CASE_MANIFEST manifest = {
        .format = CASE_FORMAT,
        .game = "test",
        .game_version = "1.0",
        .backend = "native",
        .columns_version = 3,
        .frames = 1234,
        .rows = 1240,
        .choices = (uint32_t[]) { 3 },
        .note = "a world record",
        .steps = steps,
        .step_count = 2,
        .starts = (START_VALUE[]) { { 1, (uint32_t)-2 } },
        .start_count = 1,
    };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Case_WriteManifest(&writer, &m_TestGame, &manifest);
    char *const text = M_Take(&writer);
    CHECK(
        strstr(text, "1 = stage files/x.rpy -> replay/slot.rpy\n") != nullptr);
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    READER reader;
    Reader_OpenBuffer(&reader, text, strlen(text), "case.ini");
    CASE_MANIFEST read;
    CHECK(
        Result_Ignore(Case_ReadManifest(&reader, &m_TestGame, &arena, &read)));
    Reader_Close(&reader);
    CHECK_EQ_STR(read.game, "test");
    CHECK_EQ_STR(read.backend, "native");
    CHECK_EQ_STR(read.note, "a world record");
    CHECK_EQ_INT(read.frames, 1234);
    CHECK_EQ_INT(read.rows, 1240);
    CHECK_EQ_INT(read.choices[0], 3);
    CHECK_EQ_INT(read.columns_version, 3);
    CHECK_EQ_INT(read.step_count, 2);
    CHECK_EQ_STR(read.steps[0].source, "files/x.rpy");
    CHECK_EQ_STR(read.steps[0].path, "replay/slot.rpy");
    CHECK_EQ_STR(read.steps[1].path, "x.cfg");
    CHECK(strstr(text, "[start]\nlives = -2\n") != nullptr);
    CHECK_EQ_INT(read.start_count, 1);
    CHECK_EQ_INT(read.starts[0].field, 1);
    CHECK_EQ_INT((int32_t)read.starts[0].value, -2);
    Mem_Free(text);
    Arena_Free(&arena);
}

TEST(a_manifest_of_another_format_is_rejected)
{
    const char *const text = "[case]\nformat = 1\ngame = test\n";
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    READER reader;
    Reader_OpenBuffer(&reader, text, strlen(text), "case.ini");
    CASE_MANIFEST read = { .frames = 9 };
    CHECK(
        !Result_Ignore(Case_ReadManifest(&reader, &m_TestGame, &arena, &read)));
    CHECK_EQ_INT(read.frames, 9);
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(a_manifest_without_a_choice_is_rejected)
{
    const char *const texts[] = {
        "[case]\nformat = 6\ngame = t\n",
        "[case]\nformat = 6\ngame = t\n[choices]\ndifficulty = x\n",
    };
    for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        ARENA arena;
        Arena_Init(&arena, "test", M_ARENA_SIZE);
        READER reader;
        Reader_OpenBuffer(&reader, texts[i], strlen(texts[i]), "case.ini");
        CASE_MANIFEST read;
        CHECK(!Result_Ignore(
            Case_ReadManifest(&reader, &m_TestGame, &arena, &read)));
        Reader_Close(&reader);
        Arena_Free(&arena);
    }
}

TEST(a_bad_staging_step_is_rejected)
{
    const char *const texts[] = {
        "[case]\nformat = 6\ngame = t\n[choices]\ndifficulty = "
        "0\n[preconditions]\n1 = copy a -> b\n",
        "[case]\nformat = 6\ngame = t\n[choices]\ndifficulty = "
        "0\n[preconditions]\n"
        "1 = stage files/a -> ../b\n",
        "[case]\nformat = 6\ngame = t\n[choices]\ndifficulty = "
        "0\n[preconditions]\n"
        "1 = stage files/a -> Save/b\n",
        "[case]\nformat = 6\ngame = t\n[choices]\ndifficulty = "
        "0\n[preconditions]\n"
        "1 = stage a/b -> b\n",
        "[case]\nformat = 6\ngame = t\n[choices]\ndifficulty = "
        "0\n[preconditions]\n1 =\n",
    };
    for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        ARENA arena;
        Arena_Init(&arena, "test", M_ARENA_SIZE);
        READER reader;
        Reader_OpenBuffer(&reader, texts[i], strlen(texts[i]), "case.ini");
        CASE_MANIFEST read;
        CHECK(!Result_Ignore(
            Case_ReadManifest(&reader, &m_TestGame, &arena, &read)));
        Reader_Close(&reader);
        Arena_Free(&arena);
    }
}
