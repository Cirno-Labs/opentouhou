#include "compare.h"

#include "harness_game.h"
#include "test.h"
#include "test_dir.h"

#include "core/image/decode.h"
#include "core/image/encode.h"

#define M_ARENA_SIZE (1024 * 1024)

// Holds the rows that a comparison expects, read from memory of arena.
typedef struct {
    READER reader;
    STATE_READER *state;
} M_EXPECTED;

// Reads the rows { 1, 2, 3, 4 } at frame 0 and { 5, 2, 3, 4 } at frame
// 1, the second the last of its chunk and so a check, into out.
static void M_Expect(ARENA *const arena, M_EXPECTED *const out)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, Arena_Allocator(arena), nullptr);
    STATE_WRITER *const state = State_OpenWriter(&writer, &m_TestGame);
    State_WriteRow(state, 0, (const uint32_t[]) { 1, 2, 3, 4 });
    State_WriteRow(state, 1, (const uint32_t[]) { 5, 2, 3, 4 });
    CHECK(Result_Ignore(State_CloseWriter(state)));
    size_t size;
    const uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    Reader_OpenBuffer(&out->reader, bytes, size, "state.bin");
    out->state = State_OpenReader(&out->reader, &m_TestGame, 2);
}

static void M_Close(M_EXPECTED *const expected)
{
    CHECK(Result_Ignore(State_CloseReader(expected->state)));
    Reader_Close(&expected->reader);
}

TEST(equal_rows_match)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    M_EXPECTED expected;
    M_Expect(&arena, &expected);
    STATE_COMPARE *const compare =
        Compare_OpenState(expected.state, &m_TestGame, &arena);
    Compare_StateRow(compare, 0, (const uint32_t[]) { 1, 2, 3, 4 });
    Compare_StateRow(compare, 1, (const uint32_t[]) { 5, 2, 3, 4 });
    STATE_DIFF diff;
    Compare_CloseState(compare, &diff);
    M_Close(&expected);
    CHECK_EQ_INT(diff.rows, 2);
    CHECK(!diff.differs);
    Arena_Free(&arena);
}

TEST(the_first_difference_keeps_both_rows)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    M_EXPECTED expected;
    M_Expect(&arena, &expected);
    STATE_COMPARE *const compare =
        Compare_OpenState(expected.state, &m_TestGame, &arena);
    Compare_StateRow(compare, 0, (const uint32_t[]) { 1, 2, 3, 4 });
    Compare_StateRow(compare, 1, (const uint32_t[]) { 5, 2, 9, 4 });
    Compare_StateRow(compare, 2, (const uint32_t[]) { 5, 2, 9, 4 });
    STATE_DIFF diff;
    Compare_CloseState(compare, &diff);
    M_Close(&expected);
    CHECK_EQ_INT(diff.rows, 3);
    CHECK(diff.differs);
    CHECK_EQ_INT(diff.first_row, 1);
    CHECK(diff.expected.values != nullptr && diff.actual.values != nullptr);
    if (diff.expected.values != nullptr && diff.actual.values != nullptr) {
        CHECK_EQ_INT(diff.expected.values[0], 1);
        CHECK_EQ_INT(diff.expected.values[3], 3);
        CHECK_EQ_INT(diff.actual.values[3], 9);
    }
    Arena_Free(&arena);
}

TEST(a_list_hash_differs_in_the_running_hash_and_at_the_check)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    M_EXPECTED expected;
    M_Expect(&arena, &expected);
    STATE_COMPARE *const compare =
        Compare_OpenState(expected.state, &m_TestGame, &arena);
    Compare_StateRow(compare, 0, (const uint32_t[]) { 1, 2, 3, 7 });
    Compare_StateRow(compare, 1, (const uint32_t[]) { 5, 2, 3, 7 });
    STATE_DIFF diff;
    Compare_CloseState(compare, &diff);
    M_Close(&expected);
    // The low 4 bits of a hash miss one change in 16, but not in both rows
    // here, and the check finds the change in full.
    CHECK(diff.differs);
    CHECK(diff.check_expected != nullptr && diff.check_actual != nullptr);
    CHECK_EQ_INT(diff.check_row, 1);
    if (diff.check_expected != nullptr && diff.check_actual != nullptr) {
        CHECK_EQ_INT(diff.check_expected[4], 4);
        CHECK_EQ_INT(diff.check_actual[4], 7);
    }
    Arena_Free(&arena);
}

TEST(missing_rows_differ)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    M_EXPECTED expected;
    M_Expect(&arena, &expected);
    STATE_COMPARE *const compare =
        Compare_OpenState(expected.state, &m_TestGame, &arena);
    Compare_StateRow(compare, 0, (const uint32_t[]) { 1, 2, 3, 4 });
    STATE_DIFF diff;
    Compare_CloseState(compare, &diff);
    M_Close(&expected);
    CHECK(diff.differs);
    CHECK_EQ_INT(diff.first_row, 1);
    CHECK(diff.actual.values == nullptr);
    CHECK(diff.expected.values != nullptr && diff.expected.values[0] == 1);
    Arena_Free(&arena);
}

static IMAGE M_Image(const uint8_t value)
{
    IMAGE image = Image_Create(2, 1, &g_HeapAllocator);
    memset(image.pixels, value, 8);
    return image;
}

TEST(images_give_xor_and_scaled_difference)
{
    IMAGE a = M_Image(0x10);
    IMAGE b = M_Image(0x10);
    b.pixels[1] = 0x13;
    b.pixels[2] = 0xf0;
    IMAGE xor_image;
    IMAGE delta;
    SHOT_DIFF diff;
    CHECK(Result_Ignore(Compare_Images(&a, &b, 5, &xor_image, &delta, &diff)));
    CHECK_EQ_INT(diff.frame, 5);
    CHECK_EQ_INT(diff.different_pixels, 1);
    CHECK_EQ_INT(diff.max_delta, 0xe0);
    CHECK_EQ_INT(xor_image.pixels[0], 0);
    CHECK_EQ_INT(xor_image.pixels[1], 0x03);
    CHECK_EQ_INT(xor_image.pixels[3], 255);
    CHECK_EQ_INT(delta.pixels[1], 24);
    CHECK_EQ_INT(delta.pixels[2], 255);
    CHECK_EQ_INT(delta.pixels[4], 0);
    Image_Free(&xor_image);
    Image_Free(&delta);
    Image_Free(&a);
    Image_Free(&b);
}

static void M_Save(
    const char *const dir, const char *const name, const uint8_t value)
{
    char *const shots = TestDir_Join(dir, "shots");
    CHECK(Result_Ignore(File_MakeDir(shots)));
    char *const path = TestDir_Join(shots, name);
    WRITER writer;
    CHECK(Result_Ignore(Writer_OpenFile(&writer, path)));
    IMAGE image = M_Image(value);
    Image_EncodePng(&image, &writer);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    Image_Free(&image);
    Mem_Free(path);
    Mem_Free(shots);
}

TEST(shot_directories_compare_pair_by_pair)
{
    char *const root = TestDir_Prepare("shots");
    char *const a = TestDir_Join(root, "a");
    char *const b = TestDir_Join(root, "b");
    M_Save(a, "000100.png", 1);
    M_Save(b, "000100.png", 1);
    M_Save(a, "000200.png", 1);
    M_Save(b, "000200.png", 2);
    M_Save(a, "000300.png", 1);
    uint32_t different;
    CHECK(Result_Ignore(Compare_Shots(a, b, a, &different)));
    CHECK_EQ_INT(different, 2);
    char *const tsv = TestDir_Join(a, "shots.tsv");
    READER reader;
    CHECK(Result_Ignore(Reader_OpenFile(&reader, tsv)));
    const size_t size = Reader_Remaining(&reader);
    char *const text = Mem_AllocZ(size + 1);
    Reader_ReadBytes(&reader, text, size);
    Reader_Close(&reader);
    CHECK_EQ_STR(
        text,
        "frame\tdifferent_pixels\tmax_delta\n100\t0\t0\n200\t2\t1\n300\t-"
        "1\t0\n");
    char *const xor_path = TestDir_Join(a, "xor/000200.png");
    char *const same_path = TestDir_Join(a, "xor/000100.png");
    char *const delta_path = TestDir_Join(a, "delta/000200.png");
    CHECK(File_Exists(xor_path));
    CHECK(File_Exists(delta_path));
    CHECK(!File_Exists(same_path));
    Mem_Free(xor_path);
    Mem_Free(same_path);
    Mem_Free(delta_path);
    Mem_Free(text);
    Mem_Free(tsv);
    Mem_Free(a);
    Mem_Free(b);
    Mem_Free(root);
}
