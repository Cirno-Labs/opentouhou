#include "../src/th01/vsync_irq.c"
#include "../src/th01/stage_objects.c"
#include "th01/stage_objects.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

#include <string.h>

typedef struct {
    ARENA arena;
    PTN sprites;
    STAGE_DATA_STAGE layouts[5];
    VIDEO saved_video;
    VSYNC_COUNTERS saved_vsync;
    STAGE_OBJECTS saved_objects;
} CASE;

static CASE *m_Active;

static void M_Init(CASE *const state)
{
    *state = (CASE) {};
    Th01Test_ResetWorld();
    g_Rng = 1;
    Arena_Init(&state->arena, "stage objects", 2 * 1024 * 1024);
    Video_Init(Arena_Allocator(&state->arena));
    state->sprites =
        (PTN) { .count = 34, .pixels = Arena_Alloc(&state->arena, 34 * 1024) };
    memset(state->sprites.pixels, PTN_TRANSPARENT, 34 * 1024);
    for (int32_t image = 0; image < 34; image++) {
        state->sprites.pixels[image * 1024] = image % 15;
    }
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        g_Video.pages[0][i] = 14;
        g_Video.pages[1][i] = i % 13;
    }
    state->saved_video = g_Video;
}

// Switches the video and VSync module state between two cases.
static void M_Use(CASE *const state)
{
    if (m_Active != nullptr) {
        m_Active->saved_video = g_Video;
        m_Active->saved_vsync = *g_VSync;
        m_Active->saved_objects = m_StageObjects;
    }
    g_Video = state->saved_video;
    m_VSync = state->saved_vsync;
    m_StageObjects = state->saved_objects;
    m_Active = state;
}

static void M_Begin(CASE *const state, const bool first)
{
    const STAGE_DATA data = { .stages = state->layouts };
    StageObjects_Init(
        &data, 0, first, Arena_Allocator(&state->arena), &state->sprites);
}

TEST(boss_returns_before_allocation_rng_or_access)
{
    memset(&m_StageObjects, 0, sizeof(m_StageObjects));
    m_StageObjects.card_count = 20;
    m_StageObjects.obstacle_count = 9;
    StageObjects_Init(nullptr, 4, false, nullptr, nullptr);
    CHECK_EQ_INT(m_StageObjects.card_count, 0);
    CHECK_EQ_INT(m_StageObjects.obstacle_count, 0);
    CHECK(m_StageObjects.entrance_done);
    CHECK(m_StageObjects.backgrounds == nullptr);
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
    Video_SelectDrawingPage(1);
    uint32_t rng = g_Rng;
    MasterRng_Next(&rng);
    M_Begin(&state, true);
    CHECK_EQ_INT(g_Rng, rng);
    CHECK_EQ_INT(m_StageObjects.card_count, 2);
    CHECK_EQ_INT(m_StageObjects.obstacle_count, 2);
    CHECK_EQ_INT(m_StageObjects.cards[0].left, 96);
    CHECK_EQ_INT(m_StageObjects.cards[0].hp, 2);
    CHECK_EQ_INT(m_StageObjects.cards[1].top, 96);
    CHECK_EQ_INT(m_StageObjects.obstacles[0].left, 32);
    CHECK_EQ_INT(m_StageObjects.obstacles[1].left, 96);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 96, 64), 11);
    CHECK_EQ_INT(Th01Test_Pixel(0, 97, 64), 14);
    for (int32_t slot = 0; slot < 4; slot++) {
        const int32_t left[] = { 96, 0, 32, 96 };
        const int32_t top[] = { 64, 96, 64, 64 };
        for (int32_t y = 0; y < 32; y++) {
            for (int32_t x = 0; x < 32; x++) {
                CHECK_EQ_INT(
                    m_StageObjects.backgrounds[slot * 1024 + y * 32 + x],
                    ((top[slot] + y) * 640 + left[slot] + x) % 13);
            }
        }
    }
    const size_t used = state.arena.used;
    Video_SelectDrawingPage(1);
    StageObjects_PutBackgroundAndObject(&state.sprites, 96, 64, 17, 0);
    CHECK_EQ_INT(Th01Test_Pixel(1, 96, 64), 2);
    CHECK_EQ_INT(Th01Test_Pixel(1, 97, 64), (64 * 640 + 97) % 13);
    CHECK_EQ_INT(Th01Test_Pixel(0, 96, 64), 11);
    StageObjects_PutAllBackgrounds();
    CHECK_EQ_INT(Th01Test_Pixel(1, 96, 64), (64 * 640 + 96) % 13);
    StageObjects_CopyToBackground();
    CHECK_EQ_INT(Th01Test_Pixel(1, 96, 64), 11);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
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
            VSyncIrq_Reset();
            m_VSync.count1 = raw_counts[timing];
            m_VSync.count2 = 42;
            int32_t edges = 0;
            while (!StageObjects_ResumeEntrance(&state.sprites)
                   && edges < 240) {
                const int32_t total = m_StageObjects.entrance_total;
                CHECK(!StageObjects_ResumeEntrance(&state.sprites));
                CHECK_EQ_INT(m_StageObjects.entrance_total, total);
                VSyncIrq_Edge();
                edges++;
            }
            const int32_t delays =
                counts[variant] == 0 ? 0 : counts[variant] + 13;
            CHECK_EQ_INT(
                edges, delays - (delays != 0 && raw_counts[timing] != 0));
            CHECK_EQ_INT(state.arena.used, used);
            CHECK_EQ_INT(g_VSync->count2, 42 + edges);
            CHECK_EQ_INT(g_VSync->count1, delays == 0 ? raw_counts[timing] : 0);
            for (int32_t i = 0; i < counts[variant]; i++) {
                const STAGE_CARD *const card = &m_StageObjects.cards[i];
                CHECK_EQ_INT(card->entrance_frame, -1);
                CHECK_EQ_INT(
                    Th01Test_Pixel(0, card->left, card->top),
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
    m_StageObjects.destination_left = 256;
    m_StageObjects.destination_top = 192;
    m_StageObjects.entered_portal = 4;
    m_StageObjects.portal_active = true;
    m_StageObjects.vertical_bar_active = true;
    M_Begin(&state, true);
    CHECK_EQ_INT(m_StageObjects.destination_left, 256);
    CHECK_EQ_INT(m_StageObjects.destination_top, 192);
    CHECK_EQ_INT(m_StageObjects.entered_portal, 4);
    CHECK(m_StageObjects.portal_active);
    CHECK(m_StageObjects.vertical_bar_active);
    StageObjects_Init(nullptr, 4, false, nullptr, nullptr);
    CHECK_EQ_INT(m_StageObjects.card_count, 0);
    CHECK_EQ_INT(m_StageObjects.destination_left, 256);
    CHECK_EQ_INT(m_StageObjects.destination_top, 192);
    CHECK_EQ_INT(m_StageObjects.entered_portal, 4);
    CHECK(m_StageObjects.portal_active);
    CHECK(m_StageObjects.vertical_bar_active);
    Arena_Free(&state.arena);
}

TEST(prepared_storage_matches_allocated_stages_without_growth)
{
    CASE allocated;
    CASE prepared;
    m_Active = nullptr;
    M_Init(&allocated);
    M_Init(&prepared);
    StageObjects_Prepare(Arena_Allocator(&prepared.arena));
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
        M_Use(&allocated);
        m_StageObjects.destination_left = 256;
        const uint32_t rng = g_Rng;
        StageObjects_Init(
            &a, (int16_t)stage, stage % 2 == 0,
            Arena_Allocator(&allocated.arena), &allocated.sprites);
        const uint32_t allocated_rng = g_Rng;
        const STAGE_OBJECTS allocated_objects = m_StageObjects;
        g_Rng = rng;
        M_Use(&prepared);
        m_StageObjects.destination_left = 256;
        StageObjects_InitPrepared(
            &p, (int16_t)stage, stage % 2 == 0, &prepared.sprites);
        CHECK_EQ_INT(prepared.arena.used, used);
        CHECK_EQ_INT(g_Rng, allocated_rng);
        CHECK_EQ_INT(m_StageObjects.destination_left, 256);
        CHECK_EQ_INT(m_StageObjects.card_count, allocated_objects.card_count);
        CHECK_EQ_INT(
            m_StageObjects.obstacle_count, allocated_objects.obstacle_count);
        if (stage % 5 != 4) {
            CHECK_EQ_INT(
                memcmp(
                    m_StageObjects.cards, allocated_objects.cards,
                    (size_t)m_StageObjects.card_count * sizeof(STAGE_CARD)),
                0);
            CHECK_EQ_INT(
                memcmp(
                    m_StageObjects.obstacles, allocated_objects.obstacles,
                    (size_t)m_StageObjects.obstacle_count
                        * sizeof(STAGE_OBSTACLE)),
                0);
            CHECK_EQ_INT(
                memcmp(
                    m_StageObjects.backgrounds, allocated_objects.backgrounds,
                    (size_t)(m_StageObjects.card_count
                             + m_StageObjects.obstacle_count)
                        * 1024),
                0);
        }
        allocated.saved_vsync = (VSYNC_COUNTERS) {};
        VSyncIrq_Reset();
        for (;;) {
            M_Use(&allocated);
            if (StageObjects_ResumeEntrance(&allocated.sprites)) {
                break;
            }
            VSyncIrq_Edge();
            M_Use(&prepared);
            CHECK(!StageObjects_ResumeEntrance(&prepared.sprites));
            VSyncIrq_Edge();
        }
        M_Use(&prepared);
        CHECK(StageObjects_ResumeEntrance(&prepared.sprites));
        CHECK_EQ_INT(
            memcmp(
                allocated.saved_video.pages[0], g_Video.pages[0],
                VIDEO_PAGE_SIZE),
            0);
        CHECK_EQ_INT(
            memcmp(
                allocated.saved_video.pages[1], g_Video.pages[1],
                VIDEO_PAGE_SIZE),
            0);
    }
    Arena_Free(&allocated.arena);
    Arena_Free(&prepared.arena);
}
