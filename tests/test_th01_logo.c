#include "th01/logo.h"

#include "core/base/arena.h"

#include "test.h"

static BFNT M_Sprites(const ALLOCATOR *const allocator)
{
    BFNT sprites = { .width = 64, .height = 16, .count = 16 };
    sprites.pixels = Allocator_AllocZ(allocator, 64 * 16 * 16);
    for (int32_t i = 0; i < 16; i++) {
        sprites.pixels[i * 64 * 16] = (uint8_t)(i % 15 + 1);
    }
    return sprites;
}

TEST(initial_state_uses_the_fixed_random_sequence)
{
    ARENA arena;
    Arena_Init(&arena, "logo test", 1024 * 1024);
    LOGO *const logo = Arena_Alloc(&arena, sizeof(*logo));
    Logo_Init(logo);
    CHECK_EQ_INT(logo->stars[0].x, 346);
    CHECK_EQ_INT(logo->stars[0].y, 130);
    CHECK_EQ_INT(logo->speeds[0], 12);
    CHECK_EQ_INT(logo->stars[1].x, 450);
    CHECK_EQ_INT(logo->stars[1].y, 56);
    CHECK_EQ_INT(logo->speeds[1], 19);
    CHECK_EQ_INT(logo->visible_page, 1);
    Arena_Free(&arena);
}

TEST(two_ticks_draw_then_swap_pages)
{
    ARENA arena;
    Arena_Init(&arena, "logo test", 1024 * 1024);
    LOGO *const logo = Arena_Alloc(&arena, sizeof(*logo));
    Logo_Init(logo);
    BFNT sprites = M_Sprites(Arena_Allocator(&arena));
    int32_t changed;
    CHECK(Logo_Tick(logo, &sprites, &changed));
    CHECK_EQ_INT(changed, 0);
    CHECK_EQ_INT(logo->visible_page, 1);
    CHECK_EQ_INT(logo->frame, 0);
    CHECK_EQ_INT(logo->tone, 2);
    CHECK_EQ_INT(logo->circles[0].x, 120);
    CHECK_EQ_INT(logo->stars[0].y, 142);
    CHECK(Logo_Tick(logo, &sprites, &changed));
    CHECK_EQ_INT(changed, -1);
    CHECK_EQ_INT(logo->visible_page, 0);
    CHECK_EQ_INT(logo->drawing_page, 1);
    CHECK_EQ_INT(logo->frame, 1);
    CHECK_EQ_INT(logo->stars[0].y, 142);
    Arena_Free(&arena);
}

TEST(animation_finishes_the_wave_and_fades_to_black)
{
    ARENA arena;
    Arena_Init(&arena, "logo test", 1024 * 1024);
    LOGO *const logo = Arena_Alloc(&arena, sizeof(*logo));
    Logo_Init(logo);
    BFNT sprites = M_Sprites(Arena_Allocator(&arena));
    int32_t changed;
    int32_t ticks = 0;
    while (ticks < 600 && Logo_Tick(logo, &sprites, &changed)) {
        ticks++;
        if (logo->frame == 110 && !logo->half_tick) {
            CHECK_EQ_INT(logo->sprite, 6);
            CHECK_EQ_INT(logo->period, 3);
            CHECK_EQ_INT(logo->amplitude, 80);
        }
        if (logo->frame == 130 && !logo->half_tick) {
            CHECK_EQ_INT(logo->sprite, 8);
            CHECK_EQ_INT(logo->period, 23);
            CHECK_EQ_INT(logo->amplitude, 0);
        }
    }
    CHECK_EQ_INT(ticks, 461);
    CHECK_EQ_INT(logo->frame, 231);
    CHECK_EQ_INT(logo->tone, 0);
    CHECK_EQ_INT(logo->sprite, 14);
    for (int32_t page = 0; page < 2; page++) {
        for (int32_t y = 0; y < LOGO_HEIGHT; y++) {
            for (int32_t x = 0; x < LOGO_WIDTH; x++) {
                const uint8_t pixel = logo->pages[page][y * LOGO_WIDTH + x];
                if (x < 96 || x > 543 || y < 100 || y > 299) {
                    CHECK_EQ_INT(pixel, 0);
                }
                CHECK(pixel < 16);
            }
        }
    }
    Arena_Free(&arena);
}

TEST(fade_quantizes_to_four_bit_channels)
{
    const LOGO logo = { .tone = 50 };
    BFNT sprites = {};
    sprites.palette[1] = (GFX_COLOR) { 0xf0, 0x90, 0x60, 0 };
    GFX_COLOR colors[16];
    Logo_Palette(&logo, &sprites, colors);
    CHECK_EQ_INT(colors[1].red, 119);
    CHECK_EQ_INT(colors[1].green, 68);
    CHECK_EQ_INT(colors[1].blue, 51);
    CHECK_EQ_INT(colors[1].alpha, 255);
    CHECK_EQ_INT(colors[0].alpha, 255);
}
