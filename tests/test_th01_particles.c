#include "th01/particles.h"
#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

#include <string.h>

TEST(initialize_preserves_scene_history)
{
    PARTICLES p = { .cycle = 79 };
    p.x[0] = -123;
    p.velocity_y[1] = -16;
    p.velocity_base[0] = 7;
    memset(p.alive, 255, sizeof(p.alive));
    Particles_Init(&p);
    CHECK_EQ_INT(p.spawn_interval, 2);
    CHECK_EQ_INT(p.velocity_max, 10);
    CHECK_EQ_INT(p.cycle, 79);
    CHECK_EQ_INT(p.x[0], -123);
    CHECK_EQ_INT(p.velocity_y[1], -16);
    CHECK_EQ_INT(p.velocity_base[0], 7);
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        CHECK_EQ_INT(p.alive[i], 0);
    }
}

TEST(first_spawn_skips_zero_and_uses_shared_randomness)
{
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    Video_SelectDrawingPage(&video, 1);
    PARTICLES p = {};
    Particles_Init(&p);
    uint32_t rng = 1;
    Particles_UpdateAndRender(&p, &video, &rng, PARTICLES_TOP, 7);
    CHECK_EQ_INT(rng, 1);
    CHECK_EQ_INT(video.drawing_page, 1);
    uint32_t expected_rng = rng;
    const int16_t speed = (int16_t)(MasterRng_Next(&expected_rng) % 10 + 1);
    const int16_t left = (int16_t)(MasterRng_Next(&expected_rng) % 640);
    Particles_UpdateAndRender(&p, &video, &rng, PARTICLES_TOP, 7);
    CHECK_EQ_INT(rng, expected_rng);
    CHECK_EQ_INT(p.alive[0], 0);
    CHECK_EQ_INT(p.alive[1], 1);
    CHECK_EQ_INT(p.x[1], left * 16);
    CHECK_EQ_INT(p.y[1], speed * 16);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, left, speed), 7);
    CHECK_EQ_INT(video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(first_overflow_preserves_negative_velocity_aliases)
{
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    PARTICLES p = { .cycle = 79 };
    Particles_Init(&p);
    p.x[0] = 1000;
    p.x[1] = 3200;
    p.y[1] = 1600;
    uint32_t rng = 1;
    uint32_t expected_rng = rng;
    const int16_t speed = (int16_t)(MasterRng_Next(&expected_rng) % 10 + 1);
    const int16_t left = (int16_t)(MasterRng_Next(&expected_rng) % 640);
    Particles_UpdateAndRender(&p, &video, &rng, PARTICLES_BOTTOM, 10);
    CHECK_EQ_INT(rng, expected_rng);
    CHECK_EQ_INT(p.cycle, speed);
    CHECK_EQ_INT(p.velocity_base[0], 1);
    CHECK_EQ_INT(p.velocity_x[0], 399 * 16);
    CHECK_EQ_INT(p.velocity_y[0], 0);
    CHECK_EQ_INT(p.y[0], left * 16);
    CHECK_EQ_INT(p.x[0], 1000 + 399 * 16);
    CHECK_EQ_INT(p.alive[1], 255);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 200, 100), 10);
    p.cycle = 79;
    Particles_UpdateAndRender(&p, &video, &rng, PARTICLES_TOP, 7);
    CHECK_EQ_INT(rng, expected_rng);
    CHECK_EQ_INT(p.cycle, 80);
    Arena_Free(&arena);
}

TEST(leaving_particle_restores_old_word_before_dying)
{
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    memset(video.pages[0], 7, VIDEO_PAGE_SIZE);
    memset(video.pages[1], 3, VIDEO_PAGE_SIZE);
    PARTICLES p = {};
    Particles_Init(&p);
    p.alive[0] = 255;
    p.x[0] = 639 * 16;
    p.y[0] = 100 * 16;
    p.velocity_x[0] = 16;
    uint32_t rng = 1;
    Particles_UpdateAndRender(&p, &video, &rng, PARTICLES_TOP, 10);
    CHECK_EQ_INT(p.alive[0], 0);
    CHECK_EQ_INT(p.x[0], 640 * 16);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 623, 100), 7);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 624, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 639, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 0, 101), 3);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 15, 101), 3);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 16, 101), 7);
    Arena_Free(&arena);
}

TEST(motion_wraps_before_bounds_test)
{
    ARENA arena;
    Arena_Init(&arena, "particles", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    PARTICLES p = {};
    Particles_Init(&p);
    p.alive[0] = 1;
    p.x[0] = 32760;
    p.velocity_x[0] = 16;
    uint32_t rng = 1;
    Particles_UpdateAndRender(&p, &video, &rng, PARTICLES_TOP, 7);
    CHECK_EQ_INT(p.x[0], -32760);
    CHECK_EQ_INT(p.alive[0], 0);
    Arena_Free(&arena);
}
