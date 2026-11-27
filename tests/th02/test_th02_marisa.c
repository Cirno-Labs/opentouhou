#include "th02/background/private.h"
#include "th02/boss/fight.h"
#include "th02/bullets/private.h"
#include "th02/stage/private.h"
#include "th02_score_fixture.h"
#include "th02_random_fixture.h"
#include "th02_clip_fixture.h"
#include "th02_palette_fixture.h"
#include "th02_player_fixture.h"
#include "../../src/th02/performance.c"
#include "../../src/th02/enemies.c"
#include "th02/boss/marisa/logic.c"
#include "th02/grcg.h"
#include "th02/items.h"
#include "th02/pillars.h"
#include "th02/sparks.h"
#include "th02/stage_effects.h"
#include "th02/video.h"

#include "core/base/arena.h"
#include "test.h"

typedef struct {
    ARENA arena;
    PATTERN_BANK patterns;
    TH02_SESSION session;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    memset(f, 0, sizeof(*f));
    Random_Reset();
    Clip_Reset();
    Palette_Init();
    Arena_Init(&f->arena, "marisa test", 2 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&f->arena);
    Video_Init(allocator);
    memset(g_Video.pages[0], 0, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 0, VIDEO_PAGE_SIZE);
    Grcg_Init();
    PatternBank_Init(&f->patterns, allocator);
    uint8_t body[32 * 48];
    memset(body, 5, sizeof(body));
    for (uint16_t slot = 0; slot < 48; slot++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&f->patterns, slot, 32, 48, body, 0)));
    }
    uint8_t tiny[256];
    memset(tiny, 9, sizeof(tiny));
    for (uint16_t slot = 48; slot < 128; slot++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&f->patterns, slot, 16, 16, tiny, 0)));
        CHECK(Result_Ignore(PatternBank_ConvertTiny(&f->patterns, slot)));
    }
    uint8_t boss[64 * 96];
    memset(boss, 6, sizeof(boss));
    for (uint16_t slot = 128; slot < 164; slot++) {
        CHECK(Result_Ignore(PatternBank_Put(
            &f->patterns, slot, 64, slot < 148 ? 64 : 96, boss, 0)));
    }
    Stage_Init();
    Background_Init();
    StageEffects_Init();
    Pillars_Init();
    Pillars_ResetStage(3);
    Bullets_Init();
    BulletSpawn_Init(0);
    Score_Init();
    Shots_Init();
    Shots_ResetTarget();
    Player_Init();
    Bomb_Init();
    Miss_Init();
    Enemies_Init();
    Items_Init();
    Sparks_Init();
    SoundEffects_Init(nullptr);
    Boss_Init(p_BossState);
    Marisa_Init();
}

// Overrides slots 128..137 with 48x96 bodies, matching Marisa's own bank
// layout rather than the fixture's generic boss-sized filler.
static void M_MarisaPatterns(FIXTURE *const f)
{
    uint8_t pixels[48 * 96];
    memset(pixels, 6, sizeof(pixels));
    for (uint16_t id = 128; id < 138; id++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&f->patterns, id, 48, 96, pixels, 0)));
    }
}

TEST(stage_four_midboss_entry_targets_old_position_and_moves_page_zero)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 3;
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    p_Stage->scroll.line = 200;
    m_Marisa.position = (ENTITY_POINT) { 100, 80 };
    int16_t value;
    BossFight_MidbossUpdate(
        &f.patterns, f.session.stage, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Shots.target.x, 124);
    CHECK_EQ_INT(m_Shots.target.y, 104);
    CHECK_EQ_INT(p_BossState->position[0].x, 72);
    CHECK_EQ_INT(p_BossState->position[0].y, -14);
    CHECK_EQ_INT(p_BossState->position[1].x, 64);
    CHECK_EQ_INT(p_BossState->position[1].y, -16);
    CHECK_EQ_INT(m_Marisa.midboss_page, 0);
    CHECK_EQ_INT(m_Marisa.midboss_bullet_top, 32);
    CHECK_EQ_INT(p_BossState->sprite, 150);
    CHECK(m_Marisa.midboss_active);
    value = BossFight_MidbossInvalidate(f.session.stage);
    CHECK_EQ_INT(value, 1);
    CHECK_EQ_INT(m_Marisa.midboss_page, 1);
    CHECK_EQ_INT(p_BossState->position[1].x, 72);
    CHECK_EQ_INT(p_BossState->position[1].y, -14);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    Arena_Free(&f.arena);
}

TEST(stage_four_midboss_kill_requires_a_new_hit_inside_the_rolled_y_band)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 3;
    p_Stage->scroll.line = 48;
    m_Marisa.position = (ENTITY_POINT) { 192, 48 };
    p_BossState->position[0] = p_BossState->position[1] = m_Marisa.position;
    m_Marisa.phase = 1;
    p_BossState->phase_frame = 2;
    p_BossState->damage = 399;
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    p_Stage->world.midboss_row = 944;
    Marisa_MidbossUpdate(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(p_BossState->damage, 400);
    CHECK_EQ_INT(p_BossState->defeated, 0);
    p_Stage->scroll.line = 49;
    p_Stage->world.frame = 1;
    Marisa_MidbossUpdate(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(p_BossState->defeated, 0);
    CHECK_EQ_INT(m_Score.delta, 1);
    p_Stage->world.frame = 2;
    Marisa_MidbossUpdate(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(p_BossState->defeated, 1);
    CHECK_EQ_INT(p_BossState->damage, 401);
    CHECK_EQ_INT(m_Score.delta, 50002);
    CHECK_EQ_INT(g_Items->slots[0].kind, ITEM_BOMB);
    CHECK_EQ_INT(p_BossState->hit_flash, 0);
    m_Marisa.midboss_active = true;
    for (int frame = 0; frame < 64; frame++) {
        CHECK(Marisa_MidbossInvalidate(p_BossState));
        Marisa_MidbossUpdate(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    }
    CHECK(!m_Marisa.midboss_active);
    CHECK_EQ_INT(p_BossState->defeated, 2);
    CHECK_EQ_INT(p_BossState->phase_frame, 0);
    CHECK_EQ_INT(m_Marisa.direction, -1);
    CHECK_EQ_INT(g_Stage->world.midboss_row, 1632);
    CHECK_EQ_INT(m_Shots.target.x, -1);
    CHECK_EQ_INT(m_Shots.target.y, -1);
    CHECK(!Marisa_MidbossInvalidate(p_BossState));
    Arena_Free(&f.arena);
}

TEST(stage_four_second_midboss_enters_right_and_drops_a_life)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 3;
    m_Marisa.direction = -1;
    p_Stage->world.midboss_row = 1632;
    p_Stage->scroll.line = 0;
    Marisa_MidbossUpdate(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(p_BossState->position[0].x, 312);
    CHECK_EQ_INT(p_BossState->sprite, 149);
    m_Marisa.phase = 1;
    p_BossState->phase_frame = 3;
    p_BossState->position[0].y = 100;
    p_BossState->damage = 399;
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    m_Score.lives = 4;
    Marisa_MidbossUpdate(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(p_BossState->defeated, 1);
    CHECK_EQ_INT(g_Items->slots[0].kind, ITEM_LIFE);
    CHECK_EQ_INT(m_Score.delta, 50001);
    Arena_Free(&f.arena);
}

TEST(stage_four_midboss_timeout_does_not_award_and_stops_before_hit_testing)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 3;
    p_Stage->scroll.line = 0;
    m_Marisa.midboss_active = true;
    m_Marisa.phase = 1;
    m_Marisa.direction = -1;
    m_Marisa.midboss_patterns_done = 12;
    p_BossState->position[0] = (ENTITY_POINT) { 4, 100 };
    p_BossState->phase_frame = 51;
    p_BossState->damage = 500;
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    Marisa_MidbossUpdate(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK(!m_Marisa.midboss_active);
    CHECK_EQ_INT(m_Marisa.position.x, 0);
    CHECK_EQ_INT(p_BossState->phase_frame, 0);
    CHECK_EQ_INT(p_BossState->damage, 500);
    CHECK_EQ_INT(p_BossState->defeated, 0);
    CHECK_EQ_INT(g_Stage->world.midboss_row, 1632);
    CHECK_EQ_INT(m_Score.delta, 0);
    CHECK_EQ_INT(g_Items->slots[0].state, ENTITY_FREE);
    Arena_Free(&f.arena);
}

TEST(marisa_flash_uses_masks_and_keeps_shifted_edge_pixels)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    Video_AccessPage(1);
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 15, VIDEO_PAGE_SIZE);
    TestClip_Set(100, 100, 200, 200);
    p_BossState->sprite = 128;
    p_BossState->hit_flash = 1;
    m_Marisa.position = (ENTITY_POINT) { 415, 15 };
    Marisa_DrawBody(p_BossState, &f.patterns);
    CHECK_EQ_INT(p_BossState->hit_flash, 0);
    CHECK_EQ_INT(g_Video.pages[1][15 * 640 + 415], 15);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 414], 15);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 415], 4);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 422], 4);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 423], 15);
    CHECK_EQ_INT(g_Video.pages[1][110 * 640 + 415], 4);
    CHECK_EQ_INT(g_Video.pages[1][111 * 640 + 415], 15);
    CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 415], 9);
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_PAGE_SIZE - 1], 15);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[2], 255);
    CHECK_EQ_INT(g_Grcg->tiles[0], 0);
    Arena_Free(&f.arena);
}

TEST(marisa_orb_clear_ignores_x_clip_and_keeps_bytebox_row_traversal)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    m_Marisa.phase = 3;
    p_Stage->scroll.front = 1;
    p_Stage->scroll.back = 0;
    m_Marisa.bit_state[0] = 1;
    m_Marisa.bit_flash[0] = 47;
    m_Marisa.bit_hits[0] = 110;
    m_Marisa.bit_position[1][0] = (ENTITY_POINT) { -8, 0 };
    m_Marisa.bit_position[0][0] = (ENTITY_POINT) { 100, 100 };
    TestClip_Set(300, 0, 310, 399);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 15, VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Marisa.bit_state[0], 2);
    CHECK_EQ_INT(m_Marisa.bit_flash[0], 0);
    CHECK_EQ_INT(m_Marisa.bit_hits[0], 0);
    CHECK_EQ_INT(g_Video.pages[1][0], 15);
    CHECK_EQ_INT(g_Video.pages[1][632], 0);
    CHECK_EQ_INT(g_Video.pages[1][640], 0);
    CHECK_EQ_INT(g_Video.pages[1][640 + 39], 0);
    CHECK_EQ_INT(g_Video.pages[1][640 + 40], 15);
    CHECK_EQ_INT(g_Video.pages[1][32 * 640], 0);
    CHECK_EQ_INT(g_Video.pages[1][32 * 640 + 632], 15);
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_PAGE_SIZE - 1], 15);
    CHECK_EQ_INT(g_Video.pages[0][640], 15);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    Arena_Free(&f.arena);
}

TEST(marisa_particles_spawn_move_and_retain_the_pool_across_begin)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    m_Marisa.phase = 3;
    p_Stage->world.frame = 1;
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    memset(m_Rng.ring, 0, sizeof(m_Rng.ring));
    const uint8_t cursor = m_Rng.cursor;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    MARISA_DOT *const dot = &m_Marisa.dots[0];
    CHECK_EQ_INT(dot->flag, ENTITY_ACTIVE);
    CHECK_EQ_INT(dot->angle, 128);
    CHECK_EQ_INT(dot->speed, 28);
    CHECK_EQ_INT(dot->size, 1);
    CHECK_EQ_INT(dot->position[0].x, 32 * 16 - 28);
    CHECK_EQ_INT(dot->position[0].y, 16 * 16);
    CHECK_EQ_INT(dot->position[1].x, 32 * 16);
    CHECK_EQ_INT(m_Rng.cursor, (uint8_t)(cursor + 1));
    CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 30], 9);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(dot->flag, ENTITY_ACTIVE);
    CHECK_EQ_INT(dot->speed, 28);
    m_Marisa.phase = 3;
    p_Stage->world.frame = 2;
    dot->speed = 254;
    dot->angle = 0;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(dot->speed, 2);
    CHECK_EQ_INT(dot->size, 1);
    Arena_Free(&f.arena);
}

TEST(marisa_particle_cleanup_copies_pages_and_frees_skipped_slots)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    f.session.reduce_effects = true;
    MARISA_DOT *const alive = &m_Marisa.dots[0];
    *alive = (MARISA_DOT) {
        .flag = ENTITY_ACTIVE,
        .position = { { 8 * 16, 2 * 16 }, { 100 * 16, 90 * 16 } },
        .size = 2,
    };
    MARISA_DOT *const removed = &m_Marisa.dots[1];
    *removed = (MARISA_DOT) {
        .flag = ENTITY_REMOVE,
        .position = { { 32 * 16, 2 * 16 }, { 200 * 16, 90 * 16 } },
        .size = 2,
    };
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    Marisa_Invalidate(p_BossState, f.session.reduce_effects);
    CHECK_EQ_INT(alive->position[0].x, 100 * 16);
    CHECK_EQ_INT(alive->position[0].y, 90 * 16);
    CHECK_EQ_INT(removed->flag, ENTITY_FREE);
    CHECK_EQ_INT(removed->position[0].x, 32 * 16);
    CHECK_EQ_INT(g_Video.pages[0][2 * 640 + 7], 15);
    CHECK_EQ_INT(g_Video.pages[0][2 * 640 + 8], 0);
    CHECK_EQ_INT(g_Video.pages[0][3 * 640 + 23], 0);
    CHECK_EQ_INT(g_Video.pages[0][2 * 640 + 32], 15);
    CHECK_EQ_INT(g_Video.pages[1][2 * 640 + 8], 7);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    Arena_Free(&f.arena);
}

TEST(marisa_particles_cull_before_drawing_and_move_skipped_slots)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    m_Marisa.phase = 3;
    p_Stage->world.frame = 2;
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    f.session.reduce_effects = true;
    m_Marisa.dots[0] = (MARISA_DOT) {
        .flag = ENTITY_ACTIVE,
        .position = { { 416 * 16, 40 * 16 }, { 100 * 16, 40 * 16 } },
        .size = 5,
        .speed = 252,
    };
    m_Marisa.dots[1] = (MARISA_DOT) {
        .flag = ENTITY_ACTIVE,
        .position = { { 100 * 16, 40 * 16 }, { 100 * 16, 40 * 16 } },
        .size = 1,
    };
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Marisa.dots[0].flag, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Marisa.dots[0].size, 5);
    CHECK_EQ_INT(m_Marisa.dots[0].speed, 0);
    CHECK_EQ_INT(m_Marisa.dots[1].flag, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Marisa.dots[1].speed, 4);
    CHECK_EQ_INT(m_Marisa.dots[1].position[0].x, 100 * 16 + 4);
    CHECK_EQ_INT(g_Video.pages[0][40 * 640 + 100], 15);
    CHECK_EQ_INT(g_Video.pages[0][40 * 640 + 416], 15);
    Marisa_Invalidate(p_BossState, f.session.reduce_effects);
    CHECK_EQ_INT(m_Marisa.dots[0].flag, ENTITY_FREE);
    Arena_Free(&f.arena);
}

TEST(marisa_begin_preserves_world_and_retained_orb_history)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    f.session.stage = 3;
    p_Stage->world.frame = 9216;
    m_Marisa.bit_flash[0] = 7;
    m_Marisa.defeat_frame = 9;
    m_Marisa.drift_toggle = 254;
    m_Marisa.edge_direction = -1;
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(g_Stage->world.frame, 9216);
    CHECK_EQ_INT(p_BossState->position[0].x, 176);
    CHECK_EQ_INT(m_Marisa.position.y, 64);
    CHECK_EQ_INT(p_BossState->sprite, 128);
    CHECK_EQ_INT(p_BossState->phase_frame, 0);
    CHECK_EQ_INT(p_BossState->angle, 224);
    CHECK_EQ_INT(m_Marisa.bit_state[3], 2);
    CHECK_EQ_INT(m_Marisa.bit_flash[0], 7);
    CHECK_EQ_INT(m_Marisa.defeat_frame, 9);
    CHECK_EQ_INT(m_Marisa.drift_toggle, 254);
    CHECK_EQ_INT(m_Marisa.edge_direction, -1);
    CHECK_EQ_INT(m_Marisa.damage_enable, 0);
    CHECK_EQ_INT(m_Marisa.fan, BULLET_FAN5_WIDE_AIMED);
    CHECK_EQ_INT(p_Background->cache.mode, TILE_DISABLED);
    f.session.rank = 1;
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Marisa.fan, BULLET_FAN5_MEDIUM_AIMED);
    CHECK_EQ_INT(m_Marisa.edge_count, 4);
    Arena_Free(&f.arena);
}

TEST(marisa_orbs_count_hit_calls_and_keep_real_shot_aging)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    f.session.stage = 3;
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    m_Marisa.phase = 2;
    m_Marisa.pattern = 1;
    m_Marisa.bit_state[0] = 0;
    m_Marisa.bit_hits[0] = 108;
    m_Marisa.bit_position[0][0] = (ENTITY_POINT) { 300, 120 };
    m_Shots.weapon.level = 3;
    m_Player.power = 80;
    SHOT *const shot = &m_Shots.slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->sprite = 50;
    shot->position[0] = (ENTITY_POINT) { 308 * 16, 128 * 16 };
    shot->velocity.y = -128;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Marisa.bit_hits[0], 109);
    CHECK_EQ_INT(m_Score.delta, 5);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(shot->velocity.y, -16);
    m_Marisa.bit_position[0][0] = (ENTITY_POINT) { 300, 120 };
    shot->age = 0;
    const uint8_t cursor = m_Rng.cursor;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Marisa.bit_hits[0], 110);
    CHECK_EQ_INT(m_Marisa.bit_state[0], 1);
    CHECK_EQ_INT(m_Marisa.bit_flash[0], 1);
    CHECK_EQ_INT(m_Score.delta, 5010);
    CHECK_EQ_INT(m_Rng.cursor, (uint8_t)(cursor + 1));
    CHECK_EQ_INT(g_Items->slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.target.x, -1);
    Arena_Free(&f.arena);
}

TEST(marisa_bomb_masks_and_body_multiplier_keep_distinct_damage_gates)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    f.session.stage = 3;
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    m_Marisa.phase = 2;
    m_Marisa.pattern = 1;
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(p_BossState->damage, 0);
    CHECK_EQ_INT(m_Score.delta, 1);
    m_Marisa.damage_enable = 1;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(p_BossState->damage, 1);
    CHECK_EQ_INT(m_Score.delta, 2);
    m_Marisa.bit_state[0] = 0;
    m_Marisa.bit_position[p_Stage->scroll.back][0] =
        (ENTITY_POINT) { 300, 120 };
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(p_BossState->damage, 1);
    CHECK_EQ_INT(m_Marisa.bit_hits[0], 1);
    CHECK_EQ_INT(m_Score.delta, 4);
    p_Stage->world.frame = 2;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Marisa.bit_hits[0], 1);
    CHECK_EQ_INT(m_Score.delta, 5);
    CHECK_EQ_INT(m_Marisa.bomb_mask, 1);
    Arena_Free(&f.arena);
}

TEST(marisa_real_lethal_hit_runs_one_hundred_defeat_updates)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    f.session.stage = 3;
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    m_Marisa.phase = 2;
    m_Marisa.pattern = 1;
    m_Marisa.damage_enable = 1;
    p_BossState->damage = 899;
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(p_BossState->defeated, 1);
    CHECK_EQ_INT(p_BossState->damage, 900);
    CHECK_EQ_INT(m_Score.delta, 20001);
    CHECK_EQ_INT(m_Player.invincibility, 200);
    CHECK_EQ_INT(m_Marisa.defeat_frame, 1);
    for (int frame = 2; frame < 100; frame++) {
        Marisa_Invalidate(p_BossState, f.session.reduce_effects);
        CHECK_EQ_INT(
            Marisa_Update(
                p_BossState, &f.patterns, (uint8_t)f.session.rank,
                f.session.reduce_effects),
            1);
        CHECK_EQ_INT(m_Marisa.defeat_frame, frame);
    }
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        2);
    CHECK_EQ_INT(m_Marisa.defeat_frame, 0);
    CHECK_EQ_INT(p_BossState->defeated, 3);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    CHECK_EQ_INT(m_Score.delta, 20001);
    Arena_Free(&f.arena);
}

TEST(marisa_edge_stars_read_random_position_before_speed)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    f.session.stage = 3;
    f.session.rank = 1;
    BulletSpawn_Init(1);
    m_Performance.value = 48;
    Marisa_Place(p_BossState);
    Marisa_Begin(p_BossState, (uint8_t)f.session.rank);
    m_Marisa.phase = 2;
    m_Marisa.pattern = 4;
    m_Marisa.edge_direction = -1;
    p_BossState->phase_frame = 139;
    p_Stage->world.frame = 0;
    m_Rng.cursor = 0;
    for (int i = 0; i < RANDOM_RING_SIZE; i++) {
        m_Rng.ring[i] = (uint8_t)i;
    }
    CHECK_EQ_INT(
        Marisa_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Rng.cursor, 20);
    const BULLET *const first = &g_Bullets->slots[0];
    const BULLET *const second = &g_Bullets->slots[1];
    CHECK_EQ_INT(first->position[g_Stage->scroll.back].x, 177 * 16);
    CHECK_EQ_INT(first->position[g_Stage->scroll.back].y, 16 * 16);
    CHECK_EQ_INT(first->angle, 48);
    CHECK_EQ_INT(first->speed, 36);
    CHECK_EQ_INT(second->position[g_Stage->scroll.back].x, 32 * 16);
    CHECK_EQ_INT(second->position[g_Stage->scroll.back].y, 83 * 16);
    CHECK_EQ_INT(second->speed, 40);
    Arena_Free(&f.arena);
}

TEST(marisa_borrows_banks_and_imports_logical_palettes_without_showing_them)
{
    FIXTURE f;
    M_Init(&f);
    M_MarisaPatterns(&f);
    MARISA_ASSETS assets = {};
    PatternBank_Init(&assets.summon, &g_HeapAllocator);
    PatternBank_Init(&assets.combat, &g_HeapAllocator);
    uint8_t pixels[32 * 32];
    memset(pixels, 5, sizeof(pixels));
    CHECK(
        Result_Ignore(PatternBank_Put(&assets.summon, 128, 32, 32, pixels, 0)));
    memset(pixels, 6, sizeof(pixels));
    CHECK(
        Result_Ignore(PatternBank_Put(&assets.combat, 128, 32, 32, pixels, 0)));
    assets.has_summon_palette = assets.has_combat_palette = true;
    assets.summon_palette[0] = (GFX_COLOR) { 12, 34, 56, 255 };
    assets.combat_palette[0] = (GFX_COLOR) { 78, 90, 123, 255 };
    const PATTERN startup = f.patterns.entries[0];
    const GFX_COLOR displayed = g_Video.palette[0];
    m_Palette.tone = 77;
    Marisa_SelectPatterns(&f.patterns, &assets, false);
    CHECK(f.patterns.entries[128].data == assets.summon.entries[128].data);
    CHECK(f.patterns.entries[0].data == startup.data);
    CHECK_EQ_INT(m_Palette.colors[0][0], 12);
    CHECK_EQ_INT(m_Palette.tone, 77);
    CHECK(memcmp(&g_Video.palette[0], &displayed, sizeof(displayed)) == 0);
    Marisa_SelectPatterns(&f.patterns, &assets, true);
    CHECK(f.patterns.entries[128].data == assets.combat.entries[128].data);
    CHECK_EQ_INT(m_Palette.colors[0][0], 78);
    CHECK_EQ_INT(m_Palette.colors[0][2], 123);
    CHECK_EQ_INT(m_Palette.tone, 77);
    CHECK(memcmp(&g_Video.palette[0], &displayed, sizeof(displayed)) == 0);
    MarisaAssets_Free(&assets);
    CHECK_EQ_INT(assets.combat.next, 0);
    Arena_Free(&f.arena);
}
