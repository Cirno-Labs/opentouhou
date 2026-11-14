#include "th07_data.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/writer.h"
#include "stg/lzss.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_HEADER_SIZE 16

void Th07Data_WriteArchive(
    const char *const directory, const TH07_DATA_FILE *const files,
    const int32_t count)
{
    WRITER table;
    Writer_OpenBuffer(&table, &g_HeapAllocator, nullptr);
    WRITER out;
    Writer_OpenBuffer(&out, &g_HeapAllocator, nullptr);
    static const uint8_t header[M_HEADER_SIZE] = { 0 };
    Writer_WriteBytes(&out, header, sizeof(header));
    for (int32_t i = 0; i < count; i++) {
        const size_t size = strlen(files[i].content);
        Writer_WriteBytes(&table, files[i].name, strlen(files[i].name) + 1);
        Writer_WriteU32(&table, (uint32_t)Writer_Tell(&out));
        Writer_WriteU32(&table, (uint32_t)size);
        Writer_WriteU32(&table, 0);
        Lzss_Encode((const uint8_t *)files[i].content, size, &out);
    }
    size_t table_size;
    uint8_t *const table_bytes = Writer_TakeBuffer(&table, &table_size);
    Result_Ignore(Writer_Close(&table));

    const size_t table_offset = Writer_Tell(&out);
    Lzss_Encode(table_bytes, table_size, &out);
    Allocator_Free(&g_HeapAllocator, table_bytes, table_size);
    Writer_Seek(&out, 0);
    Writer_WriteBytes(&out, "PBG4", 4);
    Writer_WriteU32(&out, (uint32_t)count);
    Writer_WriteU32(&out, (uint32_t)table_offset);
    Writer_WriteU32(&out, (uint32_t)table_size);

    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&out, &size);
    Result_Ignore(Writer_Close(&out));
    Th07Data_WriteFile(directory, "th07.dat", bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
}

void Th07Data_WriteFile(
    const char *const directory, const char *const name, const void *const data,
    const size_t size)
{
    char *const path = Str_Format("%s/%s", directory, name);
    if (!SDL_SaveFile(path, data, size)) {
        LOG_FATAL("cannot create %s: %s", path, SDL_GetError());
    }
    Mem_Free(path);
}
