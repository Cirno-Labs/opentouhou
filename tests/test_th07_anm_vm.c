#include "th07/anm.h"
#include "th07/game.h"

#include "test.h"

#include "core/base/memory.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/rng.h"

#include <string.h>

#define F(x) { .f = (x) }
#define I(x) { .i = (x) }
#define INSTR(op_, time_, mask_, ...)                                          \
    (ANM_INSTR)                                                                \
    {                                                                          \
        .opcode = (op_), .time = (time_), .param_mask = (mask_),               \
        .args = { __VA_ARGS__ },                                               \
    }
#define END INSTR(-1, 100, 0, I(0))
#define M_MAX_INSTRS 32
#define M_SCRIPT 5

static RNG m_Rng;
static ANM_INSTR m_Instrs[M_MAX_INSTRS];
static ANM_SCRIPT m_Script;

static void M_Setup(void)
{
    g_GameSpeed = 1.0f;
    Rng_SetSeed(&m_Rng, 4);
    Anm_Init(&g_HeapAllocator, &m_Rng);
    ANM_SPRITE sprite = {
        .slot = 0,
        .x0 = 32.0f,
        .y0 = 0.0f,
        .x1 = 96.0f,
        .y1 = 16.0f,
        .texture_width = 256.0f,
        .texture_height = 64.0f,
        .u0 = 0.125f,
        .v0 = 0.0f,
        .u1 = 0.375f,
        .v1 = 0.25f,
        .width = 64.0f,
        .height = 16.0f,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
    };
    Anm_SetSpriteEntry(100, &sprite);
    sprite.x0 = 0.0f;
    Anm_SetSpriteEntry(101, &sprite);
}

static void M_Start(
    ANM_VM *const vm, const ANM_INSTR *const instrs, const int32_t count,
    const int32_t sprite_offset)
{
    memcpy(m_Instrs, instrs, (size_t)count * sizeof(ANM_INSTR));
    memset(&m_Script, 0, sizeof(m_Script));
    m_Script.instrs = m_Instrs;
    m_Script.count = count;
    m_Script.sprite_offset = sprite_offset;
    Anm_SetScriptEntry(M_SCRIPT, &m_Script);
    Anm_CreateVm(vm);
    Anm_StartScript(vm, M_SCRIPT);
}

#define START(vm_, ...)                                                        \
    do {                                                                       \
        const ANM_INSTR instrs_[] = { __VA_ARGS__ };                           \
        M_Start(                                                               \
            (vm_), instrs_, (int32_t)(sizeof(instrs_) / sizeof(instrs_[0])),   \
            0);                                                                \
    } while (0)

TEST(the_end_marker_and_delete_hide_and_static_keeps_shown)
{
    M_Setup();
    ANM_VM vm;
    START(&vm, INSTR(3, 0, 0, I(100)), INSTR(-1, 1, 0, I(0)));
    CHECK(vm.shown);
    CHECK(Anm_Step(&vm));
    CHECK(!vm.shown);
    CHECK_EQ_INT(vm.pc, -1);
    START(&vm, INSTR(3, 0, 0, I(100)), INSTR(1, 1, 0, I(0)), END);
    CHECK(Anm_Step(&vm));
    CHECK(!vm.shown);
    START(&vm, INSTR(3, 0, 0, I(100)), INSTR(2, 1, 0, I(0)), END);
    CHECK(Anm_Step(&vm));
    CHECK(vm.shown);
    CHECK(Anm_Step(&vm));
    Anm_Shutdown();
}

TEST(sprite_adds_the_offset_and_sets_the_matrices)
{
    M_Setup();
    ANM_VM vm;
    const ANM_INSTR instrs[] = { INSTR(3, 0, 0, I(1)), END };
    M_Start(&vm, instrs, 2, 100);
    CHECK_EQ_INT(vm.sprite, 101);
    CHECK(vm.matrix.m[0][0] == 0.25f);
    CHECK(vm.matrix.m[1][1] == 0.0625f);
    CHECK(vm.draw_matrix.m[0][0] == 0.25f);
    CHECK(vm.size_matrix.m[0][0] == 0.25f);
    CHECK(vm.size_matrix.m[1][1] == 0.25f);
    CHECK(vm.matrix.m[2][2] == 1.0f);
    Anm_Shutdown();
}

TEST(starting_a_missing_script_clears_the_vm)
{
    M_Setup();
    ANM_VM vm;
    Anm_CreateVm(&vm);
    vm.pos = (VEC3) { 1.0f, 2.0f, 3.0f };
    vm.font_width = 9;
    Anm_StartScript(&vm, 7);
    CHECK_EQ_INT(vm.script_index, 0);
    CHECK_EQ_INT(vm.sprite, 0);
    CHECK(vm.pos.x == 0.0f);
    CHECK_EQ_INT(vm.font_width, 0);
    CHECK(vm.script == nullptr);
    CHECK(Anm_Step(&vm));
    Anm_Shutdown();
}

TEST(jump_sets_the_time_its_argument_gives)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(8, 0, 0, I(0x10)), INSTR(4, 0, 0, I(3), I(7)),
        INSTR(8, 0, 0, I(0x20)), INSTR(8, 8, 0, I(0x30)), END);
    CHECK_EQ_INT(vm.color >> 24, 0x10);
    CHECK_EQ_INT(vm.pc, 3);
    CHECK_EQ_INT(vm.script_time.current, 8);
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.color >> 24, 0x30);
    Anm_Shutdown();
}

TEST(a_loop_counts_down_its_own_argument_in_the_script)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(39, 0, 1, I(10000), I(1)), INSTR(5, 0, 0, I(3), I(0), I(0)),
        END);
    CHECK_EQ_INT(vm.int_vars[0], 3);
    CHECK_EQ_INT(m_Instrs[1].args[0].i, 0);
    // The script keeps its counter at 0, so the next VM runs the body once.
    ANM_VM other;
    Anm_CreateVm(&other);
    Anm_StartScript(&other, M_SCRIPT);
    CHECK_EQ_INT(other.int_vars[0], 1);
    CHECK_EQ_INT(m_Instrs[1].args[0].i, -1);
    Anm_Shutdown();
}

TEST(a_loop_counts_down_a_variable)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(37, 0, 1, I(10008), I(4)), INSTR(41, 0, 1, I(10001), I(2)),
        INSTR(5, 0, 1, I(10008), I(1), I(0)), END);
    CHECK_EQ_INT(vm.int_vars[1], -8);
    CHECK_EQ_INT(vm.int_vars[4], 0);
    CHECK_EQ_INT(m_Instrs[2].args[0].i, 10008);
    Anm_Shutdown();
}

TEST(param_mask_bits_name_variables)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(37, 0, 1, I(10002), I(7)),
        INSTR(38, 0, 1, F(10005.0f), F(2.75f)),
        // An int argument naming a float variable truncates it.
        INSTR(37, 0, 3, I(10009), I(10005)),
        // A float argument naming an int variable converts it.
        INSTR(38, 0, 3, F(10006.0f), F(10002.0f)),
        // Without its mask bit, an argument is the number itself.
        INSTR(37, 0, 1, I(10003), I(10002)),
        // An int write to a float variable writes the script instead.
        INSTR(37, 0, 1, I(10004), I(1)), END);
    CHECK_EQ_INT(vm.int_vars[2], 7);
    CHECK(vm.float_vars[1] == 2.75f);
    CHECK_EQ_INT(vm.int_vars[5], 2);
    CHECK(vm.float_vars[2] == 7.0f);
    CHECK_EQ_INT(vm.int_vars[3], 10002);
    CHECK(vm.float_vars[0] == 0.0f);
    CHECK_EQ_INT(m_Instrs[5].args[0].i, 1);
    Anm_Shutdown();
}

TEST(int_arithmetic)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(37, 0, 1, I(10000), I(17)),
        INSTR(49, 0, 1, I(10001), I(5), I(3)),
        INSTR(51, 0, 1, I(10002), I(5), I(8)),
        INSTR(53, 0, 1, I(10003), I(-4), I(6)),
        INSTR(55, 0, 1, I(10008), I(-17), I(5)),
        INSTR(57, 0, 1, I(10009), I(-17), I(5)),
        INSTR(39, 0, 1, I(10000), I(3)), INSTR(41, 0, 1, I(10001), I(10)),
        INSTR(43, 0, 3, I(10002), I(10000)), INSTR(45, 0, 1, I(10003), I(5)),
        INSTR(47, 0, 1, I(10008), I(2)), INSTR(55, 0, 1, I(10009), I(1), I(0)),
        END);
    CHECK_EQ_INT(vm.int_vars[0], 20);
    CHECK_EQ_INT(vm.int_vars[1], -2);
    CHECK_EQ_INT(vm.int_vars[2], -60);
    CHECK_EQ_INT(vm.int_vars[3], -4);
    CHECK_EQ_INT(vm.int_vars[4], -1);
    CHECK_EQ_INT(vm.int_vars[5], 0);
    Anm_Shutdown();
}

TEST(float_arithmetic)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(50, 0, 1, F(10004.0f), F(0.1f), F(0.2f)),
        INSTR(52, 0, 1, F(10005.0f), F(1.0f), F(0.3f)),
        INSTR(54, 0, 1, F(10006.0f), F(0.1f), F(3.0f)),
        INSTR(56, 0, 1, F(10007.0f), F(1.0f), F(3.0f)),
        INSTR(40, 0, 1, F(10004.0f), F(1.0f)),
        INSTR(42, 0, 1, F(10005.0f), F(0.5f)),
        INSTR(44, 0, 1, F(10006.0f), F(2.0f)),
        INSTR(46, 0, 1, F(10007.0f), F(7.0f)), END);
    CHECK(vm.float_vars[0] == (0.1f + 0.2f) + 1.0f);
    CHECK(vm.float_vars[1] == (1.0f - 0.3f) - 0.5f);
    CHECK(vm.float_vars[2] == (0.1f * 3.0f) * 2.0f);
    CHECK(vm.float_vars[3] == (1.0f / 3.0f) / 7.0f);
    START(
        &vm, INSTR(38, 0, 1, F(10004.0f), F(-7.5f)),
        INSTR(48, 0, 3, F(10004.0f), F(2.0f)),
        INSTR(58, 0, 1, F(10005.0f), F(7.5f), F(-2.0f)),
        // Opcode 48 reads an int variable here and writes the script.
        INSTR(37, 0, 1, I(10000), I(7)), INSTR(48, 0, 1, F(10000.0f), F(4.0f)),
        END);
    CHECK(vm.float_vars[0] == -1.5f);
    CHECK(vm.float_vars[1] == 1.5f);
    CHECK(m_Instrs[4].args[0].f == 3.0f);
    Anm_Shutdown();
}

TEST(rand_draws_from_the_rng_in_order)
{
    M_Setup();
    RNG expected = m_Rng;
    const uint32_t first = Rng_U32InRange(&expected, 1000);
    const float second = Rng_F32InRange(&expected, 2.5f);
    const uint32_t third = Rng_U32InRange(&expected, 7);
    ANM_VM vm;
    START(
        &vm, INSTR(59, 0, 1, I(10000), I(1000)),
        INSTR(60, 0, 1, F(10004.0f), F(2.5f)), INSTR(37, 0, 1, I(10001), I(7)),
        INSTR(59, 0, 3, I(10002), I(10001)), END);
    CHECK_EQ_INT(vm.int_vars[0], (int32_t)first);
    CHECK(vm.float_vars[0] == second);
    CHECK_EQ_INT(vm.int_vars[2], (int32_t)third);
    CHECK_EQ_INT(m_Rng.generated, expected.generated);
    Anm_Shutdown();
}

TEST(trigonometry_and_angle_normalisation)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(61, 0, 1, F(10004.0f), F(0.7f)),
        INSTR(62, 0, 1, F(10005.0f), F(0.7f)),
        INSTR(63, 0, 1, F(10006.0f), F(0.7f)),
        INSTR(64, 0, 1, F(10007.0f), F(0.25f)), END);
    CHECK(vm.float_vars[0] == X87_Sin(0.7f));
    CHECK(vm.float_vars[1] == X87_Cos(0.7f));
    CHECK(vm.float_vars[2] == X87_Tan(0.7f));
    CHECK(vm.float_vars[3] == X87_Atan2(X87_Sqrt(1.0f - 0.25f * 0.25f), 0.25f));
    START(
        &vm, INSTR(65, 0, 1, F(10004.0f), F(0.7f)),
        INSTR(38, 0, 1, F(10005.0f), F(5.0f)), INSTR(66, 0, 1, F(10005.0f)),
        END);
    CHECK(vm.float_vars[0] == X87_Atan2(0.7f, 1.0f));
    CHECK(vm.float_vars[1] == 5.0f - 6.2831855f);
    Anm_Shutdown();
}

TEST(conditional_jumps)
{
    static const struct {
        int16_t opcode;
        ANM_ARG taken[2], not_taken[2];
    } cases[] = {
        { 67, { I(3), I(3) }, { I(3), I(4) } },
        { 68, { F(1.5f), F(1.5f) }, { F(1.5f), F(2.0f) } },
        { 69, { I(3), I(4) }, { I(3), I(3) } },
        { 70, { F(1.5f), F(2.0f) }, { F(1.5f), F(1.5f) } },
        { 71, { I(-1), I(0) }, { I(0), I(0) } },
        { 72, { F(-1.0f), F(0.0f) }, { F(0.0f), F(0.0f) } },
        { 73, { I(0), I(0) }, { I(1), I(0) } },
        { 74, { F(0.0f), F(0.0f) }, { F(1.0f), F(0.0f) } },
        { 75, { I(1), I(0) }, { I(0), I(0) } },
        { 76, { F(1.0f), F(0.0f) }, { F(0.0f), F(0.0f) } },
        { 77, { I(0), I(0) }, { I(-1), I(0) } },
        { 78, { F(0.0f), F(0.0f) }, { F(-1.0f), F(0.0f) } },
    };
    M_Setup();
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        for (int32_t taken = 0; taken < 2; taken++) {
            const ANM_ARG *const args =
                taken ? cases[i].taken : cases[i].not_taken;
            ANM_VM vm;
            START(
                &vm, INSTR(cases[i].opcode, 0, 0, args[0], args[1], I(3), I(9)),
                INSTR(37, 0, 1, I(10000), I(1)), INSTR(2, 0, 0, I(0)),
                INSTR(37, 9, 1, I(10001), I(1)), END);
            CHECK_EQ_INT(vm.int_vars[0], taken ? 0 : 1);
            CHECK_EQ_INT(vm.int_vars[1], taken ? 1 : 0);
        }
    }
    ANM_VM vm;
    START(
        &vm, INSTR(37, 0, 1, I(10000), I(2)),
        INSTR(67, 0, 1, I(10000), I(2), I(3), I(0)), INSTR(2, 0, 0, I(0)),
        INSTR(37, 0, 1, I(10001), I(5)), END);
    CHECK_EQ_INT(vm.int_vars[1], 5);
    Anm_Shutdown();
}

TEST(each_ease_mode_shapes_the_position)
{
    const float t = 1.0f / 5.0f;
    const float s = 1.0f - t;
    const float t2 = t * t;
    const float s2 = s * s;
    const float expected[7] = {
        t,
        t * t,
        t * t * t,
        t2 * t2,
        1.0f - s * s,
        1.0f - s * s * s,
        1.0f - s2 * s2,
    };
    M_Setup();
    for (int32_t mode = 0; mode < 7; mode++) {
        ANM_VM vm;
        START(
            &vm, INSTR(32, 0, 0, I(5), I(mode), F(1.0f), F(2.0f), F(-4.0f)),
            END);
        CHECK(vm.pos.x == expected[mode]);
        CHECK(vm.pos.y == 2.0f * expected[mode]);
        CHECK(vm.pos.z == -4.0f * expected[mode]);
        for (int32_t i = 0; i < 4; i++) {
            Anm_Step(&vm);
        }
        CHECK(vm.pos.x == 1.0f);
        CHECK_EQ_INT(vm.interp_length[ANM_INTERP_POS], 0);
    }
    Anm_Shutdown();
}

TEST(the_old_move_opcodes_ease_linear_out_and_out4)
{
    const float t = 1.0f / 5.0f;
    const float s = 1.0f - t;
    const float s2 = s * s;
    const float expected[3] = { t, 1.0f - s * s, 1.0f - s2 * s2 };
    M_Setup();
    for (int32_t i = 0; i < 3; i++) {
        ANM_VM vm;
        START(
            &vm, INSTR(24, 0, 0, I(1)),
            INSTR(17 + i, 0, 0, F(10.0f), F(0.0f), F(0.0f), I(5)), END);
        CHECK(vm.pos_offset.x == 10.0f * expected[i]);
        CHECK(vm.pos.x == 0.0f);
    }
    Anm_Shutdown();
}

TEST(color_and_alpha_interpolations_truncate)
{
    M_Setup();
    ANM_VM vm;
    // Opcode 33 takes red from the argument's low byte.
    START(
        &vm, INSTR(9, 0, 0, I(0x102030)),
        INSTR(33, 0, 0, I(3), I(0), I(0x000080ff)),
        INSTR(34, 0, 0, I(3), I(0), I(0x7f)), END);
    CHECK_EQ_INT((vm.color >> 16) & 0xff, 0x10 + (0xff - 0x10) / 3);
    CHECK_EQ_INT(
        (vm.color >> 8) & 0xff, (int32_t)(96.0f * (1.0f / 3.0f) + 32.0f));
    CHECK_EQ_INT(vm.color & 0xff, 0x30 - 0x30 / 3);
    CHECK_EQ_INT(vm.color >> 24, 212);
    Anm_Step(&vm);
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.color, 0x7fff8000u);
    START(&vm, INSTR(15, 0, 0, I(0x100), I(2)), END);
    CHECK_EQ_INT(vm.color, 0x7fffffffu);
    Anm_Shutdown();
}

TEST(rotation_and_scale_interpolations)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(12, 0, 0, F(0.0f), F(0.0f), F(3.0f)),
        INSTR(35, 0, 0, I(2), I(0), F(0.0f), F(0.0f), F(4.0f)),
        INSTR(36, 0, 0, I(4), I(1), F(3.0f), F(-1.0f)), END);
    CHECK(vm.rotation.z == Angle_AddNormalize((4.0f - 3.0f) * 0.5f, 3.0f));
    CHECK(vm.rotation.z < 0.0f);
    CHECK(vm.scale_x == 2.0f * 0.0625f + 1.0f);
    CHECK(vm.scale_y == -2.0f * 0.0625f + 1.0f);
    CHECK(vm.scale_changed);
    START(&vm, INSTR(29, 0, 0, F(3.0f), F(5.0f), I(2)), END);
    CHECK(vm.scale_x == 2.0f);
    CHECK(vm.scale_y == 3.0f);
    Anm_Shutdown();
}

TEST(an_interrupt_jumps_past_its_label_or_the_last_minus_one)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(20, 0, 0, I(0)), INSTR(21, 0, 0, I(-1)),
        INSTR(37, 0, 1, I(10000), I(1)), INSTR(21, 0, 0, I(-1)),
        INSTR(37, 3, 1, I(10000), I(2)), INSTR(23, 4, 0, I(0)),
        INSTR(21, 0, 0, I(5)), INSTR(37, 6, 1, I(10001), I(3)), END);
    CHECK(vm.stopped);
    CHECK_EQ_INT(vm.script_time.current, 0);
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.script_time.current, 0);
    vm.pending_interrupt = 9;
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.int_vars[0], 2);
    CHECK_EQ_INT(vm.pc, 5);
    CHECK(!vm.stopped);
    CHECK(vm.shown);
    Anm_Step(&vm);
    CHECK(!vm.shown);
    CHECK(vm.stopped);
    vm.pending_interrupt = 5;
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.int_vars[1], 3);
    CHECK_EQ_INT(vm.script_time.current, 7);
    Anm_Shutdown();
}

TEST(an_interrupt_without_a_label_holds_the_script)
{
    M_Setup();
    ANM_VM vm;
    START(&vm, INSTR(37, 1, 1, I(10000), I(1)), END);
    vm.pending_interrupt = 4;
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.script_time.current, 1);
    CHECK_EQ_INT(vm.int_vars[0], 0);
    CHECK_EQ_INT(vm.pending_interrupt, 0);
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.int_vars[0], 1);
    Anm_Shutdown();
}

TEST(wait_holds_for_its_frames)
{
    M_Setup();
    ANM_VM vm;
    START(&vm, INSTR(79, 0, 0, I(3)), INSTR(37, 0, 1, I(10000), I(1)), END);
    int32_t steps = 0;
    while (vm.int_vars[0] == 0 && steps < 10) {
        Anm_Step(&vm);
        steps++;
    }
    CHECK_EQ_INT(steps, 3);
    CHECK_EQ_INT(vm.wait_timer.current, 0);
    CHECK_EQ_INT(vm.script_time.current, 1);
    Anm_Shutdown();
}

TEST(timers_and_speeds_run_at_the_game_speed)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(13, 0, 0, F(0.0f), F(0.0f), F(0.25f)),
        INSTR(14, 0, 0, F(0.5f), F(0.0f)), INSTR(80, 0, 0, F(0.25f)),
        INSTR(32, 0, 0, I(4), I(0), F(4.0f), F(0.0f), F(0.0f)),
        INSTR(37, 1, 1, I(10000), I(1)), END);
    g_GameSpeed = 0.5f;
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.script_time.current, 1);
    CHECK(vm.script_time.sub_frame == 0.5f);
    CHECK_EQ_INT(vm.int_vars[0], 1);
    CHECK(vm.rotation.z == 0.25f + 0.125f);
    CHECK(vm.scale_x == 1.5f + 0.25f);
    CHECK(vm.uv_scroll_x == 0.5f);
    CHECK(vm.pos.x == 1.5f);
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.script_time.current, 2);
    CHECK(vm.script_time.sub_frame == 0.0f);
    CHECK(vm.pos.x == 2.0f);
    g_GameSpeed = 1.0f;
    Anm_Shutdown();
}

TEST(stops_and_waits_hold_by_a_fraction_at_the_game_speed)
{
    M_Setup();
    ANM_VM vm;
    START(&vm, INSTR(20, 0, 0, I(0)), END);
    g_GameSpeed = 0.5f;
    for (int32_t i = 0; i < 3; i++) {
        Anm_Step(&vm);
        CHECK_EQ_INT(vm.script_time.current, 0);
        CHECK(vm.script_time.sub_frame == 0.0f);
    }
    START(&vm, INSTR(79, 0, 0, I(2)), END);
    Anm_Step(&vm);
    CHECK_EQ_INT(vm.wait_timer.current, 1);
    CHECK(vm.wait_timer.sub_frame == 0.5f);
    g_GameSpeed = 1.0f;
    Anm_Shutdown();
}

TEST(texture_scroll_wraps)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(26, 0, 0, F(0.75f)), INSTR(26, 0, 0, F(0.5f)),
        INSTR(27, 0, 0, F(-0.25f)), INSTR(81, 0, 0, F(0.5f)), END);
    CHECK(vm.uv_scroll_x == 0.25f);
    CHECK(vm.uv_scroll_y == 0.25f);
    Anm_Step(&vm);
    CHECK(vm.uv_scroll_y == 0.75f);
    Anm_Step(&vm);
    CHECK(vm.uv_scroll_y == 0.25f);
    Anm_Shutdown();
}

TEST(state_opcodes_set_their_flags)
{
    M_Setup();
    ANM_VM vm;
    START(
        &vm, INSTR(6, 0, 0, F(1.0f), F(2.0f), F(3.0f)),
        INSTR(7, 0, 0, F(2.0f), F(3.0f)), INSTR(10, 0, 0, I(0)),
        INSTR(16, 0, 0, I(3)), INSTR(22, 0, 0, I(0)), INSTR(25, 0, 0, I(1)),
        INSTR(28, 0, 0, I(2)), INSTR(30, 0, 0, I(1)), INSTR(31, 0, 0, I(1)),
        END);
    CHECK(vm.pos.y == 2.0f);
    CHECK(vm.scale_x == -2.0f);
    CHECK_EQ_INT(vm.flip, 1);
    CHECK(vm.blend_add);
    CHECK_EQ_INT(vm.anchor, 3);
    CHECK_EQ_INT(vm.auto_rotate, 1);
    CHECK(!vm.shown);
    CHECK(vm.z_write_off);
    CHECK(vm.stage_camera);
    CHECK(vm.scale_changed);
    Anm_Shutdown();
}

TEST(series_start_scripts_in_turn_and_step_vms_skips_negative_scripts)
{
    M_Setup();
    const ANM_INSTR first[] = { INSTR(3, 0, 0, I(100)), INSTR(-1, 1, 0, I(0)) };
    const ANM_INSTR second[] = { INSTR(3, 0, 0, I(101)),
                                 INSTR(-1, 1, 0, I(0)) };
    ANM_INSTR instrs[4];
    memcpy(instrs, first, sizeof(first));
    memcpy(instrs + 2, second, sizeof(second));
    const ANM_SCRIPT scripts[2] = {
        { .instrs = instrs, .count = 2 },
        { .instrs = instrs + 2, .count = 2 },
    };
    Anm_SetScriptEntry(10, &scripts[0]);
    Anm_SetScriptEntry(11, &scripts[1]);
    ANM_VM vms[2];
    Anm_CreateVm(&vms[0]);
    Anm_CreateVm(&vms[1]);
    Anm_StartVmSeries(vms, 10, 2);
    CHECK_EQ_INT(vms[0].base_sprite, 100);
    CHECK_EQ_INT(vms[1].base_sprite, 101);
    CHECK_EQ_INT(vms[1].font_height, 15);
    vms[1].script_index = -1;
    Anm_StepVms(vms, 2);
    CHECK_EQ_INT(vms[0].pc, -1);
    CHECK_EQ_INT(vms[1].pc, 1);
    Anm_SendInterrupt(vms, 2, 3);
    CHECK_EQ_INT(vms[0].pending_interrupt, 0);
    Anm_Shutdown();
}
