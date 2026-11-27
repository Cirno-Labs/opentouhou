#include "tests/test.h"
#include "th02/ending/assets.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

static RESULT M_FontFailure(
    void *const context, const DRAW_TEXT_CHARACTER *const character,
    uint16_t rows[16])
{
    (*(size_t *)context)++;
    return FAIL("Synthetic font failure");
}

TEST(failed_cache_load_clears_all_views_and_leaves_the_group_to_its_owner)
{
    ARENA arena;
    Arena_Init(&arena, "ending failure", 16384);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    ENDING_ASSETS assets;
    memset(&assets, 0xa5, sizeof(assets));
    size_t calls = 0;
    const DRAW_TEXT_FONT font = { M_FontFailure, &calls };
    const ENDING_ASSETS empty = {};
    CHECK(!Result_Ignore(
        EndingAssets_Load(files, &font, Arena_Allocator(&arena), &assets)));
    CHECK(memcmp(&assets, &empty, sizeof(assets)) == 0);
    CHECK_EQ_INT(calls, 1);
    Vfs_Destroy(files);
    Arena_Free(&arena);
}
