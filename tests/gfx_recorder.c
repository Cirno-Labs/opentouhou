#include "gfx_recorder.h"

#include "core/base/array.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include <string.h>

typedef struct {
    uintptr_t handle;
    uint8_t *pixels;
} M_TEXTURE;

typedef ARRAY(GFX_RECORDED_QUAD) M_QUAD_ARRAY;
typedef ARRAY(GFX_VERTEX) M_VERTEX_ARRAY;
typedef ARRAY(M_TEXTURE) M_TEXTURE_ARRAY;

static M_QUAD_ARRAY m_Quads;
static M_VERTEX_ARRAY m_Window;
static M_TEXTURE_ARRAY m_Textures;
static GFX_PASS m_Pass;
static int32_t m_Copies;
static uintptr_t m_NextHandle;

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
    *out = (void *)++m_NextHandle;
    if (pixels != nullptr) {
        const size_t size = (size_t)width * (size_t)height * 4;
        uint8_t *const copy = Mem_Alloc(size);
        memcpy(copy, pixels, size);
        Array_Append(&m_Textures, ((M_TEXTURE) { m_NextHandle, copy }));
    }
    return OK;
}

static void M_UploadTexture(
    void *const handle, const GFX_RECT rect, const IMAGE *const image)
{
}

static void M_CopyFromScreen(
    void *const handle, const GFX_RECT source, const int32_t x, const int32_t y)
{
    m_Copies++;
}

static void M_FreeTexture(void *const handle)
{
}

static void M_BeginPass(
    const GFX_PASS pass, const GFX_COLOR clear,
    const GFX_PASS_OUTPUT *const output)
{
    m_Pass = pass;
    if (pass == GFX_PASS_SCREEN) {
        Array_Clear(&m_Quads);
    } else {
        Array_Clear(&m_Window);
    }
}

static void M_Draw(
    const GFX_PASS_VIEW *const views, const GFX_BATCH *const batches,
    const int32_t batch_count, const GFX_VERTEX *const vertices)
{
    for (int32_t i = 0; i < batch_count; i++) {
        const GFX_BATCH *const batch = &batches[i];
        for (int32_t first = batch->first; first < batch->first + batch->count;
             first += 4) {
            if (m_Pass == GFX_PASS_WINDOW) {
                Array_AppendSlice(&m_Window, &vertices[first], 4);
                continue;
            }
            GFX_RECORDED_QUAD quad = {
                .state = batch->state,
                .texture = batch->texture,
                .view = views[batch->view],
            };
            memcpy(quad.vertices, &vertices[first], sizeof(quad.vertices));
            Array_Append(&m_Quads, quad);
        }
    }
}

static void M_EndPass(void)
{
}

static void M_BlitScreen(const GFX_PASS_OUTPUT *const output)
{
}

static RESULT M_ReadPixels(
    const GFX_PASS pass, const ALLOCATOR *const allocator, IMAGE *const out)
{
    return FAIL("The recorder keeps no pixels");
}

static const GFX_BACKEND m_Backend = {
    .init = M_Init,
    .shutdown = M_Shutdown,
    .create_texture = M_CreateTexture,
    .upload_texture = M_UploadTexture,
    .copy_from_screen = M_CopyFromScreen,
    .free_texture = M_FreeTexture,
    .begin_pass = M_BeginPass,
    .draw = M_Draw,
    .end_pass = M_EndPass,
    .blit_screen = M_BlitScreen,
    .read_pixels = M_ReadPixels,
};

void GfxRecorder_Start(void)
{
    Array_Init(&m_Quads, &g_HeapAllocator);
    Array_Init(&m_Window, &g_HeapAllocator);
    Array_Init(&m_Textures, &g_HeapAllocator);
    m_Copies = 0;
    if (!Result_Log(
            GfxFrontend_Start(&m_Backend, nullptr, 640, 480),
            LOG_LEVEL_ERROR)) {
        LOG_FATAL("Cannot start the graphics recorder");
    }
}

void GfxRecorder_Stop(void)
{
    GfxFrontend_Stop();
    Array_Free(&m_Quads);
    Array_Free(&m_Window);
    for (int32_t i = 0; i < m_Textures.count; i++) {
        Mem_Free(m_Textures.items[i].pixels);
    }
    Array_Free(&m_Textures);
}

const uint8_t *GfxRecorder_TexturePixels(const GFX_TEXTURE *const texture)
{
    for (int32_t i = 0; i < m_Textures.count; i++) {
        if ((void *)m_Textures.items[i].handle == texture->handle) {
            return m_Textures.items[i].pixels;
        }
    }
    return nullptr;
}

int32_t GfxRecorder_QuadCount(void)
{
    return m_Quads.count;
}

const GFX_RECORDED_QUAD *GfxRecorder_Quad(const int32_t index)
{
    return Array_Get(&m_Quads, index);
}

uint64_t GfxRecorder_QuadId(const int32_t index)
{
    return GfxFrontend_GetQuadId(index);
}

int32_t GfxRecorder_CopyCount(void)
{
    return m_Copies;
}

const GFX_VERTEX *GfxRecorder_Blend(const float t, const int32_t index)
{
    const GFX_OUTPUT output = { .width = 640, .height = 480 };
    if (!Result_Log(Gfx_Present(true, t, &output), LOG_LEVEL_ERROR)) {
        LOG_FATAL("Cannot present the recorded tick");
    }
    return Array_Get(&m_Window, index * 4);
}
