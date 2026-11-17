#include "th01/gameplay_internal.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    ARENA arena;
    GAMEPLAY game;
    GAMEPLAY_ASSETS assets;
    SESSION session;
    VIDEO video;
    STAGE_DATA_STAGE layouts[5];
    PELLET_ASSETS pellets;
    SHOOTOUT_LASERS_ASSETS lasers;
    MDRV_TRACK track;
    MDRV_EFFECT effects[17];
    uint8_t music_bytes[1], effect_bytes[118];
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Arena_Init(&f->arena, "gameplay", 8 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(&f->video, a);
    GAMEPLAY *const g = &f->game;
    g->assets = &f->assets;
    g->video = &f->video;
    g->session = &f->session;
    f->session = (SESSION) {
        .lives = 4,
        .bombs = 1,
        .settings = { .difficulty = 1, .bombs = 1, .lives_extra = 2 }
    };
    f->assets.stage =
        (PTN) { .count = 34, .pixels = Allocator_Alloc(a, 34 * 1024) };
    memset(f->assets.stage.pixels, PTN_TRANSPARENT, 34 * 1024);
    f->assets.boss_stage = f->assets.stage;
    f->assets.miko = f->assets.stage;
    f->assets.hud.coverage = Allocator_AllocZ(a, 640 * 66);
    f->assets.pellets = &f->pellets;
    f->assets.lasers = &f->lasers;
    GAME_TEXT_GLYPH glyphs[342] = {};
    for (uint32_t i = 0; i < 95; i++) {
        glyphs[i] = (GAME_TEXT_GLYPH) { .codepoint = i + 32, .width = 8 };
    }
    for (uint32_t i = 0; i < 240; i++) {
        glyphs[95 + i] = (GAME_TEXT_GLYPH) {
            .codepoint = 0xff00 + i,
            .width = i >= 0x61 && i <= 0x9f ? 8 : 16,
        };
    }
    const uint32_t title[] = { 0x6771, 0x65b9, 0x2605, 0x9748, 0x7570, 0x4f1d };
    for (int32_t i = 0; i < 6; i++) {
        glyphs[335 + i] =
            (GAME_TEXT_GLYPH) { .codepoint = title[i], .width = 16 };
    }
    glyphs[341] = (GAME_TEXT_GLYPH) { .codepoint = 0x3000, .width = 16 };
    CHECK(IS_OK(GameText_LoadGlyphs(glyphs, 342, a, &f->assets.hud_font)));
    f->assets.menu_font = f->assets.hud_font;
    f->assets.item_font = f->assets.hud_font;
    f->assets.bonus_font = f->assets.hud_font;
    for (int32_t i = 0; i < 7; i++) {
        f->assets.scenes[i].layouts.stages = f->layouts;
        f->assets.backgrounds[i].pixels = Allocator_AllocZ(a, VIDEO_PAGE_SIZE);
    }
    for (int32_t i = 0; i < 5; i++) {
        f->layouts[i].cells[0][0].card = true;
    }
    const uint8_t effect[] = { 0xeb, 0x80, 0xec, 15, 0x48, 8, 0xff };
    memcpy(f->effect_bytes + 88, effect, sizeof(effect));
    const uint8_t envelope[] = { 128, 64, 16, 96, 3, 7 };
    memcpy(f->effect_bytes + 112, envelope, sizeof(envelope));
    for (int32_t i = 0; i < 17; i++) {
        f->effects[i] = (MDRV_EFFECT) { .channel_count = 1,
                                        .base_channel = 0x40,
                                        .stream_offsets = { 88 } };
    }
    f->assets.effects = (MDRV_EFFECTS) { .bytes = f->effect_bytes,
                                         .size = 118,
                                         .ssg_offset = 112,
                                         .ssg_count = 1,
                                         .effects = f->effects,
                                         .effect_count = 17 };
    f->track = (MDRV_TRACK) {};
    for (int32_t i = 0; i < GAMEPLAY_SONG_COUNT; i++) {
        f->assets.songs[i] = (MDRV_MUSIC) { .bytes = f->music_bytes,
                                            .size = 1,
                                            .tracks = &f->track,
                                            .track_count = 1 };
    }
    StageObjects_Prepare(&g->storage, a);
    Hud_Init(&g->hud, a);
    HudHp_Init(&g->health, a);
    Tram_Init(&g->tram, a);
    CHECK(IS_OK(MdrvSynth_Init(
        f->assets.songs, GAMEPLAY_SONG_COUNT, &f->assets.effects, a, nullptr,
        nullptr, &g->audio)));
    Gameplay_ResetWorld(g);
    memset(&g->vsync, 0, sizeof(g->vsync));
    StageObjects_InitPrepared(
        &g->objects, &f->assets.scenes[0].layouts, 0, true, &g->storage,
        g->video, &f->assets.stage, &g->rng);
    Orb_ResetFlight(&g->orb);
    g->timer.time = 1000;
    g->phase = GP_FRAME_BEGIN;
    g->timer_initialized = true;
    g->first_stage = false;
    g->invincibility.frame[0] = 255;
}

TEST(ordinary_loop_consumes_real_input_and_suspends_after_motion)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = &f.game;
    const size_t used = f.arena.used;
    TH01_INPUT raw = { .right = true, .shot = true };
    CHECK_EQ_INT(Gameplay_Tick(g, &raw, false, 1), GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(f.session.frame_rand, 1);
    CHECK_EQ_INT(g->frame, 1);
    CHECK_EQ_INT(g->orb.force_frame, 1);
    CHECK_EQ_INT(g->player.left, 308);
    CHECK_EQ_INT(g->orb.left, 596);
    CHECK_EQ_INT(g->phase, GP_ORB);
    CHECK(g->composite.waiting);
    const PLAYER player = g->player;
    const ORB orb = g->orb;
    const uint32_t rng = g->rng;
    Gameplay_Tick(g, &raw, false, 2);
    CHECK_EQ_INT(memcmp(&player, &g->player, sizeof(player)), 0);
    CHECK_EQ_INT(memcmp(&orb, &g->orb, sizeof(orb)), 0);
    CHECK_EQ_INT(g->rng, rng);
    Gameplay_Tick(g, &raw, true, 3);
    CHECK_EQ_INT(f.session.frame_rand, 2);
    CHECK_EQ_INT(g->frame, 2);
    CHECK_EQ_INT(g->orb.force_frame, 2);
    Gameplay_Tick(g, &raw, true, 4);
    CHECK(g->shots.slots[0].moving);
    CHECK(MdrvSynth_Status(g->audio).effects_playing);
    CHECK_EQ_INT(f.arena.used, used);
    Arena_Free(&f.arena);
}

TEST(hurry_wait_freezes_the_remaining_iteration_prefix)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = &f.game;
    f.session.frame_rand = 3;
    g->timer.time = 2;
    g->player.invincibility_time = 2;
    TH01_INPUT raw = {};
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->phase, GP_TIMER);
    CHECK_EQ_INT(g->timer.phase, TIMER_WAIT_HURRY);
    CHECK_EQ_INT(g->frame, 1);
    CHECK_EQ_INT(g->player.invincibility_time, 1);
    CHECK_EQ_INT(g->orb.left, 600);
    const uint32_t rng = g->rng;
    const PLAYER player = g->player;
    for (uint64_t i = 2; i < 12; i++) {
        Gameplay_Tick(g, &raw, false, i);
        CHECK_EQ_INT(g->frame, 1);
        CHECK_EQ_INT(f.session.frame_rand, 4);
        CHECK_EQ_INT(g->rng, rng);
        CHECK_EQ_INT(memcmp(&player, &g->player, sizeof(player)), 0);
    }
    Arena_Free(&f.arena);
}

TEST(real_shot_boundary_advances_player_before_timer_and_rng_seed)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = &f.game;
    g->phase = GP_WAIT_SHOT;
    g->wait_for_shot = true;
    g->timer_initialized = false;
    g->input = (GAME_INPUT) {};
    f.session.frame_rand = 42;
    TH01_INPUT raw = { .shot = true, .left = true };
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->phase, GP_WAIT_SHOT_DELAY);
    CHECK_EQ_INT(g->player.left, 300);
    CHECK(!g->timer_initialized);
    CHECK_EQ_INT(f.session.frame_rand, 42);
    Gameplay_Tick(g, &raw, true, 2);
    CHECK(g->timer_initialized);
    CHECK(!g->wait_for_shot);
    CHECK_EQ_INT(g->frame, 1);
    CHECK_EQ_INT(f.session.frame_rand, 43);
    CHECK_EQ_INT(g->player.bomb_frame, 2);
    Arena_Free(&f.arena);
}

TEST(stage_clear_beats_miss_and_starts_real_bonus)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = &f.game;
    BONUS_STAGE_ASSETS stage = {};
    f.assets.bonus = &stage;
    g->bonus_context.stage = &stage;
    g->phase = GP_FRAME_BEGIN;
    g->player.is_hit = true;
    g->metrics.cleared = true;
    f.session.lives = 0;
    g->timer.time = 500;
    TH01_INPUT raw = {};
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->phase, GP_LIFE_EXIT);
    Gameplay_Tick(g, &raw, true, 2);
    CHECK_EQ_INT(g->phase, GP_STAGE_BONUS);
    CHECK_EQ_INT(f.session.stage_id, 1);
    CHECK_EQ_INT(f.session.lives, 0);
    CHECK(!g->player.is_hit);
    CHECK_EQ_INT(g->miss.phase, MISS_DONE);
    Arena_Free(&f.arena);
}

TEST(victory_handoff_keeps_destroy_points_uncommitted)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = &f.game;
    g->phase = GP_AFTER_ORB;
    g->game_cleared = true;
    g->metrics.score = 1000;
    g->pellet_shared.destroy_score_delta = 70;
    f.session.route = SESSION_ROUTE_JIGOKU;
    TH01_INPUT raw = {};
    Gameplay_Tick(g, &raw, false, 1);
    CHECK_EQ_INT(g->phase, GP_VICTORY_DELAY);
    CHECK_EQ_INT(f.session.score, 1000);
    CHECK_EQ_INT(f.session.highest_score, 1000);
    CHECK_EQ_INT(f.session.ending, SESSION_END_JIGOKU);
    for (uint64_t tick = 2; tick < 121; tick++) {
        CHECK_EQ_INT(Gameplay_Tick(g, &raw, true, tick), GAMEPLAY_REQUEST_NONE);
    }
    CHECK_EQ_INT(Gameplay_Tick(g, &raw, true, 121), GAMEPLAY_REQUEST_ENDING);
    CHECK_EQ_INT(g->pellet_shared.destroy_score_delta, 70);
    CHECK_EQ_INT(g->metrics.score, 1000);
    Arena_Free(&f.arena);
}

TEST(real_card_consumer_runs_bonus_and_reuses_stage_storage)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = &f.game;
    BONUS_STAGE_ASSETS bonus = {};
    f.assets.bonus = &bonus;
    Gameplay_Bind(g);
    f.arena.capacity = f.arena.used;
    const STAGE_CARD *const cards = g->storage.cards;
    const STAGE_OBSTACLE *const obstacles = g->storage.obstacles;
    TH01_INPUT raw = {};
    uint64_t tick = 0;
    for (int16_t stage = 0; stage < 3; stage++) {
        CHECK_EQ_INT(f.session.stage_id, stage);
        g->orb.left = g->objects.cards[0].left;
        g->orb.top = g->objects.cards[0].top;
        Stage_HitTestCards(&g->stage);
        CHECK_EQ_INT(g->objects.cards[0].flag, STAGE_CARD_FLIPPING);
        CHECK(g->metrics.score > 0);
        for (int32_t i = 0; i < 30; i++) {
            Stage_UpdateCards(&g->stage);
        }
        CHECK(g->metrics.cleared);
        CHECK(g->player.is_hit);
        bool entered_bonus = false;
        for (int32_t i = 0; i < 1000; i++) {
            raw.shot = i % 20 < 10;
            Gameplay_Tick(g, &raw, true, ++tick);
            entered_bonus |= g->phase == GP_STAGE_BONUS;
            if (f.session.stage_id == stage + 1 && g->phase == GP_ORB) {
                break;
            }
        }
        CHECK(entered_bonus);
        CHECK_EQ_INT(f.session.stage_id, stage + 1);
        CHECK_EQ_INT(g->phase, GP_ORB);
        CHECK(g->storage.cards == cards);
        CHECK(g->storage.obstacles == obstacles);
        CHECK_EQ_INT(g->objects.card_count, 1);
        CHECK(!g->metrics.cleared);
        CHECK_EQ_INT(g->miss.phase, MISS_DONE);
    }
    Arena_Free(&f.arena);
}

TEST(miss_gameover_and_score_handoff_keep_prepared_storage)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const g = &f.game;
    f.arena.capacity = f.arena.used;
    const TH01_INPUT raw = {};
    uint64_t tick = 0;
    g->player.is_hit = true;
    bool entered_miss = false;
    for (int32_t i = 0; i < 1000; i++) {
        Gameplay_Tick(g, &raw, true, ++tick);
        entered_miss |= g->phase == GP_MISS;
        if (entered_miss && g->phase == GP_ORB) {
            break;
        }
    }
    CHECK(entered_miss);
    CHECK_EQ_INT(f.session.lives, 3);
    CHECK(g->player.invincible);
    CHECK(!g->player.is_hit);
    f.session.lives = 0;
    g->player.is_hit = true;
    g->phase = GP_FRAME_BEGIN;
    for (int32_t i = 0; i < 200; i++) {
        if (Gameplay_Tick(g, &raw, true, ++tick) == GAMEPLAY_REQUEST_SCORE) {
            break;
        }
    }
    CHECK_EQ_INT(g->request, GAMEPLAY_REQUEST_SCORE);
    CHECK_EQ_INT(g->phase, GP_TERMINAL);
    uint8_t before[VIDEO_PAGE_SIZE];
    memcpy(before, f.video.pages[0], sizeof(before));
    for (int32_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(
            Gameplay_Tick(g, &raw, true, ++tick), GAMEPLAY_REQUEST_SCORE);
    }
    CHECK_EQ_INT(memcmp(before, f.video.pages[0], sizeof(before)), 0);
    Gameplay_ResumeAfterScore(g);
    CHECK_EQ_INT(g->phase, GP_CONTINUE);
    CHECK_EQ_INT(f.session.continues_total, 1);
    CHECK_EQ_INT(f.session.lives, 4);
    Gameplay_Tick(g, &(TH01_INPUT) { .shot = true }, true, ++tick);
    CHECK_EQ_INT(g->request, GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(g->phase, GP_TITLE);
    CHECK_EQ_INT(f.session.stage_id, 0);
    CHECK(g->storage.cards == f.game.objects.cards);
    Arena_Free(&f.arena);
}

TEST(gameover_restores_defined_adjacent_array_values)
{
    const int16_t lefts[] = { 0, 80, 88 };
    const int16_t frames[] = { 12, 10, 11 };
    for (int32_t test = 0; test < 3; test++) {
        FIXTURE f;
        M_Init(&f);
        GAMEPLAY *const g = &f.game;
        f.arena.capacity = f.arena.used;
        memset(f.video.pages[0], 9, VIDEO_PAGE_SIZE);
        memset(f.video.pages[1], 3, VIDEO_PAGE_SIZE);
        const int16_t frame = frames[test];
        const int16_t left = lefts[test];
        g->gameover_left[0] = g->gameover_left[1] = left - frame * 8;
        g->gameover_left[2] = left;
        g->gameover_left[3] = g->gameover_left[4] = left + frame * 8;
        for (int32_t i = 0; i < 5; i++) {
            g->gameover_clipped[i] = g->gameover_left[i] < 0;
            g->gameover_top[i] = PLAYER_TOP;
        }
        g->animation_frame = frame;
        g->phase = GP_GAMEOVER;
        Frame_BeginWait(&g->wait, &g->vsync);
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
    GAMEPLAY *const g = &f.game;
    f.arena.capacity = f.arena.used;
    memset(f.assets.stage.pixels + ORB_FIRST_IMAGE * 1024, 6, 1024);
    g->player.is_hit = true;
    f.session.lives = 0;
    Gameplay_Tick(g, &(TH01_INPUT) {}, false, 1);
    Gameplay_Tick(g, &(TH01_INPUT) {}, true, 2);
    CHECK_EQ_INT(g->phase, GP_GAMEOVER);
    CHECK_EQ_INT(f.video.pages[1][g->orb.top * VIDEO_WIDTH + g->orb.left], 6);
    Arena_Free(&f.arena);
}
