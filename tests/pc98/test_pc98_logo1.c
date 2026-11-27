#include "tests/test.h"

#include "pc98/zun/logo1.h"

#include <string.h>

typedef struct {
    ZUN_LOGO1 logo;
    uint8_t page[2][640 * 400];
    uint8_t *pages[2];
    uint8_t sprites[16 * 64 * 16];
} M_FIXTURE;

static M_FIXTURE m_Fixture;

static M_FIXTURE *M_Init(void)
{
    M_FIXTURE *const f = &m_Fixture;
    memset(f, 0xff, sizeof(*f));
    memset(f->sprites, 0, sizeof(f->sprites));
    for (int32_t i = 0; i < 16; i++) {
        f->sprites[i * 64 * 16] = (uint8_t)(i % 15 + 1);
    }
    f->pages[0] = f->page[0];
    f->pages[1] = f->page[1];
    ZunLogo1_Init(&f->logo, f->pages);
    return f;
}

TEST(initial_state_uses_the_fixed_random_sequence)
{
    const M_FIXTURE *const f = M_Init();
    CHECK_EQ_INT(f->logo.stars[0].x, 346);
    CHECK_EQ_INT(f->logo.stars[0].y, 130);
    CHECK_EQ_INT(f->logo.speeds[0], 12);
    CHECK_EQ_INT(f->logo.stars[1].x, 450);
    CHECK_EQ_INT(f->logo.stars[1].y, 56);
    CHECK_EQ_INT(f->logo.speeds[1], 19);
    CHECK_EQ_INT(f->logo.drawing_page, 0);
    CHECK_EQ_INT(f->logo.visible_page, 1);
    CHECK_EQ_INT(f->page[0][0], 0);
    CHECK_EQ_INT(f->page[1][640 * 400 - 1], 0);
}

TEST(two_ticks_draw_then_swap_pages)
{
    M_FIXTURE *const f = M_Init();
    CHECK(ZunLogo1_Tick(&f->logo, f->pages, f->sprites));
    CHECK_EQ_INT(f->logo.visible_page, 1);
    CHECK_EQ_INT(f->logo.frame, 0);
    CHECK_EQ_INT(f->logo.tone, 2);
    CHECK_EQ_INT(f->logo.circles[0].x, 120);
    CHECK_EQ_INT(f->logo.stars[0].y, 142);
    CHECK(ZunLogo1_Tick(&f->logo, f->pages, f->sprites));
    CHECK_EQ_INT(f->logo.visible_page, 0);
    CHECK_EQ_INT(f->logo.drawing_page, 1);
    CHECK_EQ_INT(f->logo.frame, 1);
    CHECK_EQ_INT(f->logo.stars[0].y, 142);
}

TEST(animation_finishes_the_wave_and_fades_to_black)
{
    M_FIXTURE *const f = M_Init();
    int32_t ticks = 0;
    while (ticks < 600 && ZunLogo1_Tick(&f->logo, f->pages, f->sprites)) {
        ticks++;
        if (f->logo.frame == 110 && !f->logo.half_tick) {
            CHECK_EQ_INT(f->logo.sprite, 6);
            CHECK_EQ_INT(f->logo.period, 3);
            CHECK_EQ_INT(f->logo.amplitude, 80);
        }
        if (f->logo.frame == 130 && !f->logo.half_tick) {
            CHECK_EQ_INT(f->logo.sprite, 8);
            CHECK_EQ_INT(f->logo.period, 23);
            CHECK_EQ_INT(f->logo.amplitude, 0);
        }
    }
    CHECK_EQ_INT(ticks, 461);
    CHECK_EQ_INT(f->logo.frame, 231);
    CHECK_EQ_INT(f->logo.tone, 0);
    CHECK_EQ_INT(f->logo.sprite, 14);
    for (int32_t page = 0; page < 2; page++) {
        for (int32_t y = 0; y < 400; y++) {
            for (int32_t x = 0; x < 640; x++) {
                const uint8_t pixel = f->page[page][y * 640 + x];
                if (x < 96 || x > 543 || y < 100 || y > 299) {
                    CHECK_EQ_INT(pixel, 0);
                }
                CHECK(pixel < 16);
            }
        }
    }
}

TEST(fade_quantizes_to_four_bit_channels)
{
    const ZUN_LOGO1 logo = { .tone = 50 };
    GFX_COLOR source[16] = {};
    source[1] = (GFX_COLOR) { 0xf0, 0x90, 0x60, 0 };
    GFX_COLOR colors[16];
    ZunLogo1_Palette(&logo, source, colors);
    CHECK_EQ_INT(colors[1].red, 119);
    CHECK_EQ_INT(colors[1].green, 68);
    CHECK_EQ_INT(colors[1].blue, 51);
    CHECK_EQ_INT(colors[1].alpha, 255);
    CHECK_EQ_INT(colors[0].alpha, 255);
}
