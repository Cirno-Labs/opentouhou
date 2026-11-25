#include "../src/th01/particles.c"
#include "th01/particles.h"
#include "th01/rng.h"
#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

#include <string.h>

TEST(initialize_preserves_scene_history)
{
    Th01Test_ResetWorld();
    memset(&m_Particles, 0, sizeof(m_Particles));
    m_Particles.cycle = 79;
    m_Particles.x[0] = -123;
    m_Particles.velocity_y[1] = -16;
    m_Particles.velocity_base[0] = 7;
    memset(m_Particles.alive, 255, sizeof(m_Particles.alive));
    Particles_Init();
    CHECK_EQ_INT(m_Particles.spawn_interval, 2);
    CHECK_EQ_INT(m_Particles.velocity_max, 10);
    CHECK_EQ_INT(m_Particles.cycle, 79);
    CHECK_EQ_INT(m_Particles.x[0], -123);
    CHECK_EQ_INT(m_Particles.velocity_y[1], -16);
    CHECK_EQ_INT(m_Particles.velocity_base[0], 7);
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        CHECK_EQ_INT(m_Particles.alive[i], 0);
    }
}

TEST(first_spawn_skips_zero_and_uses_shared_randomness)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Video_SelectDrawingPage(1);
    memset(&m_Particles, 0, sizeof(m_Particles));
    Particles_Init();
    g_Rng = 1;
    Particles_UpdateAndRender(PARTICLES_TOP, 7);
    CHECK_EQ_INT(g_Rng, 1);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    uint32_t expected_rng = g_Rng;
    const int16_t speed = (int16_t)(MasterRng_Next(&expected_rng) % 10 + 1);
    const int16_t left = (int16_t)(MasterRng_Next(&expected_rng) % 640);
    Particles_UpdateAndRender(PARTICLES_TOP, 7);
    CHECK_EQ_INT(g_Rng, expected_rng);
    CHECK_EQ_INT(m_Particles.alive[0], 0);
    CHECK_EQ_INT(m_Particles.alive[1], 1);
    CHECK_EQ_INT(m_Particles.x[1], left * 16);
    CHECK_EQ_INT(m_Particles.y[1], speed * 16);
    CHECK_EQ_INT(Th01Test_Pixel(0, left, speed), 7);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(first_overflow_preserves_negative_velocity_aliases)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(&m_Particles, 0, sizeof(m_Particles));
    m_Particles.cycle = 79;
    Particles_Init();
    m_Particles.x[0] = 1000;
    m_Particles.x[1] = 3200;
    m_Particles.y[1] = 1600;
    g_Rng = 1;
    uint32_t expected_rng = g_Rng;
    const int16_t speed = (int16_t)(MasterRng_Next(&expected_rng) % 10 + 1);
    const int16_t left = (int16_t)(MasterRng_Next(&expected_rng) % 640);
    Particles_UpdateAndRender(PARTICLES_BOTTOM, 10);
    CHECK_EQ_INT(g_Rng, expected_rng);
    CHECK_EQ_INT(m_Particles.cycle, speed);
    CHECK_EQ_INT(m_Particles.velocity_base[0], 1);
    CHECK_EQ_INT(m_Particles.velocity_x[0], 399 * 16);
    CHECK_EQ_INT(m_Particles.velocity_y[0], 0);
    CHECK_EQ_INT(m_Particles.y[0], left * 16);
    CHECK_EQ_INT(m_Particles.x[0], 1000 + 399 * 16);
    CHECK_EQ_INT(m_Particles.alive[1], 255);
    CHECK_EQ_INT(Th01Test_Pixel(0, 200, 100), 10);
    m_Particles.cycle = 79;
    Particles_UpdateAndRender(PARTICLES_TOP, 7);
    CHECK_EQ_INT(g_Rng, expected_rng);
    CHECK_EQ_INT(m_Particles.cycle, 80);
    Arena_Free(&arena);
}

TEST(leaving_particle_restores_old_word_before_dying)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
    memset(&m_Particles, 0, sizeof(m_Particles));
    Particles_Init();
    m_Particles.alive[0] = 255;
    m_Particles.x[0] = 639 * 16;
    m_Particles.y[0] = 100 * 16;
    m_Particles.velocity_x[0] = 16;
    g_Rng = 1;
    Particles_UpdateAndRender(PARTICLES_TOP, 10);
    CHECK_EQ_INT(m_Particles.alive[0], 0);
    CHECK_EQ_INT(m_Particles.x[0], 640 * 16);
    CHECK_EQ_INT(Th01Test_Pixel(0, 623, 100), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 624, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 101), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 15, 101), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 101), 7);
    Arena_Free(&arena);
}

TEST(motion_wraps_before_bounds_test)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(&m_Particles, 0, sizeof(m_Particles));
    Particles_Init();
    m_Particles.alive[0] = 1;
    m_Particles.x[0] = 32760;
    m_Particles.velocity_x[0] = 16;
    g_Rng = 1;
    Particles_UpdateAndRender(PARTICLES_TOP, 7);
    CHECK_EQ_INT(m_Particles.x[0], -32760);
    CHECK_EQ_INT(m_Particles.alive[0], 0);
    Arena_Free(&arena);
}
