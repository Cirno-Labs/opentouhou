#include "th01/ending_internal.h"
#include "core/base/arena.h"
#include "test.h"

#include <string.h>

typedef struct {
    ARENA arena;
    ENDING_ASSETS assets;
    ENDING *ending;
    VIDEO video;
    FRAME_VSYNC vsync;
    GAME_INPUT input;
    SESSION session;
    int32_t audio[4];
} FIXTURE;

static void M_Audio(void *const context, const ENDING_AUDIO command)
{
    FIXTURE *const f = context;
    f->audio[command]++;
}
static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Arena_Init(&f->arena, "ending", 2 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(&f->video, a);
    uint8_t *const pixels = Allocator_Alloc(a, VIDEO_PAGE_SIZE);
    for (int32_t y = 0; y < 400; y++) {
        for (int32_t x = 0; x < 640; x++) {
            pixels[y * 640 + x] = (uint8_t)(1 + x / 320 + (y / 200) * 2);
        }
    }
    for (int32_t i = 0; i < ENDING_SHEET_COUNT; i++) {
        f->assets.sheets[i].pixels = pixels;
        for (int32_t c = 0; c < 16; c++) {
            f->assets.sheets[i].palette[c] = (GFX_COLOR) { 255, 170, 85, 255 };
        }
    }
    Ending_Prepare(
        a, &f->assets, &f->video, &f->vsync, &f->input, M_Audio, f, &f->ending);
    f->arena.capacity = f->arena.used;
    f->session =
        (SESSION) { .score = 2345678,
                    .highest_score = 3000000,
                    .exit_rand = 123,
                    .settings = { .difficulty = 1, .lives_extra = 2 } };
}

TEST(snapshot_copies_bits_recomputes_total_and_clears_history)
{
    SESSION session = { .score = 0xffffffff,
                        .highest_score = 0x80000000,
                        .frame_rand = 999,
                        .exit_rand = 1234,
                        .ending = SESSION_END_JIGOKU,
                        .continues_total = -100,
                        .continues_per_scene = { 65535, 32768, 7, 9 },
                        .stage_id = 19,
                        .high_score = 888,
                        .table_high_score = 999,
                        .settings = { .difficulty = 3, .lives_extra = 4 },
                        .lives = 7 };
    END_RESULT r;
    Ending_Snapshot(&r, &session);
    CHECK_EQ_INT(r.score, 0xffffffff);
    CHECK_EQ_INT(r.highest_score, 0x80000000);
    CHECK_EQ_INT(r.random, 1234);
    CHECK_EQ_INT(r.total, 98319);
    CHECK_EQ_INT(r.continues[0], 65535);
    CHECK_EQ_INT(session.score, 0);
    CHECK_EQ_INT(session.highest_score, 0);
    CHECK_EQ_INT(session.continues_total, 0);
    CHECK_EQ_INT(session.continues_per_scene[1], 0);
    CHECK_EQ_INT(session.ending, SESSION_END_JIGOKU);
    CHECK_EQ_INT(session.stage_id, 19);
    Ending_Clear(&session);
    CHECK_EQ_INT(session.ending, SESSION_END_NONE);
    CHECK_EQ_INT(session.stage_id, 0);
    CHECK_EQ_INT(session.high_score, 888);
    CHECK_EQ_INT(session.table_high_score, 999);
    CHECK_EQ_INT(session.frame_rand, 999);
    CHECK_EQ_INT(session.exit_rand, 1234);
    CHECK_EQ_INT(session.lives, 7);
}

TEST(results_use_signed_scores_and_all_source_levels)
{
    const int32_t scores[] = { -1,      499999,  500000,  999999,
                               1000000, 1499999, 1500000, 1999999,
                               2000000, 2499999, 2500000 };
    const int32_t expected[] = { 0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10 };
    for (int32_t i = 0; i < 11; i++) {
        END_RESULT r = { .total = 12, .rank = 1, .extra_lives = 2 };
        memcpy(&r.score, &scores[i], 4);
        Ending_CalculateTitle(&r);
        CHECK_EQ_INT(r.skill, expected[i] + 10);
        CHECK_EQ_INT(r.random, 1);
    }
    int32_t levels = 0, groups = 0;
    for (uint8_t rank = 0; rank < 4; rank++) {
        for (uint32_t total = 0; total <= 24; total++) {
            END_RESULT r = { .rank = rank,
                             .total = total,
                             .random = total * 123456789u,
                             .extra_lives = 0 };
            Ending_CalculateTitle(&r);
            CHECK(r.level >= 0 && r.level < 6);
            CHECK(r.group >= 0 && r.group < 3);
            levels |= 1 << r.level;
            groups |= 1 << r.group;
        }
    }
    CHECK_EQ_INT(levels, 63);
    CHECK_EQ_INT(groups, 7);
}

TEST(all_four_stories_preserve_source_wait_totals_and_complete)
{
    for (int32_t route = 1; route <= 2; route++) {
        for (int32_t bad = 0; bad < 2; bad++) {
            FIXTURE f;
            M_Init(&f);
            f.session.ending = (SESSION_END)route;
            f.session.continues_per_scene[2] = (uint16_t)bad;
            f.session.continues_total = 100;
            Ending_Begin(f.ending, &f.session);
            int32_t duration = 0, stage5_nuls = 0, story_end = 0;
            for (int32_t i = 0; i < f.ending->count; i++) {
                const ENDING_OPERATION *const op = &f.ending->operations[i];
                if (op->kind == END_RESULT_LINES) {
                    story_end = i - 40;
                    break;
                }
                if (op->kind == END_TEXT && op->text[0] == '\0') {
                    stage5_nuls++;
                }
            }
            for (int32_t i = 0; i < story_end; i++) {
                const ENDING_OPERATION *const op = &f.ending->operations[i];
                if (op->kind == END_WAIT) {
                    duration += op->value;
                }
            }
            CHECK_EQ_INT(
                duration,
                route == 1 ? (bad != 0 ? 3202 : 6174)
                           : (bad != 0 ? 2910 : 5528));
            CHECK_EQ_INT(stage5_nuls, bad != 0 ? 0 : 2);
            CHECK(!Ending_Tick(f.ending, &(TH01_INPUT) {}, false));
            CHECK_EQ_INT(f.video.pages[0][100 * 640 + 160], 1);
            CHECK_EQ_INT(f.video.pages[0][0], 0);
            CHECK_EQ_INT(f.video.drawing_page, 0);
            CHECK_EQ_INT(f.audio[ENDING_AUDIO_IRIS], 1);
            const int32_t next = f.ending->next;
            for (int32_t i = 0; i < 100; i++) {
                CHECK(!Ending_Tick(f.ending, &(TH01_INPUT) {}, false));
            }
            CHECK_EQ_INT(f.ending->next, next);
            bool done = false;
            for (int32_t i = 0; i < 12000 && !done; i++) {
                done =
                    Ending_Tick(f.ending, &(TH01_INPUT) { .ok = true }, true);
            }
            CHECK(done);
            CHECK_EQ_INT(f.audio[ENDING_AUDIO_FADE], 1);
            CHECK_EQ_INT(f.audio[ENDING_AUDIO_STOP], 1);
            CHECK_EQ_INT(f.audio[ENDING_AUDIO_STAGE1], 1);
            CHECK_EQ_INT(f.video.palette[3].green, 255);
            CHECK_EQ_INT(f.video.palette[3].red, 119);
            CHECK_EQ_INT(f.video.palette[7].red, 255);
            CHECK_EQ_INT(f.video.scroll_top, 0);
            CHECK_EQ_INT(f.session.score, 0);
            Arena_Free(&f.arena);
        }
    }
}

TEST(entry_wait_resets_at_start_and_retains_elapsed_count)
{
    FIXTURE f;
    M_Init(&f);
    f.session.ending = SESSION_END_MAKAI;
    Ending_Begin(f.ending, &f.session);
    f.vsync.count1 = 300;
    CHECK(!Ending_Tick(f.ending, &(TH01_INPUT) {}, false));
    CHECK_EQ_INT(f.vsync.count1, 0);
    for (int32_t i = 0; i < 5; i++) {
        CHECK(!Ending_Tick(f.ending, &(TH01_INPUT) {}, true));
    }
    CHECK_EQ_INT(f.vsync.count1, 5);
    CHECK(!Ending_Tick(f.ending, &(TH01_INPUT) {}, true));
    CHECK_EQ_INT(f.vsync.count1, 0);
    f.ending->operations[0] =
        (ENDING_OPERATION) { .kind = END_WAIT, .value = 0 };
    f.ending->count = 1;
    f.ending->next = 0;
    f.ending->waiting = false;
    f.vsync.count1 = 999;
    CHECK(Ending_Tick(f.ending, &(TH01_INPUT) {}, false));
    CHECK_EQ_INT(f.vsync.count1, 0);
    Arena_Free(&f.arena);
}

TEST(results_confirmation_is_sensed_inside_its_first_wait)
{
    FIXTURE f;
    M_Init(&f);
    f.session.ending = SESSION_END_MAKAI;
    Ending_Begin(f.ending, &f.session);
    f.ending->operations[0] = (ENDING_OPERATION) { .kind = END_INPUT };
    f.ending->count = 1;
    CHECK(!Ending_Tick(f.ending, &(TH01_INPUT) { .ok = true }, false));
    CHECK_EQ_INT(f.ending->timeout, 1);
    CHECK_EQ_INT(f.vsync.count1, 0);
    CHECK(!Ending_Tick(f.ending, &(TH01_INPUT) { .ok = true }, false));
    CHECK(Ending_Tick(f.ending, &(TH01_INPUT) { .ok = true }, true));
    CHECK_EQ_INT(f.vsync.count1, 1);
    Arena_Free(&f.arena);
}
