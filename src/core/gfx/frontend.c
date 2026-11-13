// SPDX-License-Identifier: GPL-3.0-only

#include "core/gfx/private.h"

#include "core/base/array.h"
#include "core/base/assert.h"
#include "core/base/hash_map.h"
#include "core/base/memory.h"

#include <math.h>
#include <string.h>

#define M_PI_F 3.14159265f

typedef struct {
    uint64_t id;
    bool has_motion;
    GFX_MOTION motion;
} M_QUAD;

// Identifies a view within a tick: its id, and how many views with that
// id came before it in the tick.
typedef struct {
    uint32_t id;
    uint32_t occurrence;
} M_VIEW_KEY;

typedef ARRAY(GFX_VERTEX) M_VERTEX_ARRAY;
typedef ARRAY(M_QUAD) M_QUAD_ARRAY;
typedef ARRAY(GFX_BATCH) M_BATCH_ARRAY;
typedef ARRAY(GFX_VIEW) M_VIEW_ARRAY;
typedef ARRAY(GFX_PASS_VIEW) M_PASS_VIEW_ARRAY;
typedef HASH_MAP(uint64_t, int32_t) M_QUAD_INDEX;
typedef ARRAY(M_VIEW_KEY) M_VIEW_KEY_ARRAY;
typedef HASH_MAP(M_VIEW_KEY, int32_t) M_VIEW_INDEX;
typedef HASH_MAP(uint32_t, uint32_t) M_VIEW_COUNTS;

typedef struct {
    M_VERTEX_ARRAY vertices;
    M_QUAD_ARRAY quads;
    M_BATCH_ARRAY batches;
    M_VIEW_ARRAY views;
    M_VIEW_KEY_ARRAY view_keys;
    M_QUAD_INDEX quad_ids;
    M_VIEW_INDEX view_ids;
    M_VIEW_COUNTS view_counts;
    GFX_COLOR clear;
    bool drawn;
} M_TICK;

typedef struct {
    const GFX_BACKEND *backend;
    bool started;
    int32_t width, height;
    M_TICK ticks[2];
    int32_t current;
    bool in_tick;
    int32_t view;
    int32_t flushed;
    GFX_TEXTURE *textures;
    GFX_TEXTURE *freed;
    M_PASS_VIEW_ARRAY pass_views;
    M_VERTEX_ARRAY blended;
} M_FRONTEND;

static M_FRONTEND m_Frontend;

static M_TICK *M_Current(void)
{
    return &m_Frontend.ticks[m_Frontend.current];
}

static M_TICK *M_Previous(void)
{
    return &m_Frontend.ticks[1 - m_Frontend.current];
}

static void M_InitTick(M_TICK *const tick)
{
    Array_Init(&tick->vertices, &g_HeapAllocator);
    Array_Init(&tick->quads, &g_HeapAllocator);
    Array_Init(&tick->batches, &g_HeapAllocator);
    Array_Init(&tick->views, &g_HeapAllocator);
    Array_Init(&tick->view_keys, &g_HeapAllocator);
    HashMap_Init(&tick->quad_ids, &g_HeapAllocator);
    HashMap_Init(&tick->view_ids, &g_HeapAllocator);
    HashMap_Init(&tick->view_counts, &g_HeapAllocator);
    tick->drawn = false;
}

static void M_ClearTick(M_TICK *const tick)
{
    Array_Clear(&tick->vertices);
    Array_Clear(&tick->quads);
    Array_Clear(&tick->batches);
    Array_Clear(&tick->views);
    Array_Clear(&tick->view_keys);
    HashMap_Clear(&tick->quad_ids);
    HashMap_Clear(&tick->view_ids);
    HashMap_Clear(&tick->view_counts);
    tick->drawn = false;
}

static void M_FreeTick(M_TICK *const tick)
{
    Array_Free(&tick->vertices);
    Array_Free(&tick->quads);
    Array_Free(&tick->batches);
    Array_Free(&tick->views);
    Array_Free(&tick->view_keys);
    HashMap_Free(&tick->quad_ids);
    HashMap_Free(&tick->view_ids);
    HashMap_Free(&tick->view_counts);
}

static void M_Unlink(GFX_TEXTURE *const texture)
{
    GFX_TEXTURE **link = &m_Frontend.textures;
    while (*link != nullptr && *link != texture) {
        link = &(*link)->next;
    }
    if (*link != nullptr) {
        *link = texture->next;
    }
}

static void M_ReleaseFreed(void)
{
    while (m_Frontend.freed != nullptr) {
        GFX_TEXTURE *const texture = m_Frontend.freed;
        m_Frontend.freed = texture->next;
        if (texture->handle != nullptr) {
            m_Frontend.backend->free_texture(texture->handle);
        }
        Mem_Free(texture);
    }
}

static bool M_SameMatrix(const MAT4 *const a, const MAT4 *const b)
{
    for (int32_t row = 0; row < 4; row++) {
        for (int32_t col = 0; col < 4; col++) {
            if (a->m[row][col] != b->m[row][col]) {
                return false;
            }
        }
    }
    return true;
}

static bool M_SameColor(const GFX_COLOR a, const GFX_COLOR b)
{
    return a.red == b.red && a.green == b.green && a.blue == b.blue
        && a.alpha == b.alpha;
}

static bool M_SameView(const GFX_VIEW *const a, const GFX_VIEW *const b)
{
    return a->id == b->id && M_SameMatrix(&a->view, &b->view)
        && M_SameMatrix(&a->projection, &b->projection)
        && a->viewport.x == b->viewport.x && a->viewport.y == b->viewport.y
        && a->viewport.width == b->viewport.width
        && a->viewport.height == b->viewport.height
        && a->viewport.min_z == b->viewport.min_z
        && a->viewport.max_z == b->viewport.max_z
        && a->fog.enabled == b->fog.enabled && a->fog.begin == b->fog.begin
        && a->fog.end == b->fog.end && M_SameColor(a->fog.color, b->fog.color);
}

static bool M_SameState(const GFX_STATE *const a, const GFX_STATE *const b)
{
    return a->blend == b->blend && a->color_op == b->color_op
        && a->depth_test == b->depth_test && a->depth_write == b->depth_write
        && a->alpha_test == b->alpha_test && a->repeat == b->repeat;
}

// Folds the viewport of view into its projection, so that the projection
// maps to logical-screen pixels and depth from 0 to 1.
static GFX_PASS_VIEW M_PassView(const GFX_VIEW *const view)
{
    const VIEWPORT *const viewport = &view->viewport;
    MAT4 remap = {};
    remap.m[0][0] = viewport->width / 2.0f;
    remap.m[1][1] = -viewport->height / 2.0f;
    remap.m[2][2] = viewport->max_z - viewport->min_z;
    remap.m[3][0] = viewport->x + viewport->width / 2.0f;
    remap.m[3][1] = viewport->y + viewport->height / 2.0f;
    remap.m[3][2] = viewport->min_z;
    remap.m[3][3] = 1.0f;
    return (GFX_PASS_VIEW) {
        .view = view->view,
        .projection = Mat4_Multiply(&view->projection, &remap),
        .fog = view->fog,
        .scissor_x = viewport->x,
        .scissor_y = viewport->y,
        .scissor_width = viewport->width,
        .scissor_height = viewport->height,
    };
}

static void M_BuildPassViews(const M_VIEW_ARRAY *const views)
{
    Array_Clear(&m_Frontend.pass_views);
    for (int32_t i = 0; i < views->count; i++) {
        Array_Append(&m_Frontend.pass_views, M_PassView(&views->items[i]));
    }
}

static void M_Flush(void)
{
    M_TICK *const tick = M_Current();
    if (!m_Frontend.in_tick || tick->batches.count == m_Frontend.flushed) {
        return;
    }
    M_BuildPassViews(&tick->views);
    m_Frontend.backend->draw(
        m_Frontend.pass_views.items, tick->batches.items + m_Frontend.flushed,
        tick->batches.count - m_Frontend.flushed, tick->vertices.items);
    m_Frontend.flushed = tick->batches.count;
}

static GFX_PASS_OUTPUT M_ScreenOutput(void)
{
    return (GFX_PASS_OUTPUT) {
        .width = m_Frontend.width,
        .height = m_Frontend.height,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
        .rect = { 0, 0, m_Frontend.width, m_Frontend.height },
    };
}

static void M_AddView(M_TICK *const tick, const GFX_VIEW *const view)
{
    m_Frontend.view = tick->views.count;
    Array_Append(&tick->views, *view);
    M_VIEW_KEY key = { .id = view->id };
    if (view->id != 0) {
        uint32_t *const count = HashMap_GetOrPut(&tick->view_counts, view->id);
        key.occurrence = (*count)++;
        HashMap_Put(&tick->view_ids, key, m_Frontend.view);
    }
    Array_Append(&tick->view_keys, key);
}

static void M_AddQuad(
    const GFX_STATE *const state, GFX_TEXTURE *const texture,
    const GFX_VERTEX vertices[4], const GFX_MOTION *const motion,
    const uint64_t id)
{
    if (!m_Frontend.in_tick) {
        return;
    }
    M_TICK *const tick = M_Current();
    const int32_t first = tick->vertices.count;
    if (id != 0) {
        bool duplicate;
        *HashMap_GetOrPut(&tick->quad_ids, id, &duplicate) = tick->quads.count;
#ifndef NDEBUG
        ASSERT(!duplicate);
#endif
    }
    Array_Append(
        &tick->quads,
        ((M_QUAD) { .id = id,
                    .has_motion = motion != nullptr,
                    .motion = motion != nullptr ? *motion : (GFX_MOTION) {} }));
    Array_AppendSlice(&tick->vertices, vertices, 4);
    GFX_BATCH *const last = tick->batches.count > m_Frontend.flushed
        ? &tick->batches.items[tick->batches.count - 1]
        : nullptr;
    if (last != nullptr && last->texture == texture
        && last->view == m_Frontend.view && M_SameState(&last->state, state)) {
        last->count += 4;
        return;
    }
    Array_Append(
        &tick->batches,
        ((GFX_BATCH) { .state = *state,
                       .texture = texture,
                       .view = m_Frontend.view,
                       .first = first,
                       .count = 4 }));
}

static float M_Lerp(const float a, const float b, const float t)
{
    return a + (b - a) * t;
}

static GFX_VIEW M_BlendView(
    const GFX_VIEW *const previous, const GFX_VIEW *const current,
    const float t)
{
    GFX_VIEW view = *current;
    for (int32_t row = 0; row < 4; row++) {
        for (int32_t col = 0; col < 4; col++) {
            view.view.m[row][col] = M_Lerp(
                previous->view.m[row][col], current->view.m[row][col], t);
            view.projection.m[row][col] = M_Lerp(
                previous->projection.m[row][col],
                current->projection.m[row][col], t);
        }
    }
    view.viewport = (VIEWPORT) {
        M_Lerp(previous->viewport.x, current->viewport.x, t),
        M_Lerp(previous->viewport.y, current->viewport.y, t),
        M_Lerp(previous->viewport.width, current->viewport.width, t),
        M_Lerp(previous->viewport.height, current->viewport.height, t),
        M_Lerp(previous->viewport.min_z, current->viewport.min_z, t),
        M_Lerp(previous->viewport.max_z, current->viewport.max_z, t),
    };
    view.fog.begin = M_Lerp(previous->fog.begin, current->fog.begin, t);
    view.fog.end = M_Lerp(previous->fog.end, current->fog.end, t);
    view.fog.color = (GFX_COLOR) {
        (uint8_t)lroundf(
            M_Lerp(previous->fog.color.red, current->fog.color.red, t)),
        (uint8_t)lroundf(
            M_Lerp(previous->fog.color.green, current->fog.color.green, t)),
        (uint8_t)lroundf(
            M_Lerp(previous->fog.color.blue, current->fog.color.blue, t)),
        (uint8_t)lroundf(
            M_Lerp(previous->fog.color.alpha, current->fog.color.alpha, t)),
    };
    return view;
}

static void M_BlendViews(const float t)
{
    const M_TICK *const current = M_Current();
    const M_TICK *const previous = M_Previous();
    Array_Clear(&m_Frontend.pass_views);
    for (int32_t i = 0; i < current->views.count; i++) {
        const GFX_VIEW *const view = &current->views.items[i];
        const int32_t *const match = view->id != 0 && previous->drawn
            ? HashMap_Get(&previous->view_ids, current->view_keys.items[i])
            : nullptr;
        const GFX_VIEW blended = match != nullptr
            ? M_BlendView(&previous->views.items[*match], view, t)
            : *view;
        Array_Append(&m_Frontend.pass_views, M_PassView(&blended));
    }
}

static float M_ShorterTurn(const float from, const float to)
{
    float turn = fmodf(to - from, 2.0f * M_PI_F);
    if (turn > M_PI_F) {
        turn -= 2.0f * M_PI_F;
    } else if (turn < -M_PI_F) {
        turn += 2.0f * M_PI_F;
    }
    return turn;
}

static float M_Ratio(const float to, const float from)
{
    return from != 0.0f ? to / from : 1.0f;
}

// Moves the vertices of a sprite from motion current to the blend of
// previous and current: v' = C_t + R(a_t - a) S(s_t / s) (v - C).
static void M_BlendSprite(
    GFX_VERTEX vertices[4], const GFX_MOTION *const previous,
    const GFX_MOTION *const current, const float t)
{
    const VEC2 center = {
        M_Lerp(previous->center.x, current->center.x, t),
        M_Lerp(previous->center.y, current->center.y, t),
    };
    const float ratio_x = M_Ratio(
        M_Lerp(previous->scale.x, current->scale.x, t), current->scale.x);
    const float ratio_y = M_Ratio(
        M_Lerp(previous->scale.y, current->scale.y, t), current->scale.y);
    const float turn =
        (t - 1.0f) * M_ShorterTurn(previous->rotation, current->rotation);
    const float sine = sinf(turn);
    const float cosine = cosf(turn);
    for (int32_t i = 0; i < 4; i++) {
        const float x = (vertices[i].position.x - current->center.x) * ratio_x;
        const float y = (vertices[i].position.y - current->center.y) * ratio_y;
        vertices[i].position.x = center.x + x * cosine + y * sine;
        vertices[i].position.y = center.y - x * sine + y * cosine;
    }
}

static void M_BlendVertices(const float t)
{
    const M_TICK *const current = M_Current();
    const M_TICK *const previous = M_Previous();
    Array_Clear(&m_Frontend.blended);
    Array_AppendSlice(
        &m_Frontend.blended, current->vertices.items, current->vertices.count);
    if (!previous->drawn) {
        return;
    }
    for (int32_t i = 0; i < current->quads.count; i++) {
        const M_QUAD *const quad = &current->quads.items[i];
        const int32_t *const match = quad->id != 0
            ? HashMap_Get(&previous->quad_ids, quad->id)
            : nullptr;
        if (match == nullptr) {
            continue;
        }
        const M_QUAD *const old = &previous->quads.items[*match];
        GFX_VERTEX *const vertices = &m_Frontend.blended.items[i * 4];
        if (quad->has_motion && old->has_motion) {
            M_BlendSprite(vertices, &old->motion, &quad->motion, t);
            continue;
        }
        const GFX_VERTEX *const from = &previous->vertices.items[*match * 4];
        for (int32_t k = 0; k < 4; k++) {
            vertices[k].position = (VEC3) {
                M_Lerp(from[k].position.x, vertices[k].position.x, t),
                M_Lerp(from[k].position.y, vertices[k].position.y, t),
                M_Lerp(from[k].position.z, vertices[k].position.z, t),
            };
        }
    }
}

RESULT GfxFrontend_Start(
    const GFX_BACKEND *const backend, SDL_Window *const window,
    const int32_t width, const int32_t height)
{
    FAIL_IF(m_Frontend.started, "The renderer is already started");
    FAIL_IF(width <= 0 || height <= 0, "Invalid logical screen size");
    MUST(backend->init(window, width, height));
    m_Frontend = (M_FRONTEND) {
        .backend = backend,
        .started = true,
        .width = width,
        .height = height,
        .textures = m_Frontend.textures,
    };
    M_InitTick(&m_Frontend.ticks[0]);
    M_InitTick(&m_Frontend.ticks[1]);
    Array_Init(&m_Frontend.pass_views, &g_HeapAllocator);
    Array_Init(&m_Frontend.blended, &g_HeapAllocator);
    return OK;
}

void GfxFrontend_Stop(void)
{
    if (!m_Frontend.started) {
        return;
    }
    M_ReleaseFreed();
    for (GFX_TEXTURE *texture = m_Frontend.textures; texture != nullptr;
         texture = texture->next) {
        if (texture->handle != nullptr) {
            m_Frontend.backend->free_texture(texture->handle);
            texture->handle = nullptr;
        }
    }
    M_FreeTick(&m_Frontend.ticks[0]);
    M_FreeTick(&m_Frontend.ticks[1]);
    Array_Free(&m_Frontend.pass_views);
    Array_Free(&m_Frontend.blended);
    m_Frontend.backend->shutdown();
    m_Frontend = (M_FRONTEND) { .textures = m_Frontend.textures };
}

uint64_t GfxFrontend_GetQuadId(const int32_t index)
{
    return Array_Get(&M_Current()->quads, index)->id;
}

GFX_PASS_OUTPUT GfxOutput_Fit(
    const GFX_OUTPUT *const output, const int32_t width, const int32_t height)
{
    float scale = fminf(
        (float)output->width / (float)width,
        (float)output->height / (float)height);
    if (output->integer_scale && scale >= 1.0f) {
        scale = floorf(scale);
    }
    const int32_t fit_width = (int32_t)lroundf((float)width * scale);
    const int32_t fit_height = (int32_t)lroundf((float)height * scale);
    const GFX_RECT rect = {
        (output->width - fit_width) / 2,
        (output->height - fit_height) / 2,
        fit_width,
        fit_height,
    };
    return (GFX_PASS_OUTPUT) {
        .width = output->width,
        .height = output->height,
        .scale_x = (float)fit_width / (float)width,
        .scale_y = (float)fit_height / (float)height,
        .offset_x = (float)rect.x,
        .offset_y = (float)rect.y,
        .rect = rect,
        .border = output->border,
    };
}

RESULT GfxTexture_Create(const IMAGE *const image, GFX_TEXTURE **const out)
{
    *out = nullptr;
    FAIL_IF(
        image == nullptr || image->pixels == nullptr || image->width <= 0
            || image->height <= 0,
        "Invalid texture image");
    GFX_TEXTURE *const texture = Mem_AllocZ(sizeof(*texture));
    texture->width = image->width;
    texture->height = image->height;
    if (m_Frontend.started) {
        const RESULT result = m_Frontend.backend->create_texture(
            image->width, image->height, image->pixels, &texture->handle);
        if (!IS_OK(result)) {
            Mem_Free(texture);
            return result;
        }
    }
    texture->next = m_Frontend.textures;
    m_Frontend.textures = texture;
    *out = texture;
    return OK;
}

RESULT GfxTexture_CreateEmpty(
    const int32_t width, const int32_t height, GFX_TEXTURE **const out)
{
    *out = nullptr;
    FAIL_IF(width <= 0 || height <= 0, "Invalid texture size");
    GFX_TEXTURE *const texture = Mem_AllocZ(sizeof(*texture));
    texture->width = width;
    texture->height = height;
    if (m_Frontend.started) {
        const RESULT result = m_Frontend.backend->create_texture(
            width, height, nullptr, &texture->handle);
        if (!IS_OK(result)) {
            Mem_Free(texture);
            return result;
        }
    }
    texture->next = m_Frontend.textures;
    m_Frontend.textures = texture;
    *out = texture;
    return OK;
}

int32_t GfxTexture_GetWidth(const GFX_TEXTURE *const texture)
{
    return texture->width;
}

int32_t GfxTexture_GetHeight(const GFX_TEXTURE *const texture)
{
    return texture->height;
}

void GfxTexture_Upload(
    GFX_TEXTURE *const texture, const GFX_RECT rect, const IMAGE *const image)
{
    ASSERT(
        rect.x >= 0 && rect.y >= 0 && rect.width == image->width
        && rect.height == image->height
        && (int64_t)rect.x + rect.width <= texture->width
        && (int64_t)rect.y + rect.height <= texture->height);
    if (texture->handle == nullptr || rect.width <= 0 || rect.height <= 0) {
        return;
    }
    M_Flush();
    m_Frontend.backend->upload_texture(texture->handle, rect, image);
}

void GfxTexture_CopyFromScreen(
    GFX_TEXTURE *const texture, GFX_RECT source, int32_t x, int32_t y)
{
    if (texture->handle == nullptr) {
        return;
    }
    if (source.x < 0) {
        x -= source.x;
        source.width += source.x;
        source.x = 0;
    }
    if (source.y < 0) {
        y -= source.y;
        source.height += source.y;
        source.y = 0;
    }
    if (x < 0) {
        source.x -= x;
        source.width += x;
        x = 0;
    }
    if (y < 0) {
        source.y -= y;
        source.height += y;
        y = 0;
    }
    source.width =
        MIN(source.width, MIN(m_Frontend.width - source.x, texture->width - x));
    source.height = MIN(
        source.height, MIN(m_Frontend.height - source.y, texture->height - y));
    if (source.width <= 0 || source.height <= 0) {
        return;
    }
    M_Flush();
    m_Frontend.backend->copy_from_screen(texture->handle, source, x, y);
}

void GfxTexture_Free(GFX_TEXTURE *const texture)
{
    if (texture == nullptr) {
        return;
    }
    M_Unlink(texture);
    if (!m_Frontend.started) {
        Mem_Free(texture);
        return;
    }
    texture->next = m_Frontend.freed;
    m_Frontend.freed = texture;
}

GFX_VIEW GfxView_Screen(void)
{
    GFX_VIEW view = {
        .view = Mat4_Identity(),
        .projection = Mat4_Identity(),
    };
    if (m_Frontend.width > 0 && m_Frontend.height > 0) {
        view.projection.m[0][0] = 2.0f / (float)m_Frontend.width;
        view.projection.m[1][1] = -2.0f / (float)m_Frontend.height;
        view.projection.m[3][0] = -1.0f;
        view.projection.m[3][1] = 1.0f;
        view.viewport = (VIEWPORT) {
            0.0f, 0.0f, (float)m_Frontend.width, (float)m_Frontend.height,
            0.0f, 1.0f
        };
    }
    return view;
}

void Gfx_BeginTick(const GFX_COLOR clear)
{
    if (!m_Frontend.started) {
        return;
    }
    if (m_Frontend.in_tick) {
        Gfx_EndTick();
    }
    M_ReleaseFreed();
    m_Frontend.current = 1 - m_Frontend.current;
    M_TICK *const tick = M_Current();
    M_ClearTick(tick);
    tick->clear = clear;
    m_Frontend.in_tick = true;
    m_Frontend.flushed = 0;
    const GFX_VIEW screen = GfxView_Screen();
    M_AddView(tick, &screen);
    const GFX_PASS_OUTPUT output = M_ScreenOutput();
    m_Frontend.backend->begin_pass(GFX_PASS_SCREEN, clear, &output);
}

void Gfx_SetView(const GFX_VIEW *const view)
{
    if (!m_Frontend.in_tick) {
        return;
    }
    M_TICK *const tick = M_Current();
    if (M_SameView(&tick->views.items[m_Frontend.view], view)) {
        return;
    }
    if (M_SameView(&tick->views.items[tick->views.count - 1], view)) {
        m_Frontend.view = tick->views.count - 1;
        return;
    }
    M_AddView(tick, view);
}

void Gfx_DrawQuad(
    const GFX_STATE *const state, GFX_TEXTURE *const texture,
    const GFX_VERTEX vertices[4], const uint64_t id)
{
    M_AddQuad(state, texture, vertices, nullptr, id);
}

void Gfx_DrawSprite(
    const GFX_STATE *const state, GFX_TEXTURE *const texture,
    const GFX_VERTEX vertices[4], const GFX_MOTION *const motion,
    const uint64_t id)
{
    M_AddQuad(state, texture, vertices, motion, id);
}

void Gfx_EndTick(void)
{
    if (!m_Frontend.in_tick) {
        return;
    }
    M_Flush();
    m_Frontend.backend->end_pass();
    m_Frontend.in_tick = false;
    M_Current()->drawn = true;
}

RESULT Gfx_Present(
    const bool interpolate, const float t, const GFX_OUTPUT *const output)
{
    if (!m_Frontend.started || output->width <= 0 || output->height <= 0) {
        return OK;
    }
    FAIL_IF(!isfinite(t) || t < 0.0f || t > 1.0f, "Invalid blend factor");
    if (m_Frontend.in_tick) {
        Gfx_EndTick();
    }
    const GFX_PASS_OUTPUT fit =
        GfxOutput_Fit(output, m_Frontend.width, m_Frontend.height);
    const M_TICK *const current = M_Current();
    if (!current->drawn) {
        m_Frontend.backend->begin_pass(GFX_PASS_WINDOW, output->border, &fit);
        m_Frontend.backend->end_pass();
        return OK;
    }
    if (!interpolate) {
        m_Frontend.backend->blit_screen(&fit);
        return OK;
    }
    M_BlendViews(t);
    M_BlendVertices(t);
    m_Frontend.backend->begin_pass(GFX_PASS_WINDOW, current->clear, &fit);
    if (current->batches.count > 0) {
        m_Frontend.backend->draw(
            m_Frontend.pass_views.items, current->batches.items,
            current->batches.count, m_Frontend.blended.items);
    }
    m_Frontend.backend->end_pass();
    return OK;
}

RESULT Gfx_ReadScreen(const ALLOCATOR *const allocator, IMAGE *const out)
{
    *out = (IMAGE) {};
    FAIL_IF(!m_Frontend.started, "The renderer is not started");
    return m_Frontend.backend->read_pixels(GFX_PASS_SCREEN, allocator, out);
}

RESULT Gfx_ReadOutput(const ALLOCATOR *const allocator, IMAGE *const out)
{
    *out = (IMAGE) {};
    FAIL_IF(!m_Frontend.started, "The renderer is not started");
    return m_Frontend.backend->read_pixels(GFX_PASS_WINDOW, allocator, out);
}
