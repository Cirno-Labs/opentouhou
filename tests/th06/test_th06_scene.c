#include "tests/gfx_recorder.h"
#include "tests/shell_fake.h"
#include "tests/test.h"
#include "tests/test_dir.h"
#include "tests/th06/anm_builder.h"
#include "tests/th06/th06_data.h"
#include "th06/buttons.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"
#include "th06/screens/result.h"
#include "th06/screens/result/private.h"
#include "th06/settings.h"
#include "th06/sound.h"

#include "core/base/memory.h"
#include "core/gfx.h"
#include "core/image.h"
#include "core/io/file.h"
#include "core/io/writer.h"

static const char *const m_AnmFiles[] = {
    "data/text.anm",     "data/ascii.anm",    "data/asciis.anm",
    "data/capture.anm",  "data/title01.anm",  "data/title02.anm",
    "data/title03.anm",  "data/title04.anm",  "data/title01s.anm",
    "data/title04s.anm", "data/select01.anm", "data/select02.anm",
    "data/select03.anm", "data/select04.anm", "data/select05.anm",
    "data/slpl00a.anm",  "data/slpl00b.anm",  "data/slpl01a.anm",
    "data/slpl01b.anm",  "data/replay00.anm",
};

static const TH06_INPUT m_NoInput;
static char *m_Directory;
static int32_t m_Registers;
static SCENE m_RegisterScene;
static int32_t m_Removes;
static SCENE m_RemoveScene;

// Stands in for the gameplay, which needs the files of every stage module.
RESULT GameManager_Register(void)
{
    m_Registers++;
    m_RegisterScene = Scene_GetRequested();
    return OK;
}

void GameManager_Remove(void)
{
    m_Removes++;
    m_RemoveScene = Scene_GetRequested();
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
    CHECK(Result_Log(Th06Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Config_SetDefaults(&g_SettingsSchema, &g_Settings);
    ResultScreen_OpenSaves();
    Sound_Reset();
    m_Registers = 0;
    m_Removes = 0;
    CHECK(Result_Log(Game_Init(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Game_Shutdown();
    ResultScreen_CloseSaves();
    GfxRecorder_Stop();
    Th06Data_Unmount();
    Sound_Reset();
    Mem_Free(m_Directory);
}

static bool M_TitleRuns(void)
{
    for (const CHAIN_ELEM *elem = g_Chain.calc.next; elem != nullptr;
         elem = elem->next) {
        if (elem->priority == 2) {
            return true;
        }
    }
    return false;
}

// Runs a tick within a frame, as the shell does.
static bool M_Tick(const TH06_INPUT *const input)
{
    Gfx_BeginTick((GFX_COLOR) {});
    const bool running = Game_Tick(input);
    Gfx_EndTick();
    return running;
}

TEST(boot_goes_to_title)
{
    M_Setup("boot");
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK(M_TitleRuns());
    M_Teardown();
}

TEST(title_goes_to_gameplay)
{
    M_Setup("gameplay");
    M_Tick(&m_NoInput);
    Scene_Request(SCENE_GAMEPLAY);
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(m_Registers, 1);
    M_Teardown();
}

TEST(gameplay_next_stage_reregisters)
{
    M_Setup("next_stage");
    M_Tick(&m_NoInput);
    Scene_Request(SCENE_GAMEPLAY);
    M_Tick(&m_NoInput);
    Scene_Request(SCENE_NEXT_STAGE);
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(m_Removes, 1);
    CHECK_EQ_INT(m_Registers, 2);
    M_Teardown();
}

TEST(a_switch_to_the_next_stage_requests_it_until_it_is_set_up)
{
    M_Setup("next_stage_request");
    M_Tick(&m_NoInput);
    Scene_Request(SCENE_GAMEPLAY);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(m_RegisterScene, SCENE_GAMEPLAY);
    Scene_Request(SCENE_NEXT_STAGE);
    M_Tick(&m_NoInput);
    CHECK_EQ_INT(m_RemoveScene, SCENE_NEXT_STAGE);
    CHECK_EQ_INT(m_RegisterScene, SCENE_NEXT_STAGE);
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_GAMEPLAY);
    M_Teardown();
}

TEST(quit_ends_the_tick_with_false)
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
    CHECK(M_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK(M_TitleRuns());
    M_Teardown();
}

TEST(a_switch_clears_the_buttons)
{
    M_Setup("buttons");
    M_Tick(&m_NoInput); // boot to title

    TH06_INPUT input = {};
    input.shoot = true;
    M_Tick(&input); // no switch this calc
    CHECK(g_Buttons.held.shoot);

    Scene_Request(SCENE_GAMEPLAY);
    M_Tick(&input); // switches to gameplay
    CHECK(!g_Buttons.held.shoot);
    CHECK_EQ_INT(g_Buttons.word, 0);
    M_Teardown();
}

TEST(a_tick_counts_the_measured_tick_rate)
{
    M_Setup("tick_rate");
    ResultScreen_ResetFrames();
    g_GameManager.mode.playing = true;
    M_Tick(&m_NoInput);
    CHECK(ResultSaves_GetClock()->expected_frames == 0.0f);
    g_FakeShell.tick_rate_measured = true;
    g_FakeShell.tick_rate = 45.0f;
    M_Tick(&m_NoInput);
    CHECK(!g_FakeShell.tick_rate_measured);
    CHECK(ResultSaves_GetClock()->expected_frames == 60.0f);
    CHECK(ResultSaves_GetClock()->achieved_frames == 48.0f);
    g_GameManager.mode.playing = false;
    ResultScreen_ResetFrames();
    M_Teardown();
}
