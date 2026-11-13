#include "screenshot.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/gfx.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/writer.h"

#include <SDL3/SDL.h>

static RESULT M_Path(
    const char *const user_dir, const char *const game_id,
    const APP_SETTINGS *const settings, char **const out)
{
    *out = nullptr;
    FAIL_IF(user_dir == nullptr, "No user directory for screenshots");
    SDL_Time now;
    SDL_DateTime date;
    FAIL_IF(
        !SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &date, true),
        "Cannot read screenshot date: %s", SDL_GetError());
    char *const directory =
        Path_Join(&g_HeapAllocator, user_dir, "screenshots");
    const RESULT made = File_MakeDir(directory);
    if (!IS_OK(made)) {
        Mem_Free(directory);
        return made;
    }
    const char *const extension =
        settings->screenshot_format == 0 ? "png" : "jpg";
    for (uint32_t suffix = 1; suffix != 0; suffix++) {
        char *const name = suffix == 1
            ? Str_Format(
                  "%s_%04d%02d%02d_%02d%02d%02d.%s", game_id, date.year,
                  date.month, date.day, date.hour, date.minute, date.second,
                  extension)
            : Str_Format(
                  "%s_%04d%02d%02d_%02d%02d%02d_%u.%s", game_id, date.year,
                  date.month, date.day, date.hour, date.minute, date.second,
                  suffix, extension);
        char *const path = Path_Join(&g_HeapAllocator, directory, name);
        Mem_Free(name);
        if (!File_Exists(path)) {
            *out = path;
            break;
        }
        Mem_Free(path);
    }
    Mem_Free(directory);
    FAIL_IF(*out == nullptr, "No unused screenshot name");
    return OK;
}

RESULT Screenshot_Save(
    const char *const user_dir, const char *const game_id,
    const APP_SETTINGS *const settings)
{
    IMAGE image = {};
    MUST(
        settings->screenshot_resolution == 0
            ? Gfx_ReadScreen(&g_HeapAllocator, &image)
            : Gfx_ReadOutput(&g_HeapAllocator, &image));
    const size_t pixels = (size_t)image.width * (size_t)image.height;
    for (size_t i = 0; i < pixels; i++) {
        image.pixels[i * 4 + 3] = 255;
    }
    char *path = nullptr;
    RESULT result = M_Path(user_dir, game_id, settings, &path);
    if (IS_OK(result)) {
        WRITER writer;
        result = Writer_OpenFile(&writer, path);
        if (IS_OK(result)) {
            if (settings->screenshot_format == 0) {
                Image_EncodePng(&image, &writer);
            } else {
                Image_EncodeJpeg(
                    &image, settings->screenshot_jpeg_quality, &writer);
            }
            result = Writer_Close(&writer);
            if (IS_OK(result)) {
                LOG_INFO("Saved screenshot: %s", path);
            }
        }
    }
    Mem_Free(path);
    Image_Free(&image);
    return result;
}
