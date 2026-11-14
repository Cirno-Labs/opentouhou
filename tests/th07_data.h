#pragma once

// Writes th07 data directories for tests: th07.dat, built in memory as a
// PBG4 archive, thbgm.dat and loose files.

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const char *content;
} TH07_DATA_FILE;

// Writes th07.dat in directory, holding files.
void Th07Data_WriteArchive(
    const char *directory, const TH07_DATA_FILE *files, int32_t count);

// Writes thbgm.dat in directory: the header of 1.00 and then size bytes
// of pcm.
void Th07Data_WriteMusic(const char *directory, const void *pcm, size_t size);

// Writes size bytes of data as the loose file name in directory.
void Th07Data_WriteFile(
    const char *directory, const char *name, const void *data, size_t size);
