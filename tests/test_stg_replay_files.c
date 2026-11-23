#include "stg/replay_files.h"

#include "test.h"

#include "core/base/memory.h"

static uint8_t *M_Bytes(const uint8_t first, const size_t size)
{
    uint8_t *const bytes = Mem_Alloc(size);
    for (size_t i = 0; i < size; i++) {
        bytes[i] = (uint8_t)(first + i);
    }
    return bytes;
}

TEST(find_returns_the_bytes_added_for_a_path)
{
    uint8_t *const bytes = M_Bytes(1, 3);
    ReplayFiles_Add("a.rpy", bytes, 3);
    size_t size = 0;
    CHECK(ReplayFiles_Find("a.rpy", &size) == bytes);
    CHECK_EQ_INT(size, 3);
    CHECK(ReplayFiles_Find("b.rpy", &size) == nullptr);
    ReplayFiles_Clear();
}

TEST(add_replaces_the_bytes_for_the_same_path)
{
    ReplayFiles_Add("a.rpy", M_Bytes(1, 3), 3);
    ReplayFiles_Add("b.rpy", M_Bytes(7, 1), 1);
    ReplayFiles_Add("a.rpy", M_Bytes(4, 2), 2);
    size_t size = 0;
    const uint8_t *data = ReplayFiles_Find("a.rpy", &size);
    CHECK(data != nullptr && size == 2 && data[0] == 4);
    data = ReplayFiles_Find("b.rpy", &size);
    CHECK(data != nullptr && size == 1 && data[0] == 7);
    ReplayFiles_Clear();
}

TEST(add_accepts_an_empty_file)
{
    ReplayFiles_Add("a.rpy", nullptr, 0);
    size_t size = 1;
    ReplayFiles_Find("a.rpy", &size);
    CHECK_EQ_INT(size, 0);
    ReplayFiles_Clear();
}

TEST(remove_forgets_one_path)
{
    ReplayFiles_Add("a.rpy", M_Bytes(1, 3), 3);
    ReplayFiles_Add("b.rpy", M_Bytes(4, 1), 1);
    ReplayFiles_Remove("a.rpy");
    ReplayFiles_Remove("missing.rpy");
    size_t size = 0;
    CHECK(ReplayFiles_Find("a.rpy", &size) == nullptr);
    CHECK(ReplayFiles_Find("b.rpy", &size) != nullptr);
    ReplayFiles_Clear();
    CHECK(ReplayFiles_Find("b.rpy", &size) == nullptr);
}
