#include "th01/stage_objects.h"

#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

typedef struct {
    ARENA arena;
    VIDEO video;
    PTN sprites;
    STAGE_DATA_STAGE layouts[5];
    STAGE_OBJECTS objects;
    uint32_t rng;
} CASE;

static void M_Init(CASE *const state)
{
    *state = (CASE) { .rng = 1 };
    Arena_Init(&state->arena, "stage objects", 2 * 1024 * 1024);
    Video_Init(&state->video, Arena_Allocator(&state->arena));
    state->sprites =
        (PTN) { .count = 34, .pixels = Arena_Alloc(&state->arena, 34 * 1024) };
    memset(state->sprites.pixels, PTN_TRANSPARENT, 34 * 1024);
    for (int32_t image = 0; image < 34; image++) {
        state->sprites.pixels[image * 1024] = image % 15;
    }
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        state->video.pages[0][i] = 14;
        state->video.pages[1][i] = i % 13;
    }
}

static void M_Begin(CASE *const state, const bool first)
{
    const STAGE_DATA data = { .stages = state->layouts };
    StageObjects_Init(
        &state->objects, &data, 0, first, Arena_Allocator(&state->arena),
        &state->video, &state->sprites, &state->rng);
}

TEST(boss_returns_before_allocation_rng_or_access)
{
    STAGE_OBJECTS objects = { .card_count = 20, .obstacle_count = 9 };
    StageObjects_Init(
        &objects, nullptr, 4, false, nullptr, nullptr, nullptr, nullptr);
    CHECK_EQ_INT(objects.card_count, 0);
    CHECK_EQ_INT(objects.obstacle_count, 0);
    CHECK(objects.entrance_done);
    CHECK(objects.backgrounds == nullptr);
    PTN sprites = { .count = 7, .pixels = (uint8_t *)"x" };
    CHECK(IS_OK(StageObjects_ValidateSprites(&sprites, 4)));
    RESULT result = StageObjects_ValidateSprites(&sprites, 0);
    CHECK(!Result_Ignore(result));
}

TEST(enumeration_snapshots_composition_and_copy_pages)
{
    CASE state;
    M_Init(&state);
    state.layouts[0].cells[0][3] = (STAGE_DATA_CELL) {
        .card = true, .card_extra_hp = 2, .obstacle = STAGE_DATA_BUMPER
    };
    state.layouts[0].cells[1][0].card = true;
    state.layouts[0].cells[0][1].obstacle = STAGE_DATA_PORTAL;
    Video_SelectDrawingPage(&state.video, 1);
    uint32_t rng = state.rng;
    MasterRng_Next(&rng);
    M_Begin(&state, true);
    CHECK_EQ_INT(state.rng, rng);
    CHECK_EQ_INT(state.objects.card_count, 2);
    CHECK_EQ_INT(state.objects.obstacle_count, 2);
    CHECK_EQ_INT(state.objects.cards[0].left, 96);
    CHECK_EQ_INT(state.objects.cards[0].hp, 2);
    CHECK_EQ_INT(state.objects.cards[1].top, 96);
    CHECK_EQ_INT(state.objects.obstacles[0].left, 32);
    CHECK_EQ_INT(state.objects.obstacles[1].left, 96);
    CHECK_EQ_INT(state.video.drawing_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 96, 64), 11);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 97, 64), 14);
    for (int32_t slot = 0; slot < 4; slot++) {
        const int32_t left[] = { 96, 0, 32, 96 };
        const int32_t top[] = { 64, 96, 64, 64 };
        for (int32_t y = 0; y < 32; y++) {
            for (int32_t x = 0; x < 32; x++) {
                CHECK_EQ_INT(
                    state.objects.backgrounds[slot * 1024 + y * 32 + x],
                    ((top[slot] + y) * 640 + left[slot] + x) % 13);
            }
        }
    }
    const size_t used = state.arena.used;
    Video_SelectDrawingPage(&state.video, 1);
    StageObjects_PutBackgroundAndObject(
        &state.objects, &state.video, &state.sprites, 96, 64, 17, 0);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 1, 96, 64), 2);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 1, 97, 64), (64 * 640 + 97) % 13);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 0, 96, 64), 11);
    StageObjects_PutAllBackgrounds(&state.objects, &state.video);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 1, 96, 64), (64 * 640 + 96) % 13);
    StageObjects_CopyToBackground(&state.objects, &state.video);
    CHECK_EQ_INT(Th01Test_Pixel(&state.video, 1, 96, 64), 11);
    CHECK_EQ_INT(state.video.drawing_page, 0);
    CHECK_EQ_INT(state.arena.used, used);
    Arena_Free(&state.arena);
}

TEST(entrance_consumes_ready_delays_and_preserves_blocked_polls)
{
    const int32_t counts[] = { 0, 1, 43, 76, 200 };
    const int16_t raw_counts[] = { 0, 1, 7, -1 };
    for (int32_t variant = 0; variant < 5; variant++) {
        for (int32_t timing = 0; timing < 4; timing++) {
            CASE state;
            M_Init(&state);
            for (int32_t i = 0; i < counts[variant]; i++) {
                state.layouts[0].cells[i / 20][i % 20].card = true;
                state.layouts[0].cells[i / 20][i % 20].card_extra_hp = i % 4;
            }
            M_Begin(&state, false);
            const size_t used = state.arena.used;
            FRAME_VSYNC vsync = { .count1 = raw_counts[timing], .count2 = 42 };
            int32_t edges = 0;
            while (!StageObjects_ResumeEntrance(
                       &state.objects, &state.video, &state.sprites, &vsync)
                   && edges < 240) {
                const int32_t total = state.objects.entrance_total;
                CHECK(!StageObjects_ResumeEntrance(
                    &state.objects, &state.video, &state.sprites, &vsync));
                CHECK_EQ_INT(state.objects.entrance_total, total);
                Frame_VSync(&vsync);
                edges++;
            }
            const int32_t delays =
                counts[variant] == 0 ? 0 : counts[variant] + 13;
            CHECK_EQ_INT(
                edges, delays - (delays != 0 && raw_counts[timing] != 0));
            CHECK_EQ_INT(state.arena.used, used);
            CHECK_EQ_INT(vsync.count2, 42 + edges);
            CHECK_EQ_INT(vsync.count1, delays == 0 ? raw_counts[timing] : 0);
            for (int32_t i = 0; i < counts[variant]; i++) {
                const STAGE_CARD *const card = &state.objects.cards[i];
                CHECK_EQ_INT(card->entrance_frame, -1);
                CHECK_EQ_INT(
                    Th01Test_Pixel(&state.video, 0, card->left, card->top),
                    StageObjects_CardImage(card->hp, 0) % 15);
            }
            Arena_Free(&state.arena);
        }
    }
}

TEST(stage_reloads_keep_static_obstacle_history)
{
    CASE state;
    M_Init(&state);
    state.layouts[0].cells[0][0].card = true;
    state.objects.destination_left = 256;
    state.objects.destination_top = 192;
    state.objects.entered_portal = 4;
    state.objects.portal_active = true;
    state.objects.vertical_bar_active = true;
    M_Begin(&state, true);
    CHECK_EQ_INT(state.objects.destination_left, 256);
    CHECK_EQ_INT(state.objects.destination_top, 192);
    CHECK_EQ_INT(state.objects.entered_portal, 4);
    CHECK(state.objects.portal_active);
    CHECK(state.objects.vertical_bar_active);
    StageObjects_Init(
        &state.objects, nullptr, 4, false, nullptr, nullptr, nullptr, nullptr);
    CHECK_EQ_INT(state.objects.card_count, 0);
    CHECK_EQ_INT(state.objects.destination_left, 256);
    CHECK_EQ_INT(state.objects.destination_top, 192);
    CHECK_EQ_INT(state.objects.entered_portal, 4);
    CHECK(state.objects.portal_active);
    CHECK(state.objects.vertical_bar_active);
    Arena_Free(&state.arena);
}

TEST(prepared_storage_matches_allocated_stages_without_growth)
{
    CASE allocated;
    CASE prepared;
    M_Init(&allocated);
    M_Init(&prepared);
    STAGE_OBJECT_STORAGE storage;
    StageObjects_Prepare(&storage, Arena_Allocator(&prepared.arena));
    const size_t used = prepared.arena.used;
    for (int32_t stage = 0; stage < 5; stage++) {
        for (int32_t y = 0; y < 10; y++) {
            for (int32_t x = 0; x < 20; x++) {
                const STAGE_DATA_CELL cell = {
                    .card = (x + y + stage) % 3 == 0,
                    .card_extra_hp = (uint8_t)((x + stage) % 4),
                    .obstacle = (x + y) % 5 == 0 ? STAGE_DATA_PORTAL
                                                 : STAGE_DATA_OBSTACLE_NONE,
                };
                allocated.layouts[stage].cells[y][x] = cell;
                prepared.layouts[stage].cells[y][x] = cell;
            }
        }
    }
    for (int32_t stage = 0; stage < 15; stage++) {
        const STAGE_DATA a = { .stages = allocated.layouts };
        const STAGE_DATA p = { .stages = prepared.layouts };
        allocated.objects.destination_left = 256;
        prepared.objects.destination_left = 256;
        StageObjects_Init(
            &allocated.objects, &a, (int16_t)stage, stage % 2 == 0,
            Arena_Allocator(&allocated.arena), &allocated.video,
            &allocated.sprites, &allocated.rng);
        StageObjects_InitPrepared(
            &prepared.objects, &p, (int16_t)stage, stage % 2 == 0, &storage,
            &prepared.video, &prepared.sprites, &prepared.rng);
        CHECK_EQ_INT(prepared.arena.used, used);
        CHECK_EQ_INT(prepared.rng, allocated.rng);
        CHECK_EQ_INT(prepared.objects.destination_left, 256);
        CHECK_EQ_INT(prepared.objects.card_count, allocated.objects.card_count);
        CHECK_EQ_INT(
            prepared.objects.obstacle_count, allocated.objects.obstacle_count);
        if (stage % 5 != 4) {
            CHECK_EQ_INT(
                memcmp(
                    prepared.objects.cards, allocated.objects.cards,
                    (size_t)prepared.objects.card_count * sizeof(STAGE_CARD)),
                0);
            CHECK_EQ_INT(
                memcmp(
                    prepared.objects.obstacles, allocated.objects.obstacles,
                    (size_t)prepared.objects.obstacle_count
                        * sizeof(STAGE_OBSTACLE)),
                0);
            CHECK_EQ_INT(
                memcmp(
                    prepared.objects.backgrounds, allocated.objects.backgrounds,
                    (size_t)(prepared.objects.card_count
                             + prepared.objects.obstacle_count)
                        * 1024),
                0);
        }
        FRAME_VSYNC av = {};
        FRAME_VSYNC pv = {};
        while (!StageObjects_ResumeEntrance(
            &allocated.objects, &allocated.video, &allocated.sprites, &av)) {
            CHECK(!StageObjects_ResumeEntrance(
                &prepared.objects, &prepared.video, &prepared.sprites, &pv));
            Frame_VSync(&av);
            Frame_VSync(&pv);
        }
        CHECK(StageObjects_ResumeEntrance(
            &prepared.objects, &prepared.video, &prepared.sprites, &pv));
        CHECK_EQ_INT(
            memcmp(
                allocated.video.pages[0], prepared.video.pages[0],
                VIDEO_PAGE_SIZE),
            0);
        CHECK_EQ_INT(
            memcmp(
                allocated.video.pages[1], prepared.video.pages[1],
                VIDEO_PAGE_SIZE),
            0);
    }
    Arena_Free(&allocated.arena);
    Arena_Free(&prepared.arena);
}
