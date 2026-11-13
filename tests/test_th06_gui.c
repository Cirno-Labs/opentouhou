#include "th06/gui.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/anm/private.h"
#include "th06/ascii.h"
#include "th06/chain.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/note.h"
#include "th06/scene.h"
#include "th06/sound.h"
#include "th06/stage.h"
#include "th06/translation.h"

#include "core/base/memory.h"
#include "core/image.h"
#include "core/io/file.h"
#include "core/io/writer.h"
#include "core/gfx/private.h"
#include "core/math/x87.h"
#include "core/patch.h"
#include "core/base/utils.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_MAX_SCRIPTS 4
#define M_SCRIPT_SIZE 512
#define M_MSG_SIZE 4096
#define M_LEAVE_FRAMES 100
#define M_APPEAR_FRAMES 3
#define M_FACE12C_END 7
#define M_KOUMA "\x8d\x67\x96\x82" // 紅魔 in CP932

typedef struct {
    uint8_t bytes[M_SCRIPT_SIZE];
    size_t size;
} M_SCRIPT;

static char *m_Directory;
static PATCH_STACK *m_Patches;
static M_SCRIPT m_Scripts[M_MAX_SCRIPTS];
static int32_t m_ScriptCount;

static void M_Instr(
    M_SCRIPT *const script, const uint16_t time, const uint8_t opcode,
    const void *const args, const uint8_t size)
{
    uint8_t *const at = &script->bytes[script->size];
    memcpy(at, &time, 2);
    at[2] = opcode;
    at[3] = size;
    memcpy(at + 4, args, size);
    script->size += 4 + (size_t)size;
}

static M_SCRIPT *M_Script(void)
{
    M_SCRIPT *const script = &m_Scripts[m_ScriptCount++];
    *script = (M_SCRIPT) {};
    return script;
}

static void M_Op(M_SCRIPT *const script, const uint16_t time, const uint8_t op)
{
    M_Instr(script, time, op, nullptr, 0);
}

static void M_OpInt(
    M_SCRIPT *const script, const uint16_t time, const uint8_t op,
    const int32_t value)
{
    M_Instr(script, time, op, &value, 4);
}

static void M_OpWho(
    M_SCRIPT *const script, const uint16_t time, const uint8_t op,
    const int16_t who, const int16_t index)
{
    const int16_t args[2] = { who, index };
    M_Instr(script, time, op, args, 4);
}

static void M_OpText(
    M_SCRIPT *const script, const uint16_t time, const uint8_t op,
    const int16_t who, const int16_t index, const char *const text)
{
    uint8_t args[64] = {};
    memcpy(&args[0], &who, 2);
    memcpy(&args[2], &index, 2);
    const size_t length = strlen(text);
    memcpy(&args[4], text, length);
    M_Instr(script, time, op, args, (uint8_t)(4 + length + 1));
}

static void M_OpInterrupt(
    M_SCRIPT *const script, const uint16_t time, const int16_t who,
    const uint8_t label)
{
    uint8_t args[4] = {};
    memcpy(&args[0], &who, 2);
    args[2] = label;
    M_Instr(script, time, 5, args, 4);
}

static void M_OpByte(
    M_SCRIPT *const script, const uint16_t time, const uint8_t op,
    const uint8_t value)
{
    const uint8_t args[4] = { value };
    M_Instr(script, time, op, args, 4);
}

static void M_WriteMsg(const char *const name)
{
    static uint8_t bytes[M_MSG_SIZE];
    memset(bytes, 0, sizeof(bytes));
    memcpy(bytes, &m_ScriptCount, 4);
    size_t at = 4 + 4 * (size_t)m_ScriptCount;
    for (int32_t i = 0; i < m_ScriptCount; i++) {
        const int32_t offset = (int32_t)at;
        memcpy(&bytes[4 + 4 * i], &offset, 4);
        memcpy(&bytes[at], m_Scripts[i].bytes, m_Scripts[i].size);
        at += m_Scripts[i].size;
    }
    Th06Data_WriteFile(m_Directory, name, bytes, at);
}

// Writes an ANM file with one sprite and the given scripts, each showing
// the sprite on frame 0 and keeping it until frame end.
static void M_WriteAnm(
    const char *const name, const int32_t *const ids, const int16_t *const ends,
    const int32_t count)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    AnmBuilder_AddSprite(&builder, 1, 16.0f, 0.0f, 16.0f, 16.0f);
    const uint32_t sprite = 0;
    for (int32_t i = 0; i < count; i++) {
        const int32_t script = AnmBuilder_BeginScript(&builder, ids[i]);
        AnmBuilder_Instr(&builder, script, 0, 1, &sprite, 1);
        AnmBuilder_Instr(&builder, script, ends[i], 15, nullptr, 0);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, name, bytes, size);
}

static void M_WriteFace(const char *const name, const int16_t end)
{
    const int32_t ids[] = { 0, 1 };
    const int16_t ends[] = { end, end };
    M_WriteAnm(name, ids, ends, 2);
}

// Writes the loading screen: it shows its sprite, stops on frame 2, and
// ends on the third frame after interrupt 1.
static void M_WriteLoading(void)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    const uint32_t sprite = 0;
    const uint32_t label = 1;
    const uint32_t alpha = 0xff;
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    AnmBuilder_Instr(&builder, script, 0, 1, &sprite, 1);
    AnmBuilder_Instr(&builder, script, 2, 21, nullptr, 0);
    AnmBuilder_Instr(&builder, script, 2, 22, &label, 1);
    AnmBuilder_Instr(&builder, script, 2, 3, &alpha, 1);
    AnmBuilder_Instr(&builder, script, 5, 0, nullptr, 0);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, "data/loading.anm", bytes, size);
}

static void M_WriteData(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    const int32_t front_ids[] = {
        0x13, 0x14, 0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107,
    };
    int16_t front_ends[ARRAY_SIZE(front_ids)];
    for (size_t i = 0; i < ARRAY_SIZE(front_ids); i++) {
        front_ends[i] = 1000;
    }
    front_ends[0] = M_APPEAR_FRAMES;
    front_ends[1] = M_LEAVE_FRAMES;
    M_WriteAnm(
        "data/front.anm", front_ids, front_ends,
        (int32_t)ARRAY_SIZE(front_ids));
    M_WriteLoading();
    M_WriteFace("data/face00a.anm", 1000);
    M_WriteFace("data/face00b.anm", 1000);
    M_WriteFace("data/face00c.anm", 1000);
    M_WriteFace("data/face03a.anm", 1000);
    M_WriteFace("data/face03b.anm", 1000);
    M_WriteFace("data/face08a.anm", 1000);
    M_WriteFace("data/face12a.anm", 1000);
    M_WriteFace("data/face12b.anm", 1000);
    M_WriteFace("data/face12c.anm", M_FACE12C_END);
    M_WriteMsg("data/msg1.dat");
    M_WriteMsg("data/msg7.dat");
}

static RESULT M_SetupStage(const char *const case_name, const int32_t stage)
{
    M_WriteData(case_name);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Anm_Init(&g_HeapAllocator, &g_Rng);
    AsciiText_Initialize();
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = stage;
    g_GameManager.run.difficulty = 1;
    g_Buttons = (BUTTONS) {};
    Scene_Request(SCENE_GAMEPLAY);
    m_ScriptCount = 0;
    return Gui_Register();
}

static void M_Setup(const char *const case_name)
{
    CHECK(Result_Log(M_SetupStage(case_name, 1), LOG_LEVEL_ERROR));
}

// Sets up stage 1 with a patch whose jdiff for msg1.dat is diff.
static void M_SetupPatched(const char *const case_name, const char *const diff)
{
    M_WriteData(case_name);
    char *const patches = TestDir_Join(m_Directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    PatchBuilder_Write(patches, "a/lang/th06/msg1.dat.jdiff", diff);
    m_Patches = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &m_Patches),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Files_SetPatches(m_Patches);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    AsciiText_Initialize();
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    g_GameManager.run.difficulty = 1;
    g_Buttons = (BUTTONS) {};
    Scene_Request(SCENE_GAMEPLAY);
    m_ScriptCount = 0;
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    AsciiText_Initialize();
    Anm_Shutdown();
    Files_Shutdown();
    Patch_CloseStack(m_Patches);
    m_Patches = nullptr;
    Mem_Free(m_Directory);
}

static void M_Run(const int32_t frames)
{
    for (int32_t i = 0; i < frames; i++) {
        Chain_RunCalc(&g_Chain);
    }
}

TEST(registering_loads_the_interface)
{
    M_Setup("register");
    CHECK(Anm_GetScript(0x613) != nullptr);
    CHECK(Anm_GetScript(0x619) != nullptr);
    CHECK(Anm_GetScript(0x4a0) != nullptr);
    CHECK(Anm_GetScript(0x4a8) != nullptr);
    CHECK(g_Gui.hud.sprites[0x13].script == Anm_GetScript(0x613));
    CHECK(g_Gui.banners.song_name.script == Anm_GetScript(0x701));
    CHECK_EQ_INT(g_Gui.banners.song_name.font_width, 16);
    CHECK(g_Gui.banners.bomb_name.script == nullptr);
    CHECK(!g_Gui.banners.bomb_name.shown);
    CHECK_EQ_INT(g_Gui.banners.bomb_name.font_height, 15);
    CHECK_EQ_INT(g_Gui.loading.sprite, -1);
    CHECK(!Gui_IsMsgRunning());
    M_Teardown();
}

TEST(removing_releases_the_files)
{
    M_Op(M_Script(), 0, 0);
    M_Setup("remove");
    Gui_Remove();
    CHECK(Anm_GetScript(0x613) == nullptr);
    CHECK(Anm_GetScript(0x4a0) == nullptr);
    CHECK(Anm_GetScript(0x4a8) == nullptr);
    Gui_StartMsg(0);
    CHECK(!Gui_IsMsgRunning());
    M_Teardown();
}

TEST(the_next_stage_keeps_the_interface)
{
    M_Op(M_Script(), 0, 0);
    M_Setup("next_stage");
    g_Gui.popups.bonus.value = 1234;
    Scene_Request(SCENE_NEXT_STAGE);
    Gui_Remove();
    CHECK(Anm_GetScript(0x613) != nullptr);
    CHECK(Anm_GetScript(0x4a0) != nullptr);
    CHECK(Anm_GetScript(0x4a8) == nullptr);
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_Gui.popups.bonus.value, 1234);
    CHECK(Anm_GetScript(0x4a8) != nullptr);
    CHECK(g_Gui.loading.script == Anm_GetScript(0x619));
    CHECK_EQ_INT(g_Gui.loading.pending_interrupt, 1);
    M_Teardown();
}

TEST(a_script_without_an_end_fails_the_add)
{
    M_OpInt(M_Script(), 0, 6, 0);
    CHECK(!Result_Log(M_SetupStage("no_end", 1), LOG_LEVEL_INFO));
    CHECK(Anm_GetScript(0x4a8) == nullptr);
    CHECK(Anm_GetScript(0x613) == nullptr);
    M_Teardown();
}

TEST(a_text_past_its_instruction_fails_the_add)
{
    M_SCRIPT *const script = M_Script();
    const int16_t args[2] = { 0, 0 };
    M_Instr(script, 0, 3, args, 4);
    M_Op(script, 0, 0);
    CHECK(!Result_Log(M_SetupStage("short_text", 1), LOG_LEVEL_INFO));
    M_Teardown();
}

TEST(a_text_without_its_end_fails_the_add)
{
    const uint8_t args[6] = { 0, 0, 0, 0, 'a', 'b' };
    M_Instr(M_Script(), 0, 3, args, 6);
    M_Op(&m_Scripts[0], 0, 0);
    CHECK(!Result_Log(M_SetupStage("open_text", 1), LOG_LEVEL_INFO));
    M_Teardown();
}

TEST(a_line_out_of_range_fails_the_add)
{
    M_OpText(M_Script(), 0, 3, 0, 2, "a");
    M_Op(&m_Scripts[0], 0, 0);
    CHECK(!Result_Log(M_SetupStage("bad_line", 1), LOG_LEVEL_INFO));
    M_Teardown();
}

TEST(a_portrait_side_out_of_range_fails_the_add)
{
    M_OpWho(M_Script(), 0, 1, 2, 0);
    M_Op(&m_Scripts[0], 0, 0);
    CHECK(!Result_Log(M_SetupStage("bad_side", 1), LOG_LEVEL_INFO));
    M_Teardown();
}

TEST(a_script_ends_at_a_halt)
{
    M_Op(M_Script(), 0, 10);
    M_Op(M_Script(), 0, 11);
    CHECK(Result_Log(M_SetupStage("halt_end", 1), LOG_LEVEL_ERROR));
    M_Teardown();
}

TEST(starting_a_missing_script_does_nothing)
{
    M_Op(M_Script(), 0, 0);
    M_Setup("missing");
    Gui_StartMsg(1);
    CHECK(!Gui_IsMsgRunning());
    Gui_StartMsg(-1);
    CHECK(!Gui_IsMsgRunning());
    Gui_StartMsg(0);
    CHECK(Gui_IsMsgRunning());
    CHECK(Gui_IsMsgSkippable());
    M_Teardown();
}

TEST(an_instruction_runs_on_its_frame)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 2, 6);
    M_Op(script, 4, 0);
    M_Setup("frames");
    Gui_StartMsg(0);
    M_Run(2);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 0);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 3);
    M_Run(1);
    CHECK(Gui_IsMsgRunning());
    M_Run(1);
    CHECK(!Gui_IsMsgRunning());
    M_Teardown();
}

TEST(a_release_lets_the_timeline_run_for_one_frame)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 0, 6);
    M_Op(script, 0, 10);
    M_Setup("release");
    Gui_StartMsg(0);
    CHECK(Gui_MsgHoldsTimeline());
    M_Run(1);
    CHECK(!Gui_MsgHoldsTimeline());
    M_Run(1);
    CHECK(Gui_MsgHoldsTimeline());
    M_Teardown();
}

TEST(a_halt_holds_the_script_and_its_timer)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 0, 10);
    M_Op(script, 0, 6);
    M_Op(script, 0, 0);
    M_Setup("halt");
    Gui_StartMsg(0);
    M_Run(5);
    CHECK(Gui_IsMsgRunning());
    CHECK_EQ_INT(g_Gui.msg.pc, 0);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 0);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 0);
    M_Teardown();
}

TEST(a_wait_ends_after_its_frames)
{
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 4, 3);
    M_Op(script, 0, 6);
    M_Op(script, 0, 10);
    M_Setup("wait");
    Gui_StartMsg(0);
    M_Run(3);
    CHECK_EQ_INT(g_Gui.msg.wait_frames, 3);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 0);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 0);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 1);
    M_Teardown();
}

TEST(the_shoot_button_ends_a_wait_after_8_frames)
{
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 4, 1000);
    M_Op(script, 0, 6);
    M_Op(script, 0, 10);
    M_Setup("shoot");
    Gui_StartMsg(0);
    g_Buttons.held.shoot = true;
    g_Buttons.pressed.shoot = true;
    M_Run(8);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 0);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 1);
    M_Teardown();
}

TEST(a_held_shoot_button_does_not_end_a_wait)
{
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 4, 1000);
    M_Op(script, 0, 6);
    M_Op(script, 0, 10);
    M_Setup("held_shoot");
    Gui_StartMsg(0);
    g_Buttons.held.shoot = true;
    M_Run(20);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 0);
    CHECK_EQ_INT(g_Gui.msg.wait_frames, 20);
    M_Teardown();
}

TEST(skip_ends_a_wait)
{
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 4, 1000);
    M_Op(script, 0, 6);
    M_Op(script, 0, 10);
    M_Setup("skip_wait");
    Gui_StartMsg(0);
    g_Buttons.held.skip = true;
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 1);
    M_Teardown();
}

TEST(skip_moves_the_timer_to_the_next_instruction)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 100, 6);
    M_Op(script, 200, 0);
    M_Setup("skip_timer");
    Gui_StartMsg(0);
    g_Buttons.held.skip = true;
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 101);
    M_Run(1);
    CHECK(!Gui_IsMsgRunning());
    M_Teardown();
}

TEST(skip_opens_the_box_at_once)
{
    M_Op(M_Script(), 0, 10);
    M_Setup("skip_box");
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 0);
    g_Buttons.held.skip = true;
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 60);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 60);
    M_Teardown();
}

TEST(an_unskippable_script_ignores_skip)
{
    M_SCRIPT *const script = M_Script();
    M_OpByte(script, 0, 13, 0);
    M_OpInt(script, 0, 4, 1000);
    M_Op(script, 0, 6);
    M_Op(script, 0, 10);
    M_Setup("unskippable");
    Gui_StartMsg(0);
    g_Buttons.held.skip = true;
    M_Run(3);
    CHECK(!Gui_IsMsgSkippable());
    CHECK_EQ_INT(g_Gui.msg.timeline_release, 0);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 0);
    M_Teardown();
}

TEST(the_portrait_instructions_start_scripts_and_sprites)
{
    M_SCRIPT *const script = M_Script();
    M_OpWho(script, 0, 1, 0, 1);
    M_OpWho(script, 0, 1, 1, 0);
    M_OpWho(script, 0, 2, 0, 1);
    M_OpWho(script, 0, 2, 1, 1);
    M_Op(script, 0, 10);
    M_Setup("portraits");
    Gui_StartMsg(0);
    M_Run(1);
    CHECK(g_Gui.msg.portraits[0].script == Anm_GetScript(0x4a1));
    CHECK(g_Gui.msg.portraits[1].script == Anm_GetScript(0x4a2));
    CHECK_EQ_INT(g_Gui.msg.portraits[0].sprite, 0x4a1);
    CHECK_EQ_INT(g_Gui.msg.portraits[1].sprite, 0x4a9);
    M_Teardown();
}

TEST(a_line_shows_its_text_in_utf8)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 0, 3, 0, 1, M_KOUMA);
    M_OpText(script, 0, 8, 1, 0, "Reimu");
    M_Op(script, 0, 10);
    M_Setup("line");
    Gui_StartMsg(0);
    g_Gui.msg.wait_frames = 5;
    M_Run(1);
    CHECK_EQ_STR(g_Gui.msg.line_texts[1], "紅魔");
    CHECK(g_Gui.msg.lines[1].script == Anm_GetScript(0x703));
    CHECK_EQ_INT(g_Gui.msg.lines[1].font_width, 15);
    CHECK_EQ_INT(g_Gui.msg.lines[1].font_height, 15);
    CHECK_EQ_STR(g_Gui.msg.nameplate_texts[0], "Reimu");
    CHECK(g_Gui.msg.nameplates[0].script == Anm_GetScript(0x704));
    CHECK_EQ_INT(g_Gui.msg.wait_frames, 0);
    M_Teardown();
}

TEST(a_line_that_is_utf8_shows_as_it_is)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 0, 3, 0, 0, "Hello ♪");
    M_Op(script, 0, 10);
    M_Setup("line_utf8");
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "Hello ♪");
    M_Teardown();
}

TEST(a_first_line_blanks_a_shown_second_line)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 0, 3, 0, 0, "one");
    M_OpInt(script, 0, 4, 1);
    M_OpText(script, 0, 3, 0, 1, "two");
    M_OpInt(script, 0, 4, 1);
    M_OpText(script, 0, 3, 0, 0, "three");
    M_Op(script, 0, 10);
    M_Setup("blank");
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "one");
    CHECK(g_Gui.msg.line_texts[1] == nullptr);
    M_Run(1);
    CHECK_EQ_STR(g_Gui.msg.line_texts[1], "two");
    M_Run(1);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "three");
    CHECK_EQ_STR(g_Gui.msg.line_texts[1], " ");
    M_Teardown();
}

TEST(an_interrupt_reaches_a_portrait_or_a_line)
{
    M_SCRIPT *const script = M_Script();
    M_OpInterrupt(script, 0, 1, 7);
    M_OpInterrupt(script, 0, 3, 9);
    M_Op(script, 0, 10);
    M_Setup("interrupt");
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.msg.portraits[1].pending_interrupt, 7);
    CHECK_EQ_INT(g_Gui.msg.lines[1].pending_interrupt, 9);
    M_Teardown();
}

TEST(a_song_instruction_shows_the_song_name)
{
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 7, 2);
    M_Op(script, 0, 10);
    M_Setup("song");
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.banners.song, 2);
    CHECK(g_Gui.banners.song_name.script == Anm_GetScript(0x701));
    M_Teardown();
}

TEST(a_song_instruction_plays_the_song)
{
    static char path[] = "bgm/th06_03.mid";
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 7, 2);
    M_Op(script, 0, 10);
    M_Setup("song_music");
    g_Stage.data.song_paths[2] = path;
    Sound_Reset();
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th06_03.mid");
    g_Stage.data.song_paths[2] = nullptr;
    M_Teardown();
}

TEST(the_fade_instruction_fades_the_music_out_over_4_seconds)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 0, 12);
    M_Op(script, 0, 10);
    M_Setup("fade_music");
    Sound_Reset();
    Gui_StartMsg(0);
    M_Run(1);
    CHECK(g_SoundEvents.fade_out);
    CHECK(g_SoundEvents.fade_seconds == 4.0f);
    M_Teardown();
}

TEST(a_song_out_of_range_fails_the_add)
{
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 7, 4);
    M_Op(script, 0, 0);
    CHECK(!Result_Log(M_SetupStage("bad_song", 1), LOG_LEVEL_INFO));
    M_Teardown();
}

static SCENE M_NextScene(
    const int32_t stage, const int32_t difficulty, const bool practice,
    const bool replay)
{
    g_GameManager.run.stage = stage;
    g_GameManager.run.difficulty = difficulty;
    g_GameManager.mode.in_practice = practice;
    g_GameManager.mode.in_replay = replay;
    g_GameManager.mode.game_cleared = false;
    Scene_Request(SCENE_GAMEPLAY);
    Gui_StartMsg(0);
    M_Run(1);
    CHECK(Gui_IsMsgRunning());
    return Scene_GetRequested();
}

TEST(the_scene_instruction_picks_the_next_scene)
{
    M_Op(M_Script(), 0, 11);
    M_Setup("scene");
    g_GameManager.score.value = 5000;
    CHECK_EQ_INT(M_NextScene(1, 1, false, false), SCENE_NEXT_STAGE);
    CHECK_EQ_INT(g_GameManager.score.shown, 5000);
    CHECK_EQ_INT(M_NextScene(5, 1, false, false), SCENE_NEXT_STAGE);
    CHECK_EQ_INT(M_NextScene(5, 0, false, false), SCENE_ENDING);
    CHECK_EQ_INT(M_NextScene(6, 2, false, false), SCENE_ENDING);
    CHECK_EQ_INT(M_NextScene(6, 2, false, true), SCENE_REPLAY_END);
    CHECK_EQ_INT(M_NextScene(1, 1, true, false), SCENE_GAME_RESULT);
    CHECK(!g_GameManager.mode.game_cleared);
    CHECK_EQ_INT(M_NextScene(7, 4, false, false), SCENE_GAME_RESULT);
    CHECK(g_GameManager.mode.game_cleared);
    CHECK_EQ_INT(M_NextScene(7, 4, false, true), SCENE_REPLAY_END);
    CHECK(!g_GameManager.mode.game_cleared);
    M_Teardown();
}

TEST(the_stage_clear_instruction_starts_the_loading_screen)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 0, 9);
    M_Op(script, 0, 10);
    M_Setup("stage_clear");
    g_GameManager.score.next_extend = 2;
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.stage_clear.state, GUI_STAGE_CLEAR_PENDING);
    CHECK(g_Gui.loading.script == Anm_GetScript(0x619));
    CHECK(g_Gui.loading.sprite >= 0);
    CHECK_EQ_INT(g_GameManager.score.next_extend, 2);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.stage_clear.state, GUI_STAGE_CLEAR_SHOWN);
    M_Teardown();
}

TEST(the_last_stage_clear_ends_the_extends)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 0, 9);
    M_Op(script, 0, 10);
    M_Setup("last_clear");
    g_GameManager.run.stage = 6;
    g_GameManager.score.next_extend = 2;
    Gui_StartMsg(0);
    M_Run(1);
    CHECK_EQ_INT(g_GameManager.score.next_extend, -1);
    CHECK_EQ_INT(g_Gui.loading.sprite, -1);
    M_Teardown();
}

TEST(the_loading_screen_shows_once_it_stops)
{
    M_SCRIPT *const script = M_Script();
    M_Op(script, 0, 9);
    M_Op(script, 0, 10);
    M_Setup("loading");
    Gui_StartMsg(0);
    M_Run(1);
    CHECK(!Gui_IsLoadingShown());
    M_Run(2);
    CHECK(Gui_IsLoadingShown());
    M_Run(5);
    CHECK(Gui_IsLoadingShown());
    g_Gui.loading.pending_interrupt = 1;
    M_Run(1);
    CHECK(!Gui_IsLoadingShown());
    CHECK(g_Gui.loading.sprite >= 0);
    M_Run(3);
    CHECK_EQ_INT(g_Gui.loading.sprite, -1);
    M_Teardown();
}

static int32_t M_StageClearBonus(void)
{
    g_GameManager.score.value = 100;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_PENDING;
    M_Run(1);
    CHECK_EQ_INT(g_Gui.stage_clear.state, GUI_STAGE_CLEAR_SHOWN);
    CHECK_EQ_INT(g_GameManager.score.value, 100 + g_Gui.stage_clear.total);
    return g_Gui.stage_clear.total;
}

TEST(the_stage_clear_bonus_scales_by_difficulty)
{
    M_Setup("bonus");
    g_GameManager.player.graze = 3;
    g_GameManager.player.power = 1;
    g_GameManager.player.point_items = 7;
    g_GameManager.run.start_lives = 2;
    const int32_t base = (1000 + 30 + 100) * 7;
    g_GameManager.run.difficulty = 0;
    CHECK_EQ_INT(M_StageClearBonus(), 3950);
    g_GameManager.run.difficulty = 1;
    CHECK_EQ_INT(M_StageClearBonus(), base);
    g_GameManager.run.difficulty = 2;
    CHECK_EQ_INT(M_StageClearBonus(), 9490);
    g_GameManager.run.difficulty = 3;
    CHECK_EQ_INT(M_StageClearBonus(), 11860);
    g_GameManager.run.difficulty = 4;
    CHECK_EQ_INT(M_StageClearBonus(), 15820);
    M_Teardown();
}

TEST(the_stage_clear_bonus_counts_lives_and_bombs_from_stage_6)
{
    M_Setup("bonus_final");
    g_GameManager.run.stage = 6;
    g_GameManager.player.graze = 3;
    g_GameManager.player.point_items = 2;
    g_GameManager.player.lives = 2;
    g_GameManager.player.bombs = 1;
    CHECK_EQ_INT(M_StageClearBonus(), (6000 + 30) * 2 + 6000000 + 1000000);
    M_Teardown();
}

TEST(the_starting_lives_cut_the_stage_clear_bonus_everywhere)
{
    M_Setup("bonus_lives");
    g_GameManager.player.graze = 3;
    g_GameManager.player.power = 1;
    g_GameManager.player.point_items = 7;
    g_GameManager.run.difficulty = 2;
    g_GameManager.run.start_lives = 3;
    CHECK_EQ_INT(M_StageClearBonus(), 4740);
    g_GameManager.run.start_lives = 4;
    CHECK_EQ_INT(M_StageClearBonus(), 1890);
    g_GameManager.mode.in_practice = true;
    CHECK_EQ_INT(M_StageClearBonus(), 1890);
    g_GameManager.mode.in_practice = false;
    g_GameManager.run.difficulty = 4;
    CHECK_EQ_INT(M_StageClearBonus(), 3160);
    M_Teardown();
}

TEST(the_boss_bar_appears_and_leaves)
{
    M_Setup("boss_bar");
    g_Gui.boss.present = true;
    g_Gui.boss.health = 1.0f;
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_APPEARING);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0);
    CHECK(g_Gui.hud.sprites[0x13].script == Anm_GetScript(0x613));
    M_Run(M_APPEAR_FRAMES - 1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_APPEARING);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_SHOWN);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 4 * M_APPEAR_FRAMES);
    g_Gui.boss.present = false;
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_LEAVING);
    CHECK(g_Gui.hud.sprites[0x13].script == Anm_GetScript(0x614));
    M_Run(M_LEAVE_FRAMES - 2);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_LEAVING);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_HIDDEN);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0);
    CHECK(g_Gui.hud.boss_health_shown == 0.0f);
    M_Teardown();
}

TEST(the_boss_bar_alpha_rises_to_255)
{
    M_Setup("alpha_rise");
    g_Gui.boss.present = true;
    M_Run(1 + 63);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0xfc);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0xff);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0xff);
    M_Teardown();
}

TEST(the_boss_bar_alpha_wraps_below_0)
{
    M_Setup("alpha_wrap");
    g_Gui.boss.present = true;
    M_Run(1 + 64);
    g_Gui.boss.present = false;
    M_Run(63);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 3);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, UINT32_MAX);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, UINT32_MAX - 4);
    M_Teardown();
}

TEST(a_boss_coming_back_shows_the_bar_once_it_has_left)
{
    M_Setup("boss_back");
    g_Gui.boss.present = true;
    M_Run(1 + M_APPEAR_FRAMES);
    g_Gui.boss.present = false;
    M_Run(1);
    g_Gui.boss.present = true;
    M_Run(M_LEAVE_FRAMES - 2);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_LEAVING);
    M_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_SHOWN);
    M_Teardown();
}

TEST(the_boss_health_shown_follows_the_health)
{
    M_Setup("health");
    g_Gui.boss.present = true;
    g_Gui.boss.health = 0.025f;
    M_Run(M_APPEAR_FRAMES);
    CHECK(g_Gui.hud.boss_health_shown == 0.0f);
    float expected = 0.0f;
    M_Run(1);
    expected += 0.01f;
    CHECK(g_Gui.hud.boss_health_shown == expected);
    M_Run(1);
    expected += 0.01f;
    CHECK(g_Gui.hud.boss_health_shown == expected);
    M_Run(1);
    CHECK(g_Gui.hud.boss_health_shown == 0.025f);
    g_Gui.boss.health = 0.0f;
    M_Run(1);
    expected = 0.025f;
    expected -= 0.02f;
    CHECK(g_Gui.hud.boss_health_shown == expected);
    M_Run(1);
    CHECK(g_Gui.hud.boss_health_shown == 0.0f);
    M_Teardown();
}

TEST(a_dialogue_stops_the_boss_bar)
{
    M_Op(M_Script(), 0, 10);
    M_Setup("bar_msg");
    Gui_StartMsg(0);
    g_Gui.boss.present = true;
    M_Run(3);
    CHECK_EQ_INT(g_Gui.hud.boss_bar, GUI_BOSS_BAR_HIDDEN);
    CHECK(g_Gui.hud.sprites[0x13].script == Anm_GetScript(0x613));
    M_Teardown();
}

TEST(drawing_places_the_boss_health_bar)
{
    M_Setup("draw_bar");
    Chain_RunDraw(&g_Chain);
    CHECK(g_Gui.hud.sprites[0x15].scale_x == 1.0f);
    g_Gui.boss.present = true;
    g_Gui.hud.boss_health_shown = 0.3f;
    Chain_RunDraw(&g_Chain);
    CHECK(g_Gui.hud.sprites[0x15].scale_x == 0.3f * 288.0f / 14.0f);
    CHECK(g_Gui.hud.sprites[0x15].pos.x == 96.0f);
    CHECK(g_Gui.hud.sprites[0x15].pos.y == 24.0f);
    CHECK_EQ_INT(g_Gui.hud.sprites[0x15].anchor, 3);
    M_Teardown();
}

TEST(the_boss_timer_queues_its_sound_when_it_changes_below_10)
{
    M_Setup("timer_sound");
    g_Gui.boss.present = true;
    g_Gui.boss.seconds = 9;
    Sound_Reset();
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_TIMEOUT);
    Sound_Reset();
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    g_Gui.boss.seconds = 10;
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    g_Gui.boss.seconds = 8;
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_TIMEOUT);
    M_Teardown();
}

TEST(drawing_leaves_the_frame_sprites_where_they_were_drawn_last)
{
    M_Setup("draw_frame");
    g_GameManager.player.power = 127;
    Chain_RunDraw(&g_Chain);
    CHECK(g_Gui.hud.sprites[6].pos.x == 608.0f);
    CHECK(g_Gui.hud.sprites[6].pos.y == 448.0f);
    CHECK(g_Gui.hud.sprites[6].pos.z == 0.49f);
    CHECK(g_Gui.hud.sprites[7].pos.x == 384.0f);
    CHECK(g_Gui.hud.sprites[8].pos.y == 464.0f);
    CHECK(g_Gui.hud.sprites[0x16].pos.x == 0.0f);
    CHECK(g_Gui.hud.sprites[0x16].pos.y == 464.0f);
    CHECK(g_Gui.hud.sprites[0x12].pos.x == 0.0f);
    g_GameManager.player.power = 128;
    Chain_RunDraw(&g_Chain);
    CHECK(g_Gui.hud.sprites[0x12].pos.x == 496.0f);
    CHECK(g_Gui.hud.sprites[0x12].pos.y == 186.0f);
    M_Teardown();
}

static int32_t M_PopupFrames(const GUI_POPUP *const popup)
{
    int32_t frames = 0;
    while (popup->shown && frames < 1000) {
        M_Run(1);
        frames++;
    }
    return frames;
}

TEST(the_popups_show_for_their_durations)
{
    M_Setup("popups");
    Gui_ShowBonusPopup(1200);
    CHECK_EQ_INT(g_Gui.popups.bonus.value, 1200);
    CHECK_EQ_INT(M_PopupFrames(&g_Gui.popups.bonus), 251);
    Gui_ShowFullPowerPopup();
    CHECK_EQ_INT(M_PopupFrames(&g_Gui.popups.full_power), 181);
    Gui_ShowSpellCardBonus(300000);
    CHECK_EQ_INT(g_Gui.popups.spell_bonus.value, 300000);
    CHECK_EQ_INT(M_PopupFrames(&g_Gui.popups.spell_bonus), 281);
    M_Teardown();
}

TEST(the_names_show_their_portraits)
{
    M_Setup("names");
    Gui_ShowBombName(0x4a1, "霊符「夢想封印」");
    CHECK(g_Gui.banners.bomb_portrait.script == Anm_GetScript(0x4a1));
    CHECK_EQ_INT(g_Gui.banners.bomb_portrait.sprite, 0x4a1);
    CHECK(g_Gui.banners.bomb_name.script == Anm_GetScript(0x706));
    CHECK_EQ_STR(g_Gui.banners.bomb_text, "霊符「夢想封印」");
    Gui_EndBombName();
    CHECK_EQ_INT(g_Gui.banners.bomb_name.pending_interrupt, 1);
    Gui_ShowSpellCardName(1, "name");
    CHECK(g_Gui.banners.spell_portrait.script == Anm_GetScript(0x4a3));
    CHECK_EQ_INT(g_Gui.banners.spell_portrait.sprite, 0x4a9);
    CHECK(g_Gui.banners.spell_name.script == Anm_GetScript(0x707));
    CHECK_EQ_STR(g_Gui.banners.spell_text, "name");
    Gui_EndSpellCardName();
    CHECK_EQ_INT(g_Gui.banners.spell_name.pending_interrupt, 1);
    M_Teardown();
}

TEST(the_name_bars_take_the_drawn_width_of_the_names)
{
    M_Setup("names_width");
    CHECK(Result_Log(Anm_InitText(nullptr, nullptr, 0), LOG_LEVEL_ERROR));
    Gui_ShowBombName(0x4a1, "霊符「夢想封印」");
    CHECK(g_Gui.banners.bomb_width == 16.0f * 15.0f / 2.0f + 16.0f);
    Gui_ShowSpellCardName(1, "<b$Moon$Sign> Ray");
    CHECK(g_Gui.banners.spell_width == 8.0f * 15.0f / 2.0f + 16.0f);
    Anm_ShutdownText();
    M_Teardown();
}

// Puts nmlgc/base_tsa in the patch stack of the translations.
static void M_UseBaseTsa(void)
{
    char *const patches = TestDir_Join(m_Directory, "patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &m_Patches),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    Translation_Init(m_Patches);
    CHECK(Translation_HasBaseTsa());
}

static void M_ShowVm(
    ANM_VM *const vm, const ANM_SPRITE *const sprite, const float x,
    const float y)
{
    Anm_InitVm(vm);
    vm->sprite_data = sprite;
    vm->shown = true;
    vm->pos = (VEC3) { x, y, 0.0f };
}

// Draws one tick of the interface and returns the recorded quad of the
// texture in slot, or nullptr when none is drawn.
static const GFX_RECORDED_QUAD *M_DrawnQuad(const int32_t slot)
{
    const GFX_TEXTURE *const texture = Anm_GetTexture(slot);
    for (int32_t i = 0; i < GfxRecorder_QuadCount(); i++) {
        if (GfxRecorder_Quad(i)->texture == texture) {
            return GfxRecorder_Quad(i);
        }
    }
    return nullptr;
}

static void M_DrawTick(void)
{
    g_GameManager.camera.viewport = (VIEWPORT) { 32, 16, 384, 448, 0, 0.5f };
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
}

TEST(the_spell_card_name_ends_at_the_right_of_its_row)
{
    M_Setup("banner_retail");
    CHECK(Result_Ignore(Anm_CreateTexture(100, 256, 256, ANM_TEXTURE_RGBA8)));
    CHECK(Result_Ignore(Anm_CreateTexture(101, 64, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE name = {
        .slot = 100,
        .x1 = 256,
        .y1 = 16,
        .texture_width = 256,
        .texture_height = 256,
        .u1 = 1.0f,
        .v1 = 1.0f / 16.0f,
        .width = 256,
        .height = 16,
    };
    const ANM_SPRITE bar = {
        .slot = 101,
        .x1 = 14,
        .y1 = 16,
        .texture_width = 64,
        .texture_height = 64,
        .u1 = 14.0f / 64.0f,
        .v1 = 0.25f,
        .width = 14,
        .height = 16,
    };
    M_ShowVm(&g_Gui.banners.spell_name, &name, 256.5f, 40.0f);
    M_ShowVm(&g_Gui.banners.spell_bar, &bar, 0.0f, 0.0f);
    const float width = 76.0f;
    g_Gui.banners.spell_width = width;
    GfxRecorder_Start();
    M_DrawTick();
    const float bar_x = 256.5f + (128.0f - width * 16.0f / 15.0f / 2.0f);
    const float half = 14.0f * (width / 14.0f) / 2.0f;
    const GFX_RECORDED_QUAD *const bar_quad = M_DrawnQuad(101);
    CHECK(bar_quad != nullptr);
    CHECK(bar_quad->vertices[0].position.x == X87_Rint(bar_x - half));
    CHECK(bar_quad->vertices[1].position.x == X87_Rint(half + bar_x));
    const GFX_RECORDED_QUAD *const name_quad = M_DrawnQuad(100);
    CHECK(name_quad != nullptr);
    CHECK(name_quad->vertices[0].position.x == X87_Rint(256.5f - 128.0f));
    CHECK(name_quad->vertices[1].position.x == X87_Rint(128.0f + 256.5f));
    CHECK(name_quad->vertices[1].uv.x == 1.0f);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(base_tsa_crops_the_names_and_puts_them_a_texture_width_away)
{
    M_Setup("banner_base_tsa");
    M_UseBaseTsa();
    CHECK(Result_Ignore(Anm_CreateTexture(100, 512, 256, ANM_TEXTURE_RGBA8)));
    CHECK(Result_Ignore(Anm_CreateTexture(101, 64, 64, ANM_TEXTURE_RGBA8)));
    CHECK(Result_Ignore(Anm_CreateTexture(102, 512, 256, ANM_TEXTURE_RGBA8)));
    CHECK(Result_Ignore(Anm_CreateTexture(103, 64, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE spell = {
        .slot = 100,
        .x1 = 512,
        .y1 = 16,
        .texture_width = 512,
        .texture_height = 256,
        .u1 = 1.0f,
        .v1 = 1.0f / 16.0f,
        .width = 512,
        .height = 16,
    };
    ANM_SPRITE bomb = spell;
    bomb.slot = 102;
    const ANM_SPRITE bar = {
        .slot = 101,
        .x1 = 14,
        .y1 = 16,
        .texture_width = 64,
        .texture_height = 64,
        .u1 = 14.0f / 64.0f,
        .v1 = 0.25f,
        .width = 14,
        .height = 16,
    };
    ANM_SPRITE bomb_bar = bar;
    bomb_bar.slot = 103;
    M_ShowVm(&g_Gui.banners.spell_name, &spell, -128.0f, 40.0f);
    M_ShowVm(&g_Gui.banners.spell_bar, &bar, 0.0f, 0.0f);
    M_ShowVm(&g_Gui.banners.bomb_name, &bomb, 576.0f, 440.0f);
    M_ShowVm(&g_Gui.banners.bomb_bar, &bomb_bar, 0.0f, 0.0f);
    g_Gui.banners.spell_width = 116.0f;
    g_Gui.banners.bomb_width = 116.0f;
    GfxRecorder_Start();
    M_DrawTick();
    const GFX_RECORDED_QUAD *quad = M_DrawnQuad(100);
    CHECK(quad != nullptr);
    CHECK(quad->vertices[0].position.x == 284.0f);
    CHECK(quad->vertices[1].position.x == -128.0f + 512.0f);
    CHECK(quad->vertices[1].uv.x == 100.0f / 512.0f);
    quad = M_DrawnQuad(101);
    CHECK(quad != nullptr);
    CHECK(quad->vertices[0].position.x == 268.0f);
    CHECK(quad->vertices[1].position.x == 384.0f);
    quad = M_DrawnQuad(102);
    CHECK(quad != nullptr);
    CHECK(quad->vertices[0].position.x == 576.0f - 512.0f);
    CHECK(quad->vertices[1].position.x == 164.0f);
    quad = M_DrawnQuad(103);
    CHECK(quad != nullptr);
    CHECK(quad->vertices[0].position.x == 48.0f);
    CHECK(quad->vertices[1].position.x == 164.0f);
    CHECK(g_Gui.banners.spell_name.pos.x == -128.0f);
    CHECK(g_Gui.banners.bomb_name.pos.x == 576.0f);
    GfxRecorder_Stop();
    Translation_Shutdown();
    M_Teardown();
}

TEST(base_tsa_name_bars_take_whole_pixels)
{
    M_Setup("names_width_base_tsa");
    M_UseBaseTsa();
    CHECK(Result_Log(Anm_InitText(nullptr, nullptr, 0), LOG_LEVEL_ERROR));
    Gui_ShowSpellCardName(1, "abc");
    CHECK(g_Gui.banners.spell_width == 22.0f + 4.0f + 16.0f);
    Gui_ShowBombName(0x4a1, "abcd");
    CHECK(g_Gui.banners.bomb_width == 30.0f + 4.0f + 16.0f);
    Anm_ShutdownText();
    Translation_Shutdown();
    M_Teardown();
}

// Lists the text images a test patch holds.
typedef enum {
    M_IMAGE_STAGE_TITLE = 1 << 0,
    M_IMAGE_STAGE_LOGO = 1 << 1,
    M_IMAGE_SONG = 1 << 2,
    M_IMAGE_BOSS_TITLE = 1 << 3,
    M_IMAGE_BOSS_NAME = 1 << 4,
    M_IMAGE_ALL = M_IMAGE_STAGE_TITLE | M_IMAGE_SONG | M_IMAGE_BOSS_TITLE
        | M_IMAGE_BOSS_NAME,
} M_IMAGES;

// Writes the PNG name of the patch a/lang, width x height pixels of red.
static void M_WriteTextImage(
    const char *const name, const int32_t width, const int32_t height)
{
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    for (int64_t i = 0; i < (int64_t)width * height; i++) {
        image.pixels[i * 4] = 255;
        image.pixels[i * 4 + 3] = 255;
    }
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    CHECK(IS_OK(Writer_Close(&writer)));
    char path[128];
    snprintf(path, sizeof(path), "patches/a/lang/th06/%s", name);
    Th06Data_WriteFile(m_Directory, path, bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
}

// Writes text.anm: sprites 0 to 5 as 64 x 16 rows, and scripts 0, 1, 4
// and 5 that show their sprite.
static void M_WriteTextAnm(void)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 128);
    for (int32_t i = 0; i < 6; i++) {
        AnmBuilder_AddSprite(&builder, i, 0.0f, 16.0f * i, 64.0f, 16.0f);
    }
    const uint32_t sprites[] = { 0, 1, 4, 5 };
    for (size_t i = 0; i < ARRAY_SIZE(sprites); i++) {
        const int32_t script =
            AnmBuilder_BeginScript(&builder, (int32_t)sprites[i]);
        AnmBuilder_Instr(&builder, script, 0, 1, &sprites[i], 1);
        AnmBuilder_Instr(&builder, script, 1000, 15, nullptr, 0);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, "data/text.anm", bytes, size);
}

// Sets up stage with text.anm and the patch a/lang holding images, below
// nmlgc/base_tsa when base_tsa is set. The stage title image has 2 rows,
// the song image 4, and the boss images 2.
static void M_SetupTextImages(
    const char *const case_name, const int32_t stage, const bool base_tsa,
    const M_IMAGES images)
{
    static char name[] = "Stage";
    static char song[] = "Song";
    M_WriteData(case_name);
    const int32_t front_ids[] = { 0x13, 0x14 };
    const int16_t front_ends[] = { M_APPEAR_FRAMES, M_LEAVE_FRAMES };
    M_WriteAnm("data/front.anm", front_ids, front_ends, 2);
    M_WriteFace("data/face05a.anm", 1000);
    M_WriteMsg("data/msg2.dat");
    M_WriteTextAnm();
    char *const patches = TestDir_Join(m_Directory, "patches");
    if (base_tsa) {
        PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    }
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    char *const game_dir = TestDir_Join(patches, "a/lang/th06");
    CHECK(Result_Log(File_MakeDir(game_dir), LOG_LEVEL_ERROR));
    Mem_Free(game_dir);
    if ((images & M_IMAGE_STAGE_TITLE) != 0) {
        M_WriteTextImage("ti_sttitle.png", 384, 32);
    }
    if ((images & M_IMAGE_STAGE_LOGO) != 0) {
        M_WriteTextImage("ti_stlogo.png", 384, 96);
    }
    if ((images & M_IMAGE_SONG) != 0) {
        M_WriteTextImage("ti_bgm.png", 384, 128);
    }
    if ((images & M_IMAGE_BOSS_TITLE) != 0) {
        M_WriteTextImage("ti_bosstitle.png", 384, 128);
    }
    if ((images & M_IMAGE_BOSS_NAME) != 0) {
        M_WriteTextImage("ti_bossname.png", 384, 128);
    }
    m_Patches = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &m_Patches),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Files_SetPatches(m_Patches);
    Translation_Init(m_Patches);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    CHECK(Result_Log(Anm_InitText(nullptr, nullptr, 0), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Anm_Load(0, "data/text.anm", 0x700), LOG_LEVEL_ERROR));
    CHECK(Result_Log(
        Anm_CreateTexture(0, 64, 128, ANM_TEXTURE_RGBA8), LOG_LEVEL_ERROR));
    AsciiText_Initialize();
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = stage;
    g_GameManager.run.difficulty = 1;
    g_Buttons = (BUTTONS) {};
    g_Stage.data.name = name;
    g_Stage.data.song_names[0] = song;
    g_Stage.data.song_names[1] = song;
    Scene_Request(SCENE_GAMEPLAY);
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
}

static void M_TeardownTextImages(void)
{
    g_Stage.data.name = nullptr;
    g_Stage.data.song_names[0] = nullptr;
    g_Stage.data.song_names[1] = nullptr;
    Chain_Release(&g_Chain);
    Anm_ShutdownText();
    Translation_Shutdown();
    M_Teardown();
}

// Returns whether the texture of slot holds no pixel that text drew: every
// pixel is the red of M_WriteTextImage.
static bool M_TextureIsUntouched(const int32_t slot)
{
    const IMAGE *const image = AnmTexture_GetImage(slot);
    for (int64_t i = 0; i < (int64_t)image->width * image->height; i++) {
        const uint8_t *const pixel = &image->pixels[i * 4];
        if (pixel[0] != 255 || pixel[1] != 0 || pixel[3] != 255) {
            return false;
        }
    }
    return true;
}

// Returns whether the row of text.anm's texture at y holds a drawn pixel.
static bool M_TextRowIsDrawn(const int32_t y)
{
    const IMAGE *const image = AnmTexture_GetImage(0);
    for (int64_t i = (int64_t)y * image->width;
         i < (int64_t)(y + 16) * image->width; i++) {
        if (image->pixels[i * 4 + 3] != 0) {
            return true;
        }
    }
    return false;
}

TEST(text_rows_keep_their_sprites_without_base_tsa)
{
    m_ScriptCount = 0;
    M_SetupTextImages("text_images_no_base_tsa", 1, false, M_IMAGE_ALL);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x700)->slot, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x701)->slot, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x704)->slot, 0);
    CHECK(Anm_GetTexture(47) == nullptr);
    CHECK(M_TextRowIsDrawn(0));
    CHECK(M_TextRowIsDrawn(16));
    M_TeardownTextImages();
}

TEST(base_tsa_without_images_keeps_the_text_rows)
{
    m_ScriptCount = 0;
    M_SetupTextImages("text_images_none", 1, true, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x700)->slot, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x701)->slot, 0);
    CHECK(M_TextRowIsDrawn(0));
    CHECK(M_TextRowIsDrawn(16));
    M_TeardownTextImages();
}

TEST(base_tsa_text_images_replace_the_stage_rows)
{
    m_ScriptCount = 0;
    M_SetupTextImages("text_images_stage", 2, true, M_IMAGE_ALL);
    const ANM_SPRITE *const title = Anm_GetSpriteEntry(0x700);
    CHECK_EQ_INT(title->slot, 47);
    CHECK(title->y0 == 16.0f && title->width == 384.0f);
    CHECK(title->texture_width == 384.0f && title->v1 == 1.0f);
    CHECK(g_Gui.banners.stage_name.sprite_data == title);
    const ANM_SPRITE *const song = Anm_GetSpriteEntry(0x701);
    CHECK_EQ_INT(song->slot, 48);
    CHECK(song->y0 == 64.0f && song->height == 32.0f);
    CHECK(g_Gui.banners.song_name.pos.x == 384.0f);
    CHECK(g_Gui.banners.song_name.pos.y == 416.0f);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x704)->slot, 49);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x705)->slot, 50);
    CHECK(M_TextureIsUntouched(47));
    CHECK(M_TextureIsUntouched(48));
    CHECK(!M_TextRowIsDrawn(0) && !M_TextRowIsDrawn(16));
    M_TeardownTextImages();
}

TEST(base_tsa_text_images_show_the_boss_song_and_nameplates)
{
    m_ScriptCount = 0;
    M_SCRIPT *const script = M_Script();
    M_OpInt(script, 0, 7, 1);
    M_OpText(script, 0, 8, 1, 0, "Boss");
    M_Op(script, 0, 10);
    M_SetupTextImages("text_images_boss", 1, true, M_IMAGE_ALL);
    Gui_StartMsg(0);
    M_Run(1);
    CHECK(Anm_GetSpriteEntry(0x701)->y0 == 32.0f);
    CHECK(g_Gui.banners.song_name.pos.y == 416.0f);
    const ANM_VM *const nameplate = &g_Gui.msg.nameplates[0];
    CHECK_EQ_INT(nameplate->sprite_data->slot, 49);
    CHECK(nameplate->pos.x == 224.0f && nameplate->pos.y == 352.0f);
    CHECK(M_TextureIsUntouched(48));
    CHECK(M_TextureIsUntouched(49));
    M_TeardownTextImages();
}

TEST(base_tsa_boss_images_replace_both_nameplates_or_neither)
{
    m_ScriptCount = 0;
    M_SetupTextImages(
        "text_images_boss_title", 1, true,
        M_IMAGE_STAGE_TITLE | M_IMAGE_BOSS_TITLE);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x700)->slot, 47);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x704)->slot, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x705)->slot, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x701)->slot, 0);
    CHECK(M_TextRowIsDrawn(16));
    M_TeardownTextImages();
}

TEST(base_tsa_text_images_without_the_stage_row_keep_the_text)
{
    m_ScriptCount = 0;
    M_SetupTextImages("text_images_stage_7", 7, true, M_IMAGE_ALL);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x700)->slot, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x701)->slot, 0);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x704)->slot, 0);
    CHECK(Anm_GetScript(0x701)->count == 2);
    CHECK(M_TextRowIsDrawn(0));
    M_TeardownTextImages();
}

TEST(base_tsa_stage_logo_takes_precedence_over_the_stage_title)
{
    m_ScriptCount = 0;
    M_SetupTextImages(
        "text_images_logo", 2, true, M_IMAGE_STAGE_TITLE | M_IMAGE_STAGE_LOGO);
    const ANM_SPRITE *const title = Anm_GetSpriteEntry(0x700);
    CHECK(title->height == 48.0f && title->y0 == 48.0f);
    CHECK(g_Gui.banners.stage_name.shown);
    GfxRecorder_Start();
    Chain_RunDraw(&g_Chain);
    const int32_t with_logo = g_AsciiText.string_count;
    GfxRecorder_Stop();
    M_TeardownTextImages();
    m_ScriptCount = 0;
    M_SetupTextImages("text_images_title", 2, true, M_IMAGE_STAGE_TITLE);
    CHECK(Anm_GetSpriteEntry(0x700)->height == 16.0f);
    GfxRecorder_Start();
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_AsciiText.string_count, with_logo + 1);
    GfxRecorder_Stop();
    M_TeardownTextImages();
}

TEST(removing_the_interface_gives_the_text_rows_back)
{
    m_ScriptCount = 0;
    M_SetupTextImages("text_images_release", 1, true, M_IMAGE_ALL);
    const ANM_SCRIPT *const song = Anm_GetScript(0x701);
    Gui_Remove();
    CHECK_EQ_INT(Anm_GetSpriteEntry(0x700)->slot, 0);
    CHECK(Anm_GetSpriteEntry(0x701)->y0 == 16.0f);
    CHECK(Anm_GetScript(0x701) != song);
    CHECK_EQ_INT(Anm_GetScript(0x701)->count, 2);
    CHECK(Anm_GetTexture(47) == nullptr);
    M_TeardownTextImages();
}

// Returns whether the first nameplate draws before the second.
static bool M_FirstNameplateDrawsFirst(void)
{
    CHECK(Result_Ignore(Anm_CreateTexture(100, 64, 64, ANM_TEXTURE_RGBA8)));
    CHECK(Result_Ignore(Anm_CreateTexture(101, 64, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE first = {
        .slot = 100,
        .x1 = 16,
        .y1 = 16,
        .texture_width = 64,
        .texture_height = 64,
        .u1 = 0.25f,
        .v1 = 0.25f,
        .width = 16,
        .height = 16,
    };
    ANM_SPRITE second = first;
    second.slot = 101;
    g_Gui.msg.script = 0;
    Timer_Set(&g_Gui.msg.timer, 60);
    M_ShowVm(&g_Gui.msg.nameplates[0], &first, 100.0f, 100.0f);
    M_ShowVm(&g_Gui.msg.nameplates[1], &second, 100.0f, 120.0f);
    GfxRecorder_Start();
    M_DrawTick();
    const GFX_RECORDED_QUAD *const a = M_DrawnQuad(100);
    const GFX_RECORDED_QUAD *const b = M_DrawnQuad(101);
    CHECK(a != nullptr && b != nullptr);
    const bool result = a < b;
    GfxRecorder_Stop();
    return result;
}

TEST(nameplates_draw_in_order)
{
    M_Setup("nameplates_order");
    CHECK(M_FirstNameplateDrawsFirst());
    M_Teardown();
}

TEST(base_tsa_draws_the_nameplates_in_reverse)
{
    M_Setup("nameplates_base_tsa");
    M_UseBaseTsa();
    CHECK(!M_FirstNameplateDrawsFirst());
    Translation_Shutdown();
    M_Teardown();
}

TEST(the_names_queue_the_card_sound)
{
    M_Setup("names_sound");
    Sound_Reset();
    Gui_ShowBombName(0x4a1, "name");
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_CARD);
    Sound_Reset();
    Gui_ShowSpellCardName(1, "name");
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_CARD);
    M_Teardown();
}

TEST(a_frozen_game_stops_the_interface)
{
    M_Op(M_Script(), 0, 10);
    M_Setup("frozen");
    Gui_StartMsg(0);
    Gui_ShowBonusPopup(5);
    g_Gui.boss.present = true;
    g_Buttons.held.skip = true;
    g_GameManager.mode.frozen = true;
    M_Run(3);
    CHECK_EQ_INT(g_Gui.popups.bonus.timer.current, 0);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 0);
    g_GameManager.mode.frozen = false;
    M_Run(1);
    CHECK_EQ_INT(g_Gui.popups.bonus.timer.current, 1);
    CHECK_EQ_INT(g_Gui.msg.timer.current, 60);
    M_Teardown();
}

TEST(the_extra_stage_dialogue_loads_the_third_boss_portrait)
{
    M_Op(M_Script(), 0, 10);
    CHECK(Result_Log(M_SetupStage("extra", 7), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Anm_GetScript(0x4a8)->instrs[1].time, 1000);
    Gui_StartMsg(0);
    CHECK(Gui_IsMsgRunning());
    CHECK(Anm_GetScript(0x2b3) == nullptr);
    CHECK_EQ_INT(Anm_GetScript(0x4a8)->instrs[1].time, M_FACE12C_END);
    M_Teardown();
}

TEST(gui_drawing_records_the_hud_and_dialogue_gradient)
{
    M_Setup("gui_draw");
    CHECK(Result_Ignore(Anm_CreateTexture(0xd, 64, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 0xd, .width = 16, .height = 16, .u1 = 1.0f, .v1 = 1.0f
    };
    for (int32_t i = 0; i < GUI_SPRITE_COUNT; i++) {
        Anm_InitVm(&g_Gui.hud.sprites[i]);
        g_Gui.hud.sprites[i].sprite_data = &sprite;
    }
    g_Gui.msg.script = 0;
    Timer_Set(&g_Gui.msg.timer, 60);
    g_GameManager.camera.viewport = (VIEWPORT) { 32, 16, 384, 448, 0, 0.5f };
    g_GameManager.score.shown = 123456;
    g_GameManager.player.power = 16;
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK(GfxRecorder_QuadCount() > 100);
    bool found_dialogue = false;
    for (int32_t i = 0; i < GfxRecorder_QuadCount(); i++) {
        const GFX_VERTEX *const vertices = GfxRecorder_Quad(i)->vertices;
        if (vertices[0].position.y == 384.0f
            && vertices[0].color.alpha == 0xd0) {
            CHECK(vertices[2].position.y == 432.0f);
            CHECK_EQ_INT(vertices[2].color.alpha, 0x90);
            found_dialogue = true;
        }
    }
    CHECK(found_dialogue);
    bool found_score = false;
    for (int32_t i = 0; i < g_AsciiText.string_count; i++) {
        if (strcmp(g_AsciiText.strings[i].text, "000123456") == 0) {
            found_score = true;
        }
    }
    CHECK(found_score);
    CHECK(g_GameManager.camera.viewport.width == 640.0f);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(popups_slide_into_the_playfield_before_their_timer_advances)
{
    M_Setup("popup_slide");
    Gui_ShowBonusPopup(100);
    Gui_ShowFullPowerPopup();
    CHECK(g_Gui.popups.bonus.pos.x == 416.0f);
    CHECK(g_Gui.popups.bonus.pos.y == 32.0f);
    CHECK(g_Gui.popups.full_power.pos.y == 232.0f);
    M_Run(16);
    CHECK(g_Gui.popups.bonus.pos.x == 260.0f);
    CHECK(g_Gui.popups.full_power.pos.x == 260.0f);
    M_Run(15);
    CHECK(g_Gui.popups.bonus.pos.x == 104.0f);
    M_Teardown();
}

// Dialogue jdiffs

// Writes script 0 of msg1.dat: a box at time 10 of the lines first and
// second, then a halt.
static void M_WriteBox(const char *const first, const char *const second)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 10, 3, 0, 0, first);
    if (second != nullptr) {
        M_OpText(script, 10, 3, 0, 1, second);
    }
    M_Op(script, 10, 10);
}

TEST(a_jdiff_replaces_the_lines_of_a_box)
{
    M_WriteBox(M_KOUMA, M_KOUMA);
    M_SetupPatched("jdiff_lines", "{'0': {'10_0': {lines: ['One', 'Two']}}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "One");
    CHECK_EQ_STR(g_Gui.msg.line_texts[1], "Two");
    M_Teardown();
}

TEST(a_jdiff_box_can_be_a_bare_array)
{
    M_WriteBox(M_KOUMA, nullptr);
    M_SetupPatched("jdiff_bare", "{'0': {'10_0': ['One']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "One");
    M_Teardown();
}

TEST(fewer_jdiff_lines_delete_the_extra_lines)
{
    M_WriteBox(M_KOUMA, M_KOUMA);
    M_SetupPatched("jdiff_fewer", "{'0': {'10_0': ['One']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "One");
    CHECK(g_Gui.msg.line_texts[1] == nullptr);
    M_Teardown();
}

TEST(more_jdiff_lines_add_lines_on_the_same_frame)
{
    M_WriteBox(M_KOUMA, nullptr);
    M_SetupPatched("jdiff_more", "{'0': {'10_0': ['One', 'Two']}}");
    Gui_StartMsg(0);
    M_Run(10);
    CHECK(g_Gui.msg.line_texts[0] == nullptr);
    M_Run(1);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "One");
    CHECK_EQ_STR(g_Gui.msg.line_texts[1], "Two");
    M_Teardown();
}

TEST(jdiff_lines_past_the_second_are_dropped)
{
    M_WriteBox(M_KOUMA, nullptr);
    M_SetupPatched("jdiff_third", "{'0': {'10_0': ['One', 'Two', 'Three']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "One");
    CHECK_EQ_STR(g_Gui.msg.line_texts[1], "Two");
    M_Teardown();
}

TEST(a_jdiff_nameplate_takes_its_h1_key)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 10, 8, 1, 0, M_KOUMA);
    M_OpText(script, 10, 3, 0, 0, M_KOUMA);
    M_Op(script, 10, 10);
    M_SetupPatched(
        "jdiff_h1", "{'0': {'10_h1_0': ['Rumia'], '10_0': ['Line']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.nameplate_texts[0], "Rumia");
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "Line");
    M_Teardown();
}

TEST(jdiff_boxes_count_within_a_time_code)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 10, 3, 0, 0, M_KOUMA);
    M_OpInt(script, 10, 4, 1);
    M_OpText(script, 10, 3, 0, 0, M_KOUMA);
    M_Op(script, 10, 10);
    M_SetupPatched("jdiff_boxes", "{'0': {'10_0': ['A'], '10_1': ['B']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "A");
    M_Run(2);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "B");
    M_Teardown();
}

// thcrap counts each line of a box that the jdiff does not translate, so
// the box after an untranslated box of two lines is box 2.
TEST(an_untranslated_line_counts_as_a_box)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 10, 3, 0, 0, M_KOUMA);
    M_OpText(script, 10, 3, 0, 1, M_KOUMA);
    M_OpInt(script, 10, 4, 1);
    M_OpText(script, 10, 3, 0, 0, M_KOUMA);
    M_Op(script, 10, 10);
    M_SetupPatched("jdiff_untranslated", "{'0': {'10_2': ['Z']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "紅魔");
    M_Run(2);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "Z");
    M_Teardown();
}

TEST(a_new_time_code_counts_its_boxes_from_0)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 10, 3, 0, 0, M_KOUMA);
    M_OpText(script, 11, 3, 0, 0, M_KOUMA);
    M_Op(script, 11, 10);
    M_SetupPatched("jdiff_time", "{'0': {'10_0': ['A'], '11_0': ['B']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "A");
    M_Run(1);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "B");
    M_Teardown();
}

TEST(jdiff_lines_keep_markup_and_notes)
{
    M_WriteBox(M_KOUMA, nullptr);
    M_SetupPatched(
        "jdiff_plain", "{'0': {'10_0': ['<c$Rumia$x> says\\u0014a note']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(
        g_Gui.msg.line_texts[0],
        "<c$Rumia$x> says\x14"
        "a note");
    M_Teardown();
}

TEST(a_jdiff_for_another_script_changes_nothing)
{
    M_WriteBox(M_KOUMA, nullptr);
    M_SetupPatched("jdiff_other", "{'1': {'10_0': ['One']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK_EQ_STR(g_Gui.msg.line_texts[0], "紅魔");
    M_Teardown();
}

// Translator notes

// Sets up stage 1 as M_SetupPatched does, with nmlgc/base_tsa and its
// notes region, and a text.anm whose dialogue lines show their rows.
static void M_SetupNotes(const char *const case_name, const char *const diff)
{
    M_WriteData(case_name);
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 128);
    for (int32_t i = 0; i < 6; i++) {
        AnmBuilder_AddSprite(&builder, i, 0.0f, 16.0f * i, 64.0f, 16.0f);
    }
    for (uint32_t sprite = 2; sprite < 4; sprite++) {
        const int32_t script =
            AnmBuilder_BeginScript(&builder, (int32_t)sprite);
        AnmBuilder_Instr(&builder, script, 0, 1, &sprite, 1);
        AnmBuilder_Instr(&builder, script, 1000, 15, nullptr, 0);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, "data/text.anm", bytes, size);
    char *const patches = TestDir_Join(m_Directory, "patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    PatchBuilder_Write(
        patches, "nmlgc/base_tsa/global.js",
        "{tlnotes: {reference_resolution: [1280, 960], "
        "region_topleft: [864, 516], region_size: [384, 412]}}");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    PatchBuilder_Write(patches, "a/lang/th06/msg1.dat.jdiff", diff);
    m_Patches = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &m_Patches),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Files_SetPatches(m_Patches);
    Translation_Init(m_Patches);
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    CHECK(Result_Log(Anm_InitText(nullptr, nullptr, 0), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Anm_Load(0, "data/text.anm", 0x700), LOG_LEVEL_ERROR));
    CHECK(Result_Log(
        Anm_CreateTexture(0, 64, 128, ANM_TEXTURE_RGBA8), LOG_LEVEL_ERROR));
    Note_Init(m_Patches);
    AsciiText_Initialize();
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    g_GameManager.run.difficulty = 1;
    g_Buttons = (BUTTONS) {};
    Scene_Request(SCENE_GAMEPLAY);
    m_ScriptCount = 0;
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
}

static void M_TeardownNotes(void)
{
    Chain_Release(&g_Chain);
    Note_Shutdown();
    Anm_ShutdownText();
    Translation_Shutdown();
    M_Teardown();
    GfxRecorder_Stop();
}

// Returns whether a translator note shows in a tick.
static bool M_NoteShows(void)
{
    Gfx_BeginTick((GFX_COLOR) {});
    Note_Draw();
    Gfx_EndTick();
    return GfxRecorder_QuadCount() == 1;
}

// Writes script 0 of msg1.dat: a box of two lines at time 10, a box of
// one line at time 20, then a halt.
static void M_WriteNoteBoxes(void)
{
    M_SCRIPT *const script = M_Script();
    M_OpText(script, 10, 3, 0, 0, M_KOUMA);
    M_OpText(script, 10, 3, 0, 1, M_KOUMA);
    M_OpText(script, 20, 3, 0, 0, M_KOUMA);
    M_Op(script, 20, 10);
}

TEST(a_jdiff_note_shows_with_its_line_until_the_next_box)
{
    M_WriteNoteBoxes();
    M_SetupNotes(
        "note_next_box",
        "{'0': {'10_0': ['One', 'Two\\u0014A note'], '20_0': ['Three']}}");
    Gui_StartMsg(0);
    CHECK(!M_NoteShows());
    M_Run(11);
    CHECK(M_NoteShows());
    M_Run(10);
    CHECK(!M_NoteShows());
    M_TeardownNotes();
}

TEST(a_jdiff_note_stays_through_its_own_box)
{
    M_WriteNoteBoxes();
    M_SetupNotes(
        "note_own_box", "{'0': {'10_0': ['One\\u0014A note', 'Two']}}");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK(M_NoteShows());
    M_TeardownNotes();
}

TEST(an_untranslated_box_keeps_the_note)
{
    M_WriteNoteBoxes();
    M_SetupNotes(
        "note_untranslated", "{'0': {'10_0': ['One', 'Two\\u0014A note']}}");
    Gui_StartMsg(0);
    M_Run(21);
    CHECK(M_NoteShows());
    M_TeardownNotes();
}

TEST(the_first_translated_line_of_a_script_hides_the_note)
{
    M_WriteNoteBoxes();
    M_SetupNotes("note_script", "{'0': {'20_0': ['Three']}}");
    Note_Show(
        "\x14"
        "A note");
    Gui_StartMsg(0);
    M_Run(11);
    CHECK(M_NoteShows());
    M_Run(10);
    CHECK(!M_NoteShows());
    M_TeardownNotes();
}

TEST(a_spell_card_and_an_anm_file_hide_the_note)
{
    M_WriteNoteBoxes();
    M_SetupNotes("note_spell", "{}");
    Note_Show(
        "\x14"
        "A note");
    CHECK(M_NoteShows());
    Gui_ShowSpellCardName(1, "Sign");
    CHECK(!M_NoteShows());
    Note_Show(
        "\x14"
        "A note");
    CHECK(
        Result_Log(Anm_Load(0x20, "data/face00a.anm", 0x7f0), LOG_LEVEL_ERROR));
    CHECK(!M_NoteShows());
    M_TeardownNotes();
}
