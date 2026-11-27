#pragma once

#include "th02/enemies.h"
#include "th02/items.h"
#include "th02/stage.h"
#include "th02/stage/private.h"
#include "th02/player.h"
#include "th02_clip_fixture.h"
#include "th02_palette_fixture.h"
#include "th02_gameplay.c"
#include "th02_stage_flow.c"
#include "th02/boss/private.h"
#include "th02_boss_explosion.c"

#include "test.h"
#include "th02_sound.h"
#include "core/base/arena.h"

typedef struct {
    ARENA arena;

    PALETTE_STATE before_palette;
    TH02_SESSION session;
    GAMEPLAY_ASSETS assets;
    STAGE_ASSETS stage;
    BOMB_ASSETS bomb;
    MAP_SECTION sections[MAP_SECTION_COUNT];
    uint8_t order[3];
    uint8_t pixels[512];
    uint8_t eye[256 * 80];
    uint8_t bomb_tail[72];
    GAMEPLAY_MUSIC music;
    bool fix_bomb_transition_overread;
} FIXTURE;

// Returns private shared boss storage for synthetic boundary fixtures.
static inline BOSS_STATE *TestBossFight_State(void)
{
    return p_BossState;
}

static inline void M_Start(FIXTURE *const fixture)
{
    Gameplay_Begin(
        &fixture->session, &fixture->assets, &fixture->music, nullptr, 123456,
        fixture->fix_bomb_transition_overread, fixture->bomb_tail);
    Gameplay_BeginStage(
        &fixture->stage, &fixture->assets.patterns, &fixture->bomb);
}

static inline void M_Init(FIXTURE *const fixture)
{
    TestStageFlow_Boss(nullptr);
    TestBossExplosion_Observe(nullptr);
    SoundEffects_Init(nullptr);
    memset(&m_Palette, 0, sizeof(m_Palette));
    Grcg_Init();
    Tram_Init();
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "Gameplay test", 2 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&fixture->arena);
    Video_Init(allocator);
    Clip_Reset();
    Palette_Init();
    Session_Init(&fixture->session);
    fixture->session.start_lives = 2;
    fixture->session.start_bombs = 3;
    fixture->session.frame = 713;
    PatternBank_Init(&fixture->assets.patterns, allocator);
    uint8_t body[32 * 48];
    uint8_t tiny[256];
    memset(body, 5, sizeof(body));
    memset(tiny, 9, sizeof(tiny));
    for (uint16_t slot = 0; slot < 48; slot++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&fixture->assets.patterns, slot, 32, 48, body, 0)));
    }
    for (uint16_t slot = 48; slot < 128; slot++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&fixture->assets.patterns, slot, 16, 16, tiny, 0)));
        CHECK(Result_Ignore(
            PatternBank_ConvertTiny(&fixture->assets.patterns, slot)));
    }
    uint8_t boss[64 * 96];
    memset(boss, 6, sizeof(boss));
    for (uint16_t slot = 128; slot < 164; slot++) {
        CHECK(Result_Ignore(PatternBank_Put(
            &fixture->assets.patterns, slot, 64, slot < 148 ? 64 : 96, boss,
            0)));
    }
    memset(fixture->eye, 3, sizeof(fixture->eye));
    fixture->assets.backdrop.image = (PI_IMAGE) {
        .width = 256,
        .height = 80,
        .pixels = fixture->eye,
    };
    memset(fixture->pixels, 7, 256);
    memset(fixture->pixels + 256, 12, 256);
    memset(fixture->sections, 1, sizeof(fixture->sections));
    fixture->stage.background.map.sections = fixture->sections;
    fixture->stage.background.tiles =
        (MPN) { .count = 2, .pixels = fixture->pixels };
    fixture->stage.data.map = fixture->order;
    fixture->stage.data.map_count = 3;
    M_Start(fixture);
}

static inline STAGE_FLOW_RESULT M_FlowEnter(FIXTURE *const f)
{
    GAMEPLAY *const game = TestGameplay_State();
    const BOSS_MUSIC music = {
        .boss = game->music.bosses.boss,
        .transition = game->music.bosses.transition,
        .context = game->music.bosses.context,
    };
    return StageFlow_Enter(
        &game->assets.patterns, &game->assets.bosses, &music, game->session,
        &game->dialogue_input);
}

static inline STAGE_FLOW_RESULT M_FlowUpdate(FIXTURE *const f)
{
    GAMEPLAY *const game = TestGameplay_State();
    const BOSS_MUSIC music = {
        .boss = game->music.bosses.boss,
        .transition = game->music.bosses.transition,
        .context = game->music.bosses.context,
    };
    return StageFlow_Update(
        &game->assets.patterns, &game->assets.bosses, &music, game->session,
        &game->dialogue_input);
}

static inline STAGE_FLOW_RESULT M_FlowEnd(FIXTURE *const f)
{
    GAMEPLAY *const game = TestGameplay_State();
    const BOSS_MUSIC music = {
        .boss = game->music.bosses.boss,
        .transition = game->music.bosses.transition,
        .context = game->music.bosses.context,
    };
    return StageFlow_End(
        &game->assets.patterns, &game->assets.bosses, &music, game->session,
        &game->dialogue_input);
}

static inline void M_FinishReveal(
    FIXTURE *const f, const TH02_INPUT *const input)
{
    CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_REVEAL);
    const int32_t session_frame = f->session.frame;
    for (unsigned i = 0; i < 42; i++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Gameplay_Update(input), GAMEPLAY_RUNNING);
        CHECK_EQ_INT(g_Stage->world.frame, 0);
        CHECK_EQ_INT(f->session.frame, session_frame);
        if (i < 41) {
            CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_REVEAL);
        }
    }
    CHECK_EQ_INT(TestGameplay_State()->phase, GAMEPLAY_STAGE);
}
