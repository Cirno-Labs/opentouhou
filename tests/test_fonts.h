#pragma once

// Gives the tests their fonts. TEST_FONT_DIR names the folder of the
// built-in IPA fonts, and $OPENTOUHOU_MSGOTHIC, where it is set, the
// msgothic.ttc that the MS Gothic tests use.

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/reader.h"
#include "core/text/font_registry.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FONT_GOTHIC "ipag.ttf"
#define TEST_FONT_UI_GOTHIC "ipagp.ttf"

// Returns the contents of the file at path, and stops the test when it
// cannot be read. Mem_Free releases them.
[[maybe_unused]] static uint8_t *TestFonts_ReadFile(
    const char *const path, size_t *const size)
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

// Returns the contents of the built-in font file name.
[[maybe_unused]] static uint8_t *TestFonts_Read(
    const char *const name, size_t *const size)
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

// Adds the built-in font file name to the registry.
[[maybe_unused]] static void TestFonts_Add(const char *const name)
{
    size_t size;
    uint8_t *const data = TestFonts_Read(name, &size);
    CHECK(IS_OK(FontRegistry_Add(name, data, size)));
    Mem_Free(data);
}

// Leaves the registry with the two built-in fonts alone, as the native
// backend has it.
[[maybe_unused]] static void TestFonts_UseBuiltin(void)
{
    FontRegistry_Reset(false);
    TestFonts_Add(TEST_FONT_GOTHIC);
    TestFonts_Add(TEST_FONT_UI_GOTHIC);
}

// Returns the path of msgothic.ttc, or exits with meson's skip status when
// $OPENTOUHOU_MSGOTHIC names none.
[[maybe_unused]] static const char *TestFonts_RequireMsGothic(void)
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

[[maybe_unused]] static uint32_t M_TestFontsReadBe(const uint8_t *const data)
{
    return (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16
        | (uint32_t)data[2] << 8 | data[3];
}

[[maybe_unused]] static void M_TestFontsWriteBe(
    uint8_t *const data, const uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

// Builds a TrueType collection of the fonts a and b, in that order, with
// the tables of each moved behind the header. Mem_Free releases it.
[[maybe_unused]] static uint8_t *TestFonts_MakeCollection(
    const uint8_t *const a, const size_t a_size, const uint8_t *const b,
    const size_t b_size, size_t *const size)
{
    const size_t header = 20;
    const size_t a_base = header;
    const size_t b_base = (a_base + a_size + 3) & ~(size_t)3;
    *size = b_base + b_size;
    uint8_t *const data = Mem_AllocZ(*size);
    memcpy(data, "ttcf", 4);
    M_TestFontsWriteBe(data + 4, 0x00010000);
    M_TestFontsWriteBe(data + 8, 2);
    M_TestFontsWriteBe(data + 12, (uint32_t)a_base);
    M_TestFontsWriteBe(data + 16, (uint32_t)b_base);
    memcpy(data + a_base, a, a_size);
    memcpy(data + b_base, b, b_size);
    const size_t bases[] = { a_base, b_base };
    for (int32_t font = 0; font < 2; font++) {
        uint8_t *const directory = data + bases[font];
        const int32_t count = directory[4] << 8 | directory[5];
        for (int32_t i = 0; i < count; i++) {
            uint8_t *const offset = directory + 12 + i * 16 + 8;
            M_TestFontsWriteBe(
                offset, M_TestFontsReadBe(offset) + (uint32_t)bases[font]);
        }
    }
    return data;
}
