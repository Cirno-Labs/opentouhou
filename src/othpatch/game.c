#include "othpatch/game.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/process.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void Game_GetProductName(const char *const game, char name[GAME_NAME_SIZE])
{
    snprintf(name, GAME_NAME_SIZE, "o%s", game);
}

void Game_AddFound(
    GAME_LIST *const list, const char *const game, FILE *const out)
{
    int32_t pos = 0;
    while (pos < list->count && strcmp(list->items[pos], game) < 0) {
        pos++;
    }
    if (pos < list->count && strcmp(list->items[pos], game) == 0) {
        return;
    }
    if (list->count >= GAME_MAX_COUNT) {
        fprintf(
            out, "Ignoring %s: othpatch handles at most %d games.\n", game,
            GAME_MAX_COUNT);
        return;
    }
    for (int32_t i = list->count; i > pos; i--) {
        list->items[i] = list->items[i - 1];
    }
    list->items[pos] = game;
    list->count++;
}

// Returns whether name is "th" followed by one or more digits.
static bool M_IsGameId(const char *const name)
{
    if (!Str_StartsWith(name, "th")) {
        return false;
    }
    const char *digits = name + 2;
    if (*digits == '\0') {
        return false;
    }
    for (; *digits != '\0'; digits++) {
        if (!isdigit((unsigned char)*digits)) {
            return false;
        }
    }
    return true;
}

void Game_FindBeside(
    GAME_LIST *const list, ARENA *const arena, const char *const exe_dir,
    FILE *const out)
{
    if (exe_dir == nullptr) {
        return;
    }
    FILE_LIST names;
    if (!Result_Ignore(File_ListDir(exe_dir, Arena_Allocator(arena), &names))) {
        return;
    }
    static const char *const suffixes[] = { ".exe", ".AppImage", ".app" };
    for (int32_t i = 0; i < names.count; i++) {
        const char *const name = names.items[i];
        if (!Str_StartsWith(name, "oth")) {
            continue;
        }
        size_t stem_length = strlen(name);
        for (size_t s = 0; s < ARRAY_SIZE(suffixes); s++) {
            if (Str_EndsWith(name, suffixes[s])) {
                stem_length -= strlen(suffixes[s]);
                break;
            }
        }
        char *const game = Arena_Alloc(arena, stem_length);
        memcpy(game, name + 1, stem_length - 1);
        game[stem_length - 1] = '\0';
        if (M_IsGameId(game)) {
            Game_AddFound(list, game, out);
        }
    }
}

void Game_FindDownloaded(
    GAME_LIST *const list, ARENA *const arena, const char *const dir,
    FILE *const out)
{
    FILE_LIST names;
    if (!Result_Ignore(File_ListDir(dir, Arena_Allocator(arena), &names))) {
        return;
    }
    for (int32_t i = 0; i < names.count; i++) {
        const char *const name = names.items[i];
        if (!M_IsGameId(name)) {
            continue;
        }
        FILE_TYPE type;
        const char *const path = Path_Join(Arena_Allocator(arena), dir, name);
        if (!Result_Ignore(File_GetType(path, &type))
            || type != FILE_TYPE_DIR) {
            continue;
        }
        Game_AddFound(list, name, out);
    }
}

RESULT Game_GetVersion(const char *const path, char version[GAME_VERSION_SIZE])
{
    version[0] = '\0';
    const char *const args[] = { path, "--version", nullptr };
    char *output = nullptr;
    int status = -1;
    MUST(Process_Run(args, &output, &status));
    output[strcspn(output, "\r\n")] = '\0';
    const char *const space = strrchr(output, ' ');
    const char *const word = space != nullptr ? space + 1 : "";
    const size_t length = strlen(word);
    RESULT result = OK;
    if (status != 0) {
        result = FAIL("it ended with status %d", status);
    } else if (space == nullptr || length == 0 || length >= GAME_VERSION_SIZE) {
        result = FAIL("it printed no version");
    } else {
        memcpy(version, word, length + 1);
    }
    Mem_Free(output);
    return result;
}
