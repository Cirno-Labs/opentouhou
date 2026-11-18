#include "core/gfx/private.h"

#include "core/base/memory.h"
#include "core/math/vec.h"

#include "test.h"

#include <math.h>
#include <string.h>

#define MAX_EVENTS 32
#define MAX_VERTICES 64
#define MAX_BATCHES 16
#define MAX_VIEWS 8
#define WIDTH 64
#define HEIGHT 48

typedef enum {
    EVENT_CREATE,
    EVENT_UPLOAD,
    EVENT_COPY,
    EVENT_FREE,
    EVENT_BEGIN_SCREEN,
    EVENT_BEGIN_WINDOW,
    EVENT_DRAW,
    EVENT_CLEAR,
    EVENT_END,
    EVENT_BLIT,
    EVENT_READ,
} EVENT;

typedef struct {
    EVENT events[MAX_EVENTS];
    int32_t batch_counts[MAX_EVENTS];
    int32_t count;
    GFX_VERTEX vertices[MAX_VERTICES];
    int32_t vertex_count;
    GFX_BATCH batches[MAX_BATCHES];
    int32_t batch_count;
    GFX_PASS_VIEW views[MAX_VIEWS];
    GFX_PASS_OUTPUT output;
    uintptr_t next_handle;
    uint32_t clear_flags;
    GFX_COLOR clear_color;
    GFX_PASS_VIEW clear_view;
    // Makes a screen read return WIDTH x HEIGHT pixels whose red and green
    // are their x and y.
    bool readable;
} RECORDING;

static RECORDING m_Recording;

static void M_Record(const EVENT event, const int32_t batches)
{
    if (m_Recording.count < MAX_EVENTS) {
        m_Recording.batch_counts[m_Recording.count] = batches;
        m_Recording.events[m_Recording.count++] = event;
    }
}

static RESULT M_Init(
    SDL_Window *const window, const int32_t width, const int32_t height)
{
    return OK;
}

static void M_Shutdown(void)
{
}

static RESULT M_CreateTexture(
    const int32_t width, const int32_t height, const uint8_t *const pixels,
    void **const out)
{
    M_Record(EVENT_CREATE, 0);
    *out = (void *)++m_Recording.next_handle;
    return OK;
}

static void M_UploadTexture(
    void *const handle, const GFX_RECT rect, const IMAGE *const image)
{
    M_Record(EVENT_UPLOAD, 0);
}

static void M_CopyFromScreen(
    void *const handle, const GFX_RECT source, const int32_t x, const int32_t y)
{
    M_Record(EVENT_COPY, 0);
}

static void M_FreeTexture(void *const handle)
{
    M_Record(EVENT_FREE, 0);
}

static void M_BeginPass(
    const GFX_PASS pass, const GFX_COLOR clear,
    const GFX_PASS_OUTPUT *const output, const bool preserve)
{
    M_Record(
        pass == GFX_PASS_SCREEN ? EVENT_BEGIN_SCREEN : EVENT_BEGIN_WINDOW, 0);
    m_Recording.output = *output;
}

static void M_Draw(
    const GFX_PASS_VIEW *const views, const GFX_BATCH *const batches,
    const int32_t batch_count, const GFX_VERTEX *const vertices)
{
    M_Record(EVENT_DRAW, batch_count);
    const GFX_BATCH *const last = &batches[batch_count - 1];
    m_Recording.vertex_count = last->first + last->count;
    memcpy(
        m_Recording.vertices, vertices,
        sizeof(GFX_VERTEX) * (size_t)m_Recording.vertex_count);
    m_Recording.batch_count = batch_count;
    memcpy(
        m_Recording.batches, batches, sizeof(GFX_BATCH) * (size_t)batch_count);
    int32_t view_count = 0;
    for (int32_t i = 0; i < batch_count; i++) {
        view_count =
            batches[i].view + 1 > view_count ? batches[i].view + 1 : view_count;
    }
    memcpy(
        m_Recording.views, views, sizeof(GFX_PASS_VIEW) * (size_t)view_count);
}

static void M_Clear(
    const uint32_t flags, const GFX_COLOR color,
    const GFX_PASS_VIEW *const view)
{
    M_Record(EVENT_CLEAR, 0);
    m_Recording.clear_flags = flags;
    m_Recording.clear_color = color;
    m_Recording.clear_view = *view;
}

static void M_EndPass(void)
{
    M_Record(EVENT_END, 0);
}

static void M_BlitScreen(const GFX_PASS_OUTPUT *const output)
{
    M_Record(EVENT_BLIT, 0);
    m_Recording.output = *output;
}

static RESULT M_ReadPixels(
    const GFX_PASS pass, const ALLOCATOR *const allocator, IMAGE *const out)
{
    M_Record(EVENT_READ, 0);
    FAIL_IF(!m_Recording.readable || pass != GFX_PASS_SCREEN, "not recorded");
    *out = Image_Create(WIDTH, HEIGHT, allocator);
    for (int32_t y = 0; y < HEIGHT; y++) {
        for (int32_t x = 0; x < WIDTH; x++) {
            uint8_t *const pixel = &out->pixels[(y * WIDTH + x) * 4];
            pixel[0] = (uint8_t)x;
            pixel[1] = (uint8_t)y;
            pixel[3] = 255;
        }
    }
    return OK;
}

static const GFX_BACKEND m_RecordingBackend = {
    .init = M_Init,
    .shutdown = M_Shutdown,
    .create_texture = M_CreateTexture,
    .upload_texture = M_UploadTexture,
    .copy_from_screen = M_CopyFromScreen,
    .free_texture = M_FreeTexture,
    .begin_pass = M_BeginPass,
    .draw = M_Draw,
    .clear = M_Clear,
    .end_pass = M_EndPass,
    .blit_screen = M_BlitScreen,
    .read_pixels = M_ReadPixels,
};

static const GFX_STATE m_Alpha = { .blend = GFX_BLEND_ALPHA };
static const GFX_STATE m_Add = { .blend = GFX_BLEND_ADD };
static const GFX_OUTPUT m_Output = { .width = WIDTH, .height = HEIGHT };

static void M_Start(void)
{
    memset(&m_Recording, 0, sizeof(m_Recording));
    CHECK(Result_Log(
        GfxFrontend_Start(&m_RecordingBackend, nullptr, WIDTH, HEIGHT),
        LOG_LEVEL_ERROR));
}

static void M_Stop(void)
{
    GfxFrontend_Stop();
}

static void M_ClearEvents(void)
{
    m_Recording.count = 0;
}

static void M_CheckEvents(const EVENT *const expected, const int32_t count)
{
    CHECK_EQ_INT(m_Recording.count, count);
    for (int32_t i = 0; i < count && i < m_Recording.count; i++) {
        if (m_Recording.events[i] != expected[i]) {
            TEST_FAIL(
                "event %d: expected %d, got %d", i, expected[i],
                m_Recording.events[i]);
        }
    }
}

static void M_Quad(
    GFX_VERTEX vertices[4], const float x, const float y, const float size)
{
    const VEC2 corners[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    for (int32_t i = 0; i < 4; i++) {
        vertices[i] = (GFX_VERTEX) {
            .position = { x + corners[i].x * size, y + corners[i].y * size,
                          0.5f },
            .color = { 255, 255, 255, 255 },
            .uv = corners[i],
        };
    }
}

static void M_Draw4(
    const GFX_STATE *const state, GFX_TEXTURE *const texture, const float x,
    const uint64_t id)
{
    GFX_VERTEX vertices[4];
    M_Quad(vertices, x, 0.0f, 4.0f);
    Gfx_DrawQuad(state, texture, vertices, id);
}

static void M_Present(const float t)
{
    M_ClearEvents();
    CHECK(Result_Log(Gfx_Present(true, t, &m_Output), LOG_LEVEL_ERROR));
}

static void M_CheckNear(const float actual, const float expected)
{
    if (fabsf(actual - expected) > 0.0001f) {
        TEST_FAIL("expected %.6f, got %.6f", expected, actual);
    }
}

TEST(headless_textures_keep_their_size_and_draws_do_nothing)
{
    memset(&m_Recording, 0, sizeof(m_Recording));
    GFX_TEXTURE *texture = nullptr;
    CHECK(Result_Log(GfxTexture_CreateEmpty(16, 8, &texture), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(GfxTexture_GetWidth(texture), 16);
    CHECK_EQ_INT(GfxTexture_GetHeight(texture), 8);
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, texture, 0.0f, 1);
    Gfx_EndTick();
    CHECK(Result_Log(Gfx_Present(true, 0.5f, &m_Output), LOG_LEVEL_ERROR));
    IMAGE image;
    CHECK(!Result_Ignore(Gfx_ReadScreen(&g_HeapAllocator, &image)));
    GfxTexture_Free(texture);
    CHECK_EQ_INT(m_Recording.count, 0);
}

TEST(the_renderer_is_started_between_start_and_stop)
{
    CHECK(!Gfx_IsStarted());
    M_Start();
    CHECK(Gfx_IsStarted());
    M_Stop();
    CHECK(!Gfx_IsStarted());
}

TEST(state_texture_and_view_changes_split_batches)
{
    M_Start();
    GFX_TEXTURE *a = nullptr;
    GFX_TEXTURE *b = nullptr;
    CHECK(Result_Log(GfxTexture_CreateEmpty(4, 4, &a), LOG_LEVEL_ERROR));
    CHECK(Result_Log(GfxTexture_CreateEmpty(4, 4, &b), LOG_LEVEL_ERROR));
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, a, 0.0f, 0);
    M_Draw4(&m_Alpha, a, 4.0f, 0);
    M_Draw4(&m_Add, a, 8.0f, 0);
    M_Draw4(&m_Add, b, 12.0f, 0);
    GFX_VIEW view = GfxView_Screen();
    view.viewport.width = 32.0f;
    Gfx_SetView(&view);
    M_Draw4(&m_Add, b, 16.0f, 0);
    Gfx_SetView(&view);
    M_Draw4(&m_Add, b, 20.0f, 0);
    Gfx_EndTick();
    CHECK_EQ_INT(m_Recording.batch_count, 4);
    CHECK_EQ_INT(m_Recording.batches[0].count, 8);
    CHECK_EQ_INT(m_Recording.batches[1].count, 4);
    CHECK(m_Recording.batches[2].texture == b);
    CHECK_EQ_INT(m_Recording.batches[2].view, 0);
    CHECK_EQ_INT(m_Recording.batches[3].view, 1);
    CHECK_EQ_INT(m_Recording.batches[3].first, 16);
    CHECK_EQ_INT(m_Recording.batches[3].count, 8);
    GfxTexture_Free(a);
    GfxTexture_Free(b);
    M_Stop();
}

TEST(a_clear_draws_in_order_with_the_quads_around_it)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 1);
    M_ClearEvents();
    Gfx_Clear(GFX_CLEAR_COLOR, (GFX_COLOR) { 1, 2, 3, 4 });
    M_Draw4(&m_Alpha, nullptr, 4.0f, 2);
    Gfx_EndTick();
    const EVENT expected[] = { EVENT_DRAW, EVENT_CLEAR, EVENT_DRAW, EVENT_END };
    M_CheckEvents(expected, 4);
    M_Stop();
}

TEST(clear_flags_and_colour_reach_the_backend)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Gfx_Clear(GFX_CLEAR_DEPTH, (GFX_COLOR) { 10, 20, 30, 40 });
    Gfx_EndTick();
    CHECK_EQ_INT(m_Recording.clear_flags, GFX_CLEAR_DEPTH);
    CHECK_EQ_INT(m_Recording.clear_color.red, 10);
    CHECK_EQ_INT(m_Recording.clear_color.green, 20);
    CHECK_EQ_INT(m_Recording.clear_color.blue, 30);
    CHECK_EQ_INT(m_Recording.clear_color.alpha, 40);
    M_Stop();
}

TEST(a_clear_scissors_to_the_current_view)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    GFX_VIEW view = GfxView_Screen();
    view.viewport = (VIEWPORT) { 4.0f, 8.0f, 16.0f, 12.0f, 0.0f, 1.0f };
    Gfx_SetView(&view);
    Gfx_Clear(GFX_CLEAR_COLOR | GFX_CLEAR_DEPTH, (GFX_COLOR) {});
    Gfx_EndTick();
    CHECK(m_Recording.clear_view.scissor_x == 4.0f);
    CHECK(m_Recording.clear_view.scissor_y == 8.0f);
    CHECK(m_Recording.clear_view.scissor_width == 16.0f);
    CHECK(m_Recording.clear_view.scissor_height == 12.0f);
    M_Stop();
}

TEST(a_clear_between_quads_blends_into_the_present)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 1);
    Gfx_Clear(GFX_CLEAR_COLOR, (GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 4.0f, 2);
    Gfx_EndTick();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 1);
    Gfx_Clear(GFX_CLEAR_COLOR, (GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 4.0f, 2);
    Gfx_EndTick();
    M_Present(0.5f);
    const EVENT expected[] = { EVENT_BEGIN_WINDOW, EVENT_DRAW, EVENT_CLEAR,
                               EVENT_DRAW, EVENT_END };
    M_CheckEvents(expected, 5);
    M_Stop();
}

TEST(uploads_and_copies_follow_the_batches_drawn_before_them)
{
    M_Start();
    GFX_TEXTURE *texture = nullptr;
    CHECK(Result_Log(GfxTexture_CreateEmpty(4, 4, &texture), LOG_LEVEL_ERROR));
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    M_ClearEvents();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, texture, 0.0f, 0);
    GfxTexture_Upload(texture, (GFX_RECT) { 0, 0, 4, 4 }, &image);
    M_Draw4(&m_Alpha, texture, 4.0f, 0);
    M_Draw4(&m_Alpha, texture, 8.0f, 0);
    GfxTexture_CopyFromScreen(texture, (GFX_RECT) { 0, 0, 4, 4 }, 0, 0);
    GfxTexture_CopyFromScreen(texture, (GFX_RECT) { 0, 0, 4, 4 }, 0, 0);
    M_Draw4(&m_Alpha, texture, 12.0f, 0);
    Gfx_EndTick();
    const EVENT expected[] = {
        EVENT_BEGIN_SCREEN, EVENT_DRAW, EVENT_UPLOAD, EVENT_DRAW,
        EVENT_COPY,         EVENT_COPY, EVENT_DRAW,   EVENT_END,
    };
    M_CheckEvents(expected, ARRAY_SIZE(expected));
    CHECK_EQ_INT(m_Recording.batch_counts[3], 1);
    CHECK_EQ_INT(m_Recording.batches[0].first, 12);
    Image_Free(&image);
    GfxTexture_Free(texture);
    M_Stop();
}

TEST(a_copy_outside_the_screen_is_cut_or_skipped)
{
    M_Start();
    GFX_TEXTURE *texture = nullptr;
    CHECK(Result_Log(GfxTexture_CreateEmpty(4, 4, &texture), LOG_LEVEL_ERROR));
    M_ClearEvents();
    GfxTexture_CopyFromScreen(texture, (GFX_RECT) { WIDTH, 0, 4, 4 }, 0, 0);
    GfxTexture_CopyFromScreen(texture, (GFX_RECT) { 0, 0, 4, 4 }, 4, 0);
    CHECK_EQ_INT(m_Recording.count, 0);
    GfxTexture_CopyFromScreen(texture, (GFX_RECT) { -2, 0, 4, 4 }, 0, 0);
    CHECK_EQ_INT(m_Recording.count, 1);
    GfxTexture_Free(texture);
    M_Stop();
}

TEST(a_freed_texture_stays_until_the_next_tick)
{
    M_Start();
    GFX_TEXTURE *texture = nullptr;
    CHECK(Result_Log(GfxTexture_CreateEmpty(4, 4, &texture), LOG_LEVEL_ERROR));
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, texture, 0.0f, 0);
    GfxTexture_Free(texture);
    Gfx_EndTick();
    M_Present(1.0f);
    const EVENT present[] = { EVENT_BEGIN_WINDOW, EVENT_DRAW, EVENT_END };
    M_CheckEvents(present, ARRAY_SIZE(present));
    M_ClearEvents();
    Gfx_BeginTick((GFX_COLOR) {});
    const EVENT begin[] = { EVENT_FREE, EVENT_BEGIN_SCREEN };
    M_CheckEvents(begin, ARRAY_SIZE(begin));
    Gfx_EndTick();
    M_Stop();
}

TEST(present_without_interpolation_blits_the_screen)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 1);
    Gfx_EndTick();
    M_ClearEvents();
    CHECK(Result_Log(Gfx_Present(false, 0.5f, &m_Output), LOG_LEVEL_ERROR));
    const EVENT expected[] = { EVENT_BLIT };
    M_CheckEvents(expected, ARRAY_SIZE(expected));
    M_Stop();
}

TEST(matched_quads_blend_their_positions)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 7);
    Gfx_EndTick();
    Gfx_BeginTick((GFX_COLOR) {});
    GFX_VERTEX vertices[4];
    M_Quad(vertices, 8.0f, 4.0f, 4.0f);
    vertices[0].color = (GFX_COLOR) { 1, 2, 3, 4 };
    Gfx_DrawQuad(&m_Alpha, nullptr, vertices, 7);
    Gfx_EndTick();
    M_Present(0.25f);
    M_CheckNear(m_Recording.vertices[0].position.x, 2.0f);
    M_CheckNear(m_Recording.vertices[0].position.y, 1.0f);
    M_CheckNear(m_Recording.vertices[2].position.x, 6.0f);
    M_CheckNear(m_Recording.vertices[2].position.y, 5.0f);
    CHECK_EQ_INT(m_Recording.vertices[0].color.alpha, 4);
    M_Present(1.0f);
    M_CheckNear(m_Recording.vertices[0].position.x, 8.0f);
    M_Stop();
}

TEST(unmatched_and_zero_ids_keep_their_positions)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 1);
    Gfx_EndTick();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 8.0f, 0);
    M_Draw4(&m_Alpha, nullptr, 8.0f, 2);
    Gfx_EndTick();
    M_Present(0.0f);
    M_CheckNear(m_Recording.vertices[0].position.x, 8.0f);
    M_CheckNear(m_Recording.vertices[4].position.x, 8.0f);
    M_Stop();
}

static void M_DrawSprite(const GFX_MOTION *const motion, const uint64_t id)
{
    GFX_VERTEX vertices[4];
    const VEC2 corners[4] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    const float sine = sinf(motion->rotation);
    const float cosine = cosf(motion->rotation);
    for (int32_t i = 0; i < 4; i++) {
        const float x = corners[i].x * motion->scale.x;
        const float y = corners[i].y * motion->scale.y;
        vertices[i] = (GFX_VERTEX) {
            .position = { motion->center.x + x * cosine + y * sine,
                          motion->center.y - x * sine + y * cosine, 0.5f },
        };
    }
    Gfx_DrawSprite(&m_Alpha, nullptr, vertices, motion, id);
}

TEST(sprites_blend_center_scale_and_the_shorter_rotation)
{
    M_Start();
    const GFX_MOTION from = { { 10, 10 }, { 2, 2 }, 3.0f };
    const GFX_MOTION to = { { 20, 30 }, { 4, 4 }, -3.0f };
    Gfx_BeginTick((GFX_COLOR) {});
    M_DrawSprite(&from, 5);
    Gfx_EndTick();
    Gfx_BeginTick((GFX_COLOR) {});
    M_DrawSprite(&to, 5);
    Gfx_EndTick();
    M_Present(1.0f);
    GFX_VERTEX exact[4];
    memcpy(exact, m_Recording.vertices, sizeof(exact));
    M_Present(1.0f);
    for (int32_t i = 0; i < 4; i++) {
        CHECK(exact[i].position.x == m_Recording.vertices[i].position.x);
        CHECK(exact[i].position.y == m_Recording.vertices[i].position.y);
    }
    M_Present(0.5f);
    GFX_VERTEX blended[4];
    memcpy(blended, m_Recording.vertices, sizeof(blended));
    const float turn = 2.0f * 3.14159265f - 6.0f;
    const GFX_MOTION half = { { 15, 20 }, { 3, 3 }, 3.0f + turn / 2.0f };
    Gfx_BeginTick((GFX_COLOR) {});
    M_DrawSprite(&half, 9);
    Gfx_EndTick();
    M_Present(1.0f);
    for (int32_t i = 0; i < 4; i++) {
        M_CheckNear(blended[i].position.x, m_Recording.vertices[i].position.x);
        M_CheckNear(blended[i].position.y, m_Recording.vertices[i].position.y);
    }
    M_Stop();
}

TEST(sprites_without_a_previous_motion_blend_their_vertices)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 3);
    Gfx_EndTick();
    const GFX_MOTION motion = { { 10, 2 }, { 2, 2 }, 0.0f };
    Gfx_BeginTick((GFX_COLOR) {});
    M_DrawSprite(&motion, 3);
    Gfx_EndTick();
    M_Present(0.5f);
    M_CheckNear(m_Recording.vertices[0].position.x, 4.0f);
    M_CheckNear(m_Recording.vertices[0].position.y, 0.0f);
    M_Stop();
}

TEST(views_with_matching_ids_blend_and_others_stay)
{
    M_Start();
    GFX_VIEW a = GfxView_Screen();
    a.id = 1;
    a.viewport.x = 0.0f;
    a.view.m[3][0] = 0.0f;
    a.fog.begin = 10.0f;
    GFX_VIEW other = GfxView_Screen();
    other.id = 2;
    other.viewport.x = 4.0f;
    Gfx_BeginTick((GFX_COLOR) {});
    Gfx_SetView(&a);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_EndTick();
    a.viewport.x = 8.0f;
    a.view.m[3][0] = 4.0f;
    a.fog.begin = 20.0f;
    Gfx_BeginTick((GFX_COLOR) {});
    Gfx_SetView(&a);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_SetView(&other);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_EndTick();
    M_Present(0.5f);
    const GFX_PASS_VIEW *const blended = &m_Recording.views[1];
    M_CheckNear(blended->scissor_x, 4.0f);
    M_CheckNear(blended->view.m[3][0], 2.0f);
    M_CheckNear(blended->fog.begin, 15.0f);
    M_CheckNear(m_Recording.views[2].scissor_x, 4.0f);
    M_Stop();
}

TEST(a_view_remaps_its_projection_to_the_viewport)
{
    M_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_EndTick();
    const MAT4 *const projection = &m_Recording.views[0].projection;
    for (int32_t row = 0; row < 4; row++) {
        for (int32_t col = 0; col < 4; col++) {
            M_CheckNear(projection->m[row][col], row == col ? 1.0f : 0.0f);
        }
    }
    M_Stop();
}

TEST(the_output_letterboxes_and_scales_by_whole_multiples)
{
    const GFX_OUTPUT wide = { .width = 200, .height = 96 };
    GFX_PASS_OUTPUT fit = GfxOutput_Fit(&wide, WIDTH, HEIGHT);
    CHECK_EQ_INT(fit.rect.x, 36);
    CHECK_EQ_INT(fit.rect.y, 0);
    CHECK_EQ_INT(fit.rect.width, 128);
    CHECK_EQ_INT(fit.rect.height, 96);
    M_CheckNear(fit.scale_x, 2.0f);
    M_CheckNear(fit.offset_x, 36.0f);
    const GFX_OUTPUT odd = { .width = 160,
                             .height = 120,
                             .integer_scale = true };
    fit = GfxOutput_Fit(&odd, WIDTH, HEIGHT);
    CHECK_EQ_INT(fit.rect.width, 128);
    CHECK_EQ_INT(fit.rect.x, 16);
    CHECK_EQ_INT(fit.rect.y, 12);
}

TEST(a_repeated_view_id_blends_with_the_same_repetition)
{
    M_Start();
    GFX_VIEW first = GfxView_Screen();
    first.id = 3;
    first.viewport.x = 0.0f;
    GFX_VIEW second = first;
    second.viewport.x = 10.0f;
    Gfx_BeginTick((GFX_COLOR) {});
    Gfx_SetView(&first);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_SetView(&second);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_EndTick();
    first.viewport.x = 2.0f;
    second.viewport.x = 20.0f;
    Gfx_BeginTick((GFX_COLOR) {});
    Gfx_SetView(&first);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_SetView(&second);
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    Gfx_EndTick();
    M_Present(0.5f);
    M_CheckNear(m_Recording.views[1].scissor_x, 1.0f);
    M_CheckNear(m_Recording.views[2].scissor_x, 15.0f);
    M_Stop();
}

TEST(a_screen_rect_reads_after_the_quads_drawn_so_far)
{
    M_Start();
    m_Recording.readable = true;
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0.0f, 0);
    IMAGE image;
    CHECK(Result_Log(
        Gfx_ReadScreenRect((GFX_RECT) { 5, 7, 3, 2 }, &g_HeapAllocator, &image),
        LOG_LEVEL_ERROR));
    M_Draw4(&m_Alpha, nullptr, 8.0f, 0);
    Gfx_EndTick();
    M_CheckEvents(
        (EVENT[]) { EVENT_BEGIN_SCREEN, EVENT_DRAW, EVENT_READ, EVENT_DRAW,
                    EVENT_END },
        5);
    CHECK_EQ_INT(image.width, 3);
    CHECK_EQ_INT(image.height, 2);
    for (int32_t y = 0; y < 2; y++) {
        for (int32_t x = 0; x < 3; x++) {
            const uint8_t *const pixel = &image.pixels[(y * 3 + x) * 4];
            CHECK_EQ_INT(pixel[0], 5 + x);
            CHECK_EQ_INT(pixel[1], 7 + y);
        }
    }
    Image_Free(&image);
    M_Stop();
}

TEST(a_screen_rect_outside_the_screen_fails)
{
    M_Start();
    m_Recording.readable = true;
    const GFX_RECT outside[] = {
        { -1, 0, 4, 4 },         { 0, -1, 4, 4 }, { WIDTH - 3, 0, 4, 4 },
        { 0, HEIGHT - 3, 4, 4 }, { 0, 0, 0, 4 },  { 0, 0, 4, 0 },
    };
    for (size_t i = 0; i < sizeof(outside) / sizeof(outside[0]); i++) {
        IMAGE image;
        CHECK(!Result_Log(
            Gfx_ReadScreenRect(outside[i], &g_HeapAllocator, &image),
            LOG_LEVEL_INFO));
        CHECK(image.pixels == nullptr);
    }
    CHECK_EQ_INT(m_Recording.count, 0);
    IMAGE whole;
    CHECK(Result_Log(
        Gfx_ReadScreenRect(
            (GFX_RECT) { 0, 0, WIDTH, HEIGHT }, &g_HeapAllocator, &whole),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(whole.width, WIDTH);
    Image_Free(&whole);
    M_Stop();
}

TEST(a_screen_rect_fails_while_the_renderer_is_stopped)
{
    IMAGE image;
    CHECK(!Result_Log(
        Gfx_ReadScreenRect((GFX_RECT) { 0, 0, 1, 1 }, &g_HeapAllocator, &image),
        LOG_LEVEL_INFO));
    CHECK(image.pixels == nullptr);
}

TEST(a_failed_screen_read_fails_the_screen_rect)
{
    M_Start();
    IMAGE image;
    CHECK(!Result_Log(
        Gfx_ReadScreenRect((GFX_RECT) { 0, 0, 1, 1 }, &g_HeapAllocator, &image),
        LOG_LEVEL_INFO));
    CHECK(image.pixels == nullptr);
    M_Stop();
}

TEST(a_projection_pixel_offset_keeps_the_viewport_and_depth)
{
    const GFX_VIEW view = {
        .id = 17,
        .view = Mat4_Identity(),
        .projection = Mat4_PerspectiveFovLH(0.5f, 0.75f, 30.0f, 1800.0f),
        .viewport = { 32, 16, 384, 448, 0, 1 },
    };
    const GFX_VIEW shifted = GfxView_OffsetPixels(&view, 0.5f, 0.5f);
    const MAT4 world = Mat4_Identity();
    const VEC3 points[] = { { 3, 4, 100 }, { 3, 4, 500 } };
    for (size_t i = 0; i < 2; i++) {
        const VEC3 before = Vec3_Project(
            points[i], &view.viewport, &view.projection, &view.view, &world);
        const VEC3 after = Vec3_Project(
            points[i], &shifted.viewport, &shifted.projection, &shifted.view,
            &world);
        CHECK(fabsf(after.x - before.x - 0.5f) < 0.0001f);
        CHECK(fabsf(after.y - before.y - 0.5f) < 0.0001f);
        CHECK(after.z == before.z);
    }
    CHECK_EQ_INT(shifted.id, view.id);
    CHECK(
        memcmp(&shifted.viewport, &view.viewport, sizeof(view.viewport)) == 0);
    CHECK(view.projection.m[2][0] == 0.0f);
}

TEST(deferred_full_screen_replacements_discard_only_undrawn_ticks)
{
    M_Start();
    Gfx_SetDeferredTicks(true);
    Gfx_SetPersistentScreen(true);
    const GFX_STATE state = {};
    const GFX_VERTEX vertices[4] = {
        { .position = { 0, 0, 0 } },
        { .position = { WIDTH, 0, 0 } },
        { .position = { WIDTH, HEIGHT, 0 } },
        { .position = { 0, HEIGHT, 0 } },
    };
    for (int32_t i = 0; i < 3; i++) {
        Gfx_BeginTick((GFX_COLOR) {});
        Gfx_DrawQuad(&state, nullptr, vertices, 0);
        Gfx_EndTick();
    }
    CHECK_EQ_INT(m_Recording.count, 0);
    CHECK(IS_OK(Gfx_Present(false, 1, &m_Output)));
    const EVENT expected[] = {
        EVENT_BEGIN_SCREEN,
        EVENT_DRAW,
        EVENT_END,
        EVENT_BLIT,
    };
    M_CheckEvents(expected, ARRAY_SIZE(expected));
    M_Stop();
}

TEST(deferred_blending_keeps_the_previous_frame)
{
    M_Start();
    Gfx_SetDeferredTicks(true);
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0, 0);
    Gfx_EndTick();
    CHECK_EQ_INT(m_Recording.count, 0);
    Gfx_SetPersistentScreen(true);
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 4, 0);
    Gfx_EndTick();
    CHECK_EQ_INT(m_Recording.count, 3);
    CHECK(IS_OK(Gfx_Present(false, 1, &m_Output)));
    const EVENT expected[] = {
        EVENT_BEGIN_SCREEN, EVENT_DRAW, EVENT_END,  EVENT_BEGIN_SCREEN,
        EVENT_DRAW,         EVENT_END,  EVENT_BLIT,
    };
    M_CheckEvents(expected, ARRAY_SIZE(expected));
    M_Stop();
}

TEST(deferred_texture_uploads_flush_the_previous_contents)
{
    M_Start();
    Gfx_SetDeferredTicks(true);
    GFX_TEXTURE *texture = nullptr;
    CHECK(IS_OK(GfxTexture_CreateEmpty(4, 4, &texture)));
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    M_ClearEvents();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, texture, 0, 0);
    Gfx_EndTick();
    GfxTexture_Upload(texture, (GFX_RECT) { 0, 0, 4, 4 }, &image);
    const EVENT expected[] = {
        EVENT_BEGIN_SCREEN,
        EVENT_DRAW,
        EVENT_END,
        EVENT_UPLOAD,
    };
    M_CheckEvents(expected, ARRAY_SIZE(expected));
    Image_Free(&image);
    GfxTexture_Free(texture);
    M_Stop();
}

TEST(deferred_copies_flush_the_screen_before_the_copy)
{
    M_Start();
    Gfx_SetDeferredTicks(true);
    GFX_TEXTURE *texture = nullptr;
    CHECK(IS_OK(GfxTexture_CreateEmpty(4, 4, &texture)));
    M_ClearEvents();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0, 0);
    Gfx_EndTick();
    GfxTexture_CopyFromScreen(texture, (GFX_RECT) { 0, 0, 4, 4 }, 0, 0);
    const EVENT expected[] = {
        EVENT_BEGIN_SCREEN,
        EVENT_DRAW,
        EVENT_END,
        EVENT_COPY,
    };
    M_CheckEvents(expected, ARRAY_SIZE(expected));
    GfxTexture_Free(texture);
    M_Stop();
}

TEST(deferred_full_screen_draws_keep_history_when_fragments_can_be_rejected)
{
    const GFX_STATE states[] = {
        { .alpha_test = true },
        { .depth_test = GFX_DEPTH_LESS_EQUAL },
        { .blend = GFX_BLEND_ALPHA },
        { .blend = GFX_BLEND_ADD },
    };
    const GFX_VERTEX vertices[4] = {
        { .position = { 0, 0, 0 } },
        { .position = { WIDTH, 0, 0 } },
        { .position = { WIDTH, HEIGHT, 0 } },
        { .position = { 0, HEIGHT, 0 } },
    };
    for (size_t i = 0; i < ARRAY_SIZE(states); i++) {
        M_Start();
        Gfx_SetDeferredTicks(true);
        Gfx_BeginTick((GFX_COLOR) {});
        M_Draw4(&m_Alpha, nullptr, 0, 0);
        Gfx_EndTick();
        Gfx_SetPersistentScreen(true);
        Gfx_BeginTick((GFX_COLOR) {});
        Gfx_DrawQuad(&states[i], nullptr, vertices, 0);
        Gfx_EndTick();
        const EVENT expected[] = {
            EVENT_BEGIN_SCREEN,
            EVENT_DRAW,
            EVENT_END,
        };
        M_CheckEvents(expected, ARRAY_SIZE(expected));
        M_Stop();
    }
}

TEST(deferred_texture_free_flushes_before_releasing_the_texture)
{
    M_Start();
    Gfx_SetDeferredTicks(true);
    GFX_TEXTURE *texture = nullptr;
    CHECK(IS_OK(GfxTexture_CreateEmpty(4, 4, &texture)));
    M_ClearEvents();
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, texture, 0, 0);
    Gfx_EndTick();
    CHECK_EQ_INT(m_Recording.count, 0);
    GfxTexture_Free(texture);
    Gfx_BeginTick((GFX_COLOR) {});
    const EVENT expected[] = {
        EVENT_BEGIN_SCREEN,
        EVENT_DRAW,
        EVENT_END,
        EVENT_FREE,
    };
    M_CheckEvents(expected, ARRAY_SIZE(expected));
    Gfx_EndTick();
    M_Stop();
}

TEST(deferred_ticks_require_a_new_tick_before_interpolation)
{
    M_Start();
    Gfx_SetDeferredTicks(true);
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0, 0);
    Gfx_EndTick();
    CHECK(!Result_Ignore(Gfx_Present(true, 0.5f, &m_Output)));
    Gfx_SetDeferredTicks(false);
    CHECK(!Result_Ignore(Gfx_Present(true, 0.5f, &m_Output)));
    Gfx_BeginTick((GFX_COLOR) {});
    M_Draw4(&m_Alpha, nullptr, 0, 0);
    Gfx_EndTick();
    CHECK(IS_OK(Gfx_Present(true, 0.5f, &m_Output)));
    M_Stop();
}
