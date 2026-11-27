#include "core/base/log.h"
#include "core/io/reader.h"
#include "core/io/writer.h"
#include "core/text/font_rom.h"

#include <stdio.h>
#include <string.h>

#define M_SIDE 2048
#define M_PITCH (M_SIDE / 8)
#define M_PIXELS (M_SIDE * M_PITCH)
#define M_HEADER 62

static RESULT M_Load(
    const char *const full_path, const char *const half_path,
    FONT_ROM **const out)
{
    READER full;
    READER half;
    MUST(Reader_OpenFile(&full, full_path));
    RESULT result = Reader_OpenFile(&half, half_path);
    if (IS_OK(result)) {
        result = FontRom_ReadBdf(&full, &half, out);
        Reader_Close(&half);
    }
    Reader_Close(&full);
    return result;
}

static RESULT M_Write(const FONT_ROM *const rom, const char *const path)
{
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    Writer_WriteBytes(&writer, "BM", 2);
    Writer_WriteU32(&writer, M_HEADER + M_PIXELS);
    Writer_WriteU16(&writer, 0);
    Writer_WriteU16(&writer, 0);
    Writer_WriteU32(&writer, M_HEADER);
    Writer_WriteU32(&writer, 40);
    Writer_WriteI32(&writer, M_SIDE);
    Writer_WriteI32(&writer, M_SIDE);
    Writer_WriteU16(&writer, 1);
    Writer_WriteU16(&writer, 1);
    Writer_WriteU32(&writer, 0);
    Writer_WriteU32(&writer, M_PIXELS);
    Writer_WriteI32(&writer, 0);
    Writer_WriteI32(&writer, 0);
    Writer_WriteU32(&writer, 2);
    Writer_WriteU32(&writer, 2);
    const uint8_t palette[] = { 0, 0, 0, 0, 255, 255, 255, 0 };
    Writer_WriteBytes(&writer, palette, sizeof(palette));
    for (int32_t y = M_SIDE - 1; y >= 0; y--) {
        uint8_t line[M_PITCH];
        memset(line, 255, sizeof(line));
        if (y < 16) {
            for (uint16_t code = 0; code < 256; code++) {
                const uint16_t *const cell = FontRom_Cell(rom, code, false);
                line[code] = (uint8_t)~(cell[y] >> 8);
            }
        } else if (y / 16 >= 0x21 && y / 16 <= 0x7e) {
            for (uint16_t first = 0x21; first <= 0x7e; first++) {
                const uint16_t code = (uint16_t)(first << 8 | (y / 16));
                const uint16_t *const cell = FontRom_Cell(rom, code, true);
                const uint16_t row = (uint16_t)~cell[y % 16];
                line[(first - 0x20) * 2] = (uint8_t)(row >> 8);
                line[(first - 0x20) * 2 + 1] = (uint8_t)row;
            }
        }
        Writer_WriteBytes(&writer, line, sizeof(line));
    }
    return Writer_Close(&writer);
}

static RESULT M_Compare(const FONT_ROM *const source, const FONT_ROM *const out)
{
    for (uint16_t code = 0; code < 256; code++) {
        FAIL_IF(
            memcmp(
                FontRom_Cell(source, code, false),
                FontRom_Cell(out, code, false), 16 * sizeof(uint16_t))
                != 0,
            "Half-width font cell %04x differs", code);
    }
    for (uint16_t first = 0x21; first <= 0x7e; first++) {
        for (uint16_t second = 0x21; second <= 0x7e; second++) {
            const uint16_t code = (uint16_t)(first << 8 | second);
            FAIL_IF(
                memcmp(
                    FontRom_Cell(source, code, true),
                    FontRom_Cell(out, code, true), 16 * sizeof(uint16_t))
                    != 0,
                "Full-width font cell %04x differs", code);
        }
    }
    return OK;
}

static RESULT M_Verify(const FONT_ROM *const source, const char *const path)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    FONT_ROM *out = nullptr;
    RESULT result = FontRom_ReadAnex86(&reader, &out);
    Reader_Close(&reader);
    if (IS_OK(result)) {
        result = M_Compare(source, out);
    }
    FontRom_Free(out);
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s FULL.bdf HALF.bdf OUT.bmp\n", argv[0]);
        return 2;
    }
    FONT_ROM *rom = nullptr;
    RESULT result = M_Load(argv[1], argv[2], &rom);
    if (IS_OK(result)) {
        result = M_Write(rom, argv[3]);
    }
    if (IS_OK(result)) {
        result = M_Verify(rom, argv[3]);
    }
    FontRom_Free(rom);
    return Result_Log(result, LOG_LEVEL_ERROR) ? 0 : 2;
}
