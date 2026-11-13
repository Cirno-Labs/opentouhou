#include "th06_data.h"

#include "pbg3_builder.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_MAX_FILES 4

const char *const g_Th06Archives[TH06_ARCHIVE_COUNT] = {
    "紅魔郷IN.DAT", "紅魔郷MD.DAT", "紅魔郷ST.DAT",
    "紅魔郷TL.DAT", "紅魔郷CM.DAT", "紅魔郷ED.DAT",
};

void Th06Data_WriteArchive(
    const char *const directory, const char *const name,
    const TH06_DATA_FILE *const files, const int32_t count)
{
    if (count > M_MAX_FILES) {
        LOG_FATAL("%s: more than %d files", name, M_MAX_FILES);
    }
    static PBG3_BUILDER_WRITER writers[M_MAX_FILES];
    PBG3_BUILDER_ENTRY entries[M_MAX_FILES];
    for (int32_t i = 0; i < count; i++) {
        PBG3_BUILDER_WRITER *const bw = &writers[i];
        Pbg3Builder_Init(bw);
        for (const char *c = files[i].content; *c != '\0'; c++) {
            Pbg3Builder_WriteLiteral(bw, (uint8_t)*c);
        }
        Pbg3Builder_FlushByte(bw);
        entries[i] = (PBG3_BUILDER_ENTRY) {
            .name = files[i].name,
            .compressed = bw->bytes,
            .compressed_size = bw->length,
            .checksum = Pbg3Builder_Checksum(bw->bytes, bw->length),
            .decoded_size = (uint32_t)strlen(files[i].content),
        };
    }
    static uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(entries, count, archive);
    Th06Data_WriteFile(directory, name, archive, size);
}

void Th06Data_WriteArchives(const char *const directory, const int32_t skip)
{
    const TH06_DATA_FILE version = { "ver0102.dat", "1.02h" };
    for (int32_t i = 0; i < TH06_ARCHIVE_COUNT; i++) {
        if (i != skip) {
            Th06Data_WriteArchive(directory, g_Th06Archives[i], &version, 1);
        }
    }
}

void Th06Data_WriteFile(
    const char *const directory, const char *const name, const void *const data,
    const size_t size)
{
    char *const path = Str_Format("%s/%s", directory, name);
    if (!SDL_SaveFile(path, data, size)) {
        LOG_FATAL("cannot create %s: %s", path, SDL_GetError());
    }
    Mem_Free(path);
}
