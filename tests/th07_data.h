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

// Holds one shot of an SHT file as the file stores it.
typedef struct {
    int16_t interval, phase;
    float offset_x, offset_y, size_x, size_y, angle, speed;
    int16_t damage;
    uint8_t option, kind;
    int16_t script, sound;
    uint32_t fire, move, draw, hit;
} TH07_SHT_SHOT;

typedef struct {
    int32_t max_power;
    int32_t count;
    const TH07_SHT_SHOT *shots;
} TH07_SHT_TABLE;

// Holds an SHT file: its header floats from +0xc in file order, then its
// tables.
typedef struct {
    float bombs;
    int32_t bomb_window;
    float floats[10];
    int32_t table_count;
    const TH07_SHT_TABLE *tables;
} TH07_SHT;

// Returns sht as SHT bytes, which Mem_Free releases, and sets *size. Each
// table's shots follow the table entries, closed by a shot of interval -1.
uint8_t *Th07Data_BuildSht(const TH07_SHT *sht, size_t *size);
