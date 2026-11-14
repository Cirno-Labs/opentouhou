#pragma once

// Writes th07 data directories for tests: th07.dat, built in memory as a
// PBG4 archive, and loose files.

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const char *content;
} TH07_DATA_FILE;

// Writes th07.dat in directory, holding files.
void Th07Data_WriteArchive(
    const char *directory, const TH07_DATA_FILE *files, int32_t count);

// Writes size bytes of data as the loose file name in directory.
void Th07Data_WriteFile(
    const char *directory, const char *name, const void *data, size_t size);
