#include "th07/screens/main.h"

#include "gfx_recorder.h"
#include "shell_fake.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"
#include "th07_scene_stubs.h"

#include "th07/anm.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screens/result/saves.h"
#include "th07/settings.h"
#include "th07/sound.h"
#include "th07/stubs.h"

#include "core/base/memory.h"
#include "core/image.h"
#include "core/io/file.h"

#include <stdio.h>
#include <string.h>

#define M_PLAIN_SIZE 0x54
#define M_HEADER_SIZE 0xe8
#define M_STAGE_SIZE 0x2c
#define M_MAX_SIZE 0x1000
#define M_IDLE_TEST_FRAMES 900

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static const TH07_INPUT m_NoInput;
static const TH07_INPUT m_Shoot = { .shoot = true };
static const TH07_INPUT m_Bomb = { .bomb = true };
static const TH07_INPUT m_Up = { .up = true };
static const TH07_INPUT m_Down = { .down = true };
static const TH07_INPUT m_Left = { .left = true };
static const TH07_INPUT m_Right = { .right = true };

// Shows the title's sprite on the help lines, so that their text renders.
static ANM_INSTR m_HelpInstrs[] = {
    { .opcode = 3, .args = { { .i = 0x900 } } },
    { .opcode = -1 },
};
static ANM_SCRIPT m_HelpScript = { m_HelpInstrs, 2, 0 };
static char *m_Directory;

static void M_PutU32(uint8_t *const b, const size_t offset, const uint32_t v)
{
    for (int32_t i = 0; i < 4; i++) {
        b[offset + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
}

// Returns a replay of shot and difficulty with one frame in each slot of
// slots, from 1 to 7, as SaveReplay writes it, in *size bytes.
static uint8_t *M_Replay(
    const uint8_t shot, const uint8_t difficulty, const uint32_t slots,
    size_t *const size)
{
    uint8_t plain[M_MAX_SIZE] = {};
    memcpy(plain, "T7RP", 4);
    plain[4] = 0x00;
    plain[5] = 0x11;
    plain[0xc] = 0x5a;
    plain[0xd] = 0xc3;
    plain[0x54] = 0x99;
    plain[0x55] = 0x62;
    plain[0x56] = shot;
    plain[0x57] = difficulty;
    memcpy(plain + 0x58, "11/15", 6);
    memcpy(plain + 0x5e, "TEST", 5);
    memcpy(plain + 0xe0, "0100b", 6);
    size_t end = M_HEADER_SIZE;
    for (int32_t slot = 1; slot <= 7; slot++) {
        if ((slots >> slot & 1) != 0) {
            M_PutU32(plain, 0x1c + 4 * (size_t)(slot - 1), (uint32_t)end);
            M_PutU32(plain, end, (uint32_t)slot * 100);
            end += M_STAGE_SIZE + 4;
        }
    }
    for (int32_t slot = 1; slot <= 7; slot++) {
        if ((slots >> slot & 1) != 0) {
            M_PutU32(plain, 0x38 + 4 * (size_t)(slot - 1), (uint32_t)end);
            plain[end++] = 0;
        }
    }
    // Packs every byte after the plain ones as an LZSS literal.
    uint8_t *const file = Mem_AllocZ(M_MAX_SIZE * 2);
    memcpy(file, plain, M_PLAIN_SIZE);
    uint32_t bit = 0;
    uint8_t *const packed = file + M_PLAIN_SIZE;
    for (size_t i = M_PLAIN_SIZE; i <= end; i++) {
        const uint32_t value = i < end ? 0x100u | plain[i] : 0;
        const int32_t count = i < end ? 9 : 14;
        for (int32_t b = count - 1; b >= 0; b--) {
            if ((value >> b & 1) != 0) {
                packed[bit / 8] |= (uint8_t)(0x80 >> (bit % 8));
            }
            bit++;
        }
    }
    *size = M_PLAIN_SIZE + bit / 8;
    M_PutU32(file, 0x10, (uint32_t)end);
    M_PutU32(file, 0x14, bit / 8);
    M_PutU32(file, 0x18, (uint32_t)(end - M_PLAIN_SIZE));
    uint32_t checksum = 0x3f000318;
    for (size_t i = 0xd; i < *size; i++) {
        checksum += file[i];
    }
    M_PutU32(file, 8, checksum);
    uint8_t key = file[0xd];
    for (size_t i = 0x10; i < *size; i++) {
        file[i] = (uint8_t)(file[i] + key);
        key = (uint8_t)(key + 7);
    }
    return file;
}

static void M_WriteReplay(
    const char *const name, const uint8_t shot, const uint8_t difficulty,
    const uint32_t slots)
{
    size_t size;
    uint8_t *const bytes = M_Replay(shot, difficulty, slots, &size);
    Th07Data_WriteFile(m_Directory, name, bytes, size);
    Mem_Free(bytes);
}

static void M_KeepDemo(
    const int32_t index, const uint8_t shot, const uint8_t difficulty,
    const uint32_t slots)
{
    size_t size;
    uint8_t *const bytes = M_Replay(shot, difficulty, slots, &size);
    Replay_AddFile(g_DemoReplayPaths[index], bytes, size);
    Mem_Free(bytes);
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
    Th07Data_WriteFile(m_Directory, name, bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
}

static void M_Setup(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    const char *const dirs[] = { "data", "data/title", "replay" };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char *const dir = TestDir_Join(m_Directory, dirs[i]);
        CHECK(Result_Log(File_MakeDir(dir), LOG_LEVEL_ERROR));
        Mem_Free(dir);
    }
    TH07_ANM_BUILDER builder;
    Th07AnmBuilder_Init(&builder);
    TH07_ANM_BUILDER_ANM *const anm = Th07AnmBuilder_Add(&builder, 32, 32, "@");
    Th07AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    Th07AnmBuilder_WriteFile(&builder, m_Directory, "data/title01.anm");
    M_WriteImage("data/title/title00.jpg");
    M_WriteImage("data/title/select00.jpg");
    M_WriteImage("data/title/phantasm.jpg");
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    SceneStubCalls_Reset();
    g_GameManager.mode.demo_index = 2;
    g_GameManager.phantasm_was_open = true;
    FakeShell_Reset();
    for (int32_t device = 0; device < INPUT_DEVICE_COUNT; device++) {
        FakeShell_SetDeviceInput(
            (INPUT_DEVICE)device, &m_NoInput, sizeof(m_NoInput));
    }
    Config_SetDefaults(&g_SettingsSchema, &g_Settings);
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    Sound_Reset();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Anm_SetScriptEntry(0x706, &m_HelpScript);
    Chain_Init(&g_Chain);
    CHECK(Result_Log(Scene_Register(), LOG_LEVEL_ERROR));
}

static void M_OpenSaves(void)
{
    ResultScreen_OpenSaves();
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    ResultScreen_CloseSaves();
    Replay_ClearFiles();
    GfxRecorder_Stop();
    Th07Data_Unmount();
    Mem_Free(m_Directory);
}

static void M_Tick(const TH07_INPUT input)
{
    Sound_Reset();
    g_Input = input;
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    Chain_RunCalc(&g_Chain);
    ResultScreen_ScanReplays();
}

static void M_Run(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Tick(m_NoInput);
    }
}

static void M_Press(const TH07_INPUT input)
{
    M_Tick(input);
    M_Tick(m_NoInput);
}

static void M_PressTimes(const TH07_INPUT input, const int32_t times)
{
    for (int32_t i = 0; i < times; i++) {
        M_Press(input);
    }
}

static bool M_TitleAdded(void)
{
    for (const CHAIN_ELEM *elem = g_Chain.calc.next; elem != nullptr;
         elem = elem->next) {
        if (elem->priority == 3) {
            return true;
        }
    }
    return false;
}

// Boots to the title menu and waits until it takes input.
static void M_OpenTitle(void)
{
    M_Run(10);
}

// Confirms the title menu entry at the cursor, then the difficulty,
// character and shot selects.
static void M_ConfirmSelects(void)
{
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Tick(m_Shoot);
}

static bool M_PlayRequested(void)
{
    return Scene_GetRequested() == SCENE_GAMEPLAY && !M_TitleAdded();
}

TEST(start_sets_up_a_game_from_the_selects)
{
    M_Setup("start");
    M_OpenTitle();
    M_ConfirmSelects();
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_STOP);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 1);
    CHECK_EQ_INT(g_GameManager.run.character, 0);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 0);
    CHECK_EQ_INT(g_GameManager.run.stage, 0);
    CHECK(!g_GameManager.mode.in_replay);
    CHECK(!g_GameManager.mode.in_practice);
    M_Tick(m_NoInput);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_register, 1);
    M_Teardown();
}

TEST(the_selects_choose_difficulty_character_and_shot)
{
    M_Setup("choose");
    M_OpenTitle();
    M_Press(m_Shoot);
    M_Run(30);
    M_PressTimes(m_Down, 2);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Left);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_GameManager.run.difficulty, 3);
    CHECK_EQ_INT(g_GameManager.run.character, 2);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 1);
    CHECK_EQ_INT(g_Settings.difficulty, 3);
    M_Teardown();
}

TEST(a_cancel_goes_back_one_select_and_keeps_the_choice)
{
    M_Setup("cancel");
    M_OpenTitle();
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Right);
    M_Press(m_Bomb);
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    M_Run(30);
    M_Press(m_Bomb);
    M_Run(30);
    CHECK_EQ_INT(g_Settings.difficulty, 2);
    M_Run(10);
    M_ConfirmSelects();
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_GameManager.run.difficulty, 2);
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    M_Teardown();
}

TEST(the_cursor_skips_a_locked_extra_start)
{
    M_Setup("extra_locked");
    M_OpenTitle();
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK(g_GameManager.mode.in_practice);
    M_Teardown();
}

TEST(extra_starts_at_stage_seven_with_an_open_shot)
{
    M_Setup("extra");
    M_OpenTitle();
    g_GameManager.records.clears[3].no_continues[1] = 99;
    M_Press(m_Down);
    M_ConfirmSelects();
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_GameManager.run.difficulty, 4);
    CHECK_EQ_INT(g_GameManager.run.stage, 6);
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 1);
    CHECK_EQ_INT(g_Settings.difficulty, 4);
    M_Teardown();
}

static void M_OpenPhantasm(void)
{
    for (int32_t i = 0; i < 60; i++) {
        g_GameManager.records.spell_captures[i * 2] = 1;
    }
    g_GameManager.records.clears[4].no_continues[0] = 99;
    g_GameManager.records.clears[4].no_continues[4] = 99;
}

TEST(phantasm_opens_with_sixty_cards_and_starts_at_stage_eight)
{
    M_Setup("phantasm");
    M_OpenTitle();
    M_OpenPhantasm();
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_GameManager.run.difficulty, 5);
    CHECK_EQ_INT(g_GameManager.run.stage, 7);
    CHECK_EQ_INT(g_GameManager.run.character, 2);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 0);
    CHECK_EQ_INT(g_GameManager.records.clears[4].no_continues[5], 99);
    M_Teardown();
}

TEST(practice_lists_the_cleared_stages)
{
    M_Setup("practice");
    M_OpenTitle();
    g_GameManager.records.clears[0].with_continues[0] = 3;
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Up);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Shoot);
    M_Run(30);
    M_Press(m_Up);
    M_Press(m_Shoot);
    CHECK(M_PlayRequested());
    CHECK(g_GameManager.mode.in_practice);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 0);
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
    M_Teardown();
}

TEST(a_whole_clear_opens_six_practice_stages)
{
    M_Setup("practice_all");
    M_OpenTitle();
    g_GameManager.records.clears[0].with_continues[1] = 99;
    M_Press(m_Down);
    M_ConfirmSelects();
    M_Run(30);
    M_Press(m_Up);
    M_Press(m_Shoot);
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_GameManager.run.stage, 5);
    M_Teardown();
}

TEST(the_title_goes_back_to_the_practice_stage_after_a_practice)
{
    M_Setup("practice_back");
    g_GameManager.records.clears[0].with_continues[1] = 6;
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.stage = 4;
    M_Tick(m_NoInput);
    CHECK(!g_GameManager.mode.in_practice);
    M_Run(4);
    CHECK(g_GameManager.mode.in_practice);
    M_Run(30);
    M_Press(m_Shoot);
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_GameManager.run.stage, 3);
    M_Teardown();
}

// Opens the replay menu from the title and waits for its list.
static void M_OpenReplays(void)
{
    M_OpenSaves();
    M_OpenTitle();
    M_PressTimes(m_Down, 2);
    M_Press(m_Shoot);
    M_Run(30);
    M_Run(10);
}

TEST(the_replay_menu_starts_a_stage_with_the_playback_choice)
{
    M_Setup("replay");
    M_WriteReplay("replay/th7_03.rpy", 3, 2, 1 << 2 | 1 << 4);
    M_OpenReplays();
    M_Press(m_Shoot);
    M_Press(m_Down);
    M_Press(m_Shoot);
    M_PressTimes(m_Down, 2);
    M_Press(m_Shoot);
    CHECK(M_PlayRequested());
    CHECK(g_GameManager.mode.in_replay);
    CHECK_EQ_INT(g_GameManager.mode.replay_menu_choice, 2);
    CHECK_EQ_INT(g_GameManager.run.stage, 3);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 2);
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 1);
    CHECK_EQ_INT(g_GameManager.run.shot_index, 3);
    CHECK(
        strstr(g_GameManager.mode.replay_path, "replay/th7_03.rpy") != nullptr);
    M_Teardown();
}

TEST(the_replay_menu_starts_at_the_first_stage_and_skips_missing_ones)
{
    M_Setup("replay_stage");
    M_WriteReplay("replay/th7_01.rpy", 0, 0, 1 << 3 | 1 << 6);
    M_OpenReplays();
    M_Press(m_Shoot);
    M_Press(m_Shoot);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
    CHECK_EQ_INT(g_GameManager.mode.replay_menu_choice, 0);
    M_Teardown();
    M_Setup("replay_stage_up");
    M_WriteReplay("replay/th7_01.rpy", 0, 0, 1 << 3 | 1 << 6);
    M_OpenReplays();
    M_Press(m_Shoot);
    M_Press(m_Up);
    M_Press(m_Shoot);
    M_Press(m_Up);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_GameManager.run.stage, 5);
    CHECK_EQ_INT(g_GameManager.mode.replay_menu_choice, 2);
    M_Teardown();
}

TEST(a_phantasm_replay_starts_at_stage_eight)
{
    M_Setup("replay_phantasm");
    M_WriteReplay("replay/th7_01.rpy", 4, 5, 1 << 7);
    M_OpenReplays();
    M_PressTimes(m_Shoot, 3);
    CHECK_EQ_INT(g_GameManager.run.stage, 7);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 5);
    M_Teardown();
}

TEST(the_replay_menu_pages_by_fifteen)
{
    M_Setup("replay_pages");
    for (int32_t i = 1; i <= 17; i++) {
        char name[32];
        snprintf(name, sizeof(name), "replay/th7_ud%04d.rpy", i);
        M_WriteReplay(name, (uint8_t)(i % 6), 1, 1 << 1);
    }
    M_WriteReplay("replay/th7_02.rpy", 0, 1, 1 << 1);
    M_OpenReplays();
    M_Press(m_Right);
    M_PressTimes(m_Shoot, 3);
    CHECK(strstr(g_GameManager.mode.replay_path, "th7_ud0015") != nullptr);
    M_Teardown();
    M_Setup("replay_pages_left");
    for (int32_t i = 1; i <= 17; i++) {
        char name[32];
        snprintf(name, sizeof(name), "replay/th7_ud%04d.rpy", i);
        M_WriteReplay(name, (uint8_t)(i % 6), 1, 1 << 1);
    }
    M_OpenReplays();
    M_Press(m_Left);
    M_PressTimes(m_Shoot, 3);
    CHECK(strstr(g_GameManager.mode.replay_path, "th7_ud0003") != nullptr);
    M_Teardown();
}

TEST(the_replay_menu_waits_ten_calcs_and_cancels_back)
{
    M_Setup("replay_cancel");
    M_WriteReplay("replay/th7_01.rpy", 0, 0, 1 << 1);
    M_OpenSaves();
    M_OpenTitle();
    M_PressTimes(m_Down, 2);
    M_Press(m_Shoot);
    M_Run(30);
    M_Run(8);
    M_PressTimes(m_Shoot, 3);
    CHECK(M_TitleAdded());
    M_Press(m_Bomb);
    M_Press(m_Bomb);
    M_Press(m_Bomb);
    M_Run(40);
    M_Press(m_Shoot);
    M_Run(40);
    M_PressTimes(m_Shoot, 3);
    CHECK(M_PlayRequested());
    M_Teardown();
}

TEST(a_replay_menu_after_a_replay_opens_at_once)
{
    M_Setup("replay_back");
    M_WriteReplay("replay/th7_01.rpy", 0, 0, 1 << 1);
    M_OpenSaves();
    g_GameManager.mode.in_replay = true;
    g_GameManager.run.character = 2;
    g_GameManager.run.shot_index = 5;
    M_Tick(m_NoInput);
    CHECK(!g_GameManager.mode.in_replay);
    CHECK_EQ_INT(g_GameManager.run.character, 0);
    CHECK_EQ_INT(g_GameManager.run.shot_index, 0);
    M_Run(40);
    M_PressTimes(m_Shoot, 3);
    CHECK(M_PlayRequested());
    M_Teardown();
}

// Opens the options from the title, ready for input.
static void M_OpenOptions(void)
{
    M_OpenTitle();
    M_PressTimes(m_Up, 2);
    M_Press(m_Shoot);
    M_Run(30);
    M_Run(4);
}

TEST(the_options_change_the_settings_and_the_window_mode)
{
    M_Setup("options");
    M_OpenOptions();
    M_Press(m_Right);
    CHECK_EQ_INT(g_Settings.lives, 3);
    M_Press(m_Down);
    M_Press(m_Left);
    M_Press(m_Down);
    M_Tick(m_Left);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_INT(g_Settings.music, 0);
    M_Tick(m_NoInput);
    M_Press(m_Down);
    M_Press(m_Right);
    CHECK(!g_Settings.sound);
    M_Press(m_Down);
    M_Press(m_Right);
    CHECK_EQ_INT(g_FakeShell.request_count, 0);
    M_Press(m_Down);
    M_Press(m_Left);
    CHECK(g_Settings.slowdown);
    M_Press(m_Bomb);
    M_Press(m_Bomb);
    CHECK_EQ_INT(g_FakeShell.request_count, 1);
    CHECK_EQ_INT(g_FakeShell.requests[0].kind, 'f');
    CHECK_EQ_INT(g_FakeShell.requests[0].action, 1);
    M_Run(10);
    M_Press(m_Shoot);
    M_Run(30);
    M_Run(4);
    M_PressTimes(m_Down, 6);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_Settings.lives, 2);
    CHECK_EQ_INT(g_Settings.music, SETTINGS_MUSIC_WAVE);
    CHECK(g_Settings.sound);
    CHECK(!g_Settings.slowdown);
    M_Teardown();
}

TEST(the_options_close_after_an_idle_minute)
{
    M_Setup("options_idle");
    M_OpenOptions();
    M_Run(3600 - 6);
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    M_Run(1);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_CANCEL);
    M_Teardown();
}

static INPUT_BINDING M_Binding(const char *const name)
{
    INPUT_BINDING binding;
    CHECK(Input_BindingFromName(name, &binding));
    return binding;
}

static void M_Bind(const TH07_ACTION action, const char *const name)
{
    g_FakeShell.bindings[action][g_FakeShell.binding_counts[action]++] =
        M_Binding(name);
}

static void M_OpenKeyConfig(void)
{
    M_Bind(TH07_ACTION_shoot, "south");
    M_Bind(TH07_ACTION_focus, "west");
    M_Bind(TH07_ACTION_bomb, "east");
    M_OpenOptions();
    M_PressTimes(m_Down, 7);
    M_Press(m_Shoot);
}

TEST(the_key_config_swaps_a_pressed_button_with_its_row)
{
    M_Setup("keys");
    M_OpenKeyConfig();
    g_FakeShell.pad_pending = true;
    g_FakeShell.pad_press = M_Binding("west");
    M_Tick(m_NoInput);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_OK);
    M_PressTimes(m_Up, 1);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_FakeShell.request_count, 4);
    CHECK_EQ_INT(g_FakeShell.requests[0].kind, 'u');
    CHECK_EQ_INT(g_FakeShell.requests[0].action, TH07_ACTION_shoot);
    CHECK_EQ_INT(g_FakeShell.requests[1].kind, 'b');
    CHECK_EQ_INT(g_FakeShell.requests[1].action, TH07_ACTION_shoot);
    CHECK(
        g_FakeShell.requests[1].binding.codes[0] == M_Binding("west").codes[0]);
    CHECK_EQ_INT(g_FakeShell.requests[3].kind, 'b');
    CHECK_EQ_INT(g_FakeShell.requests[3].action, TH07_ACTION_focus);
    CHECK(
        g_FakeShell.requests[3].binding.codes[0]
        == M_Binding("south").codes[0]);
    M_Teardown();
}

TEST(the_key_config_toggles_shoot_focus_and_restores_defaults)
{
    M_Setup("keys_focus");
    M_OpenKeyConfig();
    M_PressTimes(m_Down, 9);
    M_Press(m_Right);
    CHECK(!g_Settings.shoot_focuses);
    M_Press((TH07_INPUT) { .left = true, .right = true });
    CHECK(!g_Settings.shoot_focuses);
    M_Press(m_Left);
    CHECK(g_Settings.shoot_focuses);
    M_Press(m_Left);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK(g_Settings.shoot_focuses);
    M_Press(m_Down);
    M_Press(m_Shoot);
    CHECK_EQ_INT(g_FakeShell.requests[0].kind, 'r');
    M_Teardown();
}

TEST(the_demo_starts_after_nine_hundred_idle_calcs_and_cycles)
{
    M_Setup("demo");
    M_KeepDemo(0, 2, 3, 1 << 4);
    M_KeepDemo(1, 1, 0, 1 << 1);
    M_KeepDemo(2, 5, 4, 1 << 7);
    const int32_t stages[] = { 3, 0, 6, 3 };
    for (int32_t round = 0; round < 4; round++) {
        M_Run(M_IDLE_TEST_FRAMES);
        CHECK(M_TitleAdded());
        M_Tick(m_NoInput);
        CHECK(M_PlayRequested());
        CHECK_EQ_INT(g_GameManager.mode.demo_index, round % 3);
        CHECK(g_GameManager.mode.in_demo);
        CHECK(g_GameManager.mode.in_replay);
        CHECK_EQ_INT(g_GameManager.run.stage, stages[round]);
        CHECK_EQ_STR(
            g_GameManager.mode.replay_path, g_DemoReplayPaths[round % 3]);
        M_Tick(m_NoInput);
        Scene_Request(SCENE_TITLE);
    }
    CHECK_EQ_INT(g_GameManager.run.difficulty, 3);
    CHECK_EQ_INT(g_GameManager.run.shot_index, 2);
    M_Teardown();
}

TEST(input_holds_the_demo_off_and_a_missing_demo_waits_again)
{
    M_Setup("demo_missing");
    M_Run(800);
    M_Press(m_Down);
    M_Run(M_IDLE_TEST_FRAMES);
    CHECK(M_TitleAdded());
    M_Run(1);
    CHECK(M_TitleAdded());
    CHECK_EQ_INT(g_GameManager.mode.demo_index, 0);
    M_KeepDemo(1, 0, 0, 1 << 1);
    M_Run(M_IDLE_TEST_FRAMES - 1);
    CHECK(M_TitleAdded());
    M_Run(1);
    CHECK(M_PlayRequested());
    CHECK_EQ_INT(g_GameManager.mode.demo_index, 1);
    M_Teardown();
}

static void M_UnlockOnLoad(void)
{
    M_OpenPhantasm();
}

TEST(the_phantasm_splash_shows_once_for_nine_hundred_calcs)
{
    M_Setup("splash");
    g_GameManager.phantasm_was_open = false;
    g_SceneStubCalls.on_load_title_records = M_UnlockOnLoad;
    M_Run(900);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
    M_Tick(m_NoInput);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK(g_GameManager.phantasm_was_open);
    M_Teardown();
    M_Setup("splash_open");
    g_SceneStubCalls.on_load_title_records = M_UnlockOnLoad;
    M_Tick(m_NoInput);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    M_Teardown();
}

TEST(a_held_key_cuts_the_splash_short_at_the_120th_calc)
{
    M_Setup("splash_skip");
    g_GameManager.phantasm_was_open = false;
    g_SceneStubCalls.on_load_title_records = M_UnlockOnLoad;
    for (int32_t i = 0; i < 120; i++) {
        M_Tick(m_Bomb);
    }
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    M_Tick(m_Bomb);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_OK);
    M_Run(59);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
    M_Tick(m_NoInput);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    M_Teardown();
}

TEST(the_title_music_keeps_playing_back_from_the_scores)
{
    M_Setup("scores");
    M_Tick(m_NoInput);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th07_01.wav");
    M_Run(9);
    M_PressTimes(m_Down, 3);
    M_Press(m_Shoot);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_SCORES);
    M_Tick(m_NoInput);
    Scene_Request(SCENE_TITLE);
    M_Tick(m_NoInput);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
    CHECK_EQ_INT(g_SceneStubCalls.fade_ins, 2);
    M_Teardown();
}

TEST(quit_leaves_sixty_calcs_after_the_choice)
{
    M_Setup("quit");
    M_OpenTitle();
    M_Press(m_Bomb);
    M_Tick(m_Shoot);
    M_Run(59);
    CHECK(M_TitleAdded());
    M_Run(1);
    CHECK(!M_TitleAdded());
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_QUIT);
    M_Teardown();
}
