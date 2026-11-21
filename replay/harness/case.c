#include "case.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/ini.h"
#include "core/vfs/normalize.h"

#include <inttypes.h>
#include <string.h>

#define M_FIELD_SIZE 16
#define M_FILES "files/"
#define M_PRECONDITIONS "preconditions"
#define M_CHOICES "choices"
#define M_START "start"
#define M_STAGE "stage "
#define M_ARROW " -> "

// Returns a copy of section.key from arena, or nullptr when it is missing
// or empty.
static char *M_Get(
    const INI *const ini, ARENA *const arena, const char *const section,
    const char *const key)
{
    const char *const text = Ini_Get(ini, section, key);
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }
    return Str_DupWith(Arena_Allocator(arena), text);
}

static RESULT M_GetU32(
    const INI *const ini, const char *const section, const char *const key,
    uint32_t *const out)
{
    const char *const text = Ini_Get(ini, section, key);
    *out = 0;
    if (text == nullptr) {
        return OK;
    }
    FAIL_IF(!Str_ParseU32(text, out), "bad %s.%s: %s", section, key, text);
    return OK;
}

static void M_SetString(
    INI *const ini, const char *const section, const char *const key,
    const char *const value)
{
    if (value != nullptr) {
        Ini_Set(ini, section, key, value);
    }
}

static void M_SetU32(
    INI *const ini, const char *const section, const char *const key,
    const uint32_t value)
{
    char text[M_FIELD_SIZE];
    snprintf(text, sizeof(text), "%" PRIu32, value);
    Ini_Set(ini, section, key, text);
}

RESULT Case_CheckStep(const STAGE_STEP *const step)
{
    const bool prefixed = Str_StartsWith(step->source, M_FILES);
    const char *const name = prefixed ? step->source + strlen(M_FILES) : "";
    FAIL_IF(
        !prefixed || name[0] == '\0' || strpbrk(name, "/\\") != nullptr
            || strcmp(name, "..") == 0,
        "staged source %s is not " M_FILES "NAME", step->source);
    char *normal;
    MUST(VfsNormalize_Path(step->path, &g_HeapAllocator, &normal));
    const bool same = strcmp(normal, step->path) == 0;
    Allocator_Free(&g_HeapAllocator, normal, strlen(normal) + 1);
    FAIL_IF(!same, "staged path %s is not in normal form", step->path);
    return OK;
}

// Reads "stage SOURCE -> PATH" from text, which it changes.
static RESULT M_ReadStep(char *const text, STAGE_STEP *const out)
{
    FAIL_IF(text == nullptr, "empty precondition");
    FAIL_IF(!Str_StartsWith(text, M_STAGE), "bad precondition %s", text);
    char *const source = text + strlen(M_STAGE);
    char *const arrow = strstr(source, M_ARROW);
    FAIL_IF(
        arrow == nullptr || arrow == source || arrow[strlen(M_ARROW)] == '\0',
        "bad precondition %s", text);
    *arrow = '\0';
    *out = (STAGE_STEP) { .source = source, .path = arrow + strlen(M_ARROW) };
    return Case_CheckStep(out);
}

static RESULT M_ReadSteps(
    const INI *const ini, ARENA *const arena, CASE_MANIFEST *const out)
{
    size_t count = 0;
    char key[M_FIELD_SIZE];
    for (;; count++) {
        snprintf(key, sizeof(key), "%zu", count + 1);
        if (Ini_Get(ini, M_PRECONDITIONS, key) == nullptr) {
            break;
        }
    }
    STAGE_STEP *const steps =
        Arena_AllocArrZ(arena, count > 0 ? count : 1, sizeof(STAGE_STEP));
    for (size_t i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "%zu", i + 1);
        MUST(
            M_ReadStep(M_Get(ini, arena, M_PRECONDITIONS, key), &steps[i]),
            M_PRECONDITIONS ".%s", key);
    }
    out->steps = steps;
    out->step_count = count;
    return OK;
}

static RESULT M_ReadChoices(
    const INI *const ini, const HARNESS_GAME *const game, ARENA *const arena,
    CASE_MANIFEST *const out)
{
    out->choices = Arena_AllocArrZ(
        arena, game->choice_count > 0 ? game->choice_count : 1,
        sizeof(uint32_t));
    for (size_t i = 0; i < game->choice_count; i++) {
        const char *const name = game->choices[i].name;
        FAIL_IF(
            Ini_Get(ini, M_CHOICES, name) == nullptr,
            M_CHOICES ".%s is missing", name);
        MUST(M_GetU32(ini, M_CHOICES, name, &out->choices[i]));
    }
    return OK;
}

static RESULT M_ReadStarts(
    const INI *const ini, const HARNESS_GAME *const game, ARENA *const arena,
    CASE_MANIFEST *const out)
{
    START_VALUE *const starts = Arena_AllocArrZ(
        arena, game->start_count > 0 ? game->start_count : 1,
        sizeof(START_VALUE));
    size_t count = 0;
    for (size_t i = 0; i < game->start_count; i++) {
        const COLUMN *const field = &game->starts[i];
        const char *const text = Ini_Get(ini, M_START, field->name);
        if (text == nullptr) {
            continue;
        }
        starts[count] = (START_VALUE) { .field = (uint32_t)i };
        FAIL_IF(
            !Game_ParseValue(field->kind, text, &starts[count].value),
            "bad " M_START ".%s: %s", field->name, text);
        count++;
    }
    out->starts = starts;
    out->start_count = count;
    return OK;
}

static RESULT M_ReadManifest(
    const INI *const ini, const HARNESS_GAME *const game, ARENA *const arena,
    CASE_MANIFEST *const out)
{
    CASE_MANIFEST manifest = {};
    MUST(M_GetU32(ini, "case", "format", &manifest.format));
    FAIL_IF(
        manifest.format != CASE_FORMAT, "case format %u is not %d",
        manifest.format, CASE_FORMAT);
    manifest.game = M_Get(ini, arena, "case", "game");
    FAIL_IF(manifest.game == nullptr, "case.game is missing");
    manifest.game_version = M_Get(ini, arena, "case", "game_version");
    manifest.backend = M_Get(ini, arena, "case", "backend");
    manifest.note = M_Get(ini, arena, "case", "note");
    MUST(M_GetU32(ini, "case", "columns_version", &manifest.columns_version));
    MUST(M_GetU32(ini, "case", "frames", &manifest.frames));
    MUST(M_GetU32(ini, "case", "rows", &manifest.rows));
    MUST(M_ReadChoices(ini, game, arena, &manifest));
    MUST(M_ReadSteps(ini, arena, &manifest));
    MUST(M_ReadStarts(ini, game, arena, &manifest));
    *out = manifest;
    return OK;
}

RESULT Case_ReadManifest(
    READER *const in, const HARNESS_GAME *const game, ARENA *const arena,
    CASE_MANIFEST *const out)
{
    size_t size;
    char *const text = Reader_ReadCString(in, &g_HeapAllocator, &size);
    if (text == nullptr) {
        return Reader_Result(in);
    }
    INI *ini;
    RESULT result = Ini_Parse(text, size, &g_HeapAllocator, &ini);
    Allocator_Free(&g_HeapAllocator, text, size + 1);
    if (IS_OK(result)) {
        result = M_ReadManifest(ini, game, arena, out);
        Ini_Free(ini);
    }
    return Result_Prefix(result, "%s", Reader_GetName(in));
}

void Case_WriteManifest(
    WRITER *const out, const HARNESS_GAME *const game,
    const CASE_MANIFEST *const manifest)
{
    INI *ini;
    if (!Result_Log(
            Ini_Parse("", 0, &g_HeapAllocator, &ini), LOG_LEVEL_ERROR)) {
        return;
    }
    M_SetU32(ini, "case", "format", manifest->format);
    M_SetString(ini, "case", "game", manifest->game);
    M_SetString(ini, "case", "game_version", manifest->game_version);
    M_SetString(ini, "case", "backend", manifest->backend);
    M_SetU32(ini, "case", "columns_version", manifest->columns_version);
    M_SetU32(ini, "case", "frames", manifest->frames);
    M_SetU32(ini, "case", "rows", manifest->rows);
    M_SetString(ini, "case", "note", manifest->note);
    for (size_t i = 0; i < game->choice_count; i++) {
        M_SetU32(ini, M_CHOICES, game->choices[i].name, manifest->choices[i]);
    }
    for (size_t i = 0; i < manifest->step_count; i++) {
        const STAGE_STEP *const step = &manifest->steps[i];
        char key[M_FIELD_SIZE];
        snprintf(key, sizeof(key), "%zu", i + 1);
        char *const value =
            Str_Format(M_STAGE "%s" M_ARROW "%s", step->source, step->path);
        Ini_Set(ini, M_PRECONDITIONS, key, value);
        Mem_Free(value);
    }
    for (size_t i = 0; i < manifest->start_count; i++) {
        const START_VALUE *const start = &manifest->starts[i];
        const COLUMN *const field = &game->starts[start->field];
        char text[M_FIELD_SIZE];
        if (field->kind == COLUMN_I32) {
            snprintf(text, sizeof(text), "%" PRId32, (int32_t)start->value);
        } else {
            snprintf(text, sizeof(text), "%" PRIu32, start->value);
        }
        Ini_Set(ini, M_START, field->name, text);
    }
    Ini_Write(ini, out);
    Ini_Free(ini);
}

RESULT Case_ReadEntry(
    ARCHIVE *const archive, const char *const name, ZIP_ENTRY *const out)
{
    const ARCHIVE_ENTRY *const entry = Archive_Find(archive, name);
    FAIL_IF(entry == nullptr, "the case has no %s", name);
    uint8_t *data;
    MUST(Archive_Read(archive, entry, &data));
    *out = (ZIP_ENTRY) {
        .name = name,
        .data = data,
        .size = ArchiveEntry_GetSize(entry),
    };
    return OK;
}
