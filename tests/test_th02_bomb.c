#include "th02_score_fixture.h"
#include "th02_random_fixture.h"
#include "th02_tram_fixture.h"
#include "th02_palette_fixture.h"
#include "th02_player_fixture.h"
#include "th02_clip_fixture.h"
#include "../src/th02/performance.c"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/bullets/private.h"
#include "th02/player.h"
#include "th02/sparks.h"
#include "th02/random.h"
#include "th02/bomb.h"
#include "th02/tram.h"

#include "core/base/arena.h"

#include "test.h"
#include "th02_sound.h"

typedef struct {
    ARENA arena;

    BOMB_ASSETS assets;

    PLAYFIELD_PAGES pages;
    uint8_t slowdown;
    uint32_t frame;
    bool reduce;
    uint8_t tail[72];
    uint8_t pixels[8];
    bool fix_transition_overread;
    const uint8_t *transition_tail;
    TEST_SOUND sound;
    uint64_t pcm;
} FIXTURE;

typedef struct {
    BOMB bomb;
    PLAYER player;
    RANDOM_STATE random;
    SPARKS sparks;
    SCORE score;
    PERFORMANCE performance;
    PALETTE_STATE palette;
    uint8_t slowdown;
    uint64_t pcm;
} BOMB_SNAPSHOT;

static void M_Init(FIXTURE *const fixture, const uint8_t type)
{
    Sparks_Init();
    Bullets_Init();
    Score_Init();
    memset(&m_Performance, 0, sizeof(m_Performance));
    memset(&m_Palette, 0, sizeof(m_Palette));
    Grcg_Init();
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    Tram_Init();
    memset(&m_Rng, 0, sizeof(m_Rng));
    memset(fixture, 0, sizeof(*fixture));
    memset(&m_Player, 0, sizeof(m_Player));
    Arena_Init(&fixture->arena, "Bomb controller", 1024 * 1024);
    Video_Init(Arena_Allocator(&fixture->arena));
    Clip_Reset();
    fixture->assets.shot_type = type;
    fixture->assets.illustration = (PI_IMAGE) {
        .width = 8,
        .height = 1,
        .pixels = fixture->pixels,
    };
    memset(fixture->assets.particles, 0xff, sizeof(fixture->assets.particles));
    memset(fixture->tail, 0x80, sizeof(fixture->tail));
    m_Palette.tone = 100;
    memset(m_Palette.colors, 87, sizeof(m_Palette.colors));
    m_Score.bombs = 3;
    fixture->pages = (PLAYFIELD_PAGES) { 0, 1, 0 };
    fixture->slowdown = 1;
    p_Background->cache.mode = TILE_IMAGES;
    fixture->transition_tail = fixture->tail;
    Bomb_Init();
    TestSound_Init(&fixture->sound);
    fixture->pcm = UINT64_C(14695981039346656037);
}

static void M_Activate(FIXTURE *const fixture)
{
    Bomb_Activate(fixture->pages);
    SoundEffects_Update();
    fixture->pcm = TestSound_Hash(&fixture->sound, fixture->pcm);
}

static void M_Update(FIXTURE *const fixture)
{
    Bomb_Update(
        fixture->pages, &fixture->assets, &fixture->slowdown, fixture->reduce,
        fixture->transition_tail, fixture->fix_transition_overread);
    SoundEffects_Update();
    fixture->pcm = TestSound_Hash(&fixture->sound, fixture->pcm);
}

static void M_Free(FIXTURE *const fixture, const uint64_t expected)
{
    CHECK(fixture->pcm == expected);
    TestSound_Free(&fixture->sound);
    Arena_Free(&fixture->arena);
}

TEST(
    activation_uses_signed_bombs_and_keeps_particles_while_clearing_real_bullets)
{
    FIXTURE fixture;
    M_Init(&fixture, 0);
    m_Bomb.ages[3] = 4;
    m_Bomb.frame = 47;
    m_Bomb.stage_bombs = m_Performance.total_bombs = 255;
    m_Score.bombs = INT8_MIN;
    p_Bullets->slots[0] = (BULLET) {
        .state = ENTITY_ACTIVE,
        .kind = BULLET_PELLET,
        .position = { { 100 * 16, 100 * 16 }, { 120 * 16, 120 * 16 } },
    };
    M_Activate(&fixture);
    CHECK_EQ_INT(m_Score.bombs, 127);
    CHECK_EQ_INT(m_Bomb.stage_bombs, 0);
    CHECK_EQ_INT(m_Performance.total_bombs, 0);
    CHECK_EQ_INT(m_Bomb.ages[3], 4);
    CHECK_EQ_INT(m_Bomb.center.x, 220);
    CHECK_EQ_INT(m_Bomb.center.y, 196);
    CHECK_EQ_INT(m_Bomb.frame, 0);
    CHECK_EQ_INT(m_Player.bomb_invincible, 1);

    CHECK_EQ_INT(p_Bullets->slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(p_Bullets->slots[0].position[0].x, 120 * 16);
    CHECK(p_Background->cache.columns[4] != 0);
    M_Activate(&fixture);
    CHECK_EQ_INT(m_Score.bombs, 127);

    Bomb_ResetStage();
    CHECK_EQ_INT(m_Bomb.active, 0);
    CHECK_EQ_INT(m_Bomb.ages[3], 4);
    m_Score.bombs = 0;
    M_Activate(&fixture);
    CHECK_EQ_INT(m_Bomb.active, 0);
    M_Free(&fixture, UINT64_C(4110778921578225087));
}

TEST(
    all_types_complete_their_circle_and_animation_without_resetting_unused_slots)
{
    for (uint8_t type = 0; type < 3; type++) {
        FIXTURE fixture;
        M_Init(&fixture, type);
        m_Bomb.columns[49] = (ENTITY_POINT) { 17, 19 };
        m_Bomb.ages[49] = 231;
        M_Activate(&fixture);
        size_t ticks = 0;
        while (m_Bomb.active != 0 && ticks < 256) {
            Bomb_Invalidate(fixture.pages, &fixture.assets);
            M_Update(&fixture);
            ticks++;
            fixture.pages.front = fixture.pages.back;
            fixture.pages.back ^= 1;
            Video_AccessPage(fixture.pages.back);
        }
        CHECK_EQ_INT(ticks, type == 2 ? 162 : 190);
        CHECK_EQ_INT(m_Bomb.active, 0);
        CHECK_EQ_INT(m_Player.bomb_invincible, 0);
        CHECK_EQ_INT(m_Player.invincibility, 70);
        CHECK_EQ_INT(m_Score.bombs, 2);
        CHECK_EQ_INT(m_Bomb.stage_bombs, 1);
        CHECK_EQ_INT(m_Performance.total_bombs, 1);
        CHECK_EQ_INT(m_Palette.tone, 100);
        CHECK_EQ_INT(p_Background->cache.mode, TILE_IMAGES);
        CHECK_EQ_INT(fixture.slowdown, 1);
        CHECK_EQ_INT(g_Grcg->mode, 0);
        CHECK_EQ_INT(m_Bomb.columns[49].y, 19);
        CHECK_EQ_INT(m_Bomb.ages[49], 231);

        CHECK_EQ_INT(fixture.frame, 0);
        const uint64_t pcm[] = { UINT64_C(10798969055977867667),
                                 UINT64_C(8096685574658686219),
                                 UINT64_C(15346402011301579193) };
        M_Free(&fixture, pcm[type]);
    }
}

TEST(transition_overread_fix_skips_only_frame_31_and_retains_exact_mode)
{
    for (uint8_t type = 0; type < 3; type++) {
        FIXTURE fixture;
        M_Init(&fixture, type);
        m_Bomb.active = 1;
        m_Bomb.circle_done = 1;
        m_Bomb.frame = 29;
        fixture.fix_transition_overread = true;
        fixture.transition_tail = nullptr;
        fixture.assets.transition[14][0][0] = 0x80;
        M_Update(&fixture);
        const uint8_t color = type == 0 ? 3 : type == 1 ? 15 : 2;
        CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 32], color);
        memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
        M_Update(&fixture);
        CHECK_EQ_INT(m_Bomb.frame, 31);
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            CHECK_EQ_INT(g_Video.pages[0][i], 7);
        }
        CHECK_EQ_INT(g_Grcg->mode, 0);
        CHECK_EQ_INT(m_Score.bombs, 3);
        m_Bomb.frame = 30;
        fixture.fix_transition_overread = false;
        fixture.transition_tail = fixture.tail;
        M_Update(&fixture);
        CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 32], color);
        M_Free(&fixture, UINT64_C(2161357797883559077));
    }
}

TEST(transition_fix_preserves_complete_animation_state_and_random_stream)
{
    BOMB_SNAPSHOT *const expected = Mem_AllocZ(256 * sizeof(*expected));
    for (uint8_t type = 0; type < 3; type++) {
        size_t expected_ticks = 0;
        for (size_t fixed = 0; fixed < 2; fixed++) {
            FIXTURE fixture;
            M_Init(&fixture, type);
            fixture.fix_transition_overread = fixed != 0;
            if (fixed != 0) {
                fixture.transition_tail = nullptr;
            }
            M_Activate(&fixture);
            size_t ticks = 0;
            while (m_Bomb.active != 0 && ticks < 256) {
                Bomb_Invalidate(fixture.pages, &fixture.assets);
                M_Update(&fixture);
                BOMB_SNAPSHOT observed = {};
                observed.bomb = m_Bomb;
                observed.player = m_Player;
                observed.random = m_Rng;
                observed.sparks = *g_Sparks;
                observed.score = *g_Score;
                observed.performance = m_Performance;
                observed.palette = m_Palette;
                observed.slowdown = fixture.slowdown;
                observed.pcm = fixture.pcm;
                if (fixed == 0) {
                    expected[ticks] = observed;
                } else {
                    CHECK(ticks < expected_ticks);
                    const BOMB_SNAPSHOT *const before = &expected[ticks];
                    CHECK(
                        memcmp(
                            &observed.bomb, &before->bomb,
                            sizeof(observed.bomb))
                        == 0);
                    CHECK(
                        memcmp(
                            &observed.player, &before->player,
                            sizeof(observed.player))
                        == 0);
                    CHECK(
                        memcmp(
                            &observed.random, &before->random,
                            sizeof(observed.random))
                        == 0);
                    CHECK(
                        memcmp(
                            &observed.sparks, &before->sparks,
                            sizeof(observed.sparks))
                        == 0);
                    CHECK(
                        memcmp(
                            &observed.score, &before->score,
                            sizeof(observed.score))
                        == 0);
                    CHECK(
                        memcmp(
                            &observed.performance, &before->performance,
                            sizeof(observed.performance))
                        == 0);
                    CHECK(
                        memcmp(
                            &observed.palette, &before->palette,
                            sizeof(observed.palette))
                        == 0);
                    CHECK_EQ_INT(observed.slowdown, before->slowdown);
                    CHECK(observed.pcm == before->pcm);
                }
                fixture.pages.front = fixture.pages.back;
                fixture.pages.back ^= 1;
                Video_AccessPage(fixture.pages.back);
                ticks++;
            }
            CHECK_EQ_INT(ticks, type == 2 ? 162 : 190);
            CHECK_EQ_INT(m_Bomb.active, 0);
            if (fixed == 0) {
                expected_ticks = ticks;
            } else {
                CHECK_EQ_INT(ticks, expected_ticks);
            }
            const uint64_t pcm[] = { UINT64_C(10798969055977867667),
                                     UINT64_C(8096685574658686219),
                                     UINT64_C(15346402011301579193) };
            M_Free(&fixture, pcm[type]);
        }
    }
    Mem_Free(expected);
}

static uint64_t M_HashValue(uint64_t hash, const uint32_t value)
{
    for (size_t byte = 0; byte < 4; byte++) {
        hash ^= (value >> (byte * 8)) & 255;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t M_HashState(const FIXTURE *const fixture, uint64_t hash)
{
    const BOMB *const bomb = &m_Bomb;
    const int32_t values[] = {
        bomb->frame,
        bomb->circle_frame,
        bomb->circle_done,
        bomb->center.x,
        bomb->center.y,
        bomb->page_scroll[0],
        bomb->page_scroll[1],
        bomb->stage_bombs,
        bomb->active,
        bomb->row,
        m_Player.bomb_invincible,
        m_Player.invincibility,
        m_Score.bombs,
        m_Performance.total_bombs,
        fixture->slowdown,
        m_Palette.tone,
        p_Background->cache.mode,
        p_Background->cache.redraw_all,
        g_Sparks->next,
        g_Grcg->mode,
        g_Video.drawing_page,
        g_Video.visible_page,
    };
    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
        hash = M_HashValue(hash, (uint32_t)values[i]);
    }
    for (size_t i = 0; i < 50; i++) {
        hash = M_HashValue(hash, (uint32_t)bomb->columns[i].x);
        hash = M_HashValue(hash, (uint32_t)bomb->columns[i].y);
        hash = M_HashValue(hash, bomb->ages[i]);
    }
    for (size_t i = 0; i < 32; i++) {
        hash = M_HashValue(hash, (uint32_t)bomb->particles[i].x);
        hash = M_HashValue(hash, (uint32_t)bomb->particles[i].y);
    }
    for (size_t i = 0; i < 3; i++) {
        hash = M_HashValue(hash, bomb->tile_modes[i]);
        hash = M_HashValue(hash, bomb->color_zero[i]);
        hash = M_HashValue(hash, bomb->color_three[i]);
    }
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        const SPARK *const spark = &g_Sparks->slots[i];
        const int32_t fields[] = {
            spark->state,         spark->age,           spark->position[0].x,
            spark->position[0].y, spark->position[1].x, spark->position[1].y,
            spark->velocity.x,    spark->velocity.y,    spark->drawing,
            spark->reserved_1,    spark->angle,         spark->base_speed,
            spark->reserved_2,    spark->preferred,
        };
        for (size_t j = 0; j < ARRAY_SIZE(fields); j++) {
            hash = M_HashValue(hash, (uint32_t)fields[j]);
        }
    }
    for (size_t i = 0; i < 16; i++) {
        for (size_t channel = 0; channel < 3; channel++) {
            hash = M_HashValue(hash, m_Palette.colors[i][channel]);
        }
    }
    for (size_t i = 0; i < 4; i++) {
        hash = M_HashValue(hash, g_Grcg->tiles[i]);
    }
    for (size_t x = 0; x < TILE_COLUMNS; x++) {
        hash = M_HashValue(hash, p_Background->cache.columns[x]);
        for (size_t y = 0; y < TILE_ROWS; y++) {
            hash = M_HashValue(hash, p_Background->cache.dirty[x][y]);
        }
    }
    hash = M_HashValue(hash, m_Rng.seed);
    hash = M_HashValue(hash, m_Rng.cursor);
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        hash = M_HashValue(hash, m_Rng.ring[i]);
    }
    for (size_t i = 0; i < TRAM_PLANE_CELLS; i++) {
        hash = M_HashValue(hash, m_Tram.codes[i]);
        hash = M_HashValue(hash, m_Tram.attributes[i]);
    }
    for (size_t page = 0; page < 2; page++) {
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            hash = (hash ^ g_Video.pages[page][i]) * UINT64_C(1099511628211);
        }
    }
    return hash;
}

TEST(full_animations_keep_live_scroll_reduction_transition_pixels_and_sound)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t type = 0; type < 3; type++) {
        for (uint8_t fixed = 0; fixed < 2; fixed++) {
            for (uint8_t reduced = 0; reduced < 2; reduced++) {
                FIXTURE fixture;
                M_Init(&fixture, type);
                fixture.reduce = reduced != 0;
                fixture.fix_transition_overread = fixed != 0;
                if (fixed != 0) {
                    fixture.transition_tail = nullptr;
                }
                m_Rng.cursor = 255;
                for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
                    m_Rng.ring[i] = (uint8_t)(i ^ 0x5a);
                }
                m_Bomb.columns[49] = (ENTITY_POINT) { 17, 19 };
                m_Bomb.ages[49] = 231;
                for (size_t i = 0; i < sizeof(fixture.assets.transition); i++) {
                    ((uint8_t *)fixture.assets.transition)[i] =
                        (uint8_t)(i * 13 + 7);
                }
                for (size_t i = 0; i < sizeof(fixture.assets.text); i++) {
                    ((uint8_t *)fixture.assets.text)[i] = (uint8_t)(i ^ 0x5a);
                }
                M_Activate(&fixture);
                size_t ticks = 0;
                while (m_Bomb.active != 0 && ticks < 256) {
                    fixture.pages.scroll = (int16_t)((ticks * 7 + 399) % 400);
                    Bomb_Invalidate(fixture.pages, &fixture.assets);
                    M_Update(&fixture);
                    hash = M_HashState(&fixture, hash);
                    hash ^= fixture.pcm;
                    hash *= UINT64_C(1099511628211);
                    fixture.pages.front = fixture.pages.back;
                    fixture.pages.back ^= 1;
                    Video_AccessPage(fixture.pages.back);
                    ticks++;
                }
                CHECK_EQ_INT(ticks, type == 2 ? 162 : 190);
                CHECK_EQ_INT(m_Bomb.active, 0);
                TestSound_Free(&fixture.sound);
                Arena_Free(&fixture.arena);
            }
        }
    }
    CHECK(hash == UINT64_C(6998216576759112773));
}
