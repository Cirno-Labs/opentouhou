#include "th06/screens/main.h"

#include "anm_builder.h"
#include "shell_fake.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/replay/format.h"
#include "th06/scene.h"
#include "th06/screens/result.h"
#include "th06/screens/result/private.h"
#include "th06/settings.h"
#include "th06/sound.h"

#include "core/base/memory.h"
#include "core/image.h"
#include "core/io/file.h"

#include <string.h>

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;

static const char *const m_AnmFiles[] = {
    "data/title01.anm",  "data/title02.anm",  "data/title03.anm",
    "data/title04.anm",  "data/title01s.anm", "data/title04s.anm",
    "data/select01.anm", "data/select02.anm", "data/select03.anm",
    "data/select04.anm", "data/select05.anm", "data/slpl00a.anm",
    "data/slpl00b.anm",  "data/slpl01a.anm",  "data/slpl01b.anm",
    "data/replay00.anm",
};

static char *m_Directory;
static SCENE m_Previous;
static SCENE m_Requested;
static TH06_INPUT m_Input;
static CHAIN_ELEM m_InputElem;
static int32_t m_Calcs;

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
}

SCENE Scene_Get(void)
{
    return m_Previous;
}

SCENE Scene_GetRequested(void)
{
    return m_Requested;
}

static CHAIN_STATUS M_Input(void *const arg)
{
    Buttons_Update(&g_Buttons, &m_Input);
    m_Calcs++;
    return CHAIN_CONTINUE;
}

static void M_WriteImage(const char *const name)
{
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    Th06Data_WriteFile(m_Directory, name, bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
}

static void M_WriteData(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const title = TestDir_Join(m_Directory, "data/title");
    CHECK(Result_Log(File_MakeDir(title), LOG_LEVEL_ERROR));
    Mem_Free(title);
    for (size_t i = 0; i < sizeof(m_AnmFiles) / sizeof(m_AnmFiles[0]); i++) {
        ANM_BUILDER builder;
        AnmBuilder_Init(&builder, 32, 32);
        builder.blank_texture = true;
        AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 16.0f, 16.0f);
        uint8_t bytes[ANM_BUILDER_MAX_SIZE];
        const size_t size = AnmBuilder_Build(&builder, bytes);
        Th06Data_WriteFile(m_Directory, m_AnmFiles[i], bytes, size);
    }
    M_WriteImage("data/title/title00.jpg");
    M_WriteImage("data/title/select00.jpg");
}

static void M_Setup(const char *const case_name)
{
    M_WriteData(case_name);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    ResultScreen_OpenSaves(nullptr);
    Config_SetDefaults(&g_SettingsSchema, &g_Settings);
    g_Buttons = (BUTTONS) {};
    m_Input = (TH06_INPUT) {};
    m_Previous = SCENE_BOOT;
    m_Requested = SCENE_TITLE;
    m_Calcs = 0;
    Sound_Reset();
    Chain_InitElem(&m_InputElem, M_Input, nullptr);
    CHECK(
        Result_Log(Chain_AddCalc(&g_Chain, &m_InputElem, 0), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    AsciiText_Initialize();
    ResultScreen_CloseSaves();
    Anm_Shutdown();
    GfxRecorder_Stop();
    Files_Shutdown();
    Sound_Reset();
    Mem_Free(m_Directory);
}

static void M_Tick(void)
{
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    AsciiText_Initialize();
    Chain_RunCalc(&g_Chain);
    Sound_Reset();
}

static void M_Run(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Tick();
    }
}

static void M_Press(const TH06_INPUT input)
{
    m_Input = input;
    M_Tick();
    m_Input = (TH06_INPUT) {};
    M_Tick();
}

static const TH06_INPUT m_Shoot = { .shoot = true };
static const TH06_INPUT m_Down = { .down = true };
static const TH06_INPUT m_Up = { .up = true };
static const TH06_INPUT m_Right = { .right = true };

// Opens the title menu from the title: the first calc takes the key.
static void M_OpenTitleMenu(void)
{
    CHECK(Result_Log(TitleScene_SetUp(0), LOG_LEVEL_ERROR));
    M_Press(m_Shoot);
    M_Run(20);
}

// Goes from the title menu entry at the cursor through the difficulty,
// character and shot selects, confirming each.
static void M_ConfirmSelects(void)
{
    M_Press(m_Shoot);
    M_Run(60);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
}

static bool M_TitleRemoved(void)
{
    for (const CHAIN_ELEM *elem = g_Chain.calc.next; elem != nullptr;
         elem = elem->next) {
        if (elem->priority == 2) {
            return false;
        }
    }
    return true;
}

TEST(start_sets_up_a_game_from_the_settings)
{
    M_Setup("start");
    g_Settings.lives = 4;
    g_Settings.bombs = 0;
    M_OpenTitleMenu();
    M_ConfirmSelects();
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    CHECK(M_TitleRemoved());
    CHECK_EQ_INT(g_GameManager.run.difficulty, 1);
    CHECK_EQ_INT(g_GameManager.run.character, 0);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 0);
    CHECK_EQ_INT(g_GameManager.run.stage, 0);
    CHECK_EQ_INT(g_GameManager.player.lives, 4);
    CHECK_EQ_INT(g_GameManager.player.bombs, 0);
    CHECK(!g_GameManager.mode.in_replay);
    CHECK(!g_GameManager.mode.in_practice);
    CHECK_EQ_INT(g_Settings.difficulty, 1);
    M_Teardown();
}

TEST(the_selects_choose_difficulty_character_and_shot)
{
    M_Setup("choose");
    M_OpenTitleMenu();
    M_Press(m_Shoot);
    M_Run(60);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press((TH06_INPUT) { .left = true });
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 3);
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 1);
    CHECK_EQ_INT(g_Settings.difficulty, 3);
    M_Teardown();
}

TEST(the_cursor_skips_a_locked_extra_start)
{
    M_Setup("extra_locked");
    M_OpenTitleMenu();
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK(g_GameManager.mode.in_practice);
    M_Teardown();
}

TEST(extra_start_needs_a_clear_and_starts_with_two_lives)
{
    M_Setup("extra");
    g_Settings.lives = 4;
    g_Settings.bombs = 0;
    M_OpenTitleMenu();
    g_GameManager.records.clears[1].without_continues[2] = 99;
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(60);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 4);
    CHECK_EQ_INT(g_GameManager.run.character, 0);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 1);
    CHECK_EQ_INT(g_GameManager.run.stage, 6);
    CHECK_EQ_INT(g_GameManager.player.lives, 2);
    CHECK_EQ_INT(g_GameManager.player.bombs, 3);
    CHECK_EQ_INT(g_Settings.difficulty, 4);
    M_Teardown();
}

TEST(practice_starts_at_a_cleared_stage)
{
    M_Setup("practice");
    g_Settings.lives = 0;
    M_OpenTitleMenu();
    g_GameManager.records.clears[0].with_continues[0] = 6;
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(60);
    M_Press(m_Up);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Up);
    M_Press(m_Shoot);
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    CHECK(g_GameManager.mode.in_practice);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 0);
    CHECK_EQ_INT(g_GameManager.run.stage, 4);
    CHECK_EQ_INT(g_GameManager.run.last_practice_stage, 4);
    CHECK_EQ_INT(g_GameManager.player.lives, 2);
    CHECK_EQ_INT(g_GameManager.player.bombs, 3);
    M_Teardown();
}

static uint8_t *M_ReplayBytes(const int32_t first, size_t *const size)
{
    REPLAY_FRAME frames[] = { { 0, 0 }, { 5, 0x81 }, { 9999999, 0 } };
    REPLAY_STAGE second = {
        .lives = 4,
        .bombs = 1,
        .frames = frames,
        .frame_count = 3,
    };
    REPLAY_STAGE third = {
        .lives = 1,
        .bombs = 2,
        .frames = frames,
        .frame_count = 3,
    };
    REPLAY_FILE file = {
        .shot = 3,
        .difficulty = 2,
        .stages = { [3] = &third },
    };
    file.stages[first] = &second;
    const REPLAY_SAVE_INFO info = {
        .name = "REPLAY",
        .date = "01/02/03",
        .achieved_frames = 1.0f,
        .expected_frames = 1.0f,
    };
    REPLAY_FORMAT_SAVE prepared;
    CHECK(Result_Log(ReplayFormat_Prepare(&info, &prepared), LOG_LEVEL_ERROR));
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    const uint8_t random[4] = { 70, 1, 2, 3 };
    CHECK(Result_Log(
        ReplayFormat_Write(&writer, &file, &prepared, random),
        LOG_LEVEL_ERROR));
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    Result_Ignore(Writer_Close(&writer));
    return data;
}

static void M_PutReplay(const int32_t slot, const int32_t first)
{
    size_t size;
    uint8_t *const data = M_ReplayBytes(first, &size);
    ResultSaves_PutReplay(slot, data, size);
}

static void M_WriteReplay(const char *const name, const int32_t first)
{
    size_t size;
    uint8_t *const data = M_ReplayBytes(first, &size);
    Th06Data_WriteFile(m_Directory, name, data, size);
    Allocator_Free(&g_HeapAllocator, data, size);
}

TEST(a_replay_starts_with_the_lives_of_its_first_stage)
{
    M_Setup("replay");
    M_PutReplay(6, 2);
    CHECK(Result_Log(TitleScene_SetUp(1), LOG_LEVEL_ERROR));
    M_Tick();
    M_Press(m_Shoot);
    M_Run(40);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    CHECK(M_TitleRemoved());
    CHECK(g_GameManager.mode.in_replay);
    CHECK_EQ_STR(g_GameManager.mode.replay_path, "replay/th6_07.rpy");
    CHECK_EQ_INT(g_GameManager.run.difficulty, 2);
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 1);
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
    CHECK_EQ_INT(g_GameManager.player.lives, 4);
    CHECK_EQ_INT(g_GameManager.player.bombs, 1);
    M_Teardown();
}

TEST(the_replay_stage_cursor_skips_absent_stages)
{
    M_Setup("replay_skip");
    M_PutReplay(0, 1);
    CHECK(Result_Log(TitleScene_SetUp(1), LOG_LEVEL_ERROR));
    M_Tick();
    M_Press(m_Shoot);
    M_Run(40);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_GameManager.run.stage, 0);
    M_Teardown();
}

TEST(the_demo_starts_after_720_idle_calcs)
{
    M_Setup("demo");
    CHECK(Result_Log(TitleScene_SetUp(0), LOG_LEVEL_ERROR));
    M_Run(719);
    CHECK(m_Requested != SCENE_GAMEPLAY);
    M_Tick();
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    CHECK(g_GameManager.mode.in_demo);
    CHECK(g_GameManager.mode.in_replay);
    CHECK_EQ_STR(g_GameManager.mode.replay_path, "data/demo/demo00.rpy");
    CHECK_EQ_INT(g_GameManager.run.stage, 3);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 3);
    M_Teardown();
}

TEST(input_restarts_the_demo_count)
{
    M_Setup("demo_input");
    CHECK(Result_Log(TitleScene_SetUp(0), LOG_LEVEL_ERROR));
    M_Run(700);
    m_Input = (TH06_INPUT) { .skip = true };
    M_Tick();
    m_Input = (TH06_INPUT) {};
    M_Run(700);
    CHECK(m_Requested != SCENE_GAMEPLAY);
    M_Teardown();
}

TEST(the_options_change_and_restore_the_settings)
{
    M_Setup("options");
    M_OpenTitleMenu();
    for (int32_t i = 0; i < 5; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
    M_Run(32);
    M_Press(m_Right);
    CHECK_EQ_INT(g_Settings.lives, 3);
    M_Press((TH06_INPUT) { .left = true });
    M_Press((TH06_INPUT) { .left = true });
    M_Press((TH06_INPUT) { .left = true });
    M_Press((TH06_INPUT) { .left = true });
    CHECK_EQ_INT(g_Settings.lives, 4);
    M_Press(m_Down);
    M_Press(m_Right);
    CHECK_EQ_INT(g_Settings.bombs, 0);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Right);
    CHECK_EQ_INT(g_Settings.music, SETTINGS_MUSIC_MIDI);
    M_Press(m_Down);
    M_Press(m_Right);
    CHECK(!g_Settings.sound);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_Settings.lives, 2);
    CHECK_EQ_INT(g_Settings.bombs, 3);
    CHECK_EQ_INT(g_Settings.music, SETTINGS_MUSIC_WAVE);
    CHECK(g_Settings.sound);
    M_Teardown();
}

TEST(the_scores_entry_leaves_after_60_frames)
{
    M_Setup("scores");
    M_OpenTitleMenu();
    for (int32_t i = 0; i < 3; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
    M_Run(57);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    M_Run(2);
    CHECK_EQ_INT(m_Requested, SCENE_SCORES);
    CHECK(M_TitleRemoved());
    M_Teardown();
}

TEST(quit_leaves_after_60_frames)
{
    M_Setup("quit");
    M_OpenTitleMenu();
    M_Press((TH06_INPUT) { .quit = true });
    M_Run(59);
    CHECK_EQ_INT(m_Requested, SCENE_QUIT);
    M_Teardown();
}

TEST(the_cursor_starts_on_the_entry_of_the_previous_scene)
{
    M_Setup("cursor");
    m_Previous = SCENE_MUSIC_ROOM;
    CHECK(Result_Log(TitleScene_SetUp(0), LOG_LEVEL_ERROR));
    M_Press(m_Shoot);
    M_Run(20);
    M_Press(m_Shoot);
    M_Run(60);
    CHECK_EQ_INT(m_Requested, SCENE_MUSIC_ROOM);
    M_Teardown();
}

TEST(a_practice_game_returns_to_the_practice_entry)
{
    M_Setup("cursor_practice");
    g_GameManager.mode.in_practice = true;
    m_Previous = SCENE_GAME_RESULT;
    CHECK(Result_Log(TitleScene_SetUp(0), LOG_LEVEL_ERROR));
    CHECK(!g_GameManager.mode.in_practice);
    M_Press(m_Shoot);
    M_Run(20);
    M_Press(m_Shoot);
    CHECK(g_GameManager.mode.in_practice);
    M_Teardown();
}

TEST(the_runner_removes_the_title)
{
    M_Setup("remove");
    CHECK(Result_Log(TitleScene_SetUp(0), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK(!M_TitleRemoved());
    TitleScene_Remove();
    CHECK(M_TitleRemoved());
    TitleScene_Remove();
    M_Teardown();
}

static const char *const m_PadNames[9] = {
    "south",     "east",      "west",       "start", "dpad_up",
    "dpad_down", "dpad_left", "dpad_right", "north",
};

static void M_SetUpApp(void)
{
    FakeShell_Reset();
    for (size_t action = 0; action < 9; action++) {
        INPUT_BINDING *const bindings = g_FakeShell.bindings[action];
        CHECK(Input_BindingFromName("z", &bindings[0]));
        CHECK(Input_BindingFromName(m_PadNames[action], &bindings[1]));
        g_FakeShell.binding_counts[action] = 2;
    }
    g_FakeShell.fullscreen = true;
}

static void M_CheckRequest(
    const int32_t index, const char kind, const size_t action,
    const char *const name)
{
    CHECK(index < g_FakeShell.request_count);
    const FAKE_SHELL_REQUEST *const request = &g_FakeShell.requests[index];
    char request_name[INPUT_BINDING_NAME_SIZE] = "";
    if (request->kind == 'b' || request->kind == 'u') {
        Input_BindingName(request->binding, request_name);
    }
    CHECK_EQ_INT(request->kind, kind);
    CHECK_EQ_INT(request->action, action);
    CHECK_EQ_STR(request_name, name);
}

static void M_OpenOptions(void)
{
    M_OpenTitleMenu();
    for (int32_t i = 0; i < 5; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
    M_Run(32);
}

static void M_OpenPadConfig(void)
{
    M_OpenOptions();
    for (int32_t i = 0; i < 7; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
    M_Run(32);
}

static void M_PressPad(const char *const name)
{
    CHECK(Input_BindingFromName(name, &g_FakeShell.pad_press));
    g_FakeShell.pad_pending = true;
    M_Tick();
}

static void M_LeavePadConfig(void)
{
    for (int32_t i = 0; i < 10; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
}

TEST(a_pad_press_takes_the_row_and_swaps_the_other_row)
{
    M_Setup("pad_swap");
    M_SetUpApp();
    M_OpenPadConfig();
    M_PressPad("east");
    CHECK_EQ_INT(g_FakeShell.request_count, 0);
    M_LeavePadConfig();
    CHECK_EQ_INT(g_FakeShell.request_count, 4);
    M_CheckRequest(0, 'u', 0, "south");
    M_CheckRequest(1, 'b', 0, "east");
    M_CheckRequest(2, 'u', 1, "east");
    M_CheckRequest(3, 'b', 1, "south");
    FakeShell_Reset();
    M_Teardown();
}

TEST(shoot_and_focus_may_share_a_pad_button)
{
    M_Setup("pad_share");
    M_SetUpApp();
    M_OpenPadConfig();
    M_PressPad("west");
    M_LeavePadConfig();
    CHECK_EQ_INT(g_FakeShell.request_count, 2);
    M_CheckRequest(0, 'u', 0, "south");
    M_CheckRequest(1, 'b', 0, "west");
    FakeShell_Reset();
    M_Teardown();
}

TEST(a_joystick_button_counts_and_a_stick_does_not)
{
    M_Setup("pad_joystick");
    M_SetUpApp();
    M_OpenPadConfig();
    M_PressPad("left_stick_up");
    M_Press(m_Down);
    M_PressPad("joy_button_12");
    for (int32_t i = 0; i < 9; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_FakeShell.request_count, 2);
    M_CheckRequest(0, 'u', 1, "east");
    M_CheckRequest(1, 'b', 1, "joy_button_12");
    FakeShell_Reset();
    M_Teardown();
}

TEST(the_dpad_moves_the_pad_config_cursor_and_binds_nothing)
{
    M_Setup("pad_dpad");
    M_SetUpApp();
    M_OpenPadConfig();
    CHECK(Input_BindingFromName("dpad_down", &g_FakeShell.pad_press));
    g_FakeShell.pad_pending = true;
    M_Press(m_Down);
    M_PressPad("north");
    for (int32_t i = 0; i < 9; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_FakeShell.request_count, 4);
    M_CheckRequest(0, 'u', 1, "east");
    M_CheckRequest(1, 'b', 1, "north");
    M_CheckRequest(2, 'u', 8, "north");
    M_CheckRequest(3, 'b', 8, "east");
    FakeShell_Reset();
    M_Teardown();
}

TEST(the_pad_defaults_reset_the_pad_before_the_rows)
{
    M_Setup("pad_defaults");
    M_SetUpApp();
    M_OpenPadConfig();
    for (int32_t i = 0; i < 9; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Shoot);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_FakeShell.request_count, 1);
    M_CheckRequest(0, 'r', 0, "");
    FakeShell_Reset();
    M_Teardown();
}

TEST(the_window_row_changes_the_mode_when_the_options_close)
{
    M_Setup("window");
    M_SetUpApp();
    M_OpenOptions();
    for (int32_t i = 0; i < 5; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Right);
    CHECK_EQ_INT(g_FakeShell.request_count, 0);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_FakeShell.request_count, 1);
    M_CheckRequest(0, 'f', 0, "");
    FakeShell_Reset();
    M_Teardown();
}

TEST(an_unchanged_window_row_asks_for_nothing)
{
    M_Setup("window_same");
    M_SetUpApp();
    M_OpenOptions();
    for (int32_t i = 0; i < 5; i++) {
        M_Press(m_Down);
    }
    M_Press(m_Right);
    M_Press(m_Right);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_FakeShell.request_count, 0);
    FakeShell_Reset();
    M_Teardown();
}

TEST(the_replay_menu_after_a_replay_takes_the_scanned_list)
{
    M_Setup("scan");
    char *const replays = TestDir_Join(m_Directory, "replay");
    CHECK(Result_Log(File_MakeDir(replays), LOG_LEVEL_ERROR));
    Mem_Free(replays);
    M_WriteReplay("replay/th6_01.rpy", 2);
    ResultScreen_OpenSaves(m_Directory);
    M_WriteReplay("replay/th6_02.rpy", 1);
    CHECK(Result_Log(TitleScene_SetUp(1), LOG_LEVEL_ERROR));
    M_Tick();
    ResultScreen_ScanReplays();
    M_Tick();
    M_Run(40);
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(40);
    M_Press(m_Shoot);
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    const size_t length = strlen(g_GameManager.mode.replay_path);
    CHECK(
        strcmp(
            g_GameManager.mode.replay_path + length - 17, "replay/th6_02.rpy")
        == 0);
    CHECK_EQ_INT(g_GameManager.run.stage, 0);
    M_Teardown();
}

TEST(the_title_menu_asks_for_a_scan_when_the_replay_menu_opens)
{
    M_Setup("scan_title");
    char *const replays = TestDir_Join(m_Directory, "replay");
    CHECK(Result_Log(File_MakeDir(replays), LOG_LEVEL_ERROR));
    Mem_Free(replays);
    ResultScreen_OpenSaves(m_Directory);
    M_WriteReplay("replay/th6_ud0001.rpy", 1);
    M_OpenTitleMenu();
    M_Press(m_Down);
    M_Press(m_Down);
    const int32_t scan = ResultScreen_GetReplayScan();
    m_Input = m_Shoot;
    M_Tick();
    m_Input = (TH06_INPUT) {};
    ResultScreen_ScanReplays();
    CHECK(ResultScreen_GetReplayScan() != scan);
    M_Run(62);
    M_Press(m_Shoot);
    M_Run(40);
    M_Press(m_Shoot);
    CHECK_EQ_INT(m_Requested, SCENE_GAMEPLAY);
    const size_t length = strlen(g_GameManager.mode.replay_path);
    CHECK(
        strcmp(
            g_GameManager.mode.replay_path + length - 21,
            "replay/th6_ud0001.rpy")
        == 0);
    M_Teardown();
}
