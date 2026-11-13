// SPDX-License-Identifier: GPL-3.0-only

#include "th06/stage.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/chain.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"

#include "core/base/memory.h"
#include "core/gfx/private.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_STD_SIZE 4096

typedef struct {
    uint8_t bytes[M_STD_SIZE];
    size_t size;
} M_STD;

static char *m_Directory;

static void M_Put(M_STD *const std, const size_t at, const void *data, size_t n)
{
    memcpy(&std->bytes[at], data, n);
    if (at + n > std->size) {
        std->size = at + n;
    }
}

static void M_PutI16(M_STD *const std, const size_t at, const int16_t v)
{
    M_Put(std, at, &v, 2);
}

static void M_PutI32(M_STD *const std, const size_t at, const int32_t v)
{
    M_Put(std, at, &v, 4);
}

static void M_PutF32(M_STD *const std, const size_t at, const float v)
{
    M_Put(std, at, &v, 4);
}

// Builds a data file with one object of one quad running background
// script 0, placed once, and the instructions given.
static void M_BuildStd(
    M_STD *const std, const STAGE_INSTR *const script, const int32_t count,
    const int16_t instance_object)
{
    *std = (M_STD) {};
    const size_t object = 0x494;
    const size_t instances = object + 0x1c + 28 + 4;
    const size_t instrs = instances + 2 * 16;
    M_PutI16(std, 0, 1);
    M_PutI16(std, 2, 1);
    M_PutI32(std, 4, (int32_t)instances);
    M_PutI32(std, 8, (int32_t)instrs);
    M_Put(std, 0x10, "\x8d\x67\x96\x82", 4); // 紅魔 in CP932
    M_Put(std, 0x90, "song", 4);
    M_Put(std, 0x290, "bgm/th06_01.mid", 15);
    M_PutI32(std, 0x490, (int32_t)object);
    std->bytes[object + 2] = 1; // layer
    M_PutI16(std, object + 0x1c, 0);
    M_PutI16(std, object + 0x1c + 2, 28);
    M_PutI16(std, object + 0x1c + 4, 0);
    M_PutF32(std, object + 0x1c + 20, 16.0f);
    M_PutI16(std, object + 0x1c + 28, -1);
    M_PutI16(std, object + 0x1c + 30, 4);
    M_PutI16(std, instances, instance_object);
    M_PutF32(std, instances + 4, 5.0f);
    M_PutI16(std, instances + 16, -1);
    for (int32_t i = 0; i < count; i++) {
        const size_t at = instrs + 20 * (size_t)i;
        M_PutI32(std, at, script[i].frame);
        M_PutI16(std, at + 4, script[i].opcode);
        M_PutF32(std, at + 8, script[i].args.x);
        M_PutF32(std, at + 12, script[i].args.y);
        M_PutF32(std, at + 16, script[i].args.z);
    }
}

static float M_Bits(const int32_t value)
{
    float f;
    memcpy(&f, &value, 4);
    return f;
}

static RESULT M_Setup(
    const char *const name, const STAGE_INSTR *const script,
    const int32_t count, const int16_t instance_object)
{
    m_Directory = TestDir_Prepare(name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    const int32_t background = AnmBuilder_BeginScript(&builder, 0);
    AnmBuilder_Instr(&builder, background, 3, 0, nullptr, 0);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, "data/stg1bg.anm", bytes, size);
    M_STD std;
    M_BuildStd(&std, script, count, instance_object);
    Th06Data_WriteFile(m_Directory, "data/stage1.std", std.bytes, std.size);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    return Stage_Register();
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

static const STAGE_INSTR m_Keyframes[] = {
    { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    { 10, STAGE_OP_KEYFRAME, { 10.0f, 20.0f, 30.0f } },
    { -1, STAGE_OP_KEYFRAME, { 10.0f, 20.0f, 30.0f } },
    { -1, -1, { 0.0f, 0.0f, 0.0f } },
};

TEST(registering_loads_the_data_file)
{
    CHECK(Result_Log(M_Setup("load", m_Keyframes, 4, 0), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(g_Stage.data.name, "紅魔");
    CHECK_EQ_STR(g_Stage.data.song_names[0], "song");
    CHECK_EQ_STR(g_Stage.data.song_paths[0], "bgm/th06_01.mid");
    CHECK_EQ_INT(g_Stage.data.object_count, 1);
    CHECK_EQ_INT(g_Stage.data.objects[0].layer, 1);
    CHECK_EQ_INT(g_Stage.data.objects[0].quad_count, 1);
    CHECK(g_Stage.data.objects[0].quads[0].width == 16.0f);
    CHECK_EQ_INT(g_Stage.data.instance_count, 1);
    CHECK(g_Stage.data.instances[0].pos.x == 5.0f);
    CHECK_EQ_INT(g_Stage.data.script_count, 4);
    CHECK(g_Stage.data.sprites[0].script == Anm_GetScript(0x300));
    M_Teardown();
}

TEST(the_camera_moves_between_keyframes)
{
    CHECK(Result_Log(M_Setup("camera", m_Keyframes, 4, 0), LOG_LEVEL_ERROR));
    for (int frame = 0; frame <= 5; frame++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK(g_Stage.camera.pos.x == 5.0f);
    CHECK(g_Stage.camera.pos.y == 10.0f);
    for (int frame = 6; frame <= 12; frame++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK(g_Stage.camera.pos.x == 10.0f);
    CHECK(g_Stage.camera.pos.z == 30.0f);
    M_Teardown();
}

TEST(a_wait_holds_the_clock_until_resumed)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_WAIT, { 0.0f, 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { -1, -1, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("wait", script, 4, 0), LOG_LEVEL_ERROR));
    for (int i = 0; i < 5; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK_EQ_INT(g_Stage.script.time.current, 1);
    g_Stage.script.resume = true;
    Chain_RunCalc(&g_Chain);
    CHECK(!g_Stage.script.resume);
    CHECK_EQ_INT(g_Stage.script.index, 2);
    M_Teardown();
}

TEST(the_fog_fades_to_the_new_fog)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_FOG_FADE, { M_Bits(4), 0.0f, 0.0f } },
        { 0, STAGE_OP_FOG, { M_Bits((int32_t)0xff8040c0), 100.0f, 300.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { -1, -1, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("fog", script, 5, 0), LOG_LEVEL_ERROR));
    // The first update stops at the keyframe it passes, so the fade
    // starts on the second, a quarter of the way.
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK(g_Stage.fog.current.near == 175.0f);
    CHECK(g_Stage.fog.current.far == 450.0f);
    CHECK_EQ_INT(g_Stage.fog.current.color, 0xff201030);
    for (int i = 0; i < 3; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK_EQ_INT(g_Stage.fog.frames, 0);
    CHECK_EQ_INT(g_Stage.fog.current.color, 0xff8040c0);
    M_Teardown();
}

TEST(objects_stop_animating_when_their_scripts_end)
{
    CHECK(Result_Log(M_Setup("objects", m_Keyframes, 4, 0), LOG_LEVEL_ERROR));
    // Starting the script ran its first frame, so it ends on the third
    // update.
    for (int i = 0; i < 2; i++) {
        Chain_RunCalc(&g_Chain);
        CHECK(g_Stage.data.objects[0].animating);
    }
    Chain_RunCalc(&g_Chain);
    CHECK(!g_Stage.data.objects[0].animating);
    M_Teardown();
}

TEST(a_spell_card_background_fades_in_over_sixty_frames)
{
    CHECK(
        Result_Log(M_Setup("spell_card", m_Keyframes, 4, 0), LOG_LEVEL_ERROR));
    g_Stage.spell_card.state = 1;
    for (int i = 0; i < 60; i++) {
        Chain_RunDraw(&g_Chain);
        Chain_RunCalc(&g_Chain);
        CHECK_EQ_INT(g_Stage.spell_card.state, 1);
    }
    Chain_RunDraw(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_Stage.spell_card.state, 2);
    M_Teardown();
}

TEST(a_frozen_game_only_tints_the_spell_card_background)
{
    CHECK(Result_Log(M_Setup("frozen", m_Keyframes, 4, 0), LOG_LEVEL_ERROR));
    g_GameManager.mode.frozen = true;
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_Stage.script.time.current, 0);
    CHECK_EQ_INT(g_Stage.spell_card.background.color, 0x60803030);
    M_Teardown();
}

TEST(a_keyframe_with_none_after_it_fails_the_load)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { -1, -1, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(!Result_Ignore(M_Setup("no_next_key", script, 2, 0)));
    CHECK(g_Stage.data.script == nullptr);
    M_Teardown();
}

TEST(a_script_that_runs_past_its_end_fails_the_load)
{
    const STAGE_INSTR script[] = {
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_FOG, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(!Result_Ignore(M_Setup("past_end", script, 2, 0)));
    M_Teardown();
}

TEST(an_instance_of_a_missing_object_fails_the_load)
{
    CHECK(!Result_Ignore(M_Setup("bad_instance", m_Keyframes, 4, 3)));
    M_Teardown();
}

TEST(drawing_restores_the_fixed_camera_and_foreground_depth)
{
    CHECK(Result_Log(
        M_Setup("fixed_camera", m_Keyframes, 4, 0), LOG_LEVEL_ERROR));
    g_GameManager.camera.viewport = (VIEWPORT) {
        .width = 384.0f,
        .height = 448.0f,
        .min_z = 0.5f,
        .max_z = 1.0f,
    };
    g_GameManager.camera.offset_x = 10.0f;
    g_GameManager.camera.offset_y = 20.0f;
    g_GameManager.camera.zoom = 2.0f;
    GameManager_SetUpCamera(0.0f);
    Chain_RunDraw(&g_Chain);
    CHECK(g_GameManager.camera.viewport.min_z == 0.0f);
    CHECK(g_GameManager.camera.viewport.max_z == 0.5f);
    CHECK(g_GameManager.camera.view.m[0][0] == 1.0f);
    CHECK(g_GameManager.camera.view.m[1][1] == 1.0f);
    CHECK(g_GameManager.camera.view.m[2][2] == 1.0f);
    CHECK(g_GameManager.camera.view.m[3][0] == -192.0f);
    CHECK(g_GameManager.camera.view.m[3][1] == 224.0f);
    CHECK(g_GameManager.camera.view.m[3][2] == g_GameManager.camera.distance);
    CHECK(g_GameManager.camera.distance > 800.0f);
    CHECK(g_GameManager.camera.distance < 900.0f);
    M_Teardown();
}

TEST(stage_layers_record_current_fog_and_instance_geometry)
{
    const STAGE_INSTR script[] = { { -1, STAGE_OP_KEYFRAME, { 0, 0, 0 } } };
    CHECK(Result_Ignore(M_Setup("stage_draw_layer", script, 1, 0)));
    const ANM_SPRITE sprite = {
        .slot = 4, .width = 4.0f, .height = 4.0f, .u1 = 1.0f, .v1 = 1.0f
    };
    CHECK(Result_Ignore(Anm_CreateTexture(4, 4, 4, ANM_TEXTURE_RGBA8)));
    ANM_VM *const vm = &g_Stage.data.sprites[0];
    vm->sprite_data = &sprite;
    vm->shown = true;
    vm->visible = true;
    vm->color = 0xffffffffu;
    vm->uv_scale_x = 1.0f;
    vm->uv_scale_y = 1.0f;
    g_GameManager.camera.viewport = (VIEWPORT) { 32, 16, 384, 448, 0.5f, 1.0f };
    g_GameManager.camera.view = Mat4_Identity();
    g_GameManager.camera.projection = Mat4_Identity();
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    const GFX_FOG *const fog = &GfxRecorder_Quad(0)->view.fog;
    CHECK(fog->enabled);
    CHECK(fog->begin == 200.0f);
    CHECK(fog->end == 500.0f);
    CHECK(vm->pos.x == 5.0f);
    CHECK(vm->scale_x == 4.0f);
    CHECK(g_GameManager.camera.viewport.min_z == 0.0f);
    CHECK(g_GameManager.camera.viewport.max_z == 0.5f);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(repeated_stage_instances_keep_separate_draw_parts)
{
    const STAGE_INSTR script[] = { { -1, STAGE_OP_KEYFRAME, { 0, 0, 0 } } };
    CHECK(IS_OK(M_Setup("stage_instance_presentation", script, 1, 0)));
    const ANM_SPRITE sprite = { .slot = 4, .width = 4, .height = 4 };
    CHECK(IS_OK(Anm_CreateTexture(4, 4, 4, ANM_TEXTURE_RGBA8)));
    ANM_VM *const vm = &g_Stage.data.sprites[0];
    vm->sprite_data = &sprite;
    vm->shown = true;
    vm->visible = true;
    vm->color = 0xffffffff;
    vm->uv_scale_x = vm->uv_scale_y = 1;
    g_Stage.data.instances = Mem_ReallocArr(
        g_Stage.data.instances, 2, sizeof(*g_Stage.data.instances));
    g_Stage.data.instances[1] = g_Stage.data.instances[0];
    g_Stage.data.instances[1].pos.x += 32;
    g_Stage.data.instance_count = 2;
    g_GameManager.camera.viewport = (VIEWPORT) { 32, 16, 384, 448, 0.5f, 1 };
    g_GameManager.camera.view = Mat4_Identity();
    g_GameManager.camera.projection = Mat4_Identity();
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    const uint64_t first = GfxRecorder_QuadId(0);
    const uint64_t second = GfxRecorder_QuadId(1);
    CHECK(first != 0 && second != 0);
    CHECK_EQ_INT(first >> 32, second >> 32);
    CHECK_EQ_INT((uint32_t)first, 0);
    CHECK_EQ_INT((uint32_t)second, 1 << 16);
    CHECK(
        GfxRecorder_Quad(1)->vertices[0].position.x
        == GfxRecorder_Quad(0)->vertices[0].position.x + 32);
    GfxRecorder_Stop();
    M_Teardown();
}
