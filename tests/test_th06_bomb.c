#include "th06/bomb.h"

#include "anm_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/bullet.h"
#include "th06/chain.h"
#include "th06/effect.h"
#include "th06/enemy.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"
#include "th06/scene.h"
#include "th06/screen_effect.h"

#include "core/base/memory.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#define M_START_X 192.0f
#define M_START_Y 384.0f
#define M_MAX_CALLS 64
#define M_OFFSET_X 10.0f
#define M_OFFSET_Y 20.0f

// The test runs the bombs through the real player update. It links these
// instead of the stand-ins in cycle_stubs.c and the modules they stand
// for. Each records its calls.

typedef struct {
    EFFECT_TYPE type;
    VEC3 pos;
    int32_t count;
    uint32_t color;
} M_EFFECT_CALL;

typedef struct {
    int32_t duration;
    int32_t from;
    int32_t to;
} M_SHAKE_CALL;

ENEMY_MANAGER g_Enemies;

static char *m_Directory;
static int32_t m_NameSprite;
static const char *m_Name;
static int32_t m_NameEnds;
static int32_t m_CollectCalls;
static M_EFFECT_CALL m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static M_SHAKE_CALL m_Shakes[M_MAX_CALLS];
static int32_t m_ShakeCount;
static EFFECT m_SpareEffect;

bool Gui_IsMsgRunning(void)
{
    return false;
}

void Gui_ShowBombName(const int32_t sprite, const char *const name)
{
    m_NameSprite = sprite;
    m_Name = name;
}

void Gui_EndBombName(void)
{
    m_NameEnds++;
}

void Item_CollectAll(void)
{
    m_CollectCalls++;
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
}

void Bullet_ClearAll(void)
{
}

EFFECT *Effect_Spawn(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t count,
    const uint32_t color)
{
    if (m_EffectCount < M_MAX_CALLS) {
        m_Effects[m_EffectCount] = (M_EFFECT_CALL) { type, pos, count, color };
    }
    m_EffectCount++;
    return &m_SpareEffect;
}

void ScreenEffect_Shake(
    const int32_t duration, const int32_t from, const int32_t to)
{
    if (m_ShakeCount < M_MAX_CALLS) {
        m_Shakes[m_ShakeCount] = (M_SHAKE_CALL) { duration, from, to };
    }
    m_ShakeCount++;
}

static uint32_t M_Bits(const float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void M_AddScript(ANM_BUILDER *const builder, const int32_t id)
{
    const int32_t script = AnmBuilder_BeginScript(builder, id);
    const uint32_t sprite = 0;
    AnmBuilder_Instr(builder, script, 0, 1, &sprite, 1);
    AnmBuilder_Instr(builder, script, 1000, 0, nullptr, 0);
}

// Adds an orb script that waits at a stop and ends at interrupt 1.
static void M_AddOrbScript(ANM_BUILDER *const builder, const int32_t id)
{
    const int32_t script = AnmBuilder_BeginScript(builder, id);
    const uint32_t sprite = 0;
    const uint32_t label = 1;
    AnmBuilder_Instr(builder, script, 0, 1, &sprite, 1);
    AnmBuilder_Instr(builder, script, 0, 21, nullptr, 0);
    AnmBuilder_Instr(builder, script, 0, 22, &label, 1);
    AnmBuilder_Instr(builder, script, 0, 0, nullptr, 0);
}

// Adds a pillar script that sets its position offset to M_OFFSET_X,
// M_OFFSET_Y.
static void M_AddPillarScript(ANM_BUILDER *const builder, const int32_t id)
{
    const int32_t script = AnmBuilder_BeginScript(builder, id);
    const uint32_t sprite = 0;
    const uint32_t use_offset = 1;
    const uint32_t offset[] = { M_Bits(M_OFFSET_X), M_Bits(M_OFFSET_Y), 0 };
    AnmBuilder_Instr(builder, script, 0, 1, &sprite, 1);
    AnmBuilder_Instr(builder, script, 0, 25, &use_offset, 1);
    AnmBuilder_Instr(builder, script, 0, 17, offset, 3);
    AnmBuilder_Instr(builder, script, 1000, 0, nullptr, 0);
}

// Writes a player ANM file numbered from 0x400: a 16 by 16 sprite, the
// player, option, star and beam scripts, the orb scripts and the pillar
// scripts.
static void M_WriteAnm(const char *const name)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    for (int32_t i = 0; i < 0x0c; i++) {
        M_AddScript(&builder, i);
    }
    M_AddScript(&builder, 0x80);
    M_AddScript(&builder, 0x81);
    for (int32_t i = 0x85; i < 0x89; i++) {
        M_AddOrbScript(&builder, i);
    }
    for (int32_t i = 0x89; i < 0x8d; i++) {
        M_AddPillarScript(&builder, i);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, name, bytes, size);
}

static void M_Frame(void)
{
    Chain_RunCalc(&g_Chain);
}

// Sets the player up alive at the start position of the shot type, and
// starts the bomb.
static void M_Setup(
    const char *const case_name, const uint8_t character,
    const uint8_t shot_type)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    M_WriteAnm("data/player00.anm");
    M_WriteAnm("data/player01.anm");
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    g_GameManager.run.character = character;
    g_GameManager.run.shot_type = shot_type;
    Scene_Request(SCENE_GAMEPLAY);
    g_Enemies = (ENEMY_MANAGER) {};
    g_Buttons = (BUTTONS) {};
    m_NameSprite = 0;
    m_Name = nullptr;
    m_NameEnds = 0;
    m_CollectCalls = 0;
    m_ShakeCount = 0;
    CHECK(Result_Log(Player_Register(), LOG_LEVEL_ERROR));
    M_Frame();
    g_Player.state.value = PLAYER_ALIVE;
    m_EffectCount = 0;
}

static void M_StartBomb(void)
{
    g_GameManager.player.bombs = 3;
    g_Buttons.pressed.bomb = true;
    M_Frame();
    g_Buttons.pressed.bomb = false;
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

// Runs frames up to the one in which the bomb runs its frame `frame`.
static void M_RunTo(const int32_t frame)
{
    while (g_Player.bomb.timer.current <= frame) {
        M_Frame();
    }
}

// Mirrors the steering of an orb to aim.
static void M_Steer(PLAYER_BOMB_PART *const orb, const VEC3 aim)
{
    float x = aim.x - orb->pos.x;
    float y = aim.y - orb->pos.y;
    float len = sqrtf(x * x + y * y) / (orb->speed / 8.0f);
    if (len < 1.0f) {
        len = 1.0f;
    }
    x = x / len + orb->velocity.x;
    y = y / len + orb->velocity.y;
    len = sqrtf(x * x + y * y);
    orb->speed = len > 10.0f ? 10.0f : len;
    if (orb->speed < 1.0f) {
        orb->speed = 1.0f;
    }
    orb->velocity.x = x * orb->speed / len;
    orb->velocity.y = y * orb->speed / len;
    orb->pos.x += orb->velocity.x;
    orb->pos.y += orb->velocity.y;
}

static bool M_SameShake(
    const M_SHAKE_CALL *const shake, const int32_t duration, const int32_t from,
    const int32_t to)
{
    return shake->duration == duration && shake->from == from
        && shake->to == to;
}

// Reimu A

TEST(the_orbs_bomb_starts_with_its_name_and_a_clear_area)
{
    M_Setup("reimu_a_start", 0, 0);
    M_StartBomb();
    CHECK(g_Player.bomb.running);
    CHECK_EQ_INT(m_NameSprite, 0x4a1);
    CHECK_EQ_STR(m_Name, "霊符「夢想封印」");
    CHECK_EQ_INT(g_Player.bomb.duration, 300);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_Player.state.timer.current, 359);
    CHECK_EQ_INT(m_CollectCalls, 1);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_SHOCKWAVE);
    CHECK_EQ_INT(m_Effects[0].color, 0xff4040ffu);
    const PLAYER_CLEAR_AREA area = g_Player.bomb.clear_areas[8];
    CHECK(area.x == M_START_X && area.y == M_START_Y);
    CHECK(area.width == 256.0f && area.height == 256.0f);
    CHECK_EQ_INT(g_Player.bomb.timer.current, 1);
    M_Teardown();
}

TEST(the_orbs_launch_every_16_frames_into_parts_1_to_7)
{
    M_Setup("reimu_a_launch", 0, 0);
    M_StartBomb();
    M_RunTo(79);
    for (int32_t i = 0; i < PLAYER_BOMB_PART_COUNT; i++) {
        CHECK_EQ_INT(g_Player.bomb.parts[i].state, 0);
    }
    M_RunTo(80);
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 1);
    CHECK_EQ_INT(g_Player.bomb.parts[2].state, 0);
    M_RunTo(176);
    CHECK_EQ_INT(g_Player.bomb.parts[0].state, 0);
    for (int32_t i = 1; i < PLAYER_BOMB_PART_COUNT; i++) {
        CHECK_EQ_INT(g_Player.bomb.parts[i].state, 1);
    }
    M_Teardown();
}

TEST(an_orb_launches_at_a_random_angle_and_steers_at_once)
{
    M_Setup("reimu_a_angle", 0, 0);
    M_StartBomb();
    M_RunTo(79);
    RNG rng = g_Rng;
    M_RunTo(80);
    const float angle = Rng_F32(&rng) * 6.2831855f - 3.1415927f;
    PLAYER_BOMB_PART expected = {
        .state = 1,
        .speed = 4.0f,
        .pos = g_Player.pos,
        .velocity = {
            X87_Cos(angle) * 4.0f,
            X87_Sin(angle) * 4.0f,
            0.0f,
        },
    };
    M_Steer(&expected, g_Player.pos);
    const PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[1];
    CHECK(orb->speed == expected.speed);
    CHECK(orb->velocity.x == expected.velocity.x);
    CHECK(orb->velocity.y == expected.velocity.y);
    CHECK(orb->pos.x == expected.pos.x && orb->pos.y == expected.pos.y);
    CHECK_EQ_INT(g_Player.bomb.vms[4].script_index, 0x485);
    CHECK_EQ_INT(g_Player.bomb.vms[7].script_index, 0x488);
    M_Teardown();
}

TEST(an_orb_homes_to_the_target_and_carries_its_boxes)
{
    M_Setup("reimu_a_home", 0, 0);
    M_StartBomb();
    M_RunTo(80);
    PLAYER_BOMB_PART expected = g_Player.bomb.parts[1];
    const VEC3 start = expected.pos;
    const VEC3 target = { 100.0f, 100.0f, 0.0f };
    g_Player.homing_target = target;
    M_Frame();
    M_Steer(&expected, target);
    const PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[1];
    CHECK(orb->velocity.x == expected.velocity.x);
    CHECK(orb->velocity.y == expected.velocity.y);
    CHECK(orb->pos.x == expected.pos.x && orb->pos.y == expected.pos.y);
    const PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[1];
    CHECK(hit->pos.x == start.x && hit->pos.y == start.y);
    CHECK(hit->size.x == 48.0f && hit->size.y == 48.0f);
    CHECK_EQ_INT(hit->damage, 8);
    const PLAYER_CLEAR_AREA area = g_Player.bomb.clear_areas[1];
    CHECK(area.x == start.x && area.width == 48.0f);
    M_Teardown();
}

TEST(an_orb_bursts_after_100_damage)
{
    M_Setup("reimu_a_burst", 0, 0);
    M_StartBomb();
    M_RunTo(80);
    g_Player.bomb.hits[1].damage_dealt = 99;
    M_Frame();
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 1);
    const VEC3 pos = g_Player.bomb.parts[1].pos;
    g_Player.bomb.hits[1].damage_dealt = 100;
    m_EffectCount = 0;
    M_Frame();
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 2);
    const PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[1];
    CHECK(hit->size.x == 256.0f && hit->size.y == 256.0f);
    CHECK_EQ_INT(hit->damage, 200);
    CHECK(g_Player.bomb.clear_areas[1].width == 256.0f);
    CHECK(g_Player.bomb.clear_areas[1].x == pos.x);
    CHECK_EQ_INT(m_EffectCount, 2);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_PARTICLE + 2);
    CHECK_EQ_INT(m_Effects[0].count, 8);
    CHECK(m_Effects[0].pos.x == pos.x);
    CHECK_EQ_INT(m_Effects[1].type, EFFECT_SHOCKWAVE);
    CHECK_EQ_INT(m_ShakeCount, 1);
    CHECK(M_SameShake(&m_Shakes[0], 16, 8, 0));
    CHECK(g_Player.bomb.vms[4].pc < 0);
    CHECK(g_Player.bomb.vms[7].pc < 0);
    M_Teardown();
}

TEST(a_burst_orb_stops_hitting_and_frees_its_part_at_30)
{
    M_Setup("reimu_a_free", 0, 0);
    M_StartBomb();
    M_RunTo(80);
    g_Player.bomb.hits[1].damage_dealt = 100;
    M_Frame();
    M_Frame();
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 3);
    CHECK(g_Player.bomb.hits[1].size.x == 0.0f);
    CHECK(g_Player.bomb.clear_areas[1].width == 0.0f);
    for (int32_t i = 0; i < 26; i++) {
        M_Frame();
    }
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 29);
    M_Frame();
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 0);
    M_Teardown();
}

TEST(the_orbs_burst_in_the_last_30_frames)
{
    M_Setup("reimu_a_last", 0, 0);
    M_StartBomb();
    M_RunTo(269);
    CHECK_EQ_INT(g_Player.bomb.parts[7].state, 1);
    M_RunTo(270);
    for (int32_t i = 1; i < PLAYER_BOMB_PART_COUNT; i++) {
        CHECK_EQ_INT(g_Player.bomb.parts[i].state, 2);
    }
    M_Teardown();
}

TEST(the_orbs_bomb_ends_after_300_frames)
{
    M_Setup("reimu_a_end", 0, 0);
    M_StartBomb();
    M_RunTo(299);
    CHECK(g_Player.bomb.running);
    CHECK_EQ_INT(m_NameEnds, 0);
    M_Frame();
    CHECK(!g_Player.bomb.running);
    CHECK_EQ_INT(m_NameEnds, 1);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_Player.state.timer.current, 59);
    M_Teardown();
}

// Reimu B

TEST(the_pillars_bomb_starts_through_the_player)
{
    M_Setup("reimu_b_start", 0, 1);
    g_Player.pos = (VEC3) { 100.0f, 300.0f, 0.49f };
    M_StartBomb();
    CHECK_EQ_INT(m_NameSprite, 0x4a3);
    CHECK_EQ_STR(m_Name, "夢符「封魔陣」");
    CHECK_EQ_INT(g_Player.bomb.duration, 140);
    CHECK_EQ_INT(g_Player.state.timer.current, 199);
    CHECK_EQ_INT(m_CollectCalls, 1);
    CHECK_EQ_INT(m_ShakeCount, 1);
    CHECK(M_SameShake(&m_Shakes[0], 60, 2, 6));
    const PLAYER_BOMB_PART *const parts = g_Player.bomb.parts;
    CHECK(parts[0].pos.x == 100.0f && parts[0].pos.y == 224.0f);
    CHECK(parts[0].pos.z == 0.42f);
    CHECK(parts[1].pos.x == 192.0f && parts[1].pos.y == 300.0f);
    CHECK(parts[3].pos.z == 0.405f);
    CHECK_EQ_INT(g_Player.bomb.vms[3].script_index, 0x48c);
    CHECK(g_Player.bomb.clear_areas[0].width == 0.0f);
    CHECK(g_Player.bomb.hits[0].size.x == 0.0f);
    M_Teardown();
}

TEST(the_pillars_set_their_boxes_on_odd_frames)
{
    M_Setup("reimu_b_odd", 0, 1);
    g_Player.pos = (VEC3) { 100.0f, 300.0f, 0.49f };
    M_StartBomb();
    M_Frame();
    const PLAYER_CLEAR_AREA *const areas = g_Player.bomb.clear_areas;
    const PLAYER_BOMB_HIT *const hits = g_Player.bomb.hits;
    CHECK(areas[0].x == 100.0f + M_OFFSET_X);
    CHECK(areas[0].y == 224.0f + M_OFFSET_Y);
    CHECK(areas[0].width == 62.0f && areas[0].height == 448.0f);
    CHECK(areas[1].x == 192.0f + M_OFFSET_X);
    CHECK(areas[1].width == 384.0f && areas[1].height == 62.0f);
    CHECK(hits[0].pos.x == 100.0f + M_OFFSET_X);
    CHECK(hits[0].pos.y == 224.0f + M_OFFSET_Y);
    CHECK(hits[0].size.x == 62.0f && hits[0].size.y == 448.0f);
    CHECK_EQ_INT(hits[0].damage, 8);
    M_Frame();
    CHECK(hits[0].size.x == 0.0f);
    CHECK(areas[0].width == 62.0f);
    CHECK(areas[0].x == 100.0f + M_OFFSET_X);
    M_Teardown();
}

TEST(the_pillars_shake_the_screen_again_at_frame_60)
{
    M_Setup("reimu_b_shake", 0, 1);
    M_StartBomb();
    M_RunTo(59);
    CHECK_EQ_INT(m_ShakeCount, 1);
    M_RunTo(60);
    CHECK_EQ_INT(m_ShakeCount, 2);
    CHECK(M_SameShake(&m_Shakes[1], 80, 20, 0));
    M_RunTo(139);
    CHECK(g_Player.bomb.running);
    M_Frame();
    CHECK(!g_Player.bomb.running);
    M_Teardown();
}

// Marisa A

TEST(the_stars_bomb_sends_eight_stars_out)
{
    M_Setup("marisa_a_start", 1, 0);
    M_StartBomb();
    CHECK_EQ_INT(m_NameSprite, 0x4a3);
    CHECK_EQ_STR(m_Name, "魔符「スターダストレヴァリエ」");
    CHECK_EQ_INT(g_Player.bomb.duration, 250);
    CHECK_EQ_INT(g_Player.state.timer.current, 299);
    CHECK_EQ_INT(m_ShakeCount, 1);
    CHECK(M_SameShake(&m_Shakes[0], 120, 4, 1));
    for (int32_t i = 0; i < PLAYER_BOMB_PART_COUNT; i++) {
        const PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[i];
        const float angle = (float)i * 6.2831855f / 8.0f;
        CHECK(star->velocity.x == X87_Cos(angle) * 2.0f);
        CHECK(star->velocity.y == X87_Sin(angle) * 2.0f);
        CHECK(star->pos.x == M_START_X && star->pos.y == M_START_Y);
        CHECK_EQ_INT(g_Player.bomb.vms[i].script_index, 0x405 + i % 3);
    }
    M_Teardown();
}

TEST(the_stars_set_their_boxes_two_frames_in_three)
{
    M_Setup("marisa_a_boxes", 1, 0);
    M_StartBomb();
    M_Frame();
    const PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[2];
    CHECK(star->pos.x == M_START_X + star->velocity.x);
    CHECK(star->pos.y == M_START_Y + star->velocity.y);
    const PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[2];
    CHECK(hit->pos.x == star->pos.x && hit->pos.y == star->pos.y);
    CHECK(hit->size.x == 128.0f && hit->size.y == 128.0f);
    CHECK_EQ_INT(hit->damage, 8);
    CHECK(g_Player.bomb.clear_areas[2].x == star->pos.x);
    CHECK(g_Player.bomb.clear_areas[2].width == 128.0f);
    M_Frame();
    CHECK(hit->size.x == 128.0f);
    M_Frame();
    CHECK(hit->size.x == 0.0f);
    CHECK(g_Player.bomb.clear_areas[2].width == 0.0f);
    M_Teardown();
}

// Marisa B

TEST(the_beam_bomb_slows_the_player)
{
    M_Setup("marisa_b_start", 1, 1);
    M_StartBomb();
    CHECK_EQ_INT(m_NameSprite, 0x4a1);
    CHECK_EQ_STR(m_Name, "恋符「マスタースパーク」");
    CHECK_EQ_INT(g_Player.bomb.duration, 300);
    CHECK_EQ_INT(g_Player.state.timer.current, 359);
    CHECK_EQ_INT(g_Player.bomb.vms[0].script_index, 0x408);
    CHECK_EQ_INT(g_Player.bomb.vms[3].script_index, 0x40b);
    CHECK(g_Player.move.speed_scale_x == 0.3f);
    CHECK(g_Player.move.speed_scale_y == 0.3f);
    g_Buttons.held.right = true;
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X + 5.0f * 0.3f);
    M_Teardown();
}

TEST(the_beam_sets_its_box_three_frames_in_four)
{
    M_Setup("marisa_b_boxes", 1, 1);
    M_StartBomb();
    M_Frame();
    const PLAYER_CLEAR_AREA area = g_Player.bomb.clear_areas[0];
    CHECK(area.x == 192.0f && area.y == M_START_Y / 2.0f);
    CHECK(area.width == 384.0f && area.height == M_START_Y);
    const PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[0];
    CHECK(hit->pos.x == 192.0f && hit->pos.y == M_START_Y / 2.0f);
    CHECK(hit->size.x == 384.0f && hit->size.y == M_START_Y);
    CHECK_EQ_INT(hit->damage, 12);
    M_RunTo(3);
    CHECK(hit->size.x == 384.0f);
    M_RunTo(4);
    CHECK(hit->size.x == 0.0f);
    M_Teardown();
}

TEST(the_beam_shakes_the_screen_at_frames_60_and_120)
{
    M_Setup("marisa_b_shake", 1, 1);
    M_StartBomb();
    M_RunTo(59);
    CHECK_EQ_INT(m_ShakeCount, 0);
    M_RunTo(60);
    CHECK_EQ_INT(m_ShakeCount, 1);
    CHECK(M_SameShake(&m_Shakes[0], 60, 1, 7));
    M_RunTo(120);
    CHECK_EQ_INT(m_ShakeCount, 2);
    CHECK(M_SameShake(&m_Shakes[1], 200, 24, 0));
    M_Teardown();
}

TEST(the_end_of_the_beam_restores_the_speed)
{
    M_Setup("marisa_b_end", 1, 1);
    M_StartBomb();
    M_RunTo(299);
    CHECK(g_Player.move.speed_scale_x == 0.3f);
    M_Frame();
    CHECK(!g_Player.bomb.running);
    CHECK_EQ_INT(m_NameEnds, 1);
    CHECK(g_Player.move.speed_scale_x == 1.0f);
    CHECK(g_Player.move.speed_scale_y == 1.0f);
    M_Teardown();
}
