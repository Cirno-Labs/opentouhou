#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_sound.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/bullets/private.h"
#include "th02/enemies.c"
#include "th02/enemies.h"
#include "th02/items.h"
#include "th02/performance_private.h"
#include "th02/player.h"
#include "th02/shots/private.h"
#include "th02/sparks.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

typedef struct {
    uint8_t rank;
    uint8_t bomb_frame_mask;
    PLAYFIELD_PAGES pages;
    uint8_t scrolled;
    uint32_t frame;
    STAGE_DATA data;
    STAGE_FORMATION formations[2];
    STAGE_SCRIPT scripts[2];
    STAGE_LINE lines[2];

    PATTERN_BANK patterns;
    ARENA arena;
    uint8_t pixels[640];
    TEST_SOUND sound;
} FIXTURE;

static FIXTURE *M_New(void)
{
    Sparks_Init();
    Bullets_Init();
    Enemies_Init();
    Items_Init();
    Score_Init();
    memset(p_Performance, 0, sizeof(*p_Performance));
    Grcg_Init();
    Background_Init();
    memset(p_Rng, 0, sizeof(*p_Rng));
    FIXTURE *const fixture = Mem_AllocZ(sizeof(*fixture));
    Arena_Init(&fixture->arena, "enemy tests", 1024 * 1024);
    TestSound_Init(&fixture->sound);
    Video_Init(Arena_Allocator(&fixture->arena));
    TestClip_Set(0, 0, 639, 399);
    fixture->data = (STAGE_DATA) { .formation_count = 2,
                                   .script_count = 2,
                                   .line_count = 2,
                                   .declared_line_count = 2,
                                   .formations = fixture->formations,
                                   .scripts = fixture->scripts,
                                   .lines = fixture->lines };
    for (size_t i = 0; i < 2; i++) {
        fixture->formations[i] = (STAGE_FORMATION) {
            .pattern = 10, .hp = 10, .score = 100, .script = (uint8_t)i
        };
        fixture->scripts[i].bytes[1] = 255;
        for (size_t j = 0; j < STAGE_DATA_COLUMNS; j++) {
            fixture->lines[i].formations[j] = -1;
        }
    }
    memset(fixture->pixels, 255, 256);
    for (size_t i = 0; i < 20; i++) {
        fixture->patterns.entries[i] =
            (PATTERN) { .size = 0x0420,
                        .data = fixture->pixels,
                        .storage_size = sizeof(fixture->pixels) };
    }
    Player_Init();
    m_Player.current = (ENTITY_POINT) { 300, 300 };
    m_Player.position[1] = m_Player.current;
    Random_Reset();
    BulletSpawn_Init(1);
    fixture->pages = (PLAYFIELD_PAGES) { 0, 1, 0 };
    fixture->rank = 1;
    fixture->bomb_frame_mask = 1;
    return fixture;
}

static void M_Update(FIXTURE *const fixture)
{
    Enemies_Update(
        fixture->pages, fixture->scrolled, &fixture->data, &fixture->patterns,
        fixture->frame, fixture->rank, fixture->bomb_frame_mask);
}

static void M_Free(FIXTURE *const fixture)
{
    Arena_Free(&fixture->arena);
    TestSound_Free(&fixture->sound);
    Mem_Free(fixture);
}

static void M_Add(const uint8_t slot, const int16_t formation)
{
    Enemies_Add(0, 100, 100, slot, formation);
    m_Enemies.count = slot + 1;
}

TEST(spawning_keeps_slot_zero_count_bug_and_unwritten_bytes)
{
    FIXTURE *const fixture = M_New();
    memset(&m_Enemies, 0xa5, sizeof(m_Enemies));
    m_Enemies.count = 0;
    p_Rng->ring[0] = 73;
    Enemies_Add(0, 100, -20, 0, 1);
    CHECK_EQ_INT(m_Enemies.count, 0);
    CHECK_EQ_INT(m_Enemies.slots[0].age, 73);
    CHECK_EQ_INT(m_Enemies.slots[0].velocity_x, (int8_t)0xa5);
    CHECK_EQ_INT(m_Enemies.slots[0].position[1].x, (int16_t)0xa5a5);
    CHECK_EQ_INT(m_Enemies.slots[0].bullet_speed, 0xa5);
    Enemies_Add(0, 300, 10, 1, 0);
    CHECK_EQ_INT(m_Enemies.count, 2);
    CHECK_EQ_INT(m_Enemies.slots[1].spawned_left, 0);
    Enemies_ResetStage();
    CHECK_EQ_INT(m_Enemies.line, 0);
    CHECK_EQ_INT(m_Enemies.count, 2);
    CHECK_EQ_INT(m_Enemies.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Enemies.slots[0].age, 73);
    M_Free(fixture);
}

TEST(schedule_uses_exact_row_equality_and_midboss_still_consumes_row)
{
    FIXTURE *const fixture = M_New();
    fixture->lines[0].position = 10;
    fixture->lines[0].formations[0] = 0;
    fixture->lines[0].formations[47] = 1;
    fixture->formations[1].spawn_top = -16;
    fixture->lines[1].position = 9;
    fixture->lines[1].formations[0] = 0;
    Enemies_Spawn(&fixture->data, fixture->pages.back, 11, 0);
    CHECK_EQ_INT(m_Enemies.line, 0);
    Enemies_Spawn(&fixture->data, fixture->pages.back, 10, 0);
    CHECK_EQ_INT(m_Enemies.line, 1);
    CHECK_EQ_INT(m_Enemies.slots[0].position[0].x, 24);
    CHECK_EQ_INT(m_Enemies.slots[1].position[0].x, 400);
    CHECK_EQ_INT(m_Enemies.slots[1].position[0].y, -16);
    CHECK_EQ_INT(m_Enemies.count, 2);
    Enemies_Spawn(&fixture->data, fixture->pages.back, 10, 0);
    CHECK_EQ_INT(m_Enemies.line, 1);
    Enemies_Spawn(&fixture->data, fixture->pages.back, 9, 1);
    CHECK_EQ_INT(m_Enemies.line, 2);
    CHECK_EQ_INT(p_Rng->cursor, 2);
    M_Free(fixture);
}

TEST(missing_declared_rows_keep_spawn_state_without_reading_past_storage)
{
    FIXTURE *const fixture = M_New();
    fixture->data.declared_line_count = 6;
    fixture->data.lines = nullptr;
    for (uint16_t stored = 0; stored <= 2; stored++) {
        fixture->data.line_count = stored;
        m_Enemies.line = (int16_t)stored;
        const ENEMIES before = m_Enemies;
        const RANDOM_STATE random_before = *p_Rng;
        for (int16_t rows = 0; rows < 100; rows++) {
            Enemies_Spawn(&fixture->data, fixture->pages.back, rows, 0);
            CHECK(memcmp(&m_Enemies, &before, sizeof(before)) == 0);
            CHECK(memcmp(p_Rng, &random_before, sizeof(random_before)) == 0);
        }
    }
    M_Free(fixture);
}

TEST(invalidation_frees_after_marking_and_rebuilds_count)
{
    FIXTURE *const fixture = M_New();
    M_Add(0, 0);
    M_Add(24, 0);
    p_Background->cache.mode = TILE_IMAGES;
    m_Enemies.slots[0].state = ENTITY_REMOVE;
    m_Enemies.slots[24].state = 255;
    m_Enemies.slots[24].position[1] = (ENTITY_POINT) { 120, 130 };
    Enemies_Invalidate((PLAYFIELD_PAGES) { 0, 1, 0 });
    CHECK_EQ_INT(m_Enemies.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Enemies.slots[24].position[0].x, 120);
    CHECK_EQ_INT(m_Enemies.count, 25);
    CHECK_EQ_INT(p_Background->cache.columns[4], 1);
    Enemies_RemoveAll();
    CHECK_EQ_INT(m_Enemies.count, 0);
    CHECK_EQ_INT(m_Enemies.slots[24].state, ENTITY_REMOVE);
    M_Free(fixture);
}

TEST(scripts_emit_real_bullets_then_wait_and_write_both_tile_pages)
{
    FIXTURE *const fixture = M_New();
    const uint8_t script[] = { 0x25, BULLET_SINGLE, 64, 0x20, 0xaf, 32,
                               9,    0x01,          2,  255,  2,    0x10 };
    memcpy(fixture->scripts[0].bytes, script, sizeof(script));
    M_Add(0, 0);
    M_Update(fixture);
    CHECK_EQ_INT(p_Bullets->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(p_Bullets->slots[0].position[0].x, 1600);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.x, 64);
    CHECK_EQ_INT(m_Enemies.slots[0].position[0].x, 99);
    CHECK_EQ_INT(m_Enemies.slots[0].position[0].y, 102);
    CHECK_EQ_INT(m_Enemies.slots[0].script_ip, 7);
    CHECK_EQ_INT(p_Background->cache.images[2][4], 9);
    CHECK_EQ_INT(p_Background->scroll.copy_top, 0);
    CHECK_EQ_INT(p_Background->scroll.copy_height, 16);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    M_Update(fixture);
    CHECK_EQ_INT(m_Enemies.slots[0].script_ip, 11);
    M_Update(fixture);
    CHECK_EQ_INT(m_Enemies.slots[0].state, ENTITY_REMOVE);
    M_Free(fixture);
}

TEST(damage_uses_real_shots_items_sparks_score_and_lunatic_return_bullets)
{
    FIXTURE *const fixture = M_New();
    fixture->rank = 3;
    fixture->formations[0].hp = 1;
    fixture->formations[0].item = ITEM_BOMB + ITEM_POINT;
    p_Score->delta = 0x1234ffff;
    p_Shots->slots[0] = (SHOT) { .state = ENTITY_ACTIVE,
                                 .sprite = 50,
                                 .position = { { 100 * 16, 100 * 16 } } };
    M_Add(0, 0);
    M_Update(fixture);
    CHECK_EQ_INT(m_Enemies.slots[0].dying, 1);
    CHECK_EQ_INT(m_Enemies.slots[0].damage, 6);
    CHECK_EQ_INT(p_Score->delta, 0x12340069);
    CHECK_EQ_INT(g_Items->slots[0].kind, ITEM_POINT);
    CHECK_EQ_INT(g_Items->slots[0].position[0].x, 112);
    CHECK_EQ_INT(g_Sparks->next, 1);
    CHECK_EQ_INT(p_Bullets->slots[0].state, ENTITY_ACTIVE);
    SoundEffects_Update();
    TestSound_Check(&fixture->sound, 3);
    M_Update(fixture);
    CHECK_EQ_INT(g_Sparks->next, 9);
    CHECK_EQ_INT(m_Enemies.slots[0].wait_frame, 1);
    M_Free(fixture);
}

TEST(invincible_formations_and_script_return_carry_keep_their_order)
{
    FIXTURE *const fixture = M_New();
    fixture->formations[0].hp = -2;
    m_Bomb.active = 1;
    M_Add(0, 0);
    M_Update(fixture);
    CHECK_EQ_INT(m_Enemies.slots[0].damage, 0);
    CHECK_EQ_INT(m_Enemies.slots[0].dying, 0);
    SoundEffects_Update();
    TestSound_Check(&fixture->sound, 4);
    CHECK_EQ_INT(p_Score->delta, 1);
    m_Bomb.active = 0;
    fixture->scripts[0].bytes[0] = 0x11;
    M_Add(1, 1);
    m_Enemies.slots[1].dying = 1;
    m_Enemies.slots[1].age = 42;
    M_Update(fixture);
    CHECK_EQ_INT(m_Enemies.slots[1].wait_frame, 0);
    CHECK_EQ_INT(m_Enemies.slots[1].age, 42);
    M_Free(fixture);
}

TEST(player_collision_uses_front_x_and_current_y_without_testing_protection)
{
    FIXTURE *const fixture = M_New();
    M_Add(0, 0);
    m_Player.current = (ENTITY_POINT) { 300, 100 };
    m_Player.position[1].x = 100;
    m_Player.invincibility = 99;
    m_Player.hit = 255;
    M_Update(fixture);
    CHECK_EQ_INT(m_Player.hit, 1);
    CHECK_EQ_INT(m_Enemies.slots[0].dying, 1);
    CHECK_EQ_INT(m_Enemies.slots[0].wait_frame, 0);
    SoundEffects_Update();
    TestSound_Check(&fixture->sound, -1);
    CHECK_EQ_INT(p_Shots->target.x, 108);
    M_Add(0, 0);
    m_Player.position[1].x = 300;
    m_Player.hit = 255;
    M_Update(fixture);
    CHECK_EQ_INT(m_Player.hit, 255);
    CHECK_EQ_INT(m_Enemies.slots[0].dying, 0);
    M_Free(fixture);
}

static uint64_t M_HashValue(uint64_t hash, const uint32_t value)
{
    for (size_t byte = 0; byte < 4; byte++) {
        hash ^= (value >> (byte * 8)) & 255;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t M_HashState(uint64_t hash)
{
    hash = M_HashValue(hash, m_Enemies.count);
    hash = M_HashValue(hash, (uint32_t)m_Enemies.line);
    for (size_t i = 0; i < ENEMY_COUNT; i++) {
        const ENEMY *const enemy = &m_Enemies.slots[i];
        const int32_t values[] = {
            enemy->position[0].x,
            enemy->position[0].y,
            enemy->position[1].x,
            enemy->position[1].y,
            enemy->script_ip,
            enemy->wait_frame,
            enemy->formation,
            enemy->state,
            enemy->age,
            enemy->dying,
            enemy->reserved_11,
            enemy->image,
            enemy->animation,
            enemy->loop_i,
            enemy->spawned_left,
            enemy->loop_j,
            enemy->velocity_x,
            enemy->velocity_y,
            enemy->clip_y,
            enemy->reserved_1f,
            enemy->damage,
            enemy->hit_disabled,
            enemy->collision_disabled,
            enemy->bullet_pattern,
            enemy->bullet_speed,
        };
        for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
            hash = M_HashValue(hash, (uint32_t)values[j]);
        }
    }
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const bullet = &p_Bullets->slots[i];
        const int32_t values[] = {
            bullet->state,         bullet->kind,          bullet->position[0].x,
            bullet->position[0].y, bullet->position[1].x, bullet->position[1].y,
            bullet->velocity.x,    bullet->velocity.y,    bullet->sprite,
            bullet->motion,        bullet->angle,         bullet->speed,
            bullet->phase,         bullet->reserved,
        };
        for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
            hash = M_HashValue(hash, (uint32_t)values[j]);
        }
    }
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        const ITEM *const item = &g_Items->slots[i];
        const int32_t values[] = {
            item->state,         item->kind,          item->position[0].x,
            item->position[0].y, item->position[1].x, item->position[1].y,
            item->speed_y,       item->drift_x,       item->age,
        };
        for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
            hash = M_HashValue(hash, (uint32_t)values[j]);
        }
    }
    hash = M_HashValue(hash, g_Sparks->next);
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        const SPARK *const spark = &g_Sparks->slots[i];
        const int32_t values[] = {
            spark->state,         spark->age,           spark->position[0].x,
            spark->position[0].y, spark->position[1].x, spark->position[1].y,
            spark->velocity.x,    spark->velocity.y,    spark->drawing,
            spark->reserved_1,    spark->angle,         spark->base_speed,
            spark->reserved_2,    spark->preferred,
        };
        for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
            hash = M_HashValue(hash, (uint32_t)values[j]);
        }
    }
    hash = M_HashValue(hash, m_Player.hit);
    hash = M_HashValue(hash, (uint32_t)p_Shots->target.x);
    hash = M_HashValue(hash, (uint32_t)p_Shots->target.y);
    hash = M_HashValue(hash, (uint32_t)p_Score->delta);
    hash = M_HashValue(hash, p_Rng->seed);
    hash = M_HashValue(hash, p_Rng->cursor);
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        hash = M_HashValue(hash, p_Rng->ring[i]);
    }
    return hash;
}

TEST(script_commands_pages_ranks_and_dying_slots_keep_state_pixels_and_sound)
{
    static const uint8_t scripts[][16] = {
        { 0x00, 2, 0x10 },
        { 0x01, 2, 255, 2, 0x10 },
        { 0x08, 2, 255, 2, 0x10 },
        { 0x0a, 2, 0x10 },
        { 0xaa, 48, 0x00, 2, 0x10 },
        { 0x09, 2, 48, 0x10 },
        { 0x02, 2, 0x10 },
        { 0x03, 2, 1, 64, 2, 48, 0x10 },
        { 0x07, 2, 64, 2, 48, 0x10 },
        { 0x0b, 2, 1, 64, 2, 48, 0x10 },
        { 0x04, 12, 12, 0x00, 2, 0x10 },
        { 0x05, 8, 8, 12, 0x00, 2, 0x10 },
        { 0x06, 8, 8, 12, 0x00, 2, 0x10 },
        { 0x10 },
        { 0x11 },
        { 0x20, 0x00, 2, 0x10 },
        { 0x21, 0, 0x00, 2, 0x10 },
        { 0x22, 7, 0x00, 2, 0x10 },
        { 0x25, BULLET_SINGLE, 64, 0x00, 2, 0x10 },
        { 0x24, 0, 50, 0x00, 2, 0x10 },
        { 0x26, 100, 0, BULLET_SINGLE, 64, 0x00, 2, 0x10 },
        { 0x27, 100, 0, BULLET_MOTION_CHASE, 50, 64, 0x00, 2, 0x10 },
        { 0xa0, 2, 0x00, 2, 0x10 },
        { 0xa1, 0, 2, 0x00, 2, 0x10 },
        { 0xa7, 0, 2, 0x00, 2, 0x10 },
        { 0xa2, 1, 0x00, 2, 0x10 },
        { 0xa3, 0x00, 2, 0x10 },
        { 0xa4, 0x00, 2, 0x10 },
        { 0xa5, 1, 0x00, 2, 0x10 },
        { 0xa6, 10, 0x00, 2, 0x10 },
        { 0xa9, 0x00, 2, 0x10 },
        { 0xa8, 4, 0x00, 2, 0x10 },
        { 0xab, 0x00, 2, 0x10 },
        { 0xac, 0x00, 2, 0x10 },
        { 0xad, 0x00, 2, 0x10 },
        { 0xae, 0x00, 2, 0x10 },
        { 0xaf, 32, 9, 0x00, 2, 0x10 },
    };
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t script = 0; script < ARRAY_SIZE(scripts); script++) {
        for (uint8_t back = 0; back < 2; back++) {
            for (uint8_t rank = 0; rank < 4; rank++) {
                FIXTURE *const fixture = M_New();
                fixture->patterns.entries[20] = fixture->patterns.entries[10];
                fixture->pages.back = back;
                fixture->pages.front = back ^ 1;
                fixture->pages.scroll = rank * 127;
                fixture->scrolled = rank & 1;
                fixture->rank = rank;
                fixture->bomb_frame_mask = rank & 1;
                p_Performance->value = rank * 24;
                p_Shots->weapon.level = rank;
                m_Bomb.active = rank == 3 ? 1 : 0;
                m_Player.current =
                    (ENTITY_POINT) { rank == 2 ? 100 : 300, 100 };
                m_Player.position[back ^ 1] = m_Player.current;
                p_Rng->cursor = 255;
                for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
                    p_Rng->ring[i] = (uint8_t)(i ^ 0x5a);
                }
                memcpy(
                    fixture->scripts[0].bytes, scripts[script],
                    sizeof(scripts[script]));
                fixture->formations[0].hp = 3;
                fixture->formations[0].image_count = 3;
                fixture->formations[0].frames_per_image = 1;
                fixture->formations[0].item = ITEM_BOMB + ITEM_POINT;
                for (uint8_t slot = 0; slot < 3; slot++) {
                    Enemies_Add(
                        back, 100 + slot * 32, 100, slot, slot == 0 ? 0 : 1);
                    m_Enemies.slots[slot].position[back ^ 1] =
                        (ENTITY_POINT) { 32, 32 };
                }
                m_Enemies.count = 3;
                m_Enemies.slots[0].age = 255;
                m_Enemies.slots[0].velocity_x = 3;
                m_Enemies.slots[0].velocity_y = -3;
                m_Enemies.slots[0].bullet_pattern = BULLET_SINGLE;
                m_Enemies.slots[0].bullet_speed = 64;
                m_Enemies.slots[1].dying = 1;
                m_Enemies.slots[1].wait_frame = 23;
                if (rank == 1) {
                    p_Shots->slots[0] = (SHOT) {
                        .state = ENTITY_ACTIVE,
                        .sprite = 50,
                    };
                    p_Shots->slots[0].position[back] =
                        (ENTITY_POINT) { 100 * 16, 100 * 16 };
                }
                for (uint32_t frame = 0; frame < 6; frame++) {
                    fixture->frame = frame;
                    Video_AccessPage(back);
                    M_Update(fixture);
                    hash = M_HashState(hash);
                    SoundEffects_Update();
                    hash = TestSound_Hash(&fixture->sound, hash);
                }
                for (size_t page = 0; page < 2; page++) {
                    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
                        hash = (hash ^ g_Video.pages[page][i])
                            * UINT64_C(1099511628211);
                    }
                }
                M_Free(fixture);
            }
        }
    }
    CHECK(hash == UINT64_C(7549611679071084817));
}
