// SPDX-License-Identifier: GPL-3.0-only

#include "th06/screens/music.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/scene.h"
#include "th06/sound.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/image.h"
#include "core/io/file.h"

#include <string.h>

#define M_KOUMA "\x8d\x67\x96\x82" // 紅魔 in CP932

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;

static MUSIC_TUNE m_Tunes[MUSIC_TUNE_COUNT];
static char *m_Directory;
static SCENE m_Requested;
static TH06_INPUT m_Input;
static CHAIN_ELEM m_InputElem;

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
}

SCENE Scene_Get(void)
{
    return SCENE_MUSIC_ROOM;
}

SCENE Scene_GetRequested(void)
{
    return m_Requested;
}

static int32_t M_Parse(const char *const text)
{
    READER reader;
    Reader_OpenBuffer(&reader, text, strlen(text));
    int32_t count = -1;
    memset(m_Tunes, 0xcc, sizeof(m_Tunes));
    CHECK(Result_Log(
        MusicScreen_ReadComments(&reader, m_Tunes, &count), LOG_LEVEL_ERROR));
    return count;
}

TEST(the_first_comment_row_is_the_break_after_the_title)
{
    const int32_t count = M_Parse(
        "@bgm/"
        "th06_01.mid\r\nTitle\r\n1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n8\r\n");
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_STR(m_Tunes[0].path, "bgm/th06_01.mid");
    CHECK_EQ_STR(m_Tunes[0].title, "Title");
    CHECK_EQ_STR(m_Tunes[0].comment[0][0], "");
    CHECK_EQ_STR(m_Tunes[0].comment[1][0], "1");
    CHECK_EQ_STR(m_Tunes[0].comment[7][0], "7");
}

TEST(an_at_sign_starts_the_next_tune)
{
    const int32_t count = M_Parse("junk\n@a.mid\nA\n\nx\n@b.mid\nB\n\ny\nz\n");
    CHECK_EQ_INT(count, 2);
    CHECK_EQ_STR(m_Tunes[0].comment[1][0], "x");
    CHECK_EQ_STR(m_Tunes[0].comment[2][0], "");
    CHECK_EQ_STR(m_Tunes[1].path, "b.mid");
    CHECK_EQ_STR(m_Tunes[1].comment[1][0], "y");
    CHECK_EQ_STR(m_Tunes[1].comment[2][0], "z");
}

TEST(a_row_shows_its_second_half_past_32_bytes)
{
    const int32_t count = M_Parse(
        "@a.mid\nA\n\n"
        "0123456789abcdef0123456789abcdef\n"
        "0123456789abcdef0123456789abcdefXY\n");
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_STR(m_Tunes[0].comment[1][0], "0123456789abcdef0123456789abcdef");
    CHECK_EQ_STR(m_Tunes[0].comment[1][1], "");
    CHECK_EQ_STR(m_Tunes[0].comment[2][0], "0123456789abcdef0123456789abcdef");
    CHECK_EQ_STR(m_Tunes[0].comment[2][1], "XY");
}

TEST(text_converts_from_cp932_by_halves)
{
    const int32_t count = M_Parse(
        "@a.mid\n" M_KOUMA
        "\n\n"
        "0123456789abcdef0123456789abcd" M_KOUMA
        "\n"
        "0123456789abcdef0123456789abc" M_KOUMA "\n");
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_STR(m_Tunes[0].title, "紅魔");
    CHECK_EQ_STR(m_Tunes[0].comment[1][0], "0123456789abcdef0123456789abcd紅");
    CHECK_EQ_STR(m_Tunes[0].comment[1][1], "魔");
    CHECK_EQ_STR(m_Tunes[0].comment[2][0], "");
    CHECK_EQ_STR(m_Tunes[0].comment[2][1], "");
}

TEST(text_that_is_utf8_loads_as_utf8)
{
    const int32_t count = M_Parse("@a.mid\nTitle ♪\n\nComment ♪\n");
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_STR(m_Tunes[0].title, "Title ♪");
    CHECK_EQ_STR(m_Tunes[0].comment[1][0], "Comment ♪");
}

TEST(a_tune_cut_by_the_end_of_the_data_counts)
{
    CHECK_EQ_INT(M_Parse("@a.mid\nTit"), 1);
    CHECK_EQ_STR(m_Tunes[0].title, "Tit");
    CHECK_EQ_INT(M_Parse("no tunes\n"), 0);
}

TEST(the_list_holds_32_tunes_and_cuts_long_fields)
{
    char *text = Str_Dup("");
    for (int32_t i = 0; i < 33; i++) {
        char *const next = Str_Format("%s@%d.mid\nT\n\nx\n", text, i);
        Mem_Free(text);
        text = next;
    }
    CHECK_EQ_INT(M_Parse(text), 32);
    CHECK_EQ_STR(m_Tunes[31].path, "31.mid");
    Mem_Free(text);
    char long_title[64];
    memset(long_title, 'T', sizeof(long_title) - 1);
    long_title[63] = 0;
    char *const tune = Str_Format("@a.mid\n%s\n", long_title);
    CHECK_EQ_INT(M_Parse(tune), 1);
    CHECK_EQ_INT(strlen(m_Tunes[0].title), MUSIC_TITLE_BYTES - 1);
    Mem_Free(tune);
}

static CHAIN_STATUS M_InputUpdate(void *const arg)
{
    Buttons_Update(&g_Buttons, &m_Input);
    return CHAIN_CONTINUE;
}

static void M_WriteData(const char *const case_name, const int32_t tunes)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const result = TestDir_Join(m_Directory, "data/result");
    CHECK(Result_Log(File_MakeDir(result), LOG_LEVEL_ERROR));
    Mem_Free(result);
    const char *const anms[] = {
        "data/music00.anm",
        "data/music01.anm",
        "data/music02.anm",
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
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    Th06Data_WriteFile(m_Directory, "data/result/music.jpg", bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
    char *text = Str_Dup("");
    for (int32_t i = 0; i < tunes; i++) {
        char *const next = Str_Format(
            "%s@bgm/th06_%.2d.mid\r\nTune %d\r\n\r\nComment\r\n", text, i, i);
        Mem_Free(text);
        text = next;
    }
    Th06Data_WriteFile(m_Directory, "data/musiccmt.txt", text, strlen(text));
    Mem_Free(text);
}

static void M_Setup(const char *const case_name, const int32_t tunes)
{
    M_WriteData(case_name, tunes);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    g_Buttons = (BUTTONS) {};
    m_Input = (TH06_INPUT) {};
    m_Requested = SCENE_MUSIC_ROOM;
    Sound_Reset();
    Chain_InitElem(&m_InputElem, M_InputUpdate, nullptr);
    CHECK(
        Result_Log(Chain_AddCalc(&g_Chain, &m_InputElem, 0), LOG_LEVEL_ERROR));
    CHECK(Result_Log(MusicScreen_Register(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    AsciiText_Initialize();
    Anm_Shutdown();
    GfxRecorder_Stop();
    Files_Shutdown();
    Sound_Reset();
    Mem_Free(m_Directory);
}

// Runs one tick and keeps its sound events until the next tick.
static void M_Tick(const TH06_INPUT input)
{
    Sound_Reset();
    m_Input = input;
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    AsciiText_Initialize();
    Chain_RunCalc(&g_Chain);
}

static void M_Run(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Tick((TH06_INPUT) {});
    }
}

static void M_Press(const TH06_INPUT input)
{
    M_Tick(input);
    M_Tick((TH06_INPUT) {});
}

static const TH06_INPUT m_Shoot = { .shoot = true };

static bool M_Plays(const char *const path)
{
    return g_SoundEvents.music == SOUND_MUSIC_PLAY
        && strcmp(g_SoundEvents.music_path, path) == 0;
}

TEST(the_list_takes_input_from_the_tenth_calc)
{
    M_Setup("intro", 3);
    M_Run(8);
    M_Tick(m_Shoot);
    CHECK(g_SoundEvents.music == SOUND_MUSIC_KEEP);
    M_Tick((TH06_INPUT) {});
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th06_00.mid"));
    M_Teardown();
}

TEST(up_from_the_first_tune_plays_the_last)
{
    M_Setup("wrap", 12);
    M_Run(9);
    M_Press((TH06_INPUT) { .up = true });
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th06_11.mid"));
    M_Teardown();
}

TEST(down_and_up_count_presses_only)
{
    M_Setup("presses", 12);
    M_Run(9);
    for (int32_t i = 0; i < 40; i++) {
        M_Tick((TH06_INPUT) { .down = true });
    }
    M_Tick((TH06_INPUT) {});
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th06_01.mid"));
    M_Teardown();
}

TEST(the_list_scrolls_ten_rows_and_wraps_down)
{
    M_Setup("scroll", 12);
    M_Run(9);
    for (int32_t i = 0; i < 11; i++) {
        M_Press((TH06_INPUT) { .down = true });
    }
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th06_11.mid"));
    M_Tick((TH06_INPUT) {});
    M_Press((TH06_INPUT) { .down = true });
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th06_00.mid"));
    M_Teardown();
}

TEST(cancel_returns_to_the_title_after_a_confirm_in_the_same_calc)
{
    M_Setup("cancel", 3);
    M_Run(9);
    M_Tick((TH06_INPUT) { .shoot = true, .bomb = true });
    CHECK(M_Plays("bgm/th06_00.mid"));
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK(g_Chain.calc.next == &m_InputElem && m_InputElem.next == nullptr);
    M_Teardown();
}

TEST(the_screen_draws_every_quad_once)
{
    M_Setup("draw", 12);
    M_Run(9);
    M_Press(m_Shoot);
    M_Tick((TH06_INPUT) {});
    CHECK(GfxRecorder_QuadCount() >= 2);
    M_Teardown();
}
