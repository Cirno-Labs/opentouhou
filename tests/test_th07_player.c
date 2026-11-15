#include "th07/player.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/bomb.h"
#include "th07/buttons.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_FAN_ANGLE 0.22439948f
#define M_START_X 192.0f
#define M_START_Y 384.0f
#define M_MAX_CALLS 64
#define M_SHOOT 0x1
#define M_BOMB 0x2
#define M_FOCUS 0x4
#define M_UP 0x10
#define M_DOWN 0x20
#define M_LEFT 0x40
#define M_RIGHT 0x80
#define M_EFFECT_GRAZE ((EFFECT_TYPE)(EFFECT_PARTICLE + 4))

// The test links these instead of the modules they stand for. Each
// records its calls.

typedef struct {
    EFFECT_TYPE type;
    VEC3 pos;
    int32_t count;
    uint32_t color;
} M_EFFECT_CALL;

typedef struct {
    VEC3 pos;
    ITEM_TYPE type;
    ITEM_STATE state;
} M_ITEM_CALL;

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
float g_GameSpeed;
ENEMY_MANAGER g_Enemies;
ASCII_TEXT g_AsciiText;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;
static SCENE m_Scene;
static bool m_MsgRunning;
static uint32_t m_Events;
static uint32_t m_Tint;
static int32_t m_ClearCalls;
static int32_t m_BombCalls;
static int32_t m_BombDraws;
static int32_t m_CollectCalls;
static int32_t m_DropCalls;
static int32_t m_StartPopups;
static int32_t m_BonusPopup;
static M_EFFECT_CALL m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static EFFECT m_Reserved[EFFECT_RESERVED_COUNT];
static EFFECT_TYPE m_ReservedTypes[EFFECT_RESERVED_COUNT];
static M_ITEM_CALL m_Items[M_MAX_CALLS];
static int32_t m_ItemCount;
static EFFECT m_SpareEffect;

SCENE Scene_GetRequested(void)
{
    return m_Scene;
}

bool Gui_IsMsgRunning(void)
{
    return m_MsgRunning;
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
}

void Gui_ShowBorderStartPopup(void)
{
    m_StartPopups++;
}

void Gui_ShowBorderBonusPopup(const int32_t points)
{
    m_BonusPopup = points;
}

void Replay_MarkEvent(const REPLAY_EVENT event)
{
    m_Events |= event;
}

void Stage_SetTint(const uint32_t color)
{
    m_Tint = color;
}

void AsciiText_SetSummaryInterrupt(const int16_t interrupt)
{
    g_AsciiText.summary.pending_interrupt = interrupt;
    g_AsciiText.summary_interrupt = interrupt;
}

void Bullet_ClearAll(void)
{
    m_ClearCalls++;
}

void Bomb_Update(void)
{
    m_BombCalls++;
}

void Bomb_Draw(const ANM_DRAW *const draw)
{
    m_BombDraws++;
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
    if (m_ItemCount < M_MAX_CALLS) {
        m_Items[m_ItemCount] = (M_ITEM_CALL) { pos, type, state };
    }
    m_ItemCount++;
}

void Item_CollectAll(void)
{
    m_CollectCalls++;
}

void Item_DropPulled(void)
{
    m_DropCalls++;
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
    effect->vm.rotation_speed.z = 0.5f;
    effect->pos = pos;
    effect->active = true;
    m_ReservedTypes[slot] = type;
    return effect;
}

// Writes a player ANM file numbered from 0x400: the turning scripts, the
// option scripts and a shot script, each showing a 16 by 16 sprite.
static void M_WriteAnm(const char *const name)
{
    static const int32_t scripts[] = { 0, 1, 2, 3, 4, 0x40, 0x80, 0x81 };
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    const uint32_t sprite = 0;
    for (size_t i = 0; i < ARRAY_SIZE(scripts); i++) {
        const int32_t script = AnmBuilder_BeginScript(anm, scripts[i]);
        AnmBuilder_Instr(anm, script, 3, 0, 0, &sprite, 1);
        AnmBuilder_End(anm, script);
    }
    AnmBuilder_WriteFile(&builder, m_Directory, name);
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
        // The boxes, the item reach, the share of the cherry that a death
        // takes, the item line and the speeds.
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
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    memset(&g_AsciiText, 0, sizeof(g_AsciiText));
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_GameSpeed = 1.0f;
    m_Scene = SCENE_GAMEPLAY;
    m_MsgRunning = false;
    m_Events = 0;
    m_Tint = 0;
    m_ClearCalls = 0;
    m_BombCalls = 0;
    m_BombDraws = 0;
    m_CollectCalls = 0;
    m_DropCalls = 0;
    m_StartPopups = 0;
    m_BonusPopup = -1;
    m_EffectCount = 0;
    m_ItemCount = 0;
    for (int32_t i = 0; i < EFFECT_RESERVED_COUNT; i++) {
        m_Reserved[i] = (EFFECT) {};
        m_ReservedTypes[i] = (EFFECT_TYPE)-1;
    }
    Sound_Reset();
}

// Writes the files of shot index 0 and, unless without_focused, its
// focused shot type, then sets the game up for shot shot.
static void M_SetUpFiles(
    const char *const case_name, const uint8_t shot, const bool without_focused)
{
    GfxRecorder_Start();
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    static const char *const names[] = {
        "data/ply00a.sht", "data/ply00b.sht", "data/ply01a.sht",
        "data/ply01b.sht", "data/ply02a.sht", "data/ply02b.sht",
    };
    static const char *const focused[] = {
        "data/ply00as.sht", "data/ply00bs.sht", "data/ply01as.sht",
        "data/ply01bs.sht", "data/ply02as.sht", "data/ply02bs.sht",
    };
    M_WriteSht(names[shot]);
    if (!without_focused) {
        M_WriteSht(focused[shot]);
    }
    char *const anm = Str_Format("data/player0%d.anm", shot / 2);
    M_WriteAnm(anm);
    Mem_Free(anm);
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    g_GameManager.run.character = shot / 2;
    g_GameManager.run.shot_type = shot % 2;
    g_GameManager.run.shot_index = shot;
    g_GameManager.rank.value = 20;
    g_GameManager.rank.max = 99;
    g_GameManager.cherry.offset = 1000;
    g_GameManager.cherry.value = 1000;
    g_GameManager.cherry.max = 1000 + 100000;
    g_GameManager.cherry.plus = 1000;
    g_GameManager.player.lives = 2.0f;
    g_GameManager.player.bombs = 3.0f;
    M_Reset();
}

static void M_SetUp(const char *const case_name, const uint8_t shot)
{
    M_SetUpFiles(case_name, shot, false);
    CHECK(Result_Log(Player_Register(), LOG_LEVEL_ERROR));
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

static void M_Frames(const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        M_Frame();
    }
}

static void M_Hold(const uint16_t word)
{
    g_Buttons.play_word = word;
}

// Sets the player up past its first update, alive at the start position,
// with the counts of its first update cleared.
static void M_SetUpAlive(const char *const case_name, const uint8_t shot)
{
    M_SetUp(case_name, shot);
    M_Frame();
    g_Player.state.value = PLAYER_ALIVE;
    g_Player.state.respawn_clear_frames = 0;
    M_Reset();
}

static VEC3 M_At(const float x, const float y)
{
    return (VEC3) { x, y, 0.0f };
}

static const VEC3 m_Small = { 2.0f, 2.0f, 0.0f };

static int32_t M_CountEffects(const EFFECT_TYPE type)
{
    int32_t count = 0;
    for (int32_t i = 0; i < m_EffectCount && i < M_MAX_CALLS; i++) {
        count += m_Effects[i].type == type;
    }
    return count;
}

static bool M_Queued(const int32_t sound)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        if (g_SoundEvents.queue[i] == sound) {
            return true;
        }
    }
    return false;
}

// Registration and the state

TEST(register_reads_the_shot_type_and_places_the_player_respawning)
{
    M_SetUp("register", 0);
    CHECK(g_Player.state.value == PLAYER_RESPAWNING);
    CHECK_EQ_INT(g_Player.state.timer.current, 120);
    CHECK(g_Player.pos.x == M_START_X && g_Player.pos.y == M_START_Y);
    CHECK(g_Player.box.hitbox_size.x == 1.5f);
    CHECK(g_Player.box.hitbox_size.y == 1.5f);
    CHECK(g_Player.box.graze_size.x == 12.0f);
    CHECK(g_Player.box.collect_size.x == 12.0f);
    CHECK(g_Player.box.collect_size.z == 5.0f);
    CHECK_EQ_INT(g_Player.state.bomb_window, 8);
    CHECK(g_Player.shots.aim_angle == -MATH_HALF_PI);
    CHECK(g_Player.options.state == PLAYER_OPTIONS_OUT);
    CHECK_EQ_INT(g_Player.shot_type_focused.table_count, 1);
    CHECK(Anm_GetScript(0x480) != nullptr);
    CHECK_EQ_INT(g_AsciiText.summary_interrupt, 1);
    CHECK_EQ_INT(g_AsciiText.boss_markers[2].pending_interrupt, 2);
    CHECK_EQ_INT(g_AsciiText.boss_markers[3].pending_interrupt, 0);
    M_TearDown();
}

TEST(register_fails_without_the_focused_shot_type)
{
    M_SetUpFiles("no_focused", 0, true);
    CHECK(!Result_Ignore(Player_Register()));
    M_TearDown();
}

TEST(remove_releases_the_sprites_and_the_shot_types)
{
    M_SetUp("remove", 2);
    Player_Remove();
    CHECK(Anm_GetScript(0x400) == nullptr);
    CHECK(g_Player.shot_type.tables == nullptr);
    CHECK(g_Player.shot_type_focused.tables == nullptr);
    CHECK_EQ_INT(g_AsciiText.summary_interrupt, 99);
    CHECK_EQ_INT(g_AsciiText.boss_markers[0].pending_interrupt, 99);
    M_TearDown();
}

TEST(the_stage_scenes_keep_the_sprites)
{
    const SCENE scenes[] = {
        SCENE_NEXT_STAGE,
        SCENE_RESTART_STAGE,
        SCENE_NEXT_STAGE_AGAIN,
    };
    for (size_t i = 0; i < ARRAY_SIZE(scenes); i++) {
        M_SetUp("keep", 0);
        m_Scene = scenes[i];
        Player_Remove();
        CHECK(Anm_GetScript(0x400) != nullptr);
        CHECK(g_Player.shot_type.tables == nullptr);
        CHECK_EQ_INT(g_AsciiText.summary_interrupt, 1);
        M_TearDown();
    }
}

TEST(a_full_plus_leaves_the_border_pending_on_register)
{
    M_SetUpFiles("full_plus", 0, false);
    g_GameManager.cherry.plus = 1000 + 60000;
    CHECK(Result_Log(Player_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_GameManager.cherry.plus, 1000 + 50000);
    CHECK(g_Player.border_state == PLAYER_BORDER_PENDING);
    M_TearDown();
}

TEST(the_first_update_makes_the_player_invulnerable)
{
    M_SetUp("first_update", 0);
    M_Frame();
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_Player.state.timer.current, 239);
    CHECK_EQ_INT(g_Player.state.bomb_window, 8);
    CHECK(g_Player.vm.color == COLOR_WHITE);
    M_TearDown();
}

TEST(a_respawn_clears_the_bullets_for_60_frames)
{
    M_SetUp("respawn_clear", 0);
    M_Frames(100);
    CHECK_EQ_INT(m_ClearCalls, 60);
    M_TearDown();
}

TEST(invulnerability_blinks_and_ends_after_240_frames)
{
    M_SetUp("invulnerable", 0);
    M_Frames(2);
    CHECK_EQ_INT(g_Player.state.timer.current, 238);
    CHECK(g_Player.vm.color == COLOR_WHITE);
    M_Frames(6);
    CHECK_EQ_INT(g_Player.state.timer.current, 232);
    CHECK(g_Player.vm.color == COLOR_ARGB(0xff, 0x40, 0x40, 0x40));
    M_Frames(232);
    CHECK(g_Player.state.value == PLAYER_ALIVE);
    CHECK_EQ_INT(g_Player.state.timer.current, 0);
    M_TearDown();
}

// Movement and the options

TEST(the_player_moves_at_the_speeds_of_the_shot_type)
{
    M_SetUpAlive("move", 0);
    M_Hold(M_RIGHT);
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X + 4.0f);
    M_Hold(M_RIGHT | M_FOCUS);
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X + 6.0f);
    M_Hold(M_LEFT | M_UP);
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X + 6.0f - 2.828427f);
    CHECK(g_Player.pos.y == M_START_Y - 2.828427f);
    M_Hold(M_RIGHT | M_DOWN | M_FOCUS);
    const float x = g_Player.pos.x;
    M_Frame();
    CHECK(g_Player.pos.x == x + 1.4142135f);
    M_TearDown();
}

TEST(the_player_moves_at_the_game_speed_and_the_speed_scale)
{
    M_SetUpAlive("move_speed", 0);
    g_GameSpeed = 0.5f;
    g_Player.move.speed_scale_x = 0.5f;
    M_Hold(M_LEFT | M_DOWN);
    M_Frame();
    CHECK(g_Player.move.last_x == -2.828427f);
    CHECK(g_Player.move.delta_x == -2.828427f * 0.5f * 0.5f);
    CHECK(g_Player.pos.x == M_START_X + -2.828427f * 0.5f * 0.5f);
    CHECK(g_Player.pos.y == M_START_Y + 2.828427f * 0.5f);
    M_TearDown();
}

TEST(right_wins_over_left_and_up_over_down)
{
    M_SetUpAlive("both_ways", 0);
    M_Hold(M_LEFT | M_RIGHT | M_UP | M_DOWN);
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X + 2.828427f);
    CHECK(g_Player.pos.y == M_START_Y - 2.828427f);
    M_TearDown();
}

TEST(the_player_stays_in_the_player_area_with_its_boxes)
{
    M_SetUpAlive("area", 0);
    g_Player.pos = M_At(9.0f, 17.0f);
    M_Hold(M_LEFT | M_UP);
    M_Frame();
    CHECK(g_Player.pos.x == 8.0f && g_Player.pos.y == 16.0f);
    CHECK(g_Player.box.hitbox_min.x == 6.5f);
    CHECK(g_Player.box.graze_max.y == 28.0f);
    CHECK(g_Player.box.collect_min.x == -4.0f);
    g_Player.pos = M_At(375.0f, 431.0f);
    M_Hold(M_RIGHT | M_DOWN);
    M_Frame();
    CHECK(g_Player.pos.x == 376.0f && g_Player.pos.y == 432.0f);
    M_TearDown();
}

TEST(a_turn_starts_the_turning_scripts)
{
    M_SetUpAlive("turn", 0);
    M_Hold(M_LEFT);
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x401);
    M_Hold(0);
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x402);
    M_Hold(M_RIGHT);
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x403);
    M_Hold(0);
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x404);
    M_TearDown();
}

TEST(the_options_slide_in_while_focused_with_the_focus_aura)
{
    M_SetUpAlive("slide", 0);
    M_Frame();
    CHECK(g_Player.options.pos[0].x == M_START_X - 24.0f);
    M_Hold(M_FOCUS);
    M_Frame();
    CHECK(g_Player.options.state == PLAYER_OPTIONS_SLIDING_IN);
    CHECK(m_ReservedTypes[2] == EFFECT_PLAYER_AURA);
    CHECK(g_Player.effects.focus == &m_Reserved[2]);
    M_Frames(8);
    CHECK(g_Player.options.state == PLAYER_OPTIONS_IN);
    M_Frame();
    CHECK(g_Player.options.pos[0].x == M_START_X - 8.0f);
    CHECK(g_Player.options.pos[1].y == M_START_Y - 32.0f);
    M_Hold(0);
    M_Frame();
    CHECK(g_Player.options.state == PLAYER_OPTIONS_SLIDING_OUT);
    CHECK_EQ_INT(m_Reserved[2].vm.pending_interrupt, 1);
    M_TearDown();
}

TEST(a_slide_that_turns_back_starts_where_the_other_was)
{
    M_SetUpAlive("slide_back", 0);
    M_Hold(M_FOCUS);
    M_Frames(3);
    M_Hold(0);
    M_Frame();
    CHECK(g_Player.options.state == PLAYER_OPTIONS_SLIDING_OUT);
    CHECK_EQ_INT(g_Player.options.timer.current, 5);
    M_TearDown();
}

TEST(sakuya_b_fans_the_options_along_the_aim_angle)
{
    M_SetUpAlive("fan", 5);
    g_Player.shots.aim_angle = -1.0f;
    M_Frame();
    const float side_x = X87_Cos(-1.0f + MATH_HALF_PI) * 24.0f;
    const float side_y = X87_Sin(-1.0f + MATH_HALF_PI) * 24.0f;
    CHECK(g_Player.options.pos[0].x == M_START_X - side_x);
    CHECK(g_Player.options.pos[0].y == M_START_Y - side_y);
    CHECK(g_Player.options.pos[1].y == M_START_Y + side_y);
    M_Hold(M_FOCUS);
    M_Frames(9);
    CHECK(g_Player.options.state == PLAYER_OPTIONS_IN);
    M_Frame();
    CHECK(
        g_Player.options.pos[1].x
        == M_START_X + X87_Cos(-1.0f + M_FAN_ANGLE) * 24.0f);
    CHECK(
        g_Player.options.pos[0].y
        == M_START_Y + X87_Sin(-1.0f - M_FAN_ANGLE) * 24.0f);
    M_TearDown();
}

TEST(sakuya_b_fans_out_over_8_frames)
{
    M_SetUpAlive("fan_out", 5);
    M_Hold(M_FOCUS);
    M_Frame();
    M_Frame();
    const float angle = g_Player.shots.aim_angle;
    const float t = 2.0f / 8.0f;
    const float side = X87_Cos(angle + MATH_HALF_PI) * 24.0f;
    const float spread = X87_Cos(angle + M_FAN_ANGLE) * 24.0f;
    CHECK(
        g_Player.options.pos[1].x == M_START_X + ((spread - side) * t + side));
    const float other = X87_Cos(angle - M_FAN_ANGLE) * 24.0f;
    CHECK(g_Player.options.pos[0].x == M_START_X + ((other + side) * t - side));
    M_TearDown();
}

TEST(the_aim_angle_turns_against_the_move_up_to_its_limit)
{
    M_SetUpAlive("aim", 0);
    M_Hold(M_SHOOT | M_RIGHT);
    M_Frame();
    const float step = -(4.0f / 4.0f) * 3.1415927f / 5.0f / 10.0f;
    CHECK(g_Player.shots.aim_angle == -MATH_HALF_PI - step);
    M_Frames(20);
    CHECK(g_Player.shots.aim_angle == -0.9424778f);
    M_Hold(M_SHOOT | M_RIGHT | M_FOCUS);
    g_Player.shots.aim_angle = -1.0f;
    M_Frame();
    CHECK(g_Player.shots.aim_angle == -1.0f);
    M_TearDown();
}

TEST(the_aim_angle_returns_upwards_while_still)
{
    M_SetUpAlive("aim_back", 0);
    g_Player.shots.aim_angle = -1.0f;
    g_GameSpeed = 0.5f;
    M_Hold(M_SHOOT);
    M_Frame();
    CHECK(g_Player.shots.aim_angle == -1.0f + -0.062831856f * 0.5f);
    g_Player.shots.aim_angle = -MATH_HALF_PI + 0.03f;
    M_Frame();
    CHECK(g_Player.shots.aim_angle == -MATH_HALF_PI);
    m_MsgRunning = true;
    g_Player.shots.aim_angle = -1.0f;
    M_Frame();
    CHECK(g_Player.shots.aim_angle == -1.0f);
    M_TearDown();
}

TEST(the_shot_button_fires_outside_a_dialogue)
{
    M_SetUpAlive("fire", 0);
    m_MsgRunning = true;
    M_Hold(M_SHOOT);
    M_Frame();
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    m_MsgRunning = false;
    M_Frame();
    CHECK(g_Player.shots.bullets[0].state == PLAYER_BULLET_FLYING);
    M_TearDown();
}

TEST(the_update_resets_the_targets)
{
    M_SetUpAlive("targets", 0);
    g_Player.homing_target = M_At(1.0f, 2.0f);
    g_Player.aim_target = M_At(3.0f, 4.0f);
    g_Player.target_found = true;
    M_Frame();
    CHECK(g_Player.homing_target.x == -999.0f);
    CHECK(g_Player.aim_target.y == -999.0f);
    CHECK(!g_Player.target_found);
    M_TearDown();
}

TEST(the_player_under_the_summary_hides_it)
{
    M_SetUpAlive("summary", 0);
    g_Player.pos = M_At(100.0f, 410.0f);
    M_Frame();
    CHECK_EQ_INT(g_AsciiText.summary_interrupt, 2);
    g_Player.pos = M_At(200.0f, 410.0f);
    M_Frame();
    CHECK_EQ_INT(g_AsciiText.summary_interrupt, 3);
    g_Player.pos = M_At(100.0f, 410.0f);
    M_Frame();
    g_Player.pos = M_At(100.0f, 300.0f);
    M_Frame();
    CHECK_EQ_INT(g_AsciiText.summary_interrupt, 3);
    M_TearDown();
}

// The hit tests

TEST(a_bullet_on_the_hitbox_kills_and_marks_the_hit)
{
    M_SetUpAlive("hit", 0);
    RNG expected = g_Rng;
    for (int32_t i = 0; i < 5; i++) {
        Rng_U32InRange(&expected, 100000);
    }
    for (int32_t i = 0; i < 3; i++) {
        Rng_F32(&expected);
    }
    Rng_U32InRange(&expected, 100000);
    Rng_U32InRange(&expected, 100000);
    CHECK(
        Player_CheckHit(M_At(M_START_X + 2.0f, M_START_Y), m_Small)
        == PLAYER_CONTACT_BOX);
    CHECK(g_Player.state.value == PLAYER_DYING);
    CHECK_EQ_INT(g_Player.state.timer.current, 0);
    CHECK(m_Events == REPLAY_EVENT_HIT);
    CHECK_EQ_INT(g_Rng.generated, expected.generated);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    CHECK(m_ReservedTypes[3] == EFFECT_SHOCKWAVE);
    CHECK(M_Queued(SOUND_PLAYER_DEATH));
    CHECK_EQ_INT(g_Player.clear_item, ITEM_STAR);
    M_TearDown();
}

TEST(a_bullet_past_the_hitbox_misses)
{
    M_SetUpAlive("miss", 0);
    CHECK(
        Player_CheckHit(M_At(M_START_X + 2.6f, M_START_Y), m_Small)
        == PLAYER_CONTACT_NONE);
    CHECK(g_Player.state.value == PLAYER_ALIVE);
    CHECK(m_Events == 0);
    M_TearDown();
}

TEST(a_hit_touches_in_every_state_but_kills_only_a_live_player)
{
    M_SetUpAlive("hit_states", 0);
    g_Player.state.value = PLAYER_INVULNERABLE;
    const int32_t generated = g_Rng.generated;
    CHECK(
        Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small)
        == PLAYER_CONTACT_BOX);
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK(m_Events == REPLAY_EVENT_HIT);
    CHECK_EQ_INT(g_Rng.generated, generated);
    M_TearDown();
}

TEST(a_hit_in_the_border_breaks_it)
{
    M_SetUpAlive("hit_border", 0);
    Player_StartBorder();
    m_Events = 0;
    g_GameManager.cherry.plus = 5000;
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.bonus = 100;
    CHECK(
        Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small)
        == PLAYER_CONTACT_BOX);
    CHECK(m_Events == (REPLAY_EVENT_HIT | REPLAY_EVENT_BORDER_BROKEN));
    CHECK(g_Player.border_state == PLAYER_BORDER_NONE);
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_Player.state.timer.current, 40);
    CHECK_EQ_INT(g_Player.state.bomb_cooldown, 40);
    CHECK_EQ_INT(g_GameManager.cherry.plus, 1000);
    CHECK(!g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 0);
    const PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[0];
    CHECK(area->radius == 32.0f && area->radius_growth == 16.0f);
    CHECK_EQ_INT(area->frames_left, 50);
    CHECK_EQ_INT(area->item, ITEM_BORDER_STAR);
    CHECK_EQ_INT(M_CountEffects(EFFECT_DIVERGE), 32);
    CHECK(m_Effects[0].type == EFFECT_DIVERGE);
    const EFFECT *const effect = &m_Reserved[4];
    CHECK(g_Player.effects.border == effect);
    CHECK_EQ_INT(effect->vm.interp_length[ANM_INTERP_SCALE], 30);
    CHECK(effect->vm.scale_from.x == 0.0625f);
    CHECK(effect->vm.scale_to.y == 1.3f);
    CHECK_EQ_INT(effect->vm.interp_length[ANM_INTERP_ALPHA], 30);
    CHECK_EQ_INT(effect->vm.interp_ease[ANM_INTERP_ALPHA], ANM_EASE_IN2);
    CHECK(effect->vm.color_from >> 24 == 0xffu);
    CHECK(effect->vm.color_to >> 24 == 0u);
    CHECK(M_Queued(SOUND_SHOT_0) && M_Queued(SOUND_BONUS));
    M_TearDown();
}

TEST(a_clear_area_wins_over_the_hitbox_and_gives_its_item)
{
    M_SetUpAlive("clear_area", 0);
    Player_AddClearArea(M_At(M_START_X, M_START_Y), 10.0f, 10.0f, ITEM_BOMB);
    CHECK(
        Player_CheckHit(M_At(M_START_X + 5.5f, M_START_Y), m_Small)
        == PLAYER_CONTACT_CLEARED);
    CHECK_EQ_INT(g_Player.clear_item, ITEM_BOMB);
    CHECK(g_Player.state.value == PLAYER_ALIVE);
    CHECK(m_Events == 0);
    M_TearDown();
}

TEST(a_clear_circle_tests_only_the_centre_of_the_box)
{
    M_SetUpAlive("clear_circle", 0);
    Player_AddClearCircle(
        M_At(100.0f, 100.0f), 10.0f, 0.0f, 5, ITEM_FULL_POWER);
    const VEC3 large = { 8.0f, 8.0f, 0.0f };
    CHECK(
        Player_CheckGraze(M_At(111.0f, 100.0f), large) == PLAYER_CONTACT_NONE);
    CHECK_EQ_INT(g_Player.clear_item, ITEM_STAR);
    CHECK(
        Player_CheckGraze(M_At(109.9f, 100.0f), m_Small)
        == PLAYER_CONTACT_CLEARED);
    CHECK_EQ_INT(g_Player.clear_item, ITEM_FULL_POWER);
    M_TearDown();
}

TEST(a_clear_circle_grows_and_is_freed_after_its_frames)
{
    M_SetUpAlive("circle_frames", 0);
    PLAYER_CLEAR_AREA *const area = Player_AddClearCircle(
        M_At(100.0f, 100.0f), 10.0f, 2.0f, 2, ITEM_FULL_POWER);
    M_Frame();
    CHECK(area->radius == 12.0f);
    M_Frame();
    CHECK(area->radius == 14.0f);
    M_Frame();
    CHECK(area->radius == 0.0f);
    Player_AddClearArea(M_At(1.0f, 1.0f), 4.0f, 4.0f, ITEM_POWER);
    M_Frame();
    CHECK(area->width == 0.0f);
    M_TearDown();
}

TEST(the_clear_areas_take_the_first_free_else_the_last)
{
    M_SetUpAlive("clear_slots", 0);
    PLAYER_CLEAR_AREA *const first =
        Player_AddClearArea(M_At(1.0f, 1.0f), 4.0f, 4.0f, ITEM_POWER);
    CHECK(first == &g_Player.clear_areas[0]);
    CHECK(
        Player_AddClearCircle(M_At(1.0f, 1.0f), 4.0f, 0.0f, 9, ITEM_POWER)
        == &g_Player.clear_areas[1]);
    first->width = 0.0f;
    CHECK(
        Player_AddClearArea(M_At(1.0f, 1.0f), 4.0f, 4.0f, ITEM_POWER)
        == &g_Player.clear_areas[0]);
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        g_Player.clear_areas[i].width = 1.0f;
    }
    CHECK(
        Player_AddClearArea(M_At(1.0f, 1.0f), 4.0f, 4.0f, ITEM_POWER)
        == &g_Player.clear_areas[PLAYER_CLEAR_AREA_COUNT - 1]);
    M_TearDown();
}

TEST(a_graze_reaches_20_past_the_bullet_to_the_graze_box)
{
    M_SetUpAlive("graze", 0);
    CHECK(
        Player_CheckGraze(M_At(M_START_X + 32.9f, M_START_Y), m_Small)
        == PLAYER_CONTACT_BOX);
    CHECK_EQ_INT(g_GameManager.player.graze, 1);
    CHECK_EQ_INT(g_GameManager.player.graze_total, 1);
    CHECK_EQ_INT(g_GameManager.score.value, 200);
    CHECK_EQ_INT(g_GameManager.rank.sub, 6);
    CHECK(M_Queued(SOUND_GRAZE));
    CHECK(m_Effects[0].type == M_EFFECT_GRAZE);
    CHECK_EQ_INT(m_Effects[0].count, 1);
    CHECK(m_Effects[0].pos.x == (M_START_X + (M_START_X + 32.9f)) * 0.5f);
    CHECK(
        Player_CheckGraze(M_At(M_START_X + 33.1f, M_START_Y), m_Small)
        == PLAYER_CONTACT_NONE);
    M_TearDown();
}

TEST(a_graze_adds_to_the_graze_bonus_by_the_cherry)
{
    M_SetUpAlive("graze_bonus", 0);
    g_GameManager.cherry.value = 1000 + 3100;
    Player_CheckGraze(M_At(M_START_X, M_START_Y), m_Small);
    CHECK_EQ_INT(g_Enemies.spell_card.graze_bonus, 2500 + 2 * 20);
    M_TearDown();
}

TEST(a_graze_in_the_border_adds_to_the_cherry)
{
    M_SetUpAlive("graze_border", 0);
    Player_StartBorder();
    Player_CheckGraze(M_At(M_START_X, M_START_Y), m_Small);
    CHECK_EQ_INT(g_GameManager.cherry.value, 1000 + 80);
    CHECK_EQ_INT(g_GameManager.cherry.max, 1000 + 100000 + 80);
    CHECK_EQ_INT(m_Effects[0].count, 3);
    CHECK(m_Effects[0].color == COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
    g_Player.move.focused = true;
    Player_CheckGraze(M_At(M_START_X, M_START_Y), m_Small);
    CHECK_EQ_INT(g_GameManager.cherry.value, 1000 + 110);
    CHECK_EQ_INT(m_Effects[1].count, 1);
    M_TearDown();
}

TEST(a_graze_during_a_bomb_scores_but_does_not_count)
{
    M_SetUpAlive("graze_bomb", 0);
    g_Player.bomb.running = true;
    Player_CheckGraze(M_At(M_START_X, M_START_Y), m_Small);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    CHECK_EQ_INT(g_GameManager.score.value, 200);
    M_TearDown();
}

TEST(no_graze_while_dying_or_respawning)
{
    M_SetUpAlive("graze_dying", 0);
    g_Player.state.value = PLAYER_DYING;
    CHECK(
        Player_CheckGraze(M_At(M_START_X, M_START_Y), m_Small)
        == PLAYER_CONTACT_NONE);
    g_Player.state.value = PLAYER_RESPAWNING;
    CHECK(
        Player_CheckGraze(M_At(M_START_X, M_START_Y), m_Small)
        == PLAYER_CONTACT_NONE);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    M_TearDown();
}

TEST(a_laser_test_turns_the_player_into_the_laser)
{
    M_SetUpAlive("laser", 0);
    const VEC3 origin = M_At(M_START_X, M_START_Y - 100.0f);
    const VEC3 size = { 4.0f, 200.0f, 0.0f };
    const VEC3 pos = M_At(M_START_X + 100.0f, M_START_Y - 100.0f);
    Player_CheckLaserHit(pos, size, origin, -MATH_HALF_PI, false);
    CHECK(g_Player.state.value == PLAYER_ALIVE);
    Player_CheckLaserHit(pos, size, origin, MATH_HALF_PI, false);
    CHECK(g_Player.state.value == PLAYER_DYING);
    CHECK(m_Events == REPLAY_EVENT_HIT);
    M_TearDown();
}

TEST(a_laser_grazes_within_48_and_breaks_the_border)
{
    M_SetUpAlive("laser_graze", 0);
    const VEC3 size = { 4.0f, 4.0f, 0.0f };
    Player_CheckLaserHit(
        M_At(M_START_X + 51.0f, M_START_Y), size, M_At(0.0f, 0.0f), 0.0f,
        false);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    Player_CheckLaserHit(
        M_At(M_START_X + 51.0f, M_START_Y), size, M_At(0.0f, 0.0f), 0.0f, true);
    CHECK_EQ_INT(g_GameManager.player.graze, 1);
    Player_CheckLaserHit(
        M_At(M_START_X + 52.0f, M_START_Y), size, M_At(0.0f, 0.0f), 0.0f, true);
    CHECK_EQ_INT(g_GameManager.player.graze, 1);
    Player_StartBorder();
    Player_CheckLaserHit(
        M_At(M_START_X, M_START_Y), size, M_At(0.0f, 0.0f), 0.0f, true);
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK(m_Events & REPLAY_EVENT_BORDER_BROKEN);
    M_TearDown();
}

TEST(items_touch_the_collect_box_unless_dying_or_respawning)
{
    M_SetUpAlive("collect", 0);
    const VEC3 pos = M_At(M_START_X + 12.9f, M_START_Y);
    CHECK(Player_TouchesCollectBox(pos, m_Small));
    CHECK(
        !Player_TouchesCollectBox(M_At(M_START_X + 13.1f, M_START_Y), m_Small));
    g_Player.state.value = PLAYER_IN_BORDER;
    CHECK(Player_TouchesCollectBox(pos, m_Small));
    g_Player.state.value = PLAYER_INVULNERABLE;
    CHECK(Player_TouchesCollectBox(pos, m_Small));
    g_Player.state.value = PLAYER_DYING;
    CHECK(!Player_TouchesCollectBox(pos, m_Small));
    g_Player.state.value = PLAYER_RESPAWNING;
    CHECK(!Player_TouchesCollectBox(pos, m_Small));
    M_TearDown();
}

TEST(the_angle_points_from_a_position_to_the_player)
{
    M_SetUpAlive("angle", 0);
    CHECK(Player_AngleFrom(M_At(M_START_X, M_START_Y)) == MATH_HALF_PI);
    CHECK(
        Player_AngleFrom(M_At(M_START_X - 10.0f, M_START_Y - 10.0f))
        == X87_Atan2(10.0f, 10.0f));
    M_TearDown();
}

// The bomb

TEST(the_bomb_button_starts_a_bomb)
{
    M_SetUpAlive("bomb", 0);
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.bonus = 500;
    g_Player.move.focused = true;
    M_Hold(M_BOMB);
    M_Frame();
    CHECK(g_Player.bomb.running);
    CHECK(g_Player.bomb.focused);
    CHECK(g_Player.state.bomb_started);
    CHECK_EQ_INT(g_Player.bomb.duration, 999);
    CHECK_EQ_INT(m_BombCalls, 1);
    CHECK(g_GameManager.player.bombs == 2.0f);
    CHECK(g_GameManager.stats.bombs_used == 1.0f);
    CHECK(m_Events == REPLAY_EVENT_BOMB);
    CHECK(g_Enemies.spell_card.bombed);
    CHECK(!g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 0);
    CHECK_EQ_INT(g_GameManager.rank.value, 18);
    // The bomb clears bomb_started; a running one keeps it.
    M_Frame();
    CHECK(g_Player.state.bomb_started);
    CHECK_EQ_INT(m_BombCalls, 2);
    M_TearDown();
}

TEST(a_running_bomb_drains_the_cherry_on_its_new_frames)
{
    M_SetUpAlive("bomb_drain", 0);
    g_Player.bomb.running = true;
    g_Player.bomb.drain = 7;
    g_Player.bomb.timer = (TIMER) { .previous = 5, .current = 5 };
    g_GameManager.cherry.value = 1100;
    M_Frame();
    CHECK_EQ_INT(g_GameManager.cherry.value, 1100);
    g_Player.bomb.timer = (TIMER) { .previous = 5, .current = 6 };
    M_Frame();
    CHECK_EQ_INT(g_GameManager.cherry.value, 1093);
    CHECK_EQ_INT(m_BombCalls, 2);
    M_TearDown();
}

TEST(no_bomb_without_bombs_during_a_dialogue_or_the_cooldown)
{
    M_SetUpAlive("no_bomb", 0);
    M_Hold(M_BOMB);
    m_MsgRunning = true;
    M_Frame();
    m_MsgRunning = false;
    g_Player.state.bomb_cooldown = 2;
    M_Frame();
    CHECK_EQ_INT(g_Player.state.bomb_cooldown, 1);
    g_GameManager.player.bombs = 0.9f;
    g_Player.state.bomb_cooldown = 0;
    M_Frame();
    g_GameManager.player.bombs = 3.0f;
    g_Player.state.bomb_window = 0;
    M_Frame();
    CHECK(!g_Player.bomb.running);
    CHECK_EQ_INT(m_BombCalls, 0);
    M_TearDown();
}

TEST(a_bomb_in_the_window_after_a_hit_extends_the_window)
{
    M_SetUpAlive("deathbomb", 0);
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    M_Frames(3);
    CHECK_EQ_INT(g_Player.state.bomb_window, 5);
    M_Hold(M_BOMB);
    M_Frame();
    CHECK(g_Player.bomb.running);
    CHECK_EQ_INT(g_Player.state.bomb_window, 7);
    M_TearDown();
}

TEST(the_bomb_button_in_the_border_breaks_it_and_collects_the_items)
{
    M_SetUpAlive("bomb_border", 0);
    Player_StartBorder();
    m_Events = 0;
    M_Hold(M_BOMB);
    M_Frame();
    CHECK(g_Player.border_state == PLAYER_BORDER_NONE);
    CHECK_EQ_INT(m_CollectCalls, 1);
    CHECK(!g_Player.bomb.running);
    CHECK(m_Events == REPLAY_EVENT_BORDER_BROKEN);
    CHECK_EQ_INT(g_Player.state.bomb_cooldown, 40);
    M_TearDown();
}

// Dying and respawning

TEST(the_life_goes_when_the_window_closes)
{
    M_SetUpAlive("death", 0);
    g_GameManager.player.power = 40.0f;
    g_GameManager.player.power_item_combo = 9;
    g_GameManager.cherry.value = 1000 + 12345;
    g_Enemies.spell_card.capturable = true;
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    g_Player.pos = M_At(50.0f, 60.0f);
    M_Frames(7);
    CHECK_EQ_INT(m_ItemCount, 0);
    M_Frame();
    CHECK(m_Events == (REPLAY_EVENT_HIT | REPLAY_EVENT_DEATH));
    CHECK(g_GameManager.stats.deaths == 1.0f);
    CHECK(g_GameManager.player.power == 24.0f);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 0);
    CHECK(!g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(m_ItemCount, 6);
    CHECK_EQ_INT(m_Items[0].type, ITEM_BIG_POWER);
    CHECK_EQ_INT(m_Items[0].state, ITEM_THROWN);
    CHECK_EQ_INT(m_Items[5].type, ITEM_POWER);
    CHECK(m_Items[5].pos.x == 50.0f);
    // A quarter of 12345 is 3086, rounded down to 3080.
    CHECK_EQ_INT(g_GameManager.cherry.value, 1000 + 12345 - 3080);
    CHECK_EQ_INT(m_DropCalls, 1);
    CHECK_EQ_INT(g_GameManager.rank.value, 4);
    M_TearDown();
}

TEST(a_death_takes_at_most_100000_of_the_cherry_or_60000_for_sakuya)
{
    for (uint8_t shot = 3; shot <= 4; shot++) {
        M_SetUpAlive("death_cap", shot);
        g_GameManager.cherry.max = 10000000;
        g_GameManager.cherry.value = 1000 + 800000;
        Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
        M_Frames(8);
        const int32_t cap = shot == 4 ? 60000 : 100000;
        CHECK_EQ_INT(g_GameManager.cherry.value, 1000 + 800000 - cap);
        M_TearDown();
    }
}

TEST(a_death_at_16_power_or_less_takes_all_of_it)
{
    M_SetUpAlive("death_power", 0);
    g_GameManager.player.power = 16.9f;
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    M_Frames(8);
    CHECK(g_GameManager.player.power == 0.0f);
    M_TearDown();
}

TEST(the_last_life_drops_full_power_items)
{
    M_SetUpAlive("last_life", 0);
    g_GameManager.player.lives = 0.0f;
    g_GameManager.player.power = 100.0f;
    const int32_t cherry = g_GameManager.cherry.value;
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    M_Frames(8);
    CHECK_EQ_INT(m_ItemCount, 5);
    CHECK_EQ_INT(m_Items[4].type, ITEM_FULL_POWER);
    CHECK(g_GameManager.player.power == 0.0f);
    CHECK_EQ_INT(g_GameManager.cherry.value, cherry);
    CHECK_EQ_INT(m_DropCalls, 0);
    M_TearDown();
}

TEST(the_player_shrinks_away_then_respawns_with_its_bombs)
{
    M_SetUpAlive("respawn", 0);
    g_GameManager.player.bombs = 0.0f;
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    g_Player.pos = M_At(50.0f, 60.0f);
    // The state timer counts through the bomb window too.
    M_Frames(15);
    CHECK_EQ_INT(g_Player.state.timer.current, 15);
    CHECK(g_Player.vm.scale_y == 3.0f * (14.0f / 30.0f) + 1.0f);
    CHECK(
        g_Player.vm.color
        == ((uint32_t)X87_Ftol(255.0f - 14.0f * 255.0f / 30.0f) << 24
            | 0xffffffu));
    CHECK(g_Player.vm.blend_add);
    M_Frames(16);
    CHECK(g_Player.state.value == PLAYER_RESPAWNING);
    CHECK(g_Player.pos.x == M_START_X && g_Player.pos.y == M_START_Y);
    CHECK(g_GameManager.player.lives == 1.0f);
    CHECK(g_GameManager.player.bombs == 3.0f);
    CHECK(g_Player.vm.scale_x == 0.0f);
    CHECK_EQ_INT(g_Player.state.respawn_clear_frames, 59);
    M_Frames(30);
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_Player.state.bomb_window, 8);
    CHECK(!g_Player.vm.blend_add);
    M_TearDown();
}

TEST(no_life_left_opens_the_continue_menu)
{
    M_SetUpAlive("continue", 0);
    g_GameManager.player.lives = 0.0f;
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    M_Frames(31);
    CHECK(g_GameManager.mode.in_continue_menu);
    CHECK(g_Player.state.value == PLAYER_RESPAWNING);
    CHECK(g_Player.pos.y == M_START_Y);
    M_TearDown();
}

// The Border

TEST(the_border_starts_for_540_frames)
{
    M_SetUpAlive("border", 0);
    Player_StartBorder();
    CHECK(g_Player.border_state == PLAYER_BORDER_RUNNING);
    CHECK(g_Player.state.value == PLAYER_IN_BORDER);
    CHECK_EQ_INT(g_Player.state.timer.current, 540);
    CHECK_EQ_INT(g_Player.state.border_length.current, 540);
    CHECK(m_Events == REPLAY_EVENT_BORDER);
    CHECK_EQ_INT(m_StartPopups, 1);
    CHECK(M_Queued(SOUND_BORDER) && M_Queued(SOUND_BONUS_2));
    CHECK(m_ReservedTypes[4] == EFFECT_BORDER);
    const ANM_VM *const vm = &m_Reserved[4].vm;
    CHECK_EQ_INT(vm->interp_length[ANM_INTERP_SCALE], 540);
    CHECK_EQ_INT(vm->interp_time[ANM_INTERP_SCALE].current, 0);
    CHECK(vm->scale_from.x == 1.0f && vm->scale_to.x == 0.25f);
    CHECK_EQ_INT(vm->int_vars[0], 540);
    CHECK(vm->rotation_speed.z == -0.5f);
    M_TearDown();
}

TEST(the_border_waits_while_the_player_cannot_take_it)
{
    M_SetUpAlive("border_pending", 0);
    g_Player.state.value = PLAYER_INVULNERABLE;
    Timer_Set(&g_Player.state.timer, 2);
    Player_StartBorder();
    CHECK(g_Player.border_state == PLAYER_BORDER_PENDING);
    CHECK(m_Events == 0);
    M_Frame();
    CHECK(g_Player.border_state == PLAYER_BORDER_PENDING);
    M_Frame();
    CHECK(g_Player.state.value == PLAYER_ALIVE);
    M_Frame();
    CHECK(g_Player.border_state == PLAYER_BORDER_RUNNING);
    CHECK(g_Player.state.value == PLAYER_IN_BORDER);
    M_TearDown();
}

TEST(the_border_waits_during_a_bomb_or_a_dialogue)
{
    M_SetUpAlive("border_bomb", 0);
    g_Player.bomb.running = true;
    Player_StartBorder();
    CHECK(g_Player.border_state == PLAYER_BORDER_PENDING);
    g_Player.bomb.running = false;
    m_MsgRunning = true;
    Player_StartBorder();
    CHECK(g_Player.border_state == PLAYER_BORDER_PENDING);
    CHECK(g_Player.state.value == PLAYER_ALIVE);
    M_TearDown();
}

TEST(a_border_in_the_bomb_window_breaks_at_once_and_saves_the_player)
{
    M_SetUpAlive("border_window", 0);
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    M_Frame();
    m_Events = 0;
    Player_StartBorder();
    CHECK(m_Events == REPLAY_EVENT_BORDER_BROKEN);
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK(g_Player.border_state == PLAYER_BORDER_NONE);
    M_TearDown();
}

TEST(a_border_past_the_bomb_window_waits_for_the_respawn)
{
    M_SetUpAlive("border_dying", 0);
    Player_CheckHit(M_At(M_START_X, M_START_Y), m_Small);
    M_Frames(8);
    Player_StartBorder();
    CHECK(g_Player.border_state == PLAYER_BORDER_PENDING);
    CHECK(g_Player.state.value == PLAYER_DYING);
    M_TearDown();
}

TEST(the_border_runs_the_plus_down_and_tints_the_stage)
{
    M_SetUpAlive("border_plus", 0);
    Player_StartBorder();
    M_Frame();
    CHECK_EQ_INT(g_GameManager.cherry.plus, 1000 + 50000);
    CHECK_EQ_INT(g_Player.state.timer.current, 539);
    CHECK(m_Tint == COLOR_ARGB(0x80, 0x7e, 0x7e, 0x7e));
    CHECK(g_Player.vm.color == COLOR_WHITE);
    M_Frames(2);
    CHECK(g_Player.vm.color == COLOR_ARGB(0xff, 0xff, 0x00, 0x00));
    M_Frames(98);
    CHECK_EQ_INT(g_GameManager.cherry.plus, 1000 + 440 * 50000 / 540);
    CHECK(m_Tint == COLOR_ARGB(0x80, 0x30, 0x30, 0x30));
    CHECK(g_Player.vm.color == COLOR_WHITE);
    M_Frames(420);
    CHECK_EQ_INT(g_Player.state.timer.current, 19);
    CHECK(m_Tint == (uint32_t)(0x80 - 19 * 80 / 30) * 0x10101u + 0x80000000u);
    M_TearDown();
}

TEST(the_end_of_the_border_awards_the_cherry)
{
    M_SetUpAlive("border_end", 0);
    g_GameManager.cherry.value = 1000 + 2000;
    Player_StartBorder();
    M_Frames(540);
    CHECK(g_Player.border_state == PLAYER_BORDER_NONE);
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_Player.state.timer.current, 40);
    CHECK_EQ_INT(g_Player.state.bomb_cooldown, 40);
    CHECK_EQ_INT(g_GameManager.cherry.value, 1000 + 12000);
    CHECK_EQ_INT(g_GameManager.cherry.max, 1000 + 110000);
    CHECK_EQ_INT(m_BonusPopup, 120000);
    CHECK_EQ_INT(g_GameManager.score.value, 12000);
    CHECK_EQ_INT(g_GameManager.cherry.plus, 1000);
    CHECK(g_Player.effects.border == nullptr);
    CHECK(!m_Reserved[4].active);
    CHECK(M_Queued(SOUND_BONUS));
    M_TearDown();
}

TEST(the_end_of_the_border_awards_even_while_pending)
{
    M_SetUpAlive("border_end_pending", 0);
    g_Player.state.value = PLAYER_RESPAWNING;
    g_Player.state.bomb_window = 0;
    Player_StartBorder();
    Player_EndBorder();
    CHECK_EQ_INT(g_GameManager.cherry.value, 1000 + 10000);
    CHECK(g_Player.state.value == PLAYER_INVULNERABLE);
    CHECK(g_Player.border_state == PLAYER_BORDER_NONE);
    CHECK_EQ_INT(g_Player.state.bomb_window, 8);
    M_TearDown();
}

// The draw

TEST(the_draw_places_the_player_and_runs_the_bomb_draw)
{
    M_SetUpAlive("draw", 0);
    g_GameManager.play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    Chain_RunDraw(&g_Chain);
    CHECK(g_Player.vm.pos.x == 32.0f + M_START_X);
    CHECK(g_Player.option_vms[0].pos.y == 16.0f + M_START_Y);
    CHECK_EQ_INT(m_BombDraws, 0);
    g_Player.bomb.running = true;
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(m_BombDraws, 1);
    M_TearDown();
}

TEST(the_draw_blinks_and_tints_through_the_border)
{
    M_SetUpAlive("draw_border", 0);
    Player_StartBorder();
    Timer_Set(&g_Player.state.timer, 300);
    Chain_RunDraw(&g_Chain);
    CHECK(m_Tint == COLOR_ARGB(0x80, 0x30, 0x30, 0x30));
    CHECK(g_Player.vm.color == COLOR_ARGB(0xff, 0xff, 0x00, 0x00));
    M_TearDown();
}
