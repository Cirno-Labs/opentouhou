#include "th07/effect.h"

#include "th07/anm.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/stage.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/stubs.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"
#include "th07_scene_stubs.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"

#include <SDL3/SDL.h>

#define I(x) { .i = (x) }
#define F(x) { .f = (x) }
#define INSTR(op_, time_, ...)                                                 \
    {                                                                          \
        .opcode = (op_),                                                       \
        .time = (time_),                                                       \
        .args = { __VA_ARGS__ },                                               \
    }
#define M_SPRITE 3
#define M_PETAL_SPRITE 0x2d8
#define M_SPRITE_OP 3
#define M_ROTATION_SPEED_OP 13
#define M_BLEND_OP 16
#define M_END_OP -1
#define M_PI 3.1415927f
#define M_TWO_PI 6.2831855f

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static const VEC3 m_Origin = { 100.0f, 200.0f, 0.0f };
static char *m_Directory;

// Runs until frame 1000.
static ANM_INSTR m_LongInstrs[] = {
    INSTR(M_SPRITE_OP, 0, I(M_SPRITE)),
    INSTR(M_END_OP, 1000),
};
// Ends at frame 2.
static ANM_INSTR m_ShortInstrs[] = {
    INSTR(M_SPRITE_OP, 0, I(M_SPRITE)),
    INSTR(M_END_OP, 2),
};
static ANM_INSTR m_AddInstrs[] = {
    INSTR(M_SPRITE_OP, 0, I(M_SPRITE)),
    INSTR(M_BLEND_OP, 0, I(1)),
    INSTR(M_END_OP, 1000),
};
static ANM_INSTR m_SpinInstrs[] = {
    INSTR(M_SPRITE_OP, 0, I(M_SPRITE)),
    INSTR(M_ROTATION_SPEED_OP, 0, F(0.0f), F(0.0f), F(0.25f)),
    INSTR(M_END_OP, 1000),
};
static ANM_SCRIPT m_Long = { m_LongInstrs, 2, 0 };
static ANM_SCRIPT m_Spin = { m_SpinInstrs, 3, 0 };
static ANM_SCRIPT m_Short = { m_ShortInstrs, 2, 0 };
static ANM_SCRIPT m_Add = { m_AddInstrs, 3, 0 };

static void M_WriteAnm(const char *const name, const int32_t script)
{
    TH07_ANM_BUILDER builder;
    Th07AnmBuilder_Init(&builder);
    TH07_ANM_BUILDER_ANM *const anm = Th07AnmBuilder_Add(&builder, 64, 64, "@");
    Th07AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    Th07AnmBuilder_End(anm, Th07AnmBuilder_BeginScript(anm, script));
    Th07AnmBuilder_WriteFile(&builder, m_Directory, name);
}

// Installs a script that runs for 1000 frames at every script number of
// the effect types, and sprites for it and for the petals.
static void M_Setup(const char *const case_name, const int32_t stage)
{
    SceneStubCalls_Reset();
    GfxRecorder_Start();
    g_GameSpeed = 1.0f;
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    M_WriteAnm("data/eff04.anm", 0);
    M_WriteAnm("data/eff04b.anm", 0);
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    CHECK(Result_Log(
        Anm_CreateTexture(0, 256, 256, ANM_TEXTURE_RGBA8), LOG_LEVEL_ERROR));
    const ANM_SPRITE sprite = {
        .slot = 0,
        .x1 = 16.0f,
        .y1 = 16.0f,
        .texture_width = 256.0f,
        .texture_height = 256.0f,
        .u1 = 0.0625f,
        .v1 = 0.0625f,
        .width = 16.0f,
        .height = 16.0f,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
    };
    Anm_SetSpriteEntry(M_SPRITE, &sprite);
    Anm_SetSpriteEntry(M_PETAL_SPRITE, &sprite);
    for (int32_t i = 0x2ab; i <= 0x304; i++) {
        Anm_SetScriptEntry(i, &m_Long);
    }
    Chain_Init(&g_Chain);
    g_GameManager.run.stage = stage;
    g_GameManager.camera.viewport = (VIEWPORT) { 0, 0, 640, 480, 0, 1 };
    g_GameManager.cherry.max = 100;
    CHECK(Result_Log(Effect_Register(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Th07Data_Unmount();
    GfxRecorder_Stop();
    Mem_Free(m_Directory);
}

// Looks along +z from far behind the origin, so petals stay in view.
static void M_SetCamera(void)
{
    STAGE_CAMERA *const camera = &g_Stage.camera.current;
    camera->eye = (VEC3) { 0.0f, 0.0f, -10000.0f };
    camera->at = (VEC3) { 0.0f, 0.0f, 4000.0f };
    camera->up = (VEC3) { 0.0f, 1.0f, 0.0f };
    camera->dir = (VEC3) { 0.0f, 0.0f, 1.0f };
    camera->right = (VEC3) { 1.0f, 0.0f, 0.0f };
    const GFX_VIEW view = {
        .view = Mat4_LookAtLH(
            camera->eye, Vec3_Add(camera->eye, camera->at), camera->up),
        .projection =
            Mat4_PerspectiveFovLH(0.5f, 640.0f / 480.0f, 1.0f, 20000.0f),
        .viewport = { 0, 0, 640, 480, 0, 1 },
        .id = ANM_VIEW_STAGE,
    };
    Anm_SetCameras(&view, &view, camera->right);
    Anm_SetCamera(ANM_CAMERA_STAGE);
}

static void M_Tick(void)
{
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Effect_DrawPetals();
    Gfx_EndTick();
}

TEST(registering_loads_the_stage_effects)
{
    M_Setup("register", 4);
    CHECK(Anm_GetScript(0x2dc) != nullptr);
    CHECK(Anm_GetScript(0x2dd) != nullptr);
    CHECK_EQ_INT(g_Stage.spell_background.count, 2);
    CHECK_EQ_INT(g_Stage.spell_background.offset, 0);
    Effect_Remove();
    CHECK(Anm_GetScript(0x2dc) == nullptr);
    CHECK(Anm_GetScript(0x2dd) == nullptr);
    M_Teardown();
}

TEST(spawn_returns_the_last_effect_started)
{
    M_Setup("spawn", 9);
    EFFECT *const effect =
        Effect_Spawn(EFFECT_EXPLOSION, m_Origin, 3, 0x80ffffffu);
    CHECK(effect->active);
    CHECK(effect->pos.x == 100.0f && effect->pos.y == 200.0f);
    CHECK(effect->vm.color == 0x80ffffffu);
    CHECK(effect->vm.script == Anm_GetScript(0x2ab));
    CHECK(effect->vm.z_write_off);
    CHECK_EQ_INT(effect->draw_list, 0);
    M_Teardown();
}

TEST(a_moving_spawn_keeps_its_velocity_and_depth_writes)
{
    M_Setup("moving", 9);
    const VEC3 velocity = { 1.0f, 2.0f, 3.0f };
    EFFECT *const effect =
        Effect_SpawnMoving(EFFECT_SPARK, m_Origin, velocity, 1, 0);
    CHECK(effect->velocity.x == 1.0f && effect->velocity.z == 3.0f);
    CHECK(!effect->vm.z_write_off);
    CHECK(Effect_Spawn(EFFECT_SPARK, m_Origin, 1, 0)->velocity.x == 0.0f);
    M_Teardown();
}

TEST(a_full_pool_returns_a_spare_that_never_runs)
{
    M_Setup("full", 9);
    EFFECT *const reserved =
        Effect_SpawnReserved(EFFECT_SPELL_CIRCLE, m_Origin, 7, 0);
    for (int32_t i = 0; i < EFFECT_COUNT; i++) {
        EFFECT *const effect = Effect_Spawn(EFFECT_SHOCKWAVE, m_Origin, 1, 0);
        CHECK(effect->active && effect != reserved);
    }
    EFFECT *const spare = Effect_Spawn(EFFECT_SHOCKWAVE, m_Origin, 1, 0);
    CHECK(!spare->active);
    CHECK(spare != reserved);
    Chain_RunCalc(&g_Chain);
    CHECK(!spare->active);
    CHECK(reserved->active);
    M_Teardown();
}

TEST(a_reserved_spawn_restarts_its_slot)
{
    M_Setup("reserved", 9);
    EFFECT *const first =
        Effect_SpawnReserved(EFFECT_SPELL_CIRCLE, m_Origin, 2, 1);
    Chain_RunCalc(&g_Chain);
    EFFECT *const second = Effect_SpawnReserved(EFFECT_BORDER, m_Origin, 2, 2);
    CHECK(first == second);
    CHECK_EQ_INT(second->type, EFFECT_BORDER);
    CHECK_EQ_INT(second->timer.current, 0);
    CHECK(second->vm.color == 2);
    M_Teardown();
}

TEST(an_effect_ends_with_its_script)
{
    M_Setup("ends", 9);
    Anm_SetScriptEntry(0x2ab, &m_Short);
    EFFECT *const effect = Effect_Spawn(EFFECT_EXPLOSION, m_Origin, 1, 0);
    Chain_RunCalc(&g_Chain);
    CHECK(effect->active);
    Chain_RunCalc(&g_Chain);
    CHECK(!effect->active);
    M_Teardown();
}

TEST(timers_run_at_the_game_speed)
{
    M_Setup("speed", 9);
    EFFECT *const effect = Effect_Spawn(EFFECT_EXPLOSION, m_Origin, 1, 0);
    g_GameSpeed = 0.5f;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(effect->timer.current, 0);
    CHECK(effect->timer.sub_frame == 0.5f);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(effect->timer.current, 1);
    M_Teardown();
}

TEST(a_drift_picks_its_step_at_spawn_scaled_by_the_game_speed)
{
    M_Setup("drift", 9);
    g_GameSpeed = 0.5f;
    RNG expected = g_Rng;
    EFFECT *effects[32];
    for (int32_t i = 0; i < 32; i++) {
        effects[i] = Effect_Spawn(EFFECT_PARTICLE + 5, m_Origin, 1, 0);
    }
    // D3DX divides a vector by multiplying with the reciprocal, which
    // rounds differently from a division for some of these values.
    const float reciprocal = 1.0f / 19.0f;
    for (int32_t i = 0; i < 32; i++) {
        const float x = (Rng_F32(&expected) * 256.0f - 128.0f) / 12.0f;
        const float y = (Rng_F32(&expected) * 256.0f - 128.0f) / 12.0f;
        const float ax = -x * reciprocal * 0.5f;
        CHECK(effects[i]->step.x == x * 0.5f);
        CHECK(effects[i]->step.y == y * 0.5f);
        CHECK(effects[i]->acceleration.x == ax);
        CHECK(effects[i]->acceleration.y == -y * reciprocal * 0.5f);
    }
    CHECK(g_Rng.seed == expected.seed);
    const float x = effects[0]->step.x;
    const float ax = effects[0]->acceleration.x;
    Chain_RunCalc(&g_Chain);
    CHECK(effects[0]->pos.x == 100.0f + x);
    CHECK(effects[0]->step.x == x + ax);
    CHECK(g_Rng.seed == expected.seed);
    M_Teardown();
}

TEST(a_fast_drift_slows_over_twenty_frames)
{
    M_Setup("drift_fast", 9);
    RNG expected = g_Rng;
    EFFECT *const effect = Effect_Spawn(EFFECT_BOMB_SPARK, m_Origin, 1, 0);
    const float x = (Rng_F32(&expected) * 256.0f - 128.0f) * 4.0f / 33.0f;
    CHECK(effect->step.x == x);
    CHECK(effect->acceleration.x == -x * (1.0f / 20.0f));
    M_Teardown();
}

TEST(an_orbit_draws_projected_and_fades_out_over_sixteen_frames)
{
    M_Setup("orbit", 9);
    EFFECT *const effect = Effect_Spawn(EFFECT_ORBIT, m_Origin, 1, 0xffffffffu);
    CHECK_EQ_INT(effect->draw_list, 2);
    effect->origin = m_Origin;
    effect->axis = (VEC3) { 0.0f, 0.0f, 1.0f };
    effect->radius = 10.0f;
    Chain_RunCalc(&g_Chain);
    CHECK(effect->active);
    effect->fade_out = true;
    for (int i = 0; i < 15; i++) {
        Chain_RunCalc(&g_Chain);
        CHECK(effect->active);
    }
    CHECK(effect->vm.color >> 24 == (uint32_t)(1.0f / 16.0f * 255.0f));
    Chain_RunCalc(&g_Chain);
    CHECK(!effect->active);
    M_Teardown();
}

TEST(a_charge_comes_in_at_z_zero)
{
    M_Setup("charge", 9);
    RNG expected = g_Rng;
    const VEC3 pos = { 100.0f, 200.0f, 5.0f };
    EFFECT *const effect = Effect_Spawn(EFFECT_CHARGE, pos, 1, 0);
    const float angle = Rng_F32(&expected) * M_TWO_PI - M_PI;
    CHECK(effect->origin.z == 0.0f);
    CHECK(effect->axis.x == X87_Cos(angle));
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    const float distance = 256.0f - 1.0f * 256.0f / 60.0f;
    CHECK(effect->pos.x == effect->axis.x * distance + 100.0f);
    CHECK(effect->pos.z == 0.0f);
    M_Teardown();
}

TEST(a_burst_goes_out_at_the_angle_of_its_velocity)
{
    M_Setup("burst", 9);
    RNG expected = g_Rng;
    EFFECT *const effect = Effect_SpawnMoving(
        EFFECT_BURST, m_Origin, (VEC3) { 1.0f, 0.0f, 0.0f }, 1, 0);
    const float length = Rng_F32(&expected) * 1.5f + 1.0f;
    CHECK(g_Rng.seed == expected.seed);
    CHECK(effect->axis.x == X87_Cos(1.0f) * length);
    CHECK(effect->axis.y == X87_Sin(1.0f) * length);
    for (int i = 0; i < 46; i++) {
        Chain_RunCalc(&g_Chain);
    }
    float f = 45.0f / 90.0f;
    f = 1.0f - (1.0f - f) * (1.0f - f);
    CHECK(effect->pos.x == effect->axis.x * f * 128.0f + 100.0f);
    M_Teardown();
}

TEST(a_burst_from_minus_990_picks_a_random_angle)
{
    M_Setup("burst_random", 9);
    RNG expected = g_Rng;
    EFFECT *const effect = Effect_SpawnMoving(
        EFFECT_BURST + 10, m_Origin, (VEC3) { -990.0f, 0.0f, 0.0f }, 1, 0);
    const float angle = Rng_F32(&expected) * M_TWO_PI - M_PI;
    const float length = Rng_F32(&expected) * 1.5f + 1.0f;
    CHECK(effect->axis.x == X87_Cos(angle) * length);
    M_Teardown();
}

TEST(a_diverge_goes_out_256_every_30_frames)
{
    M_Setup("diverge", 9);
    EFFECT *const effect = Effect_Spawn(EFFECT_DIVERGE, m_Origin, 1, 0);
    effect->axis = (VEC3) { 1.0f, 0.0f, 0.0f };
    for (int i = 0; i < 31; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK(effect->pos.x == 100.0f + 256.0f);
    M_Teardown();
}

TEST(a_player_aura_follows_the_player_until_its_script_ends)
{
    M_Setup("aura", 9);
    Anm_SetScriptEntry(0x2c2, &m_Short);
    EFFECT *const effect =
        Effect_SpawnReserved(EFFECT_PLAYER_AURA, m_Origin, 2, 0);
    g_Player.pos = (VEC3) { 5.0f, 6.0f, 0.0f };
    Chain_RunCalc(&g_Chain);
    CHECK(effect->pos.x == 5.0f && effect->pos.y == 6.0f);
    Chain_RunCalc(&g_Chain);
    CHECK(!effect->active);
    M_Teardown();
}

TEST(play_area_lists_draw_offset_with_additive_ones_last)
{
    M_Setup("lists", 9);
    Anm_SetScriptEntry(0x2ab, &m_Add);
    g_GameManager.play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    EFFECT *const add = Effect_Spawn(EFFECT_EXPLOSION, m_Origin, 1, 0xffffffff);
    EFFECT *const normal = Effect_Spawn(
        EFFECT_SHOCKWAVE, (VEC3) { 300.0f, 200.0f, 0.0f }, 1, 0xffffffff);
    Chain_RunCalc(&g_Chain);
    M_Tick();
    CHECK(add->vm.pos.x == 132.0f && add->vm.pos.y == 216.0f);
    CHECK(normal->vm.pos.x == 332.0f);
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    CHECK(GfxRecorder_Quad(0)->vertices[0].position.x > 300.0f);
    CHECK(GfxRecorder_Quad(1)->vertices[0].position.x < 300.0f);
    M_Teardown();
}

TEST(a_petal_starts_around_the_camera_and_falls_through_its_view)
{
    M_Setup("petal", 9);
    M_SetCamera();
    Anm_SetScriptEntry(0x2bf, &m_Spin);
    g_GameSpeed = 0.5f;
    RNG expected = g_Rng;
    EFFECT *const effect = Effect_SpawnMoving(
        EFFECT_PETAL_FALLING, m_Origin, (VEC3) { 0.0f, 0.0f, 0.5f }, 1, 0);
    const float x = Rng_F32(&expected) * 160.0f - 80.0f;
    Rng_F32(&expected);
    const float z = Rng_F32(&expected) * 200.0f - 0.0f;
    const float step_x = Rng_F32(&expected) * 0.06f - 0.03f;
    Rng_F32(&expected);
    const float step_z = -(Rng_F32(&expected) * 0.1f) + 0.5f;
    const float spin = Rng_F32(&expected) * M_TWO_PI - M_PI;
    CHECK(effect->true_pos.x == x + 0.0f);
    CHECK(effect->true_pos.z == z + -8000.0f);
    CHECK(effect->step.x == step_x * 0.5f);
    CHECK(effect->step.z == step_z * 0.5f);
    CHECK(effect->acceleration.z == -0.015f);
    CHECK(effect->vm.rotation_speed.z == 0.5f);
    CHECK(effect->vm.rotation.z == spin);
    CHECK_EQ_INT(effect->vm.sprite, M_PETAL_SPRITE);
    CHECK_EQ_INT(effect->draw_list, 1);
    Rng_F32(&expected);
    CHECK(g_Rng.seed == expected.seed);

    const float true_z = effect->true_pos.z;
    Chain_RunCalc(&g_Chain);
    CHECK(effect->active);
    CHECK(effect->pos.z == true_z + (step_z * 0.5f + -0.015f));
    const float turned = Angle_AddNormalize(spin, effect->vm.rotation.x);
    CHECK(effect->vm.rotation.z == Angle_AddNormalize(turned, 0.5f * 0.5f));
    effect->true_pos.z = -1.0f;
    effect->step = (VEC3) {};
    effect->acceleration = (VEC3) { 0.0f, 0.0f, 1.0f };
    Chain_RunCalc(&g_Chain);
    CHECK(!effect->active);
    M_Teardown();
}

TEST(a_petal_out_of_the_camera_view_ends)
{
    M_Setup("petal_view", 9);
    M_SetCamera();
    EFFECT *const effect = Effect_Spawn(EFFECT_PETAL_DRIFT, m_Origin, 1, 0);
    effect->true_pos = (VEC3) { 3000.0f, 0.0f, -9000.0f };
    Chain_RunCalc(&g_Chain);
    CHECK(!effect->active);
    M_Teardown();
}

TEST(a_petal_keeps_the_z_acceleration_its_slot_had)
{
    M_Setup("petal_leak", 9);
    M_SetCamera();
    g_GameSpeed = 0.5f;
    EFFECT *const old = Effect_Spawn(EFFECT_EXPLOSION, m_Origin, 1, 0);
    old->acceleration.z = 4.0f;
    old->active = false;
    for (int32_t i = 1; i < EFFECT_COUNT; i++) {
        Effect_Spawn(EFFECT_SHOCKWAVE, m_Origin, 1, 0);
    }
    EFFECT *const petal = Effect_Spawn(EFFECT_PETAL, m_Origin, 1, 0);
    CHECK(petal == old);
    CHECK(petal->acceleration.z == 2.0f);
    M_Teardown();
}

TEST(the_petal_sprite_shows_as_the_cherry_over_its_whole_maximum)
{
    M_Setup("petal_sprite", 9);
    M_SetCamera();
    g_GameManager.cherry.offset = 1000;
    g_GameManager.cherry.value = 1100;
    g_GameManager.cherry.max = 1100;
    int32_t shown = 0;
    for (int32_t i = 0; i < 200; i++) {
        RNG expected = g_Rng;
        for (int32_t j = 0; j < 10; j++) {
            Rng_F32(&expected);
        }
        const bool roll = 100 * 100 / 1100 >= Rng_U32InRange(&expected, 100);
        EFFECT *const effect = Effect_Spawn(EFFECT_PETAL, m_Origin, 1, 0);
        CHECK(g_Rng.seed == expected.seed);
        CHECK((effect->vm.sprite == M_PETAL_SPRITE) == roll);
        CHECK(!roll || (effect->vm.color & 0xffffffu) == 0xffffffu);
        shown += roll;
        effect->active = false;
    }
    CHECK(shown > 0 && shown < 50);
    M_Teardown();
}

TEST(a_cherry_below_its_offset_always_shows_the_petal_sprite)
{
    M_Setup("petal_sprite_below", 9);
    M_SetCamera();
    g_GameManager.cherry.offset = 1000;
    for (int32_t i = 0; i < 20; i++) {
        EFFECT *const effect = Effect_Spawn(EFFECT_PETAL, m_Origin, 1, 0);
        CHECK_EQ_INT(effect->vm.sprite, M_PETAL_SPRITE);
        effect->active = false;
    }
    M_Teardown();
}

TEST(a_sinking_petal_always_shows_the_petal_sprite)
{
    M_Setup("petal_sinking", 9);
    M_SetCamera();
    RNG expected = g_Rng;
    EFFECT *const effect = Effect_SpawnMoving(
        EFFECT_PETAL_SINKING, m_Origin, (VEC3) { 60.0f, 0.0f, 0.0f }, 1, 0);
    const float x = Rng_F32(&expected) * 160.0f - 80.0f;
    const float y = Rng_F32(&expected) * 160.0f - 80.0f;
    Rng_F32(&expected);
    const float step_z = -(Rng_F32(&expected) * 0.2f) - 0.06f;
    CHECK(effect->step.x == -y / 60.0f);
    CHECK(effect->step.y == x / 60.0f);
    CHECK(effect->step.z == step_z);
    CHECK_EQ_INT(effect->vm.sprite, M_PETAL_SPRITE);
    Rng_F32(&expected);
    Rng_F32(&expected);
    CHECK(g_Rng.seed == expected.seed);
    M_Teardown();
}

TEST(drifting_petals_start_falling_on_request)
{
    M_Setup("petal_falling", 9);
    M_SetCamera();
    g_GameSpeed = 0.5f;
    EFFECT *const drift = Effect_SpawnMoving(
        EFFECT_PETAL_DRIFT, m_Origin, (VEC3) { 0.0f, 0.0f, 0.5f }, 1, 0);
    CHECK(drift->acceleration.z == 0.0f);
    CHECK(drift->step.z > 0.5f);
    EFFECT *const other = Effect_Spawn(EFFECT_PETAL_FALLING, m_Origin, 1, 0);
    Effect_StartPetalsFalling();
    CHECK(drift->acceleration.z == -0.01f);
    CHECK(other->acceleration.z == -0.015f);
    M_Teardown();
}

TEST(offsetting_moves_two_petal_types_even_when_inactive)
{
    M_Setup("petal_offset", 9);
    M_SetCamera();
    EFFECT *const petal = Effect_Spawn(EFFECT_PETAL, m_Origin, 1, 0);
    EFFECT *const falling = Effect_Spawn(EFFECT_PETAL_FALLING, m_Origin, 1, 0);
    EFFECT *const drift = Effect_Spawn(EFFECT_PETAL_DRIFT, m_Origin, 1, 0);
    const VEC3 a = petal->true_pos;
    const VEC3 b = falling->true_pos;
    const VEC3 c = drift->true_pos;
    petal->active = false;
    Effect_OffsetPetals((VEC3) { 1.0f, 2.0f, 4.0f });
    CHECK(petal->true_pos.x == a.x + 1.0f && petal->true_pos.z == a.z + 4.0f);
    CHECK(falling->true_pos.y == b.y + 2.0f);
    CHECK(drift->true_pos.x == c.x && drift->true_pos.z == c.z);
    M_Teardown();
}

TEST(petals_draw_by_the_effects_quality)
{
    M_Setup("petal_quality", 9);
    M_SetCamera();
    Effect_Spawn(EFFECT_PETAL_DRIFT, m_Origin, 1, 0xffffffffu);
    Chain_RunCalc(&g_Chain);
    M_Tick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    g_EffectsQuality = 1;
    M_Tick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    g_EffectsQuality = 0;
    M_Tick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    M_Teardown();
}

TEST(the_first_petal_type_draws_transparent_and_keeps_its_color)
{
    M_Setup("petal_scale", 9);
    M_SetCamera();
    g_GameManager.cherry.value = 100;
    EFFECT *const effect = Effect_Spawn(EFFECT_PETAL, m_Origin, 1, 0xffffffffu);
    effect->true_pos = (VEC3) { 0.0f, 0.0f, -8000.0f };
    effect->step = (VEC3) {};
    effect->acceleration = (VEC3) {};
    Chain_RunCalc(&g_Chain);
    CHECK(effect->active);
    M_Tick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    CHECK(effect->vm.color == 0xffffffffu);
    M_Teardown();
}

TEST(an_effect_at_the_camera_draws_in_3d_with_the_petals)
{
    M_Setup("at_camera", 9);
    M_SetCamera();
    EFFECT *const effect =
        Effect_Spawn(EFFECT_AT_CAMERA, m_Origin, 1, 0xffffffffu);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(effect->draw_list, 3);
    CHECK(effect->true_pos.z == -6000.0f);
    CHECK(effect->pos.z == 0.0f);
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    M_Tick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    M_Teardown();
}
