// SPDX-License-Identifier: GPL-3.0-only

// Stands in for a game or othpatch in the othpatch tests. fake_program.h
// appends an output and an exit status to a copy of this executable. The
// copy prints that output and exits with that status.

#include "fake_program_marker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <fcntl.h>
    #include <io.h>
#endif

static char *M_ReadFile(const char *const path, size_t *const size)
{
    FILE *const file = fopen(path, "rb");
    if (file == nullptr) {
        return nullptr;
    }
    size_t capacity = 1 << 16;
    char *data = malloc(capacity);
    *size = 0;
    size_t count;
    while ((count = fread(data + *size, 1, capacity - *size, file)) > 0) {
        *size += count;
        if (*size == capacity) {
            capacity *= 2;
            data = realloc(data, capacity);
        }
    }
    fclose(file);
    return data;
}

int main(const int argc, char **const argv)
{
    if (argc < 1) {
        return EXIT_FAILURE;
    }
    size_t size;
    char *const data = M_ReadFile(argv[0], &size);
    if (data == nullptr) {
        fprintf(stderr, "cannot read %s\n", argv[0]);
        return EXIT_FAILURE;
    }
    // Takes the last marker, because the marker is also in the text of
    // this executable.
    const size_t marker_size = strlen(FAKE_PROGRAM_MARKER);
    const char *trailer = nullptr;
    for (size_t i = size; i >= marker_size && trailer == nullptr; i--) {
        if (memcmp(data + i - marker_size, FAKE_PROGRAM_MARKER, marker_size)
            == 0) {
            trailer = data + i;
        }
    }
    if (trailer == nullptr) {
        fprintf(stderr, "%s has no output\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *output;
    const int status = (int)strtol(trailer, &output, 10);
    output++;
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    fwrite(output, 1, (size_t)(data + size - output), stdout);
    return status;
}
