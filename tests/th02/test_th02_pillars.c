#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_player_graphics.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_sound.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/draw.h"
#include "th02/pillars.c"
#include "th02/pillars.h"
#include "th02/player.h"
#include "th02/random.h"

typedef struct {
    TEST_PLAYER_GRAPHICS graphics;
    int16_t scroll;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    memset(f, 0, sizeof(*f));
    TestPlayerGraphics_Init(&f->graphics);
    Random_Reset();
    memset(&m_Player, 0, sizeof(m_Player));
    Background_Init();
    Pillars_Init();
    Pillars_ResetStage(3);
}

static void M_Free(FIXTURE *const f)
{
    TestPlayerGraphics_Free(&f->graphics);
}

static PLAYFIELD_PAGES M_Pages(const uint8_t back, const int16_t scroll)
{
    return (PLAYFIELD_PAGES) { back, back, scroll };
}

TEST(stage_reset_clears_flags_without_erasing_pillar_payloads)
{
    FIXTURE f;
    M_Init(&f);
    for (uint8_t stage = 0; stage < 6; stage++) {
        memset(&m_Pillars, 0x57, sizeof(m_Pillars));
        Pillars_ResetStage(stage);
        CHECK_EQ_INT(m_Pillars.frames_new, 16);
        CHECK_EQ_INT(m_Pillars.enabled, stage == 3 || stage == 5);
        for (unsigned i = 0; i < ARRAY_SIZE(m_Pillars.slots); i++) {
            CHECK_EQ_INT(m_Pillars.slots[i].state, ENTITY_FREE);
            CHECK_EQ_INT(m_Pillars.slots[i].frames, 0x5757);
            CHECK_EQ_INT(m_Pillars.slots[i].phase, 0x57);
        }
        CHECK_EQ_INT(m_Pillars.current.x, 0x5757);
    }
    M_Free(&f);
}

TEST(spawn_uses_first_free_slot_and_rejects_only_full_pool_or_invalid_x)
{
    FIXTURE f;
    M_Init(&f);
    TEST_SOUND sound;
    TestSound_Init(&sound);
    Pillars_Add(31, 80, 6, 0x6f);
    Pillars_Add(416, 80, 6, 0x6f);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    Pillars_SetSpawnInterval(48);
    for (unsigned i = 0; i < 13; i++) {
        Pillars_Add(i % 2 == 0 ? 32 : 415, 80, -1, 0x16f);
        SoundEffects_Update();
        TestSound_Check(&sound, i < 12 ? 6 : -1);
    }
    for (unsigned i = 0; i < 12; i++) {
        CHECK_EQ_INT(m_Pillars.slots[i].state, ENTITY_ACTIVE);
        CHECK_EQ_INT(m_Pillars.slots[i].frames, 48);
        CHECK_EQ_INT(m_Pillars.slots[i].duration, -1);
        CHECK_EQ_INT(m_Pillars.slots[i].pattern, 0x6f);
    }
    m_Pillars.slots[2].state = ENTITY_FREE;
    Pillars_Add(224, 112, 160, 0x67);
    CHECK_EQ_INT(m_Pillars.slots[2].position.x, 224);
    SoundEffects_Update();
    TestSound_Check(&sound, 6);
    TestSound_Free(&sound);
    M_Free(&f);
}

TEST(startup_cleanup_advances_cel_before_update_and_frees_after_invalidation)
{
    FIXTURE f;
    M_Init(&f);
    Pillars_Add(224, 96, 3, 0x6f);
    Pillars_Invalidate(M_Pages(0, f.scroll), 0);
    CHECK_EQ_INT(m_Pillars.slots[0].cel, 1);
    CHECK_EQ_INT(p_Background->cache.dirty[11][5], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[13][7], 1);
    Pillars_Update(M_Pages(0, 399), 0, &f.graphics.patterns);
    CHECK_EQ_INT(g_Video.pages[0][87 * 640 + 216], 2);
    CHECK_EQ_INT(g_Video.pages[0][87 * 640 + 247], 2);
    CHECK_EQ_INT(g_Video.pages[0][86 * 640 + 216], 0);
    CHECK_EQ_INT(m_Pillars.slots[0].frames, 15);
    m_Pillars.slots[0].state = ENTITY_REMOVE;
    m_Pillars.slots[0].cel = 5;
    Pillars_Invalidate(M_Pages(0, f.scroll), 1);
    CHECK_EQ_INT(p_Background->cache.dirty[12][24], 1);
    CHECK_EQ_INT(m_Pillars.slots[0].state, ENTITY_FREE);
    M_Free(&f);
}

TEST(disabled_pillars_are_inert_and_enabling_keeps_existing_slots)
{
    FIXTURE f;
    M_Init(&f);
    Pillars_ResetStage(0);
    Pillars_Add(224, 96, 3, 0x6f);
    const PILLARS before = m_Pillars;
    Pillars_Invalidate(M_Pages(0, f.scroll), 0);
    Pillars_Update(M_Pages(0, 0), 0, &f.graphics.patterns);
    CHECK(memcmp(&before, &m_Pillars, sizeof(before)) == 0);
    CHECK_EQ_INT(g_Video.pages[0][88 * 640 + 216], 0);
    Pillars_Enable();
    Pillars_Update(M_Pages(0, 0), 0, &f.graphics.patterns);
    CHECK_EQ_INT(g_Video.pages[0][88 * 640 + 216], 1);
    M_Free(&f);
}

TEST(active_collision_has_strict_x_edges_and_uses_new_phase_after_duration)
{
    static const struct {
        int16_t x;
        int16_t y;
        int16_t duration;
        bool hit;
    } cases[] = {
        { 200, 96, 10, false }, { 201, 96, 10, true },  { 231, 96, 10, true },
        { 232, 96, 10, false }, { 224, 95, 10, false }, { 224, 96, 1, false },
        { 224, 96, 0, true },
    };
    for (unsigned i = 0; i < ARRAY_SIZE(cases); i++) {
        FIXTURE f;
        M_Init(&f);
        Pillars_Add(224, 96, cases[i].duration, 0x6f);
        m_Pillars.slots[0].cel = 5;
        m_Pillars.slots[0].phase = 4;
        m_Player.current = (ENTITY_POINT) { cases[i].x, cases[i].y };
        m_Player.invincibility = 255;
        Pillars_Update(M_Pages(0, 399), 0, &f.graphics.patterns);
        CHECK_EQ_INT(m_Player.hit, cases[i].hit);
        CHECK_EQ_INT(m_Pillars.slots[0].duration, cases[i].duration - 1);
        M_Free(&f);
    }
}

TEST(pillar_grows_becomes_active_and_retracts_before_cleanup_releases_it)
{
    FIXTURE f;
    M_Init(&f);
    TEST_SOUND sound;
    TestSound_Init(&sound);
    Pillars_Add(224, 96, 3, 0x6f);
    SoundEffects_Update();
    TestSound_Check(&sound, 6);
    bool removed = false;
    for (uint32_t frame = 0; frame < 128; frame++) {
        Pillars_Invalidate(M_Pages(0, f.scroll), frame);
        Pillars_Update(M_Pages(0, 399), frame, &f.graphics.patterns);
        SoundEffects_Update();
        TestSound_Check(&sound, frame == 24 ? 7 : -1);
        if (m_Pillars.slots[0].state == ENTITY_REMOVE) {
            CHECK_EQ_INT(m_Pillars.slots[0].phase, 9);
            removed = true;
            Pillars_Invalidate(M_Pages(0, f.scroll), frame + 1);
            CHECK_EQ_INT(m_Pillars.slots[0].state, ENTITY_FREE);
            break;
        }
    }
    CHECK(removed);
    TestSound_Free(&sound);
    CHECK_EQ_INT(g_Video.pages[0][103 * 640 + 224], 15);
    M_Free(&f);
}

static uint64_t M_HashValue(uint64_t hash, const uint32_t value)
{
    for (size_t byte = 0; byte < 4; byte++) {
        hash ^= (value >> (byte * 8)) & 255;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

TEST(pillar_cycles_keep_pool_order_scroll_pixels_collision_and_sound)
{
    const int16_t durations[] = { 0, 3, 160 };
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t page = 0; page < 2; page++) {
        for (uint8_t rolling = 0; rolling < 2; rolling++) {
            for (size_t duration = 0; duration < ARRAY_SIZE(durations);
                 duration++) {
                FIXTURE f;
                M_Init(&f);
                TEST_SOUND sound;
                TestSound_Init(&sound);
                Video_AccessPage(page);
                m_Player.current = (ENTITY_POINT) { 224, 96 };
                m_Player.invincibility = 255;
                Pillars_Add(224, 96, durations[duration], 111);
                Pillars_Add(32, 112, 3, 103);
                Pillars_Add(415, 128, 3, 111);
                for (uint32_t frame = 0; frame < 160; frame++) {
                    f.scroll =
                        rolling != 0 ? (int16_t)((frame * 7 + 399) % 400) : 0;
                    Pillars_Invalidate(M_Pages(0, f.scroll), frame);
                    Pillars_Update(
                        M_Pages(0, f.scroll), frame, &f.graphics.patterns);
                    const int32_t values[] = {
                        m_Pillars.current.x,  m_Pillars.current.y,
                        m_Pillars.frames_new, m_Pillars.enabled,
                        m_Player.hit,         m_Rng.cursor,
                        g_Grcg->mode,         g_Video.drawing_page,
                    };
                    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
                        hash = M_HashValue(hash, (uint32_t)values[i]);
                    }
                    for (size_t i = 0; i < ARRAY_SIZE(m_Pillars.slots); i++) {
                        const PILLAR *const pillar = &m_Pillars.slots[i];
                        const int32_t fields[] = {
                            pillar->state,      pillar->phase,
                            pillar->position.x, pillar->position.y,
                            pillar->frames,     pillar->duration,
                            pillar->cel,        pillar->pattern,
                        };
                        for (size_t j = 0; j < ARRAY_SIZE(fields); j++) {
                            hash = M_HashValue(hash, (uint32_t)fields[j]);
                        }
                    }
                    for (size_t x = 0; x < TILE_COLUMNS; x++) {
                        hash =
                            M_HashValue(hash, p_Background->cache.columns[x]);
                        for (size_t y = 0; y < TILE_ROWS; y++) {
                            hash = M_HashValue(
                                hash, p_Background->cache.dirty[x][y]);
                        }
                    }
                    for (size_t plane = 0; plane < 4; plane++) {
                        hash = M_HashValue(hash, g_Grcg->tiles[plane]);
                    }
                    for (size_t back = 0; back < 2; back++) {
                        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
                            hash = (hash ^ g_Video.pages[back][i])
                                * UINT64_C(1099511628211);
                        }
                    }
                    SoundEffects_Update();
                    hash = TestSound_Hash(&sound, hash);
                    Video_AccessPage(g_Video.drawing_page ^ 1);
                }
                TestSound_Free(&sound);
                M_Free(&f);
            }
        }
    }
    CHECK(hash == UINT64_C(3789402431294161053));
}
