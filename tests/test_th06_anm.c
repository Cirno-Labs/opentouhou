#include "th06/anm.h"
#include "th06/anm/private.h"

#include "th06/files.h"
#include "th06/timer.h"
#include "th06/translation.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "core/base/memory.h"
#include "core/gfx/private.h"
#include "core/io/writer.h"
#include "core/patch.h"
#include "core/text.h"

#include "stg/rng.h"

#include <string.h>

// Prepares a directory with the six archives Files_Init needs and a loose
// ANM file built from builder.
static char *M_SetupFiles(
    const char *const case_name, const char *const anm_name,
    const ANM_BUILDER *const builder)
{
    char *const directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(directory, -1);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(builder, bytes);
    Th06Data_WriteFile(directory, anm_name, bytes, size);
    return directory;
}

static RNG m_Rng;

static void M_Setup(const char *const directory)
{
    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&m_Rng, 4);
    Anm_Init(&g_HeapAllocator, &m_Rng);
}

static void M_Teardown(char *const directory)
{
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(directory);
}

TEST(load_sets_sprite_table_and_uvs)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 256, 128);
    AnmBuilder_AddSprite(&builder, 0, 16.0f, 32.0f, 48.0f, 64.0f);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);

    char *const directory =
        M_SetupFiles("load_sprite_table", "test.anm", &builder);
    M_Setup(directory);

    CHECK(Result_Log(Anm_Load(0, "test.anm", 100), LOG_LEVEL_ERROR));
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_SetSprite(&vm, 100);
    CHECK(vm.sprite_data != nullptr);
    CHECK(vm.sprite_data->x0 == 16.0f);
    CHECK(vm.sprite_data->y0 == 32.0f);
    CHECK(vm.sprite_data->x1 == 64.0f);
    CHECK(vm.sprite_data->y1 == 96.0f);
    CHECK(vm.sprite_data->width == 48.0f);
    CHECK(vm.sprite_data->height == 64.0f);
    CHECK(vm.sprite_data->u0 == 16.0f / 256.0f);
    CHECK(vm.sprite_data->v0 == 32.0f / 128.0f);
    CHECK(vm.sprite_data->u1 == 64.0f / 256.0f);
    CHECK(vm.sprite_data->v1 == 96.0f / 128.0f);
    CHECK(vm.uv_scale_x == 48.0f / 256.0f);
    CHECK(vm.uv_scale_y == 64.0f / 128.0f);

    M_Teardown(directory);
}

TEST(load_sets_script_table_and_sprite_offset)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    const int32_t script = AnmBuilder_BeginScript(&builder, 5);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);

    char *const directory =
        M_SetupFiles("load_script_table", "test.anm", &builder);
    M_Setup(directory);

    CHECK(Anm_GetScript(205) == nullptr);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 200), LOG_LEVEL_ERROR));
    const ANM_SCRIPT *const loaded = Anm_GetScript(205);
    CHECK(loaded != nullptr);
    CHECK_EQ_INT(loaded->count, 1);
    CHECK_EQ_INT(loaded->sprite_offset, 200);
    CHECK_EQ_INT(loaded->instrs[0].opcode, 0);

    M_Teardown(directory);
}

TEST(release_frees_sprites_and_scripts)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);

    char *const directory = M_SetupFiles("release", "test.anm", &builder);
    M_Setup(directory);

    CHECK(Result_Log(Anm_Load(3, "test.anm", 10), LOG_LEVEL_ERROR));
    CHECK(Anm_GetScript(10) != nullptr);
    Anm_Release(3);
    CHECK(Anm_GetScript(10) == nullptr);
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_SetSprite(&vm, 10);
    CHECK(vm.sprite_data == nullptr);
    Anm_Release(3); // a second release does nothing

    M_Teardown(directory);
}

TEST(failed_load_forgets_what_it_set)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    const int32_t good = AnmBuilder_BeginScript(&builder, 0);
    AnmBuilder_Instr(&builder, good, 0, 0, nullptr, 0);
    const int32_t bad = AnmBuilder_BeginScript(&builder, 1);
    const uint32_t jump_args[1] = { 999 };
    AnmBuilder_Instr(&builder, bad, 0, 5, jump_args, 1);
    AnmBuilder_Instr(&builder, bad, 0, 0, nullptr, 0);

    char *const directory = M_SetupFiles("failed_load", "test.anm", &builder);
    M_Setup(directory);

    CHECK(!Result_Ignore(Anm_Load(2, "test.anm", 10)));
    CHECK(Anm_GetScript(10) == nullptr);
    CHECK(Anm_GetScript(11) == nullptr);
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_SetSprite(&vm, 10);
    CHECK(vm.sprite_data == nullptr);

    M_Teardown(directory);
}

TEST(load_fails_on_oversized_args)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    const uint32_t args[5] = {};
    AnmBuilder_Instr(&builder, script, 0, 2, args, 5); // 20 bytes, over 16

    char *const directory =
        M_SetupFiles("oversized_args", "test.anm", &builder);
    M_Setup(directory);

    const RESULT result = Anm_Load(0, "test.anm", 0);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "test.anm") != nullptr);
    Result_Ignore(result);

    M_Teardown(directory);
}

TEST(load_fails_on_bad_jump_target)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    const uint32_t jump_args[1] = { 999 }; // not an instruction boundary
    AnmBuilder_Instr(&builder, script, 0, 5, jump_args, 1);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);

    char *const directory = M_SetupFiles("bad_jump", "test.anm", &builder);
    M_Setup(directory);

    const RESULT result = Anm_Load(0, "test.anm", 0);
    CHECK(!IS_OK(result));
    Result_Ignore(result);

    M_Teardown(directory);
}

TEST(load_fails_on_truncated_instruction)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    const uint32_t args[2] = { 0, 0 };
    AnmBuilder_Instr(&builder, script, 0, 2, args, 2); // no terminal opcode

    char *const directory =
        M_SetupFiles("truncated_instr", "test.anm", &builder);
    M_Setup(directory);

    const RESULT result = Anm_Load(0, "test.anm", 0);
    CHECK(!IS_OK(result));
    Result_Ignore(result);

    M_Teardown(directory);
}

// Starts a one-sprite, one-script ANM file at slot 0, first sprite 0, and
// returns the script's index to add instructions to before the caller
// calls AnmBuilder_Build through M_SetupFiles.
static int32_t M_StartBuilder(ANM_BUILDER *const builder, const int32_t id)
{
    AnmBuilder_Init(builder, 32, 32);
    AnmBuilder_AddSprite(builder, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    AnmBuilder_AddSprite(builder, 1, 8.0f, 0.0f, 8.0f, 8.0f);
    AnmBuilder_AddSprite(builder, 2, 16.0f, 0.0f, 8.0f, 8.0f);
    return AnmBuilder_BeginScript(builder, id);
}

TEST(step_set_sprite_shows_it)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    const uint32_t args[1] = { 1 };
    AnmBuilder_Instr(&builder, script, 0, 1, args, 1);
    AnmBuilder_Instr(&builder, script, 0, 15, nullptr, 0);

    char *const directory = M_SetupFiles("set_sprite", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.shown);
    CHECK(vm.sprite == 1);
    CHECK(vm.sprite_time == 0);

    M_Teardown(directory);
}

TEST(step_random_sprite_uses_rng)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    const uint32_t args[2] = { 0, 3 };
    AnmBuilder_Instr(&builder, script, 0, 16, args, 2);
    AnmBuilder_Instr(&builder, script, 0, 15, nullptr, 0);

    char *const directory = M_SetupFiles("random_sprite", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    RNG expected_rng;
    Rng_SetSeed(&expected_rng, 4);
    const uint16_t roll = Rng_U16InRange(&expected_rng, 3);

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.sprite == (int16_t)roll);
    CHECK_EQ_INT(m_Rng.generated, expected_rng.generated);

    M_Teardown(directory);
}

TEST(step_jump_skips_instructions)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    const uint32_t scale_a[2] = { 0, 0 };
    AnmBuilder_Instr(&builder, script, 0, 2, scale_a, 2); // offset 0, size 12
    const uint32_t jump_to_end[1] = { 28 };
    AnmBuilder_Instr(&builder, script, 0, 5, jump_to_end, 1); // offset 12
    const uint32_t sprite_arg[1] = { 2 };
    AnmBuilder_Instr(
        &builder, script, 0, 1, sprite_arg, 1); // offset 20, skipped
    AnmBuilder_Instr(&builder, script, 0, 15, nullptr, 0); // offset 28

    char *const directory = M_SetupFiles("jump", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.sprite == -1); // opcode 1 at offset 20 never ran
    CHECK(vm.pc == -1); // and opcode 15 at offset 28 did run

    M_Teardown(directory);
}

TEST(step_interrupt_jumps_to_label)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 21, nullptr, 0); // stop
    const uint32_t label[1] = { 5 };
    AnmBuilder_Instr(&builder, script, 0, 22, label, 1);
    const uint32_t sprite_arg[1] = { 2 };
    AnmBuilder_Instr(&builder, script, 0, 1, sprite_arg, 1);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);

    char *const directory =
        M_SetupFiles("interrupt_label", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.stopped);

    vm.pending_interrupt = 5;
    const bool ended = Anm_Step(&vm);
    CHECK(ended);
    CHECK(vm.sprite == 2);
    CHECK(!vm.shown); // opcode 0 hides before returning
    CHECK(!vm.stopped);

    M_Teardown(directory);
}

TEST(step_interrupt_falls_back_to_minus_one_label)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 21, nullptr, 0); // stop
    const uint32_t fallback_label[1] = { (uint32_t)-1 };
    AnmBuilder_Instr(&builder, script, 0, 22, fallback_label, 1);
    const uint32_t sprite_arg[1] = { 2 };
    AnmBuilder_Instr(&builder, script, 0, 1, sprite_arg, 1);
    AnmBuilder_Instr(&builder, script, 0, 15, nullptr, 0);

    char *const directory =
        M_SetupFiles("interrupt_fallback", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    vm.pending_interrupt = 123; // no label 123 exists; falls back to -1
    const bool ended = Anm_Step(&vm);
    CHECK(ended);
    CHECK(vm.sprite == 2);
    CHECK(vm.shown); // opcode 15 keeps the sprite shown

    M_Teardown(directory);
}

TEST(step_interrupt_with_no_label_stays_frozen)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 21, nullptr, 0); // stop, no labels
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);

    char *const directory =
        M_SetupFiles("interrupt_no_label", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.stopped);
    const int32_t time_before = vm.script_time.current;
    const int32_t pc_before = vm.pc;

    vm.pending_interrupt = 9;
    const bool ended = Anm_Step(&vm);
    CHECK(!ended);
    // Sub(1) then Tick() leaves the count unchanged, and the script stays
    // at the same instruction, but the interrupt search always
    // clears "stopped", even when it finds nothing to run.
    CHECK_EQ_INT(vm.script_time.current, time_before);
    CHECK_EQ_INT(vm.pc, pc_before);
    CHECK(!vm.stopped);
    CHECK_EQ_INT(vm.pending_interrupt, 0);

    M_Teardown(directory);
}

TEST(step_scale_speed_then_interpolation_then_speed_again)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    uint32_t tmp[2];
    memcpy(&tmp[0], &(float) { 0.5f }, 4);
    memcpy(&tmp[1], &(float) { 1.0f }, 4);
    AnmBuilder_Instr(&builder, script, 0, 11, tmp, 2);
    uint32_t target_args[3];
    memcpy(&target_args[0], &(float) { 10.0f }, 4);
    memcpy(&target_args[1], &(float) { 20.0f }, 4);
    target_args[2] = 4;
    AnmBuilder_Instr(&builder, script, 0, 30, target_args, 3);
    uint32_t speed_b[2];
    memcpy(&speed_b[0], &(float) { 0.25f }, 4);
    memcpy(&speed_b[1], &(float) { 0.0f }, 4);
    AnmBuilder_Instr(&builder, script, 5, 11, speed_b, 2);
    AnmBuilder_Instr(&builder, script, 6, 0, nullptr, 0);

    char *const directory = M_SetupFiles("scale", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0); // call 1: runs opcodes 11 and 30, then ticks once
    CHECK(vm.scale_x == (10.0f - 1.0f) * 1.0f / 4.0f + 1.0f);
    CHECK(vm.scale_y == (20.0f - 1.0f) * 1.0f / 4.0f + 1.0f);

    Anm_Step(&vm); // call 2
    Anm_Step(&vm); // call 3
    Anm_Step(&vm); // call 4: the interpolation reaches its target
    CHECK(vm.scale_x == 10.0f);
    CHECK(vm.scale_y == 20.0f);

    Anm_Step(&vm); // call 5: no interpolation, speed is still 0
    CHECK(vm.scale_x == 10.0f);
    CHECK(vm.scale_y == 20.0f);

    Anm_Step(&vm); // call 6: runs opcode 11 again, then adds the new speed
    CHECK(vm.scale_x == 10.25f);
    CHECK(vm.scale_y == 20.0f);

    const bool ended = Anm_Step(&vm); // call 7: reaches the end
    CHECK(ended);
    CHECK(vm.scale_x == 10.25f); // the end returns before interpolating

    M_Teardown(directory);
}

TEST(step_alpha_fade_clamps_bytes)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    const uint32_t fade_args[2] = { 0xff, 2 };
    AnmBuilder_Instr(&builder, script, 0, 12, fade_args, 2);
    AnmBuilder_Instr(&builder, script, 3, 0, nullptr, 0);

    char *const directory = M_SetupFiles("alpha_fade", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0); // fade_from = 0xffffffff, fade_to = 0xffffffff
    // The alpha byte does not actually change (0xff to 0xff); check the
    // clamp with a lower target instead.
    M_Teardown(directory);

    // Rebuild with a target alpha of 0, from the opaque initial color.
    ANM_BUILDER builder2;
    const int32_t script2 = M_StartBuilder(&builder2, 0);
    const uint32_t fade_to_zero[2] = { 0x00, 2 };
    AnmBuilder_Instr(&builder2, script2, 0, 12, fade_to_zero, 2);
    AnmBuilder_Instr(&builder2, script2, 3, 0, nullptr, 0);
    char *const directory2 = M_SetupFiles("alpha_fade2", "test.anm", &builder2);
    M_Setup(directory2);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm2;
    Anm_CreateVm(&vm2);
    Anm_StartScript(&vm2, 0); // call 1: alpha = 1/2 of the way to 0
    const uint8_t alpha1 = (uint8_t)(vm2.color >> 24);
    CHECK_EQ_INT(alpha1, 127); // X87_Ftol truncates 127.5 toward zero
    Anm_Step(&vm2); // call 2: fully faded, clamped at 0, not negative
    const uint8_t alpha2 = (uint8_t)(vm2.color >> 24);
    CHECK_EQ_INT(alpha2, 0);
    CHECK_EQ_INT(vm2.color & 0xffffff, 0xffffff); // rgb unaffected

    M_Teardown(directory2);
}

TEST(step_move_linear_interpolates_position)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    uint32_t move_args[4];
    memcpy(&move_args[0], &(float) { 10.0f }, 4);
    memcpy(&move_args[1], &(float) { 0.0f }, 4);
    memcpy(&move_args[2], &(float) { 0.0f }, 4);
    move_args[3] = 2; // posInterpTime
    AnmBuilder_Instr(&builder, script, 0, 18, move_args, 4);
    AnmBuilder_Instr(&builder, script, 3, 0, nullptr, 0);

    char *const directory = M_SetupFiles("move", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0); // call 1: posT computed before the timer ticks
    CHECK(vm.pos.x == 0.0f); // move_timer.current is still 0 at the check
    Anm_Step(&vm); // call 2
    CHECK(vm.pos.x == 5.0f);
    Anm_Step(&vm); // call 3: reaches move_time, posT clamps to 1
    CHECK(vm.pos.x == 10.0f);

    M_Teardown(directory);
}

TEST(step_uv_scroll_wraps)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    uint32_t scroll_args[1];
    memcpy(&scroll_args[0], &(float) { 0.75f }, 4);
    AnmBuilder_Instr(&builder, script, 0, 27, scroll_args, 1);
    AnmBuilder_Instr(&builder, script, 1, 27, scroll_args, 1);
    AnmBuilder_Instr(&builder, script, 2, 15, nullptr, 0);

    char *const directory = M_SetupFiles("uv_scroll", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.uv_scroll_x == 0.75f);
    Anm_Step(&vm);
    CHECK(vm.uv_scroll_x == 0.5f); // 0.75 + 0.75 - 1.0

    M_Teardown(directory);
}

TEST(step_end_hides_sprite_and_stop_keeps_it_shown)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    const uint32_t sprite_arg[1] = { 0 };
    AnmBuilder_Instr(&builder, script, 0, 1, sprite_arg, 1);
    AnmBuilder_Instr(&builder, script, 1, 0, nullptr, 0);

    const int32_t script_stop = AnmBuilder_BeginScript(&builder, 1);
    AnmBuilder_Instr(&builder, script_stop, 0, 1, sprite_arg, 1);
    AnmBuilder_Instr(&builder, script_stop, 1, 15, nullptr, 0);

    char *const directory = M_SetupFiles("end_opcodes", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.shown);
    const bool ended = Anm_Step(&vm);
    CHECK(ended);
    CHECK(!vm.shown);

    ANM_VM vm2;
    Anm_CreateVm(&vm2);
    Anm_StartScript(&vm2, 1);
    CHECK(vm2.shown);
    const bool ended2 = Anm_Step(&vm2);
    CHECK(ended2);
    CHECK(vm2.shown);

    M_Teardown(directory);
}

TEST(step_after_the_script_ended_changes_nothing)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);

    char *const directory =
        M_SetupFiles("step_after_end", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(vm.pc == -1);
    vm.rotation_speed.x = 1.0f;
    vm.scale_speed_x = 1.0f;
    const VEC3 rotation_before = vm.rotation;
    const float scale_before = vm.scale_x;
    const int32_t time_before = vm.script_time.current;

    const bool ended = Anm_Step(&vm);
    CHECK(ended);
    CHECK(vm.rotation.x == rotation_before.x);
    CHECK(vm.scale_x == scale_before);
    CHECK_EQ_INT(vm.script_time.current, time_before);

    M_Teardown(directory);
}

TEST(step_flag_opcodes_use_only_the_low_bit)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0);
    const uint32_t two[1] = { 2 }; // bit 0 clear, unlike a plain != 0 test
    AnmBuilder_Instr(&builder, script, 0, 25, two, 1);
    AnmBuilder_Instr(&builder, script, 0, 29, two, 1);
    AnmBuilder_Instr(&builder, script, 0, 31, two, 1);
    AnmBuilder_Instr(&builder, script, 1, 15, nullptr, 0);

    char *const directory = M_SetupFiles("flag_low_bit", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0);
    CHECK(!vm.use_pos_offset);
    CHECK(!vm.shown);
    CHECK(!vm.z_write_off);

    M_Teardown(directory);
}

TEST(clear_menu_scripts_leaves_a_running_vm_running)
{
    ANM_BUILDER builder;
    const int32_t script = M_StartBuilder(&builder, 0x100);
    const uint32_t sprite_arg[1] = { 1 };
    AnmBuilder_Instr(&builder, script, 0, 1, sprite_arg, 1);
    AnmBuilder_Instr(&builder, script, 5, 1, sprite_arg, 1);
    AnmBuilder_Instr(&builder, script, 6, 0, nullptr, 0);

    char *const directory =
        M_SetupFiles("clear_menu_scripts", "test.anm", &builder);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));

    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_StartScript(&vm, 0x100);
    CHECK(Anm_GetScript(0x100) != nullptr);

    Anm_ClearMenuScripts();
    CHECK(Anm_GetScript(0x100) == nullptr);

    // The table forgot the script, but vm already holds its own pointer
    // to it, so it keeps running to its end instruction.
    for (int32_t frame = 0; frame < 6; frame++) {
        Anm_Step(&vm);
    }
    const bool ended = Anm_Step(&vm);
    CHECK(ended);
    CHECK(vm.sprite == 1);

    M_Teardown(directory);
}

TEST(draw_variants_copy_geometry_and_preserve_vm_state)
{
    RNG rng;
    Rng_SetSeed(&rng, 5);
    Anm_Init(&g_HeapAllocator, &rng);
    CHECK(Result_Ignore(Anm_CreateTexture(0, 256, 128, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = { .slot = 0,
                                .width = 32.0f,
                                .height = 16.0f,
                                .u0 = 0.25f,
                                .u1 = 0.375f,
                                .v0 = 0.5f,
                                .v1 = 0.625f };
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_InitVm(&vm);
    vm.sprite_data = &sprite;
    vm.shown = true;
    vm.pos = (VEC3) { 100.25f, 50.75f, 0.5f };
    vm.uv_scale_x = 32.0f / 256.0f;
    vm.uv_scale_y = 16.0f / 128.0f;
    vm.uv_scroll_x = 0.125f;
    vm.color = 0x80402010;
    vm.blend_add = true;
    vm.color_op = true;
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    ANM_DRAW draw = Anm_ScreenDraw();
    Anm_Draw(&vm, &draw);
    Anm_DrawNoRound(&vm, &draw);
    vm.rotation.z = 0.5f;
    Anm_Draw(&vm, &draw);
    vm.rotation.z = 0.0f;
    draw.view = (GFX_VIEW) {
        .view = Mat4_Identity(),
        .projection = Mat4_Identity(),
        .viewport = { 0, 0, 640, 480, 0, 1 },
    };
    const ANM_VM before = vm;
    Anm_Draw3D(&vm, &draw);
    Anm_Draw3DRounded(&vm, &draw);
    CHECK(memcmp(&before, &vm, sizeof(vm)) == 0);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 5);
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(0);
    CHECK(quad->vertices[0].position.x == 84.0f);
    CHECK(quad->vertices[0].position.y == 43.0f);
    CHECK(quad->vertices[1].position.x == 116.0f);
    CHECK(quad->vertices[0].uv.x == 0.375f);
    CHECK_EQ_INT(quad->vertices[0].color.alpha, 0x80);
    CHECK(quad->state.blend == GFX_BLEND_ADD);
    CHECK(quad->state.color_op == GFX_COLOR_ADD);
    CHECK(quad->state.alpha_test && quad->state.repeat);
    CHECK(quad->texture == Anm_GetTexture(0));
    CHECK(GfxRecorder_Quad(1)->vertices[0].position.x == 84.75f);
    CHECK(GfxRecorder_Quad(3)->vertices[0].position.x == 84.25f);
    CHECK(GfxRecorder_Quad(3)->vertices[0].position.y == -34.75f);
    CHECK(GfxRecorder_Quad(4)->vertices[0].position.x == 83.5f);
    GfxRecorder_Stop();
    Anm_Shutdown();
}

TEST(anm_metadata_creates_blank_texture_and_high_texture_slots_are_independent)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 32);
    AnmBuilder_AddSprite(&builder, 0, 0, 0, 16, 16);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);
    char *const directory = TestDir_Prepare("anm_blank_texture");
    Th06Data_WriteArchives(directory, -1);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    size_t size = AnmBuilder_Build(&builder, bytes);
    const uint32_t offset = (uint32_t)size;
    bytes[28] = (uint8_t)offset;
    bytes[29] = (uint8_t)(offset >> 8);
    bytes[30] = (uint8_t)(offset >> 16);
    bytes[31] = (uint8_t)(offset >> 24);
    bytes[size++] = '@';
    bytes[size++] = 0;
    Th06Data_WriteFile(directory, "blank.anm", bytes, size);
    M_Setup(directory);
    CHECK(Result_Ignore(Anm_Load(3, "blank.anm", 0)));
    CHECK(Anm_GetTexture(3) != nullptr);
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(3)), 64);
    CHECK_EQ_INT(GfxTexture_GetHeight(Anm_GetTexture(3)), 32);
    CHECK(Result_Ignore(
        Anm_CreateTexture(ANM_TEXTURE_COUNT - 1, 4, 4, ANM_TEXTURE_RGBA8)));
    Anm_Release(3);
    CHECK(Anm_GetTexture(3) == nullptr);
    CHECK(Anm_GetTexture(ANM_TEXTURE_COUNT - 1) != nullptr);
    M_Teardown(directory);
}

TEST(a_pending_copy_replaces_the_texture_with_the_screen_once)
{
    RNG rng;
    Rng_SetSeed(&rng, 3);
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &rng);
    CHECK(Result_Ignore(Anm_CreateTexture(3, 16, 16, ANM_TEXTURE_RGBA8)));
    GFX_TEXTURE *const original = Anm_GetTexture(3);
    Gfx_BeginTick((GFX_COLOR) {});
    CHECK(Result_Ignore(Anm_DoPendingCopy()));
    CHECK(Anm_GetTexture(3) == original);
    Anm_SetPendingCopy(3, (GFX_RECT) { 0, 0, 8, 4 });
    CHECK(Result_Ignore(Anm_DoPendingCopy()));
    GFX_TEXTURE *const copy = Anm_GetTexture(3);
    CHECK(copy != original);
    CHECK_EQ_INT(GfxTexture_GetWidth(copy), 8);
    CHECK_EQ_INT(GfxTexture_GetHeight(copy), 4);
    CHECK_EQ_INT(GfxRecorder_CopyCount(), 1);
    CHECK(Result_Ignore(Anm_DoPendingCopy()));
    CHECK_EQ_INT(GfxRecorder_CopyCount(), 1);
    Gfx_EndTick();
    Anm_Shutdown();
    GfxRecorder_Stop();
}

TEST(presentation_identity_survives_motion_and_breaks_at_discontinuities)
{
    GfxRecorder_Start();
    CHECK(IS_OK(Anm_CreateTexture(100, 256, 256, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = { .slot = 100, .width = 32, .height = 32 };
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_InitVm(&vm);
    vm.sprite_data = &sprite;
    vm.pos = (VEC3) { 100, 200, 0 };
    vm.uv_scale_x = 0.125f;
    vm.uv_scale_y = 0.125f;
    const uint32_t generation = vm.presentation_generation;
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.interpolate = true;
    draw.part = 7;
    Gfx_BeginTick((GFX_COLOR) {});
    Anm_Draw3D(&vm, &draw);
    Gfx_EndTick();
    const VEC3 old = GfxRecorder_Quad(0)->vertices[0].position;
    vm.pos.x += 20;
    Gfx_BeginTick((GFX_COLOR) {});
    Anm_Draw3D(&vm, &draw);
    ANM_VM clone = vm;
    Anm_RenewPresentation(&clone);
    CHECK(clone.presentation_generation != generation);
    Anm_Draw3D(&clone, &draw);
    draw.part = 8;
    Anm_Draw3D(&vm, &draw);
    Gfx_EndTick();
    CHECK(GfxRecorder_Quad(0)->vertices[0].position.x == old.x + 20);
    CHECK(GfxRecorder_Blend(0.0f, 0)[0].position.x == old.x);
    CHECK(GfxRecorder_Blend(0.0f, 1)[0].position.x == old.x + 20);
    CHECK(GfxRecorder_Blend(0.0f, 2)[0].position.x == old.x + 20);
    Anm_ReleaseTexture(100);
    GfxRecorder_Stop();
}

TEST(sprite_changes_renew_only_the_presentation_lifetime)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 256, 256);
    AnmBuilder_AddSprite(&builder, 0, 0, 0, 16, 16);
    AnmBuilder_AddSprite(&builder, 1, 16, 0, 32, 16);
    char *const directory =
        M_SetupFiles("presentation_sprite", "test.anm", &builder);
    M_Setup(directory);
    CHECK(IS_OK(Anm_Load(0, "test.anm", 0)));
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_SetSprite(&vm, 0);
    const uint32_t first = vm.presentation_generation;
    const uint32_t generated = m_Rng.generated;
    Anm_SetSprite(&vm, 0);
    CHECK_EQ_INT(vm.presentation_generation, first);
    Anm_SetSprite(&vm, 1);
    CHECK(vm.presentation_generation != first);
    CHECK_EQ_INT(m_Rng.generated, generated);
    M_Teardown(directory);
}

static int32_t M_FirstTextPixel(const IMAGE *const image, const int32_t row)
{
    for (int32_t x = 0; x < image->width; x++) {
        for (int32_t y = row; y < row + 16; y++) {
            if (image->pixels
                    [((size_t)y * (size_t)image->width + (size_t)x) * 4 + 3]
                > 32) {
                return x;
            }
        }
    }
    return -1;
}

TEST(text_rows_align_by_measured_width_and_preserve_other_rows)
{
    CHECK(IS_OK(Anm_InitText(nullptr, nullptr, 0)));
    CHECK(IS_OK(Anm_CreateTexture(100, 128, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 100,
        .texture_width = 128,
        .texture_height = 64,
        .y0 = 16,
        .x1 = 128,
        .width = 128,
    };
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0x001122ee, UINT32_MAX, "%s", "東")));
    CHECK(vm.shown);
    const IMAGE *const image = AnmTexture_GetImage(100);
    const int32_t left = M_FirstTextPixel(image, 16);
    CHECK(left >= 0 && left < 8);
    CHECK_EQ_INT(M_FirstTextPixel(image, 0), -1);
    bool colored = false;
    for (int32_t i = 0; i < image->width * image->height; i++) {
        const uint8_t *const pixel = image->pixels + (size_t)i * 4;
        if (pixel[3] > 128) {
            colored |= pixel[0] > pixel[1] && pixel[1] > pixel[2];
        }
    }
    CHECK(colored);
    CHECK(IS_OK(
        Anm_RenderTextRightAligned(&vm, 0xffffff, UINT32_MAX, "%s", "東")));
    const int32_t right = M_FirstTextPixel(AnmTexture_GetImage(100), 16);
    CHECK(right - left >= 112 && right - left <= 114);
    CHECK(IS_OK(Anm_RenderTextCentered(&vm, 0xffffff, UINT32_MAX, "%s", "東")));
    const int32_t center = M_FirstTextPixel(AnmTexture_GetImage(100), 16);
    CHECK(center - left >= 56 && center - left <= 58);
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, 0, "%s", " ")));
    CHECK_EQ_INT(M_FirstTextPixel(AnmTexture_GetImage(100), 16), -1);
    Anm_ReleaseTexture(100);
    Anm_ShutdownText();
}

TEST(text_rows_lay_out_markup)
{
    CHECK(IS_OK(Anm_InitText(nullptr, nullptr, 0)));
    CHECK(IS_OK(Anm_CreateTexture(100, 128, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 100,
        .texture_width = 128,
        .texture_height = 64,
        .y0 = 16,
        .x1 = 128,
        .width = 128,
    };
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, UINT32_MAX, "%s", "東")));
    const int32_t left = M_FirstTextPixel(AnmTexture_GetImage(100), 16);
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, UINT32_MAX, "%s", "<s$東>")));
    CHECK_EQ_INT(M_FirstTextPixel(AnmTexture_GetImage(100), 16), -1);
    CHECK(
        IS_OK(Anm_RenderText(&vm, 0xffffff, UINT32_MAX, "%s", "<r$東$東東>")));
    const int32_t right = M_FirstTextPixel(AnmTexture_GetImage(100), 16);
    CHECK(right - left >= 14 && right - left <= 16);
    Anm_ReleaseTexture(100);
    Anm_ShutdownText();
}

TEST(text_aligns_by_the_drawn_width_and_fills_the_sprite_row)
{
    CHECK(IS_OK(Anm_InitText(nullptr, nullptr, 0)));
    CHECK(IS_OK(Anm_CreateTexture(100, 256, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 100,
        .texture_width = 256,
        .texture_height = 64,
        .y0 = 16,
        .x1 = 192,
        .width = 192,
    };
    // Left text from x0 starts where the original puts aligned text.
    ANM_SPRITE moved = sprite;
    ANM_VM vm = { .sprite_data = &moved, .font_width = 15 };
    moved.x0 = 192.0f - 2 * 16 / 2;
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, UINT32_MAX, "%s", "Ab")));
    const int32_t right = M_FirstTextPixel(AnmTexture_GetImage(100), 16);
    moved.x0 = 192.0f / 2 - 2 * 16 / 4;
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, UINT32_MAX, "%s", "Ab")));
    const int32_t center = M_FirstTextPixel(AnmTexture_GetImage(100), 16);
    vm.sprite_data = &sprite;
    CHECK(IS_OK(
        Anm_RenderTextRightAligned(&vm, 0xffffff, UINT32_MAX, "%s", "Ab")));
    const int32_t plain = M_FirstTextPixel(AnmTexture_GetImage(100), 16);
    CHECK_EQ_INT(plain, right);
    CHECK(IS_OK(Anm_RenderTextCentered(&vm, 0xffffff, UINT32_MAX, "%s", "Ab")));
    CHECK_EQ_INT(M_FirstTextPixel(AnmTexture_GetImage(100), 16), center);
    CHECK(IS_OK(
        Anm_RenderTextRightAligned(&vm, 0xffffff, UINT32_MAX, "%s", "<l$Ab>")));
    CHECK_EQ_INT(M_FirstTextPixel(AnmTexture_GetImage(100), 16), plain);
    float width;
    CHECK(IS_OK(Anm_MeasureText("Ab", 15, &width)));
    CHECK(width == 15.0f);
    CHECK(IS_OK(Anm_MeasureText("<c$東$東東>", 15, &width)));
    CHECK(width == 30.0f);
    Anm_ReleaseTexture(100);
    Anm_ShutdownText();
    CHECK(IS_OK(Anm_MeasureText("Ab", 15, &width)));
    CHECK(width == 0.0f);
}

// Draws a 384 x 16 sprite of a 512 x 512 texture in 3D and returns the
// size of the drawn quad.
static VEC2 M_Draw3DSize(void)
{
    RNG rng;
    Rng_SetSeed(&rng, 5);
    Anm_Init(&g_HeapAllocator, &rng);
    CHECK(Result_Ignore(Anm_CreateTexture(0, 512, 512, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 0,
        .x1 = 384.0f,
        .y1 = 16.0f,
        .width = 384.0f,
        .height = 16.0f,
        .texture_width = 512.0f,
        .texture_height = 512.0f,
        .u1 = 0.75f,
        .v1 = 1.0f / 32.0f,
    };
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_InitVm(&vm);
    vm.sprite_data = &sprite;
    vm.shown = true;
    vm.uv_scale_x = 384.0f / 512.0f;
    vm.uv_scale_y = 16.0f / 512.0f;
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = (GFX_VIEW) {
        .view = Mat4_Identity(),
        .projection = Mat4_Identity(),
        .viewport = { 0, 0, 640, 480, 0, 1 },
    };
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Anm_Draw3D(&vm, &draw);
    Gfx_EndTick();
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(0);
    const VEC2 size = {
        quad->vertices[1].position.x - quad->vertices[0].position.x,
        quad->vertices[2].position.y - quad->vertices[0].position.y,
    };
    GfxRecorder_Stop();
    Anm_Shutdown();
    return size;
}

TEST(world_sprites_take_their_share_of_256_units)
{
    const VEC2 size = M_Draw3DSize();
    CHECK(size.x == 192.0f);
    CHECK(size.y == -8.0f);
}

TEST(base_tsa_world_sprites_take_their_size_in_pixels)
{
    char *const directory = TestDir_Prepare("anm_draw_base_tsa");
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &stack),
        LOG_LEVEL_ERROR));
    Translation_Init(stack);
    const VEC2 size = M_Draw3DSize();
    CHECK(size.x == 384.0f);
    CHECK(size.y == -16.0f);
    Translation_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(patches);
    Mem_Free(directory);
}

// Draws text as a text row of the bundled font does when it scales a
// region of surface_width x surface_height pixels at the doubled size to
// width x 16 pixels, and returns the row.
static IMAGE M_ScaleRow(
    const char *const text, const int32_t width, const int32_t surface_width,
    const int32_t surface_height)
{
    TEXT_FONT *font;
    CHECK(IS_OK(TextFont_LoadDefault(30.0f, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.color = (GFX_COLOR) { 0xee, 0x22, 0x11, 255 };
    style.shadow_x = 3;
    style.shadow_y = 2;
    IMAGE glyphs;
    CHECK(IS_OK(
        Text_RenderMarkup(font, text, &style, &g_HeapAllocator, &glyphs)));
    TextFont_Free(font);
    IMAGE row = Image_Create(surface_width, surface_height, &g_HeapAllocator);
    for (int32_t y = 0; y < glyphs.height && y < row.height; y++) {
        memcpy(
            row.pixels + (size_t)y * (size_t)row.width * 4,
            glyphs.pixels + (size_t)y * (size_t)glyphs.width * 4,
            (size_t)(glyphs.width < row.width ? glyphs.width : row.width) * 4);
    }
    Image_Free(&glyphs);
    Image_Quantize(&row, IMAGE_DEPTH_A1R5G5B5);
    Image_Resize(&row, width, 16);
    return row;
}

// Draws "Night Sign" in a row of width 200 at y 16 and returns whether the
// row holds the pixels of M_ScaleRow for the given region.
static bool M_DrawsScaledRow(
    const int32_t surface_width, const int32_t surface_height)
{
    CHECK(IS_OK(Anm_InitText(nullptr, nullptr, 0)));
    CHECK(IS_OK(Anm_CreateTexture(100, 256, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 100,
        .texture_width = 256,
        .texture_height = 64,
        .y0 = 16,
        .x1 = 200,
        .width = 200,
    };
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15 };
    const char *const text = "Night Sign";
    CHECK(IS_OK(Anm_RenderText(&vm, 0x001122ee, UINT32_MAX, "%s", text)));
    IMAGE expected = M_ScaleRow(text, 200, surface_width, surface_height);
    const IMAGE *const image = AnmTexture_GetImage(100);
    bool same = true;
    for (int32_t y = 0; y < 16; y++) {
        same = same
            && memcmp(
                   image->pixels + ((size_t)(16 + y) * 256) * 4,
                   expected.pixels + (size_t)y * 200 * 4, 200 * 4)
                == 0;
    }
    Image_Free(&expected);
    Anm_ReleaseTexture(100);
    Anm_ShutdownText();
    return same;
}

TEST(text_rows_scale_the_doubled_row_less_two_pixels)
{
    CHECK(M_DrawsScaledRow(398, 28));
}

TEST(base_tsa_text_rows_scale_twice_the_row_by_32_pixels)
{
    char *const directory = TestDir_Prepare("anm_text_base_tsa");
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &stack),
        LOG_LEVEL_ERROR));
    Translation_Init(stack);
    CHECK(M_DrawsScaledRow(400, 32));
    Translation_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(patches);
    Mem_Free(directory);
}

TEST(text_takes_the_patch_font_of_the_family)
{
    const char *const paths[] = { SDL_getenv("TEST_FONT") };
    CHECK(IS_OK(Anm_InitText("BIZ UDGothic", paths, 1)));
    Anm_ShutdownText();
    CHECK(IS_OK(Anm_InitText("Aroania", paths, 1)));
    CHECK(IS_OK(Anm_CreateTexture(100, 128, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 100,
        .texture_width = 128,
        .texture_height = 64,
        .y0 = 16,
        .x1 = 128,
        .width = 128,
    };
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, UINT32_MAX, "%s", "東")));
    CHECK(M_FirstTextPixel(AnmTexture_GetImage(100), 16) >= 0);
    Anm_ReleaseTexture(100);
    Anm_ShutdownText();
}

TEST(headless_text_sets_shown_without_a_font_or_texture)
{
    const ANM_SPRITE sprite = { .slot = 100, .texture_width = 128 };
    ANM_VM vm = { .sprite_data = &sprite };
    CHECK(IS_OK(Anm_RenderTextCentered(&vm, 0xffffff, 0, "%s", "東方")));
    CHECK(vm.shown);
}

TEST(texture_updates_keep_the_loaded_color_depth)
{
    CHECK(IS_OK(Anm_CreateTexture(100, 2, 2, ANM_TEXTURE_A4R4G4B4)));
    IMAGE image = Image_Create(1, 1, &g_HeapAllocator);
    memcpy(image.pixels, (uint8_t[]) { 128, 33, 245, 128 }, 4);
    CHECK(IS_OK(Anm_UpdateTexture(100, (GFX_RECT) { 1, 1, 1, 1 }, &image)));
    const uint8_t *const pixel = AnmTexture_GetImage(100)->pixels + 12;
    CHECK_EQ_INT(pixel[0], 136);
    CHECK_EQ_INT(pixel[1], 34);
    CHECK_EQ_INT(pixel[2], 255);
    CHECK_EQ_INT(pixel[3], 136);
    CHECK_EQ_INT(image.pixels[0], 128);
    Image_Free(&image);
    Anm_ReleaseTexture(100);
}

static void M_PutWord(uint8_t *const out, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        out[i] = (uint8_t)(value >> (i * 8));
    }
}

// Writes directory/name as a 4 x 4 PNG of one color.
static void M_WritePng(
    const char *const directory, const char *const name, const uint8_t red,
    const uint8_t blue, const uint8_t alpha)
{
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    for (int32_t i = 0; i < 16; i++) {
        image.pixels[i * 4] = red;
        image.pixels[i * 4 + 2] = blue;
        image.pixels[i * 4 + 3] = alpha;
    }
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    Th06Data_WriteFile(directory, name, bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
}

// Loads masked.anm, whose image img.png has the mask img_a.png, with the
// patch a/lang holding the given files, and returns the alpha of its first
// pixel.
static int32_t M_LoadMaskedAlpha(
    const char *const case_name, const bool patch_image, const bool patch_mask)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 4, 4);
    AnmBuilder_AddSprite(&builder, 0, 0, 0, 4, 4);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 0, nullptr, 0);
    char *const directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(directory, -1);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    size_t size = AnmBuilder_Build(&builder, bytes);
    M_PutWord(&bytes[28], (uint32_t)size);
    memcpy(&bytes[size], "img.png", 8);
    size += 8;
    M_PutWord(&bytes[36], (uint32_t)size);
    memcpy(&bytes[size], "img_a.png", 10);
    size += 10;
    Th06Data_WriteFile(directory, "masked.anm", bytes, size);
    M_WritePng(directory, "img.png", 255, 0, 255);
    M_WritePng(directory, "img_a.png", 0, 10, 255);
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    char *const game_dir = TestDir_Join(patches, "a/lang/th06");
    CHECK(Result_Log(File_MakeDir(game_dir), LOG_LEVEL_ERROR));
    Mem_Free(game_dir);
    if (patch_image) {
        M_WritePng(directory, "patches/a/lang/th06/img.png", 0, 255, 77);
    }
    if (patch_mask) {
        M_WritePng(directory, "patches/a/lang/th06/img_a.png", 0, 99, 255);
    }
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &stack),
        LOG_LEVEL_ERROR));
    M_Setup(directory);
    Files_SetPatches(stack);
    CHECK(Result_Log(Anm_Load(3, "masked.anm", 0), LOG_LEVEL_ERROR));
    const IMAGE *const image = AnmTexture_GetImage(3);
    const int32_t alpha = image->pixels != nullptr ? image->pixels[3] : -1;
    M_Teardown(directory);
    Patch_CloseStack(stack);
    Mem_Free(patches);
    return alpha;
}

TEST(the_mask_of_the_game_gives_its_image_alpha)
{
    CHECK_EQ_INT(M_LoadMaskedAlpha("mask_game", false, false), 10);
}

TEST(a_patch_image_without_a_patch_mask_keeps_its_alpha)
{
    CHECK_EQ_INT(M_LoadMaskedAlpha("mask_image", true, false), 77);
}

TEST(a_patch_mask_gives_the_patch_image_alpha)
{
    CHECK_EQ_INT(M_LoadMaskedAlpha("mask_both", true, true), 99);
}
