#include "folders.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <string.h>

#define M_PORTABLE_FILE "portable.txt"
#define M_PORTABLE_USER_DIR "userdata"

#ifdef _WIN32
    #define M_SEPARATOR "\\"
#else
    #define M_SEPARATOR "/"
#endif

static bool M_IsSeparator(const char c)
{
    return c == '/' || c == '\\';
}

static const char *M_FileName(const char *const path)
{
    const char *name = path;
    for (const char *c = path; *c != '\0'; c++) {
        if (M_IsSeparator(*c)) {
            name = c + 1;
        }
    }
    return name;
}

// Returns path with one separator at its end.
static char *M_AsDir(const char *const path)
{
    const size_t length = strlen(path);
    if (length > 0 && M_IsSeparator(path[length - 1])) {
        return Str_Dup(path);
    }
    return Str_Format("%s" M_SEPARATOR, path);
}

// Returns the .AppImage file that holds the executable, or nullptr outside
// an AppImage.
static const char *M_AppImageOf(const APP_FOLDERS_INPUT *const input)
{
#ifdef __linux__
    if (input->appimage != nullptr && input->appimage[0] == '/'
        && input->appdir != nullptr && input->appdir[0] != '\0'
        && input->exe_dir != nullptr
        && Str_StartsWith(input->exe_dir, input->appdir)) {
        return input->appimage;
    }
#endif
    return nullptr;
}

static char *M_ProgramDirOf(const APP_FOLDERS_INPUT *const input)
{
    const char *const appimage = M_AppImageOf(input);
    if (appimage != nullptr) {
        const size_t length = (size_t)(M_FileName(appimage) - appimage);
        char *const dir = Mem_Alloc(length + 1);
        memcpy(dir, appimage, length);
        dir[length] = '\0';
        return dir;
    }
    if (input->exe_dir == nullptr) {
        return nullptr;
    }
    char *const dir = M_AsDir(input->exe_dir);
#ifdef __APPLE__
    static const char suffix[] = ".app/Contents/MacOS/";
    const size_t length = strlen(dir);
    if (length > sizeof(suffix) - 1
        && strcmp(dir + length - (sizeof(suffix) - 1), suffix) == 0) {
        dir[length - (sizeof("/Contents/MacOS/") - 1)] = '\0';
        char *const slash = strrchr(dir, '/');
        if (slash != nullptr) {
            slash[1] = '\0';
        }
    }
#endif
    return dir;
}

static bool M_IsPortable(const char *const program_dir)
{
    if (program_dir == nullptr) {
        return false;
    }
    char *const path =
        Path_Join(&g_HeapAllocator, program_dir, M_PORTABLE_FILE);
    const bool portable = File_Exists(path);
    Mem_Free(path);
    return portable;
}

static RESULT M_MakeUserDir(const char *const dir, char **const out)
{
    MUST(File_MakeDir(dir), "the user folder");
    *out = M_AsDir(dir);
    return OK;
}

static RESULT M_FindUserDir(
    const APP_FOLDERS_INPUT *const input, const char *const game_id,
    APP_FOLDERS *const out)
{
    if (input->user_dir != nullptr) {
        return M_MakeUserDir(input->user_dir, &out->user_dir);
    }
    if (out->mode == APP_FOLDERS_PORTABLE) {
        char *const dir =
            Path_Join(&g_HeapAllocator, out->program_dir, M_PORTABLE_USER_DIR);
        const RESULT result = M_MakeUserDir(dir, &out->user_dir);
        Mem_Free(dir);
        return result;
    }
    return Path_GetUserDir(game_id, &out->user_dir);
}

RESULT AppFolders_Find(
    const APP_FOLDERS_INPUT *const input, const char *const game_id,
    APP_FOLDERS *const out)
{
    *out = (APP_FOLDERS) {};
    out->program_dir = M_ProgramDirOf(input);
    const char *const appimage = M_AppImageOf(input);
    const char *const program = appimage != nullptr ? appimage
        : input->program != nullptr                 ? input->program
                                                    : game_id;
    out->program_name = Str_Dup(M_FileName(program));
    if (input->data_dir != nullptr) {
        out->data_dir = Str_Dup(input->data_dir);
    } else if (out->program_dir != nullptr) {
        out->data_dir = Str_Dup(out->program_dir);
    }
    const bool portable = M_IsPortable(out->program_dir);
    if (input->user_dir != nullptr) {
        out->mode =
            portable ? APP_FOLDERS_USER_DIR_PORTABLE : APP_FOLDERS_USER_DIR;
    } else {
        out->mode = portable ? APP_FOLDERS_PORTABLE : APP_FOLDERS_DEFAULT;
    }
    return M_FindUserDir(input, game_id, out);
}

RESULT AppFolders_GetPatchesDir(
    const APP_FOLDERS *const folders, char **const out)
{
    const bool portable = folders->mode == APP_FOLDERS_PORTABLE
        || folders->mode == APP_FOLDERS_USER_DIR_PORTABLE;
    return Path_GetPatchesDir(portable ? folders->program_dir : nullptr, out);
}

const char *AppFolders_GetModeName(const APP_FOLDERS_MODE mode)
{
    switch (mode) {
    case APP_FOLDERS_DEFAULT:
        return "default";
    case APP_FOLDERS_PORTABLE:
        return "portable (portable.txt is in the program folder)";
    case APP_FOLDERS_USER_DIR:
        return "--user-dir";
    case APP_FOLDERS_USER_DIR_PORTABLE:
        return "--user-dir, with the portable patches folder";
    }
    return "unknown";
}

void AppFolders_Free(APP_FOLDERS *const folders)
{
    Mem_Free(folders->program_dir);
    Mem_Free(folders->program_name);
    Mem_Free(folders->data_dir);
    Mem_Free(folders->user_dir);
    *folders = (APP_FOLDERS) {};
}
