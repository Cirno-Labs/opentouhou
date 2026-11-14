#include "th06/screens/result.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/anm/private.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/replay.h"
#include "th06/scene.h"
#include "th06/screens/result/private.h"
#include "th06/sound.h"
#include "th06/translation.h"

#include "core/base/memory.h"
#include "core/io/file.h"
#include "core/image.h"
#include "core/patch.h"

#include <string.h>

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;

static char *m_Directory;
static PATCH_STACK *m_Patches;
static SCENE m_Requested;
static TH06_INPUT m_Input;
static CHAIN_ELEM m_InputElem;
static int32_t m_Sounds[SOUND_COUNT];

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
}

SCENE Scene_Get(void)
{
    return SCENE_GAME_RESULT;
}

SCENE Scene_GetRequested(void)
{
    return m_Requested;
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

bool Gui_IsMsgSkippable(void)
{
    return false;
}

static CHAIN_STATUS M_Input(void *const arg)
{
    Buttons_Update(&g_Buttons, &m_Input);
    return CHAIN_CONTINUE;
}

// Writes an ANM file whose scripts each show its one sprite on their
// second frame, also after any interrupt, and then wait.
static void M_WriteAnm(const char *const name, const int32_t scripts)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 32, 32);
    builder.blank_texture = true;
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    const uint32_t sprite = 0;
    const uint32_t any = (uint32_t)-1;
    for (int32_t i = 0; i < scripts; i++) {
        const int32_t script = AnmBuilder_BeginScript(&builder, i);
        AnmBuilder_Instr(&builder, script, 0, 22, &any, 1);
        AnmBuilder_Instr(&builder, script, 1, 1, &sprite, 1);
        AnmBuilder_Instr(&builder, script, 10000, 15, nullptr, 0);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, name, bytes, size);
}

static void M_WriteData(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data/result");
    CHECK(Result_Log(File_MakeDir(data), LOG_LEVEL_ERROR));
    Mem_Free(data);
    M_WriteAnm("data/result00.anm", 8);
    M_WriteAnm("data/result01.anm", 7);
    M_WriteAnm("data/result02.anm", 22);
    M_WriteAnm("data/result03.anm", 1);
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    Th06Data_WriteFile(m_Directory, "data/result/result.jpg", bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
}

static void M_Start(void)
{
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    ResultScreen_OpenSaves(nullptr);
    Rng_SetSeed(&g_Rng, 0x1357);
    g_Buttons = (BUTTONS) {};
    m_Input = (TH06_INPUT) {};
    m_Requested = SCENE_GAME_RESULT;
    memset(m_Sounds, 0, sizeof(m_Sounds));
    Chain_InitElem(&m_InputElem, M_Input, nullptr);
    CHECK(
        Result_Log(Chain_AddCalc(&g_Chain, &m_InputElem, 0), LOG_LEVEL_ERROR));
}

static void M_Setup(const char *const case_name)
{
    M_WriteData(case_name);
    M_Start();
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Replay_Discard();
    AsciiText_Initialize();
    ResultScreen_CloseSaves();
    Anm_Shutdown();
    GfxRecorder_Stop();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

static void M_CountSounds(void)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        const int32_t sound = g_SoundEvents.queue[i];
        if (sound >= 0 && sound < SOUND_COUNT) {
            m_Sounds[sound]++;
        }
    }
    Sound_Reset();
}

// Runs one tick: the draw chain, then the calc chain, as Game_Tick does.
static void M_Tick(void)
{
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    AsciiText_Initialize();
    Chain_RunCalc(&g_Chain);
    M_CountSounds();
}

static void M_Run(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Tick();
    }
}

// Holds input down for one tick, then releases it for one tick.
static void M_Press(const TH06_INPUT input)
{
    m_Input = input;
    M_Tick();
    m_Input = (TH06_INPUT) {};
    M_Tick();
}

static RNG M_After(const int32_t draws)
{
    RNG rng = g_Rng;
    for (int32_t i = 0; i < draws; i++) {
        Rng_U16InRange(&rng, 256);
    }
    return rng;
}

static void M_LoadSaved(
    SCORE_LIST *const list, const int32_t shot, const int32_t difficulty)
{
    *list = (SCORE_LIST) {};
    ScoreFile_LoadHighScores(
        ResultSaves_GetScoreFile(), list, shot, difficulty);
}

static void M_StartGame(const uint32_t score)
{
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 0;
    g_GameManager.run.difficulty = 2;
    g_GameManager.run.stage = 4;
    g_GameManager.score.value = score;
    g_GameManager.score.shown = score;
    const SCORE_FILE empty = {};
    ScoreFile_LoadClears(&empty, g_GameManager.records.clears);
    ScoreFile_LoadPractice(&empty, g_GameManager.records.practice);
}

// Waits through the statistics and leaves from the replay prompt.
static void M_LeaveAfterStatistics(void)
{
    M_Run(95);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(110);
    M_Press((TH06_INPUT) { .bomb = true });
    M_Run(61);
}

TEST(practice_records_a_better_score_and_leaves_at_once)
{
    M_Setup("practice");
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 0;
    g_GameManager.run.stage = 3;
    g_GameManager.run.difficulty = 1;
    g_GameManager.score.value = 777;
    const SCORE_FILE empty = {};
    ScoreFile_LoadPractice(&empty, g_GameManager.records.practice);
    g_GameManager.records.practice[2][2][1].score = 500;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    CHECK(Anm_GetScript(0x100) == nullptr);
    CHECK_EQ_INT(g_GameManager.records.practice[2][2][1].score, 777);
    const RNG expected = M_After(2);
    M_Tick();
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    PRACTICE_RECORD practice[4][6][4];
    ScoreFile_LoadPractice(ResultSaves_GetScoreFile(), practice);
    CHECK_EQ_INT(practice[2][2][1].score, 777);
    M_Teardown();
}

TEST(practice_keeps_a_better_stored_score)
{
    M_Setup("practice_worse");
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.stage = 1;
    g_GameManager.score.value = 400;
    g_GameManager.records.practice[0][0][0].score = 500;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.records.practice[0][0][0].score, 500);
    M_Teardown();
}

TEST(the_browser_loads_the_records_and_a_game_keeps_its_own)
{
    M_Setup("records");
    g_GameManager.records.clears[2].with_continues[3] = 7;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.records.clears[2].with_continues[3], 7);
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    ResultScreen_OpenSaves(nullptr);
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.records.clears[2].with_continues[3], 1);
    M_Teardown();
}

TEST(name_entry_saves_the_named_score)
{
    M_Setup("name");
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Press((TH06_INPUT) { .right = true });
    M_Press((TH06_INPUT) { .shoot = true });
    M_Press((TH06_INPUT) { .menu = true });
    M_LeaveAfterStatistics();
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    SCORE_LIST list;
    M_LoadSaved(&list, 2, 2);
    CHECK_EQ_INT(list.count, 1);
    CHECK_EQ_INT(list.entries[0].score, 5000000);
    CHECK_EQ_INT(list.entries[0].stage, 4);
    CHECK(memcmp(list.entries[0].name, "AB      ", 9) == 0);
    M_Teardown();
}

TEST(a_cleared_game_stores_stage_99)
{
    M_Setup("cleared");
    M_StartGame(5000000);
    g_GameManager.mode.game_cleared = true;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_Press((TH06_INPUT) { .menu = true });
    M_LeaveAfterStatistics();
    SCORE_LIST list;
    M_LoadSaved(&list, 2, 2);
    CHECK_EQ_INT(list.entries[0].stage, 99);
    M_Teardown();
}

// Erasing writes a space where the cursor was before it moves back, so
// the last character typed stays.
TEST(erasing_keeps_the_character_before_the_cursor)
{
    M_Setup("erase");
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Press((TH06_INPUT) { .shoot = true });
    M_Press((TH06_INPUT) { .bomb = true });
    M_Press((TH06_INPUT) { .right = true });
    M_Press((TH06_INPUT) { .shoot = true });
    M_Press((TH06_INPUT) { .menu = true });
    M_LeaveAfterStatistics();
    SCORE_LIST list;
    M_LoadSaved(&list, 2, 2);
    CHECK(memcmp(list.entries[0].name, "AB      ", 9) == 0);
    M_Teardown();
}

TEST(a_ninth_character_replaces_the_eighth)
{
    M_Setup("ninth");
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    for (int32_t i = 0; i < 8; i++) {
        M_Press((TH06_INPUT) { .shoot = true });
    }
    M_Press((TH06_INPUT) { .up = true });
    M_Press((TH06_INPUT) { .shoot = true });
    M_Press((TH06_INPUT) { .bomb = true });
    M_Press((TH06_INPUT) { .menu = true });
    M_LeaveAfterStatistics();
    SCORE_LIST list;
    M_LoadSaved(&list, 2, 2);
    CHECK(memcmp(list.entries[0].name, "AAAAAAA ", 9) == 0);
    M_Teardown();
}

TEST(a_score_below_the_tenth_skips_the_name)
{
    M_Setup("low");
    M_StartGame(1);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_Press((TH06_INPUT) { .shoot = true });
    M_LeaveAfterStatistics();
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    SCORE_LIST list;
    M_LoadSaved(&list, 2, 2);
    CHECK_EQ_INT(list.count, 0);
    M_Teardown();
}

TEST(a_continued_game_cannot_save_its_replay)
{
    M_Setup("continued");
    M_StartGame(5000000);
    g_GameManager.stats.continues_used = 1;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_Press((TH06_INPUT) { .menu = true });
    M_Run(95);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(110);
    CHECK(m_Requested != SCENE_TITLE);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK(!ResultSaves_GetSlot(0)->valid);
    M_Teardown();
}

TEST(a_refused_game_cannot_save_its_replay)
{
    M_Setup("refused");
    M_StartGame(5000000);
    ResultScreen_RefuseReplays(true);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_Press((TH06_INPUT) { .menu = true });
    M_Run(95);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(110);
    CHECK(m_Requested != SCENE_TITLE);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK(!ResultSaves_GetSlot(0)->valid);
    ResultScreen_RefuseReplays(false);
    M_Teardown();
}

TEST(saving_a_replay_writes_the_slot_and_draws_six_values)
{
    M_Setup("replay");
    M_StartGame(5000000);
    g_GameManager.run.stage = 1;
    g_GameManager.player.lives = 5;
    CHECK(Result_Log(Replay_Register(REPLAY_RECORD, nullptr), LOG_LEVEL_ERROR));
    ResultScreen_SetDate("12/24/26");
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Press((TH06_INPUT) { .menu = true });
    M_Run(95);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(110);
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(21);
    M_Press((TH06_INPUT) { .down = true });
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(31);
    M_Press((TH06_INPUT) { .up = true });
    M_Press((TH06_INPUT) { .left = true });
    RNG expected = g_Rng;
    Rng_U16InRange(&expected, 128);
    for (int32_t i = 0; i < 5; i++) {
        Rng_U16InRange(&expected, 256);
    }
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    const SAVED_REPLAY *const slot = ResultSaves_GetSlot(1);
    CHECK(slot->valid);
    CHECK(memcmp(slot->name, "A       ", 9) == 0);
    CHECK_EQ_STR(slot->summary.date, "12/24/26");
    CHECK_EQ_INT(slot->summary.shot, 2);
    CHECK_EQ_INT(slot->summary.score, 5000000);
    g_GameManager.player.lives = 0;
    g_GameManager.mode.in_replay = true;
    CHECK(
        Result_Log(Replay_Register(REPLAY_PLAY, slot->path), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.player.lives, 5);
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    Replay_Discard();
    M_Teardown();
}

TEST(the_unlock_sequence_marks_every_game_cleared)
{
    M_Setup("unlock");
    M_StartGame(0);
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    for (int32_t i = 0; i < 3; i++) {
        M_Press((TH06_INPUT) { .down = true });
    }
    M_Press((TH06_INPUT) { .shoot = true });
    const TH06_INPUT keys[] = {
        { .focus = true, .unlock_start = true },
        { .focus = true },
        { .focus = true, .unlock_start = true },
        { .focus = true },
        { .focus = true, .unlock_start = true },
        { .focus = true },
        { .focus = true, .unlock_start = true },
        { .focus = true },
        { .focus = true, .unlock_start = true },
        { .focus = true, .quit = true },
        { .focus = true },
        { .focus = true, .quit = true },
        { .focus = true, .unlock_end = true },
        { .focus = true },
        { .focus = true, .unlock_end = true },
        { .focus = true },
        { .focus = true, .unlock_end = true },
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        m_Input = keys[i];
        M_Tick();
    }
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[4], 1);
    m_Input = (TH06_INPUT) { .focus = true };
    M_Tick();
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[4], 99);
    CHECK_EQ_INT(g_GameManager.records.clears[0].without_continues[0], 99);
    CHECK_EQ_INT(m_Sounds[SOUND_EXTEND], 1);
    M_Teardown();
}

TEST(a_wrong_key_restarts_the_unlock_sequence)
{
    M_Setup("unlock_wrong");
    M_StartGame(0);
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    for (int32_t i = 0; i < 3; i++) {
        M_Press((TH06_INPUT) { .down = true });
    }
    M_Press((TH06_INPUT) { .shoot = true });
    for (int32_t i = 0; i < 11; i++) {
        m_Input = (TH06_INPUT) {
            .focus = true,
            .unlock_start = i < 5,
            .quit = i == 5 || i == 7,
            .menu = i == 6,
            .unlock_end = i >= 8,
        };
        M_Tick();
        m_Input = (TH06_INPUT) { .focus = true };
        M_Tick();
    }
    M_Tick();
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[4], 1);
    M_Teardown();
}

TEST(the_browser_leaves_from_its_last_entry)
{
    M_Setup("browser");
    M_StartGame(0);
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    M_Press((TH06_INPUT) { .bomb = true });
    M_Press((TH06_INPUT) { .shoot = true });
    CHECK_EQ_INT(m_Sounds[SOUND_CANCEL], 2);
    M_Run(55);
    CHECK(m_Requested != SCENE_TITLE);
    const RNG expected = M_After(2);
    M_Tick();
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_Teardown();
}

// Every calc steps all 38 sprites, so each shows on its second frame.
TEST(the_screen_steps_and_draws_every_sprite)
{
    M_Setup("draw");
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Tick();
    M_Tick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 39);
    M_Teardown();
}

TEST(the_unlock_sequence_needs_focus_or_skip_held)
{
    M_Setup("unlock_held");
    M_StartGame(0);
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    for (int32_t i = 0; i < 3; i++) {
        M_Press((TH06_INPUT) { .down = true });
    }
    M_Press((TH06_INPUT) { .shoot = true });
    for (int32_t i = 0; i < 10; i++) {
        M_Press((TH06_INPUT) {
            .unlock_start = i < 5,
            .quit = i == 5 || i == 6,
            .unlock_end = i >= 7,
        });
    }
    M_Tick();
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[4], 1);
    M_Teardown();
}

// Layout with nmlgc/base_tsa

// Sets up as M_Setup does, with text.anm holding the 16 text rows of the
// screen, 256 x 16 each, and the text font. With base_tsa, puts
// nmlgc/base_tsa in the patch stack of the translations.
static void M_SetupText(const char *const case_name, const bool base_tsa)
{
    M_WriteData(case_name);
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 256, 256);
    for (int32_t i = 0; i < 16; i++) {
        AnmBuilder_AddSprite(&builder, 8 + i, 0.0f, 16.0f * i, 256.0f, 16.0f);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, "data/text.anm", bytes, size);
    M_Start();
    CHECK(Result_Log(Anm_Load(0, "data/text.anm", 0x700), LOG_LEVEL_ERROR));
    CHECK(Result_Log(
        Anm_CreateTexture(0, 256, 256, ANM_TEXTURE_RGBA8), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Anm_InitText(nullptr, nullptr, 0), LOG_LEVEL_ERROR));
    if (base_tsa) {
        char *const patches = TestDir_Join(m_Directory, "patches");
        PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
        CHECK(Result_Log(
            Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, &m_Patches),
            LOG_LEVEL_ERROR));
        Mem_Free(patches);
        Translation_Init(m_Patches);
        CHECK(Translation_HasBaseTsa());
    }
}

static void M_TeardownText(void)
{
    M_Teardown();
    Anm_ShutdownText();
    Translation_Shutdown();
    Patch_CloseStack(m_Patches);
    m_Patches = nullptr;
}

// Returns the leftmost column of text row row that text drew into, or -1.
static int32_t M_LeftmostDrawn(const int32_t row)
{
    const IMAGE *const image = AnmTexture_GetImage(0);
    int32_t leftmost = -1;
    for (int32_t y = row * 16; y < row * 16 + 16; y++) {
        for (int32_t x = 0; x < image->width; x++) {
            const size_t at = ((size_t)y * (size_t)image->width + (size_t)x);
            if (image->pixels[at * 4 + 3] != 0
                && (leftmost < 0 || x < leftmost)) {
                leftmost = x;
            }
        }
    }
    return leftmost;
}

TEST(the_shot_names_are_centered_in_their_rows)
{
    M_SetupText("shot_names_centered", false);
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK(M_LeftmostDrawn(0) > 32);
    CHECK(M_LeftmostDrawn(1) > 32);
    M_TeardownText();
}

TEST(base_tsa_puts_the_shot_names_at_the_left_of_their_rows)
{
    M_SetupText("shot_names_left", true);
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK(M_LeftmostDrawn(0) >= 0 && M_LeftmostDrawn(0) < 4);
    CHECK(M_LeftmostDrawn(1) >= 0 && M_LeftmostDrawn(1) < 4);
    M_TeardownText();
}

// Opens the spell card page of the browser, four entries below Normal,
// draws it once, and returns how far right of its number the capture rate
// of the first card is drawn.
static float M_CaptureOffset(void)
{
    M_StartGame(0);
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    for (int32_t i = 0; i < 4; i++) {
        M_Press((TH06_INPUT) { .down = true });
    }
    M_Press((TH06_INPUT) { .shoot = true });
    M_Run(25);
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    float number = -1.0f;
    float capture = -1.0f;
    for (int32_t i = 0; i < g_AsciiText.string_count; i++) {
        const ASCII_STRING *const string = &g_AsciiText.strings[i];
        if (strcmp(string->text, "No.01") == 0) {
            number = string->pos.x;
        } else if (
            number >= 0.0f && capture < 0.0f
            && strcmp(string->text, "  0/  0") == 0) {
            capture = string->pos.x;
        }
    }
    CHECK(number >= 0.0f && capture >= 0.0f);
    return capture - number;
}

TEST(the_capture_rates_are_368_pixels_right_of_the_numbers)
{
    M_SetupText("capture_retail", false);
    CHECK(M_CaptureOffset() == 368.0f);
    M_TeardownText();
}

TEST(base_tsa_moves_the_capture_rates_472_pixels_right)
{
    M_SetupText("capture_base_tsa", true);
    CHECK(M_CaptureOffset() == 472.0f);
    M_TeardownText();
}
