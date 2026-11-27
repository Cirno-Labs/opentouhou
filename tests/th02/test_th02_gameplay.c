#include "tests/th02/th02_gameplay_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/background/private.h"
#include "th02/bullets/private.h"
#include "th02/enemies.c"
#include "th02/items.c"
#include "th02/performance.c"
#include "th02/sparks.c"

TEST(pause_entry_reads_input_before_its_first_frame_delay)
{
    FIXTURE f;
    M_Init(&f);
    g_VSync.count1 = 100;
    TH02_INPUT input = {};
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    M_FinishReveal(&f, &input);
    VSyncIrq_VSync();
    input.cancel = true;
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(g_Stage->world.keys, GAME_KEY_CANCEL);
    CHECK_EQ_INT(TestGameplay_State()->loop.wait, STAGE_PAUSE_FRAME);
    CHECK_EQ_INT(g_VSync.count1, 0);

    const uint32_t frame = g_Stage->world.frame;
    input.cancel = false;
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(TestGameplay_State()->loop.wait, STAGE_PAUSE_FRAME);
    CHECK_EQ_INT(g_Stage->world.keys, GAME_KEY_CANCEL);
    CHECK_EQ_INT(g_Stage->world.frame, frame);
    Arena_Free(&f.arena);
}

TEST(main_entry_restores_continue_score_and_recovery_items)
{
    static const uint16_t continues[] = { 1, 3, 9, 12 };
    static const uint8_t stages[] = { 0, 4, 5, 3 };
    static const int32_t scores[] = { 1, 3, 9, 9 };
    for (size_t i = 0; i < ARRAY_SIZE(continues); i++) {
        FIXTURE f;
        M_Init(&f);
        f.session.continues_used = continues[i];
        f.session.stage = stages[i];
        f.stage.stage = stages[i];
        f.session.score = 12345;
        M_Start(&f);
        CHECK_EQ_INT(m_Score.value, scores[i]);
        CHECK_EQ_INT(m_Items.big_power, (stages[i] % 5) + 2);
        CHECK_EQ_INT(f.session.score, 12345);
        CHECK_EQ_INT(f.session.continues_used, continues[i]);
        Arena_Free(&f.arena);
    }
}

typedef struct {
    FIXTURE *fixture;
    WAIT wait;
    uint32_t world_frame;
    int32_t session_frame;
    unsigned begins;
    unsigned ends;
    unsigned updates;
    bool entering;
    bool leaving;
} BOSS_FIXTURE;

static bool M_BossWait(
    void *const context, const TEST_BOSS_CALL call, int16_t *const value)
{
    BOSS_FIXTURE *const boss = context;
    FIXTURE *const f = boss->fixture;
    if (call == TEST_BOSS_UPDATE) {
        boss->updates++;
        *value = 2;
        return true;
    }
    if (call == TEST_BOSS_DRAW) {
        return true;
    }
    bool *const started =
        call == TEST_BOSS_BEGIN ? &boss->entering : &boss->leaving;
    if (call == TEST_BOSS_BEGIN) {
        boss->begins++;
    } else {
        boss->ends++;
    }
    if (!*started) {
        *started = true;
        CHECK_EQ_INT(g_VSync.count1, 0);
        boss->world_frame = g_Stage->world.frame;
        boss->session_frame = f->session.frame;
        Wait_Delay(&boss->wait, 2);
        return false;
    }
    CHECK_EQ_INT(g_Stage->world.frame, boss->world_frame);
    CHECK_EQ_INT(f->session.frame, boss->session_frame);
    return Wait_Ready(&boss->wait, nullptr, 0);
}

TEST(map_end_boss_waits_share_vsync_and_disable_ordinary_enemy_calls)
{
    FIXTURE f;
    M_Init(&f);
    BOSS_FIXTURE state = { .fixture = &f };
    const TEST_BOSS boss = { M_BossWait, &state };
    TestStageFlow_Boss(&boss);
    g_VSync.count1 = 100;
    const TH02_INPUT input = {};
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    M_FinishReveal(&f, &input);
    p_Stage->scroll.done = 1;
    bool complete = false;
    bool checked_gate = false;
    for (unsigned frame = 0; frame < 20 && !complete; frame++) {
        VSyncIrq_VSync();
        complete = Gameplay_Update(&input) != GAMEPLAY_RUNNING;
        if (m_Flow.phase == STAGE_FLOW_FIGHT && !checked_gate) {
            checked_gate = true;
            m_Enemies.slots[0].state = ENTITY_REMOVE;
            m_Shots.target = (ENTITY_POINT) { 123, 45 };
        }
    }
    CHECK(complete);
    CHECK(checked_gate);
    CHECK_EQ_INT(m_Enemies.slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Shots.target.x, 123);
    CHECK_EQ_INT(m_Shots.target.y, 45);
    CHECK_EQ_INT(TestGameplay_State()->loop.wait, STAGE_NEXT);
    CHECK_EQ_INT(m_Flow.phase, STAGE_FLOW_DONE);
    CHECK_EQ_INT(g_Stage->world.frame, 2);
    CHECK_EQ_INT(f.session.frame, 715);
    CHECK_EQ_INT(state.begins, 3);
    CHECK_EQ_INT(state.ends, 3);
    CHECK_EQ_INT(state.updates, 1);
    CHECK_EQ_INT(m_Score.value, 0);
    Arena_Free(&f.arena);
}

TEST(stage_entry_keeps_cached_images_alive_through_wait_and_page_swaps)
{
    FIXTURE fixture;
    M_Init(&fixture);
    GAMEPLAY *const game = TestGameplay_State();
    const uint8_t pattern_byte = fixture.assets.patterns.entries[0].data[0];
    CHECK_EQ_INT(game->phase, GAMEPLAY_PREPARE);
    CHECK_EQ_INT(g_Video.pages[0][144 * VIDEO_WIDTH + 96], 3);
    CHECK_EQ_INT(m_Score.lives, 2);
    CHECK_EQ_INT(m_Score.bombs, 3);
    CHECK_EQ_INT(m_Score.highest, 12345);
    CHECK_EQ_INT(m_Score.highest_continues, 6);
    CHECK_EQ_INT(fixture.session.frame, 713);
    TH02_INPUT input = { .right = true, .shot = true };
    for (size_t frame = 0; frame < 99; frame++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
        CHECK_EQ_INT(game->phase, GAMEPLAY_PREPARE);
    }
    VSyncIrq_VSync();
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    M_FinishReveal(&fixture, &input);
    CHECK_EQ_INT(game->phase, GAMEPLAY_STAGE);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    CHECK_EQ_INT(m_Player.current.x, 213);
    CHECK_EQ_INT(g_Video.pages[1][32], 12);
    CHECK_EQ_INT(g_Video.pages[0][416], 11);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_ACTIVE);
    uint8_t *const overlay = Mem_Alloc(VIDEO_VISIBLE_SIZE);
    const TRAM_FONT font = {};
    Tram_Scanout(&game->assets.gaiji, &font, 0, true, overlay, VIDEO_WIDTH);
    CHECK_EQ_INT(overlay[16 * VIDEO_WIDTH + 32], 0);
    CHECK_EQ_INT(overlay[16 * VIDEO_WIDTH + 600], 1);
    Mem_Free(overlay);
    for (size_t frame = 0; frame < 32; frame++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    }
    CHECK_EQ_INT(g_Stage->world.frame, 32);
    CHECK_EQ_INT(fixture.session.frame, 745);
    CHECK_EQ_INT(m_Player.current.x, 357);
    CHECK_EQ_INT(game->assets.patterns.entries[0].data[0], pattern_byte);
    CHECK_EQ_INT(game->assets.backdrop.image.pixels[0], 3);
    CHECK_EQ_INT(fixture.pixels[256], 12);
    p_Stage->world.quit = 1;
    VSyncIrq_VSync();
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_GAME_OVER);
    CHECK_EQ_INT(game->loop.wait, STAGE_QUIT);
    const uint32_t frame = g_Stage->world.frame;
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_GAME_OVER);
    CHECK_EQ_INT(g_Stage->world.frame, frame);
    Arena_Free(&fixture.arena);
}

TEST(active_bomb_damages_stage_enemies_every_other_frame)
{
    FIXTURE fixture;
    M_Init(&fixture);
    GAMEPLAY *const game = TestGameplay_State();
    STAGE_FORMATION formations[] = {
        { .pattern = 10, .hp = 2, .score = 100, .item = 3 },
        { .pattern = 10, .hp = -1 },
    };
    STAGE_SCRIPT script = {};
    script.bytes[1] = 255;
    fixture.stage.data.formations = formations;
    fixture.stage.data.formation_count =
        sizeof(formations) / sizeof(*formations);
    fixture.stage.data.scripts = &script;
    fixture.stage.data.script_count = 1;
    for (size_t i = 0; i < 3; i++) {
        ENEMY *const enemy = &m_Enemies.slots[i];
        enemy->state = ENTITY_ACTIVE;
        enemy->position[0] = enemy->position[1] = (ENTITY_POINT) { 100, 100 };
        enemy->formation = i == 1 ? 1 : 0;
        enemy->hit_disabled = i == 2;
    }
    m_Enemies.count = 3;
    Enemies_Update(
        StageScroll_Pages(), g_Stage->scroll.moved, &fixture.stage.data,
        &game->assets.patterns, g_Stage->world.frame,
        (uint8_t)fixture.session.rank, 1);
    CHECK_EQ_INT(m_Enemies.slots[0].damage, 0);
    Bomb_Activate(StageScroll_Pages());
    CHECK_EQ_INT(m_Bomb.active, 1);
    CHECK_EQ_INT(m_Score.bombs, 2);
    for (uint16_t frame = 0; frame < 3; frame++) {
        p_Stage->world.frame = frame;
        Enemies_Update(
            StageScroll_Pages(), g_Stage->scroll.moved, &fixture.stage.data,
            &game->assets.patterns, g_Stage->world.frame,
            (uint8_t)fixture.session.rank, 1);
        CHECK_EQ_INT(m_Enemies.slots[0].damage, frame / 2 + 1);
        CHECK_EQ_INT(m_Enemies.slots[0].dying, frame == 2);
        CHECK_EQ_INT(m_Enemies.slots[1].damage, 0);
        CHECK_EQ_INT(m_Enemies.slots[2].damage, 0);
    }
    CHECK_EQ_INT(m_Score.delta, 102);
    CHECK_EQ_INT(m_Items.slots[0].state, ENTITY_ACTIVE);
    Arena_Free(&fixture.arena);
}

TEST(reentry_rebinds_extra_and_selected_bomb_without_reloading)
{
    FIXTURE fixture;
    M_Init(&fixture);
    for (uint8_t shot = 0; shot < 3; shot++) {
        fixture.session.stage = fixture.stage.stage = 5;
        fixture.session.rank = 4;
        fixture.session.shot_type = fixture.bomb.shot_type = shot;
        fixture.fix_bomb_transition_overread = shot != 1;
        memset(fixture.bomb_tail, shot + 1, sizeof(fixture.bomb_tail));
        M_Start(&fixture);
        if (shot == 1) {
            CHECK(
                memcmp(TestGameplay_State()->bomb_tail, fixture.bomb_tail, 72)
                == 0);
            memset(fixture.bomb_tail, 0, sizeof(fixture.bomb_tail));
            CHECK_EQ_INT(TestGameplay_State()->bomb_tail[0], 2);
            CHECK_EQ_INT(TestGameplay_State()->bomb_tail[71], 2);
        }
        CHECK_EQ_INT(g_Stage->scroll.interval, 2);
        CHECK_EQ_INT(g_Stage->world.midboss_row, 200);
        CHECK_EQ_INT(m_Player.option_sprite, 72 + shot);
        CHECK_EQ_INT(
            TestGameplay_State()->fix_bomb_transition_overread, shot != 1);
        CHECK(TestGameplay_State()->bomb == &fixture.bomb);
        CHECK_EQ_INT(g_VSync.count1, 0);
    }
    Arena_Free(&fixture.arena);
}

TEST(next_stage_keeps_run_state_and_resets_only_stage_state)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const game = TestGameplay_State();
    const TH02_INPUT input = {};
    m_Flow.phase = STAGE_FLOW_DONE;
    bool done = false;
    for (size_t frame = 0; frame < 160 && !done; frame++) {
        VSyncIrq_VSync();
        done = Gameplay_Update(&input) != GAMEPLAY_RUNNING;
    }
    CHECK(done);
    CHECK_EQ_INT(game->loop.wait, STAGE_NEXT);
    CHECK_EQ_INT(Gameplay_Update(nullptr), GAMEPLAY_CLEAR);
    m_Score.value = 1234;
    m_Score.delta = 7;
    m_Score.highest = 9000;
    m_Score.lives = 4;
    m_Score.bombs = 2;
    m_Score.extends = 5;
    Score_Commit();
    m_Player.power = 37;
    m_Shots.weapon.cycle[0] = 17;
    m_Shots.weapon.cooldown[1] = 9;
    m_Shots.weapon.parameter[0] = 52;
    m_Player.hit = 1;
    m_Player.invincibility = 3;
    m_Shots.fire_cycle = 254;
    m_Shots.target = (ENTITY_POINT) { 20, 30 };
    m_Rng.cursor = 77;
    memset(&m_Performance, 0, sizeof(m_Performance));
    m_Performance.value = 5;
    m_Performance.item_skill = 21;
    m_Performance.maximum = 16;
    m_Performance.total_misses = 3;
    m_Performance.total_bombs = 4;
    m_Performance.collected = 13;
    m_Performance.dropped = 9;
    const PERFORMANCE performance = m_Performance;
    p_Bullets->slots[0].state = ENTITY_ACTIVE;
    p_Bullets->slots[0].position[0].x = 17;
    p_Stage->scroll.done = 1;
    p_Stage->scroll.rows = 115;
    p_Stage->world.frame = 900;
    (*p_BossState) = (BOSS_STATE) {
        .position = { { 123, 45 }, { 124, 46 } },
        .phase_frame = -32768,
        .damage = 32767,
        .sprite = 150,
        .defeated = 255,
        .hit_flash = 254,
        .angle = 0xe0,
    };
    m_Bomb.active = m_Bomb.stage_bombs = 1;
    f.session.stage = 1;
    STAGE_ASSETS stage = f.stage;
    stage.stage = 1;
    PATTERN_BANK patterns = f.assets.patterns;
    const uint16_t count2 = g_VSync.count2;
    const uint64_t edges = g_VSync.edges;
    const int32_t session_frame = f.session.frame;
    Gameplay_BeginStage(&stage, &patterns, &f.bomb);
    CHECK_EQ_INT(game->phase, GAMEPLAY_PREPARE);
    CHECK(game->stage == &stage);
    CHECK(game->assets.patterns.entries[0].data == patterns.entries[0].data);
    CHECK_EQ_INT(m_Score.value, 1241);
    CHECK_EQ_INT(m_Score.highest, 9000);
    CHECK_EQ_INT(m_Score.lives, 4);
    CHECK_EQ_INT(m_Score.bombs, 2);
    CHECK_EQ_INT(m_Score.extends, 0);
    CHECK_EQ_INT(m_Player.power, 37);
    CHECK_EQ_INT(m_Shots.weapon.level, 5);
    CHECK_EQ_INT(m_Shots.weapon.cycle[0], 17);
    CHECK_EQ_INT(m_Shots.weapon.cooldown[1], 9);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 52);
    CHECK_EQ_INT(m_Player.hit, 0);
    CHECK_EQ_INT(m_Player.invincibility, 0);
    CHECK_EQ_INT(m_Player.position[0].x, 208);
    CHECK_EQ_INT(m_Player.position[1].y, 336);
    CHECK_EQ_INT(m_Shots.fire_cycle, 254);
    CHECK_EQ_INT(m_Shots.target.x, -1);
    CHECK_EQ_INT(m_Rng.cursor, 78);
    CHECK(memcmp(&m_Performance, &performance, sizeof(performance)) == 0);
    CHECK_EQ_INT(p_Bullets->slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(p_Bullets->slots[0].position[0].x, 17);
    CHECK_EQ_INT(m_Bomb.active, 0);
    CHECK_EQ_INT(m_Bomb.stage_bombs, 0);
    CHECK_EQ_INT(g_Stage->scroll.done, 0);
    CHECK_EQ_INT(g_Stage->scroll.rows, 0);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    CHECK_EQ_INT(g_Stage->world.midboss_row, 80);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(BossFight_State()->damage, 0);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(BossFight_State()->hit_flash, 0);
    CHECK_EQ_INT(BossFight_State()->position[0].x, 123);
    CHECK_EQ_INT(BossFight_State()->position[1].y, 46);
    CHECK_EQ_INT(BossFight_State()->sprite, 150);
    CHECK_EQ_INT(BossFight_State()->angle, 0xe0);
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, count2);
    CHECK_EQ_INT(g_VSync.edges, edges);
    CHECK_EQ_INT(f.session.frame, session_frame);
    Arena_Free(&f.arena);
}

TEST(continue_resumes_same_stage_without_resetting_world_or_rng)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const game = TestGameplay_State();
    const TH02_INPUT input = {};
    for (size_t frame = 0; frame < 152; frame++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    }
    p_Stage->world.quit = 1;
    bool done = false;
    for (size_t frame = 0; frame < 4 && !done; frame++) {
        VSyncIrq_VSync();
        done = Gameplay_Update(&input) != GAMEPLAY_RUNNING;
    }
    CHECK(done);
    CHECK_EQ_INT(game->loop.wait, STAGE_QUIT);
    f.session.score = 123450;
    m_Score.value = 12345;
    m_Score.extends = 2;
    m_Items.overflow = 41;
    m_Player.power = 64;
    m_Player.hit = 1;
    m_Shots.weapon.cycle[0] = 17;
    m_Shots.fire_cycle = 123;
    p_Stage->scroll.page_lines[0] = 398;
    p_Stage->scroll.page_lines[1] = 399;
    const RANDOM_STATE before_random = m_Rng;
    const PERFORMANCE performance = m_Performance;
    const uint16_t count2 = g_VSync.count2;
    const uint64_t edges = g_VSync.edges;
    const uint16_t world_frame = g_Stage->world.frame;
    const int32_t session_frame = f.session.frame;
    const uint16_t rows = g_Stage->scroll.rows;
    Gameplay_EndCredit(true);
    CHECK_EQ_INT(game->phase, GAMEPLAY_CONTINUE_COVER);
    CHECK_EQ_INT(m_Player.hit, 1);
    for (unsigned i = 0; i < 42; i++) {
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
        CHECK_EQ_INT(game->phase, GAMEPLAY_CONTINUE_COVER);
        VSyncIrq_VSync();
    }
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(game->phase, GAMEPLAY_CONTINUE_REVEAL);
    CHECK_EQ_INT(f.session.continues_used, 1);
    CHECK_EQ_INT(f.session.highest_score, 123450);
    CHECK_EQ_INT(m_Score.value, 0);
    CHECK_EQ_INT(m_Score.extends, 2);
    CHECK_EQ_INT(m_Score.lives, 2);
    CHECK_EQ_INT(m_Score.bombs, 3);
    CHECK_EQ_INT(m_Items.big_power, 2);
    CHECK_EQ_INT(m_Items.overflow, 10);
    CHECK_EQ_INT(m_Player.power, 1);
    CHECK_EQ_INT(m_Player.invincibility, 200);
    CHECK_EQ_INT(m_Player.hit, 0);
    CHECK_EQ_INT(m_Player.position[0].x, 208);
    CHECK_EQ_INT(m_Player.position[1].y, 336);
    CHECK_EQ_INT(m_Shots.weapon.cycle[0], 17);
    CHECK_EQ_INT(m_Shots.fire_cycle, 123);
    CHECK_EQ_INT(g_Stage->world.frame, world_frame);
    CHECK_EQ_INT(f.session.frame, session_frame);
    CHECK_EQ_INT(g_Stage->scroll.rows, rows);
    CHECK_EQ_INT(g_Stage->scroll.page_lines[0], 398);
    CHECK_EQ_INT(g_Stage->scroll.page_lines[1], 399);
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, (uint16_t)(count2 + 42));
    CHECK_EQ_INT(g_VSync.edges, edges + 42);
    CHECK(memcmp(&before_random, &m_Rng, sizeof(before_random)) == 0);
    CHECK(memcmp(&performance, &m_Performance, sizeof(performance)) == 0);
    CHECK_EQ_INT(g_Video.drawing_page, g_Stage->scroll.back);
    CHECK_EQ_INT(game->loop.wait, STAGE_QUIT);
    for (unsigned i = 0; i < 42; i++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    }
    CHECK_EQ_INT(game->phase, GAMEPLAY_STAGE);
    CHECK_EQ_INT(m_Player.invincibility, 199);
    Arena_Free(&f.arena);
}

TEST(gameplay_sound_requests_wait_for_the_next_sound_update)
{
    FIXTURE fixture;
    M_Init(&fixture);
    TEST_SOUND sound;
    TestSound_Init(&sound);
    M_Start(&fixture);
    const TH02_INPUT input = {};
    for (unsigned i = 0; i < 100; i++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    }
    M_FinishReveal(&fixture, &input);
    SoundEffects_Play(SOUND_EFFECT_ITEM);
    TestSound_Check(&sound, -1);
    VSyncIrq_VSync();
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    TestSound_Check(&sound, 13);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    VSyncIrq_VSync();
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    TestSound_Check(&sound, -1);
    SoundEffects_Play(SOUND_EFFECT_ITEM);
    M_Start(&fixture);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    TestSound_Free(&sound);
    Arena_Free(&fixture.arena);
}

typedef struct {
    const GAMEPLAY *game;
    unsigned calls;
} MUSIC_REQUEST;

static void M_StageMusic(void *const context)
{
    MUSIC_REQUEST *const request = context;
    CHECK_EQ_INT(request->game->phase, GAMEPLAY_PREPARE);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    request->calls++;
}

TEST(stage_waits_for_both_music_and_frame_gates_before_playing_its_song)
{
    for (unsigned late_music = 0; late_music < 2; late_music++) {
        FIXTURE fixture;
        M_Init(&fixture);
        MUSIC_WAIT wait;
        MusicWait_Init(&wait);
        MUSIC_REQUEST request = { TestGameplay_State(), 0 };
        fixture.music.entry = &wait;
        fixture.music.stage = M_StageMusic;
        fixture.music.bosses.context = &request;
        M_Start(&fixture);
        const TH02_INPUT input = {};
        for (unsigned i = 0; i < 99; i++) {
            VSyncIrq_VSync();
            CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
            CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_MUSIC_FADE);
            CHECK_EQ_INT(g_Stage->world.frame, 0);
            CHECK_EQ_INT(request.calls, 0);
        }
        if (late_music != 0) {
            for (unsigned i = 0; i < 37; i++) {
                VSyncIrq_VSync();
                CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
                CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_MUSIC_FADE);
                CHECK_EQ_INT(g_Stage->world.frame, 0);
                CHECK_EQ_INT(request.calls, 0);
            }
        }
        MusicWait_Stop(&wait);
        if (late_music == 0) {
            CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
            CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_PREPARE);
            VSyncIrq_VSync();
        }
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
        CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_REVEAL);
        M_FinishReveal(&fixture, &input);
        CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_STAGE);
        CHECK_EQ_INT(request.calls, 1);
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
        CHECK_EQ_INT(request.calls, 1);
        Arena_Free(&fixture.arena);
    }
}

TEST(stage_music_fade_precedes_item_and_extend_resets)
{
    FIXTURE f;
    M_Init(&f);
    MUSIC_WAIT wait;
    MusicWait_Init(&wait);
    f.music.entry = &wait;
    M_Start(&f);
    CHECK_EQ_INT(m_Rng.cursor, 0);
    m_Items.slots[0].state = ENTITY_ACTIVE;
    m_Items.point_count = 37;
    m_Score.delta = 123;
    m_Score.extends = 5;
    const TH02_INPUT input = {};
    g_VSync.count1 = 37;
    for (unsigned i = 0; i < 3; i++) {
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
        CHECK_EQ_INT(m_Rng.cursor, 0);
        CHECK_EQ_INT(m_Items.slots[0].state, ENTITY_ACTIVE);
        CHECK_EQ_INT(m_Items.point_count, 37);
        CHECK_EQ_INT(m_Score.delta, 123);
        CHECK_EQ_INT(m_Score.extends, 5);
    }
    MusicWait_Stop(&wait);
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(m_Rng.cursor, 1);
    CHECK_EQ_INT(m_Items.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Items.point_count, 0);
    CHECK_EQ_INT(m_Score.delta, 0);
    CHECK_EQ_INT(m_Score.extends, 0);
    m_Score.delta = 19;
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(m_Rng.cursor, 1);
    CHECK_EQ_INT(m_Score.delta, 19);
    CHECK_EQ_INT(g_VSync.count1, 37);
    g_VSync.count1 = 100;
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_REVEAL);
    CHECK_EQ_INT(m_Rng.cursor, 1);
    CHECK_EQ_INT(m_Score.delta, 19);
    Arena_Free(&f.arena);
}

TEST(stage_entry_keeps_carried_cache_row_and_both_source_strips)
{
    FIXTURE f;
    M_Init(&f);
    GAMEPLAY *const game = TestGameplay_State();
    memset(p_Background->cache.images[24], 99, TILE_COLUMNS);
    for (unsigned page = 0; page < 2; page++) {
        for (unsigned y = 0; y < 16; y++) {
            for (unsigned x = 0; x < 16; x++) {
                g_Video.pages[page][(384 + y) * VIDEO_WIDTH + 624 + x] =
                    6 + page;
            }
        }
    }
    g_VSync.count1 = 100;
    const TH02_INPUT input = {};
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(game->phase, GAMEPLAY_REVEAL);
    for (unsigned x = 0; x < TILE_COLUMNS; x++) {
        CHECK_EQ_INT(p_Background->cache.images[24][x], 99);
    }
    for (unsigned page = 0; page < 2; page++) {
        for (unsigned image = 0; image < TILE_SOURCE_COUNT; image++) {
            CHECK_EQ_INT(
                p_Background->sources[image],
                72 + image / 25 * 2 + image % 25 * 1280);
        }
        CHECK_EQ_INT(g_Video.pages[page][576], 7);
        CHECK_EQ_INT(g_Video.pages[page][16 * VIDEO_WIDTH + 576], 12);
        CHECK_EQ_INT(g_Video.pages[page][384 * VIDEO_WIDTH + 624], 6 + page);
    }
    CHECK_EQ_INT(g_Video.pages[0][384 * VIDEO_WIDTH + 32], 6);
    CHECK_EQ_INT(g_Video.pages[1][384 * VIDEO_WIDTH + 32], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    Arena_Free(&f.arena);
}

TEST(world_cleanup_restores_the_back_page_before_swapping_it)
{
    FIXTURE f;
    M_Init(&f);
    const TH02_INPUT input = {};
    g_VSync.count1 = 100;
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    for (unsigned i = 0; i < 41; i++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    }
    CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_REVEAL);
    for (unsigned page = 0; page < 2; page++) {
        memset(g_Video.pages[page] + 100 * VIDEO_WIDTH + 100, 5, 8);
    }
    SPARK *const spark = &m_Sparks.slots[0];
    spark->state = ENTITY_REMOVE;
    spark->drawing = 1;
    spark->position[g_Stage->scroll.back] =
        (ENTITY_POINT) { 100 * 16, 100 * 16 };
    const uint8_t back = g_Stage->scroll.back;
    const uint8_t front = g_Stage->scroll.front;
    VSyncIrq_VSync();
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(spark->state, ENTITY_FREE);
    CHECK_EQ_INT(g_Video.pages[back][100 * VIDEO_WIDTH + 100], 12);
    CHECK_EQ_INT(g_Video.pages[front][100 * VIDEO_WIDTH + 100], 5);
    CHECK_EQ_INT(g_Video.visible_page, front);
    CHECK_EQ_INT(g_Video.drawing_page, back);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    VSyncIrq_VSync();
    CHECK_EQ_INT(Gameplay_Update(&input), GAMEPLAY_RUNNING);
    CHECK_EQ_INT(g_Video.visible_page, back);
    CHECK_EQ_INT(g_Video.drawing_page, front);
    CHECK_EQ_INT(g_Stage->scroll.front, back);
    CHECK_EQ_INT(g_Stage->scroll.back, front);
    CHECK_EQ_INT(g_Stage->world.frame, 1);
    for (unsigned x = 0; x < TILE_COLUMNS; x++) {
        CHECK_EQ_INT(p_Background->cache.columns[x], 0);
        for (unsigned y = 0; y < TILE_ROWS; y++) {
            CHECK_EQ_INT(p_Background->cache.dirty[x][y], 0);
        }
    }
    Arena_Free(&f.arena);
}
