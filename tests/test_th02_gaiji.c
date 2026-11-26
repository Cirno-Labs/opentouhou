#include "th02/gaiji.h"

#include "core/base/arena.h"

#include "test.h"

TEST(bfnt_registration_keeps_glyph_order_and_both_halves)
{
    uint8_t bytes[32 + 8192] = {};
    memcpy(bytes, "BFNT\x1a", 5);
    bytes[6] = 0x16;
    bytes[8] = 16;
    bytes[10] = 16;
    bytes[14] = 255;
    for (size_t glyph = 0; glyph < 256; glyph++) {
        for (size_t y = 0; y < 16; y++) {
            bytes[32 + glyph * 32 + y * 2] = glyph;
            bytes[33 + glyph * 32 + y * 2] = 0x80u >> (y & 7);
        }
    }
    ARENA arena;
    Arena_Init(&arena, "gaiji test", 131072);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    BFNT font;
    const bool loaded =
        Result_Ignore(Bfnt_Load(&reader, Arena_Allocator(&arena), &font));
    Reader_Close(&reader);
    CHECK(loaded);
    GAIJI bank;
    if (loaded) {
        CHECK(Result_Ignore(Gaiji_Register(&bank, &font)));
    }
    Arena_Free(&arena);
    if (loaded) {
        for (size_t glyph = 0; glyph < 256; glyph++) {
            for (size_t y = 0; y < 16; y++) {
                CHECK_EQ_INT(
                    bank.rows[glyph][y], (glyph << 8) | (0x80u >> (y & 7)));
            }
        }
    }
}

TEST(invalid_fonts_preserve_the_previous_bank)
{
    uint8_t pixels[65536] = {};
    BFNT valid = { .width = 16,
                   .height = 16,
                   .count = 256,
                   .bits_per_pixel = 1,
                   .pixels = pixels };
    GAIJI bank;
    memset(&bank, 0xa5, sizeof(bank));
    const GAIJI saved = bank;
    for (int32_t fault = 0; fault < 9; fault++) {
        BFNT font = valid;
        switch (fault) {
        case 0:
            font.width = 8;
            break;
        case 1:
            font.height = 8;
            break;
        case 2:
            font.count = 255;
            break;
        case 3:
            font.first = 1;
            break;
        case 4:
            font.bits_per_pixel = 4;
            break;
        case 5:
            font.has_palette = true;
            break;
        case 6:
            font.pixels = nullptr;
            break;
        case 7:
            pixels[0] = 2;
            break;
        case 8:
            pixels[65535] = 255;
            break;
        }
        CHECK(!Result_Ignore(Gaiji_Register(&bank, &font)));
        CHECK(memcmp(&bank, &saved, sizeof(bank)) == 0);
        pixels[0] = 0;
        pixels[65535] = 0;
    }
    CHECK(Result_Ignore(Gaiji_Register(&bank, &valid)));
    for (size_t glyph = 0; glyph < 256; glyph++) {
        for (size_t y = 0; y < 16; y++) {
            CHECK_EQ_INT(bank.rows[glyph][y], 0);
        }
    }
}
