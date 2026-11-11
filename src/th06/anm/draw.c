#include "th06/anm/draw.h"

#include "core/base/assert.h"
#include "core/base/utils.h"
#include "core/math/x87.h"

#include <math.h>

static GFX_COLOR M_Color(const uint32_t color)
{
    return (GFX_COLOR) { (uint8_t)(color >> 16), (uint8_t)(color >> 8),
                         (uint8_t)color, (uint8_t)(color >> 24) };
}

static bool M_Visible(const ANM_VM *const vm)
{
    return vm->shown && vm->visible && vm->color != 0
        && vm->sprite_data != nullptr && vm->sprite_data->slot >= 0
        && Anm_GetTexture(vm->sprite_data->slot) != nullptr;
}

// Draws vertices, given in strip order (top left, top right, bottom left,
// bottom right), as a sprite when motion is not nullptr.
static void M_Record(
    const ANM_VM *const vm, const ANM_DRAW *const context,
    const VEC3 positions[4], const GFX_MOTION *const motion)
{
    const ANM_SPRITE *const sprite = vm->sprite_data;
    const float u0 = sprite->u0 + vm->uv_scroll_x;
    const float u1 = sprite->u1 + vm->uv_scroll_x;
    const float v0 = sprite->v0 + vm->uv_scroll_y;
    const float v1 = sprite->v1 + vm->uv_scroll_y;
    const GFX_COLOR color = M_Color(vm->color);
    const GFX_VERTEX vertices[4] = {
        { positions[0], color, { u0, v0 } },
        { positions[1], color, { u1, v0 } },
        { positions[3], color, { u1, v1 } },
        { positions[2], color, { u0, v1 } },
    };
    const GFX_STATE state = {
        .blend = vm->blend_add ? GFX_BLEND_ADD : GFX_BLEND_ALPHA,
        .color_op = vm->color_op ? GFX_COLOR_ADD : GFX_COLOR_MODULATE,
        .depth_test = context->depth_test,
        .depth_write = context->depth_write && !vm->z_write_off,
        .alpha_test = true,
        .repeat = true,
    };
    const uint64_t id = context->interpolate && vm->presentation_generation != 0
        ? ((uint64_t)vm->presentation_generation << 32) | context->part
        : 0;
    GFX_TEXTURE *const texture = Anm_GetTexture(sprite->slot);
    Gfx_SetView(&context->view);
    if (motion != nullptr) {
        Gfx_DrawSprite(&state, texture, vertices, motion, id);
    } else {
        Gfx_DrawQuad(&state, texture, vertices, id);
    }
}

static void M_Unrotated(
    const ANM_VM *const vm, const ANM_DRAW *const draw, const bool round)
{
    if (!M_Visible(vm)) {
        return;
    }
    const float w = vm->sprite_data->width * vm->scale_x / 2.0f;
    const float h = vm->sprite_data->height * vm->scale_y / 2.0f;
    float x0 = (vm->anchor & 1) == 0 ? vm->pos.x - w : vm->pos.x;
    float x1 = (vm->anchor & 1) == 0 ? w + vm->pos.x : w + vm->pos.x + w;
    float y0 = (vm->anchor & 2) == 0 ? vm->pos.y - h : vm->pos.y;
    float y1 = (vm->anchor & 2) == 0 ? h + vm->pos.y : h + vm->pos.y + h;
    if (round) {
        x0 = X87_Rint(x0);
        x1 = X87_Rint(x1);
        y0 = X87_Rint(y0);
        y1 = X87_Rint(y1);
    } else {
        x0 += 0.5f;
        x1 += 0.5f;
        y0 += 0.5f;
        y1 += 0.5f;
    }
    const VEC3 positions[4] = { { x0, y0, vm->pos.z },
                                { x1, y0, vm->pos.z },
                                { x0, y1, vm->pos.z },
                                { x1, y1, vm->pos.z } };
    M_Record(vm, draw, positions, nullptr);
}

static MAT4 M_Rotation(const int32_t axis, const float angle)
{
    MAT4 rotation = Mat4_Identity();
    float sine, cosine;
    X87_SinCos(angle, &sine, &cosine);
    const int32_t a = (axis + 1) % 3;
    const int32_t b = (axis + 2) % 3;
    rotation.m[a][a] = cosine;
    rotation.m[a][b] = sine;
    rotation.m[b][a] = -sine;
    rotation.m[b][b] = cosine;
    return rotation;
}

static void M_World(
    const ANM_VM *const vm, const ANM_DRAW *const draw, const bool round)
{
    if (!M_Visible(vm)) {
        return;
    }
    const ANM_SPRITE *const sprite = vm->sprite_data;
    MAT4 world = Mat4_Identity();
    world.m[0][0] = vm->uv_scale_x * vm->scale_x;
    world.m[1][1] = -vm->scale_y * vm->uv_scale_y;
    const float angles[3] = { vm->rotation.x, vm->rotation.y, vm->rotation.z };
    const bool rounded =
        round && angles[0] == 0.0f && angles[1] == 0.0f && angles[2] == 0.0f;
    for (int32_t axis = 0; axis < 3; axis++) {
        if (angles[axis] != 0.0f) {
            const MAT4 rotation = M_Rotation(axis, angles[axis]);
            world = Mat4_Multiply(&world, &rotation);
        }
    }
    world.m[3][0] = rounded ? X87_Rint(vm->pos.x) - 0.5f : vm->pos.x;
    world.m[3][1] = rounded ? -X87_Rint(vm->pos.y) + 0.5f : -vm->pos.y;
    if ((vm->anchor & 1) != 0) {
        const float half = sprite->width * vm->scale_x / 2.0f;
        world.m[3][0] += rounded ? half : fabsf(half);
    }
    if ((vm->anchor & 2) != 0) {
        const float half = sprite->height * vm->scale_y / 2.0f;
        world.m[3][1] -= rounded ? half : fabsf(half);
    }
    world.m[3][2] = vm->pos.z;
    VEC3 positions[4] = { { -128.0f, -128.0f, 0.0f },
                          { 128.0f, -128.0f, 0.0f },
                          { -128.0f, 128.0f, 0.0f },
                          { 128.0f, 128.0f, 0.0f } };
    for (int32_t i = 0; i < 4; i++) {
        positions[i] = Vec3_TransformCoord(positions[i], &world);
    }
    M_Record(vm, draw, positions, nullptr);
}

ANM_DRAW Anm_ScreenDraw(void)
{
    ANM_DRAW draw = {
        .view = GfxView_Screen(),
        .depth_test = GFX_DEPTH_LESS_EQUAL,
        .depth_write = true,
    };
    draw.view.id = ANM_VIEW_SCREEN;
    return draw;
}

GFX_VIEW Anm_ScreenView(const VIEWPORT viewport, const uint32_t id)
{
    GFX_VIEW view = GfxView_Screen();
    const float depth = viewport.max_z - viewport.min_z;
    view.projection = Mat4_Identity();
    view.projection.m[0][0] = 2.0f / viewport.width;
    view.projection.m[1][1] = -2.0f / viewport.height;
    view.projection.m[2][2] = depth != 0.0f ? 1.0f / depth : 1.0f;
    view.projection.m[3][0] = -2.0f * viewport.x / viewport.width - 1.0f;
    view.projection.m[3][1] = 2.0f * viewport.y / viewport.height + 1.0f;
    view.projection.m[3][2] = depth != 0.0f ? -viewport.min_z / depth : 0.0f;
    view.viewport = viewport;
    view.id = id;
    return view;
}

void Anm_DrawRect(const GFX_RECT rect, const uint32_t color)
{
    const float x0 = (float)rect.x;
    const float y0 = (float)rect.y;
    const float x1 = (float)(rect.x + rect.width);
    const float y1 = (float)(rect.y + rect.height);
    const GFX_COLOR rgba = M_Color(color);
    const GFX_VERTEX vertices[4] = {
        { { x0, y0, 0.0f }, rgba, {} },
        { { x1, y0, 0.0f }, rgba, {} },
        { { x1, y1, 0.0f }, rgba, {} },
        { { x0, y1, 0.0f }, rgba, {} },
    };
    GFX_VIEW view = GfxView_Screen();
    view.id = ANM_VIEW_SCREEN;
    Gfx_SetView(&view);
    Gfx_DrawQuad(
        &(GFX_STATE) { .blend = GFX_BLEND_ALPHA }, nullptr, vertices, 0);
}

void Anm_DrawUnrotated(const ANM_VM *const vm, const ANM_DRAW *const draw)
{
    M_Unrotated(vm, draw, true);
}

void Anm_DrawNoRound(const ANM_VM *const vm, const ANM_DRAW *const draw)
{
    M_Unrotated(vm, draw, false);
}

void Anm_Draw(const ANM_VM *const vm, const ANM_DRAW *const draw)
{
    if (vm->rotation.z == 0.0f) {
        Anm_DrawUnrotated(vm, draw);
        return;
    }
    if (!M_Visible(vm)) {
        return;
    }
    float sine, cosine;
    X87_SinCos(vm->rotation.z, &sine, &cosine);
    const float cx = X87_Rint(vm->pos.x);
    const float cy = X87_Rint(vm->pos.y);
    const float w = X87_Rint(vm->sprite_data->width * vm->scale_x / 2.0f);
    const float h = X87_Rint(vm->sprite_data->height * vm->scale_y / 2.0f);
    const float x[4] = { -w - 0.5f, w - 0.5f, -w - 0.5f, w - 0.5f };
    const float y[4] = { -h - 0.5f, -h - 0.5f, h - 0.5f, h - 0.5f };
    const VEC2 pivot = {
        cx + ((vm->anchor & 1) != 0 ? w : 0.0f) + 0.5f,
        cy + ((vm->anchor & 2) != 0 ? h : 0.0f) + 0.5f,
    };
    VEC3 positions[4];
    for (int32_t i = 0; i < 4; i++) {
        positions[i] = (VEC3) { x[i] * cosine + y[i] * sine + cx,
                                -x[i] * sine + y[i] * cosine + cy, vm->pos.z };
        if ((vm->anchor & 1) != 0) {
            positions[i].x += w;
        }
        if ((vm->anchor & 2) != 0) {
            positions[i].y += h;
        }
        positions[i].x += 0.5f;
        positions[i].y += 0.5f;
    }
    const GFX_MOTION motion = {
        .center = pivot,
        .scale = { vm->scale_x, vm->scale_y },
        .rotation = vm->rotation.z,
    };
    M_Record(vm, draw, positions, &motion);
}

void Anm_Draw3D(const ANM_VM *const vm, const ANM_DRAW *const draw)
{
    M_World(vm, draw, false);
}

void Anm_Draw3DRounded(const ANM_VM *const vm, const ANM_DRAW *const draw)
{
    M_World(vm, draw, true);
}
