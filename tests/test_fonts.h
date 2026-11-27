#pragma once

// Gives the tests their fonts. TEST_FONT_DIR names the folder of the
// built-in fonts, and $OPENTOUHOU_MSGOTHIC, where it is set, the
// msgothic.ttc that the MS Gothic tests use.

#include <stddef.h>
#include <stdint.h>

#define TEST_FONT_GOTHIC "ipag.ttf"
#define TEST_FONT_UI_GOTHIC "ipagp.ttf"

// Returns the contents of the file at path, and stops the test when it
// cannot be read. Mem_Free releases them.
uint8_t *TestFonts_ReadFile(const char *path, size_t *size);

// Returns the contents of the built-in font file name.
uint8_t *TestFonts_Read(const char *name, size_t *size);

// Adds the built-in font file name to the registry.
void TestFonts_Add(const char *name);

// Leaves the registry with the two built-in fonts alone, as the native
// backend has it.
void TestFonts_UseBuiltin(void);

// Returns the path of msgothic.ttc, or exits with meson's skip status when
// $OPENTOUHOU_MSGOTHIC names none.
const char *TestFonts_RequireMsGothic(void);

uint32_t TestFonts_ReadU32Be(const uint8_t *data);

void TestFonts_WriteU32Be(uint8_t *data, uint32_t value);

// Builds a TrueType collection of the fonts a and b, in that order, with
// the tables of each moved behind the header. Mem_Free releases it.
uint8_t *TestFonts_MakeCollection(
    const uint8_t *a, size_t a_size, const uint8_t *b, size_t b_size,
    size_t *size);
