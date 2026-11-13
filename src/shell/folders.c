#include "folders.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <string.h>

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
static const char *M_AppImageOf(const FOLDERS_INPUT *const input)
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

static char *M_ProgramDirOf(const FOLDERS_INPUT *const input)
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
    // SDL gives the Resources folder of an application bundle, and the
    // executable sits in its MacOS folder.
    static const char *const suffixes[] = {
        ".app/Contents/Resources/",
        ".app/Contents/MacOS/",
    };
    const size_t length = strlen(dir);
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        const size_t suffix = strlen(suffixes[i]);
        if (length > suffix
            && strcmp(dir + length - suffix, suffixes[i]) == 0) {
            dir[length - suffix + strlen(".app")] = '\0';
            char *const slash = strrchr(dir, '/');
            if (slash != nullptr) {
                slash[1] = '\0';
            }
            break;
        }
    }
    return dir;
}

static bool M_IsPortable(const char *const program_dir)
{
    if (program_dir == nullptr) {
        return false;
    }
    char *const path =
        Path_Join(&g_HeapAllocator, program_dir, FOLDERS_PORTABLE_FILE);
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
    const FOLDERS_INPUT *const input, const char *const game_id,
    FOLDERS *const out)
{
    if (input->user_dir != nullptr) {
        return M_MakeUserDir(input->user_dir, &out->user_dir);
    }
    if (out->mode == FOLDERS_PORTABLE) {
        char *const dir =
            Path_Join(&g_HeapAllocator, out->program_dir, M_PORTABLE_USER_DIR);
        const RESULT result = M_MakeUserDir(dir, &out->user_dir);
        Mem_Free(dir);
        return result;
    }
    return Path_GetUserDir(game_id, &out->user_dir);
}

RESULT Folders_Find(
    const FOLDERS_INPUT *const input, const char *const game_id,
    FOLDERS *const out)
{
    *out = (FOLDERS) {};
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
        out->mode = portable ? FOLDERS_USER_DIR_PORTABLE : FOLDERS_USER_DIR;
    } else {
        out->mode = portable ? FOLDERS_PORTABLE : FOLDERS_DEFAULT;
    }
    return M_FindUserDir(input, game_id, out);
}

RESULT Folders_GetPatchesDir(const FOLDERS *const folders, char **const out)
{
    const bool portable = folders->mode == FOLDERS_PORTABLE
        || folders->mode == FOLDERS_USER_DIR_PORTABLE;
    return Path_GetPatchesDir(portable ? folders->program_dir : nullptr, out);
}

RESULT Folders_FindPatchesDir(
    const FOLDERS_INPUT *const input, char **const out)
{
    char *const program_dir = M_ProgramDirOf(input);
    const RESULT result = Path_GetPatchesDir(
        M_IsPortable(program_dir) ? program_dir : nullptr, out);
    Mem_Free(program_dir);
    return result;
}

const char *Folders_GetModeName(const FOLDERS_MODE mode)
{
    switch (mode) {
    case FOLDERS_DEFAULT:
        return "default";
    case FOLDERS_PORTABLE:
        return "portable (portable.txt is in the program folder)";
    case FOLDERS_USER_DIR:
        return "--user-dir";
    case FOLDERS_USER_DIR_PORTABLE:
        return "--user-dir, with the portable patches folder";
    }
    return "unknown";
}

void Folders_Free(FOLDERS *const folders)
{
    Mem_Free(folders->program_dir);
    Mem_Free(folders->program_name);
    Mem_Free(folders->data_dir);
    Mem_Free(folders->user_dir);
    *folders = (FOLDERS) {};
}
