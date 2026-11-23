#include "th01/timer.h"
#include "pc98/zun/master_rng.h"
#include "core/base/arena.h"
#include "test.h"

typedef struct {
    ARENA arena;
    VIDEO video;
    HUD hud;
    HUD_CONTEXT display;
    TIMER_CONTEXT context;
    STAGE_TIMER timer;
    PLAYER player;
    STAGE_GAMEPLAY gameplay;
    GRF mask;
    PTN sprites;
    GAME_TEXT_FONT font;
    TRAM tram;
    FRAME_VSYNC vsync;
    PELLETS pellets;
    PELLET_SHARED shared;
    uint32_t rng, high_score;
    int16_t lives;
    int32_t effects;
} FIXTURE;

static void M_Effect(void *const user, const int16_t number)
{
    FIXTURE *const f = user;
    CHECK_EQ_INT(number, 17);
    CHECK_EQ_INT(f->shared.speed, 2);
    CHECK_EQ_INT(f->video.drawing_page, 0);
    CHECK_EQ_INT(f->timer.time, 0);
    CHECK_EQ_INT(f->timer.phase, TIMER_IDLE);
    f->effects++;
}

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) { .rng = 1 };
    Arena_Init(&f->arena, "timer", 2 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(&f->video, a);
    Hud_Init(&f->hud, a);
    Tram_Init(&f->tram, a);
    GAME_TEXT_GLYPH glyphs[105] = {};
    for (int32_t i = 0; i < 105; i++) {
        glyphs[i].codepoint = (uint32_t)(i < 95 ? i + 32 : i - 95 + 0xff10);
        glyphs[i].width = i < 95 ? 8 : 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = i == 0 ? 0 : 0x8000;
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 105, a, &f->font)));
    f->mask.coverage = Allocator_AllocZ(a, 640 * 66);
    f->sprites = (PTN) { .count = 1, .pixels = Allocator_AllocZ(a, 1024) };
    f->display = (HUD_CONTEXT) { .video = &f->video,
                                 .mask = &f->mask,
                                 .sprites = &f->sprites,
                                 .font = &f->font,
                                 .gameplay = &f->gameplay,
                                 .player = &f->player,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives,
                                 .stage_number = 1,
                                 .rank = SETTINGS_DIFFICULTY_NORMAL };
    f->context = (TIMER_CONTEXT) { .hud = &f->hud,
                                   .display = &f->display,
                                   .tram = &f->tram,
                                   .vsync = &f->vsync,
                                   .pellets = &f->pellets,
                                   .shared = &f->shared,
                                   .rng = &f->rng,
                                   .effect = M_Effect,
                                   .user = f };
    Hud_SnapTimer(&f->hud, &f->display, 1000);
}

TEST(route_table_and_reinitialization_keep_cycle)
{
    FIXTURE f;
    M_Init(&f);
    f.timer.hurry_cycle = 255;
    const uint16_t expected[2][20] = {
        { 1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 3000,
          1400, 1400, 1400, 1400, 4000, 1600, 1600, 1600, 600,  6000 },
        { 1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 2000,
          1400, 1400, 1400, 1400, 4000, 1600, 1800, 1800, 1600, 8000 }
    };
    for (int16_t route = 0; route < 2; route++) {
        for (int16_t stage = 0; stage < 20; stage++) {
            f.timer.frames_since_hurry = 555;
            f.display.stage_number = (uint16_t)(stage + 1);
            Timer_Redraw(&f.timer, &f.context, false, false, route);
            CHECK_EQ_INT(f.timer.time, expected[route][stage]);
            CHECK_EQ_INT(f.timer.hurry_cycle, 255);
            CHECK_EQ_INT(f.timer.frames_since_hurry, 0);
        }
    }
    Arena_Free(&f.arena);
}

TEST(transition_waits_suspend_without_replaying_side_effects)
{
    FIXTURE f;
    M_Init(&f);
    f.timer.time = 2;
    CHECK(!Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.effects, 1);
    CHECK_EQ_INT(f.timer.phase, TIMER_WAIT_HURRY);
    CHECK_EQ_INT(f.tram.pixels[80 * 640], TRAM_RED + 1);
    for (int32_t i = 0; i < 26; i++) {
        Frame_VSync(&f.vsync);
        CHECK(!Timer_TickAndPut(&f.timer, &f.context));
    }
    Frame_VSync(&f.vsync);
    CHECK(!Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.timer.phase, TIMER_WAIT_UP);
    CHECK_EQ_INT(f.vsync.count1, 0);
    CHECK_EQ_INT(f.tram.pixels[80 * 640], TRAM_BLACK + 1);
    CHECK_EQ_INT(f.tram.pixels[80 * 640 + 192], TRAM_RED + 1);
    for (int32_t i = 0; i < 27; i++) {
        Frame_VSync(&f.vsync);
    }
    CHECK(Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.effects, 1);
    CHECK_EQ_INT(f.shared.speed, 2);
    CHECK_EQ_INT(f.timer.frames_since_hurry, 0);
    CHECK_EQ_INT(f.timer.hurry_cycle, 0);
    CHECK_EQ_INT(f.tram.pixels[80 * 640 + 192], 0);
    Arena_Free(&f.arena);
}

TEST(ready_count_is_consumed_before_first_suspension_and_extension_wraps)
{
    FIXTURE f;
    M_Init(&f);
    f.timer.time = 2;
    f.vsync.count1 = 31;
    CHECK(!Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.timer.phase, TIMER_WAIT_UP);
    CHECK_EQ_INT(f.vsync.count1, 0);
    f.vsync.count1 = 27;
    CHECK(Timer_TickAndPut(&f.timer, &f.context));
    f.timer.hurry_cycle = 81;
    f.timer.frames_since_hurry = 500;
    Timer_ExtendAndPut(&f.timer, &f.context);
    CHECK_EQ_INT(f.timer.time, 1000);
    CHECK_EQ_INT(f.timer.frames_since_hurry, 0);
    CHECK_EQ_INT(f.timer.hurry_cycle, 81);
    f.timer.time = 65500;
    Timer_ExtendAndPut(&f.timer, &f.context);
    CHECK_EQ_INT(f.timer.time, 164);
    f.timer.time = 1;
    CHECK(Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.timer.time, 65535);
    CHECK_EQ_INT(f.effects, 1);
    Arena_Free(&f.arena);
}

TEST(full_pool_consumes_x_only_and_cycle_and_frame_wrap)
{
    FIXTURE f;
    M_Init(&f);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        f.pellets.slots[i].moving = true;
    }
    f.display.rank = SETTINGS_DIFFICULTY_LUNATIC;
    uint32_t expected = f.rng;
    f.timer.hurry_cycle = 255;
    f.timer.frames_since_hurry = 65535;
    for (int32_t i = 0; i < 4; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.timer.frames_since_hurry, 0);
    CHECK_EQ_INT(f.timer.hurry_cycle, 0);
    CHECK_EQ_INT(f.rng, expected);
    CHECK(Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.rng, expected);
    f.timer.frames_since_hurry = 499;
    for (int32_t i = 0; i < 4; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(Timer_TickAndPut(&f.timer, &f.context));
    CHECK_EQ_INT(f.rng, expected);
    Arena_Free(&f.arena);
}

TEST(redraw_does_not_claim_world_timer_initialization)
{
    FIXTURE f;
    M_Init(&f);
    f.timer.hurry_cycle = 97;
    f.display.stage_number = 20;
    Timer_Redraw(&f.timer, &f.context, true, false, 1);
    CHECK_EQ_INT(f.timer.time, 8000);
    CHECK_EQ_INT(f.timer.hurry_cycle, 97);
    f.timer.time = 750;
    Timer_Redraw(&f.timer, &f.context, false, true, 0);
    CHECK_EQ_INT(f.timer.time, 750);
    CHECK(!f.hud.force_numbers);
    Timer_Redraw(&f.timer, &f.context, false, false, 0);
    CHECK_EQ_INT(f.timer.time, 6000);
    Arena_Free(&f.arena);
}
