#include "shell/data.h"

#include "core/archive/zip.h"
#include "core/vfs.h"
#include "hdi_builder.h"
#include "test.h"
#include "test_dir.h"
#include "zip_builder.h"

static void M_CheckRoot(
    const ZIP_BUILDER_ENTRY *const entries, const int count,
    const char *const expected)
{
    ZIP_BUILDER_ARCHIVE data = ZipBuilder_Build(entries, count);
    READER reader;
    Reader_OpenBuffer(&reader, data.data, data.size, nullptr);
    ARCHIVE *archive;
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    char *const root = ShellData_FindRoot(archive);
    CHECK_EQ_STR(root, expected);
    Mem_Free(root);
    Archive_Close(archive);
    Mem_Free(data.data);
}

TEST(the_root_is_the_one_top_folder_of_every_file)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "東方紅魔郷/", "", 0, 0, false },
        { "東方紅魔郷/th06.exe", "a", 1, 0, false },
        { "東方紅魔郷/data/th06e.dat", "b", 1, 0, false },
    };
    M_CheckRoot(entries, 3, "東方紅魔郷/");
}

TEST(the_root_is_empty_with_a_file_at_the_top)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "game/th06.exe", "a", 1, 0, false },
        { "th06e.dat", "b", 1, 0, false },
    };
    M_CheckRoot(entries, 2, "");
}

TEST(the_root_is_empty_with_two_top_folders)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "game/th06.exe", "a", 1, 0, false },
        { "gamex/th06e.dat", "b", 1, 0, false },
        { "other/", "", 0, 0, false },
    };
    M_CheckRoot(entries, 3, "");
}

static const HDI_BUILDER_ENTRY m_ImageEntries[] = {
    { "OTHER/", "", 0, false },
    { "OTHER/NOTE.TXT", "n", 1, false },
    { "GAME/", "", 0, false },
    { "GAME/TOUHOU.DAT", "dat", 3, false },
};

static HDI_BUILDER_IMAGE M_BuildImage(void)
{
    return HdiBuilder_Build(
        512, false, m_ImageEntries,
        (int)(sizeof(m_ImageEntries) / sizeof(m_ImageEntries[0])));
}

static void M_CheckMounted(const char *const path)
{
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Log(
        ShellData_Mount(files, path, "TOUHOU.DAT"), LOG_LEVEL_ERROR));
    READER reader;
    CHECK(Result_Log(
        Vfs_Open(files, "TOUHOU.DAT", VFS_STREAM, &reader), LOG_LEVEL_ERROR));
    char text[4] = "";
    Reader_ReadBytes(&reader, text, 3);
    CHECK(Result_Ignore(Reader_Result(&reader)));
    CHECK_EQ_STR(text, "dat");
    Reader_Close(&reader);
    Vfs_Destroy(files);
}

TEST(an_image_mounts_below_the_folder_of_the_marker)
{
    char *const directory = TestDir_Prepare("bare_image");
    char *const path = TestDir_Join(directory, "game.zip");
    HDI_BUILDER_IMAGE image = M_BuildImage();
    TestDir_Write(path, image.data, image.size);
    M_CheckMounted(path);
    Mem_Free(image.data);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(a_zip_that_holds_one_image_mounts_the_image)
{
    char *const directory = TestDir_Prepare("held_image");
    char *const path = TestDir_Join(directory, "game.zip");
    HDI_BUILDER_IMAGE image = M_BuildImage();
    const ZIP_BUILDER_ENTRY entries[] = {
        { "ReadMe.txt", "r", 1, 0, false },
        { "Game Files/", "", 0, 0, false },
        { "Game Files/Touhou.HDI", image.data, image.size, 0, true },
    };
    ZIP_BUILDER_ARCHIVE zip = ZipBuilder_Build(entries, 3);
    TestDir_Write(path, zip.data, zip.size);
    M_CheckMounted(path);
    Mem_Free(zip.data);
    Mem_Free(image.data);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(a_zip_that_holds_two_images_fails)
{
    char *const directory = TestDir_Prepare("two_images");
    char *const path = TestDir_Join(directory, "game.zip");
    const ZIP_BUILDER_ENTRY entries[] = {
        { "a.hdi", "a", 1, 0, false },
        { "b/b.hdi", "b", 1, 0, false },
    };
    ZIP_BUILDER_ARCHIVE zip = ZipBuilder_Build(entries, 2);
    TestDir_Write(path, zip.data, zip.size);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    const RESULT result = ShellData_Mount(files, path, "TOUHOU.DAT");
    CHECK(!IS_OK(result));
    CHECK(
        strstr(result.message, "holds more than one .hdi: a.hdi and b/b.hdi")
        != nullptr);
    Result_Ignore(result);
    Vfs_Destroy(files);
    Mem_Free(zip.data);
    Mem_Free(path);
    Mem_Free(directory);
}
