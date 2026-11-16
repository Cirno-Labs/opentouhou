#include "th07/stage.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"
#include "th07_scene_stubs.h"

#include "th07/anm.h"
#include "th07/effect.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"

#include "core/base/color.h"
#include "core/base/memory.h"

#include "stg/chain.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_STD_SIZE 4096
#define M_STAGE_SPRITE 0x300
#define M_PETAL_SCRIPT 0x2bf
#define M_PETAL_SPRITE 0x2d8

typedef struct {
    uint8_t bytes[M_STD_SIZE];
    size_t size;
} M_STD;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
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

static float M_Bits(const int32_t value)
{
    float f;
    memcpy(&f, &value, 4);
    return f;
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
    M_Put(std, 0x10, "\x97\x64\x81\x58\x96\xb2", 6); // 妖々夢 in CP932
    M_Put(std, 0x90, "song", 4);
    M_Put(std, 0x290, "th07_01.wav", 11);
    M_PutI32(std, 0x490, (int32_t)object);
    std->bytes[object + 2] = 1; // layer
    M_PutI16(std, object + 0x1c, 0);
    M_PutI16(std, object + 0x1c + 2, 28);
    M_PutI16(std, object + 0x1c + 4, 0);
    M_PutF32(std, object + 0x1c + 8, 4.0f);
    M_PutF32(std, object + 0x1c + 20, 32.0f);
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

// Writes the ANM at name: script 0 shows a 16 x 16 sprite and stops on
// its third frame.
static void M_WriteAnm(const char *const directory, const char *const name)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    const int32_t script = AnmBuilder_BeginScript(anm, 0);
    AnmBuilder_Instr(anm, script, 3, 0, 0, (uint32_t[]) { 0 }, 1);
    AnmBuilder_Instr(anm, script, 2, 2, 0, nullptr, 0);
    AnmBuilder_End(anm, script);
    AnmBuilder_WriteFile(&builder, directory, name);
}

static RESULT M_Setup(
    const char *const name, const STAGE_INSTR *const script,
    const int32_t count, const int16_t instance_object)
{
    m_Directory = TestDir_Prepare(name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    M_WriteAnm(m_Directory, "data/stg1bg.anm");
    M_WriteAnm(m_Directory, "data/eff01.anm");
    M_STD std;
    M_BuildStd(&std, script, count, instance_object);
    Th07Data_WriteFile(m_Directory, "data/stage1.std", std.bytes, std.size);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Anm_ResetColorModulation();
    Chain_Init(&g_Chain);
    SceneStubCalls_Reset();
    g_GameSpeed = 1.0f;
    g_GameManager.run.stage = 1;
    g_GameManager.camera.viewport = (VIEWPORT) { 32, 16, 384, 448, 0, 1 };
    // Resets the effects, so no petal of an earlier test is drawn.
    MUST(Effect_Register());
    return Stage_Register();
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

// Starts a still petal 100 in front of the eye of m_Camera, drawn with the
// stage's sprite.
static EFFECT *M_StartPetal(void)
{
    Anm_SetScriptEntry(M_PETAL_SCRIPT, Anm_GetScript(M_STAGE_SPRITE));
    Anm_SetSpriteEntry(M_PETAL_SPRITE, Anm_GetSpriteEntry(M_STAGE_SPRITE));
    EFFECT *const petal =
        Effect_Spawn(EFFECT_PETAL_FALLING, (VEC3) {}, 1, COLOR_WHITE);
    petal->true_pos = (VEC3) { 0.0f, 0.0f, -100.0f };
    petal->step = (VEC3) {};
    petal->acceleration = (VEC3) {};
    return petal;
}

static void M_Run(const int32_t updates)
{
    for (int32_t i = 0; i < updates; i++) {
        Chain_RunCalc(&g_Chain);
    }
}

static const STAGE_INSTR m_Keyframes[] = {
    { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    { 10, STAGE_OP_KEYFRAME, { 10.0f, 20.0f, 30.0f } },
    { -1, STAGE_OP_KEYFRAME, { 10.0f, 20.0f, 30.0f } },
};

TEST(registering_loads_the_data_file)
{
    CHECK(Result_Log(M_Setup("load", m_Keyframes, 3, 0), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(g_Stage.data.name, "妖々夢");
    CHECK_EQ_STR(g_Stage.data.song_names[0], "song");
    CHECK_EQ_STR(g_Stage.data.song_paths[0], "th07_01.wav");
    CHECK_EQ_INT(g_Stage.data.objects[0].layer, 1);
    CHECK_EQ_INT(g_Stage.data.objects[0].quad_count, 1);
    CHECK(g_Stage.data.objects[0].quads[0].width == 32.0f);
    CHECK(g_Stage.data.instances[0].pos.x == 5.0f);
    CHECK_EQ_INT(g_Stage.data.script_count, 3);
    CHECK(g_Stage.data.sprites[0].script == Anm_GetScript(0x300));
    CHECK(g_Stage.camera.current.eye.z == 1000.0f);
    CHECK(g_Stage.camera.start.fov == 0.5235988f);
    CHECK(g_Stage.fog.current.color == COLOR_ARGB(0xff, 0x00, 0x00, 0x00));
    M_Teardown();
}

TEST(keyframes_move_the_camera_in_jumps)
{
    CHECK(Result_Log(M_Setup("camera", m_Keyframes, 3, 0), LOG_LEVEL_ERROR));
    M_Run(10);
    CHECK(g_Stage.camera.pos.x == 0.0f);
    CHECK(g_Stage.camera.next_key_pos.y == 20.0f);
    CHECK_EQ_INT(g_Stage.camera.next_key_frame, 10);
    M_Run(1);
    CHECK(g_Stage.camera.pos.x == 10.0f);
    CHECK(g_Stage.camera.pos.z == 30.0f);
    CHECK_EQ_INT(g_Stage.camera.next_key_frame, -1);
    M_Teardown();
}

TEST(a_label_restarts_the_script_after_it)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_WAIT, { 0.0f, 0.0f, 0.0f } },
        { 50, STAGE_OP_LABEL, { M_Bits(3), 0.0f, 0.0f } },
        { 50, STAGE_OP_LABEL, { M_Bits(7), 0.0f, 0.0f } },
        { 50, STAGE_OP_CLEAR_COLOR, { M_Bits(0x204060), 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("label", script, 6, 0), LOG_LEVEL_ERROR));
    M_Run(5);
    CHECK_EQ_INT(g_Stage.script.time.current, 0);
    CHECK_EQ_INT(g_Stage.script.index, 1);
    g_Stage.script.label = 7;
    M_Run(1);
    CHECK_EQ_INT(g_Stage.script.label, 0);
    CHECK_EQ_INT(g_Stage.script.index, 5);
    CHECK_EQ_INT(g_Stage.script.time.current, 51);
    CHECK_EQ_INT(g_Stage.clear_color, 0x204060);
    M_Teardown();
}

TEST(a_label_the_script_lacks_passes_the_next_wait)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_WAIT, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_WAIT, { 0.0f, 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("no_label", script, 4, 0), LOG_LEVEL_ERROR));
    M_Run(1);
    g_Stage.script.label = 9;
    M_Run(1);
    CHECK_EQ_INT(g_Stage.script.label, 0);
    CHECK_EQ_INT(g_Stage.script.index, 2);
    CHECK_EQ_INT(g_Stage.script.time.current, 0);
    M_Teardown();
}

TEST(a_wait_not_yet_due_holds_the_clock)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 5, STAGE_OP_WAIT, { 0.0f, 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("early_wait", script, 3, 0), LOG_LEVEL_ERROR));
    M_Run(10);
    CHECK_EQ_INT(g_Stage.script.time.current, 0);
    CHECK_EQ_INT(g_Stage.script.index, 1);
    M_Teardown();
}

TEST(a_jump_moves_the_petals_with_the_next_eye)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_EYE, { 10.0f, 0.0f, 0.0f } },
        { 3, STAGE_OP_JUMP, { M_Bits(3), M_Bits(2), 0.0f } },
        { 2, STAGE_OP_EYE, { 25.0f, 4.0f, 0.0f } },
        { 2, STAGE_OP_EYE, { 30.0f, 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("jump", script, 6, 0), LOG_LEVEL_ERROR));
    const EFFECT *const petal = M_StartPetal();
    M_Run(3);
    CHECK(petal->true_pos.x == 0.0f);
    M_Run(1);
    CHECK(petal->true_pos.x == 15.0f);
    CHECK(petal->true_pos.y == 4.0f);
    CHECK(petal->true_pos.z == -100.0f);
    CHECK(g_Stage.camera.current.eye.x == 30.0f);
    CHECK(g_Stage.camera.start.eye.x == 25.0f);
    CHECK_EQ_INT(g_Stage.script.time.current, 3);
    M_Teardown();
}

TEST(an_eye_fade_eases_from_the_last_eye_to_the_new_one)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_EYE_FADE, { M_Bits(4), M_Bits(STAGE_EASE_OUT2), 0.0f } },
        { 0, STAGE_OP_EYE, { 100.0f, 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("eye_fade", script, 4, 0), LOG_LEVEL_ERROR));
    M_Run(1);
    CHECK(g_Stage.camera.current.eye.x == 43.75f);
    CHECK(g_Stage.camera.current.eye.z == 562.5f);
    M_Run(2);
    CHECK(g_Stage.camera.current.eye.x == 93.75f);
    M_Run(1);
    CHECK(g_Stage.camera.current.eye.x == 100.0f);
    CHECK_EQ_INT(g_Stage.camera.frames[STAGE_FADE_EYE], 4);
    M_Run(1);
    CHECK_EQ_INT(g_Stage.camera.frames[STAGE_FADE_EYE], 0);
    M_Teardown();
}

TEST(a_hermite_fade_follows_its_tangents)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_AT_START, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_AT_END, { 10.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_AT_START_TANGENT, { 8.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_AT_END_TANGENT, { 4.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_AT_HERMITE, { M_Bits(4), 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("hermite", script, 7, 0), LOG_LEVEL_ERROR));
    M_Run(1);
    CHECK(g_Stage.camera.current.at.x == 2.5f);
    CHECK(g_Stage.camera.current.dir.x == 1.0f);
    M_Teardown();
}

TEST(a_fov_fade_runs_on_the_game_speed)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_FOV_FADE, { M_Bits(2), M_Bits(STAGE_EASE_IN2), 0.0f } },
        { 0, STAGE_OP_FOV, { 1.0f, 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("fov_fade", script, 4, 0), LOG_LEVEL_ERROR));
    g_Stage.camera.end.fov = 0.0f;
    g_GameSpeed = 0.5f;
    M_Run(1);
    CHECK(g_Stage.camera.current.fov == 0.0625f);
    CHECK_EQ_INT(g_Stage.script.time.current, 0);
    M_Run(1);
    CHECK(g_Stage.camera.current.fov == 0.25f);
    CHECK_EQ_INT(g_Stage.script.time.current, 1);
    M_Teardown();
}

TEST(the_fog_fades_to_the_new_fog)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_FOG_FADE, { M_Bits(4), 0.0f, 0.0f } },
        { 0,
          STAGE_OP_FOG,
          { M_Bits((int32_t)COLOR_ARGB(0xff, 0x80, 0x40, 0xc0)), 100.0f,
            300.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("fog", script, 4, 0), LOG_LEVEL_ERROR));
    M_Run(1);
    CHECK(g_Stage.fog.current.near == 175.0f);
    CHECK(g_Stage.fog.current.far == 450.0f);
    CHECK_EQ_INT(g_Stage.fog.current.color, COLOR_ARGB(0xff, 0x20, 0x10, 0x30));
    M_Run(3);
    CHECK_EQ_INT(g_Stage.fog.frames, 0);
    CHECK_EQ_INT(g_Stage.fog.current.color, COLOR_ARGB(0xff, 0x80, 0x40, 0xc0));
    M_Teardown();
}

TEST(the_fog_fade_runs_on_the_game_speed)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_FOG_FADE, { M_Bits(4), 0.0f, 0.0f } },
        { 0,
          STAGE_OP_FOG,
          { M_Bits((int32_t)COLOR_ARGB(0xff, 0x00, 0x00, 0x00)), 100.0f,
            300.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("fog_speed", script, 4, 0), LOG_LEVEL_ERROR));
    g_GameSpeed = 0.5f;
    M_Run(1);
    CHECK(g_Stage.fog.current.near == 187.5f);
    M_Teardown();
}

TEST(objects_stop_animating_when_their_scripts_end)
{
    CHECK(Result_Log(M_Setup("objects", m_Keyframes, 3, 0), LOG_LEVEL_ERROR));
    // Starting the script ran its first frame, so it stops on the second
    // update.
    M_Run(1);
    CHECK(g_Stage.data.objects[0].animating);
    M_Run(1);
    CHECK(!g_Stage.data.objects[0].animating);
    M_Teardown();
}

TEST(a_negative_layer_script_hides_layer_zero)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_LAYER0, { M_Bits(0), 0.0f, 0.0f } },
        { 0, STAGE_OP_LAYER1, { M_Bits(0), 0.0f, 0.0f } },
        { 1, STAGE_OP_LAYER1, { M_Bits(-1), 0.0f, 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("layers", script, 5, 0), LOG_LEVEL_ERROR));
    M_Run(1);
    CHECK_EQ_INT(g_Stage.layers[0].sprite, 0x300);
    CHECK_EQ_INT(g_Stage.layers[1].sprite, 0x300);
    M_Run(1);
    CHECK_EQ_INT(g_Stage.layers[0].sprite, -1);
    CHECK_EQ_INT(g_Stage.layers[1].sprite, 0x300);
    M_Teardown();
}

TEST(a_spell_card_runs_its_sprites_and_fades_in_over_sixty_frames)
{
    CHECK(
        Result_Log(M_Setup("spell_card", m_Keyframes, 3, 0), LOG_LEVEL_ERROR));
    Anm_StartScript(&g_Stage.spell_background.sprites[0], 0x300);
    g_Stage.spell_background.count = 1;
    g_Stage.spell_card.state = 1;
    for (int i = 0; i < 60; i++) {
        Chain_RunDraw(&g_Chain);
        M_Run(1);
        CHECK_EQ_INT(g_Stage.spell_card.state, 1);
    }
    CHECK(g_Stage.spell_background.sprites[0].pc < 0);
    M_Run(1);
    CHECK_EQ_INT(g_Stage.spell_card.state, 2);
    M_Teardown();
}

TEST(set_tint_averages_with_a_tint_already_set)
{
    CHECK(Result_Log(M_Setup("tint", m_Keyframes, 3, 0), LOG_LEVEL_ERROR));
    Stage_SetTint(COLOR_ARGB(0x40, 0xff, 0x20, 0x10));
    CHECK_EQ_INT(g_Stage.tint.color, COLOR_ARGB(0x40, 0xff, 0x20, 0x10));
    Stage_SetTint(COLOR_ARGB(0xc0, 0x01, 0x30, 0x40));
    CHECK_EQ_INT(g_Stage.tint.color, COLOR_ARGB(0x80, 0x80, 0x28, 0x28));
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_Stage.tint.color, COLOR_ARGB(0x00, 0x80, 0x80, 0x80));
    M_Teardown();
}

TEST(a_keyframe_with_none_after_it_fails_the_load)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { -1, STAGE_OP_FOG, { 0.0f, 0.0f, 0.0f } },
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

TEST(a_script_may_end_on_a_jump)
{
    const STAGE_INSTR script[] = {
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_JUMP, { M_Bits(1), M_Bits(0), 0.0f } },
    };
    CHECK(Result_Log(M_Setup("end_jump", script, 2, 0), LOG_LEVEL_ERROR));
    M_Teardown();
}

TEST(a_jump_out_of_the_script_fails_the_load)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_JUMP, { M_Bits(2), M_Bits(0), 0.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(!Result_Ignore(M_Setup("bad_jump", script, 2, 0)));
    M_Teardown();
}

TEST(an_instance_of_a_missing_object_fails_the_load)
{
    CHECK(!Result_Ignore(M_Setup("bad_instance", m_Keyframes, 3, 3)));
    M_Teardown();
}

static const STAGE_INSTR m_Camera[] = {
    { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    { 0, STAGE_OP_EYE, { 0.0f, 0.0f, -200.0f } },
    { 0, STAGE_OP_AT, { 0.0f, 0.0f, 1.0f } },
    { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
};

TEST(stage_layers_draw_through_the_stage_camera_in_fog)
{
    CHECK(Result_Log(M_Setup("draw", m_Camera, 4, 0), LOG_LEVEL_ERROR));
    g_Stage.data.sprites[0].stage_camera = true;
    Stage_SetTint(COLOR_ARGB(0xff, 0x40, 0x40, 0x40));
    M_StartPetal();
    M_Run(1);
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2); // the layer, then the petal
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(0);
    CHECK(quad->view.view.m[3][2] == 200.0f);
    CHECK(quad->view.fog.enabled);
    CHECK(quad->view.fog.begin == 200.0f);
    CHECK(quad->view.fog.end == 500.0f);
    CHECK_EQ_INT(quad->view.fog.color.alpha, 0xff);
    CHECK_EQ_INT(quad->vertices[0].color.red, 0x7f);
    CHECK(g_Stage.data.sprites[0].pos.x == 9.0f);
    CHECK(g_Stage.data.sprites[0].scale_x == 2.0f);
    CHECK(g_Stage.camera.current.right.x == -1.0f);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(objects_behind_the_camera_are_not_drawn)
{
    CHECK(Result_Log(M_Setup("behind", m_Camera, 4, 0), LOG_LEVEL_ERROR));
    M_Run(1);
    g_Stage.camera.current.dir.z = -1.0f;
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(the_loading_sprite_hides_the_layers)
{
    CHECK(Result_Log(M_Setup("loading", m_Camera, 4, 0), LOG_LEVEL_ERROR));
    M_StartPetal();
    M_Run(1);
    g_SceneStubCalls.loading_shown = true;
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(projected_sprites_fade_into_the_fog_by_hand)
{
    const STAGE_INSTR script[] = {
        { 0, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
        { 0, STAGE_OP_EYE, { 9.0f, 0.0f, -200.0f } },
        { 0, STAGE_OP_AT, { 0.0f, 0.0f, 1.0f } },
        { -1, STAGE_OP_KEYFRAME, { 0.0f, 0.0f, 0.0f } },
    };
    CHECK(Result_Log(M_Setup("projected", script, 4, 0), LOG_LEVEL_ERROR));
    ANM_VM *const vm = &g_Stage.data.sprites[0];
    vm->auto_rotate = 2;
    M_Run(1);
    g_Stage.fog.current.near = 0.0f;
    g_Stage.fog.current.far = 400.0f;
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(0);
    CHECK(!quad->view.fog.enabled);
    CHECK_EQ_INT(quad->vertices[0].color.red, 0x80);
    CHECK_EQ_INT(quad->vertices[0].color.alpha, 0x7f);
    CHECK_EQ_INT(vm->color, COLOR_WHITE);
    CHECK(vm->pos.x == 224.0f);
    CHECK(vm->scale_x == vm->scale_y);
    GfxRecorder_Stop();
    M_Teardown();
}
