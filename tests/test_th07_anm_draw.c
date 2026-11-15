#include "th07/anm.h"

#include "gfx_recorder.h"
#include "test.h"

#include "core/base/memory.h"
#include "core/math/x87.h"

#include "stg/rng.h"

static RNG m_Rng;

// Sets vm up on a 64 x 16 sprite of a 256 x 64 texture.
static void M_Setup(ANM_VM *const vm)
{
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &m_Rng);
    CHECK(Result_Log(
        Anm_CreateTexture(0, 256, 64, ANM_TEXTURE_RGBA8), LOG_LEVEL_ERROR));
    const ANM_SPRITE sprite = {
        .slot = 0,
        .x1 = 64.0f,
        .y1 = 16.0f,
        .texture_width = 256.0f,
        .texture_height = 64.0f,
        .u0 = 0.125f,
        .u1 = 0.375f,
        .v1 = 0.25f,
        .width = 64.0f,
        .height = 16.0f,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
    };
    Anm_SetSpriteEntry(3, &sprite);
    Anm_CreateVm(vm);
    Anm_InitVm(vm);
    Anm_SetSprite(vm, 3);
    Anm_SetShake(0.0f, 0.0f);
    Anm_ResetColorModulation();
    Anm_SetLastCamera(-1);
    Gfx_BeginTick((GFX_COLOR) {});
}

static void M_Teardown(void)
{
    GfxRecorder_Stop();
    Anm_Shutdown();
}

TEST(unrotated_sprites_round_after_the_shake)
{
    ANM_VM vm;
    M_Setup(&vm);
    vm.pos = (VEC3) { 100.25f, 50.75f, 0.5f };
    vm.uv_scroll_x = 0.125f;
    const ANM_DRAW draw = Anm_ScreenDraw();
    Anm_DrawUnrotated(&vm, &draw);
    Anm_SetShake(1.5f, 0.0f);
    Anm_DrawUnrotated(&vm, &draw);
    Anm_DrawNoRound(&vm, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 3);
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(0);
    CHECK(quad->vertices[0].position.x == 68.0f);
    CHECK(quad->vertices[0].position.y == 43.0f);
    CHECK(quad->vertices[2].position.x == 132.0f);
    CHECK(quad->vertices[2].position.y == 59.0f);
    CHECK(quad->vertices[0].uv.x == 0.25f);
    CHECK(quad->vertices[2].uv.x == 0.5f);
    CHECK(GfxRecorder_Quad(1)->vertices[0].position.x == 70.0f);
    CHECK(GfxRecorder_Quad(2)->vertices[0].position.x == 70.25f);
    M_Teardown();
}

TEST(sprites_outside_the_viewport_are_skipped)
{
    ANM_VM vm;
    M_Setup(&vm);
    const ANM_DRAW draw = Anm_ScreenDraw();
    vm.pos = (VEC3) { -40.0f, 50.0f, 0.0f };
    Anm_DrawNoRound(&vm, &draw);
    vm.pos.x = -31.0f;
    Anm_DrawNoRound(&vm, &draw);
    vm.color &= 0x00ffffffu;
    Anm_DrawNoRound(&vm, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    M_Teardown();
}

TEST(colors_modulate_and_color2_replaces_color)
{
    ANM_VM vm;
    M_Setup(&vm);
    const ANM_DRAW draw = Anm_ScreenDraw();
    vm.pos = (VEC3) { 100.0f, 100.0f, 0.0f };
    vm.color = 0x80402010u;
    Anm_SetColorModulation(0x80ff4080u);
    Anm_DrawNoRound(&vm, &draw);
    Anm_ResetColorModulation();
    vm.color2 = 0x11223344u;
    vm.use_color2 = true;
    Anm_DrawNoRound(&vm, &draw);
    Gfx_EndTick();
    const GFX_COLOR modulated = GfxRecorder_Quad(0)->vertices[0].color;
    CHECK_EQ_INT(modulated.alpha, 0x80);
    CHECK_EQ_INT(modulated.red, 0x7f);
    CHECK_EQ_INT(modulated.green, 0x10);
    CHECK_EQ_INT(modulated.blue, 0x10);
    const GFX_COLOR second = GfxRecorder_Quad(1)->vertices[0].color;
    CHECK_EQ_INT(second.alpha, 0x11);
    CHECK_EQ_INT(second.blue, 0x44);
    M_Teardown();
}

TEST(rotated_sprites_turn_the_other_way_from_th06)
{
    ANM_VM vm;
    M_Setup(&vm);
    const ANM_DRAW draw = Anm_ScreenDraw();
    vm.pos = (VEC3) { 100.0f, 100.0f, 0.0f };
    vm.rotation.z = 0.5f;
    Anm_Draw(&vm, &draw);
    float s, c;
    X87_SinCos(0.5f, &s, &c);
    Gfx_EndTick();
    const GFX_VERTEX *const top_right = &GfxRecorder_Quad(0)->vertices[1];
    CHECK(top_right->position.x == 32.0f * c - -8.0f * s + 100.0f + 0.5f);
    CHECK(top_right->position.y == 32.0f * s + -8.0f * c + 100.0f + 0.5f);
    M_Teardown();
}

TEST(projected_sprites_scale_by_the_right_vector_and_reject_depth)
{
    ANM_VM vm;
    M_Setup(&vm);
    GFX_VIEW camera = {
        .view = Mat4_Identity(),
        .projection = Mat4_Identity(),
        .viewport = { 0, 0, 512, 480, 0, 1 },
    };
    GFX_VIEW stage = camera;
    stage.projection.m[0][0] = 2.0f;
    Anm_SetCameras(&camera, &stage, (VEC3) { 0.03125f, 0.0f, 0.0f });
    Anm_SetCamera(ANM_CAMERA_FIXED);
    const ANM_DRAW draw = Anm_ScreenDraw();
    vm.pos = (VEC3) { 0.5f, 0.5f, 0.25f };
    Anm_DrawProjected(&vm, &draw);
    vm.pos.z = 1.5f;
    Anm_DrawProjected(&vm, &draw);
    vm.pos.z = -0.25f;
    Anm_DrawProjected(&vm, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    const GFX_VERTEX *const top_left = &GfxRecorder_Quad(0)->vertices[0];
    CHECK(top_left->position.x == 128.5f);
    CHECK(top_left->position.y == 56.5f);
    CHECK(top_left->position.z == 0.25f);
    M_Teardown();
}

TEST(draw_3d_sets_up_the_camera_of_its_flag)
{
    ANM_VM vm;
    M_Setup(&vm);
    GFX_VIEW camera = {
        .view = Mat4_Identity(),
        .projection = Mat4_Identity(),
        .viewport = { 0, 0, 512, 480, 0, 1 },
    };
    GFX_VIEW stage = camera;
    stage.projection.m[0][0] = 2.0f;
    camera.id = ANM_VIEW_CAMERA;
    stage.id = ANM_VIEW_STAGE;
    Anm_SetCameras(&camera, &stage, (VEC3) { 0.03125f, 0.0f, 0.0f });
    Anm_SetCamera(ANM_CAMERA_FIXED);
    const ANM_DRAW draw = Anm_ScreenDraw();
    vm.pos = (VEC3) { 0.5f, 0.5f, 0.25f };
    vm.scale_x = 2.0f;
    vm.scale_changed = true;
    vm.stage_camera = true;
    Anm_Draw3D(&vm, &draw);
    CHECK(!vm.scale_changed && !vm.rotation_changed);
    CHECK(vm.draw_matrix.m[0][0] == 0.5f);
    // The stage camera stays current for the projected sprite after it.
    Anm_DrawProjected(&vm, &draw);
    // The flag matches the last 3D sprite's, so the fixed camera stays.
    Anm_SetCamera(ANM_CAMERA_FIXED);
    Anm_Draw3D(&vm, &draw);
    Anm_SetLastCamera(-1);
    Anm_Draw3D(&vm, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 4);
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(0);
    const float fixed = GfxRecorder_Quad(2)->view.projection.m[0][0];
    CHECK(quad->view.projection.m[0][0] == 2.0f * fixed);
    CHECK(quad->vertices[0].position.x == -128.0f * 0.5f + 0.5f);
    CHECK(quad->vertices[0].uv.x == 0.125f);
    CHECK(quad->vertices[1].uv.x == 0.25f + 0.125f);
    CHECK(
        GfxRecorder_Quad(1)->vertices[0].position.x == 512.0f - 1024.0f + 0.5f);
    CHECK(GfxRecorder_Quad(3)->view.projection.m[0][0] == 2.0f * fixed);
    M_Teardown();
}

TEST(strips_draw_a_quad_per_vertex_pair)
{
    ANM_VM vm;
    M_Setup(&vm);
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.interpolate = true;
    draw.part = 5;
    vm.blend_add = true;
    ANM_STRIP_VERTEX vertices[6];
    for (int32_t i = 0; i < 6; i++) {
        vertices[i] = (ANM_STRIP_VERTEX) {
            { 10.0f * (float)(i / 2), 20.0f * (float)(i % 2), 0.5f },
            0x40000000u | (uint32_t)i,
            { (float)i, 0.0f },
        };
    }
    Anm_DrawStrip(&vm, vertices, 6, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    const GFX_RECORDED_QUAD *const second = GfxRecorder_Quad(1);
    CHECK(second->state.blend == GFX_BLEND_ADD);
    CHECK(second->vertices[0].position.x == 10.5f);
    CHECK(second->vertices[0].position.y == 0.5f);
    CHECK(second->vertices[1].uv.x == 3.0f);
    CHECK(second->vertices[2].uv.x == 5.0f);
    CHECK(second->vertices[3].uv.x == 4.0f);
    CHECK_EQ_INT(second->vertices[2].color.blue, 5);
    CHECK_EQ_INT(second->vertices[2].color.alpha, 0x40);
    M_Teardown();
}

TEST(strips_of_hidden_or_transparent_sprites_are_skipped)
{
    ANM_VM vm;
    M_Setup(&vm);
    const ANM_DRAW draw = Anm_ScreenDraw();
    const ANM_STRIP_VERTEX vertices[4] = {};
    vm.visible = false;
    Anm_DrawStrip(&vm, vertices, 4, &draw);
    vm.visible = true;
    vm.color &= 0x00ffffffu;
    Anm_DrawStrip(&vm, vertices, 4, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    M_Teardown();
}
