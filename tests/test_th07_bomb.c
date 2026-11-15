#include "th07/bomb.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/buttons.h"
#include "th07/effect.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screen_effect.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_START_X 192.0f
#define M_START_Y 384.0f
#define M_MAX_CALLS 64
#define M_BOMB 0x2
#define M_OFFSET_X 10.0f
#define M_OFFSET_Y 20.0f
#define M_SPRITE_SIZE 16.0f
#define M_AURA_SCALE_X 2.0f
#define M_AURA_SCALE_Y 3.0f
#define M_REIMU_A 0
#define M_REIMU_B 1
#define M_MARISA_A 2
#define M_MARISA_B 3
#define M_SAKUYA_A 4
#define M_SAKUYA_B 5

// The test runs the bombs through the real player update. It links these
// instead of the modules they stand for. Each records its calls.

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

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
float g_GameSpeed;
ENEMIES g_Enemies;
ASCII_TEXT g_AsciiText;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;
static int32_t m_NameSprite;
static const char *m_Name;
static int32_t m_NameEnds;
static int32_t m_CollectCalls;
static int32_t m_StopCalls;
static M_EFFECT_CALL m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static M_SHAKE_CALL m_Shakes[M_MAX_CALLS];
static int32_t m_ShakeCount;
static EFFECT m_Reserved[EFFECT_RESERVED_COUNT];
static EFFECT_TYPE m_ReservedTypes[EFFECT_RESERVED_COUNT];
static EFFECT m_SpareEffect;

SCENE Scene_GetRequested(void)
{
    return SCENE_GAMEPLAY;
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
}

void Gui_ShowBorderStartPopup(void)
{
}

void Gui_ShowBorderBonusPopup(const int32_t points)
{
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

void Replay_MarkEvent(const REPLAY_EVENT event)
{
}

void Stage_SetTint(const uint32_t color)
{
}

void AsciiText_SetSummaryInterrupt(const int16_t interrupt)
{
    g_AsciiText.summary_interrupt = interrupt;
}

void Bullet_ClearAll(void)
{
}

void Bullet_StopAll(void)
{
    m_StopCalls++;
}

void Bomb_Draw(void)
{
}

void Item_Spawn(const VEC3 pos, const int32_t type, const int32_t state)
{
}

void Item_CollectAll(void)
{
    m_CollectCalls++;
}

void Item_DropPulled(void)
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

EFFECT *Effect_SpawnReserved(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t slot,
    const uint32_t color)
{
    EFFECT *const effect = &m_Reserved[slot];
    *effect = (EFFECT) {};
    Anm_InitVm(&effect->vm);
    effect->vm.color = color;
    effect->vm.scale_x = M_AURA_SCALE_X;
    effect->vm.scale_y = M_AURA_SCALE_Y;
    effect->vm.rotation_speed.z = 0.5f;
    effect->pos = pos;
    effect->active = true;
    m_ReservedTypes[slot] = type;
    return effect;
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

static void M_AddScript(TH07_ANM_BUILDER_ANM *const anm, const int32_t id)
{
    const int32_t script = Th07AnmBuilder_BeginScript(anm, id);
    const uint32_t sprite = 0;
    Th07AnmBuilder_Instr(anm, script, 3, 0, 0, &sprite, 1);
    Th07AnmBuilder_Instr(anm, script, 20, 0, 0, nullptr, 0);
    Th07AnmBuilder_End(anm, script);
}

// Adds an orb script that waits at a stop and ends at interrupt 1.
static void M_AddOrbScript(TH07_ANM_BUILDER_ANM *const anm, const int32_t id)
{
    const int32_t script = Th07AnmBuilder_BeginScript(anm, id);
    const uint32_t sprite = 0;
    const uint32_t label = 1;
    Th07AnmBuilder_Instr(anm, script, 3, 0, 0, &sprite, 1);
    Th07AnmBuilder_Instr(anm, script, 20, 0, 0, nullptr, 0);
    Th07AnmBuilder_Instr(anm, script, 21, 0, 0, &label, 1);
    Th07AnmBuilder_Instr(anm, script, 1, 0, 0, nullptr, 0);
    Th07AnmBuilder_End(anm, script);
}

// Adds a script that sets its position offset to M_OFFSET_X, M_OFFSET_Y.
static void M_AddOffsetScript(TH07_ANM_BUILDER_ANM *const anm, const int32_t id)
{
    const int32_t script = Th07AnmBuilder_BeginScript(anm, id);
    const uint32_t sprite = 0;
    const uint32_t use_offset = 1;
    const uint32_t offset[] = { M_Bits(M_OFFSET_X), M_Bits(M_OFFSET_Y), 0 };
    Th07AnmBuilder_Instr(anm, script, 3, 0, 0, &sprite, 1);
    Th07AnmBuilder_Instr(anm, script, 24, 0, 0, &use_offset, 1);
    Th07AnmBuilder_Instr(anm, script, 6, 0, 0, offset, 3);
    Th07AnmBuilder_Instr(anm, script, 20, 0, 0, nullptr, 0);
    Th07AnmBuilder_End(anm, script);
}

// Writes the player ANM file of character, numbered from 0x400: the
// player, option and shot scripts, then the scripts of its bombs, each
// showing a 16 by 16 sprite.
static void M_WriteAnm(const int32_t character)
{
    static const int32_t scripts[] = { 0, 1, 2, 3, 4, 0x40, 0x80, 0x81 };
    TH07_ANM_BUILDER builder;
    Th07AnmBuilder_Init(&builder);
    TH07_ANM_BUILDER_ANM *const anm = Th07AnmBuilder_Add(&builder, 64, 64, "@");
    Th07AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, M_SPRITE_SIZE, M_SPRITE_SIZE);
    for (size_t i = 0; i < ARRAY_SIZE(scripts); i++) {
        M_AddScript(anm, scripts[i]);
    }
    if (character == 0) {
        for (int32_t i = 0x85; i < 0x89; i++) {
            M_AddOrbScript(anm, i);
        }
        for (int32_t i = 0x89; i < 0x90; i++) {
            M_AddOffsetScript(anm, i);
        }
    } else {
        for (int32_t i = 0x05; i < 0x0f; i++) {
            M_AddScript(anm, i);
        }
        M_AddScript(anm, 0x60);
    }
    char *const name = Str_Format("data/player0%d.anm", character);
    Th07AnmBuilder_WriteFile(&builder, m_Directory, name);
    Mem_Free(name);
}

static void M_WriteSht(const char *const name)
{
    static const TH07_SHT_SHOT shot = {
        .interval = 4,
        .size_x = 12.0f,
        .size_y = 12.0f,
        .angle = -MATH_HALF_PI,
        .speed = 10.0f,
        .damage = 10,
        .kind = SHOT_KIND_PLAIN,
        .script = 0x440,
        .sound = -1,
        .fire = SHOT_FIRE_PLAIN,
    };
    static const TH07_SHT_TABLE table = { 999, 1, &shot };
    static const TH07_SHT sht = {
        .bombs = 3.0f,
        .bomb_window = 8,
        .floats = { 3.0f, 24.0f, 8.0f, 32.0f, 0.25f, 128.0f, 4.0f, 2.0f,
                    2.828427f, 1.4142135f },
        .table_count = 1,
        .tables = &table,
    };
    size_t size;
    uint8_t *const bytes = Th07Data_BuildSht(&sht, &size);
    Th07Data_WriteFile(m_Directory, name, bytes, size);
    Mem_Free(bytes);
}

static void M_Reset(void)
{
    m_NameSprite = 0;
    m_Name = nullptr;
    m_NameEnds = 0;
    m_CollectCalls = 0;
    m_StopCalls = 0;
    m_EffectCount = 0;
    m_ShakeCount = 0;
    for (int32_t i = 0; i < EFFECT_RESERVED_COUNT; i++) {
        m_Reserved[i] = (EFFECT) {};
        m_ReservedTypes[i] = (EFFECT_TYPE)-1;
    }
    Sound_Reset();
}

// Sets the player of shot index shot up alive at the start position, past
// its first update.
static void M_SetUp(const char *const case_name, const uint8_t shot)
{
    static const char *const names[] = {
        "data/ply00a.sht", "data/ply00b.sht", "data/ply01a.sht",
        "data/ply01b.sht", "data/ply02a.sht", "data/ply02b.sht",
    };
    static const char *const focused[] = {
        "data/ply00as.sht", "data/ply00bs.sht", "data/ply01as.sht",
        "data/ply01bs.sht", "data/ply02as.sht", "data/ply02bs.sht",
    };
    GfxRecorder_Start();
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    M_WriteSht(names[shot]);
    M_WriteSht(focused[shot]);
    M_WriteAnm(shot / 2);
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    g_GameManager.run.character = shot / 2;
    g_GameManager.run.shot_type = shot % 2;
    g_GameManager.run.shot_index = shot;
    g_GameManager.cherry.offset = 1000;
    g_GameManager.cherry.value = 1000 + 100000;
    g_GameManager.cherry.max = 1000 + 200000;
    g_GameManager.player.lives = 2.0f;
    g_GameManager.player.bombs = 3.0f;
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    memset(&g_AsciiText, 0, sizeof(g_AsciiText));
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_GameSpeed = 1.0f;
    CHECK(Result_Log(Player_Register(), LOG_LEVEL_ERROR));
    Chain_RunCalc(&g_Chain);
    g_Player.state.value = PLAYER_ALIVE;
    g_Player.state.respawn_clear_frames = 0;
    M_Reset();
}

static void M_TearDown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Th07Data_Unmount();
    GfxRecorder_Stop();
    Mem_Free(m_Directory);
}

static void M_Frame(void)
{
    Chain_RunCalc(&g_Chain);
}

// Starts the bomb, focused or not, at pos.
static void M_StartBombAt(const bool focused, const VEC3 pos)
{
    g_Player.pos = pos;
    g_Player.move.focused = focused;
    g_Buttons.play_word = M_BOMB;
    M_Frame();
    g_Buttons.play_word = 0;
}

static void M_StartBomb(const bool focused)
{
    M_StartBombAt(focused, (VEC3) { M_START_X, M_START_Y, 0.0f });
}

// Runs frames up to the one in which the bomb runs its frame `frame`.
static void M_RunTo(const int32_t frame)
{
    while (g_Player.bomb.running && g_Player.bomb.timer.current <= frame) {
        M_Frame();
    }
}

static bool M_SameShake(
    const M_SHAKE_CALL *const shake, const int32_t duration, const int32_t from,
    const int32_t to)
{
    return shake->duration == duration && shake->from == from
        && shake->to == to;
}

// Returns the clear area at x, y with radius, or nullptr.
static const PLAYER_CLEAR_AREA *M_FindCircle(
    const float x, const float y, const float radius)
{
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        const PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[i];
        if (area->x == x && area->y == y && area->radius == radius) {
            return area;
        }
    }
    return nullptr;
}

static int32_t M_CountAreas(void)
{
    int32_t count = 0;
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        const PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[i];
        count += area->width != 0.0f || area->radius != 0.0f;
    }
    return count;
}

// The start, the end and the drain

typedef struct {
    uint8_t shot;
    bool focused;
    int32_t sprite;
    const char *name;
    int32_t duration;
    int32_t state_frames;
    float speed_scale;
    int32_t drain; // on Normal with 100000 cherry
} M_BOMB_CASE;

static const M_BOMB_CASE m_Bombs[] = {
    { M_REIMU_A, false, 0x4a1, "霊符「夢想封印　散」", 140, 200, 1.0f, 140 },
    { M_REIMU_A, true, 0x4a1, "霊符「夢想封印　集」", 300, 360, 0.6f, 70 },
    { M_REIMU_B, false, 0x4a1, "夢符「封魔陣」", 140, 200, 1.0f, 120 },
    { M_REIMU_B, true, 0x4a1, "夢符「二重結界」", 190, 250, 0.4f, 80 },
    { M_MARISA_A, false, 0x4a3, "魔符「スターダストレヴァリエ」", 200, 250,
      1.0f, 150 },
    { M_MARISA_A, true, 0x4a2, "魔符「ミルキーウェイ」", 260, 310, 0.4f, 120 },
    { M_MARISA_B, false, 0x4a1, "恋符「ノンディレクショナルレーザー」", 300,
      300, 0.4f, 110 },
    { M_MARISA_B, true, 0x4a2, "恋符「マスタースパーク」", 340, 390, 0.2f,
      120 },
    { M_SAKUYA_A, false, 0x4a1, "幻符「インディスクリミネイト」", 160, 210,
      1.0f, 170 },
    { M_SAKUYA_A, true, 0x4a1, "幻符「殺人ドール」", 250, 290, 0.3f, 110 },
    { M_SAKUYA_B, false, 0x4a3, "時符「パーフェクトスクウェア」", 160, 260,
      2.0f, 160 },
    { M_SAKUYA_B, true, 0x4a3, "時符「プライベートスクウェア」", 300, 420, 1.5f,
      90 },
};

TEST(each_bomb_starts_with_its_name_items_drain_and_state_timer)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Bombs); i++) {
        const M_BOMB_CASE *const bomb = &m_Bombs[i];
        M_SetUp("start", bomb->shot);
        M_StartBomb(bomb->focused);
        CHECK(g_Player.bomb.running);
        CHECK_EQ_INT(m_NameSprite, bomb->sprite);
        CHECK_EQ_STR(m_Name, bomb->name);
        CHECK_EQ_INT(m_CollectCalls, 1);
        CHECK_EQ_INT(g_Player.bomb.duration, bomb->duration);
        CHECK_EQ_INT(g_Player.bomb.drain, bomb->drain);
        CHECK_EQ_INT(g_Player.state.value, PLAYER_INVULNERABLE);
        CHECK_EQ_INT(g_Player.state.timer.current, bomb->state_frames - 1);
        CHECK(g_Player.move.speed_scale_x == bomb->speed_scale);
        CHECK(g_Player.move.speed_scale_y == bomb->speed_scale);
        CHECK_EQ_INT(g_Player.bomb.timer.current, 1);
        M_TearDown();
    }
}

TEST(each_bomb_ends_after_its_duration_and_restores_the_speed)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Bombs); i++) {
        const M_BOMB_CASE *const bomb = &m_Bombs[i];
        M_SetUp("end", bomb->shot);
        M_StartBomb(bomb->focused);
        M_RunTo(bomb->duration - 1);
        CHECK(g_Player.bomb.running);
        CHECK_EQ_INT(m_NameEnds, 0);
        M_Frame();
        CHECK(!g_Player.bomb.running);
        CHECK_EQ_INT(m_NameEnds, 1);
        CHECK(g_Player.move.speed_scale_x == 1.0f);
        CHECK(g_Player.move.speed_scale_y == 1.0f);
        M_TearDown();
    }
}

TEST(the_drain_divides_by_difficulty_and_duration_in_tens)
{
    // Reimu A: 0.2 of the cherry, at least 4000, over 140 frames.
    static const struct {
        int32_t difficulty;
        int32_t cherry;
        int32_t drain;
    } cases[] = {
        { 0, 100000, 140 }, { 1, 100000, 140 }, { 2, 100000, 70 },
        { 3, 100000, 30 },  { 4, 100000, 40 },  { 5, 100000, 40 },
        { 0, 1000, 20 },    { 0, 0, 20 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        M_SetUp("drain", M_REIMU_A);
        g_GameManager.run.difficulty = cases[i].difficulty;
        g_GameManager.cherry.value = 1000 + cases[i].cherry;
        M_StartBomb(false);
        CHECK_EQ_INT(g_Player.bomb.drain, cases[i].drain);
        M_TearDown();
    }
}

TEST(the_bomb_aura_becomes_a_shrinking_spell_circle)
{
    M_SetUp("aura", M_REIMU_A);
    EFFECT old = { .active = true };
    g_Player.effects.bomb_aura = &old;
    M_StartBomb(false);
    CHECK(!old.active);
    CHECK_EQ_INT(m_ReservedTypes[0], EFFECT_SPELL_CIRCLE);
    CHECK(g_Player.effects.bomb_aura == &m_Reserved[0]);
    const ANM_VM *const vm = &m_Reserved[0].vm;
    CHECK_EQ_INT(vm->interp_time[ANM_INTERP_SCALE].current, 0);
    CHECK_EQ_INT(vm->interp_length[ANM_INTERP_SCALE], 200);
    CHECK_EQ_INT(vm->interp_ease[ANM_INTERP_SCALE], ANM_EASE_LINEAR);
    CHECK(vm->scale_from.x == M_AURA_SCALE_X);
    CHECK(vm->scale_from.y == M_AURA_SCALE_Y);
    CHECK(vm->scale_to.x == 0.0625f && vm->scale_to.y == 0.0625f);
    CHECK_EQ_INT(vm->int_vars[0], 200);
    CHECK(vm->rotation_speed.z == -0.5f);
    CHECK_EQ_INT(vm->color, 0xffff4040u);
    M_TearDown();
}

// Reimu A

TEST(the_orbs_fan_out_every_6_frames_mirrored_on_the_right)
{
    for (int32_t side = 0; side < 2; side++) {
        M_SetUp("reimu_a_fan", M_REIMU_A);
        const float x = side == 0 ? 100.0f : M_START_X;
        M_StartBombAt(false, (VEC3) { x, M_START_Y, 0.0f });
        M_RunTo(11);
        CHECK_EQ_INT(g_Player.bomb.parts[0].state, 0);
        M_RunTo(12);
        CHECK_EQ_INT(g_Player.bomb.parts[0].state, 1);
        CHECK_EQ_INT(g_Player.bomb.parts[1].state, 0);
        CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_GUN);
        M_RunTo(18);
        const PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[1];
        const float sign = side == 0 ? 1.0f : -1.0f;
        const float angle = Angle_AddNormalize(
            (float)(int32_t)sign * MATH_TWO_PI / 8.0f - MATH_HALF_PI, 0.0f);
        CHECK(orb->angle == angle);
        CHECK(orb->speed == 15.0f - 0.4f);
        float vx;
        float vy;
        X87_SinCosScale(angle, orb->speed, &vx, &vy);
        CHECK(orb->velocity.x == vx && orb->velocity.y == vy);
        CHECK(orb->pos.x == x + vx && orb->pos.y == M_START_Y + vy);
        CHECK_EQ_INT(orb->vms[3].script_index, 0x488);
        M_TearDown();
    }
}

TEST(the_orbs_launch_into_12_parts_but_only_8_move)
{
    M_SetUp("reimu_a_twelve", M_REIMU_A);
    M_StartBomb(false);
    M_RunTo(77);
    CHECK_EQ_INT(g_Player.bomb.parts[11].state, 0);
    g_Player.damage_boxes[11].damage_dealt = 5;
    Sound_Reset();
    M_RunTo(78);
    const PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[11];
    CHECK_EQ_INT(orb->state, 1);
    CHECK_EQ_INT(orb->vms[0].script_index, 0x485);
    CHECK_EQ_INT(g_Player.damage_boxes[11].damage_dealt, 0);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_GUN);
    M_RunTo(84);
    CHECK(orb->pos.x == M_START_X && orb->pos.y == M_START_Y);
    CHECK(orb->speed == 15.0f);
    CHECK_EQ_INT(g_Player.bomb.parts[12].state, 0);
    M_TearDown();
}

TEST(a_flying_orb_carries_a_box_and_a_clear_circle)
{
    M_SetUp("reimu_a_box", M_REIMU_A);
    M_StartBomb(false);
    M_RunTo(12);
    const VEC3 pos = g_Player.bomb.parts[0].pos;
    M_Frame();
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    CHECK(box->pos.x == pos.x && box->pos.y == pos.y);
    CHECK(box->size.x == 48.0f && box->size.y == 48.0f);
    CHECK_EQ_INT(box->damage, 8);
    CHECK(M_FindCircle(pos.x, pos.y, 128.0f) != nullptr);
    M_TearDown();
}

TEST(a_bursting_orb_loses_its_400_box_to_the_flying_box)
{
    M_SetUp("reimu_a_burst", M_REIMU_A);
    M_StartBomb(false);
    M_RunTo(12);
    PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[0];
    while (orb->speed - 0.4f >= -10.0f) {
        M_Frame();
    }
    m_EffectCount = 0;
    m_ShakeCount = 0;
    Sound_Reset();
    const VEC3 pos = orb->pos;
    M_Frame();
    CHECK_EQ_INT(orb->state, 2);
    CHECK(orb->velocity.x == 0.0f && orb->velocity.y == 0.0f);
    CHECK(orb->pos.x == pos.x && orb->pos.y == pos.y);
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    CHECK(box->size.x == 48.0f);
    CHECK_EQ_INT(box->damage, 8);
    const PLAYER_CLEAR_AREA *const burst = M_FindCircle(pos.x, pos.y, 64.0f);
    CHECK(burst != nullptr);
    CHECK(burst->radius_growth == 4.2666669f);
    CHECK_EQ_INT(burst->frames_left, 30);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_PARTICLE + 2);
    CHECK_EQ_INT(m_Effects[0].count, 8);
    CHECK_EQ_INT(m_Effects[1].type, EFFECT_SHOCKWAVE);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SHOT_0_ALT);
    CHECK(M_SameShake(&m_Shakes[0], 16, 8, 0));
    CHECK_EQ_INT(orb->vms[0].pending_interrupt, 0);
    CHECK(orb->vms[0].pc < 0);
    M_Frame();
    CHECK(box->size.x == 256.0f);
    CHECK_EQ_INT(box->damage, 2);
    for (int32_t i = 0; i < 28; i++) {
        M_Frame();
    }
    CHECK_EQ_INT(orb->state, 2);
    M_Frame();
    CHECK_EQ_INT(orb->state, 0);
    M_TearDown();
}

TEST(the_orbs_slow_at_the_game_speed)
{
    M_SetUp("reimu_a_speed", M_REIMU_A);
    M_StartBomb(false);
    M_RunTo(12);
    const PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[0];
    const float speed = orb->speed;
    const VEC3 pos = orb->pos;
    g_GameSpeed = 0.5f;
    M_Frame();
    CHECK(orb->speed == speed - 0.4f * 0.5f);
    CHECK(orb->pos.x == pos.x + 0.5f * orb->velocity.x);
    M_TearDown();
}

// Reimu A focused

// Mirrors the steering of a homing orb to aim.
static void M_Steer(PLAYER_BOMB_PART *const orb, const VEC3 aim)
{
    float x = aim.x - orb->pos.x;
    float y = aim.y - orb->pos.y;
    float len = X87_Sqrt(x * x + y * y) / (orb->homing_speed / 8.0f);
    if (len < 1.0f) {
        len = 1.0f;
    }
    x = x / len + orb->velocity.x;
    y = y / len + orb->velocity.y;
    len = X87_Sqrt(x * x + y * y);
    orb->homing_speed = len > 10.0f ? 10.0f : len;
    if (orb->homing_speed < 1.0f) {
        orb->homing_speed = 1.0f;
    }
    orb->velocity.x = x * orb->homing_speed / len;
    orb->velocity.y = y * orb->homing_speed / len;
    orb->pos.x += orb->velocity.x;
    orb->pos.y += orb->velocity.y;
}

TEST(a_homing_orb_launches_at_a_random_angle_into_parts_1_to_7)
{
    M_SetUp("reimu_a_focused_launch", M_REIMU_A);
    M_StartBomb(true);
    M_RunTo(79);
    for (int32_t i = 0; i < 8; i++) {
        CHECK_EQ_INT(g_Player.bomb.parts[i].state, 0);
    }
    RNG rng = g_Rng;
    M_RunTo(80);
    const float angle = Rng_F32(&rng) * MATH_TWO_PI - MATH_PI;
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    PLAYER_BOMB_PART expected = { .homing_speed = 8.0f, .pos = g_Player.pos };
    X87_SinCosScale(angle, 8.0f, &expected.velocity.x, &expected.velocity.y);
    M_Steer(&expected, g_Player.pos);
    const PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[1];
    CHECK_EQ_INT(orb->state, 1);
    CHECK(orb->homing_speed == expected.homing_speed);
    CHECK(orb->velocity.x == expected.velocity.x);
    CHECK(orb->velocity.y == expected.velocity.y);
    CHECK(orb->pos.x == expected.pos.x && orb->pos.y == expected.pos.y);
    M_RunTo(176);
    CHECK_EQ_INT(g_Player.bomb.parts[0].state, 0);
    CHECK_EQ_INT(g_Player.bomb.parts[7].state, 1);
    M_TearDown();
}

TEST(a_homing_orb_relaunches_on_each_call_of_its_frame_below_full_speed)
{
    M_SetUp("reimu_a_focused_slow", M_REIMU_A);
    M_StartBomb(true);
    M_RunTo(79);
    g_GameSpeed = 0.5f;
    const int32_t generated = g_Rng.generated;
    Bomb_Update();
    CHECK_EQ_INT(g_Player.bomb.timer.current, 80);
    const int32_t per_launch = g_Rng.generated - generated;
    CHECK(per_launch > 0);
    Bomb_Update();
    CHECK_EQ_INT(g_Player.bomb.timer.current, 81);
    CHECK_EQ_INT(g_Rng.generated - generated, 2 * per_launch);
    M_TearDown();
}

TEST(a_homing_orb_bursts_after_100_damage_and_keeps_flying)
{
    M_SetUp("reimu_a_focused_burst", M_REIMU_A);
    M_StartBomb(true);
    M_RunTo(80);
    const PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[1];
    g_Player.damage_boxes[1].damage_dealt = 100;
    m_EffectCount = 0;
    M_Frame();
    CHECK_EQ_INT(orb->state, 2);
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[1];
    CHECK(box->size.x == 256.0f && box->size.y == 256.0f);
    CHECK_EQ_INT(box->damage, 400);
    const VEC3 velocity = orb->velocity;
    CHECK(velocity.x != 0.0f || velocity.y != 0.0f);
    CHECK_EQ_INT(m_EffectCount, 2);
    bool found = false;
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        const PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[i];
        found |= area->radius == 32.0f && area->radius_growth == 6.6666665f
            && area->frames_left == 15;
    }
    CHECK(found);
    const VEC3 pos = orb->pos;
    M_Frame();
    CHECK(orb->pos.x == pos.x + velocity.x);
    CHECK(box->size.x == 0.0f);
    M_TearDown();
}

TEST(the_homing_orbs_burst_in_the_last_30_frames)
{
    M_SetUp("reimu_a_focused_last", M_REIMU_A);
    M_StartBomb(true);
    M_RunTo(269);
    CHECK_EQ_INT(g_Player.bomb.parts[7].state, 1);
    M_RunTo(270);
    for (int32_t i = 1; i < 8; i++) {
        CHECK_EQ_INT(g_Player.bomb.parts[i].state, 2);
    }
    M_TearDown();
}

// Reimu B

TEST(the_barriers_start_through_the_player)
{
    M_SetUp("reimu_b_start", M_REIMU_B);
    M_StartBombAt(false, (VEC3) { 100.0f, 300.0f, 0.0f });
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_POWER_1);
    CHECK(M_SameShake(&m_Shakes[0], 60, 2, 6));
    const PLAYER_BOMB_PART *const parts = g_Player.bomb.parts;
    CHECK(parts[0].pos.x == 100.0f && parts[0].pos.y == 224.0f);
    CHECK(parts[0].pos.z == 0.42f);
    CHECK(parts[1].pos.x == 192.0f && parts[1].pos.y == 300.0f);
    CHECK(parts[3].pos.z == 0.405f);
    CHECK_EQ_INT(parts[3].vms[0].script_index, 0x48c);
    CHECK(g_Player.damage_boxes[0].size.x == 0.0f);
    M_TearDown();
}

TEST(the_barriers_move_their_boxes_on_odd_frames_only)
{
    M_SetUp("reimu_b_odd", M_REIMU_B);
    M_StartBombAt(false, (VEC3) { 100.0f, 300.0f, 0.0f });
    M_Frame();
    const PLAYER_CLEAR_AREA *const areas = g_Player.clear_areas;
    const PLAYER_DAMAGE_BOX *const boxes = g_Player.damage_boxes;
    CHECK(areas[0].x == 100.0f + M_OFFSET_X);
    CHECK(areas[0].y == 224.0f + M_OFFSET_Y);
    CHECK(areas[0].width == 62.0f && areas[0].height == 448.0f);
    CHECK(areas[1].x == 192.0f + M_OFFSET_X);
    CHECK(areas[1].width == 384.0f && areas[1].height == 62.0f);
    CHECK(boxes[0].pos.x == 100.0f + M_OFFSET_X);
    CHECK(boxes[0].size.x == 62.0f && boxes[0].size.y == 448.0f);
    CHECK_EQ_INT(boxes[0].damage, 16);
    M_Frame();
    CHECK(boxes[0].size.x == 0.0f);
    CHECK(areas[0].x == 100.0f && areas[0].y == 300.0f);
    CHECK(areas[3].width == 384.0f);
    M_TearDown();
}

TEST(the_barriers_and_boundary_shake_again_at_frame_60)
{
    for (int32_t focused = 0; focused < 2; focused++) {
        M_SetUp("reimu_b_shake", M_REIMU_B);
        M_StartBomb(focused != 0);
        M_RunTo(59);
        CHECK_EQ_INT(m_ShakeCount, 1);
        M_RunTo(60);
        CHECK_EQ_INT(m_ShakeCount, 2);
        CHECK(M_SameShake(&m_Shakes[1], 80, 20, 0));
        M_TearDown();
    }
}

TEST(the_boundary_hits_on_every_frame_around_the_start)
{
    M_SetUp("reimu_b_focused", M_REIMU_B);
    M_StartBombAt(true, (VEC3) { 100.0f, 300.0f, 0.0f });
    const PLAYER_CLEAR_AREA *const circle =
        M_FindCircle(100.0f, 300.0f, 192.0f);
    CHECK(circle != nullptr);
    CHECK(circle->radius_growth == 0.384f);
    CHECK_EQ_INT(circle->frames_left, 210);
    CHECK_EQ_INT(g_Player.bomb.parts[0].vms[2].script_index, 0x48f);
    g_Player.pos.x = 50.0f;
    for (int32_t i = 0; i < 2; i++) {
        M_Frame();
        const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
        CHECK(box->pos.x == 100.0f + M_OFFSET_X);
        CHECK(box->pos.y == 300.0f + M_OFFSET_Y);
        CHECK(box->size.x == 256.0f && box->size.y == 256.0f);
        CHECK_EQ_INT(box->damage, 18);
    }
    M_TearDown();
}

// Marisa A

TEST(the_stars_go_out_in_eight_directions)
{
    M_SetUp("marisa_a_start", M_MARISA_A);
    M_StartBomb(false);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_POWER_1);
    CHECK(M_SameShake(&m_Shakes[0], 120, 4, 1));
    for (int32_t i = 0; i < 8; i++) {
        const PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[i];
        float vx;
        float vy;
        X87_SinCosScale((float)i * MATH_TWO_PI / 8.0f, 2.0f, &vx, &vy);
        CHECK(star->velocity.x == vx && star->velocity.y == vy);
        CHECK(star->pos.x == M_START_X && star->pos.y == M_START_Y);
        CHECK_EQ_INT(star->vms[0].script_index, 0x405 + i % 3);
    }
    M_TearDown();
}

TEST(the_stars_set_their_boxes_two_frames_in_three)
{
    M_SetUp("marisa_a_boxes", M_MARISA_A);
    M_StartBomb(false);
    g_GameSpeed = 0.5f;
    M_Frame();
    const PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[2];
    CHECK(star->pos.x == M_START_X + star->velocity.x * 0.5f);
    CHECK(star->pos.y == M_START_Y + star->velocity.y * 0.5f);
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[2];
    CHECK(box->pos.x == star->pos.x && box->pos.y == star->pos.y);
    CHECK(box->size.x == 128.0f && box->size.y == 128.0f);
    CHECK_EQ_INT(box->damage, 8);
    CHECK(M_FindCircle(star->pos.x, star->pos.y, 96.0f) != nullptr);
    M_Frame();
    CHECK(box->size.x == 0.0f);
    g_GameSpeed = 1.0f;
    M_Frame();
    CHECK_EQ_INT(g_Player.bomb.timer.current, 3);
    CHECK(box->size.x == 128.0f);
    M_Frame();
    CHECK(box->size.x == 0.0f);
    CHECK_EQ_INT(M_CountAreas(), 0);
    M_TearDown();
}

// Marisa A focused

static float M_MilkyWayAngle(RNG *const rng)
{
    return Rng_F32(rng) * 0.3926991f - 0.19634955f - MATH_HALF_PI;
}

TEST(a_milky_way_star_draws_its_velocity_then_its_acceleration)
{
    M_SetUp("marisa_a_focused_rng", M_MARISA_A);
    g_Player.bomb.running = true;
    g_Player.bomb.focused = true;
    g_Player.bomb.duration = 999;
    Timer_Set(&g_Player.bomb.timer, 0);
    RNG rng = g_Rng;
    Bomb_Update();
    const float angle = M_MilkyWayAngle(&rng);
    const float turn = M_MilkyWayAngle(&rng);
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    VEC3 velocity = {};
    VEC3 acceleration = {};
    X87_SinCosScale(angle, -5.0f, &velocity.x, &velocity.y);
    X87_SinCosScale(turn, 0.24f, &acceleration.x, &acceleration.y);
    const PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[0];
    CHECK_EQ_INT(star->state, 1);
    CHECK(star->acceleration.x == acceleration.x);
    CHECK(star->acceleration.y == acceleration.y);
    CHECK(star->pos.x == M_START_X + velocity.x);
    CHECK(star->velocity.x == velocity.x + acceleration.x);
    CHECK(star->velocity.y == velocity.y + acceleration.y);
    CHECK(star->trail[0].x == M_START_X && star->trail[7].y == M_START_Y);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SHOT_0);
    CHECK(M_SameShake(&m_Shakes[0], 120, 4, 1));
    M_TearDown();
}

TEST(the_milky_way_launches_a_star_every_6_frames_up_to_24)
{
    M_SetUp("marisa_a_focused_launch", M_MARISA_A);
    M_StartBomb(true);
    M_RunTo(5);
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 0);
    M_RunTo(6);
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 1);
    CHECK_EQ_INT(g_Player.bomb.parts[1].vms[0].script_index, 0x406);
    M_RunTo(138);
    CHECK_EQ_INT(g_Player.bomb.parts[23].state, 1);
    const int32_t generated = g_Rng.generated;
    M_RunTo(144);
    CHECK_EQ_INT(g_Rng.generated, generated);
    CHECK_EQ_INT(g_Player.bomb.parts[24].state, 0);
    M_TearDown();
}

TEST(a_milky_way_star_stops_hitting_after_80_damage_and_frees_off_the_top)
{
    M_SetUp("marisa_a_focused_box", M_MARISA_A);
    M_StartBomb(true);
    PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[0];
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    M_Frame();
    CHECK(box->size.x == 128.0f);
    CHECK_EQ_INT(box->damage, 12);
    CHECK(box->pos.x == star->pos.x && box->pos.y == star->pos.y);
    g_Player.damage_boxes[0].damage_dealt = 80;
    M_Frame();
    CHECK(box->size.x == 0.0f);
    CHECK(M_FindCircle(star->pos.x, star->pos.y, 96.0f) != nullptr);
    star->pos.y = -250.0f;
    star->velocity = (VEC3) { 0.0f, -10.0f, 0.0f };
    M_Frame();
    CHECK_EQ_INT(star->state, 0);
    CHECK(M_FindCircle(star->pos.x, -260.0f, 96.0f) != nullptr);
    M_TearDown();
}

// Marisa B

TEST(the_lasers_turn_away_from_the_start_side_faster_over_time)
{
    for (int32_t side = 0; side < 2; side++) {
        M_SetUp("marisa_b_turn", M_MARISA_B);
        const float x = side == 0 ? 100.0f : M_START_X;
        M_StartBombAt(false, (VEC3) { x, M_START_Y, 0.0f });
        CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SPARK);
        const PLAYER_BOMB_PART *const laser = &g_Player.bomb.parts[1];
        float angle = MATH_TWO_PI / 3.0f - MATH_HALF_PI;
        CHECK(laser->laser_angle == angle);
        CHECK_EQ_INT(laser->vms[0].script_index, 0x40d);
        const float pi = side == 0 ? MATH_PI : -MATH_PI;
        for (int32_t frame = 1; frame < 3; frame++) {
            M_Frame();
            angle =
                Angle_AddNormalize(angle, (float)frame * pi / 30.0f / 300.0f);
            CHECK(laser->laser_angle == angle);
        }
        M_TearDown();
    }
}

TEST(each_laser_carries_six_boxes_along_its_sprite)
{
    M_SetUp("marisa_b_boxes", M_MARISA_B);
    M_StartBomb(false);
    M_Frame();
    for (int32_t i = 0; i < 3; i++) {
        const PLAYER_BOMB_PART *const laser = &g_Player.bomb.parts[i];
        const float angle = laser->laser_angle;
        const float c = X87_Cos(angle);
        const float s = X87_Sin(angle);
        CHECK(
            laser->vms[0].pos.x == M_START_X + c * M_SPRITE_SIZE * 1.0f / 2.0f);
        CHECK(
            laser->vms[0].pos.y == M_START_Y + s * M_SPRITE_SIZE * 1.0f / 2.0f);
        float len = 32.0f;
        for (int32_t k = 0; k < 6; k++) {
            const PLAYER_DAMAGE_BOX *const box =
                &g_Player.damage_boxes[i * 6 + k];
            CHECK(box->pos.x == M_START_X + c * len);
            CHECK(box->pos.y == M_START_Y + s * len);
            CHECK(box->size.x == 128.0f && box->size.y == 128.0f);
            CHECK_EQ_INT(box->damage, 10);
            CHECK(M_FindCircle(box->pos.x, box->pos.y, 64.0f) != nullptr);
            len += M_SPRITE_SIZE * 1.0f / 5.0f;
        }
    }
    CHECK(g_Player.damage_boxes[18].size.x == 0.0f);
    M_TearDown();
}

TEST(the_lasers_shake_the_screen_at_frames_20_and_80)
{
    M_SetUp("marisa_b_shake", M_MARISA_B);
    M_StartBomb(false);
    M_RunTo(19);
    CHECK_EQ_INT(m_ShakeCount, 0);
    M_RunTo(20);
    CHECK(M_SameShake(&m_Shakes[0], 60, 1, 7));
    M_RunTo(80);
    CHECK_EQ_INT(m_ShakeCount, 2);
    CHECK(M_SameShake(&m_Shakes[1], 100, 24, 0));
    M_TearDown();
}

// Marisa B focused

TEST(the_spark_sets_its_box_three_frames_in_four)
{
    M_SetUp("marisa_b_focused_boxes", M_MARISA_B);
    M_StartBomb(true);
    CHECK_EQ_INT(g_Player.bomb.parts[0].vms[3].script_index, 0x40b);
    M_Frame();
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    CHECK(box->pos.x == 192.0f && box->pos.y == M_START_Y / 2.0f);
    CHECK(box->size.x == 384.0f && box->size.y == M_START_Y);
    CHECK_EQ_INT(box->damage, 23);
    const PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[0];
    CHECK(area->x == 192.0f && area->y == M_START_Y / 2.0f);
    CHECK(area->width == 384.0f && area->height == M_START_Y);
    M_RunTo(3);
    CHECK(box->size.x == 384.0f);
    M_RunTo(4);
    CHECK(box->size.x == 0.0f);
    M_TearDown();
}

TEST(the_spark_shakes_the_screen_at_frames_60_and_120)
{
    M_SetUp("marisa_b_focused_shake", M_MARISA_B);
    M_StartBomb(true);
    M_RunTo(59);
    CHECK_EQ_INT(m_ShakeCount, 0);
    M_RunTo(60);
    CHECK(M_SameShake(&m_Shakes[0], 60, 1, 7));
    M_RunTo(120);
    CHECK_EQ_INT(m_ShakeCount, 2);
    CHECK(M_SameShake(&m_Shakes[1], 200, 24, 0));
    M_TearDown();
}

// Sakuya A

typedef struct {
    float angle, speed, speed_step, turn;
} M_KNIFE;

static M_KNIFE M_DrawKnife(RNG *const rng)
{
    M_KNIFE knife;
    knife.angle = Rng_F32(rng) * MATH_TWO_PI - MATH_PI;
    knife.speed = Rng_F32(rng) * 6.0f + 5.5f;
    knife.speed_step = Rng_F32(rng) * 0.1f + 0.1f;
    knife.turn = Rng_F32(rng) * 0.06283186f - 0.031415927f;
    return knife;
}

TEST(the_knives_start_with_the_knife_effect)
{
    M_SetUp("sakuya_a_start", M_SAKUYA_A);
    M_StartBomb(false);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_KNIFE_BOMB);
    CHECK(m_Effects[0].pos.x == M_START_X && m_Effects[0].pos.y == M_START_Y);
    CHECK_EQ_INT(m_Effects[0].count, 1);
    CHECK_EQ_INT(m_Effects[0].color, 0xffffffffu);
    CHECK(g_Player.bomb.parts[0].effect == &m_SpareEffect);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_POWER_0);
    M_TearDown();
}

TEST(the_knives_fly_five_at_a_time_from_frame_60_to_120)
{
    M_SetUp("sakuya_a_throw", M_SAKUYA_A);
    M_StartBomb(false);
    g_Player.pos.x = 100.0f;
    M_RunTo(59);
    CHECK_EQ_INT(g_Player.bomb.parts[0].state, 0);
    RNG rng = g_Rng;
    M_RunTo(60);
    M_KNIFE expected[5];
    for (int32_t i = 0; i < 5; i++) {
        expected[i] = M_DrawKnife(&rng);
        CHECK_EQ_INT(g_Player.bomb.parts[i].state, 1);
        CHECK_EQ_INT(g_Player.bomb.parts[i].vms[0].script_index, 0x405 + i % 2);
    }
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    CHECK_EQ_INT(g_Player.bomb.parts[5].state, 0);
    PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[0];
    VEC3 velocity = {};
    X87_SinCosScale(expected[0].angle, 24.0f, &velocity.x, &velocity.y);
    CHECK(knife->angle == expected[0].angle);
    CHECK(knife->speed == expected[0].speed);
    CHECK(knife->speed_step == expected[0].speed_step);
    CHECK(knife->acceleration.x == expected[0].turn);
    CHECK(knife->pos.x == M_START_X + velocity.x);
    CHECK(knife->pos.y == M_START_Y + velocity.y);
    const VEC3 pos = knife->pos;
    M_Frame();
    CHECK_EQ_INT(g_Player.bomb.parts[9].state, 1);
    CHECK_EQ_INT(g_Player.bomb.parts[10].state, 0);
    const float angle = Angle_AddNormalize(expected[0].angle, expected[0].turn);
    const float speed = expected[0].speed + expected[0].speed_step;
    X87_SinCosScale(angle, speed, &velocity.x, &velocity.y);
    CHECK(knife->angle == angle && knife->speed == speed);
    CHECK(knife->velocity.x == velocity.x && knife->velocity.y == velocity.y);
    CHECK(knife->pos.x == pos.x + velocity.x);
    CHECK(knife->pos.y == pos.y + velocity.y);
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    CHECK(box->pos.x == knife->pos.x && box->pos.y == knife->pos.y);
    CHECK(box->size.x == 24.0f && box->size.y == 24.0f);
    CHECK_EQ_INT(box->damage, 10);
    CHECK(M_FindCircle(knife->pos.x, knife->pos.y, 32.0f) != nullptr);
    M_RunTo(120);
    const int32_t generated = g_Rng.generated;
    M_Frame();
    CHECK_EQ_INT(g_Rng.generated, generated);
    M_TearDown();
}

TEST(the_knives_throw_turn_and_speed_up_on_each_call_below_full_speed)
{
    M_SetUp("sakuya_a_slow", M_SAKUYA_A);
    M_StartBomb(false);
    M_RunTo(59);
    g_GameSpeed = 0.5f;
    Bomb_Update();
    CHECK_EQ_INT(g_Player.bomb.timer.current, 60);
    const PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[0];
    const float angle = knife->angle;
    const float speed = knife->speed;
    const VEC3 pos = knife->pos;
    CHECK_EQ_INT(g_Player.bomb.parts[5].state, 0);
    Bomb_Update();
    CHECK_EQ_INT(g_Player.bomb.timer.current, 61);
    CHECK_EQ_INT(g_Player.bomb.parts[9].state, 1);
    CHECK(knife->angle == Angle_AddNormalize(angle, knife->acceleration.x));
    CHECK(knife->speed == speed + knife->speed_step);
    CHECK(knife->pos.x == pos.x + knife->velocity.x * 0.5f);
    M_TearDown();
}

TEST(a_knife_stops_hitting_after_30_damage_and_frees_off_the_play_area)
{
    M_SetUp("sakuya_a_hit", M_SAKUYA_A);
    M_StartBomb(false);
    M_RunTo(60);
    PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[0];
    PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    box->damage_dealt = 30;
    const VEC3 pos = knife->pos;
    const float speed = knife->speed;
    M_Frame();
    CHECK_EQ_INT(knife->vms[0].script_index, 0x460);
    CHECK_EQ_INT(box->damage_dealt, 999);
    CHECK(box->size.x == 0.0f);
    CHECK(knife->pos.x == pos.x && knife->pos.y == pos.y);
    CHECK(knife->speed == speed + knife->speed_step);
    CHECK_EQ_INT(knife->state, 1);
    knife->pos.x = -100.0f;
    M_Frame();
    CHECK_EQ_INT(knife->state, 0);
    CHECK_EQ_INT(knife->vms[0].script_index, 0x460);
    M_TearDown();
}

// Sakuya A focused

TEST(the_doll_throws_two_knives_every_2_frames_from_frame_20)
{
    M_SetUp("sakuya_a_focused_throw", M_SAKUYA_A);
    M_StartBomb(true);
    M_RunTo(19);
    CHECK_EQ_INT(m_ShakeCount, 0);
    RNG rng = g_Rng;
    M_RunTo(20);
    for (int32_t k = 0; k < 2; k++) {
        const int32_t i = k * 48;
        const PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[i];
        const float speed = Rng_F32(&rng) * 1.0f + 0.5f;
        const float step = Rng_F32(&rng) * 0.1f + 0.03f;
        CHECK_EQ_INT(Rng_U16InRange(&rng, 1), 0);
        const float angle = (float)i * MATH_TWO_PI / 96.0f - MATH_PI;
        CHECK_EQ_INT(knife->state, 1);
        CHECK_EQ_INT(knife->vms[0].script_index, 0x407);
        CHECK(knife->angle == angle);
        CHECK(knife->speed == speed + step);
        CHECK(knife->acceleration.x == -0.15707964f);
        CHECK_EQ_INT(knife->timer.current, 1);
        VEC3 out = {};
        VEC3 velocity = {};
        X87_SinCosScale(angle, 24.0f, &out.x, &out.y);
        X87_SinCosScale(angle, speed + step, &velocity.x, &velocity.y);
        CHECK(knife->pos.x == M_START_X + out.x + velocity.x);
        CHECK(knife->pos.y == M_START_Y + out.y + velocity.y);
        const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[i];
        CHECK(box->pos.x == knife->pos.x && box->size.x == 24.0f);
        CHECK_EQ_INT(box->damage, 22);
    }
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    CHECK_EQ_INT(g_Player.bomb.parts[1].state, 0);
    M_RunTo(22);
    CHECK_EQ_INT(g_Player.bomb.parts[1].vms[0].script_index, 0x408);
    CHECK_EQ_INT(g_Player.bomb.parts[49].state, 1);
    M_RunTo(114);
    CHECK_EQ_INT(g_Player.bomb.parts[47].state, 1);
    CHECK_EQ_INT(g_Player.bomb.parts[95].state, 1);
    M_TearDown();
}

TEST(the_doll_restarts_its_sound_and_shake_on_each_call)
{
    M_SetUp("sakuya_a_focused_shake", M_SAKUYA_A);
    M_StartBomb(true);
    M_RunTo(19);
    g_GameSpeed = 0.5f;
    Sound_Reset();
    Bomb_Update();
    CHECK_EQ_INT(m_ShakeCount, 1);
    CHECK(M_SameShake(&m_Shakes[0], 120, 4, 1));
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_POWER_1);
    Sound_Reset();
    const int32_t generated = g_Rng.generated;
    Bomb_Update();
    CHECK_EQ_INT(g_Player.bomb.timer.current, 21);
    CHECK_EQ_INT(m_ShakeCount, 2);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_POWER_1);
    CHECK_EQ_INT(g_Rng.generated, generated);
    g_GameSpeed = 1.0f;
    M_RunTo(115);
    const int32_t shakes = m_ShakeCount;
    M_Frame();
    CHECK_EQ_INT(m_ShakeCount, shakes);
    M_TearDown();
}

TEST(the_knife_effect_follows_the_player_to_frame_60)
{
    M_SetUp("sakuya_a_focused_effect", M_SAKUYA_A);
    M_StartBomb(true);
    M_RunTo(59);
    g_Player.pos.x = 100.0f;
    M_Frame();
    CHECK(m_SpareEffect.pos.x == 100.0f);
    g_Player.pos.x = 120.0f;
    M_Frame();
    CHECK(m_SpareEffect.pos.x == 100.0f);
    M_TearDown();
}

TEST(a_doll_knife_spins_in_place_then_flies_at_the_target)
{
    M_SetUp("sakuya_a_focused_spin", M_SAKUYA_A);
    M_StartBomb(true);
    M_RunTo(48);
    PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[0];
    CHECK_EQ_INT(knife->timer.current, 29);
    M_Frame();
    const VEC3 pos = knife->pos;
    float angle = knife->angle;
    for (int32_t i = 0; i < 40; i++) {
        M_Frame();
        angle = Angle_AddNormalize(angle, -0.15707964f);
        CHECK(knife->angle == angle);
        CHECK(knife->velocity.x == 0.0f && knife->velocity.y == 0.0f);
        CHECK(knife->pos.x == pos.x && knife->pos.y == pos.y);
    }
    CHECK_EQ_INT(knife->timer.current, 70);
    const VEC3 target = { 300.0f, 50.0f, 0.0f };
    g_Player.homing_target = target;
    M_Frame();
    angle =
        Angle_AddNormalize(X87_Atan2(target.y - pos.y, target.x - pos.x), 0.0f);
    CHECK(knife->angle == angle);
    CHECK(knife->speed == 14.0f + knife->speed_step);
    float vx;
    float vy;
    X87_SinCosScale(angle, knife->speed, &vx, &vy);
    CHECK(knife->pos.x == pos.x + vx && knife->pos.y == pos.y + vy);
    M_TearDown();
}

TEST(a_doll_knife_bursts_on_its_first_hit)
{
    M_SetUp("sakuya_a_focused_hit", M_SAKUYA_A);
    M_StartBomb(true);
    M_RunTo(20);
    PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[0];
    PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    box->damage_dealt = 1;
    m_EffectCount = 0;
    const VEC3 pos = knife->pos;
    M_Frame();
    CHECK_EQ_INT(knife->vms[0].script_index, 0x460);
    CHECK_EQ_INT(box->damage_dealt, 999);
    CHECK(box->size.x == 0.0f);
    CHECK(knife->pos.x == pos.x && knife->pos.y == pos.y);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_EXPLOSION);
    CHECK(m_Effects[0].pos.x == pos.x && m_Effects[0].pos.y == pos.y);
    CHECK_EQ_INT(m_Effects[0].color, 0xffff80ffu);
    M_Frame();
    CHECK_EQ_INT(m_EffectCount, 1);
    M_TearDown();
}

// Sakuya B

TEST(the_square_stops_the_bullets_at_frames_0_60_and_120)
{
    M_SetUp("sakuya_b_stop", M_SAKUYA_B);
    M_StartBomb(false);
    CHECK_EQ_INT(m_StopCalls, 1);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SPARK);
    M_RunTo(59);
    CHECK_EQ_INT(m_StopCalls, 1);
    M_RunTo(60);
    CHECK_EQ_INT(m_StopCalls, 2);
    M_RunTo(119);
    CHECK_EQ_INT(m_StopCalls, 2);
    M_RunTo(120);
    CHECK_EQ_INT(m_StopCalls, 3);
    CHECK(g_Player.state.bomb_started);
    M_TearDown();
}

TEST(the_square_puts_four_sprites_in_the_corners_at_frame_30)
{
    M_SetUp("sakuya_b_corners", M_SAKUYA_B);
    M_StartBomb(false);
    M_RunTo(29);
    CHECK_EQ_INT(g_Player.bomb.parts[0].state, 0);
    M_RunTo(30);
    static const VEC2 corners[] = {
        { 64.0f, 96.0f },
        { 320.0f, 96.0f },
        { 64.0f, 352.0f },
        { 320.0f, 352.0f },
    };
    for (int32_t i = 0; i < 4; i++) {
        const PLAYER_BOMB_PART *const part = &g_Player.bomb.parts[i];
        CHECK_EQ_INT(part->state, 1);
        CHECK_EQ_INT(part->vms[0].script_index, 0x409 + i);
        CHECK(part->vms[0].pos.x == corners[i].x);
        CHECK(part->vms[0].pos.y == corners[i].y);
        CHECK(part->vms[0].pos.z == 0.49f);
    }
    M_TearDown();
}

TEST(the_square_hits_the_play_area_on_multiples_of_4_from_frame_30)
{
    M_SetUp("sakuya_b_box", M_SAKUYA_B);
    M_StartBomb(false);
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    M_RunTo(28);
    CHECK(box->size.x == 0.0f);
    M_RunTo(31);
    CHECK(box->size.x == 0.0f);
    M_RunTo(32);
    CHECK(box->pos.x == 192.0f && box->pos.y == 224.0f);
    CHECK(box->size.x == 352.0f && box->size.y == 416.0f);
    CHECK_EQ_INT(box->damage, 3);
    M_RunTo(33);
    CHECK(box->size.x == 0.0f);
    M_TearDown();
}

TEST(the_squares_shake_the_screen_at_frames_40_and_100)
{
    for (int32_t focused = 0; focused < 2; focused++) {
        M_SetUp("sakuya_b_shake", M_SAKUYA_B);
        M_StartBomb(focused != 0);
        M_RunTo(39);
        CHECK_EQ_INT(m_ShakeCount, 0);
        M_RunTo(40);
        CHECK(M_SameShake(&m_Shakes[0], 60, 1, 7));
        M_RunTo(100);
        CHECK_EQ_INT(m_ShakeCount, 2);
        CHECK(M_SameShake(&m_Shakes[1], 70, 24, 0));
        M_TearDown();
    }
}

TEST(the_square_clears_a_circle_of_800_as_it_ends)
{
    for (int32_t focused = 0; focused < 2; focused++) {
        M_SetUp("sakuya_b_end", M_SAKUYA_B);
        M_StartBomb(focused != 0);
        M_RunTo(g_Player.bomb.duration - 1);
        g_Player.pos.x = 100.0f;
        M_Frame();
        CHECK(!g_Player.bomb.running);
        const PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[0];
        CHECK(area->radius == 800.0f);
        if (focused == 0) {
            CHECK(area->x == 100.0f && area->y == M_START_Y);
            CHECK(area->width == 0.0f);
        } else {
            CHECK(area->x == 192.0f && area->y == 224.0f);
            CHECK(area->width == 448.0f && area->height == 512.0f);
        }
        M_TearDown();
    }
}

// Sakuya B focused

TEST(the_private_square_stops_the_bullets_at_frames_40_and_100)
{
    M_SetUp("sakuya_b_focused_stop", M_SAKUYA_B);
    M_StartBomb(true);
    CHECK_EQ_INT(m_StopCalls, 0);
    CHECK(!g_Player.state.bomb_started);
    M_RunTo(39);
    CHECK_EQ_INT(m_StopCalls, 0);
    M_RunTo(40);
    CHECK_EQ_INT(m_StopCalls, 1);
    M_RunTo(100);
    CHECK_EQ_INT(m_StopCalls, 2);
    M_TearDown();
}

TEST(the_private_square_hits_around_its_first_sprite_on_each_call)
{
    M_SetUp("sakuya_b_focused_box", M_SAKUYA_B);
    M_StartBomb(true);
    const PLAYER_BOMB_PART *const part = &g_Player.bomb.parts[0];
    CHECK_EQ_INT(part->vms[0].script_index, 0x40d);
    CHECK_EQ_INT(g_Player.bomb.parts[1].vms[0].script_index, 0x40e);
    const PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[0];
    CHECK(box->pos.x == M_START_X && box->pos.y == M_START_Y);
    CHECK(box->size.x == 160.0f && box->size.y == 160.0f);
    CHECK_EQ_INT(box->damage, 1);
    CHECK(M_FindCircle(M_START_X, M_START_Y, 96.0f) != nullptr);
    const int32_t areas = M_CountAreas();
    g_GameSpeed = 0.5f;
    Bomb_Update();
    Bomb_Update();
    CHECK_EQ_INT(M_CountAreas(), areas + 2);
    M_TearDown();
}

TEST(the_private_square_pulls_its_sprites_back_to_the_player)
{
    M_SetUp("sakuya_b_focused_pull", M_SAKUYA_B);
    M_StartBomb(true);
    PLAYER_BOMB_PART *const part = &g_Player.bomb.parts[1];
    CHECK(part->pos.x == M_START_X && part->pos.y == M_START_Y);
    CHECK(part->acceleration.y == 0.0f);
    CHECK(part->trail[31].x == M_START_X);
    part->velocity = (VEC3) { 1.0f, -2.0f, 0.0f };
    part->pos = (VEC3) { 100.0f, 200.0f, 0.0f };
    g_GameSpeed = 0.5f;
    M_Frame();
    const VEC3 acceleration = Vec3_Scale(
        Vec3_Scale(
            Vec3_Sub(
                (VEC3) { M_START_X, M_START_Y, 0.0f },
                (VEC3) { 100.0f, 200.0f, 0.0f }),
            1.0f / 1700.0f),
        0.5f);
    CHECK(part->acceleration.x == acceleration.x);
    CHECK(part->acceleration.y == acceleration.y);
    const VEC3 velocity =
        Vec3_Add((VEC3) { 1.0f, -2.0f, 0.0f }, Vec3_Scale(acceleration, 0.5f));
    CHECK(part->velocity.x == velocity.x && part->velocity.y == velocity.y);
    CHECK(part->trail[0].x == 100.0f && part->trail[0].y == 200.0f);
    CHECK(part->trail[1].x == M_START_X);
    CHECK(part->pos.x == 100.0f + velocity.x * 0.5f);
    CHECK(part->pos.y == 200.0f + velocity.y * 0.5f);
    M_TearDown();
}
