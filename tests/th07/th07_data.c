#include "tests/th07/th07_data.h"

#include "th07/files.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/writer.h"
#include "shell/null.h"
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

void Th07Data_WriteMusic(
    const char *const directory, const void *const pcm, const size_t size)
{
    WRITER out;
    Writer_OpenBuffer(&out, &g_HeapAllocator, nullptr);
    Writer_WriteBytes(&out, "ZWAV", 4);
    Writer_WriteU32(&out, 1);
    Writer_WriteU32(&out, 0x700);
    Writer_WriteU32(&out, 0);
    Writer_WriteBytes(&out, pcm, size);
    size_t total;
    uint8_t *const bytes = Writer_TakeBuffer(&out, &total);
    Result_Ignore(Writer_Close(&out));
    Th07Data_WriteFile(directory, "thbgm.dat", bytes, total);
    Allocator_Free(&g_HeapAllocator, bytes, total);
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

uint8_t *Th07Data_BuildSht(const TH07_SHT *const sht, size_t *const size)
{
    WRITER out;
    Writer_OpenBuffer(&out, &g_HeapAllocator, nullptr);
    Writer_WriteU16(&out, 0);
    Writer_WriteU16(&out, (uint16_t)sht->table_count);
    Writer_WriteF32(&out, sht->bombs);
    Writer_WriteI32(&out, sht->bomb_window);
    for (int32_t i = 0; i < 10; i++) {
        Writer_WriteF32(&out, sht->floats[i]);
    }
    size_t offset = Writer_Tell(&out) + (size_t)sht->table_count * 8;
    for (int32_t i = 0; i < sht->table_count; i++) {
        Writer_WriteU32(&out, (uint32_t)offset);
        Writer_WriteI32(&out, sht->tables[i].max_power);
        offset += ((size_t)sht->tables[i].count + 1) * 0x34;
    }
    for (int32_t i = 0; i < sht->table_count; i++) {
        const TH07_SHT_TABLE *const table = &sht->tables[i];
        for (int32_t j = 0; j <= table->count; j++) {
            const TH07_SHT_SHOT shot = j < table->count
                ? table->shots[j]
                : (TH07_SHT_SHOT) { .interval = -1 };
            Writer_WriteI16(&out, shot.interval);
            Writer_WriteI16(&out, shot.phase);
            Writer_WriteF32(&out, shot.offset_x);
            Writer_WriteF32(&out, shot.offset_y);
            Writer_WriteF32(&out, shot.size_x);
            Writer_WriteF32(&out, shot.size_y);
            Writer_WriteF32(&out, shot.angle);
            Writer_WriteF32(&out, shot.speed);
            Writer_WriteI16(&out, shot.damage);
            Writer_WriteU8(&out, shot.option);
            Writer_WriteU8(&out, shot.kind);
            Writer_WriteI16(&out, shot.script);
            Writer_WriteI16(&out, shot.sound);
            Writer_WriteU32(&out, shot.fire);
            Writer_WriteU32(&out, shot.move);
            Writer_WriteU32(&out, shot.draw);
            Writer_WriteU32(&out, shot.hit);
        }
    }
    uint8_t *const bytes = Writer_TakeBuffer(&out, size);
    Result_Ignore(Writer_Close(&out));
    return bytes;
}

RESULT Th07Data_Mount(const char *const directory)
{
    ASSERT(Result_Ignore(NullShell_OpenFiles(directory, directory)));
    return Files_Mount();
}

void Th07Data_Unmount(void)
{
    Files_SetPatches(nullptr);
    NullShell_CloseFiles();
}
