#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_sound.h"
#include "th02/background/private.h"
#include "th02/draw.h"
#include "th02/palette.h"
#include "th02/random.h"
#include "th02/sparks.h"
#include "th02/stage_effects.c"

typedef struct {
    uint8_t slowdown;
} FIXTURE;

static PLAYFIELD_PAGES M_Pages(const uint8_t back, const uint8_t front)
{
    return (PLAYFIELD_PAGES) { .back = back, .front = front };
}

static void M_Init(FIXTURE *const f, const uint8_t stage)
{
    Sparks_Init();
    memset(&m_Palette, 0, sizeof(m_Palette));
    memset(&m_Rng, 0, sizeof(m_Rng));
    memset(f, 0, sizeof(*f));
    Video_Init(&g_HeapAllocator);
    Clip_Reset();
    Palette_Init();
    Grcg_Init();
    f->slowdown = 1;
    Background_Init();
    StageEffects_Init();
    StageEffects_ResetStage(stage);
}

static void M_Update(
    FIXTURE *const f, const uint8_t stage, const bool reduce_effects,
    const PLAYFIELD_PAGES pages)
{
    StageEffects_Update(pages, stage, reduce_effects, &f->slowdown);
}

static void M_Free(void)
{
    Mem_Free(g_Video.pages[0]);
}

TEST(weather_waits_for_activation_and_keeps_persistent_state_between_stages)
{
    FIXTURE f;
    M_Init(&f, 0);
    const STAGE_EFFECTS before = m_StageEffects;
    M_Update(&f, 0, false, M_Pages(0, 1));
    CHECK(memcmp(&m_StageEffects, &before, sizeof(before)) == 0);
    CHECK_EQ_INT(m_Rng.cursor, 0);
    m_StageEffects.storm_frame = 511;
    M_Update(&f, 0, false, M_Pages(0, 1));
    CHECK_EQ_INT(m_StageEffects.storm_frame, 1000);
    CHECK_EQ_INT(m_StageEffects.lightning_frame, 1);
    CHECK_EQ_INT(g_Sparks->acceleration_x, -1);
    CHECK_EQ_INT(g_Clip->left, 32);
    CHECK_EQ_INT(g_Clip->right, 416);
    CHECK_EQ_INT(m_Palette.colors[14][0], 224);
    m_StageEffects.vortex_frame = 900;
    m_StageEffects.particles[0].state = ENTITY_ACTIVE;
    StageEffects_ResetStage(1);
    CHECK_EQ_INT(m_StageEffects.stage, 1);
    CHECK_EQ_INT(m_StageEffects.vortex_frame, 0);
    CHECK_EQ_INT(m_StageEffects.storm_frame, 1000);
    CHECK_EQ_INT(m_StageEffects.lightning_frame, 1);
    CHECK_EQ_INT(m_StageEffects.particles[0].state, ENTITY_ACTIVE);
    M_Free();
}

TEST(weather_spawns_after_ring_draw_and_cleans_both_page_footprints)
{
    FIXTURE f;
    M_Init(&f, 1);
    m_Rng.ring[0] = 0;
    m_Rng.ring[1] = 1;
    M_Update(&f, 1, false, M_Pages(0, 1));
    CHECK_EQ_INT(m_Rng.cursor, 0);
    CHECK_EQ_INT(g_Sparks->acceleration_x, -1);
    M_Update(&f, 1, false, M_Pages(0, 1));
    STAGE_PARTICLE *const p = &m_StageEffects.particles[0];
    CHECK_EQ_INT(m_Rng.cursor, 2);
    CHECK_EQ_INT(p->speed, 4);
    CHECK_EQ_INT(p->position[0].x, 300);
    CHECK_EQ_INT(p->position[0].y, 20);
    CHECK_EQ_INT(p->position[1].x, 304);
    CHECK_EQ_INT(p->position[1].y, 16);
    CHECK_EQ_INT(g_Video.pages[0][20 * 640 + 300], 3);
    CHECK_EQ_INT(g_Video.pages[0][21 * 640 + 301], 3);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    StageEffects_Invalidate(M_Pages(1, 0));
    CHECK_EQ_INT(p->position[1].x, 300);
    CHECK_EQ_INT(p->position[1].y, 20);
    CHECK_EQ_INT(p_Background->cache.columns[17], 1);
    p->state = ENTITY_REMOVE;
    StageEffects_Invalidate(M_Pages(1, 0));
    CHECK_EQ_INT(p->state, ENTITY_FREE);
    M_Free();
}

TEST(full_weather_pool_consumes_spawn_position_without_consuming_speed)
{
    FIXTURE f;
    M_Init(&f, 1);
    m_StageEffects.rain_frame = 1;
    for (unsigned i = 0; i < STAGE_PARTICLE_COUNT; i++) {
        m_StageEffects.particles[i].state = ENTITY_ACTIVE;
        m_StageEffects.particles[i].position[0] = (ENTITY_POINT) { 500, 16 };
    }
    M_Update(&f, 1, false, M_Pages(0, 1));
    CHECK_EQ_INT(m_Rng.cursor, 1);
    m_StageEffects.lightning_frame = 106;
    m_StageEffects.lightning_next = 100;
    M_Update(&f, 1, false, M_Pages(0, 1));
    CHECK_EQ_INT(m_Rng.cursor, 2);
    CHECK_EQ_INT(m_StageEffects.lightning_next, 120);
    CHECK_EQ_INT(m_Palette.tone, 100);
    m_StageEffects.lightning_next = 65532;
    m_StageEffects.lightning_frame = 65531;
    m_StageEffects.rain_frame = 0;
    M_Update(&f, 1, false, M_Pages(0, 1));
    CHECK_EQ_INT(m_Rng.cursor, 2);
    CHECK_EQ_INT(m_Palette.tone, 100);
    M_Free();
}

TEST(vortex_runs_sound_shake_palette_and_slowdown_at_original_boundaries)
{
    FIXTURE f;
    M_Init(&f, 2);
    TEST_SOUND sound;
    TestSound_Init(&sound);
    const PLAYFIELD_PAGES pages = { .back = 0, .front = 1, .scroll = 399 };
    for (unsigned frame = 1; frame <= 1000; frame++) {
        M_Update(&f, 2, false, pages);
        SoundEffects_Update();
        TestSound_Check(&sound, frame == 168 ? 14 : -1);
        if (frame == 168) {
            CHECK_EQ_INT(f.slowdown, 1);
        } else if (frame == 169) {
            CHECK_EQ_INT(f.slowdown, 3);
            CHECK_EQ_INT(m_Palette.tone, 170);
            CHECK_EQ_INT(g_Video.first_lines, 1);
            CHECK_EQ_INT(g_Video.first_address, 15961);
            CHECK_EQ_INT(g_Video.second_address, 1);
        } else if (frame == 200) {
            CHECK_EQ_INT(f.slowdown, 1);
            CHECK_EQ_INT(m_StageEffects.radius, 0);
            CHECK_EQ_INT(m_StageEffects.angle, 1);
            CHECK_EQ_INT(m_Palette.colors[0][0], 100);
        } else if (frame == 600) {
            CHECK_EQ_INT(m_Palette.colors[0][2], 100);
        }
    }
    CHECK_EQ_INT(m_StageEffects.vortex_frame, 200);

    CHECK_EQ_INT(m_Rng.cursor, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    TestSound_Free(&sound);
    M_Free();
}

TEST(portal_restores_and_ors_only_intensity_on_back_page_zero)
{
    static const int16_t lines[] = { 0, 152, 232, 248, 399 };
    for (unsigned mode = 0; mode < 2; mode++) {
        for (unsigned run = 0; run < ARRAY_SIZE(lines); run++) {
            FIXTURE f;
            M_Init(&f, 3);
            p_Background->cache.mode = mode == 0 ? TILE_IMAGES : TILE_DISABLED;
            memset(
                p_Background->cache.images, 99,
                sizeof(p_Background->cache.images));
            PLAYFIELD_PAGES pages = { .back = 0,
                                      .front = 1,
                                      .scroll = lines[run] };
            for (unsigned page = 0; page < 2; page++) {
                for (unsigned i = 0; i < VIDEO_PAGE_SIZE; i++) {
                    g_Video.pages[page][i] =
                        (uint8_t)(((i * 3 + i / 640 + page) & 15) | 0xa0);
                }
            }
            for (unsigned image = 0; image < 100; image++) {
                const unsigned x = 576 + image / 25 * 16;
                const unsigned y = image % 25 * 16;
                p_Background->sources[image] = x / 8 + y * 80;
                for (unsigned row = 0; row < 16; row++) {
                    for (unsigned pixel = 0; pixel < 16; pixel++) {
                        g_Video.pages[0][(y + row) * 640 + x + pixel] =
                            image == 99 ? 0 : ((image + row + pixel) & 1) * 8;
                    }
                }
            }
            uint8_t *const expected = Mem_Alloc(VIDEO_PAGE_SIZE * 2);
            memcpy(expected, g_Video.pages[0], VIDEO_PAGE_SIZE * 2);
            M_Update(&f, 3, false, pages);
            const STAGE_EFFECTS state = m_StageEffects;
            pages.back = 1;
            g_Video.drawing_page = 1;
            M_Update(&f, 3, false, pages);
            CHECK(memcmp(&state, &m_StageEffects, sizeof(state)) == 0);
            CHECK(
                memcmp(
                    expected + VIDEO_PAGE_SIZE, g_Video.pages[1],
                    VIDEO_PAGE_SIZE)
                == 0);
            if (mode == 0) {
                const unsigned top = (156 + lines[run]) % 400;
                for (unsigned row = top / 16; row <= (top + 95) / 16; row++) {
                    for (unsigned y = 0; y < 16; y++) {
                        for (unsigned x = 176; x < 272; x++) {
                            expected[((row % 25) * 16 + y) * 640 + x] &= 7;
                        }
                    }
                }
            }
            const unsigned top = (152 + lines[run]) % 400;
            for (unsigned y = 0; y < 96; y++) {
                for (unsigned x = 0; x < 96; x++) {
                    const unsigned image = y / 16 * 16 + x / 16;
                    expected[((top + y) % 400) * 640 + 176 + x] |=
                        ((image + y % 16 + x % 16) & 1) * 8;
                }
            }
            CHECK(memcmp(expected, g_Video.pages[0], VIDEO_PAGE_SIZE) == 0);
            CHECK_EQ_INT(m_StageEffects.portal_top, top);
            CHECK_EQ_INT(g_Grcg->mode, 0);
            CHECK_EQ_INT(m_Rng.cursor, 0);
            for (unsigned x = 0; x < TILE_COLUMNS; x++) {
                CHECK_EQ_INT(p_Background->cache.columns[x], 0);
                for (unsigned y = 0; y < TILE_ROWS; y++) {
                    CHECK_EQ_INT(p_Background->cache.dirty[x][y], 0);
                }
            }
            Mem_Free(expected);
            M_Free();
        }
    }
}

static uint64_t M_HashValue(uint64_t hash, const uint32_t value)
{
    for (size_t byte = 0; byte < 4; byte++) {
        hash ^= (value >> (byte * 8)) & 255;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t M_HashState(const FIXTURE *const f, uint64_t hash)
{
    const STAGE_EFFECTS *const effects = &m_StageEffects;
    const int32_t values[] = {
        effects->storm_frame,
        effects->rain_frame,
        effects->lightning_frame,
        effects->lightning_next,
        effects->vortex_frame,
        effects->rain_clip_reset,
        effects->radius,
        effects->center_x,
        effects->center_y,
        effects->angle,
        effects->portal_top,
        f->slowdown,
        g_Sparks->acceleration_x,
        g_Clip->left,
        g_Clip->top,
        g_Clip->right,
        g_Clip->bottom,
        m_Palette.tone,
        g_Grcg->mode,
        g_Video.first_lines,
        g_Video.first_address,
        g_Video.second_address,
        g_Video.drawing_page,
    };
    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
        hash = M_HashValue(hash, (uint32_t)values[i]);
    }
    for (size_t i = 0; i < STAGE_PARTICLE_COUNT; i++) {
        const STAGE_PARTICLE *const particle = &effects->particles[i];
        const int32_t fields[] = {
            particle->state,         particle->speed,
            particle->position[0].x, particle->position[0].y,
            particle->position[1].x, particle->position[1].y,
        };
        for (size_t j = 0; j < ARRAY_SIZE(fields); j++) {
            hash = M_HashValue(hash, (uint32_t)fields[j]);
        }
    }
    for (size_t page = 0; page < 2; page++) {
        for (size_t i = 0; i < 64; i++) {
            hash = M_HashValue(hash, (uint32_t)effects->dots[page][i].x);
            hash = M_HashValue(hash, (uint32_t)effects->dots[page][i].y);
        }
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            hash = (hash ^ g_Video.pages[page][i]) * UINT64_C(1099511628211);
        }
    }
    for (size_t x = 0; x < TILE_COLUMNS; x++) {
        hash = M_HashValue(hash, p_Background->cache.columns[x]);
        for (size_t y = 0; y < TILE_ROWS; y++) {
            hash = M_HashValue(hash, p_Background->cache.dirty[x][y]);
        }
    }
    for (size_t i = 0; i < 4; i++) {
        hash = M_HashValue(hash, g_Grcg->tiles[i]);
    }
    for (size_t i = 0; i < 16; i++) {
        for (size_t channel = 0; channel < 3; channel++) {
            hash = M_HashValue(hash, m_Palette.colors[i][channel]);
        }
    }
    hash = M_HashValue(hash, m_Rng.seed);
    hash = M_HashValue(hash, m_Rng.cursor);
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        hash = M_HashValue(hash, m_Rng.ring[i]);
    }
    return hash;
}

TEST(
    weather_boundaries_wrap_pages_and_reduction_keep_state_pixels_rng_and_sound)
{
    const int16_t first[] = { 0, 511, 1000, INT16_MAX, 1000 };
    const uint16_t lightning[] = { 0, 1, 99, 65531, 106 };
    const uint16_t next[] = { 0, 100, 100, 65532, 100 };
    const uint16_t vortex[] = { 166, 198, 398, 798, 998 };
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t stage = 0; stage < 3; stage++) {
        for (uint8_t reduced = 0; reduced < 2; reduced++) {
            for (size_t start = 0; start < ARRAY_SIZE(first); start++) {
                FIXTURE f;
                M_Init(&f, stage);
                TEST_SOUND sound;
                TestSound_Init(&sound);
                StageEffects_ResetStage(stage);
                m_StageEffects.storm_frame = first[start];
                m_StageEffects.rain_frame = start == 3 ? UINT16_MAX : 0;
                m_StageEffects.lightning_frame = lightning[start];
                m_StageEffects.lightning_next = next[start];
                m_StageEffects.vortex_frame = vortex[start];
                p_Background->cache.mode = TILE_IMAGES;
                if (start == 4) {
                    for (size_t i = 0; i < STAGE_PARTICLE_COUNT; i++) {
                        m_StageEffects.particles[i].state = ENTITY_ACTIVE;
                        m_StageEffects.particles[i].position[0] =
                            m_StageEffects.particles[i].position[1] =
                                (ENTITY_POINT) { 500, 16 };
                    }
                }
                m_Rng.cursor = 255;
                for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
                    m_Rng.ring[i] = (uint8_t)(i ^ 0x5a);
                }
                for (uint32_t frame = 0; frame < 32; frame++) {
                    const uint8_t back = frame & 1;
                    const PLAYFIELD_PAGES pages = {
                        .back = back,
                        .front = (uint8_t)(back ^ 1),
                        .scroll = (int16_t)((frame * 7 + 399) % 400),
                    };
                    Video_AccessPage(pages.back);
                    StageEffects_Invalidate(pages);
                    M_Update(&f, stage, reduced != 0, pages);
                    hash = M_HashState(&f, hash);
                    SoundEffects_Update();
                    hash = TestSound_Hash(&sound, hash);
                }
                TestSound_Free(&sound);
                M_Free();
            }
        }
    }
    CHECK(hash == UINT64_C(5668805400130187672));
}
