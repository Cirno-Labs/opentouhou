#include "tests/test.h"

#include "core/base/memory.h"
#include "core/gfx.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 8
#define BLACK ((GFX_COLOR) { 0, 0, 0, 255 })
#define WHITE ((GFX_COLOR) { 255, 255, 255, 255 })
#define RED ((GFX_COLOR) { 255, 0, 0, 255 })
#define GREEN ((GFX_COLOR) { 0, 255, 0, 255 })
#define BLUE ((GFX_COLOR) { 0, 0, 255, 255 })
#define CHECK_OK(expression_) CHECK(Result_Log((expression_), LOG_LEVEL_ERROR))
#define REQUIRE_GL()                                                           \
    do {                                                                       \
        if (!Result_Log(M_Init(), LOG_LEVEL_ERROR)) {                          \
            TEST_FAIL("Cannot initialize OpenGL test context");                \
            return;                                                            \
        }                                                                      \
    } while (0)

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
        "gfx tests", 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    FAIL_IF(m_Window == nullptr, "%s", SDL_GetError());
    m_Context = SDL_GL_CreateContext(m_Window);
    FAIL_IF(m_Context == nullptr, "%s", SDL_GetError());
    return Gfx_Init(m_Window, SIZE, SIZE);
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

static GFX_TEXTURE *M_Texture(
    const int32_t width, const int32_t height, const GFX_COLOR *const colors)
{
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    memcpy(image.pixels, colors, (size_t)width * (size_t)height * 4);
    GFX_TEXTURE *texture = nullptr;
    CHECK_OK(GfxTexture_Create(&image, &texture));
    Image_Free(&image);
    return texture;
}

// Draws the rectangle x0, y0 to x1, y1 at depth z, with texture
// coordinates from 0 to uv.
static void M_Rect(
    const GFX_STATE *const state, GFX_TEXTURE *const texture, const float x0,
    const float y0, const float x1, const float y1, const float z,
    const GFX_COLOR color, const float uv)
{
    const GFX_VERTEX vertices[4] = {
        { { x0, y0, z }, color, { 0, 0 } },
        { { x1, y0, z }, color, { uv, 0 } },
        { { x1, y1, z }, color, { uv, uv } },
        { { x0, y1, z }, color, { 0, uv } },
    };
    Gfx_DrawQuad(state, texture, vertices, 0);
}

static void M_Fill(
    const GFX_STATE *const state, GFX_TEXTURE *const texture,
    const GFX_COLOR color)
{
    M_Rect(state, texture, 0, 0, SIZE, SIZE, 0.5f, color, 1.0f);
}

static IMAGE M_Screen(void)
{
    Gfx_EndTick();
    IMAGE image = {};
    CHECK_OK(Gfx_ReadScreen(&g_HeapAllocator, &image));
    if (image.pixels == nullptr) {
        abort();
    }
    return image;
}

static IMAGE M_Output(const bool interpolate, const int32_t width)
{
    const GFX_OUTPUT output = {
        .width = width,
        .height = 16,
        .border = BLUE,
    };
    CHECK_OK(Gfx_Present(interpolate, 1.0f, &output));
    IMAGE image = {};
    CHECK_OK(Gfx_ReadOutput(&g_HeapAllocator, &image));
    if (image.pixels == nullptr) {
        abort();
    }
    return image;
}

static void M_CheckPixel(
    const IMAGE *const image, const int32_t x, const int32_t y,
    const GFX_COLOR expected, const int tolerance)
{
    const uint8_t *const actual =
        image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4;
    const uint8_t wanted[4] = { expected.red, expected.green, expected.blue,
                                expected.alpha };
    for (int i = 0; i < 4; i++) {
        if (abs((int)actual[i] - wanted[i]) > tolerance) {
            TEST_FAIL(
                "pixel (%d,%d) channel %d: expected %u, got %u", x, y, i,
                wanted[i], actual[i]);
        }
    }
}

static const GFX_STATE m_Opaque = {};

TEST(clear_and_texture_orientation)
{
    REQUIRE_GL();
    const GFX_COLOR colors[4] = { RED, GREEN, BLUE, WHITE };
    GFX_TEXTURE *const texture = M_Texture(2, 2, colors);
    Gfx_BeginTick(BLACK);
    M_Rect(&m_Opaque, texture, 0, 0, SIZE, SIZE / 2, 0.5f, WHITE, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 1, 1, RED, 0);
    M_CheckPixel(&image, 6, 1, GREEN, 0);
    M_CheckPixel(&image, 1, 3, BLUE, 0);
    M_CheckPixel(&image, 6, 3, WHITE, 0);
    M_CheckPixel(&image, 3, 6, BLACK, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(blend_modes)
{
    REQUIRE_GL();
    const GFX_COLOR half_green = { 0, 255, 0, 128 };
    Gfx_BeginTick(RED);
    M_Rect(&m_Opaque, nullptr, 0, 0, 2, SIZE, 0.5f, half_green, 1.0f);
    const GFX_STATE alpha = { .blend = GFX_BLEND_ALPHA };
    M_Rect(&alpha, nullptr, 2, 0, 4, SIZE, 0.5f, half_green, 1.0f);
    const GFX_STATE add = { .blend = GFX_BLEND_ADD };
    M_Rect(&add, nullptr, 4, 0, 6, SIZE, 0.5f, half_green, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, GREEN, 0);
    M_CheckPixel(&image, 2, 0, (GFX_COLOR) { 127, 128, 0, 255 }, 1);
    M_CheckPixel(&image, 4, 0, (GFX_COLOR) { 255, 128, 0, 255 }, 1);
    M_CheckPixel(&image, 6, 0, RED, 0);
    Image_Free(&image);
}

TEST(color_ops_modulate_or_add_and_alpha_multiplies)
{
    REQUIRE_GL();
    const GFX_COLOR texel = { 128, 64, 0, 255 };
    GFX_TEXTURE *const texture = M_Texture(1, 1, &texel);
    const GFX_COLOR vertex = { 255, 128, 128, 128 };
    Gfx_BeginTick(BLACK);
    const GFX_STATE modulate = { .blend = GFX_BLEND_ALPHA };
    M_Rect(&modulate, texture, 0, 0, 4, SIZE, 0.5f, vertex, 1.0f);
    const GFX_STATE add = { .blend = GFX_BLEND_ALPHA,
                            .color_op = GFX_COLOR_ADD };
    M_Rect(&add, texture, 4, 0, SIZE, SIZE, 0.5f, vertex, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, (GFX_COLOR) { 64, 16, 0, 255 }, 2);
    M_CheckPixel(&image, 4, 0, (GFX_COLOR) { 128, 96, 64, 255 }, 2);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(alpha_test_discards_alpha_below_four_with_its_depth)
{
    REQUIRE_GL();
    const GFX_COLOR texels[4] = {
        { 255, 255, 255, 0 },
        { 255, 255, 255, 1 },
        { 255, 255, 255, 3 },
        { 255, 255, 255, 4 },
    };
    GFX_TEXTURE *const texture = M_Texture(4, 1, texels);
    Gfx_BeginTick(BLACK);
    const GFX_STATE tested = {
        .depth_test = GFX_DEPTH_LESS_EQUAL,
        .depth_write = true,
        .alpha_test = true,
    };
    M_Rect(&tested, texture, 0, 0, SIZE, SIZE, 0.2f, WHITE, 1.0f);
    const GFX_STATE behind = {
        .depth_test = GFX_DEPTH_LESS_EQUAL,
        .depth_write = true,
    };
    M_Rect(&behind, nullptr, 0, 0, SIZE, SIZE, 0.6f, RED, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 1, 1, RED, 0);
    M_CheckPixel(&image, 2, 1, RED, 0);
    M_CheckPixel(&image, 4, 1, RED, 0);
    M_CheckPixel(&image, 6, 1, WHITE, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(depth_less_equal_always_and_off)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    const GFX_STATE write = { .depth_test = GFX_DEPTH_ALWAYS,
                              .depth_write = true };
    M_Rect(&write, nullptr, 0, 0, SIZE, SIZE, 0.5f, RED, 1.0f);
    const GFX_STATE less_equal = { .depth_test = GFX_DEPTH_LESS_EQUAL };
    M_Rect(&less_equal, nullptr, 0, 0, 2, SIZE, 0.7f, GREEN, 1.0f);
    M_Rect(&less_equal, nullptr, 2, 0, 4, SIZE, 0.5f, GREEN, 1.0f);
    const GFX_STATE always = { .depth_test = GFX_DEPTH_ALWAYS };
    M_Rect(&always, nullptr, 4, 0, 6, SIZE, 0.9f, BLUE, 1.0f);
    M_Rect(&m_Opaque, nullptr, 6, 0, SIZE, SIZE, 0.9f, WHITE, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, RED, 0);
    M_CheckPixel(&image, 2, 0, GREEN, 0);
    M_CheckPixel(&image, 4, 0, BLUE, 0);
    M_CheckPixel(&image, 6, 0, WHITE, 0);
    Image_Free(&image);
}

TEST(nearby_depths_compare_at_sixteen_bit_precision)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    const GFX_STATE state = {
        .depth_test = GFX_DEPTH_LESS_EQUAL,
        .depth_write = true,
    };
    M_Rect(&state, nullptr, 0, 0, SIZE, SIZE, 0.500001f, RED, 1.0f);
    M_Rect(&state, nullptr, 0, 0, SIZE, SIZE, 0.500002f, GREEN, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 4, 4, GREEN, 0);
    Image_Free(&image);
}

TEST(fog_mixes_toward_its_color_by_view_depth)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    GFX_VIEW view = GfxView_Screen();
    view.fog = (GFX_FOG) {
        .enabled = true, .begin = 0.0f, .end = 1.0f, .color = BLUE
    };
    Gfx_SetView(&view);
    M_Rect(&m_Opaque, nullptr, 0, 0, 4, SIZE, 0.0f, RED, 1.0f);
    M_Rect(&m_Opaque, nullptr, 4, 0, SIZE, SIZE, 0.5f, RED, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, RED, 0);
    M_CheckPixel(&image, 4, 0, (GFX_COLOR) { 128, 0, 128, 255 }, 1);
    Image_Free(&image);
}

TEST(fog_is_clamped_after_interpolation)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    GFX_VIEW view = GfxView_Screen();
    view.projection.m[2][2] = 0.0f;
    view.projection.m[3][2] = 0.5f;
    view.fog = (GFX_FOG) {
        .enabled = true, .begin = 1.0f, .end = 2.0f, .color = BLUE
    };
    Gfx_SetView(&view);
    const GFX_VERTEX vertices[4] = {
        { { 0, 0, 0 }, RED, { 0, 0 } },
        { { SIZE, 0, 4 }, RED, { 0, 0 } },
        { { SIZE, SIZE, 4 }, RED, { 0, 0 } },
        { { 0, SIZE, 0 }, RED, { 0, 0 } },
    };
    Gfx_DrawQuad(&m_Opaque, nullptr, vertices, 0);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 4, RED, 0);
    M_CheckPixel(&image, 2, 4, (GFX_COLOR) { 191, 0, 64, 255 }, 1);
    M_CheckPixel(&image, 4, 4, BLUE, 0);
    Image_Free(&image);
}

TEST(persistent_screens_keep_color_and_clear_depth_in_both_presentations)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    const GFX_STATE state = { .depth_test = GFX_DEPTH_LESS_EQUAL,
                              .depth_write = true };
    M_Rect(&state, nullptr, 0, 0, SIZE, SIZE, 0.1f, RED, 1.0f);
    Gfx_EndTick();
    Gfx_SetPersistentScreen(true);
    Gfx_BeginTick(BLUE);
    M_Rect(&state, nullptr, 0, 0, 4, SIZE, 0.7f, GREEN, 1.0f);
    IMAGE screen = M_Screen();
    M_CheckPixel(&screen, 1, 1, GREEN, 0);
    M_CheckPixel(&screen, 6, 1, RED, 0);
    Image_Free(&screen);
    for (int32_t interpolate = 0; interpolate <= 1; interpolate++) {
        IMAGE output = M_Output(interpolate != 0, 16);
        M_CheckPixel(&output, 2, 2, GREEN, 0);
        M_CheckPixel(&output, 12, 2, RED, 0);
        Image_Free(&output);
    }
    Gfx_SetPersistentScreen(false);
}

TEST(repeat_wraps_and_clamp_stretches_the_edge)
{
    REQUIRE_GL();
    const GFX_COLOR texels[2] = { RED, GREEN };
    GFX_TEXTURE *const texture = M_Texture(2, 1, texels);
    Gfx_BeginTick(BLACK);
    const GFX_STATE repeat = { .repeat = true };
    M_Rect(&repeat, texture, 0, 0, SIZE, 4, 0.5f, WHITE, 2.0f);
    M_Rect(&m_Opaque, texture, 0, 4, SIZE, SIZE, 0.5f, WHITE, 2.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, RED, 0);
    M_CheckPixel(&image, 2, 0, GREEN, 0);
    M_CheckPixel(&image, 4, 0, RED, 0);
    M_CheckPixel(&image, 6, 0, GREEN, 0);
    M_CheckPixel(&image, 0, 4, RED, 0);
    M_CheckPixel(&image, 2, 4, GREEN, 0);
    M_CheckPixel(&image, 6, 4, GREEN, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(a_linear_filter_blends_the_texels_that_a_pixel_covers)
{
    REQUIRE_GL();
    const GFX_COLOR texels[2] = { RED, GREEN };
    GFX_TEXTURE *const texture = M_Texture(2, 1, texels);
    Gfx_BeginTick(BLACK);
    const GFX_STATE linear = { .filter = GFX_FILTER_LINEAR };
    M_Rect(&linear, texture, 0, 0, 1, SIZE, 0.5f, WHITE, 1.0f);
    M_Rect(&m_Opaque, texture, 1, 0, 2, SIZE, 0.5f, WHITE, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, (GFX_COLOR) { 128, 128, 0, 255 }, 1);
    const uint8_t *const nearest = &image.pixels[4];
    CHECK(
        (nearest[0] == 255 && nearest[1] == 0)
        || (nearest[0] == 0 && nearest[1] == 255));
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(a_viewport_remaps_and_scissors_its_view)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    GFX_VIEW view = GfxView_Screen();
    view.viewport = (VIEWPORT) { 4, 0, 4, 4, 0, 1 };
    Gfx_SetView(&view);
    M_Rect(&m_Opaque, nullptr, -4, -4, 12, 12, 0.5f, RED, 1.0f);
    GFX_VIEW half = GfxView_Screen();
    half.viewport = (VIEWPORT) { 0, 4, 8, 4, 0, 1 };
    Gfx_SetView(&half);
    M_Rect(&m_Opaque, nullptr, 0, 0, 4, 8, 0.5f, GREEN, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 3, 0, BLACK, 0);
    M_CheckPixel(&image, 4, 0, RED, 0);
    M_CheckPixel(&image, 7, 3, RED, 0);
    M_CheckPixel(&image, 7, 4, BLACK, 0);
    M_CheckPixel(&image, 0, 4, GREEN, 0);
    M_CheckPixel(&image, 3, 7, GREEN, 0);
    M_CheckPixel(&image, 4, 7, BLACK, 0);
    Image_Free(&image);
}

TEST(present_scales_the_screen_into_a_letterbox)
{
    REQUIRE_GL();
    for (int32_t interpolate = 0; interpolate < 2; interpolate++) {
        Gfx_BeginTick(BLACK);
        M_Rect(&m_Opaque, nullptr, 0, 0, 1, 1, 0.5f, RED, 1.0f);
        GFX_VIEW view = GfxView_Screen();
        view.viewport = (VIEWPORT) { 4, 4, 4, 4, 0, 1 };
        Gfx_SetView(&view);
        M_Rect(&m_Opaque, nullptr, 0, 0, SIZE, SIZE, 0.5f, GREEN, 1.0f);
        Gfx_EndTick();
        IMAGE image = M_Output(interpolate != 0, 24);
        CHECK_EQ_INT(image.width, 24);
        M_CheckPixel(&image, 3, 0, BLUE, 0);
        M_CheckPixel(&image, 4, 0, RED, 0);
        M_CheckPixel(&image, 5, 1, RED, 0);
        M_CheckPixel(&image, 6, 2, BLACK, 0);
        M_CheckPixel(&image, 11, 7, BLACK, 0);
        M_CheckPixel(&image, 12, 8, GREEN, 0);
        M_CheckPixel(&image, 19, 15, GREEN, 0);
        M_CheckPixel(&image, 20, 15, BLUE, 0);
        Image_Free(&image);
    }
}

TEST(a_smooth_output_blends_the_pixels_that_it_scales)
{
    REQUIRE_GL();
    const GFX_COLOR texels[2] = { RED, GREEN };
    GFX_TEXTURE *const texture = M_Texture(2, 1, texels);
    for (int32_t interpolate = 0; interpolate < 2; interpolate++) {
        for (int32_t smooth = 0; smooth < 2; smooth++) {
            Gfx_BeginTick(BLACK);
            M_Rect(&m_Opaque, texture, 0, 0, 2, SIZE, 0.5f, WHITE, 1.0f);
            Gfx_EndTick();
            const GFX_OUTPUT output = {
                .width = 24,
                .height = 16,
                .border = BLUE,
                .filter = smooth != 0 ? GFX_FILTER_LINEAR : GFX_FILTER_NEAREST,
            };
            CHECK_OK(Gfx_Present(interpolate != 0, 1.0f, &output));
            IMAGE image = {};
            CHECK_OK(Gfx_ReadOutput(&g_HeapAllocator, &image));
            if (image.pixels == nullptr) {
                abort();
            }
            M_CheckPixel(&image, 4, 4, RED, 0);
            M_CheckPixel(
                &image, 5, 4,
                smooth != 0 ? (GFX_COLOR) { 191, 64, 0, 255 } : RED, 2);
            Image_Free(&image);
        }
    }
    GfxTexture_Free(texture);
}

TEST(a_sharp_output_keeps_a_linear_sprite_inside_its_texels)
{
    REQUIRE_GL();
    const GFX_COLOR texels[2] = { RED, GREEN };
    GFX_TEXTURE *const texture = M_Texture(1, 2, texels);
    const GFX_STATE sprite = {
        .repeat = true,
        .filter = GFX_FILTER_LINEAR,
    };
    Gfx_BeginTick(BLACK);
    const GFX_VERTEX vertices[4] = {
        { { 0, 0, 0.5f }, WHITE, { 0, 0 } },
        { { SIZE, 0, 0.5f }, WHITE, { 1, 0 } },
        { { SIZE, SIZE, 0.5f }, WHITE, { 1, 0.5f } },
        { { 0, SIZE, 0.5f }, WHITE, { 0, 0.5f } },
    };
    Gfx_DrawQuad(&sprite, texture, vertices, 0);
    Gfx_EndTick();
    IMAGE image = M_Output(true, 16);
    M_CheckPixel(&image, 8, 0, RED, 0);
    M_CheckPixel(&image, 8, 15, RED, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

static IMAGE M_SmoothOutput(void)
{
    const GFX_OUTPUT output = {
        .width = 16,
        .height = 16,
        .border = BLUE,
        .filter = GFX_FILTER_LINEAR,
    };
    CHECK_OK(Gfx_Present(true, 1.0f, &output));
    IMAGE image = {};
    CHECK_OK(Gfx_ReadOutput(&g_HeapAllocator, &image));
    if (image.pixels == nullptr) {
        abort();
    }
    return image;
}

TEST(a_smooth_output_keeps_a_linear_sprite_inside_its_texels)
{
    REQUIRE_GL();
    const GFX_COLOR texels[2] = { RED, GREEN };
    GFX_TEXTURE *const texture = M_Texture(1, 2, texels);
    const GFX_STATE sprite = {
        .repeat = true,
        .filter = GFX_FILTER_LINEAR,
    };
    Gfx_BeginTick(BLACK);
    const GFX_VERTEX vertices[4] = {
        { { 0, 0, 0.5f }, WHITE, { 0, 0 } },
        { { SIZE, 0, 0.5f }, WHITE, { 1, 0 } },
        { { SIZE, SIZE, 0.5f }, WHITE, { 1, 0.5f } },
        { { 0, SIZE, 0.5f }, WHITE, { 0, 0.5f } },
    };
    Gfx_DrawQuad(&sprite, texture, vertices, 0);
    IMAGE screen = M_Screen();
    // The logical screen still blends the edge with the wrapped texel.
    M_CheckPixel(&screen, 4, 0, (GFX_COLOR) { 143, 112, 0, 255 }, 1);
    M_CheckPixel(&screen, 4, 7, (GFX_COLOR) { 143, 112, 0, 255 }, 1);
    Image_Free(&screen);
    IMAGE image = M_SmoothOutput();
    M_CheckPixel(&image, 8, 0, RED, 0);
    M_CheckPixel(&image, 8, 15, RED, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(a_smooth_output_still_tiles_a_repeating_quad)
{
    REQUIRE_GL();
    const GFX_COLOR texels[2] = { RED, GREEN };
    GFX_TEXTURE *const texture = M_Texture(2, 1, texels);
    const GFX_STATE tiles = {
        .repeat = true,
        .filter = GFX_FILTER_LINEAR,
    };
    Gfx_BeginTick(BLACK);
    M_Rect(&tiles, texture, 0, 0, SIZE, SIZE, 0.5f, WHITE, 2.0f);
    Gfx_EndTick();
    IMAGE image = M_SmoothOutput();
    M_CheckPixel(&image, 10, 4, (GFX_COLOR) { 223, 32, 0, 255 }, 2);
    M_CheckPixel(&image, 14, 4, (GFX_COLOR) { 32, 223, 0, 255 }, 2);
    M_CheckPixel(&image, 0, 4, (GFX_COLOR) { 159, 96, 0, 255 }, 2);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(present_draws_finer_than_a_logical_pixel)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    M_Rect(&m_Opaque, nullptr, 0.5f, 0, 1.5f, 1, 0.5f, RED, 1.0f);
    Gfx_EndTick();
    IMAGE image = M_Output(true, 16);
    M_CheckPixel(&image, 0, 0, BLACK, 0);
    M_CheckPixel(&image, 1, 0, RED, 0);
    M_CheckPixel(&image, 2, 0, RED, 0);
    M_CheckPixel(&image, 3, 0, BLACK, 0);
    Image_Free(&image);
}

TEST(copy_from_screen_takes_the_screen_drawn_so_far)
{
    REQUIRE_GL();
    GFX_TEXTURE *texture = nullptr;
    CHECK_OK(GfxTexture_CreateEmpty(SIZE, SIZE, &texture));
    Gfx_BeginTick(BLACK);
    M_Rect(&m_Opaque, nullptr, 0, 0, SIZE, 4, 0.5f, RED, 1.0f);
    GfxTexture_CopyFromScreen(texture, (GFX_RECT) { 0, 0, SIZE, SIZE }, 0, 0);
    M_Fill(&m_Opaque, nullptr, GREEN);
    M_Rect(&m_Opaque, texture, 0, 0, 4, SIZE, 0.5f, WHITE, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, RED, 0);
    M_CheckPixel(&image, 0, 5, BLACK, 0);
    M_CheckPixel(&image, 5, 0, GREEN, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(upload_changes_only_the_quads_drawn_after_it)
{
    REQUIRE_GL();
    GFX_TEXTURE *const texture =
        M_Texture(1, 1, &(GFX_COLOR) { 255, 0, 0, 255 });
    IMAGE green = Image_Create(1, 1, &g_HeapAllocator);
    memcpy(green.pixels, &GREEN, 4);
    Gfx_BeginTick(BLACK);
    M_Rect(&m_Opaque, texture, 0, 0, 4, SIZE, 0.5f, WHITE, 1.0f);
    GfxTexture_Upload(texture, (GFX_RECT) { 0, 0, 1, 1 }, &green);
    M_Rect(&m_Opaque, texture, 4, 0, SIZE, SIZE, 0.5f, WHITE, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, RED, 0);
    M_CheckPixel(&image, 4, 0, GREEN, 0);
    Image_Free(&image);
    Image_Free(&green);
    GfxTexture_Free(texture);
}

TEST(the_screen_reads_back_without_window_scaling_or_borders)
{
    REQUIRE_GL();
    Gfx_BeginTick(RED);
    Gfx_EndTick();
    IMAGE window = M_Output(false, 24);
    CHECK_EQ_INT(window.width, 24);
    CHECK_EQ_INT(window.height, 16);
    M_CheckPixel(&window, 0, 0, BLUE, 0);
    M_CheckPixel(&window, 12, 8, RED, 0);
    IMAGE screen;
    CHECK_OK(Gfx_ReadScreen(&g_HeapAllocator, &screen));
    CHECK_EQ_INT(screen.width, SIZE);
    CHECK_EQ_INT(screen.height, SIZE);
    M_CheckPixel(&screen, 0, 0, RED, 0);
    M_CheckPixel(&screen, 7, 7, RED, 0);
    Image_Free(&screen);
    Image_Free(&window);
}

TEST(a_screen_rect_reads_mid_tick_and_drawing_goes_on)
{
    REQUIRE_GL();
    Gfx_BeginTick(BLACK);
    M_Rect(&m_Opaque, nullptr, 0, 0, SIZE, 4, 0.5f, RED, 1.0f);
    IMAGE rect;
    CHECK_OK(
        Gfx_ReadScreenRect((GFX_RECT) { 2, 3, 4, 2 }, &g_HeapAllocator, &rect));
    M_Rect(&m_Opaque, nullptr, 0, 4, SIZE, SIZE, 0.5f, GREEN, 1.0f);
    IMAGE screen = M_Screen();
    CHECK_EQ_INT(rect.width, 4);
    CHECK_EQ_INT(rect.height, 2);
    M_CheckPixel(&rect, 0, 0, RED, 0);
    M_CheckPixel(&rect, 3, 0, RED, 0);
    M_CheckPixel(&rect, 0, 1, BLACK, 0);
    M_CheckPixel(&screen, 0, 0, RED, 0);
    M_CheckPixel(&screen, 7, 7, GREEN, 0);
    Image_Free(&screen);
    Image_Free(&rect);
}

TEST(alpha_test_keeps_fractional_alpha_until_the_comparison)
{
    REQUIRE_GL();
    const GFX_COLOR texels[] = { { 255, 255, 255, 128 },
                                 { 255, 255, 255, 146 } };
    GFX_TEXTURE *const texture = M_Texture(2, 1, texels);
    Gfx_BeginTick(BLACK);
    const GFX_STATE tested = { .depth_test = GFX_DEPTH_LESS_EQUAL,
                               .depth_write = true,
                               .alpha_test = true };
    M_Rect(
        &tested, texture, 0, 0, SIZE, SIZE, 0.2f,
        (GFX_COLOR) { 255, 255, 255, 7 }, 1.0f);
    const GFX_STATE behind = { .depth_test = GFX_DEPTH_LESS_EQUAL,
                               .depth_write = true };
    M_Rect(&behind, nullptr, 0, 0, SIZE, SIZE, 0.6f, RED, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 1, 1, RED, 0);
    M_CheckPixel(&image, 6, 1, WHITE, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(quads_interpolate_across_the_strip_diagonal)
{
    REQUIRE_GL();
    const GFX_COLOR yellow = { 255, 255, 0, 255 };
    const GFX_VERTEX vertices[4] = {
        { { 0, 0, 0.5f }, RED, {} },
        { { SIZE, 0, 0.5f }, GREEN, {} },
        { { SIZE, SIZE, 0.5f }, BLUE, {} },
        { { 0, SIZE, 0.5f }, yellow, {} },
    };
    Gfx_BeginTick(BLACK);
    Gfx_DrawQuad(&m_Opaque, nullptr, vertices, 0);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 3, 3, (GFX_COLOR) { 143, 223, 0, 255 }, 1);
    M_CheckPixel(&image, 4, 4, (GFX_COLOR) { 112, 223, 32, 255 }, 1);
    Image_Free(&image);
}

TEST(indexed_uploads_preserve_rows_and_palette_indices)
{
    REQUIRE_GL();
    GFX_TEXTURE *texture = nullptr;
    CHECK_OK(GfxTexture_CreateIndexed(3, 2, &texture));
    const uint8_t indices[] = { 0, 15, 255, 255, 15, 0 };
    GfxTexture_UploadIndices(texture, (GFX_RECT) { 0, 0, 3, 2 }, indices);
    GfxTexture_UploadPalette(texture, 0, 1, &RED);
    GfxTexture_UploadPalette(texture, 15, 1, &GREEN);
    GfxTexture_UploadPalette(texture, 255, 1, &BLUE);
    Gfx_BeginTick(BLACK);
    M_Fill(&m_Opaque, texture, WHITE);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, RED, 0);
    M_CheckPixel(&image, 3, 0, GREEN, 0);
    M_CheckPixel(&image, 7, 0, BLUE, 0);
    M_CheckPixel(&image, 0, 7, BLUE, 0);
    M_CheckPixel(&image, 3, 7, GREEN, 0);
    M_CheckPixel(&image, 7, 7, RED, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(index_and_palette_updates_preserve_draw_order)
{
    REQUIRE_GL();
    GFX_TEXTURE *texture = nullptr;
    CHECK_OK(GfxTexture_CreateIndexed(1, 1, &texture));
    const GFX_COLOR palette[] = { RED, GREEN };
    GfxTexture_UploadPalette(texture, 0, 2, palette);
    Gfx_BeginTick(BLACK);
    M_Rect(&m_Opaque, texture, 0, 0, 2, SIZE, 0.5f, WHITE, 1.0f);
    GfxTexture_UploadPalette(texture, 0, 1, &BLUE);
    M_Rect(&m_Opaque, texture, 2, 0, 4, SIZE, 0.5f, WHITE, 1.0f);
    const uint8_t index = 1;
    GfxTexture_UploadIndices(texture, (GFX_RECT) { 0, 0, 1, 1 }, &index);
    M_Rect(&m_Opaque, texture, 4, 0, 6, SIZE, 0.5f, WHITE, 1.0f);
    GFX_TEXTURE *const rgba = M_Texture(1, 1, &WHITE);
    M_Rect(&m_Opaque, rgba, 6, 0, 8, SIZE, 0.5f, WHITE, 1.0f);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 0, RED, 0);
    M_CheckPixel(&image, 2, 0, BLUE, 0);
    M_CheckPixel(&image, 4, 0, GREEN, 0);
    M_CheckPixel(&image, 6, 0, WHITE, 0);
    Image_Free(&image);
    GfxTexture_Free(rgba);
    GfxTexture_Free(texture);
}

TEST(indexed_textures_use_palette_alpha_and_nearest_sampling)
{
    REQUIRE_GL();
    GFX_TEXTURE *texture = nullptr;
    CHECK_OK(GfxTexture_CreateIndexed(2, 1, &texture));
    const uint8_t indices[] = { 0, 1 };
    const GFX_COLOR palette[] = { { 255, 0, 0, 0 }, GREEN };
    GfxTexture_UploadIndices(texture, (GFX_RECT) { 0, 0, 2, 1 }, indices);
    GfxTexture_UploadPalette(texture, 0, 2, palette);
    const GFX_STATE state = {
        .filter = GFX_FILTER_LINEAR,
        .alpha_test = true,
    };
    Gfx_BeginTick(BLUE);
    M_Fill(&state, texture, WHITE);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 3, 0, BLUE, 0);
    M_CheckPixel(&image, 4, 0, GREEN, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}

TEST(indexed_partial_upload_and_palette_only_tick)
{
    REQUIRE_GL();
    GFX_TEXTURE *texture = nullptr;
    CHECK_OK(GfxTexture_CreateIndexed(3, 2, &texture));
    const GFX_COLOR palette[] = { RED, GREEN };
    GfxTexture_UploadPalette(texture, 0, 2, palette);
    const uint8_t indices[] = { 1, 1 };
    GfxTexture_UploadIndices(texture, (GFX_RECT) { 1, 0, 1, 2 }, indices);
    Gfx_BeginTick(BLACK);
    M_Fill(&m_Opaque, texture, WHITE);
    IMAGE image = M_Screen();
    M_CheckPixel(&image, 0, 7, RED, 0);
    M_CheckPixel(&image, 3, 7, GREEN, 0);
    M_CheckPixel(&image, 7, 7, RED, 0);
    Image_Free(&image);
    GfxTexture_UploadPalette(texture, 1, 1, &BLUE);
    Gfx_BeginTick(BLACK);
    M_Fill(&m_Opaque, texture, WHITE);
    image = M_Screen();
    M_CheckPixel(&image, 0, 7, RED, 0);
    M_CheckPixel(&image, 3, 7, BLUE, 0);
    M_CheckPixel(&image, 7, 7, RED, 0);
    Image_Free(&image);
    GfxTexture_Free(texture);
}
