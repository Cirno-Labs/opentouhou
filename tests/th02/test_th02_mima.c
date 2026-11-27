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
#include "th02/boss/mima/logic.c"
#include "th02/boss/mima/transition.c"
#include "th02/grcg.h"
#include "th02/items.h"
#include "th02/pillars.h"
#include "th02/sparks.h"
#include "th02/stage_effects.h"
#include "th02/video.h"

#include "core/base/arena.h"
#include "test.h"
#include "th02_boss_hp.h"

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
    Arena_Init(&f->arena, "mima test", 2 * 1024 * 1024);
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
    Pillars_ResetStage(4);
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
    Mima_Init();
}

// Overrides slots 128..140 with Mima's own body and bit sizes, matching her
// bank layout rather than the fixture's generic boss-sized filler.
static void M_MimaPatterns(FIXTURE *const f)
{
    uint8_t pixels[48 * 128];
    memset(pixels, 6, sizeof(pixels));
    for (uint16_t id = 128; id < 141; id++) {
        CHECK(Result_Ignore(PatternBank_Put(
            &f->patterns, id, id < 137 ? 48 : 32, id < 137 ? 128 : 32, pixels,
            0)));
    }
    Mima_Place(p_BossState);
    Mima_BeginScene(p_BossState, (uint8_t)f->session.rank);
}

TEST(mima_banks_borrow_cached_patterns_and_import_palettes_without_dac_writes)
{
    FIXTURE f;
    M_Init(&f);
    MIMA_ASSETS assets = {};
    uint8_t pixels[48 * 128];
    for (int bank = 0; bank < MIMA_BANK_COUNT; bank++) {
        PatternBank_Init(&assets.patterns[bank], &g_HeapAllocator);
        memset(pixels, 4 + bank, sizeof(pixels));
        CHECK(Result_Ignore(
            PatternBank_Put(&assets.patterns[bank], 128, 48, 128, pixels, 0)));
        assets.has_palette[bank] = true;
        assets.palettes[bank][0] = (GFX_COLOR) { 20 + bank, 40, 60, 255 };
    }
    const uint8_t *const startup = f.patterns.entries[0].data;
    const GFX_COLOR displayed = g_Video.palette[0];
    m_Palette.tone = 77;
    for (int bank = 0; bank < MIMA_BANK_COUNT; bank++) {
        Mima_SelectPatterns(&f.patterns, &assets, bank);
        CHECK(
            f.patterns.entries[128].data
            == assets.patterns[bank].entries[128].data);
        CHECK(f.patterns.entries[0].data == startup);
        CHECK_EQ_INT(f.patterns.next, 129);
        CHECK_EQ_INT(m_Palette.colors[0][0], 20 + bank);
        CHECK_EQ_INT(m_Palette.tone, 77);
        CHECK(memcmp(&displayed, &g_Video.palette[0], sizeof(displayed)) == 0);
    }
    Mima_Place(p_BossState);
    CHECK_EQ_INT(p_BossState->position[0].x, 144);
    CHECK_EQ_INT(p_BossState->position[1].y, 64);
    CHECK_EQ_INT(p_BossState->sprite, 128);
    MimaAssets_Free(&assets);
    CHECK_EQ_INT(assets.patterns[0].next, 0);
    Arena_Free(&f.arena);
}

TEST(
    mima_combat_scene_clears_both_pages_and_keeps_right_border_and_hidden_tails)
{
    FIXTURE f;
    M_Init(&f);
    p_Stage->scroll.front = 0;
    p_Stage->scroll.back = 1;
    p_Stage->world.frame = 1234;
    Mima_Place(p_BossState);
    p_BossState->damage = 12;
    p_BossState->phase_frame = 91;
    p_BossState->hit_flash = 1;
    p_BossState->defeated = 2;
    p_BossState->angle = 77;
    m_Shots.slots[0].state = ENTITY_ACTIVE;
    const PLAYER player = m_Player;
    const RANDOM_STATE before_random = m_Rng;
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    Mima_BeginScene(p_BossState, (uint8_t)f.session.rank);
    for (int page = 0; page < 2; page++) {
        for (int pixel = 0; pixel < VIDEO_VISIBLE_SIZE; pixel++) {
            CHECK_EQ_INT(
                g_Video.pages[page][pixel], pixel % 640 >= 416 ? 11 : 0);
        }
        CHECK_EQ_INT(g_Video.pages[page][VIDEO_PAGE_SIZE - 1], 5 + page);
    }
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Clip->left, 32);
    CHECK_EQ_INT(g_Clip->right, 416);
    CHECK_EQ_INT(g_Clip->bottom, 399);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(p_Background->cache.mode, TILE_DISABLED);
    CHECK_EQ_INT(p_BossState->damage, 0);
    CHECK_EQ_INT(p_BossState->phase_frame, 0);
    CHECK_EQ_INT(p_BossState->hit_flash, 0);
    CHECK_EQ_INT(p_BossState->defeated, 0);
    CHECK_EQ_INT(p_BossState->sprite, 128);
    CHECK_EQ_INT(p_BossState->angle, 77);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(g_Stage->world.frame, 1234);
    CHECK(memcmp(&m_Player, &player, sizeof(player)) == 0);
    CHECK(memcmp(&m_Rng, &before_random, sizeof(before_random)) == 0);
    Arena_Free(&f.arena);
}

TEST(mima_real_hits_consume_shots_while_shielded_and_spheres_ignore_damage)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    m_Mima.phase = MIMA_ATTACK_ONE;
    m_Mima.pattern = 99;
    m_Shots.weapon.level = 3;
    SHOT *const shot = &m_Shots.slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->sprite = 50;
    shot->position[p_Stage->scroll.back] =
        (ENTITY_POINT) { 200 * 16, 104 * 16 };
    shot->velocity.y = -128;
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(p_BossState->damage, 0);
    CHECK_EQ_INT(m_Score.delta, 5);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(shot->velocity.y, -16);
    m_Mima.damage_enable = 1;
    shot->age = 0;
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(p_BossState->damage, 5);
    CHECK_EQ_INT(m_Score.delta, 10);
    Shots_FreeAll();
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    m_Mima.sphere_active[0] = 1;
    m_Mima.sphere_position[g_Stage->scroll.back][0] =
        (ENTITY_POINT) { 300, 120 };
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(p_BossState->damage, 6);
    CHECK_EQ_INT(m_Score.delta, 12);
    CHECK_EQ_INT(m_Mima.sphere_active[0], 1);
    CHECK_EQ_INT(m_Mima.inactive_spheres, 7);
    Arena_Free(&f.arena);
}

TEST(mima_six_thousand_damage_awards_direct_score_without_ending_the_phase)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    m_Mima.phase = MIMA_ATTACK_FOUR;
    m_Mima.pattern = 99;
    m_Mima.damage_enable = 1;
    p_BossState->phase_frame = 100;
    p_BossState->damage = 5999;
    m_Score.value = 123;
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(p_BossState->damage, 6000);
    CHECK_EQ_INT(p_BossState->defeated, 1);
    CHECK_EQ_INT(m_Score.value, 500123);
    CHECK_EQ_INT(m_Score.delta, 1);
    CHECK_EQ_INT(m_Player.invincibility, 200);
    CHECK_EQ_INT(m_Mima.phase, MIMA_ATTACK_FOUR);
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(m_Score.value, 500123);
    CHECK_EQ_INT(m_Score.delta, 1);
    Arena_Free(&f.arena);
}

TEST(mima_first_form_wraps_pattern_seven_and_suspends_at_its_real_boundary)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    m_Mima.phase = MIMA_ATTACK_FOUR;
    m_Mima.form = 1;
    m_Mima.pattern = 7;
    m_Mima.pattern_count = 9;
    m_Mima.max_patterns = 200;
    m_Mima.min_patterns = 3;
    m_Mima.damage_threshold = 1500;
    p_BossState->phase_frame = 160;
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(m_Mima.pattern, 0);
    CHECK_EQ_INT(m_Mima.pattern_cycles, 1);
    CHECK_EQ_INT(m_Mima.phase, MIMA_ATTACK_FOUR);
    m_Mima.pattern = 7;
    m_Mima.pattern_cycles = 201;
    p_BossState->phase_frame = 160;
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_WAIT);
    CHECK_EQ_INT(m_Mima.phase, MIMA_FORM);
    CHECK_EQ_INT(p_BossState->phase_frame, 0);
    const MIMA mima = m_Mima;
    const BOSS_STATE boss = *p_BossState;
    const RANDOM_STATE before_random = m_Rng;
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_WAIT);
    CHECK(memcmp(&mima, &m_Mima, sizeof(mima)) == 0);
    CHECK(memcmp(&boss, p_BossState, sizeof(boss)) == 0);
    CHECK(memcmp(&before_random, &m_Rng, sizeof(before_random)) == 0);
    Arena_Free(&f.arena);
}

TEST(mima_expanding_spheres_keep_the_source_birth_center_and_cleanup_guard)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    m_Mima.phase = MIMA_ATTACK_THREE;
    m_Mima.pattern = 2;
    m_Mima.expand_center = (ENTITY_POINT) { 224, 144 };
    m_Mima.expand_radius = 4;
    m_Mima.expand_angle = 0;
    p_BossState->phase_frame = 79;
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].x, 18);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].y, 9);
    CHECK_EQ_INT(
        Mima_Update(
            p_BossState, &f.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(m_Mima.expand_radius, 6);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].x, 229);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].y, 143);
    const int16_t tops[] = { -16, -15, 381, 382 };
    for (int i = 0; i < 4; i++) {
        m_Mima.sphere_active[i] = 2;
        m_Mima.sphere_position[g_Stage->scroll.back][i] =
            (ENTITY_POINT) { 128, tops[i] };
    }
    Mima_Invalidate(p_BossState, f.session.reduce_effects);
    CHECK_EQ_INT(m_Mima.sphere_active[0], 2);
    CHECK_EQ_INT(m_Mima.sphere_active[1], 0);
    CHECK_EQ_INT(m_Mima.sphere_active[2], 0);
    CHECK_EQ_INT(m_Mima.sphere_active[3], 2);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].y, -16);
    Arena_Free(&f.arena);
}

TEST(mima_flash_consumes_every_fourth_pending_draw_and_keeps_other_page)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    Video_AccessPage(1);
    const uint8_t back = g_Stage->scroll.back;
    const uint8_t front = g_Stage->scroll.front;
    p_BossState->position[back] = (ENTITY_POINT) { 145, 16 };
    p_BossState->hit_flash = 1;
    for (int draw = 1; draw <= 3; draw++) {
        Mima_DrawBody(p_BossState, &f.patterns);
        CHECK_EQ_INT(m_Mima.flash_cycle, draw);
        CHECK_EQ_INT(p_BossState->hit_flash, 1);
        CHECK_EQ_INT(g_Video.pages[back][16 * 640 + 145], 6);
    }
    memset(g_Video.pages[back], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[front], 11, VIDEO_PAGE_SIZE);
    Mima_DrawBody(p_BossState, &f.patterns);
    CHECK_EQ_INT(m_Mima.flash_cycle, 4);
    CHECK_EQ_INT(p_BossState->hit_flash, 0);
    for (int y = 0; y < 400; y++) {
        for (int x = 0; x < 640; x++) {
            CHECK_EQ_INT(
                g_Video.pages[back][y * 640 + x],
                y >= 16 && y < 144 && x >= 145 && x <= 288 ? 4 : 9);
            CHECK_EQ_INT(g_Video.pages[front][y * 640 + x], 11);
        }
    }
    CHECK_EQ_INT(g_Video.drawing_page, back);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[2], 255);
    Mima_DrawBody(p_BossState, &f.patterns);
    CHECK_EQ_INT(m_Mima.flash_cycle, 4);
    CHECK_EQ_INT(g_Video.pages[back][16 * 640 + 145], 6);
    m_Mima.flash_cycle = 255;
    p_BossState->hit_flash = 1;
    p_BossState->position[back].y = 383;
    Mima_DrawBody(p_BossState, &f.patterns);
    CHECK_EQ_INT(m_Mima.flash_cycle, 0);
    CHECK_EQ_INT(p_BossState->hit_flash, 0);
    CHECK_EQ_INT(g_Video.pages[back][383 * 640 + 145], 4);
    CHECK_EQ_INT(g_Video.pages[back][384 * 640 + 145], 9);
    Arena_Free(&f.arena);
}

TEST(mima_transition_requests_each_script_once_and_stays_inert_after_ending)
{
    for (unsigned form = 0; form < 2; form++) {
        FIXTURE f;
        M_Init(&f);
        BOSS_STATE *const boss = p_BossState;
        const PATTERN_BANK *const patterns = &f.patterns;
        m_Mima.phase = form != 0 ? MIMA_FORM : MIMA_POST;
        CHECK_EQ_INT(
            Mima_BeginTransition(boss, patterns),
            form != 0 ? MIMA_TRANSITION_FORM : MIMA_TRANSITION_POST);
        const MIMA waiting = m_Mima;
        CHECK_EQ_INT(
            Mima_BeginTransition(boss, patterns), MIMA_TRANSITION_WAIT);
        CHECK(memcmp(&m_Mima, &waiting, sizeof(waiting)) == 0);
        CHECK_EQ_INT(Mima_EndTransition(boss, true), BOSS_FINISHED);
        const SCORE score = *g_Score;
        const MIMA ended = m_Mima;
        CHECK_EQ_INT(
            Mima_BeginTransition(boss, patterns), MIMA_TRANSITION_ENDED);
        CHECK_EQ_INT(Mima_EndTransition(boss, true), BOSS_FINISHED);
        CHECK(memcmp(&m_Mima, &ended, sizeof(ended)) == 0);
        CHECK(memcmp(g_Score, &score, sizeof(score)) == 0);
        Arena_Free(&f.arena);
    }
}

TEST(mima_hp_hook_waits_for_each_phase_setup_including_final_form)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    TEST_BOSS_HP hp;
    TestBossHp_Begin(&hp);
    BOSS_STATE *const boss = p_BossState;
    Mima_BeginScene(boss, 1);
    CHECK_EQ_INT(hp.count, 0);
    static const int16_t phases[] = {
        MIMA_APPEAR,
        MIMA_CHANGE_TWO,
        MIMA_CHANGE_THREE,
        MIMA_CHANGE_FOUR,
    };
    static const int16_t thresholds[] = { 600, 700, 800, 1500, 1100 };
    for (size_t i = 0; i < ARRAY_SIZE(phases); i++) {
        m_Mima.phase = phases[i];
        boss->phase_frame = 100;
        CHECK_EQ_INT(Mima_Update(boss, &f.patterns, 1, false), BOSS_RUNNING);
        CHECK_EQ_INT(hp.count, i + 1);
        CHECK_EQ_INT(boss->damage, thresholds[i] - 1);
    }
    m_Mima.phase = MIMA_FORM;
    CHECK_EQ_INT(Mima_EndTransition(boss, false), BOSS_RUNNING);
    CHECK_EQ_INT(boss->damage, 1099);
    CHECK_EQ_INT(hp.count, ARRAY_SIZE(thresholds));
    CHECK(memcmp(hp.thresholds, thresholds, sizeof(thresholds)) == 0);
    TestBossHp_End();
    Arena_Free(&f.arena);
}
