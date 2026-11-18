#include "th07/screens/ending/screen.h"
#include "th07/screens/ending/script.h"

#include "gfx_recorder.h"
#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/buttons.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/scene.h"
#include "th07/sound.h"

#include "core/base/memory.h"
#include "core/image.h"
#include "core/io/file.h"
#include "core/patch.h"

#include <string.h>

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
float g_GameSpeed = 1.0f;

ASCII_TEXT g_AsciiText;

static char *m_Directory;
static SCENE m_Requested;
static TH07_INPUT m_Input;
static CHAIN_ELEM m_InputElem;
static char m_Music[64];
static float m_Fade;

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
}

SCENE Scene_Get(void)
{
    return SCENE_ENDING;
}

SCENE Scene_GetRequested(void)
{
    return m_Requested;
}

static ENDING_SCRIPT M_Script(const char *const text, const size_t size)
{
    ENDING_SCRIPT script;
    EndingScript_Init(text, size, &script);
    return script;
}

TEST(a_text_line_converts_from_cp932_as_a_whole)
{
    const char text[] = "\x8d\x67\x96\x82\r\n"; // 紅魔
    ENDING_SCRIPT script = M_Script(text, sizeof(text) - 1);
    CHECK_EQ_STR(EndingScript_GetLine(&script, 0), "紅魔");
    EndingScript_Free(&script);
}

TEST(a_command_line_has_no_text)
{
    const char text[] = "@c255\0\r\nAB\r\n";
    ENDING_SCRIPT script = M_Script(text, sizeof(text) - 1);
    CHECK(EndingScript_GetLine(&script, 0) == nullptr);
    CHECK_EQ_STR(EndingScript_GetLine(&script, 8), "AB");
    EndingScript_Free(&script);
}

TEST(a_line_cut_by_the_end_of_the_data_still_converts)
{
    const char text[] = "AB";
    ENDING_SCRIPT script = M_Script(text, sizeof(text) - 1);
    CHECK_EQ_STR(EndingScript_GetLine(&script, 0), "AB");
    EndingScript_Free(&script);
}

static CHAIN_STATUS M_InputUpdate(void *const arg)
{
    Buttons_Update(&g_Buttons, &m_Input, false);
    return CHAIN_CONTINUE;
}

static void M_WriteScript(
    const char *const name, const char *const text, const size_t size)
{
    Th07Data_WriteFile(m_Directory, name, text, size);
}

#define M_SCRIPT(name_, text_) M_WriteScript(name_, text_, sizeof(text_) - 1)

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };

static void M_WriteData(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const end = TestDir_Join(m_Directory, "data/end");
    CHECK(Result_Log(File_MakeDir(end), LOG_LEVEL_ERROR));
    Mem_Free(end);

    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 32, 32, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    const uint32_t sprite = 0;
    const int32_t s = AnmBuilder_BeginScript(anm, 0);
    AnmBuilder_Instr(anm, s, 3, 0, 0, &sprite, 1);
    AnmBuilder_Instr(anm, s, 20, 10000, 0, nullptr, 0);
    AnmBuilder_End(anm, s);
    AnmBuilder_WriteFile(&builder, m_Directory, "data/staff01.anm");

    const char *const names[] = {
        "data/end00.end",  "data/end01.end",  "data/end10.end",
        "data/end11.end",  "data/end20.end",  "data/end21.end",
        "data/end00b.end", "data/end10b.end", "data/end20b.end",
    };
    for (size_t i = 0; i < 9; i++) {
        char text[64];
        const int length =
            snprintf(text, sizeof(text), "@m%s\r\n@z\r\n", names[i]);
        text[2 + strlen(names[i])] = '\0';
        M_WriteScript(names[i], text, (size_t)length);
    }
}

static void M_Setup(const char *const case_name)
{
    M_WriteData(case_name);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    for (int32_t i = 0; i < SCORE_SHOT_COUNT; i++) {
        for (int32_t j = 0; j < 6; j++) {
            g_GameManager.records.clears[i].no_continues[j] = 1;
            g_GameManager.records.clears[i].with_continues[j] = 1;
        }
    }
    g_GameManager.run.difficulty = 1;
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    memset(&m_Input, 0, sizeof(m_Input));
    m_Requested = SCENE_ENDING;
    m_Music[0] = '\0';
    m_Fade = 0.0f;
    Sound_Reset();
    Chain_InitElem(&m_InputElem, M_InputUpdate, nullptr);
    CHECK(
        Result_Log(Chain_AddCalc(&g_Chain, &m_InputElem, 0), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    GfxRecorder_Stop();
    Files_Shutdown();
    Sound_Reset();
    Mem_Free(m_Directory);
}

static void M_Tick(void)
{
    Sound_Reset();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    Chain_RunCalc(&g_Chain);
    if (g_SoundEvents.music == SOUND_MUSIC_PLAY) {
        memcpy(m_Music, g_SoundEvents.music_path, sizeof(m_Music) - 1);
    }
    if (g_SoundEvents.fade_out) {
        m_Fade = (float)g_SoundEvents.fade_seconds;
    }
}

// Returns the number of ticks until the ending requests the result screen.
static int32_t M_TicksToEnd(const int32_t limit)
{
    for (int32_t i = 1; i <= limit; i++) {
        M_Tick();
        if (m_Requested == SCENE_GAME_RESULT) {
            return i;
        }
    }
    return -1;
}

static void M_CheckScript(
    const int32_t character, const int32_t shot_type, const int32_t difficulty,
    const int32_t continues, const char *const path)
{
    M_Setup("pick");
    g_GameManager.run.character = (uint8_t)character;
    g_GameManager.run.shot_type = (uint8_t)shot_type;
    g_GameManager.run.shot_index = (uint8_t)(character * 2 + shot_type);
    g_GameManager.run.difficulty = difficulty;
    g_GameManager.stats.continues_used = (uint8_t)continues;
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 1);
    CHECK_EQ_STR(m_Music, path);
    CHECK(g_GameManager.mode.game_cleared);
    M_Teardown();
}

TEST(the_shot_picks_the_good_ending_and_a_continue_the_bad_one)
{
    M_CheckScript(0, 0, 1, 0, "data/end00.wav");
    M_CheckScript(0, 1, 3, 0, "data/end01.wav");
    M_CheckScript(1, 0, 2, 0, "data/end10.wav");
    M_CheckScript(1, 1, 1, 0, "data/end11.wav");
    M_CheckScript(2, 0, 0, 0, "data/end20.wav");
    M_CheckScript(2, 1, 3, 0, "data/end21.wav");
    M_CheckScript(0, 1, 1, 1, "data/end00b.wav");
    M_CheckScript(1, 0, 3, 2, "data/end10b.wav");
    M_CheckScript(2, 1, 0, 1, "data/end20b.wav");
}

TEST(the_clear_records_c_with_and_without_continues)
{
    M_Setup("clear");
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 1;
    g_GameManager.run.shot_index = 3;
    g_GameManager.run.difficulty = 2;
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK(g_AsciiText.hide_frame_rate);
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[2], 'c');
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[2], 'c');
    Chain_Release(&g_Chain);
    CHECK(!g_AsciiText.hide_frame_rate);
    Chain_Init(&g_Chain);
    g_GameManager.stats.continues_used = 1;
    g_GameManager.run.difficulty = 3;
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[3], 1);
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[3], 'c');
    M_Teardown();
}

static void M_Run(const char *const text, const size_t size)
{
    M_WriteScript("data/end00.end", text, size);
}

#define M_RUN(text_) M_Run(text_, sizeof(text_) - 1)

TEST(a_line_wait_ends_early_with_skip_after_a_clear)
{
    M_Setup("wait");
    M_RUN(
        "@w10\0"
        "0\0\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(20), 12);
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    m_Requested = SCENE_ENDING;
    Chain_InitElem(&m_InputElem, M_InputUpdate, nullptr);
    CHECK(
        Result_Log(Chain_AddCalc(&g_Chain, &m_InputElem, 0), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    memset(&m_Input, 0, sizeof(m_Input));
    m_Input.skip = true;
    CHECK_EQ_INT(M_TicksToEnd(20), 1);
    M_Teardown();
}

TEST(skip_runs_nothing_more_before_a_clear)
{
    M_Setup("no_skip");
    M_RUN(
        "@w10\0"
        "0\0\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    memset(&m_Input, 0, sizeof(m_Input));
    m_Input.skip = true;
    CHECK_EQ_INT(M_TicksToEnd(20), 12);
    M_Teardown();
}

TEST(the_minimum_frames_hold_a_wait_against_confirm)
{
    M_Setup("minimum");
    M_RUN(
        "@w10\0"
        "3\0\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    M_Tick();
    memset(&m_Input, 0, sizeof(m_Input));
    m_Input.shoot = true;
    M_Tick();
    memset(&m_Input, 0, sizeof(m_Input));
    M_Tick();
    CHECK(m_Requested != SCENE_GAME_RESULT);
    M_Tick();
    memset(&m_Input, 0, sizeof(m_Input));
    m_Input.shoot = true;
    M_Tick();
    CHECK(m_Requested != SCENE_GAME_RESULT);
    M_Tick();
    CHECK_EQ_INT(m_Requested, SCENE_GAME_RESULT);
    M_Teardown();
}

TEST(a_loaded_script_clears_the_running_scripts)
{
    M_Setup("load");
    M_RUN("@Fdata/next.end\0\r\n");
    M_SCRIPT("data/next.end", "@z\r\n@msecond.mid\0\r\n@z\r\n");
    Files_Shutdown();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 1);
    CHECK_EQ_STR(m_Music, "second.wav");
    M_Teardown();
}

TEST(music_fades_over_its_seconds)
{
    M_Setup("fade_music");
    M_RUN("@M3\0\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    M_TicksToEnd(5);
    CHECK(m_Fade == 3.0f);
    M_Teardown();
}

TEST(a_text_line_shows_on_the_line_sprites)
{
    M_Setup("text");
    M_RUN("Hello, world.\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK(m_Requested != SCENE_GAME_RESULT);
    CHECK_EQ_INT(M_TicksToEnd(20), 9);
    M_Teardown();
}

TEST(a_fade_out_holds_its_last_colour)
{
    M_Setup("fade");
    M_RUN(
        "@1"
        "2\0\r\n@r100\0"
        "100\0\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    uint8_t alphas[4];
    for (int32_t i = 0; i < 4; i++) {
        M_Tick();
        const int32_t count = GfxRecorder_QuadCount();
        alphas[i] = count > 0
            ? GfxRecorder_Quad(count - 1)->vertices[0].color.alpha
            : 0;
    }
    CHECK_EQ_INT(alphas[1], 0);
    CHECK_EQ_INT(alphas[2], 127);
    CHECK_EQ_INT(alphas[3], 255);
    M_Teardown();
}

TEST(a_script_without_its_end_command_ends_at_its_last_byte)
{
    M_Setup("no_end");
    M_RUN("@c255\0\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 1);
    M_Teardown();
}

TEST(an_unknown_command_letter_is_skipped)
{
    M_Setup("unknown");
    M_RUN("@q123\0\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 1);
    M_Teardown();
}

// Patched endings and jdiffs

static PATCH_STACK *M_Patch(const char *const diff)
{
    char *const patches = TestDir_Join(m_Directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    PatchBuilder_Write(patches, "a/lang/th07/end00.end.jdiff", diff);
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    Files_SetPatches(stack);
    return stack;
}

TEST(a_jdiff_patched_line_shows_as_translated)
{
    M_Setup("patched");
    M_RUN(
        "@s0\0"
        "0\0\r\n"
        "original\r\n@z\r\n");
    PATCH_STACK *const stack =
        M_Patch("{'0': {lines: ['A translated line.']}}");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 2);
    M_Teardown();
    Patch_CloseStack(stack);
}
