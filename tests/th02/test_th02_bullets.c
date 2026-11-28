#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_sprites.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/bullets.h"
#include "th02/bullets/private.h"
#include "th02/performance_private.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/sparks.h"
#include "th02/sparks_private.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

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
    const int32_t parameters[] = {
        p_Bullets->motion_a, p_Bullets->motion_b, p_Bullets->turn_limit,
        p_Spawn->rank,       p_Spawn->speed_bias, p_Spawn->base_layers,
        p_Spawn->layers,     p_Spawn->slow_cycle,
    };
    for (size_t i = 0; i < ARRAY_SIZE(parameters); i++) {
        hash = M_HashValue(hash, (uint32_t)parameters[i]);
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
    hash = M_HashValue(hash, p_Rng->seed);
    hash = M_HashValue(hash, p_Rng->cursor);
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        hash = M_HashValue(hash, p_Rng->ring[i]);
    }
    return hash;
}

TEST(stage_init_preserves_records_and_consumes_two_draws_per_spark)
{
    memset(p_Bullets, 0xa5, sizeof(*p_Bullets));
    memset(p_Spawn, 0xa5, sizeof(*p_Spawn));
    memset(p_Sparks, 0x5a, sizeof(*p_Sparks));
    const BULLET before_bullet = p_Bullets->slots[0];
    const SPARK before_spark = p_Sparks->slots[0];
    Random_Reset();
    memset(p_Rng->ring, 0x3c, sizeof(p_Rng->ring));
    p_Rng->cursor = 37;
    p_Sparks->next = 17;
    p_Sparks->acceleration_x = -1;
    Bullets_ResetStage(3, true);
    CHECK_EQ_INT(p_Rng->seed, UINT32_C(0x7db41981));
    CHECK_EQ_INT(p_Rng->cursor, 37);
    CHECK_EQ_INT(p_Rng->ring[0], 0x3c);
    CHECK_EQ_INT(p_Rng->ring[255], 0x3c);
    CHECK_EQ_INT(p_Sparks->slots[0].angle, 90);
    CHECK_EQ_INT(p_Sparks->slots[0].base_speed, 2);
    CHECK_EQ_INT(p_Sparks->slots[63].angle, 228);
    CHECK_EQ_INT(p_Sparks->slots[63].base_speed, 52);
    CHECK_EQ_INT(p_Sparks->next, 17);
    CHECK_EQ_INT(p_Sparks->acceleration_x, -1);
    CHECK_EQ_INT(p_Sparks->sprite_interval, 7);
    CHECK_EQ_INT(p_Sparks->age_limit, 24);
    size_t sprites = 0;
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        CHECK_EQ_INT(p_Sparks->slots[i].state, ENTITY_FREE);
        sprites += p_Sparks->slots[i].preferred == 14;
    }
    CHECK_EQ_INT(sprites, 8);
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        CHECK_EQ_INT(p_Bullets->slots[i].state, ENTITY_FREE);
    }
    CHECK(
        memcmp(
            p_Bullets->slots[0].position, before_bullet.position,
            sizeof(before_bullet.position))
        == 0);
    CHECK_EQ_INT(p_Bullets->slots[0].reserved, before_bullet.reserved);
    CHECK_EQ_INT(p_Sparks->slots[0].age, before_spark.age);
    CHECK_EQ_INT(p_Sparks->slots[0].drawing, before_spark.drawing);
    CHECK_EQ_INT(p_Sparks->slots[0].reserved_1, before_spark.reserved_1);
    CHECK_EQ_INT(p_Sparks->slots[0].reserved_2, before_spark.reserved_2);
    CHECK_EQ_INT(p_Spawn->slow_cycle, 0xa5);
    CHECK_EQ_INT(p_Spawn->speed_bias, 12);
    CHECK_EQ_INT(p_Bullets->motion_a, 1);
    CHECK_EQ_INT(p_Bullets->motion_b, 64);
    CHECK_EQ_INT(p_Bullets->turn_limit, 1);
}

TEST(invalidation_marks_old_positions_before_freeing_or_copying)
{
    Bullets_Init();
    p_Bullets->slots[1] = (BULLET) {
        .state = ENTITY_ACTIVE,
        .kind = BULLET_PELLET,
        .position = { { 32 * 16 + 15, -17 * 16 - 1 }, { 64 * 16, 16 * 16 } },
    };
    p_Bullets->slots[2] = (BULLET) {
        .state = ENTITY_REMOVE,
        .kind = BULLET_SPRITE,
        .position = { { 48 * 16 + 15, 17 * 16 - 1 }, { 80 * 16, 48 } },
    };
    p_Bullets->slots[3] = (BULLET) {
        .state = 255,
        .kind = -2,
        .position = { { 64 * 16, 31 }, { -128, 144 } },
    };
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    Bullets_Invalidate((PLAYFIELD_PAGES) { 0, 1, 0 });
    CHECK_EQ_INT(p_Background->cache.images[24][23], 1);
    CHECK_EQ_INT(p_Background->cache.reserved, 1);
    CHECK_EQ_INT(p_Background->cache.dirty[1][1], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[2][1], 0);
    CHECK_EQ_INT(p_Bullets->slots[1].position[0].x, 64 * 16);
    CHECK_EQ_INT(p_Bullets->slots[1].position[0].y, 16 * 16);
    CHECK_EQ_INT(p_Bullets->slots[2].state, ENTITY_FREE);
    CHECK_EQ_INT(p_Bullets->slots[2].position[0].x, 48 * 16 + 15);
    CHECK_EQ_INT(p_Bullets->slots[3].position[0].x, -128);
    CHECK_EQ_INT(p_Bullets->slots[3].state, 255);
}

TEST(remove_all_marks_previous_footprints_and_repeated_call_frees)
{
    Bullets_Init();
    p_Bullets->slots[1].state = ENTITY_ACTIVE;
    p_Bullets->slots[1].kind = BULLET_PELLET;
    p_Bullets->slots[1].position[0] = (ENTITY_POINT) { 32 * 16, 16 * 16 };
    p_Bullets->slots[1].position[1] = (ENTITY_POINT) { 64 * 16, 32 * 16 };
    p_Bullets->slots[2].state = ENTITY_REMOVE;
    p_Bullets->slots[2].kind = BULLET_SPRITE;
    p_Bullets->slots[2].position[0] = (ENTITY_POINT) { 80 * 16, 48 * 16 };
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    Bullets_RemoveAll((PLAYFIELD_PAGES) { 0, 1, 0 });
    CHECK_EQ_INT(p_Background->cache.dirty[0][1], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[3][3], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[2][2], 0);
    CHECK_EQ_INT(p_Bullets->slots[1].state, ENTITY_REMOVE);
    CHECK_EQ_INT(p_Bullets->slots[1].position[0].x, 64 * 16);
    CHECK_EQ_INT(p_Bullets->slots[2].state, ENTITY_FREE);
    Bullets_RemoveAll((PLAYFIELD_PAGES) { 0, 1, 0 });
    CHECK_EQ_INT(p_Background->cache.dirty[2][2], 1);
    CHECK_EQ_INT(p_Bullets->slots[1].state, ENTITY_FREE);
    Bullets_Invalidate((PLAYFIELD_PAGES) { 1, 0, 0 });
    CHECK_EQ_INT(p_Background->cache.dirty[0][1], 1);
}

TEST(spawn_patterns_layers_pages_and_partial_pools_keep_state_and_rng)
{
    static const int16_t performance[] = { -48, 0, 256 };
    static const uint8_t patterns[] = {
        0,
        BULLET_FAN2,
        BULLET_FAN2_MEDIUM_AIMED,
        BULLET_FAN3_WIDE,
        BULLET_FAN4_WIDE_AIMED,
        BULLET_FAN5_WIDE_AIMED,
        BULLET_AIMED,
        BULLET_RANDOM_ONE,
        BULLET_RANDOM,
        BULLET_RANDOM_SPEED,
        BULLET_SINGLE,
        BULLET_RING4,
        BULLET_RING2,
        BULLET_MIRROR,
        BULLET_RING8,
        BULLET_RING16,
        BULLET_FAN2_EXTRA_WIDE_AIMED,
        BULLET_RING32,
        BULLET_MOTION_CHASE,
        BULLET_MOTION_GRAVITY,
        255,
    };
    uint64_t hash = UINT64_C(14695981039346656037);
    for (int8_t rank = 0; rank < 4; rank++) {
        for (uint8_t back = 0; back < 2; back++) {
            for (uint8_t bomb = 0; bomb < 2; bomb++) {
                for (size_t perf = 0; perf < ARRAY_SIZE(performance); perf++) {
                    for (size_t layout = 0; layout < 3; layout++) {
                        for (size_t pattern = 0; pattern < ARRAY_SIZE(patterns);
                             pattern++) {
                            Player_Init();
                            p_Player->current = (ENTITY_POINT) { 208, 336 };
                            p_Bomb->active = bomb;
                            p_Performance->value = performance[perf];
                            memset(p_Bullets, 0xa5, sizeof(*p_Bullets));
                            BulletSpawn_Init(rank);
                            BulletSpawn_Layers(2);
                            p_Spawn->slow_cycle = 255;
                            for (size_t i = 0; i < BULLET_COUNT; i++) {
                                p_Bullets->slots[i].state = layout == 0
                                        || (layout == 1 && (i == 4 || i == 149))
                                    ? ENTITY_FREE
                                    : ENTITY_REMOVE;
                            }
                            Random_Reset();
                            Random_Seed(0x12345678);
                            p_Rng->cursor = 255;
                            for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
                                p_Rng->ring[i] = (uint8_t)(i ^ 0x5a);
                            }
                            Bullets_AddPellets(
                                back, 100, 40, 3, patterns[pattern], 24);
                            hash = M_HashState(hash);
                            Bullets_AddSprites(
                                back, 416, 383, 3, patterns[pattern], 300, 33);
                            hash = M_HashState(hash);
                        }
                    }
                }
            }
        }
    }
    CHECK(hash == UINT64_C(16973084403680690885));
}

TEST(pellet_layers_preserve_unwritten_bytes_and_the_other_page)
{
    memset(p_Bullets, 0xa5, sizeof(*p_Bullets));
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        p_Bullets->slots[i].state = ENTITY_FREE;
    }
    BulletSpawn_Init(1);
    p_Spawn->layers = 2;
    Random_Reset();
    Player_Init();
    p_Player->current = (ENTITY_POINT) { 208, 336 };
    p_Performance->value = 0;
    Bullets_AddPellets(1, 100, 40, 0, BULLET_RING4, 128);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    for (size_t i = 0; i < 12; i++) {
        const BULLET *const bullet = &p_Bullets->slots[i];
        CHECK_EQ_INT(bullet->state, ENTITY_ACTIVE);
        CHECK_EQ_INT(bullet->kind, BULLET_PELLET);
        CHECK_EQ_INT(bullet->position[1].x, 1600);
        CHECK_EQ_INT(bullet->position[1].y, 640);
        CHECK_EQ_INT(bullet->position[0].x, (int16_t)0xa5a5);
        CHECK_EQ_INT(bullet->sprite, 0xa5);
        CHECK_EQ_INT(bullet->speed, 0xa5);
        CHECK_EQ_INT(bullet->reserved, 0xa5);
        CHECK_EQ_INT(bullet->motion, 0);
        CHECK_EQ_INT(bullet->phase, 0);
    }
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.x, 128);
    CHECK_EQ_INT(p_Bullets->slots[4].velocity.x, 64);
    CHECK_EQ_INT(p_Bullets->slots[8].velocity.x, 32);
    CHECK_EQ_INT(p_Bullets->slots[12].state, ENTITY_FREE);
}

TEST(partial_random_volley_consumes_only_allocated_slots)
{
    Bullets_Init();
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        p_Bullets->slots[i].state = ENTITY_REMOVE;
    }
    p_Bullets->slots[4].state = p_Bullets->slots[149].state = ENTITY_FREE;
    BulletSpawn_Init(3);
    Random_Reset();
    p_Rng->cursor = 255;
    p_Rng->ring[255] = 0;
    p_Rng->ring[0] = 7;
    p_Rng->ring[1] = 64;
    p_Rng->ring[2] = 31;
    Player_Init();
    p_Player->current = (ENTITY_POINT) { 208, 336 };
    p_Performance->value = 0;
    Bullets_AddSprites(0, 100, 40, 3, BULLET_RANDOM_SPEED, 300, 64);
    CHECK_EQ_INT(p_Rng->cursor, 3);
    CHECK_EQ_INT(p_Bullets->slots[4].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(p_Bullets->slots[149].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(p_Bullets->slots[5].state, ENTITY_REMOVE);
    CHECK_EQ_INT(p_Bullets->slots[4].sprite, 44);
    CHECK_EQ_INT(p_Bullets->slots[4].speed, 76);
    CHECK_EQ_INT(p_Bullets->slots[4].angle, 3);
    CHECK_EQ_INT(p_Bullets->slots[4].motion, BULLET_RANDOM_SPEED);
    CHECK_EQ_INT(p_Bullets->slots[4].phase, 0);
}

TEST(gravity_bypasses_tuning_but_full_and_rejected_spawns_keep_rng)
{
    Bullets_Init();
    BulletSpawn_Init(0);
    Random_Reset();
    Player_Init();
    p_Player->current = (ENTITY_POINT) { 208, 336 };
    p_Performance->value = -48;
    Bullets_AddSprites(0, 100, 40, 0, BULLET_MOTION_GRAVITY, 55, 3);
    CHECK_EQ_INT(p_Bullets->slots[0].speed, 3);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.x, 3);
    CHECK_EQ_INT(p_Spawn->slow_cycle, 0);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        p_Bullets->slots[i].state = ENTITY_ACTIVE;
    }
    Bullets_AddPellets(0, 100, 40, 0, BULLET_RANDOM_SPEED, 3);
    CHECK_EQ_INT(p_Spawn->slow_cycle, 1);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    p_Bomb->active = 1;
    Bullets_AddPellets(0, 100, 40, 0, BULLET_RANDOM_SPEED, 3);
    CHECK_EQ_INT(p_Spawn->slow_cycle, 1);
    p_Bomb->active = 0;
    Bullets_AddPellets(0, 23, 40, 0, BULLET_RANDOM_SPEED, 3);
    CHECK_EQ_INT(p_Spawn->slow_cycle, 1);
    CHECK_EQ_INT(p_Rng->cursor, 0);
}

typedef struct {
    ARENA arena;

    PATTERN_BANK patterns;
    BULLET_IMAGES images;
    uint32_t frame;
    int16_t scroll;
    uint8_t back;
    uint8_t tiny[36];
} FRAME_FIXTURE;

static FRAME_FIXTURE *M_Frame(void)
{
    Bullets_Init();
    Player_Init();
    Grcg_Init();
    memset(p_Rng, 0, sizeof(*p_Rng));
    FRAME_FIXTURE *const fixture = Mem_AllocZ(sizeof(*fixture));
    Arena_Init(&fixture->arena, "bullet frame", 1024 * 1024);
    Video_Init(Arena_Allocator(&fixture->arena));
    TestClip_Set(0, 0, 639, 399);
    p_Player->current = (ENTITY_POINT) { 208, 336 };
    fixture->patterns.entries[50] = (PATTERN) {
        .size = 0x0210,
        .data = fixture->tiny,
        .storage_size = sizeof(fixture->tiny),
    };
    return fixture;
}

static void M_Step(FRAME_FIXTURE *const fixture)
{
    Bullets_Update(
        (PLAYFIELD_PAGES) { fixture->back, fixture->back, fixture->scroll },
        fixture->frame, &fixture->patterns, &fixture->images);
}

static void M_FreeFrame(FRAME_FIXTURE *const fixture)
{
    Arena_Free(&fixture->arena);
    Mem_Free(fixture);
}

static BULLET M_Bullet(const uint8_t motion)
{
    return (BULLET) {
        .state = ENTITY_ACTIVE,
        .kind = BULLET_SPRITE,
        .position = { { 1600, 1600 }, { 321, 654 } },
        .velocity = { -1, 1 },
        .sprite = 50,
        .motion = motion,
        .speed = 64,
    };
}

TEST(pellet_mask_loading_owns_pre_shifted_rows)
{
    VFS *const vfs = Th02Test_Sprites();
    BULLET_IMAGES images;
    CHECK(IS_OK(BulletImages_Load(vfs, &images)));
    CHECK_EQ_INT(images.rows[0][0], 0x003c);
    CHECK_EQ_INT(images.rows[7][7], 0x7800);
    Vfs_Destroy(vfs);
}

TEST(missing_pellet_sheet_leaves_images_empty)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    BULLET_IMAGES images;
    memset(&images, 0xa5, sizeof(images));
    CHECK(!Result_Ignore(BulletImages_Load(vfs, &images)));
    CHECK_EQ_INT(images.rows[0][0], 0);
    Vfs_Destroy(vfs);
}

TEST(special_motion_changes_velocity_after_movement_and_keeps_other_page)
{
    FRAME_FIXTURE *const fixture = M_Frame();
    p_Bullets->motion_a = 3;
    p_Bullets->slots[0] = M_Bullet(BULLET_MOTION_CHASE);
    p_Bullets->slots[1] = M_Bullet(BULLET_MOTION_GRAVITY);
    M_Step(fixture);
    CHECK_EQ_INT(p_Bullets->slots[0].position[0].x, 1599);
    CHECK_EQ_INT(p_Bullets->slots[0].position[0].y, 1601);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.x, 2);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.y, 4);
    CHECK_EQ_INT(p_Bullets->slots[0].position[1].x, 321);
    CHECK_EQ_INT(p_Bullets->slots[1].velocity.x, -1);
    CHECK_EQ_INT(p_Bullets->slots[1].velocity.y, 4);
    M_FreeFrame(fixture);
}

TEST(slowing_rounds_toward_zero_and_homing_keeps_byte_phase_wrap)
{
    FRAME_FIXTURE *const fixture = M_Frame();
    p_Bullets->motion_b = 256;
    p_Bullets->turn_limit = 1;
    p_Player->current = (ENTITY_POINT) { 91, 92 };
    p_Bullets->slots[0] = M_Bullet(BULLET_MOTION_SLOW_THEN_AIM);
    p_Bullets->slots[1] = M_Bullet(BULLET_MOTION_HOMING);
    p_Bullets->slots[1].phase = 255;
    M_Step(fixture);
    CHECK_EQ_INT(p_Bullets->slots[0].phase, 1);
    CHECK_EQ_INT(p_Bullets->slots[0].motion, 0);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.x, 64);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.y, 0);
    CHECK_EQ_INT(p_Bullets->slots[1].phase, 0);
    CHECK_EQ_INT(p_Bullets->slots[1].motion, BULLET_MOTION_HOMING);
    CHECK_EQ_INT(p_Bullets->slots[1].velocity.x, 64);
    fixture->frame = 1;
    p_Bullets->slots[0] = M_Bullet(BULLET_MOTION_SLOW_THEN_AIM);
    M_Step(fixture);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.x, -1);
    CHECK_EQ_INT(p_Bullets->slots[0].phase, 0);
    M_FreeFrame(fixture);
}

TEST(drift_and_corner_bounces_wrap_bytes_before_testing_signed_limits)
{
    FRAME_FIXTURE *const fixture = M_Frame();
    p_Bullets->motion_a = 1;
    p_Bullets->motion_b = 2;
    p_Bullets->turn_limit = 0;
    p_Bullets->slots[0] = M_Bullet(BULLET_MOTION_DRIFT);
    p_Bullets->slots[0].angle = 255;
    p_Bullets->slots[0].speed = 255;
    p_Bullets->slots[0].phase = 255;
    p_Bullets->slots[1] = M_Bullet(BULLET_MOTION_BOUNCE);
    p_Bullets->slots[1].position[0] = (ENTITY_POINT) { 30 * 16, 10 * 16 };
    p_Bullets->slots[1].phase = 254;
    M_Step(fixture);
    CHECK_EQ_INT(p_Bullets->slots[0].angle, 0);
    CHECK_EQ_INT(p_Bullets->slots[0].speed, 1);
    CHECK_EQ_INT(p_Bullets->slots[0].phase, 0);
    CHECK_EQ_INT(p_Bullets->slots[0].motion, BULLET_MOTION_DRIFT);
    CHECK_EQ_INT(p_Bullets->slots[0].velocity.x, 1);
    CHECK_EQ_INT(p_Bullets->slots[1].phase, 0);
    CHECK_EQ_INT(p_Bullets->slots[1].motion, BULLET_MOTION_BOUNCE);
    CHECK_EQ_INT(p_Bullets->slots[1].velocity.x, 1);
    CHECK_EQ_INT(p_Bullets->slots[1].velocity.y, -1);
    M_FreeFrame(fixture);
}

TEST(collision_edges_override_existing_hit_and_removed_slots_do_not_draw)
{
    FRAME_FIXTURE *const fixture = M_Frame();
    p_Player->hit = 255;
    for (size_t i = 0; i < 4; i++) {
        p_Bullets->slots[i] = M_Bullet(0);
        p_Bullets->slots[i].kind = BULLET_PELLET;
        p_Bullets->slots[i].velocity = (ENTITY_POINT) {};
    }
    p_Bullets->slots[0].position[0] = (ENTITY_POINT) { 215 * 16, 348 * 16 };
    p_Bullets->slots[1].position[0] = (ENTITY_POINT) { 225 * 16, 358 * 16 };
    p_Bullets->slots[2].position[0] = (ENTITY_POINT) { 214 * 16, 348 * 16 };
    p_Bullets->slots[3].position[0] = (ENTITY_POINT) { 20 * 16 + 15, 100 * 16 };
    M_Step(fixture);
    CHECK_EQ_INT(p_Player->hit, 1);
    CHECK_EQ_INT(p_Bullets->slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(p_Bullets->slots[1].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(p_Bullets->slots[2].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(p_Bullets->slots[3].state, ENTITY_REMOVE);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    M_FreeFrame(fixture);
}

TEST(pellets_and_tiny_sprites_keep_order_and_roll_rows_with_cached_masks)
{
    FRAME_FIXTURE *const fixture = M_Frame();
    memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);
    fixture->scroll = 299;
    fixture->tiny[0] = 0x80;
    fixture->tiny[1] = 3;
    fixture->tiny[2] = 0x80;
    for (size_t shift = 0; shift < 8; shift++) {
        fixture->images.rows[shift][0] = 0x0008;
        fixture->images.rows[shift][1] = 0x0008;
    }
    for (size_t i = 0; i < 3; i++) {
        p_Bullets->slots[i] = M_Bullet(0);
        p_Bullets->slots[i].velocity = (ENTITY_POINT) {};
    }
    p_Bullets->slots[0].kind = BULLET_PELLET;
    p_Bullets->slots[2].kind = BULLET_PELLET;
    p_Bullets->slots[2].position[0].x += 8 * 16;
    M_Step(fixture);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 100], 3);
    CHECK_EQ_INT(g_Video.pages[0][100], 15);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 108], 15);
    CHECK_EQ_INT(g_Video.pages[1][100], 9);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(g_Grcg->tiles[plane], 255);
    }
    M_FreeFrame(fixture);
}
