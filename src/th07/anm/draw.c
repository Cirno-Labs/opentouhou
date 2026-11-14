#include "th07/anm/draw.h"

#include "core/math/x87.h"

#include <math.h>

typedef struct {
    GFX_VIEW cameras[ANM_CAMERA_COUNT];
    VEC3 camera_right;
    ANM_CAMERA camera;
    int32_t last_camera; // -1 for none
    bool modulate;
    uint32_t modulate_color;
    float shake_x, shake_y;
} M_STATE;

static M_STATE m_State = {
    .last_camera = -1,
    .modulate_color = 0x80808080u,
};

static const VEC3 m_Quad[4] = {
    { -128.0f, -128.0f, 0.0f },
    { 128.0f, -128.0f, 0.0f },
    { -128.0f, 128.0f, 0.0f },
    { 128.0f, 128.0f, 0.0f },
};

static GFX_COLOR M_Color(const ANM_VM *const vm)
{
    uint32_t color = vm->use_color2 ? vm->color2 : vm->color;
    if (m_State.modulate) {
        uint32_t modulated = 0;
        for (int32_t shift = 0; shift < 32; shift += 8) {
            uint32_t channel = ((color >> shift) & 0xffu)
                    * ((m_State.modulate_color >> shift) & 0xffu)
                >> 7;
            if (channel >= 0x100) {
                channel = 0xff;
            }
            modulated |= channel << shift;
        }
        color = modulated;
    }
    return (GFX_COLOR) { (uint8_t)(color >> 16), (uint8_t)(color >> 8),
                         (uint8_t)color, (uint8_t)(color >> 24) };
}

static bool M_Visible(const ANM_VM *const vm)
{
    return vm->shown && vm->visible && (vm->color >> 24) != 0
        && vm->sprite_data != nullptr && vm->sprite_data->slot >= 0
        && Anm_GetTexture(vm->sprite_data->slot) != nullptr;
}

// positions are in strip order: top left, top right, bottom left, bottom
// right.
static void M_Record(
    const ANM_VM *const vm, const ANM_DRAW *const draw,
    const GFX_VIEW *const view, const VEC3 positions[4], const VEC2 uv0,
    const VEC2 uv1, const GFX_MOTION *const motion)
{
    const GFX_COLOR color = M_Color(vm);
    const GFX_VERTEX vertices[4] = {
        { positions[0], color, { uv0.x, uv0.y } },
        { positions[1], color, { uv1.x, uv0.y } },
        { positions[3], color, { uv1.x, uv1.y } },
        { positions[2], color, { uv0.x, uv1.y } },
    };
    const GFX_STATE state = {
        .blend = vm->blend_add ? GFX_BLEND_ADD : GFX_BLEND_ALPHA,
        .color_op = GFX_COLOR_MODULATE,
        .depth_test = draw->depth_test,
        .depth_write = draw->depth_write && !vm->z_write_off,
        .alpha_test = true,
        .repeat = true,
    };
    const uint64_t id = draw->interpolate && vm->presentation_generation != 0
        ? ((uint64_t)vm->presentation_generation << 32) | draw->part
        : 0;
    GFX_TEXTURE *const texture = Anm_GetTexture(vm->sprite_data->slot);
    Gfx_SetView(view);
    if (motion != nullptr) {
        Gfx_DrawSprite(&state, texture, vertices, motion, id);
    } else {
        Gfx_DrawQuad(&state, texture, vertices, id);
    }
}

static void M_DrawQuad(
    const ANM_VM *const vm, const ANM_DRAW *const draw, VEC3 positions[4],
    const bool round, GFX_MOTION *const motion)
{
    for (int32_t i = 0; i < 4; i++) {
        positions[i].x += m_State.shake_x;
        positions[i].y += m_State.shake_y;
    }
    if (round) {
        const float x0 = X87_Rint(positions[0].x) - 0.5f;
        const float x1 = X87_Rint(positions[1].x) - 0.5f;
        const float y0 = X87_Rint(positions[0].y) - 0.5f;
        const float y1 = X87_Rint(positions[2].y) - 0.5f;
        positions[0].x = positions[2].x = x0;
        positions[1].x = positions[3].x = x1;
        positions[0].y = positions[1].y = y0;
        positions[2].y = positions[3].y = y1;
    }
    float min_x = positions[0].x, max_x = positions[0].x;
    float min_y = positions[0].y, max_y = positions[0].y;
    for (int32_t i = 1; i < 4; i++) {
        min_x = positions[i].x < min_x ? positions[i].x : min_x;
        max_x = positions[i].x > max_x ? positions[i].x : max_x;
        min_y = positions[i].y < min_y ? positions[i].y : min_y;
        max_y = positions[i].y > max_y ? positions[i].y : max_y;
    }
    const VIEWPORT *const viewport = &draw->view.viewport;
    if (viewport->x > max_x || viewport->y > max_y
        || viewport->x + viewport->width < min_x
        || viewport->y + viewport->height < min_y) {
        return;
    }
    // The screen view puts pixel centres at half integers; Direct3D 8 put
    // them at integers.
    for (int32_t i = 0; i < 4; i++) {
        positions[i].x += 0.5f;
        positions[i].y += 0.5f;
    }
    if (motion != nullptr) {
        motion->center.x += m_State.shake_x + 0.5f;
        motion->center.y += m_State.shake_y + 0.5f;
    }
    const ANM_SPRITE *const sprite = vm->sprite_data;
    M_Record(
        vm, draw, &draw->view, positions,
        (VEC2) { sprite->u0 + vm->uv_scroll_x, sprite->v0 + vm->uv_scroll_y },
        (VEC2) { sprite->u1 + vm->uv_scroll_x, sprite->v1 + vm->uv_scroll_y },
        motion);
}

static void M_Unrotated(
    const ANM_VM *const vm, const ANM_DRAW *const draw, const bool round)
{
    if (!M_Visible(vm)) {
        return;
    }
    const float w = vm->sprite_data->width * vm->scale_x / 2.0f;
    const float h = vm->sprite_data->height * vm->scale_y / 2.0f;
    const float x0 = (vm->anchor & 1) == 0 ? vm->pos.x - w : vm->pos.x;
    const float x1 = (vm->anchor & 1) == 0 ? w + vm->pos.x : w + vm->pos.x + w;
    const float y0 = (vm->anchor & 2) == 0 ? vm->pos.y - h : vm->pos.y;
    const float y1 = (vm->anchor & 2) == 0 ? h + vm->pos.y : h + vm->pos.y + h;
    VEC3 positions[4] = {
        { x0, y0, vm->pos.z },
        { x1, y0, vm->pos.z },
        { x0, y1, vm->pos.z },
        { x1, y1, vm->pos.z },
    };
    M_DrawQuad(vm, draw, positions, round, nullptr);
}

// Rotates the other way from th06.
static VEC3 M_RotatePoint(
    const float x, const float y, const float sine, const float cosine,
    const float cx, const float cy, const float z)
{
    return (VEC3) { x * cosine - y * sine + cx, x * sine + y * cosine + cy, z };
}

static void M_Rotated(
    VEC3 positions[4], const ANM_VM *const vm, const float w, const float h,
    const float sine, const float cosine, const VEC3 center)
{
    positions[0] =
        M_RotatePoint(-w, -h, sine, cosine, center.x, center.y, center.z);
    positions[1] =
        M_RotatePoint(w, -h, sine, cosine, center.x, center.y, center.z);
    positions[2] =
        M_RotatePoint(-w, h, sine, cosine, center.x, center.y, center.z);
    positions[3] =
        M_RotatePoint(w, h, sine, cosine, center.x, center.y, center.z);
    for (int32_t i = 0; i < 4; i++) {
        if ((vm->anchor & 1) != 0) {
            positions[i].x += w;
        }
        if ((vm->anchor & 2) != 0) {
            positions[i].y += h;
        }
    }
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

static MAT4 M_World(ANM_VM *const vm)
{
    if (!vm->keep_draw_matrix && (vm->scale_changed || vm->rotation_changed)) {
        vm->draw_matrix = vm->matrix;
        vm->draw_matrix.m[0][0] *= vm->scale_x;
        vm->draw_matrix.m[1][1] *= vm->scale_y;
        vm->scale_changed = false;
        const float angles[3] = { vm->rotation.x, vm->rotation.y,
                                  vm->rotation.z };
        for (int32_t axis = 0; axis < 3; axis++) {
            if (angles[axis] != 0.0f) {
                const MAT4 rotation = M_Rotation(axis, angles[axis]);
                vm->draw_matrix = Mat4_Multiply(&vm->draw_matrix, &rotation);
            }
        }
        vm->rotation_changed = false;
    }
    MAT4 world = vm->draw_matrix;
    world.m[3][0] = (vm->anchor & 1) == 0
        ? vm->pos.x
        : fabsf(vm->sprite_data->width * vm->scale_x / 2.0f) + vm->pos.x;
    world.m[3][1] = (vm->anchor & 2) == 0
        ? vm->pos.y
        : fabsf(vm->sprite_data->height * vm->scale_y / 2.0f) + vm->pos.y;
    world.m[3][2] = vm->pos.z;
    return world;
}

static const GFX_VIEW *M_Camera(void)
{
    return &m_State.cameras[m_State.camera];
}

static VEC3 M_Project(const VEC3 v, const MAT4 *const world)
{
    const GFX_VIEW *const camera = M_Camera();
    return Vec3_Project(
        v, &camera->viewport, &camera->projection, &camera->view, world);
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
    const GFX_COLOR rgba = { (uint8_t)(color >> 16), (uint8_t)(color >> 8),
                             (uint8_t)color, (uint8_t)(color >> 24) };
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

void Anm_SetCameras(
    const GFX_VIEW *const fixed, const GFX_VIEW *const stage, const VEC3 right)
{
    m_State.cameras[ANM_CAMERA_FIXED] = *fixed;
    m_State.cameras[ANM_CAMERA_STAGE] = *stage;
    m_State.camera_right = right;
}

void Anm_SetCamera(const ANM_CAMERA camera)
{
    m_State.camera = camera;
}

void Anm_SetLastCamera(const int32_t camera)
{
    m_State.last_camera = camera;
}

void Anm_SetColorModulation(const uint32_t color)
{
    m_State.modulate = true;
    m_State.modulate_color = color;
}

void Anm_ResetColorModulation(void)
{
    m_State.modulate = false;
    m_State.modulate_color = 0x80808080u;
}

void Anm_SetShake(const float x, const float y)
{
    m_State.shake_x = x;
    m_State.shake_y = y;
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
    const float w = vm->sprite_data->width * vm->scale_x / 2.0f;
    const float h = vm->sprite_data->height * vm->scale_y / 2.0f;
    VEC3 positions[4];
    M_Rotated(positions, vm, w, h, sine, cosine, vm->pos);
    GFX_MOTION motion = {
        .center = {
            vm->pos.x + ((vm->anchor & 1) != 0 ? w : 0.0f),
            vm->pos.y + ((vm->anchor & 2) != 0 ? h : 0.0f),
        },
        .scale = { vm->scale_x, vm->scale_y },
        .rotation = -vm->rotation.z,
    };
    M_DrawQuad(vm, draw, positions, false, &motion);
}

void Anm_DrawProjected(const ANM_VM *const vm, const ANM_DRAW *const draw)
{
    if (!M_Visible(vm)) {
        return;
    }
    float sine, cosine;
    X87_SinCos(vm->rotation.z, &sine, &cosine);
    MAT4 world = Mat4_Identity();
    world.m[3][0] = vm->pos.x;
    world.m[3][1] = vm->pos.y;
    world.m[3][2] = vm->pos.z;
    const VEC3 center = M_Project((VEC3) {}, &world);
    if (center.z < 0.0f || center.z > 1.0f) {
        return;
    }
    const VEC3 edge = M_Project(m_State.camera_right, &world);
    const float unit = Vec3_Length(Vec3_Sub(edge, center)) * 0.5f;
    const float w = unit * vm->sprite_data->width * vm->scale_x;
    const float h = unit * vm->sprite_data->height * vm->scale_y;
    VEC3 positions[4];
    M_Rotated(positions, vm, w, h, sine, cosine, center);
    M_DrawQuad(vm, draw, positions, false, nullptr);
}

void Anm_DrawProjected3D(ANM_VM *const vm, const ANM_DRAW *const draw)
{
    if (!M_Visible(vm)) {
        return;
    }
    const MAT4 world = M_World(vm);
    VEC3 positions[4];
    for (int32_t i = 0; i < 4; i++) {
        positions[i] = M_Project(m_Quad[i], &world);
    }
    M_DrawQuad(vm, draw, positions, false, nullptr);
}

void Anm_Draw3D(ANM_VM *const vm, const ANM_DRAW *const draw)
{
    if (!M_Visible(vm)) {
        return;
    }
    MAT4 world = M_World(vm);
    world.m[3][0] += m_State.shake_x;
    world.m[3][1] += m_State.shake_y;
    const ANM_CAMERA camera =
        vm->stage_camera ? ANM_CAMERA_STAGE : ANM_CAMERA_FIXED;
    if (m_State.last_camera != (int32_t)camera) {
        m_State.last_camera = (int32_t)camera;
        m_State.camera = camera;
    }
    VEC3 positions[4];
    for (int32_t i = 0; i < 4; i++) {
        positions[i] = Vec3_TransformCoord(m_Quad[i], &world);
    }
    const VEC2 uv0 = {
        vm->sprite_data->u0 + vm->uv_scroll_x,
        vm->sprite_data->v0 + vm->uv_scroll_y,
    };
    const VEC2 uv1 = {
        vm->size_matrix.m[0][0] + uv0.x,
        vm->size_matrix.m[1][1] + uv0.y,
    };
    M_Record(vm, draw, M_Camera(), positions, uv0, uv1, nullptr);
}
