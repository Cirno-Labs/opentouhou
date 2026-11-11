#include "th06/screens/ending.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"
#include "th06/sound.h"

#include "core/base/memory.h"
#include "core/image.h"
#include "core/io/file.h"

#include <string.h>

#define M_KOUMA "\x8d\x67\x96\x82" // 紅魔 in CP932

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;

static char *m_Directory;
static SCENE m_Requested;
static TH06_INPUT m_Input;
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

TEST(a_line_splits_into_halves_of_32_bytes)
{
    const char text[] =
        "0123456789abcdef0123456789ABCDEFxy\r\n"
        "0123456789abcdef0123456789ABCDEF"
        "0123456789abcdef0123456789ABCDEFtail\r\n";
    ENDING_SCRIPT script = M_Script(text, sizeof(text) - 1);
    CHECK_EQ_STR(
        EndingScript_GetHalf(&script, 0), "0123456789abcdef0123456789ABCDEF");
    CHECK_EQ_STR(EndingScript_GetHalf(&script, 32), "xy");
    CHECK(EndingScript_GetHalf(&script, 1) == nullptr);
    CHECK_EQ_STR(EndingScript_GetHalf(&script, 36 + 64), "tail");
    CHECK_EQ_INT(EndingScript_HalfEnd(&script, 32), 34);
    EndingScript_Free(&script);
}

TEST(text_converts_from_cp932_and_a_cut_character_drops_its_half)
{
    const char text[] = M_KOUMA
        "\r\n"
        "0123456789abcdef0123456789ABCDE" M_KOUMA "\r\n";
    ENDING_SCRIPT script = M_Script(text, sizeof(text) - 1);
    CHECK_EQ_STR(EndingScript_GetHalf(&script, 0), "紅魔");
    CHECK_EQ_STR(EndingScript_GetHalf(&script, 6), "");
    EndingScript_Free(&script);
}

TEST(a_command_line_has_no_half)
{
    const char text[] = "@c255\0\r\nAB@xx\r\n";
    ENDING_SCRIPT script = M_Script(text, sizeof(text) - 1);
    CHECK(EndingScript_GetHalf(&script, 0) == nullptr);
    CHECK_EQ_STR(EndingScript_GetHalf(&script, 8), "AB");
    CHECK_EQ_INT(EndingScript_HalfEnd(&script, 8), 10);
    EndingScript_Free(&script);
}

static CHAIN_STATUS M_InputUpdate(void *const arg)
{
    Buttons_Update(&g_Buttons, &m_Input);
    return CHAIN_CONTINUE;
}

static void M_WriteScript(
    const char *const name, const char *const text, const size_t size)
{
    Th06Data_WriteFile(m_Directory, name, text, size);
}

#define M_SCRIPT(name_, text_) M_WriteScript(name_, text_, sizeof(text_) - 1)

static void M_WriteData(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const end = TestDir_Join(m_Directory, "data/end");
    CHECK(Result_Log(File_MakeDir(end), LOG_LEVEL_ERROR));
    Mem_Free(end);
    const char *const anms[] = {
        "data/staff01.anm",
        "data/staff02.anm",
        "data/staff03.anm",
    };
    for (size_t i = 0; i < 3; i++) {
        ANM_BUILDER builder;
        AnmBuilder_Init(&builder, 32, 32);
        builder.blank_texture = true;
        AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 16.0f, 16.0f);
        const uint32_t sprite = 0;
        const int32_t script = AnmBuilder_BeginScript(&builder, 0);
        AnmBuilder_Instr(&builder, script, 0, 1, &sprite, 1);
        AnmBuilder_Instr(&builder, script, 10000, 15, nullptr, 0);
        uint8_t bytes[ANM_BUILDER_MAX_SIZE];
        const size_t size = AnmBuilder_Build(&builder, bytes);
        Th06Data_WriteFile(m_Directory, anms[i], bytes, size);
    }
    IMAGE image = Image_Create(640, 960, &g_HeapAllocator);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    Th06Data_WriteFile(m_Directory, "data/end/back.jpg", bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
    const char *const names[] = {
        "data/end00.end", "data/end01.end",  "data/end10.end",
        "data/end11.end", "data/end00b.end", "data/end10b.end",
    };
    for (size_t i = 0; i < 6; i++) {
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
    for (int32_t i = 0; i < 4; i++) {
        for (int32_t j = 0; j < 5; j++) {
            g_GameManager.records.clears[i].without_continues[j] = 1;
            g_GameManager.records.clears[i].with_continues[j] = 1;
        }
    }
    g_GameManager.run.difficulty = 1;
    g_Buttons = (BUTTONS) {};
    m_Input = (TH06_INPUT) {};
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
        m_Fade = g_SoundEvents.fade_seconds;
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
    const int32_t character, const int32_t shot, const int32_t difficulty,
    const int32_t continues, const char *const path)
{
    M_Setup("pick");
    g_GameManager.run.character = (uint8_t)character;
    g_GameManager.run.shot_type = (uint8_t)shot;
    g_GameManager.run.difficulty = difficulty;
    g_GameManager.stats.continues_used = (uint8_t)continues;
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 1);
    CHECK_EQ_STR(m_Music, path);
    CHECK(g_GameManager.mode.game_cleared);
    M_Teardown();
}

TEST(the_shot_picks_the_good_ending_and_easy_or_a_continue_the_bad_one)
{
    M_CheckScript(0, 0, 1, 0, "data/end00.end");
    M_CheckScript(0, 1, 3, 0, "data/end01.end");
    M_CheckScript(1, 0, 2, 0, "data/end10.end");
    M_CheckScript(1, 1, 1, 0, "data/end11.end");
    M_CheckScript(0, 1, 0, 0, "data/end00b.end");
    M_CheckScript(1, 0, 3, 2, "data/end10b.end");
}

TEST(the_clear_records_99_with_and_without_continues)
{
    M_Setup("clear");
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 1;
    g_GameManager.run.difficulty = 2;
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.records.clears[3].without_continues[2], 99);
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[2], 99);
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    g_GameManager.stats.continues_used = 1;
    g_GameManager.run.difficulty = 3;
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.records.clears[3].without_continues[3], 1);
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[3], 99);
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
    m_Input = (TH06_INPUT) { .skip = true };
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
    m_Input = (TH06_INPUT) { .skip = true };
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
    m_Input = (TH06_INPUT) { .shoot = true };
    M_Tick();
    m_Input = (TH06_INPUT) {};
    M_Tick();
    CHECK(m_Requested != SCENE_GAME_RESULT);
    M_Tick();
    m_Input = (TH06_INPUT) { .shoot = true };
    M_Tick();
    CHECK(m_Requested != SCENE_GAME_RESULT);
    M_Tick();
    CHECK_EQ_INT(m_Requested, SCENE_GAME_RESULT);
    M_Teardown();
}

TEST(a_loaded_script_skips_its_first_line)
{
    M_Setup("load");
    M_RUN("@Fdata/next.end\0\r\n");
    M_SCRIPT("data/next.end", "@z\r\n@msecond.mid\0\r\n@z\r\n");
    Files_Shutdown();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 1);
    CHECK_EQ_STR(m_Music, "second.mid");
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
    M_RUN("0123456789abcdef0123456789ABCDEFxy\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK(m_Requested != SCENE_GAME_RESULT);
    CHECK_EQ_INT(M_TicksToEnd(20), 9);
    M_Teardown();
}

TEST(a_full_second_half_takes_another_step_for_its_line_break)
{
    M_Setup("full");
    M_RUN(
        "@s0\0"
        "0\0\r\n"
        "0123456789abcdef0123456789ABCDEF"
        "0123456789abcdef0123456789ABCDEF\r\n@z\r\n");
    CHECK(Result_Log(Ending_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_TicksToEnd(5), 3);
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
