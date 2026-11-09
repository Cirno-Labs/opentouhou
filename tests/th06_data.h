#pragma once

// Writes th06 data directories for tests: the six archives Files_Init
// mounts, built in memory, and loose files.

#include <stddef.h>
#include <stdint.h>

#define TH06_ARCHIVE_COUNT 6

typedef struct {
    const char *name;
    const char *content;
} TH06_DATA_FILE;

// Names the archives, in the order Files_Init mounts them.
extern const char *const g_Th06Archives[TH06_ARCHIVE_COUNT];

// Writes the archive name in directory, holding files.
void Th06Data_WriteArchive(
    const char *directory, const char *name, const TH06_DATA_FILE *files,
    int32_t count);

// Writes every archive in directory with only ver0102.dat, except the one
// at index skip. A skip of -1 writes all six.
void Th06Data_WriteArchives(const char *directory, int32_t skip);

// Writes size bytes of data as the loose file name in directory.
void Th06Data_WriteFile(
    const char *directory, const char *name, const void *data, size_t size);
