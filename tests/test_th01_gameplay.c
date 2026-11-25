#include "../src/th01/scene.c"
#include "../src/th01/hud.c"
#include "../src/th01/miss.c"
#include "th01/gameplay_internal.h"
#include "th01/session.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "th01/score.h"
#include "th01_assets.h"
#include "../src/th01/music.c"
#include "../src/th01/sound.c"

#include "core/base/arena.h"
#include "core/audio.h"
#include "core/audio/backend.h"

#include "th01_world.h"
#include "test.h"

#include <SDL3/SDL.h>

typedef struct {
    ARENA arena;
    GAMEPLAY_ASSETS *assets;
} FIXTURE;

// Renders a while and reports whether an effect sounded.
static bool M_EffectSounds(void)
{
    MDRV_FRAME frames[2400];
    Mdrv_Render(m_Driver, frames, 2400);
    for (size_t i = 0; i < 2400; i++) {
        if (frames[i].effects[0] != 0 || frames[i].effects[1] != 0) {
            return true;
        }
    }
    return false;
}

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    Arena_Init(&f->arena, "gameplay", 8 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(a);
    Session_Init();
    g_Session.lives = 4;
    g_Session.bombs = 1;
    g_Session.settings.difficulty = 1;
    g_Session.settings.bombs = 1;
    g_Session.settings.lives_extra = 2;
    g_Session.settings.music = SETTINGS_MUSIC_FM;
    f->assets = Th01TestAssets_Create(a);
    Gameplay_Init(a, f->assets);
    CHECK(m_Hud.numbers != nullptr);
    CHECK(Result_Log(
        Music_Init(
            a, f->assets->songs, GAMEPLAY_SONG_COUNT, &f->assets->effects),
        LOG_LEVEL_ERROR));

    Gameplay_Reset();
    Frame_Reset();
    StageObjects_InitPrepared(
        &f->assets->scenes[0].layouts, 0, true, &f->assets->stage);
    Orb_ResetFlight();
    g_StageTimer.time = 1000;
    g_Gameplay.phase = GP_FRAME_BEGIN;
    g_Stage.timer_initialized = true;
    g_Stage.first_stage = false;
    g_Gameplay.invincibility.frame[0] = 255;
}

TEST(ordinary_loop_consumes_real_input_and_suspends_after_motion)
{
    FIXTURE f;
    M_Init(&f);

    const size_t used = f.arena.used;
    TH01_INPUT raw = { .right = true, .shot = true };
    CHECK_EQ_INT(Gameplay_Tick(&raw, false, 1), GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(g_Session.frame_rand, 1);
    CHECK_EQ_INT(g_Gameplay.frame, 1);
    CHECK_EQ_INT(g_Orb.force_frame, 1);
    CHECK_EQ_INT(g_Player.left, 308);
    CHECK_EQ_INT(g_Orb.left, 596);
    CHECK_EQ_INT(g_Gameplay.phase, GP_ORB);
    CHECK(StageOrb_Waiting());
    const PLAYER player = g_Player;
    const ORB orb = g_Orb;
    const uint32_t rng = g_Rng;
    Gameplay_Tick(&raw, false, 2);
    CHECK_EQ_INT(memcmp(&player, &g_Player, sizeof(player)), 0);
    CHECK_EQ_INT(memcmp(&orb, &g_Orb, sizeof(orb)), 0);
    CHECK_EQ_INT(g_Rng, rng);
    Gameplay_Tick(&raw, true, 3);
    CHECK_EQ_INT(g_Session.frame_rand, 2);
    CHECK_EQ_INT(g_Gameplay.frame, 2);
    CHECK_EQ_INT(g_Orb.force_frame, 2);
    Gameplay_Tick(&raw, true, 4);
    CHECK(g_Shots.slots[0].moving);
    CHECK(M_EffectSounds());
    CHECK_EQ_INT(f.arena.used, used);
    Arena_Free(&f.arena);
}

TEST(hurry_wait_freezes_the_remaining_iteration_prefix)
{
    FIXTURE f;
    M_Init(&f);

    g_Session.frame_rand = 3;
    g_StageTimer.time = 2;
    g_Player.invincibility_time = 2;
    TH01_INPUT raw = {};
    Gameplay_Tick(&raw, false, 1);
    CHECK_EQ_INT(g_Gameplay.phase, GP_TIMER);
    CHECK_EQ_INT(g_StageTimer.phase, TIMER_WAIT_HURRY);
    CHECK_EQ_INT(g_Gameplay.frame, 1);
    CHECK_EQ_INT(g_Player.invincibility_time, 1);
    CHECK_EQ_INT(g_Orb.left, 600);
    const uint32_t rng = g_Rng;
    const PLAYER player = g_Player;
    for (uint64_t i = 2; i < 12; i++) {
        Gameplay_Tick(&raw, false, i);
        CHECK_EQ_INT(g_Gameplay.frame, 1);
        CHECK_EQ_INT(g_Session.frame_rand, 4);
        CHECK_EQ_INT(g_Rng, rng);
        CHECK_EQ_INT(memcmp(&player, &g_Player, sizeof(player)), 0);
    }
    Arena_Free(&f.arena);
}

TEST(real_shot_boundary_advances_player_before_timer_and_rng_seed)
{
    FIXTURE f;
    M_Init(&f);

    g_Gameplay.phase = GP_WAIT_SHOT;
    g_Gameplay.wait_for_shot = true;
    g_Stage.timer_initialized = false;
    GameInput_Init();
    g_Session.frame_rand = 42;
    TH01_INPUT raw = { .shot = true, .left = true };
    Gameplay_Tick(&raw, false, 1);
    CHECK_EQ_INT(g_Gameplay.phase, GP_WAIT_SHOT_DELAY);
    CHECK_EQ_INT(g_Player.left, 300);
    CHECK(!g_Stage.timer_initialized);
    CHECK_EQ_INT(g_Session.frame_rand, 42);
    Gameplay_Tick(&raw, true, 2);
    CHECK(g_Stage.timer_initialized);
    CHECK(!g_Gameplay.wait_for_shot);
    CHECK_EQ_INT(g_Gameplay.frame, 1);
    CHECK_EQ_INT(g_Session.frame_rand, 43);
    CHECK_EQ_INT(g_Player.bomb_frame, 2);
    Arena_Free(&f.arena);
}

TEST(stage_clear_beats_miss_and_starts_real_bonus)
{
    FIXTURE f;
    M_Init(&f);

    BONUS_STAGE_ASSETS stage = {};
    f.assets->bonus = &stage;
    g_Gameplay.phase = GP_FRAME_BEGIN;
    g_Player.is_hit = true;
    g_Stage.cleared = true;
    g_Session.lives = 0;
    g_StageTimer.time = 500;
    TH01_INPUT raw = {};
    Gameplay_Tick(&raw, false, 1);
    CHECK_EQ_INT(g_Gameplay.phase, GP_LIFE_EXIT);
    Gameplay_Tick(&raw, true, 2);
    CHECK_EQ_INT(g_Gameplay.phase, GP_STAGE_BONUS);
    CHECK_EQ_INT(g_Session.stage_id, 1);
    CHECK_EQ_INT(g_Session.lives, 0);
    CHECK(!g_Player.is_hit);
    CHECK_EQ_INT(m_Miss.phase, MISS_DONE);
    Arena_Free(&f.arena);
}

TEST(victory_handoff_keeps_destroy_points_uncommitted)
{
    FIXTURE f;
    M_Init(&f);

    g_Gameplay.phase = GP_AFTER_ORB;
    g_Stage.game_cleared = true;
    g_Stage.score = 1000;
    g_PelletShared.destroy_score_delta = 70;
    g_Session.route = SESSION_ROUTE_JIGOKU;
    TH01_INPUT raw = {};
    Gameplay_Tick(&raw, false, 1);
    CHECK_EQ_INT(g_Gameplay.phase, GP_VICTORY_DELAY);
    CHECK_EQ_INT(g_Session.score, 1000);
    CHECK_EQ_INT(g_Session.highest_score, 1000);
    CHECK_EQ_INT(g_Session.ending, SESSION_END_JIGOKU);
    for (uint64_t tick = 2; tick < 121; tick++) {
        CHECK_EQ_INT(Gameplay_Tick(&raw, true, tick), GAMEPLAY_REQUEST_NONE);
    }
    CHECK_EQ_INT(Gameplay_Tick(&raw, true, 121), GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(g_Gameplay.phase, GP_SWITCH);
    const int16_t count1 = g_VSync.count1;
    for (uint64_t tick = 122; tick < 137; tick++) {
        CHECK_EQ_INT(Gameplay_Tick(&raw, true, tick), GAMEPLAY_REQUEST_NONE);
    }
    CHECK_EQ_INT(g_VSync.count1, count1);
    CHECK_EQ_INT(g_Gameplay.switch_fade.remaining, 1);
    CHECK_EQ_INT(Gameplay_Tick(&raw, true, 137), GAMEPLAY_REQUEST_ENDING);
    CHECK_EQ_INT(g_Gameplay.switch_fade.remaining, 0);
    CHECK_EQ_INT(g_PelletShared.destroy_score_delta, 70);
    CHECK_EQ_INT(g_Stage.score, 1000);
    Arena_Free(&f.arena);
}

TEST(real_card_consumer_runs_bonus_and_reuses_stage_storage)
{
    FIXTURE f;
    M_Init(&f);

    BONUS_STAGE_ASSETS bonus = {};
    f.assets->bonus = &bonus;
    f.arena.capacity = f.arena.used;
    const STAGE_CARD *const cards = g_StageObjects.cards;
    const STAGE_OBSTACLE *const obstacles = g_StageObjects.obstacles;
    TH01_INPUT raw = {};
    uint64_t tick = 0;
    for (int16_t stage = 0; stage < 3; stage++) {
        CHECK_EQ_INT(g_Session.stage_id, stage);
        g_Orb.left = g_StageObjects.cards[0].left;
        g_Orb.top = g_StageObjects.cards[0].top;
        Stage_HitTestCards();
        CHECK_EQ_INT(g_StageObjects.cards[0].flag, STAGE_CARD_FLIPPING);
        CHECK(g_Stage.score > 0);
        for (int32_t i = 0; i < 30; i++) {
            Stage_UpdateCards(&f.assets->stage, &f.assets->hud_font);
        }
        CHECK(g_Stage.cleared);
        CHECK(g_Player.is_hit);
        bool entered_bonus = false;
        for (int32_t i = 0; i < 1000; i++) {
            raw.shot = i % 20 < 10;
            Gameplay_Tick(&raw, true, ++tick);
            entered_bonus |= g_Gameplay.phase == GP_STAGE_BONUS;
            if (g_Session.stage_id == stage + 1 && g_Gameplay.phase == GP_ORB) {
                break;
            }
        }
        CHECK(entered_bonus);
        CHECK_EQ_INT(g_Session.stage_id, stage + 1);
        CHECK_EQ_INT(g_Gameplay.phase, GP_ORB);
        CHECK(g_StageObjects.cards == cards);
        CHECK(g_StageObjects.obstacles == obstacles);
        CHECK_EQ_INT(g_StageObjects.card_count, 1);
        CHECK(!g_Stage.cleared);
        CHECK_EQ_INT(m_Miss.phase, MISS_DONE);
    }
    Arena_Free(&f.arena);
}

TEST(miss_gameover_and_score_handoff_keep_prepared_storage)
{
    FIXTURE f;
    M_Init(&f);
    const STAGE_CARD *const cards = g_StageObjects.cards;

    f.arena.capacity = f.arena.used;
    const TH01_INPUT raw = {};
    uint64_t tick = 0;
    g_Player.is_hit = true;
    bool entered_miss = false;
    for (int32_t i = 0; i < 1000; i++) {
        Gameplay_Tick(&raw, true, ++tick);
        entered_miss |= g_Gameplay.phase == GP_MISS;
        if (entered_miss && g_Gameplay.phase == GP_ORB) {
            break;
        }
    }
    CHECK(entered_miss);
    CHECK_EQ_INT(g_Session.lives, 3);
    CHECK(g_Player.invincible);
    CHECK(!g_Player.is_hit);
    g_Session.lives = 0;
    g_Player.is_hit = true;
    g_Gameplay.phase = GP_FRAME_BEGIN;
    for (int32_t i = 0; i < 200; i++) {
        if (Gameplay_Tick(&raw, true, ++tick) == GAMEPLAY_REQUEST_SCORE) {
            break;
        }
    }
    CHECK_EQ_INT(g_Gameplay.request, GAMEPLAY_REQUEST_SCORE);
    CHECK_EQ_INT(g_Gameplay.phase, GP_TERMINAL);
    uint8_t before[VIDEO_PAGE_SIZE];
    memcpy(before, g_Video.pages[0], sizeof(before));
    for (int32_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(Gameplay_Tick(&raw, true, ++tick), GAMEPLAY_REQUEST_SCORE);
    }
    CHECK_EQ_INT(memcmp(before, g_Video.pages[0], sizeof(before)), 0);
    Gameplay_ResumeAfterScore(&raw, tick);
    CHECK_EQ_INT(g_Gameplay.phase, GP_CONTINUE_DELAY);
    CHECK_EQ_INT(g_Session.continues_total, 1);
    CHECK_EQ_INT(g_Session.lives, 0);
    CHECK_EQ_INT(Extend_ResidentLives(), 4);
    Gameplay_Tick(&(TH01_INPUT) { .shot = true }, true, ++tick);
    CHECK_EQ_INT(g_Gameplay.phase, GP_CONTINUE_DELAY);
    Gameplay_Tick(&(TH01_INPUT) { .shot = true }, true, ++tick);
    CHECK_EQ_INT(g_Gameplay.phase, GP_SWITCH);
    for (int32_t i = 0; i < 16; i++) {
        Gameplay_Tick(&raw, true, ++tick);
    }
    CHECK_EQ_INT(g_Gameplay.request, GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(g_Gameplay.phase, GP_TITLE);
    CHECK_EQ_INT(g_Session.stage_id, 0);
    CHECK(g_StageObjects.cards == cards);
    Arena_Free(&f.arena);
}

// Returns the tick on which the Continue prompt confirms Yes, with shot
// still held from the name entry for 8 ticks, released for 2, then
// pressed again.
static int32_t M_ContinueConfirmTick(const bool fix)
{
    FIXTURE f;
    M_Init(&f);

    g_Session.settings.fix_debouncing = fix;
    g_Gameplay.request = GAMEPLAY_REQUEST_SCORE;
    uint64_t tick = 0;
    Gameplay_ResumeAfterScore(&(TH01_INPUT) { .shot = true }, tick);
    int32_t confirmed = 0;
    for (int32_t i = 1; i <= 20 && confirmed == 0; i++) {
        const TH01_INPUT raw = { .shot = i <= 8 || i > 10 };
        Gameplay_Tick(&raw, true, ++tick);
        if (g_Gameplay.phase == GP_SWITCH) {
            confirmed = i;
        }
    }
    CHECK_EQ_INT(g_Gameplay.menu_selection, 0);
    Arena_Free(&f.arena);
    return confirmed;
}

TEST(debouncing_fix_makes_continue_wait_for_a_fresh_press)
{
    CHECK_EQ_INT(M_ContinueConfirmTick(false), 1);
    CHECK_EQ_INT(M_ContinueConfirmTick(true), 12);
}

TEST(gameover_restores_defined_adjacent_array_values)
{
    const int16_t lefts[] = { 0, 80, 88 };
    const int16_t frames[] = { 12, 10, 11 };
    for (int32_t test = 0; test < 3; test++) {
        FIXTURE f;
        M_Init(&f);

        f.arena.capacity = f.arena.used;
        memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
        memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
        const int16_t frame = frames[test];
        const int16_t left = lefts[test];
        g_Gameplay.gameover_left[0] = g_Gameplay.gameover_left[1] =
            left - frame * 8;
        g_Gameplay.gameover_left[2] = left;
        g_Gameplay.gameover_left[3] = g_Gameplay.gameover_left[4] =
            left + frame * 8;
        for (int32_t i = 0; i < 5; i++) {
            g_Gameplay.gameover_clipped[i] = g_Gameplay.gameover_left[i] < 0;
            g_Gameplay.gameover_top[i] = PLAYER_TOP;
        }
        g_Gameplay.animation_frame = frame;
        g_Gameplay.phase = GP_GAMEOVER;
        Frame_BeginWait(&g_Gameplay.wait);
        Gameplay_Tick(&(TH01_INPUT) {}, true, 1);
        CHECK_EQ_INT(
            g_Video.pages[0][PLAYER_TOP * VIDEO_WIDTH + left + frame * 8], 3);
        Arena_Free(&f.arena);
    }
}

TEST(gameover_redraws_orb_before_copying_the_miss_frame)
{
    FIXTURE f;
    M_Init(&f);

    f.arena.capacity = f.arena.used;
    memset(f.assets->stage.pixels + ORB_FIRST_IMAGE * 1024, 6, 1024);
    g_Player.is_hit = true;
    g_Session.lives = 0;
    Gameplay_Tick(&(TH01_INPUT) {}, false, 1);
    Gameplay_Tick(&(TH01_INPUT) {}, true, 2);
    CHECK_EQ_INT(g_Gameplay.phase, GP_GAMEOVER);
    CHECK_EQ_INT(g_Video.pages[1][g_Orb.top * VIDEO_WIDTH + g_Orb.left], 6);
    Arena_Free(&f.arena);
}

TEST(real_gameover_score_registration_precedes_continue)
{
    for (uint8_t route = 0; route < 2; route++) {
        FIXTURE f;
        M_Init(&f);

        g_Session.route = route;
        g_Session.stage_id = 7;
        g_Session.lives = 0;
        g_Session.table_high_score = 3000;
        g_Session.high_score = 4000;
        g_Session.highest_score = 5000;
        g_Stage.score = 2345;
        g_Player.is_hit = true;
        uint64_t tick = 0;
        while (tick < 1000 && g_Gameplay.request != GAMEPLAY_REQUEST_SCORE) {
            Gameplay_Tick(&(TH01_INPUT) {}, true, ++tick);
        }
        CHECK_EQ_INT(g_Gameplay.request, GAMEPLAY_REQUEST_SCORE);
        VFS *const user_files = Vfs_Create(&g_HeapAllocator);
        SCORE_SAVES loaded;
        CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &loaded)));
        Vfs_Destroy(user_files);
        memset(&g_ScoreSaves, 0, sizeof(g_ScoreSaves));
        memcpy(g_ScoreSaves.tables, loaded.tables, sizeof(loaded.tables));
        SCORE_ASSETS assets = {
            .gameover = { .pixels = f.assets->backgrounds[0].pixels },
            .font = f.assets->menu_font
        };
        const uint64_t edge = g_VSync.edge;
        const int16_t count = g_VSync.count1;
        const SCORE_ENTRY entry = { .score = (int32_t)g_Session.score,
                                    .stage = (int16_t)(g_Session.stage_id + 1),
                                    .route = "魔" };
        SCORE_ENTRY routed = entry;
        strcpy(routed.route, route == 0 ? "魔" : "地");
        const size_t used = f.arena.used;
        f.arena.capacity = used;
        Score_Begin(&assets, SCORE_GAMEOVER, 1, &routed);
        for (int32_t i = 0; i < 16; i++) {
            Music_Update();
            tick++;
            CHECK(!Score_Tick(&(TH01_INPUT) {}, true));
        }
        CHECK_EQ_INT(g_VSync.edge, edge + 16);
        CHECK_EQ_INT(g_VSync.count1, count + 16);
        CHECK_EQ_INT(g_Gameplay.phase, GP_TERMINAL);
        CHECK_EQ_INT(g_Session.continues_total, 0);
        tick++;
        CHECK(Score_Tick(&(TH01_INPUT) { .ok = true }, true));
        CHECK(g_ScoreSaves.pending[1]);
        CHECK_EQ_INT(g_ScoreSaves.tables[1].entries[0].score, 2345);
        CHECK_EQ_INT(g_ScoreSaves.tables[1].entries[0].stage, 8);
        CHECK_EQ_STR(
            g_ScoreSaves.tables[1].entries[0].route, route == 0 ? "魔" : "地");
        g_Session.table_high_score =
            (uint32_t)g_ScoreSaves.tables[1].entries[0].score;
        Gameplay_ResumeAfterScore(&(TH01_INPUT) {}, tick);
        CHECK_EQ_INT(g_Gameplay.phase, GP_CONTINUE_DELAY);
        CHECK_EQ_INT(g_Session.continues_total, 1);
        CHECK_EQ_INT(g_Session.highest_score, 5000);
        CHECK_EQ_INT(g_VSync.edge, edge + 17);
        Gameplay_Tick(&(TH01_INPUT) { .shot = true }, true, ++tick);
        CHECK_EQ_INT(g_Gameplay.phase, GP_CONTINUE_DELAY);
        Gameplay_Tick(&(TH01_INPUT) { .shot = true }, true, ++tick);
        CHECK_EQ_INT(g_Gameplay.phase, GP_SWITCH);
        for (int32_t i = 0; i < 16; i++) {
            Gameplay_Tick(&(TH01_INPUT) {}, true, ++tick);
        }
        CHECK_EQ_INT(g_Gameplay.request, GAMEPLAY_REQUEST_NONE);
        CHECK_EQ_INT(g_Gameplay.phase, GP_STAGE_LABEL);
        CHECK_EQ_INT(g_Session.high_score, 2345);
        CHECK_EQ_INT(g_Session.highest_score, 5000);
        CHECK_EQ_INT(g_Session.stage_id, 7);
        CHECK_EQ_INT(g_Session.route, route);
        CHECK_EQ_INT(f.arena.used, used);
        Arena_Free(&f.arena);
    }
}

TEST(live_hud_high_score_is_distinct_from_run_history)
{
    FIXTURE f;
    M_Init(&f);
    g_Session.table_high_score = 1000;
    g_Session.high_score = 0xffffffff;
    g_Session.highest_score = 42;
    Gameplay_Reset();
    CHECK_EQ_INT(g_Session.high_score, 1000);
    CHECK_EQ_INT(g_Session.highest_score, 42);
    g_Stage.score = 0x80000000;
    Hud_ScoreAndCombo();
    CHECK_EQ_INT(g_Session.high_score, 0x80000000);
    CHECK_EQ_INT(g_Session.highest_score, 42);
    Gameplay_Reset();
    CHECK_EQ_INT(g_Session.high_score, 1000);
    CHECK_EQ_INT(g_Session.highest_score, 42);
    Arena_Free(&f.arena);
}

TEST(ending_seed_is_saved_after_the_life_exit_scroll_wait)
{
    FIXTURE f;
    M_Init(&f);

    g_Gameplay.phase = GP_LIFE_EXIT;
    g_Session.lives = 0;
    g_Session.frame_rand = 1234567;
    g_Session.exit_rand = 99;
    Frame_BeginWait(&g_Gameplay.wait);
    Gameplay_Tick(&(TH01_INPUT) {}, false, 1);
    CHECK_EQ_INT(g_Session.exit_rand, 99);
    CHECK_EQ_INT(g_Gameplay.phase, GP_LIFE_EXIT);
    Gameplay_Tick(&(TH01_INPUT) {}, true, 2);
    CHECK_EQ_INT(g_Session.exit_rand, 1234567);
    CHECK_EQ_INT(g_Gameplay.phase, GP_GAMEOVER);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    Arena_Free(&f.arena);
}

TEST(disabled_gameplay_cannot_start_effects)
{
    FIXTURE f;
    M_Init(&f);
    g_Session.settings.music = SETTINGS_MUSIC_OFF;
    Sound_Effect(5);
    Music_Update();
    CHECK(!M_EffectSounds());
    g_Session.settings.music = SETTINGS_MUSIC_FM;
    Sound_Effect(5);
    CHECK(M_EffectSounds());
    Arena_Free(&f.arena);
}

TEST(disabled_gameplay_preserves_an_existing_opening_fade)
{
    FIXTURE f;
    M_Init(&f);
    g_Session.settings.music = SETTINGS_MUSIC_OFF;
    MDRV *twin;
    CHECK(Result_Ignore(Mdrv_Init(
        f.assets->songs, GAMEPLAY_SONG_COUNT, &f.assets->effects, 8, 8,
        Arena_Allocator(&f.arena), &twin)));
    const size_t used = f.arena.used;
    Music_Play(15);
    Music_Fade();
    Mdrv_Play(twin, 15);
    Mdrv_Fade(twin, 1);
    for (int32_t i = 0; i < 10; i++) {
        Music_Update();
    }
    Mdrv_Render(twin, nullptr, 8000);
    Sound_Effect(5);
    Sound_Load(3);
    Sound_Play();
    Sound_Stop();
    Sound_Fade();
    Sound_Stop();
    Sound_Stop();
    CHECK_EQ_INT(m_Song, 3);
    static MDRV_FRAME expected[4800], actual[4800];
    Mdrv_Render(twin, expected, 4800);
    Mdrv_Render(m_Driver, actual, 4800);
    CHECK(memcmp(expected, actual, sizeof(actual)) == 0);
    Music_Stop();
    CHECK_EQ_INT(f.arena.used, used);
    Arena_Free(&f.arena);
}

TEST(enabled_gameplay_controls_the_prepared_producer)
{
    FIXTURE f;
    M_Init(&f);
    Sound_Load(3);
    Sound_Play();
    Sound_Effect(5);
    CHECK(M_EffectSounds());
    Sound_Effect(5);
    Sound_Fade();
    Sound_Load(4);
    Sound_Play();
    CHECK_EQ_INT(m_Song, 4);
    CHECK(!M_EffectSounds());
    Sound_Effect(5);
    Sound_Stop();
    CHECK(!M_EffectSounds());
    Sound_Load(5);
    Sound_Play();
    Sound_Effect(5);
    Sound_Stop();
    CHECK(!M_EffectSounds());
    Arena_Free(&f.arena);
}

TEST(prepared_output_survives_world_reset_and_opening_controls)
{
    CHECK(Result_Ignore(Audio_Init()));
    FIXTURE f;
    M_Init(&f);
    GAME_AUDIO *output;
    CHECK(Result_Ignore(GameAudio_Prepare(Arena_Allocator(&f.arena), &output)));
    Music_BindOutput(output);
    Sound_Effect(5);
    Music_Update();
    const size_t used = f.arena.used;
    Gameplay_Reset();
    CHECK(m_Output == output);
    Music_Fade();
    Music_Update();
    float samples[3200];
    const Uint64 deadline = SDL_GetTicks() + 5000;
    bool mixed = AudioBackend_Mix(samples, 1600);
    while (!mixed && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
        mixed = AudioBackend_Mix(samples, 1600);
    }
    CHECK(mixed);
    bool nonzero = false;
    for (size_t i = 0; i < 3200; i++) {
        nonzero |= samples[i] != 0;
    }
    CHECK(nonzero);
    CHECK_EQ_INT(f.arena.used, used);
    Music_BindOutput(nullptr);
    GameAudio_Free(output);
    Audio_Shutdown();
    Arena_Free(&f.arena);
}

TEST(reset_restores_prepared_defaults_without_rebuilding_storage)
{
    CHECK(Result_Ignore(Audio_Init()));
    FIXTURE f;
    M_Init(&f);

    GAME_AUDIO *output;
    CHECK(Result_Ignore(GameAudio_Prepare(Arena_Allocator(&f.arena), &output)));
    Music_BindOutput(output);
    Sound_Load(4);
    Sound_Play();
    Music_Update();
    const void *const numbers = m_Hud.numbers;
    const void *const backgrounds = g_HudHp.backgrounds;
    const size_t used = f.arena.used;
    memset(&g_Gameplay, 0xa5, sizeof(g_Gameplay));
    memset(&g_B10J, 0xa5, sizeof(g_B10J));
    memset(&g_B20J, 0xa5, sizeof(g_B20J));
    memset(&g_B20M, 0xa5, sizeof(g_B20M));
    Gameplay_Reset();
    CHECK_EQ_INT(f.arena.used, used);
    CHECK(m_Scene.active == &f.assets->scenes[Session_Scene()]);
    CHECK_EQ_INT(g_Gameplay.phase, GP_STAGE_BEGIN);
    CHECK(g_Stage.first_stage && g_Gameplay.wait_for_shot);
    CHECK_EQ_INT(g_Gameplay.frame, 0);
    CHECK_EQ_INT(g_VSync.edge, 0);
    CHECK_EQ_INT(g_Player.left, 304);
    CHECK_EQ_INT(m_Hud.score_previous, 0);
    CHECK(m_Hud.numbers == numbers);
    CHECK(g_HudHp.backgrounds == backgrounds);
    CHECK(g_Tram.visible);
    CHECK_EQ_INT(g_Tram.pixels[VIDEO_PAGE_SIZE - 1], 0);
    CHECK(g_B10J.meteor_active);
    CHECK_EQ_INT(g_B10J.spread_interval, 4);
    CHECK_EQ_INT(g_B10J.spread_speed, 8);
    CHECK_EQ_INT(g_B20M.ray_previous_x, 999);
    CHECK_EQ_INT(g_B20M.ray_previous_y, 999);
    CHECK_EQ_INT(g_B20M.flake.state, -1);
    CHECK_EQ_INT(g_B20J.ray_previous_x, 999);
    Music_BindOutput(nullptr);
    GameAudio_Free(output);
    Audio_Shutdown();
    Arena_Free(&f.arena);
}
