#include "th02_tram_fixture.h"
#include "th02/presenter.h"

#include "core/base/arena.h"
#include "core/gfx.h"

#include "test.h"

#include <SDL3/SDL.h>

static SDL_Window *m_Window;
static SDL_GLContext m_Context;

static RESULT M_Init(void)
{
    if (m_Context != nullptr) {
        return OK;
    }
    FAIL_IF(!SDL_Init(SDL_INIT_VIDEO), "%s", SDL_GetError());
    const bool es = getenv("TEST_GFX_ES") != nullptr;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
    m_Window = SDL_CreateWindow(
        "TH02 presenter tests", VIDEO_WIDTH, VIDEO_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    FAIL_IF(m_Window == nullptr, "%s", SDL_GetError());
    m_Context = SDL_GL_CreateContext(m_Window);
    FAIL_IF(m_Context == nullptr, "%s", SDL_GetError());
    return Gfx_Init(m_Window, VIDEO_WIDTH, VIDEO_HEIGHT);
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    if (m_Context != nullptr) {
        Gfx_Shutdown();
        SDL_GL_DestroyContext(m_Context);
    }
    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

static void M_Pages(void)
{
    for (size_t page = 0; page < 2; page++) {
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            g_Video.pages[page][i] = (i / 16 + i / 640 + page * 5) % 16;
        }
    }
    for (uint8_t color = 0; color < 16; color++) {
        Video_SetDac(color, color, color ^ 5, 15 - color);
    }
}

static void M_Background(GFX_COLOR *const expected)
{
    for (size_t y = 0; y < VIDEO_HEIGHT; y++) {
        size_t origin = g_Video.first_address * 16u;
        size_t line = y;
        if (g_Video.first_lines != 0 && y >= g_Video.first_lines) {
            origin = g_Video.second_address * 16u;
            line -= g_Video.first_lines;
        }
        for (size_t x = 0; x < VIDEO_WIDTH; x++) {
            const size_t offset = (origin + line * 640 + x) & 0x3ffff;
            const size_t index =
                (offset / 16 + offset / 640 + g_Video.visible_page * 5) % 16;
            expected[y * VIDEO_WIDTH + x] = g_Video.palette[index];
        }
    }
}

static void M_Check(const IMAGE *const image, const GFX_COLOR *const expected)
{
    CHECK_EQ_INT(image->width, VIDEO_WIDTH);
    CHECK_EQ_INT(image->height, VIDEO_HEIGHT);
    for (size_t i = 0; i < VIDEO_VISIBLE_SIZE; i++) {
        const uint8_t *const pixel = image->pixels + i * 4;
        const GFX_COLOR color = expected[i];
        if (pixel[0] != color.red || pixel[1] != color.green
            || pixel[2] != color.blue || pixel[3] != color.alpha) {
            TEST_FAIL(
                "pixel (%zu,%zu): expected %u,%u,%u,%u; got %u,%u,%u,%u",
                i % VIDEO_WIDTH, i / VIDEO_WIDTH, color.red, color.green,
                color.blue, color.alpha, pixel[0], pixel[1], pixel[2],
                pixel[3]);
            return;
        }
    }
}

static void M_CheckScreen(const GFX_COLOR *const expected)
{
    Gfx_EndTick();
    IMAGE image;
    CHECK(Result_Ignore(Gfx_ReadScreen(&g_HeapAllocator, &image)));
    M_Check(&image, expected);
    Image_Free(&image);
}

TEST(graphics_present_both_pages_split_scroll_hidden_tail_and_palette_changes)
{
    if (!Result_Log(M_Init(), LOG_LEVEL_ERROR)) {
        TEST_FAIL("Cannot initialize OpenGL test context");
        return;
    }
    ARENA arena;
    Arena_Init(&arena, "presenter graphics", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    M_Pages();
    CHECK(Result_Ignore(Presenter_Init()));
    GFX_COLOR *const expected =
        Mem_Alloc(VIDEO_VISIBLE_SIZE * sizeof(*expected));
    const uint16_t cases[][3] = {
        { 400, 0, 0 },   { 1, 39, 16000 },   { 123, 65535, 32760 },
        { 0, 16000, 0 }, { 399, 80, 16383 }, { 200, 16370, 65000 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        for (uint8_t page = 0; page < 2; page++) {
            Video_ShowPage(page);
            Video_AccessPage(page ^ 1);
            Video_SetSplit(cases[i][0], cases[i][1], cases[i][2]);
            Video_SetDac(7, i + page, 15 - i, 3 + page);
            const VIDEO before = g_Video;
            M_Background(expected);
            Gfx_BeginTick((GFX_COLOR) { 255, 0, 255, 255 });
            Presenter_Draw(false, nullptr, nullptr, 0, false);
            M_CheckScreen(expected);
            CHECK(memcmp(&g_Video, &before, sizeof(g_Video)) == 0);
        }
    }
    const GFX_OUTPUT output = { .width = VIDEO_WIDTH, .height = VIDEO_HEIGHT };
    for (size_t interpolate = 0; interpolate < 2; interpolate++) {
        CHECK(Result_Ignore(Gfx_Present(interpolate != 0, 0.5f, &output)));
        IMAGE image;
        CHECK(Result_Ignore(Gfx_ReadOutput(&g_HeapAllocator, &image)));
        M_Check(&image, expected);
        Image_Free(&image);
    }
    for (size_t page = 0; page < 2; page++) {
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            CHECK_EQ_INT(
                g_Video.pages[page][i], (i / 16 + i / 640 + page * 5) % 16);
        }
    }
    Mem_Free(expected);
    Presenter_Close();
    Presenter_Close();
    Arena_Free(&arena);
}

static uint16_t M_TestRow(
    const void *const context, const uint16_t code, const bool wide,
    const uint8_t y)
{
    return code == 'A' ? 0xaa00
        : code == 'B'  ? 0x5500
        : code == 'C'  ? 0xff00
                       : 0;
}

TEST(text_overlay_keeps_digital_colors_blink_gaiji_and_wrapping_text_start)
{
    if (!Result_Log(M_Init(), LOG_LEVEL_ERROR)) {
        TEST_FAIL("Cannot initialize OpenGL test context");
        return;
    }
    ARENA arena;
    Arena_Init(&arena, "presenter text", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    M_Pages();
    CHECK(Result_Ignore(Presenter_Init()));

    Tram_Init();
    m_Tram.codes[4094] = 'A';
    m_Tram.codes[4095] = 'B';
    m_Tram.codes[0] = 'C';
    m_Tram.codes[1] = m_Tram.codes[2] = 0x0156;
    m_Tram.codes[3] = 'D';
    m_Tram.attributes[4094] = 0x01;
    m_Tram.attributes[4095] = 0xe1;
    m_Tram.attributes[0] = 0x23;
    m_Tram.attributes[1] = m_Tram.attributes[2] = 0x81;
    m_Tram.attributes[3] = 0x65;
    const TRAM before = m_Tram;
    GAIJI gaiji = {};
    for (size_t y = 0; y < 16; y++) {
        gaiji.rows[1][y] = 0x4081;
    }
    const TRAM_FONT font = { M_TestRow, nullptr };
    GFX_COLOR *const expected =
        Mem_Alloc(VIDEO_VISIBLE_SIZE * sizeof(*expected));
    for (size_t mode = 0; mode < 3; mode++) {
        M_Background(expected);
        if (mode != 2) {
            for (size_t y = 0; y < 16; y++) {
                for (size_t x = 0; x < 48; x++) {
                    GFX_COLOR *const pixel = &expected[y * VIDEO_WIDTH + x];
                    if (x < 8 && x % 2 == 0) {
                        *pixel = (GFX_COLOR) { 0, 0, 0, 255 };
                    } else if (x >= 8 && x < 16 && x % 2 == 1) {
                        *pixel = (GFX_COLOR) { 255, 255, 255, 255 };
                    } else if (x >= 16 && x < 24 && mode == 0) {
                        *pixel = (GFX_COLOR) { 0, 0, 255, 255 };
                    } else if (x == 25 || x == 32 || x == 39) {
                        *pixel = (GFX_COLOR) { 0, 255, 0, 255 };
                    } else if (x >= 40) {
                        *pixel = (GFX_COLOR) { 255, 0, 255, 255 };
                    }
                }
            }
        }
        Gfx_BeginTick((GFX_COLOR) { 255, 0, 255, 255 });
        Presenter_Draw(mode != 2, &gaiji, &font, 4094, mode == 0);
        M_CheckScreen(expected);
        CHECK(memcmp(&m_Tram, &before, sizeof(m_Tram)) == 0);
    }
    Mem_Free(expected);
    Presenter_Close();
    Arena_Free(&arena);
}
