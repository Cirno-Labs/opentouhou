// SPDX-License-Identifier: GPL-3.0-only

#include "th06/game_manager.h"

#include "core/math/common.h"
#include "core/math/x87.h"

void GameManager_SetUpCamera(const float extra_far)
{
    const VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    const float mid_x = viewport->width / 2.0f;
    const float half_height = viewport->height / 2.0f;
    const float aspect = viewport->width / viewport->height;
    const float fov = 0.5235988f;
    const float distance = half_height / X87_Tan(fov / 2.0f);
    const VEC3 up = { 0.0f, 1.0f, 0.0f };
    const VEC3 at = { mid_x + g_GameManager.camera.offset_x,
                      -half_height + g_GameManager.camera.offset_y, 0.0f };
    const VEC3 from = { mid_x, -half_height,
                        -distance * g_GameManager.camera.zoom };
    g_GameManager.camera.view = Mat4_LookAtLH(from, at, up);
    g_GameManager.camera.distance = Math_Abs(distance);
    g_GameManager.camera.projection =
        Mat4_PerspectiveFovLH(fov, aspect, 100.0f, 10000.0f + extra_far);
}

void GameManager_SetUpFixedCamera(const float extra_far)
{
    const VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    const float mid_x = viewport->width / 2.0f;
    const float half_height = viewport->height / 2.0f;
    const float aspect = viewport->width / viewport->height;
    const float fov = 0.5235988f;
    const float distance = half_height / X87_Tan(fov / 2.0f);
    const VEC3 up = { 0.0f, 1.0f, 0.0f };
    const VEC3 at = { mid_x, -half_height, 0.0f };
    const VEC3 from = { mid_x, -half_height, -distance };
    g_GameManager.camera.view = Mat4_LookAtLH(from, at, up);
    g_GameManager.camera.distance = Math_Abs(distance);
    g_GameManager.camera.projection =
        Mat4_PerspectiveFovLH(fov, aspect, 100.0f, 10000.0f + extra_far);
}

ANM_DRAW GameManager_GetDrawContext(void)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = (GFX_VIEW) {
        .view = g_GameManager.camera.view,
        .projection = g_GameManager.camera.projection,
        .viewport = g_GameManager.camera.viewport,
        .id = ANM_VIEW_CAMERA,
    };
    return draw;
}
