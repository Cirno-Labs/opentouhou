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
    const char *games[APP_GAME_COUNT], int32_t *const count,
    const char *const game, FILE *const out)
{
    int32_t pos = 0;
    while (pos < *count && strcmp(games[pos], game) < 0) {
        pos++;
    }
    if (pos < *count && strcmp(games[pos], game) == 0) {
        return;
    }
    if (*count >= APP_GAME_COUNT) {
        fprintf(
            out, "Ignoring %s: othpatch handles at most %d games.\n", game,
            APP_GAME_COUNT);
        return;
    }
    for (int32_t i = *count; i > pos; i--) {
        games[i] = games[i - 1];
    }
    games[pos] = game;
    (*count)++;
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
    const char *games[APP_GAME_COUNT], int32_t *const count, ARENA *const arena,
    const char *const exe_dir, FILE *const out)
{
    if (exe_dir == nullptr) {
        return;
    }
    FILE_LIST names;
    if (!Result_Ignore(File_ListDir(exe_dir, Arena_Allocator(arena), &names))) {
        return;
    }
    // Checked longest first, so that a suffix is not mistaken for part of
    // the id.
    static const char *const suffixes[] = { ".AppImage", ".exe", ".app", "" };
    for (int32_t i = 0; i < names.count; i++) {
        const char *const name = names.items[i];
        const size_t name_length = strlen(name);
        for (size_t s = 0; s < ARRAY_SIZE(suffixes); s++) {
            const char *const suffix = suffixes[s];
            const size_t suffix_length = strlen(suffix);
            if (!Str_EndsWith(name, suffix) || name_length <= suffix_length) {
                continue;
            }
            const size_t stem_length = name_length - suffix_length;
            if (strncmp(name, "oth", 3) != 0 || stem_length <= 3) {
                break;
            }
            const size_t id_length = stem_length - 3;
            if (id_length == 5 && strncmp(name + 3, "patch", 5) == 0) {
                break;
            }
            char *const game = Arena_Alloc(arena, id_length + 3);
            memcpy(game, "th", 2);
            memcpy(game + 2, name + 3, id_length);
            game[id_length + 2] = '\0';
            Game_AddFound(games, count, game, out);
            break;
        }
    }
}

void Game_FindDownloaded(
    const char *games[APP_GAME_COUNT], int32_t *const count, ARENA *const arena,
    const char *const dir, FILE *const out)
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
        Game_AddFound(games, count, name, out);
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
