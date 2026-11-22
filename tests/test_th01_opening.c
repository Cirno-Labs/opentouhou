#include "th01/opening.h"

#include "core/base/arena.h"

#include "test.h"

static void M_Init(
    ARENA *const arena, VIDEO *const video, OPENING_ASSETS *const assets,
    TH01_SETTINGS *const settings, OPENING *const opening)
{
    Arena_Init(arena, "opening test", 4 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    Video_Init(video, allocator);
    *assets = (OPENING_ASSETS) {};
    PI_IMAGE *const images[] = { &assets->title, &assets->revealed,
                                 &assets->window };
    for (int32_t i = 0; i < 3; i++) {
        *images[i] = (PI_IMAGE) { .width = VIDEO_WIDTH,
                                  .height = VIDEO_HEIGHT,
                                  .pixels = Allocator_Alloc(
                                      allocator, VIDEO_PAGE_SIZE) };
        memset(images[i]->pixels, i == 2 ? 15 : i + 1, VIDEO_PAGE_SIZE);
        images[i]->palette[1] = (GFX_COLOR) { 255, (uint8_t)(i * 17), 0, 255 };
    }
    assets->window.pixels[0] = 7;
    const GAME_TEXT_GLYPH glyphs[] = {
        { .codepoint = 0xff33, .width = 16, .rows = { 0x4000 } },
        { .codepoint = 0xff23, .width = 16, .rows = { 0x2000 } },
        { .codepoint = 0xff32, .width = 16, .rows = { 0x1000 } },
        { .codepoint = 0xff28, .width = 16, .rows = { 0x0800 } },
    };
    CHECK(IS_OK(GameText_LoadGlyphs(glyphs, 4, allocator, &assets->font)));
    *settings =
        (TH01_SETTINGS) { .difficulty = 1, .music = 0, .lives_extra = 0 };
    const OPENING_EVENTS events =
        Opening_Init(opening, video, settings, assets);
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_PLAY);
    CHECK_EQ_INT(events.track, 0);
    CHECK(events.clear_key_buffer);
}

static OPENING_EVENTS M_Tick(OPENING *const opening, const TH01_INPUT input)
{
    return Opening_Tick(opening, &input, false);
}

static void M_Main(OPENING *const opening)
{
    const TH01_INPUT input = {};
    for (int32_t i = 0; i < 200; i++) {
        Opening_Tick(opening, &input, false);
    }
    CHECK_EQ_INT(opening->phase, OPENING_PROMPT);
    Opening_Tick(opening, &input, true);
    CHECK_EQ_INT(opening->phase, OPENING_MAIN);
}

static void M_Options(OPENING *const opening)
{
    M_Main(opening);
    M_Tick(opening, (TH01_INPUT) { .down = true });
    M_Tick(opening, (TH01_INPUT) {});
    M_Tick(opening, (TH01_INPUT) { .down = true, .ok = true });
    CHECK_EQ_INT(opening->phase, OPENING_OPTIONS);
    M_Tick(opening, (TH01_INPUT) {});
    CHECK_EQ_INT(opening->selection, 0);
}

TEST(title_shares_fade_and_hold_counter_then_draws_unique_rows)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    CHECK_EQ_INT(video.pages[0][0], 1);
    CHECK_EQ_INT(video.pages[1][0], 2);
    CHECK_EQ_INT(video.palette[1].red, 0);
    CHECK_EQ_INT(opening.palette.saved.colors[1][1], 0);
    CHECK_EQ_INT(opening.palette.resource.colors[1][1], 1);
    const size_t used = arena.used;
    uint64_t model_random = 1;
    bool model_rows[400] = {};
    for (int32_t tick = 1; tick <= 200; tick++) {
        Opening_Tick(&opening, &(TH01_INPUT) {}, false);
        if (tick == 16) {
            CHECK_EQ_INT(opening.phase, OPENING_HOLD);
            CHECK_EQ_INT(video.palette[1].red, 255);
            CHECK_EQ_INT(video.palette[1].green, 0);
        }
        if (tick == 99) {
            CHECK_EQ_INT(opening.line_count, 0);
        }
        if (tick == 100) {
            CHECK_EQ_INT(opening.line_count, 1);
            CHECK_EQ_INT(opening.line_y[0], 346);
            CHECK_EQ_INT(opening.line_y[1], 130);
        }
        if (tick >= 100 && tick < 200) {
            for (int32_t i = 0; i < 2; i++) {
                int32_t y;
                do {
                    model_random =
                        (model_random * 22695477 + 1) % UINT64_C(4294967296);
                    y = (int32_t)((model_random / 65536) % 32768) % 400;
                } while (model_rows[y]);
                model_rows[y] = true;
                CHECK_EQ_INT(opening.line_y[i], y);
            }
            CHECK_EQ_INT(opening.random, model_random);
            int32_t whites = 0;
            for (int32_t y = 0; y < VIDEO_HEIGHT; y++) {
                whites += Video_ReadPixel(&video, 0, 639, y) == 15;
            }
            CHECK_EQ_INT(whites, 2);
        }
    }
    CHECK_EQ_INT(opening.phase, OPENING_PROMPT);
    CHECK_EQ_INT(opening.prompt_frame, 0);
    int32_t rows = 0;
    for (int32_t y = 0; y < VIDEO_HEIGHT; y++) {
        rows += opening.drawn_rows[y];
    }
    CHECK_EQ_INT(rows, 200);
    CHECK(memcmp(video.pages[0], video.pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(opening.frame_rand, 0);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(prompt_blinks_after_wait_and_window_keeps_saved_palette)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    for (int32_t i = 0; i < 200; i++) {
        M_Tick(&opening, (TH01_INPUT) {});
    }
    for (int32_t i = 0; i < 70; i++) {
        M_Tick(&opening, (TH01_INPUT) {});
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, 256, 306), i < 50 ? 15 : 2);
    }
    CHECK_EQ_INT(opening.prompt_frame, 0);
    Opening_Tick(&opening, &(TH01_INPUT) {}, true);
    CHECK_EQ_INT(opening.phase, OPENING_MAIN);
    CHECK_EQ_INT(opening.random, 0);
    CHECK_EQ_INT(opening.frame_rand, 1);
    CHECK_EQ_INT(video.pages[0][0], 7);
    CHECK_EQ_INT(video.pages[1][0], 7);
    CHECK_EQ_INT(opening.palette.resource.colors[1][1], 2);
    CHECK_EQ_INT(video.palette[1].green, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 269, 276), 15);
    Arena_Free(&arena);
}

TEST(settings_wrap_and_horizontal_locks_persist_across_selections)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    M_Options(&opening);
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(settings.difficulty, 0);
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(settings.difficulty, 0);
    M_Tick(&opening, (TH01_INPUT) {});
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(settings.difficulty, 3);
    M_Tick(&opening, (TH01_INPUT) {});
    M_Tick(&opening, (TH01_INPUT) { .left = true, .right = true });
    CHECK_EQ_INT(settings.difficulty, 3);
    M_Tick(&opening, (TH01_INPUT) { .down = true, .right = true });
    CHECK_EQ_INT(settings.music, 0);
    M_Tick(&opening, (TH01_INPUT) {});
    M_Tick(&opening, (TH01_INPUT) { .right = true });
    CHECK_EQ_INT(settings.music, 1);
    M_Tick(&opening, (TH01_INPUT) { .down = true });
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(settings.lives_extra, 4);
    Arena_Free(&arena);
}

TEST(music_entry_suppresses_one_confirmation_and_shares_options_history)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    M_Options(&opening);
    for (int32_t i = 0; i < 3; i++) {
        M_Tick(&opening, (TH01_INPUT) { .down = true });
        M_Tick(&opening, (TH01_INPUT) {});
    }
    M_Tick(&opening, (TH01_INPUT) { .ok = true });
    CHECK_EQ_INT(opening.phase, OPENING_MUSIC);
    OPENING_EVENTS events =
        M_Tick(&opening, (TH01_INPUT) { .ok = true, .left = true });
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_NONE);
    CHECK_EQ_INT(opening.track, 14);
    events = M_Tick(&opening, (TH01_INPUT) { .ok = true, .left = true });
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_PLAY);
    CHECK_EQ_INT(events.track, 14);
    CHECK(strcmp(Opening_TrackFile(events.track), "iris.mdt") == 0);
    M_Tick(&opening, (TH01_INPUT) { .cancel = true, .ok = true });
    CHECK_EQ_INT(opening.phase, OPENING_WAIT_OPTIONS);
    CHECK_EQ_INT(opening.selection, 3);
    const uint32_t frame_rand = opening.frame_rand;
    for (int32_t i = 0; i < 14; i++) {
        events = M_Tick(&opening, (TH01_INPUT) { .down = true });
        CHECK_EQ_INT(opening.frame_rand, frame_rand);
        CHECK(!events.clear_key_buffer);
    }
    events = M_Tick(&opening, (TH01_INPUT) { .down = true });
    CHECK_EQ_INT(opening.phase, OPENING_OPTIONS);
    CHECK_EQ_INT(opening.frame_rand, frame_rand + 1);
    CHECK(events.clear_key_buffer);
    M_Tick(&opening, (TH01_INPUT) { .down = true });
    CHECK_EQ_INT(opening.selection, 0);
    CHECK_EQ_INT(opening.phase, OPENING_OPTIONS);
    Arena_Free(&arena);
}

static uint32_t M_HeldStarts(const bool fix)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    settings.fix_debouncing = fix;
    M_Options(&opening);
    for (int32_t i = 0; i < 3; i++) {
        M_Tick(&opening, (TH01_INPUT) { .down = true });
        M_Tick(&opening, (TH01_INPUT) {});
    }
    M_Tick(&opening, (TH01_INPUT) { .ok = true });
    CHECK_EQ_INT(opening.phase, OPENING_MUSIC);
    for (int32_t i = 0; i < 5; i++) {
        M_Tick(&opening, (TH01_INPUT) { .shot = true });
    }
    M_Tick(&opening, (TH01_INPUT) {});
    const OPENING_EVENTS events = M_Tick(&opening, (TH01_INPUT) { .ok = true });
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_PLAY);
    M_Tick(&opening, (TH01_INPUT) { .ok = true });
    const uint32_t starts = opening.track_starts;
    Arena_Free(&arena);
    return starts;
}

TEST(debouncing_fix_plays_once_per_confirm_press)
{
    CHECK_EQ_INT(M_HeldStarts(true), 2);
    CHECK_EQ_INT(M_HeldStarts(false), 6);
}

TEST(option_exit_applies_changed_music_then_waits_fifteen_plus_tail)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    M_Options(&opening);
    settings.music = 1;
    M_Tick(&opening, (TH01_INPUT) { .cancel = true });
    const uint32_t frame_rand = opening.frame_rand;
    OPENING_EVENTS events = M_Tick(&opening, (TH01_INPUT) {});
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_PLAY);
    CHECK_EQ_INT(events.track, 0);
    for (int32_t i = 1; i < 15; i++) {
        M_Tick(&opening, (TH01_INPUT) {});
    }
    CHECK_EQ_INT(opening.phase, OPENING_MAIN);
    CHECK_EQ_INT(opening.frame_rand, frame_rand + 1);
    CHECK_EQ_INT(opening.selection, 2);
    M_Tick(&opening, (TH01_INPUT) {});
    CHECK_EQ_INT(opening.frame_rand, frame_rand + 2);
    CHECK_EQ_INT(opening.selection, 2);
    M_Tick(&opening, (TH01_INPUT) { .ok = true });
    M_Tick(&opening, (TH01_INPUT) {});
    settings.music = 0;
    M_Tick(&opening, (TH01_INPUT) { .cancel = true });
    events = M_Tick(&opening, (TH01_INPUT) {});
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_STOP);
    Arena_Free(&arena);
}

TEST(start_fades_to_black_then_emits_its_boundary)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    M_Main(&opening);
    const uint32_t frame_rand = opening.frame_rand;
    CHECK_EQ_INT(video.palette[1].red, 255);
    OPENING_EVENTS events =
        M_Tick(&opening, (TH01_INPUT) { .ok = true, .cancel = true });
    CHECK_EQ_INT(events.action, OPENING_ACTION_NONE);
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_FADE);
    CHECK_EQ_INT(opening.phase, OPENING_EXIT_FADE);
    CHECK_EQ_INT(opening.frame_rand, frame_rand);
    for (int32_t tick = 1; tick <= 15; tick++) {
        events = M_Tick(&opening, (TH01_INPUT) { .ok = true });
        CHECK_EQ_INT(events.action, OPENING_ACTION_NONE);
        CHECK_EQ_INT(video.palette[1].red, tick < 15 ? (15 - tick) * 17 : 0);
    }
    events = M_Tick(&opening, (TH01_INPUT) {});
    CHECK_EQ_INT(events.action, OPENING_ACTION_START);
    CHECK_EQ_INT(video.palette[1].red, 0);
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_NONE);
    CHECK_EQ_INT(opening.frame_rand, frame_rand);
    CHECK_EQ_INT(opening.count1, 0);
    Arena_Free(&arena);
}

TEST(continue_fades_and_cancel_wins_over_options)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    M_Main(&opening);
    M_Tick(&opening, (TH01_INPUT) { .down = true });
    M_Tick(&opening, (TH01_INPUT) { .shot = true });
    CHECK_EQ_INT(opening.phase, OPENING_EXIT_FADE);
    for (int32_t tick = 0; tick < 15; tick++) {
        M_Tick(&opening, (TH01_INPUT) {});
    }
    OPENING_EVENTS events = M_Tick(&opening, (TH01_INPUT) {});
    CHECK_EQ_INT(events.action, OPENING_ACTION_CONTINUE);
    Arena_Free(&arena);
    M_Init(&arena, &video, &assets, &settings, &opening);
    M_Main(&opening);
    for (int32_t i = 0; i < 2; i++) {
        M_Tick(&opening, (TH01_INPUT) { .down = true });
        M_Tick(&opening, (TH01_INPUT) {});
    }
    events = M_Tick(&opening, (TH01_INPUT) { .ok = true, .cancel = true });
    CHECK_EQ_INT(events.action, OPENING_ACTION_QUIT);
    CHECK_EQ_INT(events.audio, OPENING_AUDIO_STOP);
    CHECK_EQ_INT(opening.phase, OPENING_QUIT);
    Arena_Free(&arena);
}

TEST(horizontal_locks_ignore_releases_in_other_menus_and_waits)
{
    ARENA arena;
    VIDEO video;
    OPENING_ASSETS assets;
    TH01_SETTINGS settings;
    OPENING opening;
    M_Init(&arena, &video, &assets, &settings, &opening);
    M_Options(&opening);
    const size_t used = arena.used;
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(settings.difficulty, 0);
    for (int32_t i = 0; i < 3; i++) {
        M_Tick(&opening, (TH01_INPUT) { .down = true, .left = true });
        M_Tick(&opening, (TH01_INPUT) { .left = true });
    }
    M_Tick(&opening, (TH01_INPUT) { .ok = true, .left = true });
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(opening.track, 14);
    M_Tick(&opening, (TH01_INPUT) { .cancel = true });
    for (int32_t i = 0; i < 15; i++) {
        M_Tick(&opening, (TH01_INPUT) {});
    }
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(opening.selection, 0);
    CHECK_EQ_INT(settings.difficulty, 0);
    M_Tick(&opening, (TH01_INPUT) {});
    M_Tick(&opening, (TH01_INPUT) { .left = true });
    CHECK_EQ_INT(settings.difficulty, 3);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(missing_archive_reports_the_canonical_name)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    const RESULT result = Opening_MountArchive(vfs);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "東方靈異.伝") != nullptr);
    Result_Ignore(result);
    Vfs_Destroy(vfs);
}
