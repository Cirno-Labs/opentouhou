#include "shell/game/fonts.h"

#include "core/text.h"
#include "core/vfs.h"

#include "test.h"
#include "test_dir.h"
#include "test_fonts.h"

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

TEST(a_folder_adds_its_font_files)
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
    TestDir_Write(readme, "IPAGothic", 13);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    CHECK(IS_OK(Vfs_MountDir(files, "", dir, 0, false)));
    FontRegistry_Reset(false);
    ShellFonts_AddDir(files, "missing");
    ShellFonts_AddDir(files, "fonts");
    FONT *opened = nullptr;
    CHECK(Result_Ignore(FontRegistry_Open("IPAPGothic", 30, &opened)));
    Font_Free(opened);
    CHECK(!Result_Ignore(FontRegistry_Open("IPAGothic", 30, &opened)));
    Vfs_Destroy(files);
    Mem_Free(readme);
    Mem_Free(broken);
    Mem_Free(font);
    Mem_Free(data);
    Mem_Free(fonts);
    Mem_Free(dir);
    FontRegistry_Reset(false);
    Text_Shutdown();
}
