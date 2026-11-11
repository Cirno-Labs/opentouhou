#include "core/app/screenshot.h"
#include "core/gfx.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include "test.h"
#include "test_dir.h"

#define CHECK_OK(expression_) CHECK(Result_Log((expression_), LOG_LEVEL_ERROR))

static int m_ScreenCaptures;
static int m_OutputCaptures;

RESULT Gfx_ReadScreen(const ALLOCATOR *const allocator, IMAGE *const out)
{
    m_ScreenCaptures++;
    *out = Image_Create(2, 1, allocator);
    memcpy(out->pixels, (uint8_t[]) { 200, 20, 40, 73, 200, 20, 40, 73 }, 8);
    return OK;
}

RESULT Gfx_ReadOutput(const ALLOCATOR *const allocator, IMAGE *const out)
{
    m_OutputCaptures++;
    *out = Image_Create(4, 2, allocator);
    for (size_t i = 0; i < 8; i++) {
        memcpy(out->pixels + i * 4, (uint8_t[]) { 200, 20, 40, 73 }, 4);
    }
    return OK;
}

TEST(saves_both_resolutions_without_overwriting_and_flattens_alpha)
{
    char *const directory = TestDir_Prepare("screenshot");
    const APP_SETTINGS settings = { .screenshot_jpeg_quality = 90 };
    m_ScreenCaptures = 0;
    m_OutputCaptures = 0;
    CHECK_OK(Screenshot_Save(directory, "test", &settings));
    CHECK_OK(Screenshot_Save(directory, "test", &settings));
    APP_SETTINGS window = settings;
    window.screenshot_resolution = 1;
    CHECK_OK(Screenshot_Save(directory, "test", &window));
    CHECK_EQ_INT(m_ScreenCaptures, 2);
    CHECK_EQ_INT(m_OutputCaptures, 1);
    char *const screenshots =
        Path_Join(&g_HeapAllocator, directory, "screenshots");
    FILE_LIST files;
    CHECK_OK(File_ListDir(screenshots, &g_HeapAllocator, &files));
    CHECK_EQ_INT(files.count, 3);
    int native_images = 0;
    int window_images = 0;
    for (int32_t i = 0; i < files.count; i++) {
        CHECK(strstr(files.items[i], "test_") == files.items[i]);
        CHECK(strstr(files.items[i], ".png") != nullptr);
        char *const path =
            Path_Join(&g_HeapAllocator, screenshots, files.items[i]);
        READER reader;
        CHECK_OK(Reader_OpenFile(&reader, path));
        IMAGE image;
        CHECK_OK(Image_Decode(&reader, &g_HeapAllocator, &image));
        if (image.width == 2 && image.height == 1) {
            native_images++;
        } else if (image.width == 4 && image.height == 2) {
            window_images++;
        } else {
            TEST_FAIL("Unexpected screenshot dimensions");
        }
        const size_t count = (size_t)image.width * (size_t)image.height;
        for (size_t p = 0; p < count; p++) {
            CHECK_EQ_INT(image.pixels[p * 4], 200);
            CHECK_EQ_INT(image.pixels[p * 4 + 3], 255);
        }
        Image_Free(&image);
        Reader_Close(&reader);
        Mem_Free(path);
    }
    CHECK_EQ_INT(native_images, 2);
    CHECK_EQ_INT(window_images, 1);
    File_FreeList(&files);
    Mem_Free(screenshots);
    Mem_Free(directory);
}

TEST(saves_jpeg_and_reports_unavailable_user_directory)
{
    char *const directory = TestDir_Prepare("screenshot_jpeg");
    const APP_SETTINGS settings = { .screenshot_format = 1,
                                    .screenshot_jpeg_quality = 90 };
    CHECK_OK(Screenshot_Save(directory, "test", &settings));
    char *const screenshots =
        Path_Join(&g_HeapAllocator, directory, "screenshots");
    FILE_LIST files;
    CHECK_OK(File_ListDir(screenshots, &g_HeapAllocator, &files));
    CHECK_EQ_INT(files.count, 1);
    char *const path = Path_Join(&g_HeapAllocator, screenshots, files.items[0]);
    CHECK(strstr(path, ".jpg") != nullptr);
    READER reader;
    CHECK_OK(Reader_OpenFile(&reader, path));
    IMAGE image;
    CHECK_OK(Image_Decode(&reader, &g_HeapAllocator, &image));
    CHECK_EQ_INT(image.width, 2);
    CHECK_EQ_INT(image.height, 1);
    Image_Free(&image);
    Reader_Close(&reader);
    CHECK(!Result_Ignore(Screenshot_Save(nullptr, "test", &settings)));
    Mem_Free(path);
    File_FreeList(&files);
    Mem_Free(screenshots);
    Mem_Free(directory);
}
