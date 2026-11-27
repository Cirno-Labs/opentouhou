#include "tests/gfx_recorder.h"
#include "tests/patch_builder.h"
#include "tests/test.h"
#include "tests/test_dir.h"
#include "tests/th07/th07_anm_builder.h"
#include "tests/th07/th07_data.h"
#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/buttons.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/scene.h"
#include "th07/screens/music.h"
#include "th07/sound.h"
#include "th07/strings.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/image.h"
#include "core/io/file.h"
#include "core/io/reader.h"
#include "core/io/writer.h"
#include "stg/translation.h"

#include <string.h>

#define M_SPRITE_OP 3
#define M_COLOR_OP 9
#define M_STOP_OP 20
#define M_LABEL_OP 21

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static MUSIC_TUNE m_Tunes[MUSIC_TUNE_COUNT];
static char *m_Directory;
static SCENE m_Requested;
static TH07_INPUT m_Input;
static CHAIN_ELEM m_InputElem;

RNG g_Rng;
float g_GameSpeed = 1.0f;

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
    Reader_OpenBuffer(&reader, text, strlen(text), nullptr);
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
        "th07_01.mid\r\nTitle\r\n1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n8\r\n");
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_STR(m_Tunes[0].path, "bgm/th07_01.mid");
    CHECK_EQ_STR(m_Tunes[0].title, "Title");
    CHECK_EQ_STR(m_Tunes[0].comment[0], "");
    CHECK_EQ_STR(m_Tunes[0].comment[1], "1");
    CHECK_EQ_STR(m_Tunes[0].comment[7], "7");
}

TEST(an_at_sign_starts_the_next_tune)
{
    const int32_t count = M_Parse("junk\n@a.mid\nA\n\nx\n@b.mid\nB\n\ny\nz\n");
    CHECK_EQ_INT(count, 2);
    CHECK_EQ_STR(m_Tunes[0].comment[1], "x");
    CHECK_EQ_STR(m_Tunes[0].comment[2], "");
    CHECK_EQ_STR(m_Tunes[1].path, "b.mid");
    CHECK_EQ_STR(m_Tunes[1].comment[1], "y");
    CHECK_EQ_STR(m_Tunes[1].comment[2], "z");
}

TEST(text_that_is_utf8_loads_as_utf8)
{
    const int32_t count =
        M_Parse("@a.mid\nTitle \xe2\x99\xaa\n\nComment \xe2\x99\xaa\n");
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_STR(m_Tunes[0].title, "Title \xe2\x99\xaa");
    CHECK_EQ_STR(m_Tunes[0].comment[1], "Comment \xe2\x99\xaa");
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
}

static CHAIN_STATUS M_InputUpdate(void *const arg)
{
    Buttons_Update(&m_Input, false);
    return CHAIN_CONTINUE;
}

// Writes a script for id that shows sprite 0, white at interrupt 1 and
// gray at interrupt 2.
static void M_AddTitleScript(ANM_BUILDER_ANM *const anm, const int32_t id)
{
    const int32_t s = AnmBuilder_BeginScript(anm, id);
    const uint32_t sprite = 0;
    const uint32_t white = COLOR_WHITE;
    const uint32_t gray = COLOR_ARGB(0xe0, 0x80, 0x80, 0x80);
    const uint32_t selected = 1;
    const uint32_t unselected = 2;
    AnmBuilder_Instr(anm, s, M_SPRITE_OP, 0, 0, &sprite, 1);
    AnmBuilder_Instr(anm, s, M_STOP_OP, 0, 0, nullptr, 0);
    AnmBuilder_Instr(anm, s, M_LABEL_OP, 0, 0, &selected, 1);
    AnmBuilder_Instr(anm, s, M_COLOR_OP, 0, 0, &white, 1);
    AnmBuilder_Instr(anm, s, M_STOP_OP, 0, 0, nullptr, 0);
    AnmBuilder_Instr(anm, s, M_LABEL_OP, 0, 0, &unselected, 1);
    AnmBuilder_Instr(anm, s, M_COLOR_OP, 0, 0, &gray, 1);
    AnmBuilder_Instr(anm, s, M_STOP_OP, 0, 0, nullptr, 0);
    AnmBuilder_End(anm, s);
}

// Writes a script for id that shows sprite 0 and never changes.
static void M_AddPlainScript(ANM_BUILDER_ANM *const anm, const int32_t id)
{
    const int32_t s = AnmBuilder_BeginScript(anm, id);
    const uint32_t sprite = 0;
    AnmBuilder_Instr(anm, s, M_SPRITE_OP, 0, 0, &sprite, 1);
    AnmBuilder_Instr(anm, s, M_STOP_OP, 0, 0, nullptr, 0);
    AnmBuilder_End(anm, s);
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

static void M_WriteData(const char *const case_name, const int32_t tunes)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    const char *const dirs[] = { "data", "data/result" };
    for (size_t i = 0; i < 2; i++) {
        char *const dir = TestDir_Join(m_Directory, dirs[i]);
        CHECK(Result_Log(File_MakeDir(dir), LOG_LEVEL_ERROR));
        Mem_Free(dir);
    }
    M_WriteImage("data/result/music.jpg");

    static ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *anm = AnmBuilder_Add(&builder, 16, 16, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    M_AddPlainScript(anm, 0);
    for (int32_t i = 0; i < MUSIC_TUNE_COUNT; i++) {
        M_AddTitleScript(anm, i + 1);
    }
    AnmBuilder_WriteFile(&builder, m_Directory, "data/music00.anm");

    AnmBuilder_Init(&builder);
    anm = AnmBuilder_Add(&builder, 16, 16, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    for (int32_t i = 7; i < 7 + MUSIC_COMMENT_ROWS; i++) {
        M_AddPlainScript(anm, i);
    }
    AnmBuilder_WriteFile(&builder, m_Directory, "data/text.anm");

    char *text = Str_Dup("");
    for (int32_t i = 0; i < tunes; i++) {
        char *const next = Str_Format(
            "%s@bgm/th07_%.2d.mid\r\nTune %d\r\n\r\nComment\r\n", text, i, i);
        Mem_Free(text);
        text = next;
    }
    Th07Data_WriteFile(m_Directory, "data/musiccmt.txt", text, strlen(text));
    Mem_Free(text);
}

static void M_Setup(const char *const case_name, const int32_t tunes)
{
    M_WriteData(case_name, tunes);
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init();
    CHECK(Result_Log(Anm_Load(0, "data/text.anm", 0x700), LOG_LEVEL_ERROR));
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    memset(&m_Input, 0, sizeof(m_Input));
    m_Requested = SCENE_MUSIC_ROOM;
    Sound_Reset();
    Chain_InitElem(&m_InputElem, M_InputUpdate, nullptr);
    CHECK(Result_Log(Chain_AddCalc(&m_InputElem, 0), LOG_LEVEL_ERROR));
    CHECK(Result_Log(MusicScreen_Register(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release();
    AsciiText_Initialize();
    Anm_Shutdown();
    GfxRecorder_Stop();
    Th07Data_Unmount();
    Sound_Reset();
    Mem_Free(m_Directory);
}

// Runs one tick and keeps its sound events until the next tick.
static void M_Tick(const TH07_INPUT input)
{
    Sound_Reset();
    m_Input = input;
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw();
    Gfx_EndTick();
    AsciiText_Initialize();
    Chain_RunCalc();
}

static void M_Run(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Tick((TH07_INPUT) {});
    }
}

static void M_Press(const TH07_INPUT input)
{
    M_Tick(input);
    M_Tick((TH07_INPUT) {});
}

static const TH07_INPUT m_Shoot = { .shoot = true };

static bool M_Plays(const char *const path)
{
    return g_SoundEvents.music == SOUND_MUSIC_PLAY
        && strcmp(g_SoundEvents.music_path, path) == 0;
}

TEST(the_list_takes_input_from_the_ninth_calc)
{
    M_Setup("intro", 3);
    M_Run(8);
    M_Tick(m_Shoot);
    CHECK(g_SoundEvents.music == SOUND_MUSIC_KEEP);
    M_Tick((TH07_INPUT) {});
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th07_00.wav"));
    M_Teardown();
}

TEST(up_from_the_first_tune_plays_the_last)
{
    M_Setup("wrap", 12);
    M_Run(9);
    M_Press((TH07_INPUT) { .up = true });
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th07_11.wav"));
    M_Teardown();
}

TEST(down_and_up_count_presses_only)
{
    M_Setup("presses", 12);
    M_Run(9);
    for (int32_t i = 0; i < 40; i++) {
        M_Tick((TH07_INPUT) { .down = true });
    }
    M_Tick((TH07_INPUT) {});
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th07_01.wav"));
    M_Teardown();
}

TEST(the_list_scrolls_ten_rows_and_wraps_down)
{
    M_Setup("scroll", 12);
    M_Run(9);
    for (int32_t i = 0; i < 11; i++) {
        M_Press((TH07_INPUT) { .down = true });
    }
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th07_11.wav"));
    M_Tick((TH07_INPUT) {});
    M_Press((TH07_INPUT) { .down = true });
    M_Tick(m_Shoot);
    CHECK(M_Plays("bgm/th07_00.wav"));
    M_Teardown();
}

TEST(cancel_returns_to_the_title_after_a_confirm_in_the_same_calc)
{
    M_Setup("cancel", 3);
    M_Run(9);
    M_Tick((TH07_INPUT) { .shoot = true, .bomb = true });
    CHECK(M_Plays("bgm/th07_00.wav"));
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK(g_Chain.calc.next == &m_InputElem && m_InputElem.next == nullptr);
    M_Teardown();
}

TEST(the_screen_draws_every_quad_once)
{
    M_Setup("draw", 12);
    M_Run(9);
    M_Press(m_Shoot);
    M_Tick((TH07_INPUT) {});
    CHECK(GfxRecorder_QuadCount() >= 2);
    M_Teardown();
}

static void M_CheckTranslatedCommentRow(const bool base_tsa)
{
    M_Setup(base_tsa ? "comment_base_tsa" : "comment_plain", 3);
    char *const patches = TestDir_Join(m_Directory, "patches");
    if (base_tsa) {
        PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    }
    PatchBuilder_AddPatch(patches, "thpatch/lang", "{}");
    PatchBuilder_Write(
        patches, "thpatch/lang/th07/musiccmt.js",
        "{'1': ['Translated comment']}");
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack),
        LOG_LEVEL_ERROR));
    Translation_Init(stack, &g_TranslationGame);
    CHECK(Translation_HasBaseTsa() == base_tsa);
    M_Run(9);
    M_Press(m_Shoot);
    int32_t comments = 0;
    for (int32_t i = 0; i < GfxRecorder_QuadCount(); i++) {
        const uint64_t id = GfxRecorder_QuadId(i);
        const uint32_t part = (uint32_t)id;
        if (id != 0 && part >= 0x200 && part < 0x208) {
            CHECK_EQ_INT(part, base_tsa ? 0x201 : 0x200);
            comments++;
        }
    }
    CHECK_EQ_INT(comments, 1);
    M_Teardown();
    Translation_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(patches);
}

TEST(base_tsa_starts_translated_comments_after_the_empty_source_row)
{
    M_CheckTranslatedCommentRow(true);
}

TEST(translated_comments_without_base_tsa_start_on_the_first_row)
{
    M_CheckTranslatedCommentRow(false);
}
