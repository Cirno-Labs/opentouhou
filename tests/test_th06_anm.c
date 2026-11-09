#include "th06/anm.h"

#include "th06/files.h"
#include "th06/timer.h"

#include "anm_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "core/base/memory.h"

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
