// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Builds programs that print a given output and exit with a given status,
// for the othpatch tests that run a game or othpatch. A program is a copy
// of the executable tests/fake_program.c, which meson names in
// FAKE_PROGRAM, with the status and the output after its end, as
// fake_program_marker.h describes. The copy runs on every platform, so the
// tests need no shell.

#include "core/base/memory.h"
#include "core/base/string.h"

#include "fake_program_marker.h"
#include "patch_builder.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <sys/stat.h>
#endif

// Holds the bytes of a program. Mem_Free releases data.
typedef struct {
    uint8_t *data;
    size_t size;
} FAKE_PROGRAM;

// Returns a program that prints output and exits with status.
[[maybe_unused]] static FAKE_PROGRAM FakeProgram_Build(
    const char *const output, const int status)
{
    const char *const path = getenv("FAKE_PROGRAM");
    size_t size = 0;
    void *const program = path != nullptr ? SDL_LoadFile(path, &size) : nullptr;
    if (program == nullptr) {
        fprintf(stderr, "FAKE_PROGRAM must name tests/fake_program\n");
        abort();
    }
    char *const trailer =
        Str_Format(FAKE_PROGRAM_MARKER "%d\n%s", status, output);
    const size_t trailer_size = strlen(trailer);
    FAKE_PROGRAM built = {
        .data = Mem_Alloc(size + trailer_size),
        .size = size + trailer_size,
    };
    memcpy(built.data, program, size);
    memcpy(built.data + size, trailer, trailer_size);
    Mem_Free(trailer);
    SDL_free(program);
    return built;
}

// Writes a program that prints output and exits with status to
// root/path, and creates the directories above it.
[[maybe_unused]] static void FakeProgram_Write(
    const char *const root, const char *const path, const char *const output,
    const int status)
{
    PatchBuilder_Write(root, path, "");
    char *const full = Str_Format("%s/%s", root, path);
    const FAKE_PROGRAM program = FakeProgram_Build(output, status);
    if (!SDL_SaveFile(full, program.data, program.size)) {
        TEST_FAIL("cannot create %s: %s", full, SDL_GetError());
    }
#ifndef _WIN32
    CHECK(chmod(full, 0755) == 0);
#endif
    Mem_Free(program.data);
    Mem_Free(full);
}

// Returns whether the file root/path holds the program that prints output
// and exits with status.
[[maybe_unused]] static bool FakeProgram_Equals(
    const char *const root, const char *const path, const char *const output,
    const int status)
{
    char *const full = Str_Format("%s/%s", root, path);
    size_t size = 0;
    void *const data = SDL_LoadFile(full, &size);
    Mem_Free(full);
    const FAKE_PROGRAM program = FakeProgram_Build(output, status);
    const bool equal = data != nullptr && size == program.size
        && memcmp(data, program.data, size) == 0;
    Mem_Free(program.data);
    SDL_free(data);
    return equal;
}
