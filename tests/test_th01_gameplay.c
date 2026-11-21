#include "th01/gameplay_internal.h"
#include "th01/score.h"
#include "th01_assets.h"

#include "core/base/arena.h"
#include "core/audio.h"
#include "core/audio/backend.h"

#include "test.h"

#include <SDL3/SDL.h>

typedef struct {
    ARENA arena;
    GAMEPLAY *game;
    GAMEPLAY_ASSETS *assets;
    SESSION session;
    VIDEO video;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Arena_Init(&f->arena, "gameplay", 8 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(&f->video, a);
    f->session = (SESSION) { .lives = 4,
                             .bombs = 1,
                             .settings = { .difficulty = 1,
                                           .bombs = 1,
                                           .lives_extra = 2,
                                           .music = SETTINGS_MUSIC_FM } };
    f->assets = Th01TestAssets_Create(a);
    CHECK(Result_Log(
        Gameplay_Prepare(a, f->assets, &f->video, &f->session, &f->game),
        LOG_LEVEL_ERROR));
    GAMEPLAY *const g = f->game;
    Gameplay_ResetWorld(g);
    memset(&g->world.vsync, 0, sizeof(g->world.vsync));
    StageObjects_InitPrepared(
        &g->world.objects, &f->assets->scenes[0].layouts, 0, true,
        &g->storage.objects, g->storage.video, &f->assets->stage,
        &g->world.rng);
    Orb_ResetFlight(&g->world.orb);
    g->world.timer.time = 1000;
    g->world.phase = GP_FRAME_BEGIN;
    g->world.timer_initialized = true;
    g->world.first_stage = false;
    g->world.invincibility.frame[0] = 255;
}

TEST(ordinary_loop_consumes_real_input_and_suspends_after_motion)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    const size_t used = f.arena.used;
    TH01_INPUT raw = { .right = true, .shot = true };
    CHECK_EQ_INT(Gameplay_Tick(g, &raw, false, 1), GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(f.session.frame_rand, 1);
    CHECK_EQ_INT(g->world.frame, 1);
    CHECK_EQ_INT(g->world.orb.force_frame, 1);
    CHECK_EQ_INT(g->world.player.left, 308);
    CHECK_EQ_INT(g->world.orb.left, 596);
    CHECK_EQ_INT(g->world.phase, GP_ORB);
    CHECK(g->world.composite.waiting);
    const PLAYER player = g->world.player;
    const ORB orb = g->world.orb;
    const uint32_t rng = g->world.rng;
    Gameplay_Tick(g, &raw, false, 2);
    CHECK_EQ_INT(memcmp(&player, &g->world.player, sizeof(player)), 0);
    CHECK_EQ_INT(memcmp(&orb, &g->world.orb, sizeof(orb)), 0);
    CHECK_EQ_INT(g->world.rng, rng);
    Gameplay_Tick(g, &raw, true, 3);
    CHECK_EQ_INT(f.session.frame_rand, 2);
    CHECK_EQ_INT(g->world.frame, 2);
    CHECK_EQ_INT(g->world.orb.force_frame, 2);
    Gameplay_Tick(g, &raw, true, 4);
    CHECK(g->world.shots.slots[0].moving);
    CHECK(MdrvSynth_Status(g->storage.audio).effects_playing);
    CHECK_EQ_INT(f.arena.used, used);
    Arena_Free(&f.arena);
}

TEST(hurry_wait_freezes_the_remaining_iteration_prefix)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    f.session.frame_rand = 3;
    g->world.timer.time = 2;
    g->world.player.invincibility_time = 2;
    TH01_INPUT raw = {};
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->world.phase, GP_TIMER);
    CHECK_EQ_INT(g->world.timer.phase, TIMER_WAIT_HURRY);
    CHECK_EQ_INT(g->world.frame, 1);
    CHECK_EQ_INT(g->world.player.invincibility_time, 1);
    CHECK_EQ_INT(g->world.orb.left, 600);
    const uint32_t rng = g->world.rng;
    const PLAYER player = g->world.player;
    for (uint64_t i = 2; i < 12; i++) {
        Gameplay_Tick(g, &raw, false, i);
        CHECK_EQ_INT(g->world.frame, 1);
        CHECK_EQ_INT(f.session.frame_rand, 4);
        CHECK_EQ_INT(g->world.rng, rng);
        CHECK_EQ_INT(memcmp(&player, &g->world.player, sizeof(player)), 0);
    }
    Arena_Free(&f.arena);
}

TEST(real_shot_boundary_advances_player_before_timer_and_rng_seed)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    g->world.phase = GP_WAIT_SHOT;
    g->world.wait_for_shot = true;
    g->world.timer_initialized = false;
    g->world.input = (GAME_INPUT) {};
    f.session.frame_rand = 42;
    TH01_INPUT raw = { .shot = true, .left = true };
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->world.phase, GP_WAIT_SHOT_DELAY);
    CHECK_EQ_INT(g->world.player.left, 300);
    CHECK(!g->world.timer_initialized);
    CHECK_EQ_INT(f.session.frame_rand, 42);
    Gameplay_Tick(g, &raw, true, 2);
    CHECK(g->world.timer_initialized);
    CHECK(!g->world.wait_for_shot);
    CHECK_EQ_INT(g->world.frame, 1);
    CHECK_EQ_INT(f.session.frame_rand, 43);
    CHECK_EQ_INT(g->world.player.bomb_frame, 2);
    Arena_Free(&f.arena);
}

TEST(stage_clear_beats_miss_and_starts_real_bonus)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    BONUS_STAGE_ASSETS stage = {};
    f.assets->bonus = &stage;
    g->bindings.bonus_context.stage = &stage;
    g->world.phase = GP_FRAME_BEGIN;
    g->world.player.is_hit = true;
    g->world.metrics.cleared = true;
    f.session.lives = 0;
    g->world.timer.time = 500;
    TH01_INPUT raw = {};
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->world.phase, GP_LIFE_EXIT);
    Gameplay_Tick(g, &raw, true, 2);
    CHECK_EQ_INT(g->world.phase, GP_STAGE_BONUS);
    CHECK_EQ_INT(f.session.stage_id, 1);
    CHECK_EQ_INT(f.session.lives, 0);
    CHECK(!g->world.player.is_hit);
    CHECK_EQ_INT(g->world.miss.phase, MISS_DONE);
    Arena_Free(&f.arena);
}

TEST(victory_handoff_keeps_destroy_points_uncommitted)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    g->world.phase = GP_AFTER_ORB;
    g->world.game_cleared = true;
    g->world.metrics.score = 1000;
    g->world.pellet_shared.destroy_score_delta = 70;
    f.session.route = SESSION_ROUTE_JIGOKU;
    TH01_INPUT raw = {};
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->world.phase, GP_VICTORY_DELAY);
    CHECK_EQ_INT(f.session.score, 1000);
    CHECK_EQ_INT(f.session.highest_score, 1000);
    CHECK_EQ_INT(f.session.ending, SESSION_END_JIGOKU);
    for (uint64_t tick = 2; tick < 121; tick++) {
        CHECK_EQ_INT(Gameplay_Tick(g, &raw, true, tick), GAMEPLAY_REQUEST_NONE);
    }
    CHECK_EQ_INT(Gameplay_Tick(g, &raw, true, 121), GAMEPLAY_REQUEST_ENDING);
    CHECK_EQ_INT(g->world.pellet_shared.destroy_score_delta, 70);
    CHECK_EQ_INT(g->world.metrics.score, 1000);
    Arena_Free(&f.arena);
}

TEST(real_card_consumer_runs_bonus_and_reuses_stage_storage)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    BONUS_STAGE_ASSETS bonus = {};
    f.assets->bonus = &bonus;
    Gameplay_Bind(g);
    f.arena.capacity = f.arena.used;
    const STAGE_CARD *const cards = g->storage.objects.cards;
    const STAGE_OBSTACLE *const obstacles = g->storage.objects.obstacles;
    TH01_INPUT raw = {};
    uint64_t tick = 0;
    for (int16_t stage = 0; stage < 3; stage++) {
        CHECK_EQ_INT(f.session.stage_id, stage);
        g->world.orb.left = g->world.objects.cards[0].left;
        g->world.orb.top = g->world.objects.cards[0].top;
        Stage_HitTestCards(&g->bindings.stage);
        CHECK_EQ_INT(g->world.objects.cards[0].flag, STAGE_CARD_FLIPPING);
        CHECK(g->world.metrics.score > 0);
        for (int32_t i = 0; i < 30; i++) {
            Stage_UpdateCards(&g->bindings.stage);
        }
        CHECK(g->world.metrics.cleared);
        CHECK(g->world.player.is_hit);
        bool entered_bonus = false;
        for (int32_t i = 0; i < 1000; i++) {
            raw.shot = i % 20 < 10;
            Gameplay_Tick(g, &raw, true, ++tick);
            entered_bonus |= g->world.phase == GP_STAGE_BONUS;
            if (f.session.stage_id == stage + 1 && g->world.phase == GP_ORB) {
                break;
            }
        }
        CHECK(entered_bonus);
        CHECK_EQ_INT(f.session.stage_id, stage + 1);
        CHECK_EQ_INT(g->world.phase, GP_ORB);
        CHECK(g->storage.objects.cards == cards);
        CHECK(g->storage.objects.obstacles == obstacles);
        CHECK_EQ_INT(g->world.objects.card_count, 1);
        CHECK(!g->world.metrics.cleared);
        CHECK_EQ_INT(g->world.miss.phase, MISS_DONE);
    }
    Arena_Free(&f.arena);
}

TEST(miss_gameover_and_score_handoff_keep_prepared_storage)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    f.arena.capacity = f.arena.used;
    const TH01_INPUT raw = {};
    uint64_t tick = 0;
    g->world.player.is_hit = true;
    bool entered_miss = false;
    for (int32_t i = 0; i < 1000; i++) {
        Gameplay_Tick(g, &raw, true, ++tick);
        entered_miss |= g->world.phase == GP_MISS;
        if (entered_miss && g->world.phase == GP_ORB) {
            break;
        }
    }
    CHECK(entered_miss);
    CHECK_EQ_INT(f.session.lives, 3);
    CHECK(g->world.player.invincible);
    CHECK(!g->world.player.is_hit);
    f.session.lives = 0;
    g->world.player.is_hit = true;
    g->world.phase = GP_FRAME_BEGIN;
    for (int32_t i = 0; i < 200; i++) {
        if (Gameplay_Tick(g, &raw, true, ++tick) == GAMEPLAY_REQUEST_SCORE) {
            break;
        }
    }
    CHECK_EQ_INT(g->world.request, GAMEPLAY_REQUEST_SCORE);
    CHECK_EQ_INT(g->world.phase, GP_TERMINAL);
    uint8_t before[VIDEO_PAGE_SIZE];
    memcpy(before, f.video.pages[0], sizeof(before));
    for (int32_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(
            Gameplay_Tick(g, &raw, true, ++tick), GAMEPLAY_REQUEST_SCORE);
    }
    CHECK_EQ_INT(memcmp(before, f.video.pages[0], sizeof(before)), 0);
    Gameplay_ResumeAfterScore(g, &raw, tick);
    CHECK_EQ_INT(g->world.phase, GP_CONTINUE_DELAY);
    CHECK_EQ_INT(f.session.continues_total, 1);
    CHECK_EQ_INT(f.session.lives, 0);
    CHECK_EQ_INT(g->world.retained_lives, 4);
    Gameplay_Tick(g, &(TH01_INPUT) { .shot = true }, true, ++tick);
    CHECK_EQ_INT(g->world.phase, GP_CONTINUE_DELAY);
    Gameplay_Tick(g, &(TH01_INPUT) { .shot = true }, true, ++tick);
    CHECK_EQ_INT(g->world.request, GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(g->world.phase, GP_TITLE);
    CHECK_EQ_INT(f.session.stage_id, 0);
    CHECK(g->storage.objects.cards == f.game->world.objects.cards);
    Arena_Free(&f.arena);
}

TEST(gameover_restores_defined_adjacent_array_values)
{
    const int16_t lefts[] = { 0, 80, 88 };
    const int16_t frames[] = { 12, 10, 11 };
    for (int32_t test = 0; test < 3; test++) {
        FIXTURE f;
        M_Init(&f);
        GAMEPLAY *const g = f.game;
        f.arena.capacity = f.arena.used;
        memset(f.video.pages[0], 9, VIDEO_PAGE_SIZE);
        memset(f.video.pages[1], 3, VIDEO_PAGE_SIZE);
        const int16_t frame = frames[test];
        const int16_t left = lefts[test];
        g->world.gameover_left[0] = g->world.gameover_left[1] =
            left - frame * 8;
        g->world.gameover_left[2] = left;
        g->world.gameover_left[3] = g->world.gameover_left[4] =
            left + frame * 8;
        for (int32_t i = 0; i < 5; i++) {
            g->world.gameover_clipped[i] = g->world.gameover_left[i] < 0;
            g->world.gameover_top[i] = PLAYER_TOP;
        }
        g->world.animation_frame = frame;
        g->world.phase = GP_GAMEOVER;
        Frame_BeginWait(&g->world.wait, &g->world.vsync);
        Gameplay_Tick(g, &(TH01_INPUT) {}, true, 1);
        CHECK_EQ_INT(
            f.video.pages[0][PLAYER_TOP * VIDEO_WIDTH + left + frame * 8], 3);
        Arena_Free(&f.arena);
    }
}

TEST(gameover_redraws_orb_before_copying_the_miss_frame)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    f.arena.capacity = f.arena.used;
    memset(f.assets->stage.pixels + ORB_FIRST_IMAGE * 1024, 6, 1024);
    g->world.player.is_hit = true;
    f.session.lives = 0;
    Gameplay_Tick(g, &(TH01_INPUT) {}, false, 1);
    Gameplay_Tick(g, &(TH01_INPUT) {}, true, 2);
    CHECK_EQ_INT(g->world.phase, GP_GAMEOVER);
    CHECK_EQ_INT(
        f.video.pages[1][g->world.orb.top * VIDEO_WIDTH + g->world.orb.left],
        6);
    Arena_Free(&f.arena);
}

TEST(real_gameover_score_registration_precedes_continue)
{
    for (uint8_t route = 0; route < 2; route++) {
        FIXTURE f;
        M_Init(&f);
        GAMEPLAY *const g = f.game;
        f.session.route = route;
        f.session.stage_id = 7;
        f.session.lives = 0;
        f.session.table_high_score = 3000;
        f.session.high_score = 4000;
        f.session.highest_score = 5000;
        g->world.metrics.score = 2345;
        g->world.player.is_hit = true;
        uint64_t tick = 0;
        while (tick < 1000 && g->world.request != GAMEPLAY_REQUEST_SCORE) {
            Gameplay_Tick(g, &(TH01_INPUT) {}, true, ++tick);
        }
        CHECK_EQ_INT(g->world.request, GAMEPLAY_REQUEST_SCORE);
        SCORE_SAVES saves = {};
        for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
            ScoreData_Defaults(&saves.tables[rank]);
        }
        SCORE_ASSETS assets = {
            .gameover = { .pixels = f.assets->backgrounds[0].pixels },
            .font = f.assets->menu_font
        };
        const GAMEPLAY_SCORE_CONTEXT context = Gameplay_ScoreContext(g);
        const uint64_t edge = context.vsync->edge;
        const int16_t count = context.vsync->count1;
        const SCORE_ENTRY entry = { .score = (int32_t)f.session.score,
                                    .stage = (int16_t)(f.session.stage_id + 1),
                                    .route = "魔" };
        SCORE_ENTRY routed = entry;
        strcpy(routed.route, route == 0 ? "魔" : "地");
        SCORE_SCENE score;
        const size_t used = f.arena.used;
        f.arena.capacity = used;
        Score_Begin(
            &score, &assets, &f.video, &saves, context.vsync, context.input,
            SCORE_GAMEOVER, 1, &routed);
        for (int32_t i = 0; i < 32; i++) {
            Gameplay_AdvanceAudio(g);
            CHECK(!Score_Tick(&score, &(TH01_INPUT) {}, true, ++tick));
        }
        CHECK_EQ_INT(context.vsync->edge, edge + 32);
        CHECK_EQ_INT(context.vsync->count1, count + 32);
        CHECK_EQ_INT(g->world.phase, GP_TERMINAL);
        CHECK_EQ_INT(f.session.continues_total, 0);
        CHECK(Score_Tick(&score, &(TH01_INPUT) { .ok = true }, true, ++tick));
        CHECK(saves.pending[1]);
        CHECK_EQ_INT(saves.tables[1].entries[0].score, 2345);
        CHECK_EQ_INT(saves.tables[1].entries[0].stage, 8);
        CHECK_EQ_STR(
            saves.tables[1].entries[0].route, route == 0 ? "魔" : "地");
        f.session.table_high_score = (uint32_t)saves.tables[1].entries[0].score;
        Gameplay_ResumeAfterScore(g, &(TH01_INPUT) {}, tick);
        CHECK_EQ_INT(g->world.phase, GP_CONTINUE_DELAY);
        CHECK_EQ_INT(f.session.continues_total, 1);
        CHECK_EQ_INT(f.session.highest_score, 5000);
        CHECK_EQ_INT(context.vsync->edge, edge + 33);
        Gameplay_Tick(g, &(TH01_INPUT) { .shot = true }, true, ++tick);
        CHECK_EQ_INT(g->world.phase, GP_CONTINUE_DELAY);
        Gameplay_Tick(g, &(TH01_INPUT) { .shot = true }, true, ++tick);
        CHECK_EQ_INT(g->world.request, GAMEPLAY_REQUEST_NONE);
        CHECK_EQ_INT(g->world.phase, GP_STAGE_LABEL);
        CHECK_EQ_INT(f.session.high_score, 2345);
        CHECK_EQ_INT(f.session.highest_score, 5000);
        CHECK_EQ_INT(f.session.stage_id, 7);
        CHECK_EQ_INT(f.session.route, route);
        CHECK_EQ_INT(f.arena.used, used);
        Arena_Free(&f.arena);
    }
}

TEST(live_hud_high_score_is_distinct_from_run_history)
{
    FIXTURE f;
    M_Init(&f);
    f.session.table_high_score = 1000;
    f.session.high_score = 0xffffffff;
    f.session.highest_score = 42;
    Gameplay_ResetWorld(f.game);
    CHECK_EQ_INT(f.session.high_score, 1000);
    CHECK_EQ_INT(f.session.highest_score, 42);
    f.game->world.metrics.score = 0x80000000;
    Hud_ScoreAndCombo(&f.game->world.hud, &f.game->bindings.display);
    CHECK_EQ_INT(f.session.high_score, 0x80000000);
    CHECK_EQ_INT(f.session.highest_score, 42);
    Gameplay_ResetWorld(f.game);
    CHECK_EQ_INT(f.session.high_score, 1000);
    CHECK_EQ_INT(f.session.highest_score, 42);
    Arena_Free(&f.arena);
}

TEST(ending_seed_is_saved_after_the_life_exit_scroll_wait)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    g->world.phase = GP_LIFE_EXIT;
    f.session.lives = 0;
    f.session.frame_rand = 1234567;
    f.session.exit_rand = 99;
    Frame_BeginWait(&g->world.wait, &g->world.vsync);
    Gameplay_Tick(g, &(TH01_INPUT) {}, false, 1);
    CHECK_EQ_INT(f.session.exit_rand, 99);
    CHECK_EQ_INT(g->world.phase, GP_LIFE_EXIT);
    Gameplay_Tick(g, &(TH01_INPUT) {}, true, 2);
    CHECK_EQ_INT(f.session.exit_rand, 1234567);
    CHECK_EQ_INT(g->world.phase, GP_GAMEOVER);
    CHECK_EQ_INT(f.video.scroll_top, 0);
    Arena_Free(&f.arena);
}

TEST(disabled_gameplay_cannot_start_effects)
{
    FIXTURE f;
    M_Init(&f);
    f.session.settings.music = SETTINGS_MUSIC_OFF;
    Gameplay_Effect(f.game, 5);
    Gameplay_AdvanceAudio(f.game);
    CHECK(!MdrvSynth_Status(f.game->storage.audio).effects_playing);
    f.session.settings.music = SETTINGS_MUSIC_FM;
    Gameplay_Effect(f.game, 5);
    CHECK(MdrvSynth_Status(f.game->storage.audio).effects_playing);
    Arena_Free(&f.arena);
}

TEST(disabled_gameplay_preserves_an_existing_opening_fade)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    f.session.settings.music = SETTINGS_MUSIC_OFF;
    const size_t used = f.arena.used;
    Gameplay_OpeningAudio(g, (OPENING_EVENTS) { .audio = OPENING_AUDIO_PLAY });
    Gameplay_OpeningAudio(g, (OPENING_EVENTS) { .audio = OPENING_AUDIO_FADE });
    for (int32_t i = 0; i < 10; i++) {
        Gameplay_AdvanceAudio(g);
    }
    const MDRV_SYNTH_STATUS before = MdrvSynth_Status(g->storage.audio);
    CHECK(before.music_playing && before.fading);
    CHECK(before.fade_attenuation > 0);
    Gameplay_Effect(g, 5);
    Gameplay_Play(g, 3);
    Gameplay_Stop(g);
    Gameplay_Fade(g);
    g->bindings.c20j.audio(g, B20J_AUDIO_STOP, nullptr);
    g->bindings.c20m.audio(g, B20M_AUDIO_STOP, nullptr);
    const MDRV_SYNTH_STATUS after = MdrvSynth_Status(g->storage.audio);
    CHECK(after.music_playing && after.fading);
    CHECK(!after.effects_playing);
    CHECK_EQ_INT(after.time, before.time);
    CHECK_EQ_INT(after.pulses, before.pulses);
    CHECK_EQ_INT(after.fade_attenuation, before.fade_attenuation);
    CHECK_EQ_INT(g->storage.song, 3);
    Gameplay_AdvanceAudio(g);
    CHECK(MdrvSynth_Status(g->storage.audio).time > after.time);
    CHECK(MdrvSynth_Status(g->storage.audio).fading);
    Gameplay_OpeningAudio(g, (OPENING_EVENTS) { .audio = OPENING_AUDIO_STOP });
    CHECK(!MdrvSynth_Status(g->storage.audio).music_playing);
    CHECK(!MdrvSynth_Status(g->storage.audio).fading);
    CHECK_EQ_INT(f.arena.used, used);
    Arena_Free(&f.arena);
}

TEST(enabled_gameplay_controls_the_prepared_producer)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    Gameplay_Play(g, 3);
    Gameplay_Effect(g, 5);
    CHECK(MdrvSynth_Status(g->storage.audio).music_playing);
    CHECK(MdrvSynth_Status(g->storage.audio).effects_playing);
    Gameplay_Fade(g);
    CHECK(MdrvSynth_Status(g->storage.audio).fading);
    Gameplay_Play(g, 4);
    CHECK(MdrvSynth_Status(g->storage.audio).music_playing);
    CHECK(!MdrvSynth_Status(g->storage.audio).effects_playing);
    CHECK(!MdrvSynth_Status(g->storage.audio).fading);
    g->bindings.c20j.audio(g, B20J_AUDIO_STOP, nullptr);
    CHECK(!MdrvSynth_Status(g->storage.audio).music_playing);
    Gameplay_Play(g, 5);
    g->bindings.c20m.audio(g, B20M_AUDIO_STOP, nullptr);
    CHECK(!MdrvSynth_Status(g->storage.audio).music_playing);
    Arena_Free(&f.arena);
}

TEST(prepared_output_survives_world_reset_and_opening_controls)
{
    CHECK(Result_Ignore(Audio_Init()));
    FIXTURE f;
    M_Init(&f);
    GAME_AUDIO *output;
    CHECK(Result_Ignore(GameAudio_Prepare(Arena_Allocator(&f.arena), &output)));
    Gameplay_BindAudio(f.game, output);
    Gameplay_Effect(f.game, 5);
    Gameplay_AdvanceAudio(f.game);
    const MDRV_SYNTH_STATUS before = MdrvSynth_Status(f.game->storage.audio);
    const size_t used = f.arena.used;
    Gameplay_ResetWorld(f.game);
    CHECK(f.game->storage.audio_output == output);
    CHECK_EQ_INT(MdrvSynth_Status(f.game->storage.audio).time, before.time);
    Gameplay_OpeningAudio(
        f.game, (OPENING_EVENTS) { .audio = OPENING_AUDIO_FADE });
    Gameplay_AdvanceAudio(f.game);
    CHECK(MdrvSynth_Status(f.game->storage.audio).time > before.time);
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
    Gameplay_BindAudio(f.game, nullptr);
    GameAudio_Free(output);
    Audio_Shutdown();
    Arena_Free(&f.arena);
}

TEST(reset_restores_prepared_defaults_without_rebuilding_storage)
{
    CHECK(Result_Ignore(Audio_Init()));
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = f.game;
    GAME_AUDIO *output;
    CHECK(Result_Ignore(GameAudio_Prepare(Arena_Allocator(&f.arena), &output)));
    Gameplay_BindAudio(g, output);
    Gameplay_Play(g, 4);
    Gameplay_AdvanceAudio(g);
    g->storage.host_tick = 1234;
    const GAMEPLAY_STORAGE prepared = g->storage;
    const uint64_t audio_time = MdrvSynth_Status(g->storage.audio).time;
    const size_t used = f.arena.used;
    memset(&g->world, 0xa5, sizeof(g->world));
    memset(&g->bindings, 0xa5, sizeof(g->bindings));
    Gameplay_ResetWorld(g);
    CHECK_EQ_INT(f.arena.used, used);
    CHECK(memcmp(&g->storage, &prepared, sizeof(prepared)) == 0);
    CHECK_EQ_INT(MdrvSynth_Status(g->storage.audio).time, audio_time);
    CHECK_EQ_INT(g->world.phase, GP_STAGE_BEGIN);
    CHECK(g->world.first_stage && g->world.wait_for_shot);
    CHECK_EQ_INT(g->world.frame, 0);
    CHECK_EQ_INT(g->world.vsync.edge, 0);
    CHECK_EQ_INT(g->world.player.left, 304);
    CHECK_EQ_INT(g->world.hud.score_previous, 0);
    CHECK(g->world.hud.numbers == prepared.initial->hud.numbers);
    CHECK(g->world.health.backgrounds == prepared.initial->health.backgrounds);
    CHECK(g->world.tram.pixels == prepared.initial->tram.pixels);
    CHECK(g->world.tram.visible);
    CHECK_EQ_INT(g->world.tram.pixels[VIDEO_PAGE_SIZE - 1], 0);
    CHECK(g->world.b10j.background == prepared.initial->b10j.background);
    CHECK(g->world.b10j.meteor_active);
    CHECK_EQ_INT(g->world.b10j.spread_interval, 4);
    CHECK_EQ_INT(g->world.b10j.spread_speed, 8);
    CHECK(g->world.b15m.background == prepared.initial->b15m.background);
    CHECK(
        g->world.b20m.wand_background
        == prepared.initial->b20m.wand_background);
    CHECK_EQ_INT(g->world.b20m.ray_previous_x, 999);
    CHECK_EQ_INT(g->world.b20m.ray_previous_y, 999);
    CHECK_EQ_INT(g->world.b20m.flake_state, -1);
    CHECK_EQ_INT(
        g->world.b20j.ray_previous_x, prepared.initial->b20j.ray_previous_x);
    CHECK(g->bindings.display.player == &g->world.player);
    CHECK(g->bindings.c05.hit == &g->bindings.hit);
    CHECK(g->bindings.c10j.rng == &g->world.rng);
    CHECK(g->bindings.c20m.input == &g->world.input);
    CHECK(g->bindings.c20m.assets == &f.assets->b20m);
    Gameplay_BindAudio(g, nullptr);
    GameAudio_Free(output);
    Audio_Shutdown();
    Arena_Free(&f.arena);
}

TEST(public_prepare_rejects_invalid_borrowed_assets_with_empty_output)
{
    ARENA arena;
    Arena_Init(&arena, "invalid gameplay group", 8 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&arena);
    GAMEPLAY_ASSETS *const assets = Th01TestAssets_Create(allocator);
    VIDEO video;
    Video_Init(&video, allocator);
    SESSION session = {};
    assets->b05.sphere.count = 0;
    GAMEPLAY *game;
    CHECK(!Result_Ignore(
        Gameplay_Prepare(allocator, assets, &video, &session, &game)));
    CHECK(game == nullptr);
    Arena_Free(&arena);
}
