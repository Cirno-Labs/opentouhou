#include "tests/th02/th02_boss_hp.h"
#include "tests/th02/th02_gameplay_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/background/private.h"
#include "th02/boss/mima/logic.c"
#include "th02/boss/mima/transition.c"

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
    const uint8_t *const startup =
        TestGameplay_State()->assets.patterns.entries[0].data;
    const uint8_t *const owner = f.assets.patterns.entries[128].data;
    const GFX_COLOR displayed = g_Video.palette[0];
    m_Palette.tone = 77;
    for (int bank = 0; bank < MIMA_BANK_COUNT; bank++) {
        Mima_SelectPatterns(
            &TestGameplay_State()->assets.patterns, &assets, bank);
        CHECK(
            TestGameplay_State()->assets.patterns.entries[128].data
            == assets.patterns[bank].entries[128].data);
        CHECK(TestGameplay_State()->assets.patterns.entries[0].data == startup);
        CHECK(f.assets.patterns.entries[128].data == owner);
        CHECK_EQ_INT(TestGameplay_State()->assets.patterns.next, 129);
        CHECK_EQ_INT(m_Palette.colors[0][0], 20 + bank);
        CHECK_EQ_INT(m_Palette.tone, 77);
        CHECK(memcmp(&displayed, &g_Video.palette[0], sizeof(displayed)) == 0);
    }
    Mima_Place(TestBossFight_State());
    CHECK_EQ_INT(BossFight_State()->position[0].x, 144);
    CHECK_EQ_INT(BossFight_State()->position[1].y, 64);
    CHECK_EQ_INT(BossFight_State()->sprite, 128);
    TestGameplay_State()->assets.patterns = f.assets.patterns;
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
    Mima_Place(TestBossFight_State());
    TestBossFight_State()->damage = 12;
    TestBossFight_State()->phase_frame = 91;
    TestBossFight_State()->hit_flash = 1;
    TestBossFight_State()->defeated = 2;
    TestBossFight_State()->angle = 77;
    m_Shots.slots[0].state = ENTITY_ACTIVE;
    const PLAYER player = m_Player;
    const RANDOM_STATE before_random = m_Rng;
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    Mima_BeginScene(TestBossFight_State(), (uint8_t)f.session.rank);
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
    CHECK_EQ_INT(BossFight_State()->damage, 0);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(BossFight_State()->hit_flash, 0);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(BossFight_State()->sprite, 128);
    CHECK_EQ_INT(BossFight_State()->angle, 77);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(g_Stage->world.frame, 1234);
    CHECK(memcmp(&m_Player, &player, sizeof(player)) == 0);
    CHECK(memcmp(&m_Rng, &before_random, sizeof(before_random)) == 0);
    m_Mima.phase = MIMA_POST;
    m_Mima.flash_cycle = 7;
    m_Mima.pattern_cycles = 17;
    m_Mima.sphere_active[0] = 1;
    m_Mima.transition_started = true;
    m_Mima.ended = true;
    Clip_Reset();
    M_Start(&f);
    CHECK_EQ_INT(m_Mima.phase, MIMA_APPEAR);
    CHECK_EQ_INT(m_Mima.flash_cycle, 0);
    CHECK_EQ_INT(m_Mima.pattern_cycles, 0);
    CHECK_EQ_INT(m_Mima.sphere_active[0], 0);
    CHECK(!m_Mima.transition_started);
    CHECK(!m_Mima.ended);
    Arena_Free(&f.arena);
}

static void M_MimaPatterns(FIXTURE *const f)
{
    uint8_t pixels[48 * 128];
    memset(pixels, 6, sizeof(pixels));
    for (uint16_t id = 128; id < 141; id++) {
        CHECK(Result_Ignore(PatternBank_Put(
            &f->assets.patterns, id, id < 137 ? 48 : 32, id < 137 ? 128 : 32,
            pixels, 0)));
        TestGameplay_State()->assets.patterns.entries[id] =
            f->assets.patterns.entries[id];
    }
    Mima_Place(TestBossFight_State());
    Mima_BeginScene(TestBossFight_State(), (uint8_t)f->session.rank);
}

TEST(mima_suspension_preserves_stage_progression)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    f.session.stage = 4;
    p_Stage->index = 4;
    m_Flow.phase = STAGE_FLOW_FIGHT;
    m_Mima.phase = MIMA_FORM;
    p_Stage->world.progression = 1;
    const MIMA mima = m_Mima;
    const BOSS_STATE boss = (*TestBossFight_State());
    const RANDOM_STATE random = m_Rng;
    const uint32_t frame = g_Stage->world.frame;
    const int32_t session_frame = f.session.frame;
    for (unsigned attempt = 0; attempt < 2; attempt++) {
        CHECK_EQ_INT(M_FlowUpdate(&f), STAGE_FLOW_WAIT);
        CHECK_EQ_INT(g_Stage->world.progression, 1);
        CHECK_EQ_INT(m_Flow.phase, STAGE_FLOW_FIGHT);
        CHECK(memcmp(&mima, &m_Mima, sizeof(mima)) == 0);
        CHECK(memcmp(&boss, TestBossFight_State(), sizeof(boss)) == 0);
        CHECK(memcmp(&random, &m_Rng, sizeof(random)) == 0);
    }
    m_Mima.phase = MIMA_ATTACK_ONE;
    m_Mima.pattern = 99;
    CHECK_EQ_INT(M_FlowUpdate(&f), STAGE_FLOW_RUNNING);
    CHECK_EQ_INT(g_Stage->world.progression, 1);
    CHECK_EQ_INT(g_Stage->world.frame, frame);
    CHECK_EQ_INT(f.session.frame, session_frame);
    Arena_Free(&f.arena);
}

TEST(mima_real_hits_consume_shots_while_shielded_and_spheres_ignore_damage)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    GAMEPLAY *const g = TestGameplay_State();
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
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(BossFight_State()->damage, 0);
    CHECK_EQ_INT(m_Score.delta, 5);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(shot->velocity.y, -16);
    m_Mima.damage_enable = 1;
    shot->age = 0;
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(BossFight_State()->damage, 5);
    CHECK_EQ_INT(m_Score.delta, 10);
    Shots_FreeAll();
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    m_Mima.sphere_active[0] = 1;
    m_Mima.sphere_position[g_Stage->scroll.back][0] =
        (ENTITY_POINT) { 300, 120 };
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(BossFight_State()->damage, 6);
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
    GAMEPLAY *const g = TestGameplay_State();
    m_Mima.phase = MIMA_ATTACK_FOUR;
    m_Mima.pattern = 99;
    m_Mima.damage_enable = 1;
    TestBossFight_State()->phase_frame = 100;
    TestBossFight_State()->damage = 5999;
    m_Score.value = 123;
    m_Bomb.active = 1;
    p_Stage->world.frame = 0;
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(BossFight_State()->damage, 6000);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(m_Score.value, 500123);
    CHECK_EQ_INT(m_Score.delta, 1);
    CHECK_EQ_INT(m_Player.invincibility, 200);
    CHECK_EQ_INT(m_Mima.phase, MIMA_ATTACK_FOUR);
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
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
    GAMEPLAY *const g = TestGameplay_State();
    m_Mima.phase = MIMA_ATTACK_FOUR;
    m_Mima.form = 1;
    m_Mima.pattern = 7;
    m_Mima.pattern_count = 9;
    m_Mima.max_patterns = 200;
    m_Mima.min_patterns = 3;
    m_Mima.damage_threshold = 1500;
    TestBossFight_State()->phase_frame = 160;
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(m_Mima.pattern, 0);
    CHECK_EQ_INT(m_Mima.pattern_cycles, 1);
    CHECK_EQ_INT(m_Mima.phase, MIMA_ATTACK_FOUR);
    m_Mima.pattern = 7;
    m_Mima.pattern_cycles = 201;
    TestBossFight_State()->phase_frame = 160;
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_WAIT);
    CHECK_EQ_INT(m_Mima.phase, MIMA_FORM);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    const MIMA mima = m_Mima;
    const BOSS_STATE boss = (*TestBossFight_State());
    const RANDOM_STATE before_random = m_Rng;
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_WAIT);
    CHECK(memcmp(&mima, &m_Mima, sizeof(mima)) == 0);
    CHECK(memcmp(&boss, TestBossFight_State(), sizeof(boss)) == 0);
    CHECK(memcmp(&before_random, &m_Rng, sizeof(before_random)) == 0);
    Arena_Free(&f.arena);
}

TEST(mima_expanding_spheres_keep_the_source_birth_center_and_cleanup_guard)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    GAMEPLAY *const g = TestGameplay_State();
    m_Mima.phase = MIMA_ATTACK_THREE;
    m_Mima.pattern = 2;
    m_Mima.expand_center = (ENTITY_POINT) { 224, 144 };
    m_Mima.expand_radius = 4;
    m_Mima.expand_angle = 0;
    TestBossFight_State()->phase_frame = 79;
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
            f.session.reduce_effects),
        BOSS_RUNNING);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].x, 18);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].y, 9);
    CHECK_EQ_INT(
        Mima_Update(
            TestBossFight_State(), &g->assets.patterns, (uint8_t)f.session.rank,
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
    Mima_Invalidate(TestBossFight_State(), f.session.reduce_effects);
    CHECK_EQ_INT(m_Mima.sphere_active[0], 2);
    CHECK_EQ_INT(m_Mima.sphere_active[1], 0);
    CHECK_EQ_INT(m_Mima.sphere_active[2], 0);
    CHECK_EQ_INT(m_Mima.sphere_active[3], 2);
    CHECK_EQ_INT(m_Mima.sphere_position[g_Stage->scroll.back][0].y, -16);
    Arena_Free(&f.arena);
}

static BOSS_RESULT M_MimaController(FIXTURE *const f)
{
    const STAGE_FLOW_RESULT result = M_FlowUpdate(f);
    return result == STAGE_FLOW_WAIT ? BOSS_WAIT
                                     : (BOSS_RESULT)g_Stage->world.progression;
}

TEST(mima_form_dialogue_resumes_once_or_aborts_before_final_score_commit)
{
    for (unsigned continued = 0; continued < 2; continued++) {
        FIXTURE f;
        M_Init(&f);
        GAMEPLAY *const g = TestGameplay_State();
        f.session.stage = 4;
        p_Stage->index = 4;
        m_Flow.phase = STAGE_FLOW_FIGHT;
        f.session.continues_used = continued;
        DIALOGUE_BOX boxes[57] = {};
        DIALOGUE_ASSETS assets[6] = {};
        assets[4] = (DIALOGUE_ASSETS) { .boxes = boxes, .count = 57 };
        assets[4].faces.count = 191;
        assets[4].faces.pixels =
            Allocator_Alloc(Arena_Allocator(&f.arena), 191 * 256);
        memset(assets[4].faces.pixels, 0, 191 * 256);
        memcpy(g->assets.bosses.stages, assets, sizeof(assets));
        memset(&g->assets.bosses.scripts, 255, sizeof(DIALOGUE_SCRIPTS));
        MIMA_ASSETS banks = {};
        banks.patterns[MIMA_BANK_SECOND] = g->assets.patterns;
        banks.patterns[MIMA_BANK_SECOND].next = 131;
        g->assets.bosses.mima = banks;
        Mima_Place(TestBossFight_State());
        Mima_BeginScene(TestBossFight_State(), (uint8_t)f.session.rank);
        m_Mima.phase = MIMA_FORM;
        m_Mima.form = 1;
        m_Mima.damage_enable = 0;
        m_Mima.flash_cycle = 3;
        TestBossFight_State()->hit_flash = 1;
        TestBossFight_State()->phase_frame = 99;
        TestBossFight_State()->damage = 1550;
        m_Talk.runtime.box = 44;
        m_Score.value = 12345;
        m_Score.delta = 48;
        RANDOM_STATE expected_random = m_Rng;
        const int pattern = expected_random.ring[expected_random.cursor++] % 3;
        const uint32_t world = g_Stage->world.frame;
        const int32_t session_frame = f.session.frame;
        CHECK_EQ_INT(M_MimaController(&f), BOSS_WAIT);
        CHECK_EQ_INT(m_Score.value, 12393);
        CHECK_EQ_INT(m_Score.delta, 48);
        CHECK_EQ_INT(m_Mima.flash_cycle, 3);
        CHECK_EQ_INT(BossFight_State()->hit_flash, 1);
        bool ready = false;
        for (unsigned tick = 0; tick < 3000 && !ready && !m_Mima.ended;
             tick++) {
            g->dialogue_input.shot = (tick / 8) % 2 != 0;
            VSyncIrq_VSync();
            const BOSS_RESULT result = M_MimaController(&f);
            CHECK(result == BOSS_WAIT || result == BOSS_RUNNING);
            ready = result == BOSS_RUNNING;
            CHECK_EQ_INT(g_Stage->world.frame, world);
            CHECK_EQ_INT(f.session.frame, session_frame);
        }
        CHECK_EQ_INT(m_Score.delta, 48);
        if (continued != 0) {
            CHECK(!ready);
            CHECK(m_Mima.ended);
            CHECK_EQ_INT(f.session.stage, 127);
            CHECK_EQ_INT(Gameplay_Update(nullptr), GAMEPLAY_ENDING);
            CHECK_EQ_INT(m_Talk.runtime.box, 49);
            CHECK_EQ_INT(m_Score.value, 62393);
            CHECK_EQ_INT(BossFight_State()->phase_frame, 99);
            CHECK_EQ_INT(g->assets.patterns.next, f.assets.patterns.next);
            CHECK(memcmp(&m_Rng, &expected_random, sizeof(RANDOM_STATE)) != 0);
        } else {
            CHECK(ready);
            CHECK(!m_Mima.ended);
            CHECK_EQ_INT(m_Mima.phase, MIMA_ATTACK_FINAL);
            CHECK_EQ_INT(m_Mima.form, 0);
            CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
            CHECK_EQ_INT(BossFight_State()->damage, 0);
            CHECK_EQ_INT(m_Mima.pattern_cycles, 0);
            CHECK_EQ_INT(m_Mima.pattern, pattern);
            CHECK_EQ_INT(m_Mima.damage_threshold, 1100);
            CHECK_EQ_INT(m_Mima.min_patterns, 2);
            CHECK_EQ_INT(m_Mima.max_patterns, 200);
            CHECK_EQ_INT(m_Mima.pattern_count, 3);
            CHECK_EQ_INT(m_Mima.disc_color, 1);
            CHECK_EQ_INT(m_Mima.trail_color, 2);
            CHECK_EQ_INT(m_Mima.background_radius, 200);
            CHECK_EQ_INT(m_Mima.disc_radius, 150);
            CHECK_EQ_INT(m_Mima.disc_base_radius, 150);
            CHECK_EQ_INT(m_Mima.damage_enable, 0);
            CHECK_EQ_INT(m_Talk.runtime.box, 53);
            CHECK_EQ_INT(g->assets.patterns.next, 131);
            CHECK_EQ_INT(m_Score.value, 12393);
            CHECK(memcmp(&m_Rng, &expected_random, sizeof(RANDOM_STATE)) == 0);
            m_Mima.phase = MIMA_POST;
            m_Score.value = 23456;
            m_Score.delta = 73;
            CHECK_EQ_INT(M_MimaController(&f), BOSS_WAIT);
            CHECK_EQ_INT(m_Score.value, 23456);
            for (unsigned tick = 0; tick < 3000 && !m_Mima.ended; tick++) {
                g->dialogue_input.shot = (tick / 8) % 2 != 0;
                VSyncIrq_VSync();
                CHECK_EQ_INT(M_MimaController(&f), BOSS_WAIT);
            }
            CHECK(m_Mima.ended);
            CHECK_EQ_INT(f.session.stage, 127);
            CHECK_EQ_INT(Gameplay_Update(nullptr), GAMEPLAY_ENDING);
            CHECK_EQ_INT(m_Talk.runtime.box, 57);
            CHECK_EQ_INT(m_Score.value, 123456);
            CHECK_EQ_INT(m_Score.delta, 73);
        }
        const SCORE score = *g_Score;
        const MIMA mima = m_Mima;
        CHECK_EQ_INT(
            Mima_Update(
                TestBossFight_State(), &g->assets.patterns,
                (uint8_t)f.session.rank, f.session.reduce_effects),
            BOSS_WAIT);
        CHECK(memcmp(&score, g_Score, sizeof(score)) == 0);
        CHECK(memcmp(&mima, &m_Mima, sizeof(mima)) == 0);
        CHECK_EQ_INT(g_Stage->world.frame, world);
        CHECK_EQ_INT(f.session.frame, session_frame);
        Arena_Free(&f.arena);
    }
}

TEST(mima_flash_consumes_every_fourth_pending_draw_and_keeps_other_page)
{
    FIXTURE f;
    M_Init(&f);
    M_MimaPatterns(&f);
    GAMEPLAY *const g = TestGameplay_State();
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    Video_AccessPage(1);
    const uint8_t back = g_Stage->scroll.back;
    const uint8_t front = g_Stage->scroll.front;
    TestBossFight_State()->position[back] = (ENTITY_POINT) { 145, 16 };
    TestBossFight_State()->hit_flash = 1;
    for (int draw = 1; draw <= 3; draw++) {
        Mima_DrawBody(TestBossFight_State(), &g->assets.patterns);
        CHECK_EQ_INT(m_Mima.flash_cycle, draw);
        CHECK_EQ_INT(BossFight_State()->hit_flash, 1);
        CHECK_EQ_INT(g_Video.pages[back][16 * 640 + 145], 6);
    }
    memset(g_Video.pages[back], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[front], 11, VIDEO_PAGE_SIZE);
    Mima_DrawBody(TestBossFight_State(), &g->assets.patterns);
    CHECK_EQ_INT(m_Mima.flash_cycle, 4);
    CHECK_EQ_INT(BossFight_State()->hit_flash, 0);
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
    Mima_DrawBody(TestBossFight_State(), &g->assets.patterns);
    CHECK_EQ_INT(m_Mima.flash_cycle, 4);
    CHECK_EQ_INT(g_Video.pages[back][16 * 640 + 145], 6);
    m_Mima.flash_cycle = 255;
    TestBossFight_State()->hit_flash = 1;
    TestBossFight_State()->position[back].y = 383;
    Mima_DrawBody(TestBossFight_State(), &g->assets.patterns);
    CHECK_EQ_INT(m_Mima.flash_cycle, 0);
    CHECK_EQ_INT(BossFight_State()->hit_flash, 0);
    CHECK_EQ_INT(g_Video.pages[back][383 * 640 + 145], 4);
    CHECK_EQ_INT(g_Video.pages[back][384 * 640 + 145], 9);
    Arena_Free(&f.arena);
}

TEST(mima_transition_requests_each_script_once_and_stays_inert_after_ending)
{
    for (unsigned form = 0; form < 2; form++) {
        FIXTURE f;
        M_Init(&f);
        BOSS_STATE *const boss = TestBossFight_State();
        const PATTERN_BANK *const patterns =
            &TestGameplay_State()->assets.patterns;
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
    BOSS_STATE *const boss = TestBossFight_State();
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
        CHECK_EQ_INT(
            Mima_Update(boss, &TestGameplay_State()->assets.patterns, 1, false),
            BOSS_RUNNING);
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
