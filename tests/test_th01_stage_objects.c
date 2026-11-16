#include "th01/stage_objects.h"

#include "th01/math.h"

#include "core/base/arena.h"

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
    Math_Random(&rng);
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
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 96, 64), 11);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 97, 64), 14);
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
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 1, 96, 64), 2);
    CHECK_EQ_INT(
        Video_ReadPixel(&state.video, 1, 97, 64), (64 * 640 + 97) % 13);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 96, 64), 11);
    StageObjects_PutAllBackgrounds(&state.objects, &state.video);
    CHECK_EQ_INT(
        Video_ReadPixel(&state.video, 1, 96, 64), (64 * 640 + 96) % 13);
    StageObjects_CopyToBackground(&state.objects, &state.video);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 1, 96, 64), 11);
    CHECK_EQ_INT(state.video.drawing_page, 1);
    CHECK_EQ_INT(state.arena.used, used);
    Arena_Free(&state.arena);
}

TEST(entrance_preserves_source_delay_phase_and_safe_large_counts)
{
    const int32_t counts[] = { 0, 1, 43, 76, 200 };
    for (int32_t variant = 0; variant < 5; variant++) {
        CASE state;
        M_Init(&state);
        for (int32_t i = 0; i < counts[variant]; i++) {
            state.layouts[0].cells[i / 20][i % 20].card = true;
            state.layouts[0].cells[i / 20][i % 20].card_extra_hp = i % 4;
        }
        M_Begin(&state, false);
        const size_t used = state.arena.used;
        FRAME_VSYNC vsync = { .count1 = -1, .count2 = 42 };
        CHECK_EQ_INT(
            StageObjects_ResumeEntrance(
                &state.objects, &state.video, &state.sprites, &vsync),
            counts[variant] == 0);
        CHECK_EQ_INT(vsync.count1, -1);
        int32_t iterations = 0;
        while (!state.objects.entrance_done && iterations < 240) {
            if (iterations > 0) {
                const int32_t total = state.objects.entrance_total;
                CHECK(!StageObjects_ResumeEntrance(
                    &state.objects, &state.video, &state.sprites, &vsync));
                CHECK_EQ_INT(state.objects.entrance_total, total);
                Frame_VSync(&vsync);
            }
            StageObjects_ResumeEntrance(
                &state.objects, &state.video, &state.sprites, &vsync);
            iterations++;
        }
        CHECK_EQ_INT(
            iterations, counts[variant] == 0 ? 0 : counts[variant] + 13);
        CHECK_EQ_INT(state.arena.used, used);
        CHECK_EQ_INT(
            vsync.count2, counts[variant] == 0 ? 42 : counts[variant] + 54);
        for (int32_t i = 0; i < counts[variant]; i++) {
            const STAGE_CARD *const card = &state.objects.cards[i];
            CHECK_EQ_INT(card->entrance_frame, -1);
            CHECK_EQ_INT(
                Video_ReadPixel(&state.video, 0, card->left, card->top),
                StageObjects_CardImage(card->hp, 0) % 15);
        }
        Arena_Free(&state.arena);
    }
}
