#include "th06/effect.h"

#include "anm_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/chain.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"

#include "core/base/memory.h"

#include "stg/rng.h"

#include <SDL3/SDL.h>

static char *m_Directory;

// Writes an ANM file of scripts that end at frame 1000, except the first,
// which ends at end_time: EFFECT_EXPLOSION's script in effects.anm.
static void M_WriteAnm(
    const char *const name, const int32_t scripts, const int16_t end_time)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    for (int32_t i = 0; i < scripts; i++) {
        const int32_t script = AnmBuilder_BeginScript(&builder, i);
        AnmBuilder_Instr(
            &builder, script, i == 0 ? end_time : 1000, 0, nullptr, 0);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, name, bytes, size);
}

static void M_Setup(const char *const case_name, const int32_t stage)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    M_WriteAnm("data/eff02.anm", 1, 1000);
    M_WriteAnm("effects.anm", 17, 2);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    CHECK(Result_Log(Anm_Load(0, "effects.anm", 669), LOG_LEVEL_ERROR));
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = stage;
    CHECK(Result_Log(Effect_Register(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

static const VEC3 m_Origin = { 100.0f, 200.0f, 0.0f };

TEST(registering_loads_the_stage_effects)
{
    M_Setup("register", 2);
    CHECK(Anm_GetScript(0x2b3) != nullptr);
    Effect_Remove();
    CHECK(Anm_GetScript(0x2b3) == nullptr);
    M_Teardown();
}

TEST(spawn_returns_the_last_effect_started)
{
    M_Setup("spawn", 2);
    EFFECT *const effect =
        Effect_Spawn(EFFECT_EXPLOSION, m_Origin, 3, 0x80ffffffu);
    CHECK(effect->active);
    CHECK(effect->pos.x == 100.0f && effect->pos.y == 200.0f);
    CHECK(effect->vm.color == 0x80ffffffu);
    CHECK(effect->vm.script == Anm_GetScript(669));
    M_Teardown();
}

TEST(a_full_pool_returns_a_spare_that_never_runs)
{
    M_Setup("full", 2);
    for (int32_t i = 0; i < EFFECT_COUNT; i++) {
        CHECK(Effect_Spawn(EFFECT_PARTICLE, m_Origin, 1, 0)->active);
    }
    EFFECT *const spare = Effect_Spawn(EFFECT_PARTICLE, m_Origin, 1, 0);
    CHECK(!spare->active);
    spare->velocity.x = 5.0f;
    Chain_RunCalc(&g_Chain);
    CHECK(!spare->active);
    CHECK(spare->pos.x == 0.0f);
    M_Teardown();
}

TEST(an_effect_ends_with_its_script)
{
    M_Setup("ends", 2);
    EFFECT *const effect = Effect_Spawn(EFFECT_EXPLOSION, m_Origin, 1, 0);
    Chain_RunCalc(&g_Chain);
    CHECK(effect->active);
    Chain_RunCalc(&g_Chain);
    CHECK(!effect->active);
    M_Teardown();
}

TEST(a_drift_picks_its_velocity_on_the_first_frame_only)
{
    M_Setup("drift", 2);
    EFFECT *effects[32];
    for (int i = 0; i < 32; i++) {
        effects[i] = Effect_Spawn(EFFECT_PARTICLE, m_Origin, 1, 0);
    }
    RNG expected = g_Rng;
    Chain_RunCalc(&g_Chain);
    // D3DX divides a vector by multiplying with the reciprocal, which
    // rounds differently from a division for some of these values.
    const float step = 1.0f / 19.0f;
    for (int i = 0; i < 32; i++) {
        const float x = (Rng_F32(&expected) * 256.0f - 128.0f) / 12.0f;
        const float y = (Rng_F32(&expected) * 256.0f - 128.0f) / 12.0f;
        CHECK(effects[i]->pos.x == 100.0f + x);
        CHECK(effects[i]->pos.y == 200.0f + y);
        CHECK(effects[i]->acceleration.x == -x * step);
        CHECK(effects[i]->acceleration.y == -y * step);
        CHECK(effects[i]->velocity.x == x + -x * step);
    }
    Chain_RunCalc(&g_Chain);
    CHECK(g_Rng.seed == expected.seed);
    M_Teardown();
}

TEST(drawing_places_the_sprite_at_the_effect)
{
    M_Setup("draw", 2);
    EFFECT *const effect = Effect_Spawn(EFFECT_PARTICLE, m_Origin, 1, 0);
    Chain_RunDraw(&g_Chain);
    CHECK(effect->vm.pos.x == 100.0f && effect->vm.pos.y == 200.0f);
    M_Teardown();
}

TEST(an_orbit_fades_out_over_sixteen_frames)
{
    M_Setup("orbit", 2);
    EFFECT *const effect = Effect_Spawn(EFFECT_ORBIT, m_Origin, 1, 0xffffffffu);
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
