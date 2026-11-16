#include "th07_gui_setup.h"

#include "gfx_recorder.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "th07/anm.h"
#include "th07/bullet.h"
#include "th07/buttons.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/screen_effect.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/screens/result/records.h"
#include "th07/stubs.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include "stg/chain.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_SPRITE_OP 3
#define M_STOP_OP 20
#define M_LABEL_OP 21

CHAIN g_Chain;
RNG g_Rng;
float g_GameSpeed;
BUTTONS g_Buttons;
PLAYER g_Player;
STAGE g_Stage;
BULLET_MANAGER g_Bullets;
ENEMY_MANAGER g_Enemies;
GUI_CALLS g_GuiCalls;
GUI_FAKE g_GuiFake;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;

void Bullet_ClearAllToItems(void)
{
    g_GuiCalls.bullets_cleared++;
}

int32_t Enemy_ClearForBonus(const int32_t max_bonus, const int32_t start)
{
    g_GuiCalls.enemies_cleared++;
    g_GuiCalls.enemy_max_bonus = max_bonus;
    g_GuiCalls.enemy_start = start;
    return 0;
}

void Item_CollectAll(void)
{
    g_GuiCalls.items_collected++;
}

void Player_EndBorder(void)
{
    g_GuiCalls.borders_ended++;
}

bool Replay_HasStage(const int32_t stage)
{
    return g_GameManager.mode.in_replay && g_GuiFake.replay_stages[stage];
}

void Replay_ResaveIfScoreChanged(void)
{
    g_GuiCalls.resaves++;
}

void ResultScreen_GetSpellCardRecord(
    const int32_t index, int32_t *const captures, int32_t *const attempts)
{
    *captures = g_GuiFake.spell_captures;
    *attempts = g_GuiFake.spell_attempts;
}

void ScreenEffect_FadeOutScreen(const int32_t duration, const uint32_t color)
{
    g_GuiCalls.screen_fades++;
    g_GuiCalls.screen_fade_duration = duration;
    g_GuiCalls.screen_fade_color = color;
}

void Scene_Request(const SCENE scene)
{
    g_GuiCalls.scene_requests++;
    g_GuiCalls.requested = scene;
}

SCENE Scene_GetRequested(void)
{
    return g_GuiFake.scene;
}

static void M_ClearCalls(void)
{
    memset(&g_GuiCalls, 0, sizeof(g_GuiCalls));
    g_GuiCalls.requested = SCENE_QUIT;
}

ANM_BUILDER_ANM *GuiSetup_AddAnm(ANM_BUILDER *const builder)
{
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(builder, 512, 64, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    AnmBuilder_AddSprite(anm, 1, 16.0f, 0.0f, 16.0f, 16.0f);
    AnmBuilder_AddSprite(anm, 2, 0.0f, 16.0f, 200.0f, 16.0f);
    AnmBuilder_AddSprite(anm, 3, 0.0f, 32.0f, 300.0f, 16.0f);
    AnmBuilder_AddSprite(anm, 4, 32.0f, 0.0f, 16.0f, 16.0f);
    AnmBuilder_AddSprite(anm, 5, 48.0f, 0.0f, 16.0f, 16.0f);
    return anm;
}

void GuiSetup_AddScript(ANM_BUILDER_ANM *const anm, const int32_t id)
{
    const int32_t script = AnmBuilder_BeginScript(anm, id);
    const uint32_t first = 0;
    const uint32_t second = 1;
    const uint32_t label = GUI_SETUP_LABEL;
    AnmBuilder_Instr(anm, script, M_SPRITE_OP, 0, 0, &first, 1);
    AnmBuilder_Instr(anm, script, M_STOP_OP, 0, 0, nullptr, 0);
    AnmBuilder_Instr(anm, script, M_LABEL_OP, 0, 0, &label, 1);
    AnmBuilder_Instr(anm, script, M_SPRITE_OP, 0, 0, &second, 1);
    AnmBuilder_Instr(anm, script, M_STOP_OP, 0, 0, nullptr, 0);
    AnmBuilder_End(anm, script);
}

void GuiSetup_WriteBuilder(
    const ANM_BUILDER *const builder, const char *const name)
{
    AnmBuilder_WriteFile(builder, m_Directory, name);
}

void GuiSetup_WriteAnm(
    const char *const name, const int32_t *const scripts, const int32_t count)
{
    static ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = GuiSetup_AddAnm(&builder);
    for (int32_t i = 0; i < count; i++) {
        GuiSetup_AddScript(anm, scripts[i]);
    }
    GuiSetup_WriteBuilder(&builder, name);
}

void GuiSetup_WriteStage(const int32_t stage, const MSG_BUILDER *const msg)
{
    char *const face = Str_Format("data/face_%02d_00.anm", stage);
    GuiSetup_WriteAnm(face, nullptr, 0);
    Mem_Free(face);
    // The titles from 0x800, and the song titles at 0x804 and 0x805.
    const int32_t titles[] = { 0, 1, 2, 3, 4, 5 };
    char *const text = Str_Format("data/std%dtxt.anm", stage);
    GuiSetup_WriteAnm(text, titles, 6);
    Mem_Free(text);
    static uint8_t bytes[MSG_BUILDER_MAX_SIZE];
    const size_t size = MsgBuilder_Build(msg, bytes);
    char *const name = Str_Format("data/msg%d.dat", stage);
    Th07Data_WriteFile(m_Directory, name, bytes, size);
    Mem_Free(name);
}

const char *GuiSetup_WriteData(
    const char *const case_name, const int32_t stage,
    const MSG_BUILDER *const msg)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    ASSERT(SDL_CreateDirectory(data));
    Mem_Free(data);
    // The lines and the nameplates, from 0x700.
    const int32_t front[] = { 0x100, 0x101, 0x102, 0x103 };
    GuiSetup_WriteAnm("data/front.anm", front, 4);
    // The icon at 0x61e and the snapshot at 0x725.
    const int32_t loading[] = { 0, 0x107 };
    GuiSetup_WriteAnm("data/loading.anm", loading, 2);
    // The player's portrait at 0x4a0 and the boss's at 0x4a2.
    const int32_t face[] = { 0, 2 };
    GuiSetup_WriteAnm("data/face_rm00.anm", face, 2);
    // Holds the caption of the loading screen, at 0x10c.
    GuiSetup_WriteAnm("data/caption.anm", nullptr, 0);
    GuiSetup_WriteStage(stage, msg);
    return m_Directory;
}

void GuiSetup_Start(const int32_t stage)
{
    GfxRecorder_Start();
    ASSERT(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    M_ClearCalls();
    memset(&g_GuiFake, 0, sizeof(g_GuiFake));
    g_GuiFake.scene = SCENE_GAMEPLAY;
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = stage;
    g_GameManager.run.difficulty = 1;
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Stage, 0, sizeof(g_Stage));
    memset(&g_Bullets, 0, sizeof(g_Bullets));
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    g_Bullets.clear_item = ITEM_STAR;
    g_GameSpeed = 1.0f;
    Sound_Reset();
}

RESULT GuiSetup_SetUpStage(
    const char *const case_name, const int32_t stage,
    const MSG_BUILDER *const msg)
{
    GuiSetup_WriteData(case_name, stage, msg);
    GuiSetup_Start(stage);
    return Gui_Register();
}

void GuiSetup_SetUp(
    const char *const case_name, const int32_t stage,
    const MSG_BUILDER *const msg)
{
    ASSERT(Result_Log(
        GuiSetup_SetUpStage(case_name, stage, msg), LOG_LEVEL_ERROR));
}

void GuiSetup_TearDown(void)
{
    Chain_Release(&g_Chain);
    // Drops a copy a test left pending; no texture is loaded in slot 4.
    ASSERT(IS_OK(Anm_DoPendingCopy()));
    Anm_Shutdown();
    Files_Shutdown();
    GfxRecorder_Stop();
    Mem_Free(m_Directory);
    m_Directory = nullptr;
}

void GuiSetup_Run(const int32_t frames)
{
    for (int32_t i = 0; i < frames; i++) {
        Chain_RunCalc(&g_Chain);
    }
}

void GuiSetup_Press(const uint16_t word)
{
    g_Buttons.previous_play_word = g_Buttons.play_word;
    g_Buttons.play_word = word;
}

void GuiSetup_StartMsg(const int32_t index)
{
    Gui_StartMsg(index);
    M_ClearCalls();
}
