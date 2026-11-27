#include "tests/test.h"
#include "tests/test_dir.h"
#include "tests/test_fonts.h"

#include "core/base/memory.h"
#include "core/io/file.h"
#include "core/text.h"
#include "core/text/font_registry.h"
#include "core/vfs.h"
#include "shell/game/fonts.h"

#include <string.h>

TEST(the_built_in_fonts_follow_the_font_option)
{
    CHECK(IS_OK(Text_Init()));
    CHECK(IS_OK(ShellFonts_Reset(false)));
    FONT *gothic = nullptr;
    FONT *ui = nullptr;
    const bool has_gothic =
        Result_Ignore(FontRegistry_Open("IPAGothic", 30, &gothic));
    const bool has_ui = Result_Ignore(FontRegistry_Open("IPAPGothic", 30, &ui));
    CHECK(has_gothic == TEST_BUILTIN_FONTS);
    CHECK(has_ui == TEST_BUILTIN_FONTS);
    Font_Free(ui);
    Font_Free(gothic);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(a_folder_adds_its_fonts_and_font_files)
{
    CHECK(IS_OK(Text_Init()));
    char *const dir = TestDir_Prepare("folder");
    char *const fonts = TestDir_Join(dir, "fonts");
    CHECK(IS_OK(File_MakeDir(fonts)));
    size_t size;
    uint8_t *const data = TestFonts_Read(TEST_FONT_UI_GOTHIC, &size);
    char *const font = TestDir_Join(fonts, "UI.TTF");
    TestDir_Write(font, data, size);
    char *const broken = TestDir_Join(fonts, "broken.otf");
    TestDir_Write(broken, "not a font", 10);
    char *const readme = TestDir_Join(fonts, "readme.txt");
    TestDir_Write(readme, "IPAGothic", 9);
    char *const rom = TestDir_Join(fonts, "FONT.ROM");
    TestDir_Write(rom, "rom", 3);
    char *const bdf = TestDir_Join(fonts, "shnmk16.bdf");
    TestDir_Write(bdf, "STARTFONT", 9);
    char *const bmp = TestDir_Join(fonts, "Nec.Bmp");
    TestDir_Write(bmp, "BM", 2);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    CHECK(IS_OK(Vfs_MountDir(files, "", dir, 0, false)));
    FontRegistry_Reset(false);
    ShellFonts_AddDir(files, "missing");
    ShellFonts_AddDir(files, "fonts");
    FONT *opened = nullptr;
    CHECK(Result_Ignore(FontRegistry_Open("IPAPGothic", 30, &opened)));
    Font_Free(opened);
    CHECK(!Result_Ignore(FontRegistry_Open("IPAGothic", 30, &opened)));
    const void *found = nullptr;
    size_t found_size = 0;
    CHECK(FontRegistry_FindFile("font.rom", &found, &found_size));
    CHECK(found_size == 3 && memcmp(found, "rom", 3) == 0);
    CHECK(FontRegistry_FindFile("SHNMK16.BDF", &found, &found_size));
    CHECK(found_size == 9 && memcmp(found, "STARTFONT", 9) == 0);
    CHECK(FontRegistry_FindFile("NEC.BMP", &found, &found_size));
    CHECK(found_size == 2);
    CHECK(!FontRegistry_FindFile("readme.txt", &found, &found_size));
    CHECK(!FontRegistry_FindFile("UI.TTF", &found, &found_size));
    Vfs_Destroy(files);
    Mem_Free(bmp);
    Mem_Free(bdf);
    Mem_Free(rom);
    Mem_Free(readme);
    Mem_Free(broken);
    Mem_Free(font);
    Mem_Free(data);
    Mem_Free(fonts);
    Mem_Free(dir);
    FontRegistry_Reset(false);
    Text_Shutdown();
}
