#include "tests/gfx_recorder.h"
#include "tests/shell_fake.h"
#include "tests/test.h"
#include "tests/test_dir.h"
#include "tests/th07/th07_anm_builder.h"
#include "tests/th07/th07_data.h"
#include "th07/buttons.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screens/result/saves.h"
#include "th07/settings.h"
#include "th07/sound.h"

#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/gfx.h"
#include "core/image.h"
#include "core/io/file.h"

#include <string.h>

#define M_TITLE_PRIORITY 3
#define M_MUSIC_PRIORITY 3
#define M_RESULT_PRIORITY 14
#define M_RECORD_PRIORITY 16

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static const TH07_INPUT m_NoInput;
static const TH07_INPUT m_Shoot = { .shoot = true };
static const TH07_INPUT m_Bomb = { .bomb = true };
static const TH07_INPUT m_Down = { .down = true };
static char *m_Directory;
static int32_t m_Registers;
static int32_t m_Removes;
static bool m_RecordingAtRegister;

static int32_t M_CalcCount(const int32_t priority)
{
    int32_t count = 0;
    for (const CHAIN_ELEM *elem = g_Chain.calc.next; elem != nullptr;
         elem = elem->next) {
        count += elem->priority == priority;
    }
    return count;
}

// Stands in for the gameplay, which needs the files of every stage module,
// and moves to the next stage and records a replay as the real one does.
RESULT GameManager_Register(void)
{
    m_Registers++;
    m_RecordingAtRegister = M_CalcCount(M_RECORD_PRIORITY) != 0;
    g_GameManager.run.stage++;
    return Replay_Register(REPLAY_RECORD, "replay/th7_00.rpy");
}

void GameManager_Remove(void)
{
    m_Removes++;
}

static void M_SetDevice(const INPUT_DEVICE device, const TH07_INPUT *input)
{
    FakeShell_SetDeviceInput(device, input, sizeof(*input));
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

// Writes the files that the title, the result screen, the music room and
// the ending load.
static void M_WriteData(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    const char *const dirs[] = {
        "data", "data/title", "data/result", "data/end", "replay",
    };
    for (size_t i = 0; i < ARRAY_SIZE(dirs); i++) {
        char *const dir = TestDir_Join(m_Directory, dirs[i]);
        CHECK(Result_Log(File_MakeDir(dir), LOG_LEVEL_ERROR));
        Mem_Free(dir);
    }
    const char *const anms[] = {
        "data/text.anm",    "data/ascii.anm",    "data/capture.anm",
        "data/title01.anm", "data/result00.anm", "data/music00.anm",
        "data/staff01.anm",
    };
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 32, 32, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    for (size_t i = 0; i < ARRAY_SIZE(anms); i++) {
        AnmBuilder_WriteFile(&builder, m_Directory, anms[i]);
    }
    const char *const images[] = {
        "data/title/title00.jpg",  "data/title/select00.jpg",
        "data/title/phantasm.jpg", "data/result/result.jpg",
        "data/result/music.jpg",
    };
    for (size_t i = 0; i < ARRAY_SIZE(images); i++) {
        M_WriteImage(images[i]);
    }
    const char comments[] = "@bgm/th07_00.mid\r\nTune\r\n\r\nComment\r\n";
    Th07Data_WriteFile(
        m_Directory, "data/musiccmt.txt", comments, sizeof(comments) - 1);
    const char *const endings[] = {
        "data/end00.end",  "data/end01.end",  "data/end10.end",
        "data/end11.end",  "data/end20.end",  "data/end21.end",
        "data/end00b.end", "data/end10b.end", "data/end20b.end",
    };
    const char ending[] = "@z\r\n";
    for (size_t i = 0; i < ARRAY_SIZE(endings); i++) {
        Th07Data_WriteFile(m_Directory, endings[i], ending, sizeof(ending) - 1);
    }
}

static void M_Setup(const char *const case_name)
{
    M_WriteData(case_name);
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Config_SetDefaults(&g_SettingsSchema, &g_Settings);
    GameManager_Create(&g_GameManager);
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    FakeShell_Reset();
    for (int32_t device = 0; device < INPUT_DEVICE_COUNT; device++) {
        M_SetDevice((INPUT_DEVICE)device, &m_NoInput);
    }
    g_Settings.shoot_focuses = true;
    ResultScreen_OpenSaves();
    Sound_Reset();
    m_Registers = 0;
    m_Removes = 0;
    m_RecordingAtRegister = false;
    CHECK(Result_Log(Game_Init(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Game_Shutdown();
    ResultScreen_CloseSaves();
    GfxRecorder_Stop();
    Th07Data_Unmount();
    Sound_Reset();
    Mem_Free(m_Directory);
}

// Runs a tick within a frame, as the shell does.
static bool M_Tick(const TH07_INPUT *const input)
{
    Sound_Reset();
    Gfx_BeginTick((GFX_COLOR) {});
    const bool running = Game_Tick(input);
    Gfx_EndTick();
    return running;
}

static void M_Run(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Tick(&m_NoInput);
    }
}

static void M_Press(const TH07_INPUT *const input)
{
    M_Tick(input);
    M_Tick(&m_NoInput);
}

// Boots to the title menu, waits until it takes input and moves the cursor
// down by downs.
static void M_OpenTitle(const int32_t downs)
{
    M_Run(10);
    for (int32_t i = 0; i < downs; i++) {
        M_Press(&m_Down);
    }
}

// Starts a game from the title through its selects, then sets the stage.
static void M_StartGameplay(const char *const case_name, const int32_t stage)
{
    M_Setup(case_name);
    M_OpenTitle(0);
    for (int32_t i = 0; i < 3; i++) {
        M_Press(&m_Shoot);
        M_Run(30);
    }
    M_Tick(&m_Shoot);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    g_GameManager.run.stage = stage;
}

typedef struct {
    int32_t calcs;
    int32_t draws;
    int32_t stop_at;
    CHAIN_STATUS status;
} M_WARMUP;

static CHAIN_STATUS M_WarmupCalc(void *const arg)
{
    M_WARMUP *const warmup = arg;
    warmup->calcs++;
    CHECK(g_Input.shoot);
    return warmup->calcs == warmup->stop_at ? warmup->status : CHAIN_CONTINUE;
}

static CHAIN_STATUS M_WarmupDraw(void *const arg)
{
    M_WARMUP *const warmup = arg;
    warmup->draws++;
    return CHAIN_CONTINUE;
}

TEST(warmup_updates_thirty_times_without_drawing)
{
    M_Setup("warmup");
    M_WARMUP warmup = {};
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    Chain_InitElem(&calc, M_WarmupCalc, &warmup);
    Chain_InitElem(&draw, M_WarmupDraw, &warmup);
    CHECK(Result_Log(Chain_AddCalc(&calc, 1), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Chain_AddDraw(&draw, 1), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Game_Warmup(&m_Shoot), CHAIN_RUN_CONTINUE);
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(warmup.calcs, 30);
    CHECK_EQ_INT(warmup.draws, 0);
    CHECK_EQ_INT(Game_Warmup(&m_Shoot), CHAIN_RUN_CONTINUE);
    CHECK_EQ_INT(warmup.calcs, 30);
    CHECK(M_Tick(&m_Shoot));
    CHECK_EQ_INT(warmup.calcs, 31);
    CHECK_EQ_INT(warmup.draws, 1);
    M_Teardown();
}

TEST(warmup_does_not_run_after_an_ordinary_boot_tick)
{
    M_Setup("warmup_once");
    M_WARMUP warmup = {};
    CHAIN_ELEM calc;
    Chain_InitElem(&calc, M_WarmupCalc, &warmup);
    CHECK(Result_Log(Chain_AddCalc(&calc, 1), LOG_LEVEL_ERROR));
    CHECK(M_Tick(&m_Shoot));
    CHECK_EQ_INT(warmup.calcs, 1);
    CHECK_EQ_INT(Game_Warmup(&m_Shoot), CHAIN_RUN_CONTINUE);
    CHECK_EQ_INT(warmup.calcs, 1);
    M_Teardown();
}

TEST(warmup_stops_for_quit_and_reset)
{
    const CHAIN_STATUS status[] = { CHAIN_QUIT, CHAIN_RESET };
    const CHAIN_RUN expected[] = { CHAIN_RUN_QUIT, CHAIN_RUN_RESET };
    for (size_t i = 0; i < ARRAY_SIZE(status); i++) {
        M_Setup("warmup_stop");
        M_WARMUP warmup = { .stop_at = 4, .status = status[i] };
        CHAIN_ELEM calc;
        Chain_InitElem(&calc, M_WarmupCalc, &warmup);
        CHECK(Result_Log(Chain_AddCalc(&calc, 1), LOG_LEVEL_ERROR));
        CHECK_EQ_INT(Game_Warmup(&m_Shoot), expected[i]);
        CHECK_EQ_INT(warmup.calcs, 4);
        M_Teardown();
    }
}

TEST(boot_goes_to_title)
{
    M_Setup("boot");
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_QUIT);
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(Scene_GetPrevious(), SCENE_BOOT);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 1);
    M_Teardown();
}

TEST(title_stays_without_a_request)
{
    M_Setup("stay");
    for (int tick = 0; tick < 10; tick++) {
        CHECK(M_Tick(&m_NoInput));
    }
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 1);
    M_Teardown();
}

TEST(title_goes_to_gameplay)
{
    M_StartGameplay("gameplay", 1);
    CHECK_EQ_INT(m_Registers, 1);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 0);
    CHECK_EQ_INT(M_CalcCount(M_RECORD_PRIORITY), 1);
    M_Teardown();
}

TEST(quit_from_title_ends_the_tick_with_false)
{
    M_Setup("quit");
    M_Tick(&m_NoInput);
    Scene_Request(SCENE_QUIT);
    CHECK(!M_Tick(&m_NoInput));
    M_Teardown();
}

TEST(reset_starts_again_from_boot)
{
    M_Setup("reset");
    M_Tick(&m_NoInput);
    Scene_Request(SCENE_RESET);
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_BOOT);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 0);
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 1);
    M_Teardown();
}

// Leaves the scores from their first page.
static void M_LeaveScores(void)
{
    M_Run(21);
    M_Press(&m_Bomb);
    M_Press(&m_Bomb);
    M_Run(62);
}

TEST(title_goes_to_scores_and_music_room_and_back)
{
    M_Setup("rooms");
    M_OpenTitle(3);
    M_Press(&m_Shoot);
    CHECK_EQ_INT(Scene_Get(), SCENE_SCORES);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 0);
    CHECK_EQ_INT(M_CalcCount(M_RESULT_PRIORITY), 1);
    M_LeaveScores();
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(M_CalcCount(M_RESULT_PRIORITY), 0);
    M_OpenTitle(1);
    M_Press(&m_Shoot);
    CHECK_EQ_INT(Scene_Get(), SCENE_MUSIC_ROOM);
    CHECK_EQ_INT(M_CalcCount(M_MUSIC_PRIORITY), 1);
    M_Run(10);
    M_Press(&m_Bomb);
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(Scene_GetPrevious(), SCENE_MUSIC_ROOM);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 1);
    M_Teardown();
}

TEST(the_title_sees_the_scene_it_came_from)
{
    M_Setup("previous");
    M_OpenTitle(3);
    M_Press(&m_Shoot);
    M_LeaveScores();
    CHECK_EQ_INT(Scene_GetPrevious(), SCENE_SCORES);
    // The title comes back with its cursor on the scores.
    M_OpenTitle(0);
    M_Press(&m_Shoot);
    CHECK_EQ_INT(Scene_Get(), SCENE_SCORES);
    M_Teardown();
}

TEST(next_stage_reregisters_the_game)
{
    M_StartGameplay("next_stage", 2);
    Scene_Request(SCENE_NEXT_STAGE);
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(m_Removes, 1);
    CHECK_EQ_INT(m_Registers, 2);
    CHECK_EQ_INT(g_GameManager.run.stage, 3);
    CHECK(m_RecordingAtRegister);
    M_Teardown();
}

TEST(retry_starts_a_game_from_the_first_stage)
{
    M_StartGameplay("retry", 4);
    g_GameManager.run.difficulty = 3;
    Scene_Request(SCENE_RETRY);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.stage, 1);
    CHECK_EQ_INT(m_Registers, 2);
    CHECK(!m_RecordingAtRegister);
    CHECK_EQ_INT(M_CalcCount(M_RECORD_PRIORITY), 1);
    M_Teardown();
}

TEST(retry_replays_the_stage_in_a_practice_and_in_extra)
{
    M_StartGameplay("retry_practice", 4);
    g_GameManager.mode.in_practice = true;
    Scene_Request(SCENE_RETRY);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(g_GameManager.run.stage, 4);
    M_Teardown();

    M_StartGameplay("retry_extra", 7);
    g_GameManager.run.difficulty = 4;
    Scene_Request(SCENE_RETRY);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(g_GameManager.run.stage, 7);
    M_Teardown();
}

TEST(gameplay_to_title_discards_the_replay)
{
    M_StartGameplay("discard", 1);
    Scene_Request(SCENE_TITLE);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(m_Removes, 1);
    CHECK_EQ_INT(M_CalcCount(M_RECORD_PRIORITY), 0);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 1);
    M_Teardown();
}

TEST(replay_end_discards_the_replay_and_goes_to_the_title)
{
    M_StartGameplay("replay_end", 1);
    Scene_Request(SCENE_REPLAY_END);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(M_CalcCount(M_RECORD_PRIORITY), 0);
    CHECK_EQ_INT(M_CalcCount(M_TITLE_PRIORITY), 1);
    M_Teardown();
}

TEST(ending_counts_a_clear_of_the_difficulty)
{
    M_StartGameplay("ending", 6);
    g_GameManager.run.difficulty = 2;
    Scene_Request(SCENE_ENDING);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_ENDING);
    CHECK_EQ_INT(g_GameManager.records.play_stats.counts[2].clears, 1);
    CHECK(g_GameManager.mode.game_cleared);
    Scene_Request(SCENE_GAME_RESULT);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_GAME_RESULT);
    CHECK_EQ_INT(M_CalcCount(M_RESULT_PRIORITY), 1);
    M_Teardown();
}

TEST(game_result_discards_the_replay_on_quit)
{
    M_StartGameplay("result_quit", 1);
    Scene_Request(SCENE_GAME_RESULT);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(M_CalcCount(M_RECORD_PRIORITY), 1);
    Scene_Request(SCENE_QUIT);
    CHECK(!M_Tick(&m_NoInput));
    CHECK_EQ_INT(M_CalcCount(M_RECORD_PRIORITY), 0);
    M_Teardown();
}

TEST(a_switch_clears_the_buttons)
{
    M_Setup("buttons");
    M_Tick(&m_NoInput);

    M_Tick(&m_Shoot);
    CHECK((g_Buttons.word & TH07_BUTTON_shoot) != 0);

    Scene_Request(SCENE_GAMEPLAY);
    M_Tick(&m_Shoot);
    CHECK((g_Buttons.word & TH07_BUTTON_shoot) == 0);
    CHECK_EQ_INT(g_Buttons.word, 0);
    M_Teardown();
}

TEST(a_dropped_calc_merges_the_next_buttons)
{
    M_Setup("dropped");
    M_Tick(&m_NoInput);
    M_Tick(&m_Shoot);
    g_GameManager.mode.calc_dropped = true;
    M_Tick(&m_Bomb);
    CHECK_EQ_INT(g_Buttons.word, 0x3);
    CHECK_EQ_INT(g_Buttons.previous_word, 0);
    M_Teardown();
}

TEST(the_shoot_focuses_setting_reaches_the_buttons)
{
    M_Setup("focus");
    M_SetDevice(INPUT_DEVICE_GAMEPAD, &m_Shoot);
    for (int32_t calc = 0; calc < 10; calc++) {
        M_Tick(&m_Shoot);
    }
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
    M_Teardown();

    M_Setup("no_focus");
    g_Settings.shoot_focuses = false;
    M_SetDevice(INPUT_DEVICE_GAMEPAD, &m_Shoot);
    for (int32_t calc = 0; calc < 10; calc++) {
        M_Tick(&m_Shoot);
    }
    CHECK((g_Buttons.word & TH07_BUTTON_focus) == 0);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);
    M_Teardown();
}
