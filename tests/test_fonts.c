#include "tests/test_fonts.h"

#include "tests/test.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/reader.h"
#include "core/text/font_registry.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *TestFonts_ReadFile(const char *const path, size_t *const size)
{
    READER reader;
    if (!Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR)) {
        abort();
    }
    *size = Reader_Size(&reader);
    uint8_t *const data = Reader_ReadAlloc(&reader, *size, &g_HeapAllocator);
    Reader_Close(&reader);
    if (!Result_Log(Reader_Result(&reader), LOG_LEVEL_ERROR)) {
        abort();
    }
    return data;
}

uint8_t *TestFonts_Read(const char *const name, size_t *const size)
{
    const char *const dir = SDL_getenv("TEST_FONT_DIR");
    if (dir == nullptr) {
        fprintf(stderr, "TEST_FONT_DIR is not set\n");
        abort();
    }
    char *const path = Str_Format("%s/%s", dir, name);
    uint8_t *const data = TestFonts_ReadFile(path, size);
    Mem_Free(path);
    return data;
}

void TestFonts_Add(const char *const name)
{
    size_t size;
    uint8_t *const data = TestFonts_Read(name, &size);
    CHECK(IS_OK(FontRegistry_Add(name, data, size)));
    Mem_Free(data);
}

void TestFonts_UseBuiltin(void)
{
    FontRegistry_Reset(false);
    TestFonts_Add(TEST_FONT_GOTHIC);
    TestFonts_Add(TEST_FONT_UI_GOTHIC);
}

const char *TestFonts_RequireMsGothic(void)
{
    const char *const path = SDL_getenv("OPENTOUHOU_MSGOTHIC");
    if (path == nullptr || path[0] == '\0') {
        TEST_SKIP("$OPENTOUHOU_MSGOTHIC names no msgothic.ttc");
    }
    if (!SDL_GetPathInfo(path, nullptr)) {
        TEST_SKIP("$OPENTOUHOU_MSGOTHIC names %s, which does not exist", path);
    }
    return path;
}

uint32_t TestFonts_ReadU32Be(const uint8_t *const data)
{
    return (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16
        | (uint32_t)data[2] << 8 | data[3];
}

void TestFonts_WriteU32Be(uint8_t *const data, const uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

uint8_t *TestFonts_MakeCollection(
    const uint8_t *const a, const size_t a_size, const uint8_t *const b,
    const size_t b_size, size_t *const size)
{
    const size_t header = 20;
    const size_t a_base = header;
    const size_t b_base = (a_base + a_size + 3) & ~(size_t)3;
    *size = b_base + b_size;
    uint8_t *const data = Mem_AllocZ(*size);
    memcpy(data, "ttcf", 4);
    TestFonts_WriteU32Be(data + 4, 0x00010000);
    TestFonts_WriteU32Be(data + 8, 2);
    TestFonts_WriteU32Be(data + 12, (uint32_t)a_base);
    TestFonts_WriteU32Be(data + 16, (uint32_t)b_base);
    memcpy(data + a_base, a, a_size);
    memcpy(data + b_base, b, b_size);
    const size_t bases[] = { a_base, b_base };
    for (int32_t font = 0; font < 2; font++) {
        uint8_t *const directory = data + bases[font];
        const int32_t count = directory[4] << 8 | directory[5];
        for (int32_t i = 0; i < count; i++) {
            uint8_t *const offset = directory + 12 + i * 16 + 8;
            TestFonts_WriteU32Be(
                offset, TestFonts_ReadU32Be(offset) + (uint32_t)bases[font]);
        }
    }
    return data;
}
