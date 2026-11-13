// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/update.h"

#include "othpatch/fetch.h"

#include "core/base/crc32.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/reader.h"
#include "core/io/writer.h"
#include "core/json/get.h"
#include "core/json/parse.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sets how many files are fetched into memory before they are written.
#define M_FETCH_BATCH 32

// Sets the largest files.js that an update accepts, and the size of the
// arena of one update, which holds files.js, its parsed tree and the paths
// of its files.
#define M_MAX_FILE_LIST_SIZE ((size_t)8 * 1024 * 1024)
#define M_ARENA_SIZE ((size_t)64 * 1024 * 1024)

typedef enum {
    M_ENTRY_KEEP,
    M_ENTRY_FETCH,
    M_ENTRY_ARRIVED,
    M_ENTRY_DELETE,
} M_ENTRY_STATE;

typedef struct {
    const char *path;
    char *file;
    char *part;
    uint32_t crc;
    M_ENTRY_STATE state;
} M_ENTRY;

typedef ARRAY(M_ENTRY) M_ENTRY_LIST;

static char *M_Join(
    ARENA *const arena, const char *const a, const char *const b)
{
    return Path_Join(Arena_Allocator(arena), a, b);
}

PRINTF_FORMAT(2, 3)
static char *M_Format(ARENA *const arena, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const heap = Str_FormatV(fmt, args);
    va_end(args);
    const size_t size = strlen(heap) + 1;
    char *const copy = Arena_Alloc(arena, size);
    memcpy(copy, heap, size);
    Mem_Free(heap);
    return copy;
}

static bool M_IsSafePath(const char *const path)
{
    if (path[0] == '\0' || strchr(path, '\\') != nullptr
        || strchr(path, ':') != nullptr) {
        return false;
    }
    const char *part = path;
    for (;;) {
        const size_t length = strcspn(part, "/");
        if (length == 0 || (length == 1 && part[0] == '.')
            || (length == 2 && part[0] == '.' && part[1] == '.')) {
            return false;
        }
        if (part[length] == '\0') {
            return true;
        }
        part += length + 1;
    }
}

static bool M_IsWanted(
    const char *const path, const char *const *const games,
    const int32_t game_count)
{
    const char *const slash = strchr(path, '/');
    if (slash == nullptr) {
        return true;
    }
    for (int32_t i = 0; i < game_count; i++) {
        const size_t length = strlen(games[i]);
        if ((size_t)(slash - path) == length
            && strncmp(path, games[i], length) == 0) {
            return true;
        }
    }
    return false;
}

// Returns the URL of path below server, with every byte other than an
// unreserved character or '/' percent-escaped.
static char *M_FileUrl(
    ARENA *const arena, const char *const server, const char *const path,
    const uint32_t crc)
{
    static const char hex[] = "0123456789ABCDEF";
    const size_t server_length = strlen(server);
    char *const url = Arena_Alloc(arena, server_length + strlen(path) * 3 + 20);
    memcpy(url, server, server_length);
    char *out = url + server_length;
    for (const unsigned char *c = (const unsigned char *)path; *c != '\0';
         c++) {
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z')
            || (*c >= '0' && *c <= '9') || strchr("-._~/", *c) != nullptr) {
            *out++ = (char)*c;
        } else {
            *out++ = '%';
            *out++ = hex[*c >> 4];
            *out++ = hex[*c & 15];
        }
    }
    snprintf(out, 20, "?crc32=%08x", crc);
    return url;
}

static RESULT M_FileCrc(const char *const path, uint32_t *const crc)
{
    *crc = 0;
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    uint8_t buffer[65536];
    uint32_t value = 0;
    while (Reader_Remaining(&reader) > 0) {
        const size_t size = MIN(Reader_Remaining(&reader), sizeof(buffer));
        Reader_ReadBytes(&reader, buffer, size);
        if (Reader_Failed(&reader)) {
            break;
        }
        value = Crc32_Update(value, buffer, size);
    }
    Reader_Close(&reader);
    MUST(Reader_Result(&reader));
    *crc = value;
    return OK;
}

static RESULT M_WriteFile(
    const char *const path, const void *const data, const size_t size)
{
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    const uint8_t *const bytes = data;
    for (size_t i = 0; i < size; i++) {
        Writer_WriteU8(&writer, bytes[i]);
    }
    return Writer_Close(&writer);
}

static RESULT M_MakeParent(ARENA *const arena, const char *const path)
{
    char *const parent = M_Format(arena, "%s", path);
    char *const slash = strrchr(parent, '/');
    if (slash == nullptr) {
        return OK;
    }
    *slash = '\0';
    return File_MakeDir(parent);
}

// Reads files.js into entries, with each file to keep, to fetch or to
// delete.
static RESULT M_ReadEntries(
    ARENA *const arena, const char *const patch_dir,
    const JSON_VALUE *const files, const char *const *const games,
    const int32_t game_count, M_ENTRY_LIST *const entries)
{
    FAIL_IF(files->type != JSON_OBJECT, "files.js is not an object");
    for (int32_t i = 0; i < files->object.count; i++) {
        const JSON_MEMBER *const member = &files->object.members[i];
        const JSON_VALUE *const value = member->value;
        if (!M_IsSafePath(member->key)
            || !M_IsWanted(member->key, games, game_count)) {
            continue;
        }
        const bool deleted = value->type == JSON_NULL;
        const int64_t crc = Json_GetInt(value, -1);
        if (!deleted && (crc < 0 || crc > UINT32_MAX)) {
            continue;
        }
        M_ENTRY entry = {
            .path = member->key,
            .file = M_Join(arena, patch_dir, member->key),
            .crc = (uint32_t)crc,
            .state = deleted ? M_ENTRY_DELETE : M_ENTRY_FETCH,
        };
        entry.part = M_Format(arena, "%s.part", entry.file);
        MUST(File_Remove(entry.part));
        uint32_t local;
        if (!deleted && File_Exists(entry.file)
            && Result_Ignore(M_FileCrc(entry.file, &local))
            && local == entry.crc) {
            entry.state = M_ENTRY_KEEP;
        }
        Array_Append(entries, entry);
    }
    return OK;
}

// Takes one batch of fetched files: writes each that has the right CRC-32
// to its .part file. Keeps the first failure in *first.
static void M_TakeBatch(
    ARENA *const arena, FETCH *const fetches, M_ENTRY **const batch,
    const int32_t count, RESULT *const first)
{
    for (int32_t i = 0; i < count; i++) {
        M_ENTRY *const entry = batch[i];
        FETCH *const fetch = &fetches[i];
        RESULT result = fetch->result;
        fetch->result = OK;
        if (IS_OK(result)) {
            const uint32_t crc = Crc32_Compute(fetch->data, fetch->size);
            if (crc != entry->crc) {
                result = FAIL(
                    "%s has the CRC-32 %08x instead of %08x", fetch->url, crc,
                    entry->crc);
            }
        }
        if (IS_OK(result)) {
            result = M_MakeParent(arena, entry->part);
        }
        if (IS_OK(result)) {
            result = M_WriteFile(entry->part, fetch->data, fetch->size);
        }
        Fetch_Release(fetch);
        if (IS_OK(result)) {
            entry->state = M_ENTRY_ARRIVED;
        } else if (IS_OK(*first)) {
            *first = result;
        } else {
            Result_Ignore(result);
        }
    }
}

// Fetches every entry still to fetch, from each server in order.
static RESULT M_FetchEntries(
    ARENA *const arena, const REPO_PATCH *const patch,
    M_ENTRY_LIST *const entries)
{
    RESULT first = OK;
    for (int32_t server = 0; server < patch->server_count; server++) {
        int32_t index = 0;
        for (;;) {
            M_ENTRY *batch[M_FETCH_BATCH];
            FETCH fetches[M_FETCH_BATCH] = {};
            int32_t count = 0;
            for (; index < entries->count && count < M_FETCH_BATCH; index++) {
                M_ENTRY *const entry = &entries->items[index];
                if (entry->state == M_ENTRY_FETCH) {
                    fetches[count].url = M_FileUrl(
                        arena, patch->servers[server], entry->path, entry->crc);
                    batch[count++] = entry;
                }
            }
            if (count == 0) {
                break;
            }
            Fetch_Run(fetches, count);
            M_TakeBatch(arena, fetches, batch, count, &first);
        }
    }
    int32_t missing = 0;
    for (int32_t i = 0; i < entries->count; i++) {
        missing += entries->items[i].state == M_ENTRY_FETCH;
    }
    if (missing == 0) {
        Result_Ignore(first);
        return OK;
    }
    for (int32_t i = 0; i < entries->count; i++) {
        if (entries->items[i].state == M_ENTRY_ARRIVED) {
            Result_Ignore(File_Remove(entries->items[i].part));
        }
    }
    return Result_Prefix(first, "%d files did not arrive", missing);
}

// Fetches files.js from the first server of the patch that gives it.
static RESULT M_FetchFileList(
    ARENA *const arena, const REPO_PATCH *const patch, FETCH *const out)
{
    RESULT last = FAIL("%s has no server", patch->id);
    for (int32_t i = 0; i < patch->server_count; i++) {
        Result_Ignore(last);
        *out = (FETCH) {
            .url = M_Format(arena, "%sfiles.js", patch->servers[i]),
        };
        Fetch_Run(out, 1);
        last = out->result;
        out->result = OK;
        if (IS_OK(last)) {
            return OK;
        }
        Fetch_Release(out);
    }
    return last;
}

static RESULT M_Update(
    ARENA *const arena, const char *const dir, const REPO_PATCH *const patch,
    const char *const *const games, const int32_t game_count,
    UPDATE_COUNTS *const counts)
{
    const char *const repo_dir = M_Join(arena, dir, patch->repo->id);
    const char *const patch_dir = M_Join(arena, repo_dir, patch->name);
    MUST(File_MakeDir(patch_dir));

    FETCH list;
    MUST(M_FetchFileList(arena, patch, &list));
    if (list.size > M_MAX_FILE_LIST_SIZE) {
        Fetch_Release(&list);
        return FAIL("files.js is larger than %zu bytes", M_MAX_FILE_LIST_SIZE);
    }
    uint8_t *const list_text = Arena_Alloc(arena, list.size);
    if (list.size > 0) {
        memcpy(list_text, list.data, list.size);
    }
    const size_t list_size = list.size;
    Fetch_Release(&list);
    JSON_VALUE *files;
    MUST(
        Json_Parse(
            (const char *)list_text, list_size, Arena_Allocator(arena), &files),
        "files.js");

    M_ENTRY_LIST entries;
    Array_Init(&entries, Arena_Allocator(arena));
    MUST(M_ReadEntries(arena, patch_dir, files, games, game_count, &entries));
    MUST(M_FetchEntries(arena, patch, &entries));

    for (int32_t i = 0; i < entries.count; i++) {
        const M_ENTRY *const entry = &entries.items[i];
        if (entry->state == M_ENTRY_ARRIVED) {
            MUST(File_Rename(entry->part, entry->file));
            counts->downloaded++;
        } else if (entry->state == M_ENTRY_KEEP) {
            counts->up_to_date++;
        } else if (entry->state == M_ENTRY_DELETE && File_Exists(entry->file)) {
            MUST(File_Remove(entry->file));
            counts->removed++;
        }
    }
    MUST(M_WriteFile(
        M_Join(arena, repo_dir, "repo.js"), patch->repo->text,
        patch->repo->size));
    MUST(M_WriteFile(
        M_Join(arena, patch_dir, "files.js"), list_text, list_size));
    MUST(M_WriteFile(
        M_Join(arena, patch_dir, "patch.js"), patch->text, patch->size));
    return OK;
}

RESULT Update_Patch(
    const char *const dir, const REPO_PATCH *const patch,
    const char *const *const games, const int32_t game_count,
    UPDATE_COUNTS *const counts)
{
    *counts = (UPDATE_COUNTS) {};
    ARENA arena;
    Arena_Init(&arena, "update", M_ARENA_SIZE);
    const RESULT result =
        M_Update(&arena, dir, patch, games, game_count, counts);
    Arena_Free(&arena);
    return result;
}

static RESULT M_ListPatches(
    ARENA *const arena, const char *const dir, const char *const repo,
    UPDATE_INSTALLED_LIST *const out)
{
    const char *const repo_dir = M_Join(arena, dir, repo);
    FILE_LIST names;
    if (!Result_Ignore(
            File_ListDir(repo_dir, Arena_Allocator(arena), &names))) {
        return OK;
    }
    for (int32_t i = 0; i < names.count; i++) {
        const char *const patch_js =
            M_Join(arena, M_Join(arena, repo_dir, names.items[i]), "patch.js");
        READER reader;
        if (!Result_Ignore(Reader_OpenFile(&reader, patch_js))) {
            continue;
        }
        const size_t size = Reader_Size(&reader);
        char *const text = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
        Reader_Close(&reader);
        bool update = true;
        JSON_VALUE *root;
        if (Result_Ignore(Reader_Result(&reader))
            && Result_Ignore(
                Json_Parse(text, size, Arena_Allocator(arena), &root))) {
            update = Json_GetBool(Json_Get(root, "update"), true);
        }
        Mem_Free(text);
        Array_Append(
            out,
            ((UPDATE_INSTALLED) {
                .id = M_Format(arena, "%s/%s", repo, names.items[i]),
                .update = update,
            }));
    }
    return OK;
}

RESULT Update_ListInstalled(
    const char *const dir, ARENA *const arena, UPDATE_INSTALLED_LIST *const out)
{
    Array_Init(out, Arena_Allocator(arena));
    FILE_LIST repos;
    if (!File_Exists(dir)) {
        return OK;
    }
    MUST(File_ListDir(dir, Arena_Allocator(arena), &repos));
    for (int32_t i = 0; i < repos.count; i++) {
        MUST(M_ListPatches(arena, dir, repos.items[i], out));
    }
    return OK;
}

static int M_CompareLength(const void *const a, const void *const b)
{
    const size_t length_a = strlen(*(char *const *)a);
    const size_t length_b = strlen(*(char *const *)b);
    return length_a < length_b ? 1 : length_a > length_b ? -1 : 0;
}

static RESULT M_Remove(
    ARENA *const arena, const char *const dir, const char *const id)
{
    const char *const patch_dir = M_Join(arena, dir, id);
    typedef ARRAY(char *) M_FOLDERS;
    M_FOLDERS folders;
    Array_Init(&folders, Arena_Allocator(arena));
    const char *const files_js = M_Join(arena, patch_dir, "files.js");
    READER reader;
    if (Result_Ignore(Reader_OpenFile(&reader, files_js))) {
        const size_t size = Reader_Size(&reader);
        char *const text =
            Reader_ReadAlloc(&reader, size, Arena_Allocator(arena));
        Reader_Close(&reader);
        JSON_VALUE *files = nullptr;
        if (Result_Ignore(Reader_Result(&reader))
            && Result_Ignore(
                Json_Parse(text, size, Arena_Allocator(arena), &files))
            && files->type == JSON_OBJECT) {
            for (int32_t i = 0; i < files->object.count; i++) {
                const char *const path = files->object.members[i].key;
                if (!M_IsSafePath(path)) {
                    continue;
                }
                char *const file = M_Join(arena, patch_dir, path);
                MUST(File_Remove(file));
                MUST(File_Remove(M_Format(arena, "%s.part", file)));
                const size_t root = strlen(file) - strlen(path);
                for (char *slash = strrchr(file, '/');
                     slash != nullptr && (size_t)(slash - file) > root;
                     slash = strrchr(file, '/')) {
                    *slash = '\0';
                    Array_Append(&folders, M_Format(arena, "%s", file));
                }
            }
        }
    }
    MUST(File_Remove(files_js));
    MUST(File_Remove(M_Join(arena, patch_dir, "patch.js")));
    if (folders.count > 1) {
        qsort(
            folders.items, (size_t)folders.count, sizeof(folders.items[0]),
            M_CompareLength);
    }
    for (int32_t i = 0; i < folders.count; i++) {
        Result_Ignore(File_Remove(folders.items[i]));
    }
    Result_Ignore(File_Remove(patch_dir));
    return OK;
}

RESULT Update_RemovePatch(const char *const dir, const char *const id)
{
    ARENA arena;
    Arena_Init(&arena, "remove", M_ARENA_SIZE);
    const RESULT result = M_Remove(&arena, dir, id);
    Arena_Free(&arena);
    return result;
}
