#include "th01/sprite_sheet.h"

#include "test.h"
#include "test_dir.h"

#include <string.h>

// Writes a 16 by 4 one-bit BMP, bottom-up, whose palette index 0 is white.
static void M_WriteSheet(const char *const path)
{
    uint8_t file[62 + 4 * 4] = { 'B', 'M' };
    const uint32_t header[] = { sizeof(file), 0, 62, 40, 16, 4 };
    memcpy(file + 2, header, sizeof(header));
    file[26] = 1;
    file[28] = 1;
    memset(file + 54, 0xff, 3);
    // The rows from the top: the inverse of 0x10 0x08, 0x20 0x04, 0x40 0x02
    // and 0x80 0x01.
    const uint8_t rows[4][2] = {
        { 0xef, 0xf7 }, { 0xdf, 0xfb }, { 0xbf, 0xfd }, { 0x7f, 0xfe }
    };
    for (int32_t y = 0; y < 4; y++) {
        memcpy(file + 62 + (3 - y) * 4, rows[y], 2);
    }
    TestDir_Write(path, file, sizeof(file));
}

TEST(cels_run_left_to_right_then_top_to_bottom)
{
    char *const dir = TestDir_Prepare("sheet");
    char *const path = TestDir_Join(dir, "sheet.bmp");
    M_WriteSheet(path);
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(IS_OK(Vfs_MountDir(vfs, "", dir, 0, false)));
    uint8_t *cels;
    size_t size;
    CHECK(IS_OK(SpriteSheet_Load(
        vfs, "sheet.bmp", 8, 2, &g_HeapAllocator, &cels, &size)));
    const uint8_t expected[] = {
        0x10, 0x20, 0x08, 0x04, 0x40, 0x80, 0x02, 0x01
    };
    CHECK_EQ_INT(size, sizeof(expected));
    CHECK(memcmp(cels, expected, sizeof(expected)) == 0);
    Mem_Free(cels);
    CHECK(!Result_Ignore(SpriteSheet_Load(
        vfs, "sheet.bmp", 8, 3, &g_HeapAllocator, &cels, &size)));
    CHECK(cels == nullptr);
    CHECK(!Result_Ignore(SpriteSheet_Load(
        vfs, "none.bmp", 8, 2, &g_HeapAllocator, &cels, &size)));
    Vfs_Destroy(vfs);
    Mem_Free(path);
    Mem_Free(dir);
}
