#include "th01/score.h"

#include "core/base/arena.h"
#include "test.h"

#include <string.h>

typedef struct {
    ARENA arena;
    SCORE_ASSETS assets;
    SCORE_SCENE scene;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Frame_Reset();
    GameInput_Init();
    Arena_Init(&f->arena, "score", 2 * 1024 * 1024);
    Video_Init(Arena_Allocator(&f->arena));
    f->assets.gameover.pixels = Arena_Alloc(&f->arena, VIDEO_PAGE_SIZE);
    memset(f->assets.gameover.pixels, 1, VIDEO_PAGE_SIZE);
    for (int32_t i = 0; i < 16; i++) {
        f->assets.gameover.palette[i] = (GFX_COLOR) { 255, 255, 255, 255 };
    }
    VFS *const user_files = Vfs_Create(&g_HeapAllocator);
    SCORE_SAVES loaded;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &loaded)));
    Vfs_Destroy(user_files);
    memset(&g_ScoreSaves, 0, sizeof(g_ScoreSaves));
    memcpy(g_ScoreSaves.tables, loaded.tables, sizeof(loaded.tables));
    f->arena.capacity = f->arena.used;
}

static void M_Begin(
    FIXTURE *const f, const SCORE_CONTEXT context, const int32_t score)
{
    const SCORE_ENTRY entry = { .score = score, .stage = 13, .route = "魔" };
    Score_Begin(&f->scene, &f->assets, context, 1, &entry);
}

static bool M_Tick(FIXTURE *const f, const TH01_INPUT raw)
{
    return Score_Tick(&f->scene, &raw, true);
}

TEST(gameover_fades_before_input_and_preserves_shared_counters)
{
    FIXTURE f;
    M_Init(&f);
    g_VSync.count1 = 100;
    g_VSync.count2 = -20;
    M_Begin(&f, SCORE_GAMEOVER, 9999);
    CHECK_EQ_INT(f.scene.phase, SCORE_FADE_WAIT);
    CHECK_EQ_INT(g_VSync.count1, 100);
    CHECK(!M_Tick(&f, (TH01_INPUT) { .shot = true }));
    CHECK_EQ_INT(g_Video.palette[0].red, 17);
    for (int32_t i = 0; i < 15; i++) {
        M_Tick(&f, (TH01_INPUT) {});
    }
    CHECK_EQ_INT(f.scene.phase, SCORE_ENTRY_INPUT);
    CHECK_EQ_INT(g_VSync.count1, 116);
    CHECK_EQ_INT(g_VSync.count2, -4);
    CHECK_EQ_INT(g_VSync.edge, 16);
    CHECK_EQ_INT(f.scene.place, 0);
    CHECK_EQ_STR(g_ScoreSaves.tables[1].entries[0].route, "魔");
    CHECK_EQ_INT(g_ScoreSaves.tables[1].entries[0].stage, 13);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 639], 1);
    CHECK_EQ_INT(g_Video.palette[7].red, 255);
    CHECK(!g_ScoreSaves.pending[1]);
    CHECK(M_Tick(&f, (TH01_INPUT) { .ok = true }));
    CHECK(g_ScoreSaves.pending[1]);
    CHECK_EQ_STR(g_ScoreSaves.tables[1].entries[0].name, "                ");
    CHECK_EQ_INT(g_VSync.count1, 117);
    CHECK(!g_GameInput.ok);
    Arena_Free(&f.arena);
}

TEST(name_slots_edit_and_gameover_last_slot_jumps_to_enter)
{
    FIXTURE f;
    M_Init(&f);
    M_Begin(&f, SCORE_ENDING, 9000);
    for (int32_t i = 0; i < 8; i++) {
        f.scene.column = i;
        g_GameInput.shot = true;
        CHECK(!Score_OnInput(&f.scene));
    }
    CHECK_EQ_INT(f.scene.cursor, 7);
    CHECK_EQ_INT(f.scene.row, 0);
    CHECK_EQ_STR(f.scene.slots[0], "ａ");
    CHECK_EQ_STR(f.scene.slots[7], "ｈ");
    f.scene.context = SCORE_GAMEOVER;
    g_GameInput.shot = true;
    CHECK(!Score_OnInput(&f.scene));
    CHECK_EQ_INT(f.scene.row, 5);
    CHECK_EQ_INT(f.scene.column, 17);
    g_GameInput.strike = true;
    CHECK(!Score_OnInput(&f.scene));
    CHECK_EQ_INT(f.scene.cursor, 6);
    f.scene.column = 10;
    g_GameInput.shot = true;
    CHECK(!Score_OnInput(&f.scene));
    CHECK_EQ_STR(f.scene.slots[6], "　");
    f.scene.column = 15;
    g_GameInput.shot = true;
    CHECK(!Score_OnInput(&f.scene));
    CHECK_EQ_INT(f.scene.cursor, 6);
    f.scene.column = 16;
    g_GameInput.shot = true;
    CHECK(!Score_OnInput(&f.scene));
    CHECK_EQ_INT(f.scene.cursor, 7);
    f.scene.column = 17;
    CHECK(M_Tick(&f, (TH01_INPUT) { .ok = true }));
    CHECK_EQ_STR(g_ScoreSaves.tables[1].entries[0].name, "ａｂｃｄｅｆ　ｈ");
    Arena_Free(&f.arena);
}

TEST(grid_navigation_wraps_and_skips_every_hole)
{
    FIXTURE f;
    M_Init(&f);
    M_Begin(&f, SCORE_ENDING, 9000);
    for (int32_t row = 0; row < 6; row++) {
        for (int32_t column = 0; column < 18; column++) {
            if (((row == 1 || row == 3) && column >= 8)
                || (row == 5 && column > 10 && column < 15)) {
                continue;
            }
            for (int32_t direction = 0; direction < 4; direction++) {
                f.scene.row = row;
                f.scene.column = column;
                GameInput_Init();
                g_GameInput.up = direction == 0;
                g_GameInput.down = direction == 1;
                g_GameInput.horizontal = direction == 2 ? GAME_INPUT_LEFT
                    : direction == 3                    ? GAME_INPUT_RIGHT
                                                        : 0;
                CHECK(!Score_OnInput(&f.scene));
                CHECK(f.scene.row >= 0 && f.scene.row < 6);
                CHECK(f.scene.column >= 0 && f.scene.column < 18);
                CHECK(
                    !((f.scene.row == 1 || f.scene.row == 3)
                      && f.scene.column >= 8));
                CHECK(
                    !(f.scene.row == 5 && f.scene.column > 10
                      && f.scene.column < 15));
                CHECK(
                    !g_GameInput.up && !g_GameInput.down
                    && g_GameInput.horizontal == 0);
            }
        }
    }
    f.scene.row = 0;
    f.scene.column = 17;
    GameInput_Init();
    g_GameInput.down = true;
    g_GameInput.shot = true;
    g_GameInput.strike = true;
    Score_OnInput(&f.scene);
    CHECK_EQ_INT(f.scene.row, 2);
    CHECK(g_GameInput.shot && g_GameInput.strike);
    GameInput_Init();
    g_GameInput.horizontal = GAME_INPUT_BOTH;
    Score_OnInput(&f.scene);
    CHECK_EQ_INT(g_GameInput.horizontal, GAME_INPUT_BOTH);
    Arena_Free(&f.arena);
}

TEST(results_input_wait_resets_at_entry_and_preserves_elapsed_count)
{
    FIXTURE f;
    M_Init(&f);
    M_Begin(&f, SCORE_ENDING, 9000);
    CHECK(!M_Tick(&f, (TH01_INPUT) { .shot = true }));
    CHECK_EQ_INT(f.scene.cursor, 1);
    CHECK_EQ_INT(f.scene.phase, SCORE_ENTRY_DELAY);
    for (int32_t i = 0; i < 3; i++) {
        CHECK(!M_Tick(&f, (TH01_INPUT) { .shot = true }));
        CHECK_EQ_INT(f.scene.phase, SCORE_ENTRY_DELAY);
    }
    CHECK(!M_Tick(&f, (TH01_INPUT) { .shot = true }));
    CHECK_EQ_INT(f.scene.phase, SCORE_ENTRY_INPUT);
    CHECK_EQ_INT(g_VSync.count1, 4);
    CHECK_EQ_INT(f.scene.inputs, 1);
    CHECK_EQ_INT(f.scene.cursor, 1);
    CHECK(!M_Tick(&f, (TH01_INPUT) {}));
    CHECK(!M_Tick(&f, (TH01_INPUT) { .shot = true }));
    CHECK_EQ_INT(f.scene.cursor, 2);
    Arena_Free(&f.arena);
}

TEST(nonqualifying_score_waits_for_a_sensed_press_and_release)
{
    FIXTURE f;
    M_Init(&f);
    M_Begin(&f, SCORE_ENDING, -1);
    CHECK_EQ_INT(f.scene.place, SCORE_PLACES);
    for (int32_t i = 0; i < 5; i++) {
        CHECK(!M_Tick(&f, (TH01_INPUT) {}));
    }
    CHECK(!M_Tick(&f, (TH01_INPUT) { .shot = true }));
    CHECK(M_Tick(&f, (TH01_INPUT) {}));
    CHECK(!g_ScoreSaves.pending[1]);
    M_Begin(&f, SCORE_ENDING, -1);
    CHECK(!M_Tick(&f, (TH01_INPUT) { .ok = true }));
    CHECK(M_Tick(&f, (TH01_INPUT) {}));
    Arena_Free(&f.arena);
}

TEST(results_retain_background_palette_and_have_no_input_timeout)
{
    FIXTURE f;
    M_Init(&f);
    memset(g_Video.pages[0], 4, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    Video_SetDac(7, 2, 4, 6);
    M_Begin(&f, SCORE_ENDING, 9000);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 639], 4);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 639], 6);
    CHECK_EQ_INT(g_Video.pages[1][0], 0);
    CHECK_EQ_INT(g_Video.palette[7].red, 34);
    CHECK_EQ_INT(g_Video.palette[7].green, 68);
    f.scene.inputs = 1001;
    CHECK(!M_Tick(&f, (TH01_INPUT) {}));
    CHECK_EQ_INT(f.scene.phase, SCORE_ENTRY_INPUT);
    f.scene.context = SCORE_GAMEOVER;
    CHECK(M_Tick(&f, (TH01_INPUT) {}));
    CHECK(g_ScoreSaves.pending[1]);
    Arena_Free(&f.arena);
}

TEST(results_ignore_keypad_and_never_generate_bomb_taps)
{
    FIXTURE f;
    M_Init(&f);
    M_Begin(&f, SCORE_ENDING, 9000);
    CHECK(!M_Tick(
        &f, (TH01_INPUT) { .keypad_down = true, .keypad_right = true }));
    CHECK_EQ_INT(f.scene.row, 0);
    CHECK_EQ_INT(f.scene.column, 0);
    CHECK_EQ_INT(f.scene.phase, SCORE_ENTRY_INPUT);
    CHECK(!M_Tick(&f, (TH01_INPUT) { .down = true }));
    CHECK_EQ_INT(f.scene.row, 1);
    for (int32_t i = 0; i < 5; i++) {
        GameInput_SenseEnding(&(TH01_INPUT) {});
        GameInput_SenseEnding(&(TH01_INPUT) { .shot = true, .strike = true });
    }
    CHECK(!g_GameInput.bomb);
    CHECK_EQ_INT(g_GameInput.shot_presses, 0);
    CHECK_EQ_INT(g_GameInput.strike_presses, 0);
    Arena_Free(&f.arena);
}
