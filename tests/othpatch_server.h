// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Builds repository networks of file:// URLs for the othpatch tests, in the
// test directories of test_dir.h.

#include "core/base/crc32.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include "patch_builder.h"

#include <SDL3/SDL.h>
#include <string.h>

// Returns the URL of the repository folder <root>/<repo>/. Mem_Free
// releases it.
[[maybe_unused]] static char *Server_RepoUrl(
    const char *const root, const char *const repo)
{
    char *const root_url = TestDir_FileUrl(root);
    char *const url = Str_Format("%s/%s/", root_url, repo);
    Mem_Free(root_url);
    return url;
}

// Writes <root>/<repo>/repo.js with the given neighbors, a JSON array of
// repository folder names below root, and patches, the members of a JSON
// object.
[[maybe_unused]] static void Server_AddRepo(
    const char *const root, const char *const repo,
    const char *const *const neighbors, const int neighbor_count,
    const char *const patches)
{
    char *list = Str_Dup("");
    for (int i = 0; i < neighbor_count; i++) {
        char *const url = Server_RepoUrl(root, neighbors[i]);
        char *const longer =
            Str_Format("%s%s\"%s\"", list, i > 0 ? ", " : "", url);
        Mem_Free(url);
        Mem_Free(list);
        list = longer;
    }
    char *const url = Server_RepoUrl(root, repo);
    char *const text = Str_Format(
        "{\"id\": \"%s\", \"title\": \"Repository %s\", \"servers\": "
        "[\"%s\"], \"neighbors\": [%s], \"patches\": {%s}}",
        repo, repo, url, list, patches);
    char *const path = Str_Format("%s/repo.js", repo);
    PatchBuilder_Write(root, path, text);
    Mem_Free(path);
    Mem_Free(text);
    Mem_Free(url);
    Mem_Free(list);
}

// Writes <root>/<repo>/<patch>/patch.js with the given members after its
// id and title, such as "\"dependencies\": [\"base\"]", or none.
[[maybe_unused]] static void Server_AddPatch(
    const char *const root, const char *const repo, const char *const patch,
    const char *const members)
{
    char *const text = Str_Format(
        "{\"id\": \"%s\", \"title\": \"Patch %s\"%s%s}", patch, patch,
        members[0] != '\0' ? ", " : "", members);
    char *const path = Str_Format("%s/%s/patch.js", repo, patch);
    PatchBuilder_Write(root, path, text);
    Mem_Free(path);
    Mem_Free(text);
}

// Writes <root>/<repo>/<patch>/<file> with content.
[[maybe_unused]] static void Server_AddFile(
    const char *const root, const char *const repo, const char *const patch,
    const char *const file, const char *const content)
{
    char *const path = Str_Format("%s/%s/%s", repo, patch, file);
    PatchBuilder_Write(root, path, content);
    Mem_Free(path);
}

// Returns the files.js member for a file with content: "\"<file>\": <crc>".
// Mem_Free releases it.
[[maybe_unused]] static char *Server_Entry(
    const char *const file, const char *const content)
{
    return Str_Format(
        "\"%s\": %u", file, (unsigned)Crc32_Compute(content, strlen(content)));
}

// Returns the contents of a file, or nullptr when it cannot be read.
// SDL_free releases them.
[[maybe_unused]] static char *Server_Read(
    const char *const root, const char *const path)
{
    char *const full = Str_Format("%s/%s", root, path);
    char *const data = SDL_LoadFile(full, nullptr);
    Mem_Free(full);
    return data;
}

// Returns whether <root>/<path> exists.
[[maybe_unused]] static bool Server_Exists(
    const char *const root, const char *const path)
{
    char *const full = Str_Format("%s/%s", root, path);
    const bool exists = File_Exists(full);
    Mem_Free(full);
    return exists;
}
