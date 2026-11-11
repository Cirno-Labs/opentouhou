#include "core/io/path.h"

#include "core/base/string.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_ORGANIZATION "opentouhou"
#define M_PATCHES_DIR "patches"

char *Path_Join(
    const ALLOCATOR *const allocator, const char *const a, const char *const b)
{
    const size_t a_length = strlen(a);
    const size_t b_length = strlen(b);
    const bool separated =
        a_length == 0 || a[a_length - 1] == '/' || a[a_length - 1] == '\\';
    const size_t separator_length = separated ? 0 : 1;
    char *const path =
        Allocator_Alloc(allocator, a_length + separator_length + b_length + 1);
    memcpy(path, a, a_length);
    if (!separated) {
        path[a_length] = '/';
    }
    memcpy(path + a_length + separator_length, b, b_length + 1);
    return path;
}

const char *Path_GetExeDir(void)
{
    return SDL_GetBasePath();
}

RESULT Path_GetUserDir(const char *const game_id, char **const out)
{
    *out = nullptr;
    char *const pref_path = SDL_GetPrefPath(M_ORGANIZATION, game_id);
    FAIL_IF(
        pref_path == nullptr, "Cannot find the user directory: %s",
        SDL_GetError());
    *out = Str_Dup(pref_path);
    SDL_free(pref_path);
    return OK;
}

RESULT Path_GetPatchesDir(char **const out)
{
    MUST(Path_GetUserDir(M_PATCHES_DIR, out), "the patches folder");
    return OK;
}
