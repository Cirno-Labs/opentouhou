#include "th07/screens/result/screen.h"

#include "gfx_recorder.h"
#include "stg/replay_files.h"

#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/buttons.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/play_time.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screens/result/private.h"
#include "th07/screens/result/records.h"
#include "th07/settings.h"
#include "th07/sound.h"

#include "core/base/memory.h"
#include "core/image.h"
#include "core/io/file.h"

#include <string.h>

RNG g_Rng;
float g_GameSpeed = 1.0f;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;
static SCENE m_Requested;
static TH07_INPUT m_Input;
static CHAIN_ELEM m_InputElem;
static int32_t m_RunTimeAdds;
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

void PlayTime_AddRun(void)
{
    m_RunTimeAdds++;
}

void Player_StartBorder(void)
{
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

bool Gui_IsMsgSkippable(void)
{
    return false;
}

bool Enemy_IsBossActive(void)
{
    return false;
}

static CHAIN_STATUS M_Input(void *const arg)
{
    Buttons_Update(&m_Input, false);
    return CHAIN_CONTINUE;
}

// Writes result00.anm: each of the screen's sprites shows its sprite and
// waits, from the start and after any interrupt.
static void M_WriteAnm(void)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 32, 32, "@");
    for (int32_t i = 0; i <= 0x10; i++) {
        AnmBuilder_AddSprite(anm, i, 0.0f, 0.0f, 16.0f, 16.0f);
    }
    const uint32_t any = (uint32_t)-1;
    const uint32_t sprite = 0;
    for (int32_t i = 0; i < 0x29; i++) {
        const int32_t s = AnmBuilder_BeginScript(anm, i);
        AnmBuilder_Instr(anm, s, 21, 0, 0, &any, 1);
        AnmBuilder_Instr(anm, s, 3, 0, 0, &sprite, 1);
        AnmBuilder_Instr(anm, s, 20, 0, 0, nullptr, 0);
        AnmBuilder_End(anm, s);
    }
    AnmBuilder_WriteFile(&builder, m_Directory, "data/result00.anm");
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
    const char *const dirs[] = { "data", "data/result" };
    for (int32_t i = 0; i < 2; i++) {
        char *const dir = TestDir_Join(m_Directory, dirs[i]);
        CHECK(Result_Log(File_MakeDir(dir), LOG_LEVEL_ERROR));
        Mem_Free(dir);
    }
    M_WriteAnm();
    M_WriteImage("data/result/result.jpg");
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init();
    GameManager_Create(&g_GameManager);
    ResultScreen_OpenSaves();
    ResultScreen_LoadPlayStats();
    ResultScreen_SetDate("12/24");
    ResultScreen_ResetFrames();
    Rng_SetSeed(&g_Rng, 0x1357);
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    memset(&m_Input, 0, sizeof(m_Input));
    memset(m_Sounds, 0, sizeof(m_Sounds));
    m_Requested = SCENE_GAME_RESULT;
    m_RunTimeAdds = 0;
    Chain_InitElem(&m_InputElem, M_Input, nullptr);
    CHECK(Result_Log(Chain_AddCalc(&m_InputElem, 0), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release();
    Replay_Discard();
    AsciiText_Initialize();
    ResultScreen_CloseSaves();
    ReplayFiles_Clear();
    Anm_Shutdown();
    GfxRecorder_Stop();
    Th07Data_Unmount();
    Mem_Free(m_Directory);
}

static void M_Tick(void)
{
    Sound_Reset();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw();
    Gfx_EndTick();
    AsciiText_Initialize();
    Chain_RunCalc();
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        const int32_t sound = g_SoundEvents.queue[i];
        if (sound >= 0 && sound < SOUND_COUNT) {
            m_Sounds[sound]++;
        }
    }
}

static void M_Run(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Tick();
    }
}

// Holds input down for one tick, then releases it for one tick.
static void M_Press(const TH07_INPUT input)
{
    m_Input = input;
    M_Tick();
    memset(&m_Input, 0, sizeof(m_Input));
    M_Tick();
}

static void M_PressTimes(const TH07_INPUT input, const int32_t times)
{
    for (int32_t i = 0; i < times; i++) {
        M_Press(input);
    }
}

static RNG M_After(const int32_t draws_128, const int32_t draws_256)
{
    RNG rng = g_Rng;
    for (int32_t i = 0; i < draws_128; i++) {
        Rng_U16InRange(&rng, 128);
    }
    for (int32_t i = 0; i < draws_256; i++) {
        Rng_U16InRange(&rng, 256);
    }
    return rng;
}

static void M_LoadSaved(SCORE_LIST *const list, const int32_t shot)
{
    memset(list, 0, sizeof(*list));
    ScoreFile_LoadHighScores(
        ResultSaves_GetScoreFile(), list, shot, g_GameManager.run.difficulty);
}

static void M_StartGame(const uint32_t score)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->run.character = 1;
    gm->run.shot_type = 0;
    gm->run.shot_index = 2;
    gm->run.difficulty = 2;
    gm->run.stage = 4;
    gm->run.start_lives = (uint8_t)g_Settings.lives;
    gm->score.value = score;
    gm->score.shown = score;
    const SCORE_FILE empty = {};
    ScoreFile_LoadClears(&empty, gm->records.clears);
    ScoreFile_LoadPractice(&empty, gm->records.practice);
}

// Waits through the statistics to the replay prompt.
static void M_PassStatistics(void)
{
    M_Run(95);
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(110);
}

// Moves from the first glyph to the end mark: up, then left.
static void M_GoToEndMark(void)
{
    M_Press((TH07_INPUT) { .up = true });
    M_Press((TH07_INPUT) { .left = true });
}

// Names the score "A" and ends the name.
static void M_EnterName(void)
{
    M_Run(31);
    M_Press((TH07_INPUT) { .shoot = true });
    M_GoToEndMark();
    M_Press((TH07_INPUT) { .shoot = true });
}

TEST(the_browser_leaves_from_its_last_item_and_saves_the_score)
{
    M_Setup("leave");
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    M_Press((TH07_INPUT) { .bomb = true });
    CHECK_EQ_INT(m_Requested, SCENE_GAME_RESULT);
    M_Press((TH07_INPUT) { .bomb = true });
    M_Run(58);
    CHECK_EQ_INT(m_Requested, SCENE_GAME_RESULT);
    const RNG expected = M_After(0, 2);
    M_Tick();
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK(memcmp(&g_Rng, &expected, sizeof(expected)) == 0);
    CHECK(ResultSaves_GetScoreFile()->data != nullptr);
    M_Teardown();
}

// From item 1, five downs reach the play statistics while Phantasm is
// closed, which the cursor skips, and the spell cards while it is open.
TEST(the_menu_skips_phantasm_while_it_is_closed)
{
    M_Setup("phantasm");
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    M_PressTimes((TH07_INPUT) { .down = true }, 5);
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(2);
    CHECK_EQ_INT(m_RunTimeAdds, 2);
    M_Run(100);
    M_Press((TH07_INPUT) { .shoot = true });
    CHECK_EQ_INT(m_RunTimeAdds, 3);
    M_Run(45);
    M_Press((TH07_INPUT) { .bomb = true });
    M_Press((TH07_INPUT) { .bomb = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK_EQ_INT(m_RunTimeAdds, 3);
    M_Teardown();
}

// Holds focus through the sequence: three ups, two cheats, two downs and
// three quits. The calc after it marks every stage cleared.
TEST(the_unlock_sequence_on_hard_marks_every_stage_cleared)
{
    M_Setup("unlock");
    CHECK(Result_Log(ResultScreen_Register(0), LOG_LEVEL_ERROR));
    M_Run(21);
    M_Press((TH07_INPUT) { .down = true });
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(31);
    const TH07_INPUT steps[] = {
        { .focus = true, .up = true },    { .focus = true, .up = true },
        { .focus = true, .up = true },    { .focus = true, .cheat = true },
        { .focus = true, .cheat = true }, { .focus = true, .down = true },
        { .focus = true, .down = true },  { .focus = true, .quit = true },
        { .focus = true, .quit = true },  { .focus = true, .quit = true },
    };
    const TH07_INPUT focus = { .focus = true };
    for (int32_t i = 0; i < 10; i++) {
        CHECK(g_GameManager.records.clears[3].no_continues[5] != 99);
        m_Input = steps[i];
        M_Tick();
        m_Input = focus;
        M_Tick();
    }
    for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
        for (int32_t d = 0; d < SCORE_DIFFICULTY_COUNT; d++) {
            CHECK_EQ_INT(
                g_GameManager.records.clears[shot].no_continues[d], 99);
            CHECK_EQ_INT(
                g_GameManager.records.clears[shot].with_continues[d], 99);
        }
    }
    CHECK_EQ_INT(m_Sounds[SOUND_EXTEND], 1);
    M_Teardown();
}

TEST(name_entry_saves_the_named_score_and_the_last_name)
{
    M_Setup("name");
    M_StartGame(5000000);
    g_GameManager.stats.continues_used = 2;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_EnterName();
    M_PassStatistics();
    M_Press((TH07_INPUT) { .bomb = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    SCORE_LIST list;
    M_LoadSaved(&list, 2);
    CHECK_EQ_INT(list.count, 1);
    const HIGH_SCORE *const entry = &list.entries[0];
    CHECK_EQ_INT(entry->score, 5000000);
    CHECK_EQ_INT(entry->stage, 4);
    CHECK_EQ_INT(entry->continues_used, 2);
    CHECK(memcmp(entry->name, "A       ", 9) == 0);
    CHECK(memcmp(entry->date, "12/24", 6) == 0);
    LAST_NAME_RECORD last_name = {};
    CHECK(ScoreFile_LoadLastName(ResultSaves_GetScoreFile(), &last_name));
    CHECK(memcmp(last_name.name, "A       ", 9) == 0);
    M_Teardown();
}

TEST(erasing_blanks_the_character_before_the_cursor_and_at_it)
{
    M_Setup("erase");
    M_StartGame(5000000);
    g_GameManager.mode.game_cleared = true;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_PressTimes((TH07_INPUT) { .shoot = true }, 3);
    M_Press((TH07_INPUT) { .bomb = true });
    M_GoToEndMark();
    M_Press((TH07_INPUT) { .shoot = true });
    M_PassStatistics();
    M_Press((TH07_INPUT) { .bomb = true });
    M_Run(61);
    SCORE_LIST list;
    M_LoadSaved(&list, 2);
    CHECK_EQ_INT(list.entries[0].stage, 99);
    CHECK(memcmp(list.entries[0].name, "AA      ", 9) == 0);
    M_Teardown();
}

// The menu key also counts as the cancel key, which erases, before it ends
// the name.
TEST(the_menu_key_erases_the_last_character_and_ends_the_name)
{
    M_Setup("menu_key");
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(31);
    M_PressTimes((TH07_INPUT) { .shoot = true }, 2);
    M_Press((TH07_INPUT) { .menu = true });
    M_PassStatistics();
    M_Press((TH07_INPUT) { .bomb = true });
    M_Run(61);
    SCORE_LIST list;
    M_LoadSaved(&list, 2);
    CHECK(memcmp(list.entries[0].name, "A       ", 9) == 0);
    M_Teardown();
}

TEST(a_score_below_the_tenth_skips_the_name)
{
    M_Setup("low");
    M_StartGame(1);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(1);
    M_PassStatistics();
    M_Press((TH07_INPUT) { .bomb = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    SCORE_LIST list;
    M_LoadSaved(&list, 2);
    CHECK_EQ_INT(list.count, 0);
    M_Teardown();
}

TEST(a_replayed_game_records_no_score_and_restores_its_spell_cards)
{
    M_Setup("replayed");
    M_StartGame(5000000);
    g_GameManager.mode.in_replay = true;
    g_GameManager.records.spell_cards[3].attempts[6] = 7;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK_EQ_INT(g_GameManager.records.spell_cards[3].attempts[6], 0);
    M_PassStatistics();
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK(!ResultSaves_GetSlot(0)->valid);
    SCORE_LIST list;
    M_LoadSaved(&list, 2);
    CHECK_EQ_INT(list.count, 0);
    M_Teardown();
}

TEST(a_continued_game_cannot_save_its_replay)
{
    M_Setup("continued");
    M_StartGame(1);
    g_GameManager.stats.continues_used = 1;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Run(1);
    M_PassStatistics();
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(21);
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    CHECK(!ResultSaves_GetSlot(0)->valid);
    M_Teardown();
}

// Records 31 calcs of stage 1.
static void M_RecordStage(void)
{
    g_GameManager.run.stage = 1;
    CHECK(Result_Log(
        Replay_Register(REPLAY_RECORD, "replay/th7_00.rpy"), LOG_LEVEL_ERROR));
    g_GameManager.mode.playing = true;
    M_Run(31);
    g_GameManager.mode.playing = false;
}

// Chooses slot 2.
static void M_SaveToSlot2(void)
{
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(21);
    M_Press((TH07_INPUT) { .down = true });
    M_Press((TH07_INPUT) { .shoot = true });
}

static void M_EndReplayName(void)
{
    M_Run(31);
    M_GoToEndMark();
}

TEST(saving_a_replay_writes_a_slot_that_plays_back)
{
    M_Setup("replay");
    M_StartGame(5000000);
    g_GameManager.player.lives = 5;
    M_RecordStage();
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_EnterName();
    M_PassStatistics();
    M_SaveToSlot2();
    M_EndReplayName();
    const RNG expected = M_After(1, 2);
    M_Press((TH07_INPUT) { .shoot = true });
    CHECK(memcmp(&g_Rng, &expected, sizeof(expected)) == 0);
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    const SAVED_REPLAY *const slot = ResultSaves_GetSlot(1);
    CHECK(slot->valid);
    CHECK_EQ_STR(slot->summary.name, "A       ");
    CHECK_EQ_STR(slot->summary.date, "12/24");
    CHECK_EQ_INT(slot->summary.shot, 2);
    CHECK_EQ_INT(slot->summary.difficulty, 2);
    CHECK_EQ_INT(slot->summary.score, 5000000);
    CHECK(slot->summary.stages[1]);
    g_GameManager.player.lives = 0;
    g_GameManager.mode.in_replay = true;
    CHECK(
        Result_Log(Replay_Register(REPLAY_PLAY, slot->path), LOG_LEVEL_ERROR));
    CHECK_EQ_INT((int32_t)g_GameManager.player.lives, 5);
    M_Teardown();
}

TEST(a_used_slot_asks_before_it_is_overwritten)
{
    M_Setup("overwrite");
    M_StartGame(5000000);
    M_RecordStage();
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_EnterName();
    M_PassStatistics();
    M_SaveToSlot2();
    M_EndReplayName();
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(61);
    M_RecordStage();
    g_GameManager.score.value = 6000000;
    g_GameManager.score.shown = 6000000;
    m_Requested = SCENE_GAME_RESULT;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_EnterName();
    M_PassStatistics();
    M_SaveToSlot2();
    M_Run(21);
    M_Press((TH07_INPUT) { .right = true });
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(21);
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(21);
    M_Press((TH07_INPUT) { .shoot = true });
    // The name starts on the end mark once score.dat holds a last name.
    M_Run(31);
    M_Press((TH07_INPUT) { .shoot = true });
    M_Run(61);
    CHECK_EQ_INT(ResultSaves_GetSlot(1)->summary.score, 6000000);
    M_Teardown();
}

TEST(practice_records_a_better_score_and_opens_the_replay_prompt)
{
    M_Setup("practice");
    M_StartGame(5000000);
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.stage = 2;
    g_GameManager.run.difficulty = 1;
    g_GameManager.records.practice[2][1][1].score = 4000000;
    g_GameManager.records.practice[2][1][2].score = 10;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK_EQ_INT(g_GameManager.records.practice[2][1][1].score, 5000000);
    CHECK_EQ_INT(g_GameManager.records.practice[2][1][2].score, 10);
    M_Run(80);
    M_Press((TH07_INPUT) { .bomb = true });
    M_Run(61);
    CHECK_EQ_INT(m_Requested, SCENE_TITLE);
    M_Teardown();
}

TEST(practice_keeps_a_better_stored_score)
{
    M_Setup("practice_kept");
    M_StartGame(5000000);
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.stage = 2;
    g_GameManager.records.practice[2][1][2].score = 6000000;
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Tick();
    CHECK_EQ_INT(g_GameManager.records.practice[2][1][2].score, 6000000);
    M_Teardown();
}

// Every calc steps the 41 sprites, so each shows from its first frame, over
// the background.
TEST(the_screen_steps_and_draws_every_sprite)
{
    M_Setup("draw");
    M_StartGame(5000000);
    CHECK(Result_Log(ResultScreen_Register(1), LOG_LEVEL_ERROR));
    M_Tick();
    M_Tick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0x29 + 1);
    M_Teardown();
}
