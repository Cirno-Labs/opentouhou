#include "th07_play_setup.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/bullet.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include "stg/chain.h"
#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_SPRITE_OP 3
#define M_DELETE_OP 1

RNG g_Rng;
CHAIN g_Chain;
float g_GameSpeed;
PLAYER g_Player;
GUI g_Gui;
ASCII_TEXT g_AsciiText;
uint8_t g_EffectsQuality = 2;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;

PLAYER_CONTACT Player_CheckGraze(const VEC3 pos, const VEC3 size)
{
    return PLAYER_CONTACT_NONE;
}

PLAYER_CONTACT Player_CheckHit(const VEC3 pos, const VEC3 size)
{
    return PLAYER_CONTACT_NONE;
}

void Player_CheckLaserHit(
    const VEC3 pos, const VEC3 size, const VEC3 origin, const float angle,
    const bool can_graze)
{
}

int32_t Player_CalcDamage(
    const VEC3 pos, const VEC3 size, bool *const hit_by_bomb)
{
    return 0;
}

void Player_StartBorder(void)
{
}

void Item_Reset(void)
{
}

void Item_Update(void)
{
}

void Item_Draw(void)
{
}

void AsciiText_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
}

void Replay_MarkEvent(const REPLAY_EVENT event)
{
}

SCENE Scene_GetRequested(void)
{
    return SCENE_GAMEPLAY;
}

void Gui_StartMsg(const int32_t index)
{
}

bool Gui_MsgHoldsTimeline(void)
{
    return false;
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
}

bool Gui_IsLoadingShown(void)
{
    return false;
}

static void M_AddScript(
    ANM_BUILDER_ANM *const anm, const int32_t script, const int32_t sprite,
    const int16_t end)
{
    const int32_t index =
        AnmBuilder_BeginScript(anm, script - PLAY_SETUP_SPRITE_8);
    const uint32_t arg = (uint32_t)(sprite - PLAY_SETUP_SPRITE_8);
    AnmBuilder_Instr(anm, index, M_SPRITE_OP, 0, 0, &arg, 1);
    AnmBuilder_Instr(anm, index, M_DELETE_OP, end, 0, nullptr, 0);
    AnmBuilder_End(anm, index);
}

// Writes etama.anm as th07/bullet's test does.
static void M_WriteEtama(void)
{
    static ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 256, 256, "@");
    for (int32_t i = 0; i < 16; i++) {
        AnmBuilder_AddSprite(anm, i, 0.0f, 0.0f, 8.0f, 8.0f);
        AnmBuilder_AddSprite(anm, 16 + i, 0.0f, 0.0f, 16.0f, 16.0f);
        AnmBuilder_AddSprite(anm, 32 + i, 0.0f, 0.0f, 32.0f, 32.0f);
        AnmBuilder_AddSprite(anm, 48 + i, 0.0f, 0.0f, 64.0f, 64.0f);
    }
    M_AddScript(anm, 512, PLAY_SETUP_SPRITE_8, 1000);
    for (int32_t i = 513; i <= 518; i++) {
        M_AddScript(anm, i, PLAY_SETUP_SPRITE_16, 1000);
    }
    for (int32_t i = 519; i <= 521; i++) {
        M_AddScript(anm, i, PLAY_SETUP_SPRITE_32, 1000);
    }
    M_AddScript(anm, 680, PLAY_SETUP_SPRITE_64, 1000);
    M_AddScript(anm, 527, PLAY_SETUP_SPRITE_8, 4);
    M_AddScript(anm, 528, PLAY_SETUP_SPRITE_16, 4);
    M_AddScript(anm, 529, PLAY_SETUP_SPRITE_32, 4);
    M_AddScript(anm, 681, PLAY_SETUP_SPRITE_64, 4);
    for (int32_t i = 0; i < 3; i++) {
        M_AddScript(anm, 530 + i, PLAY_SETUP_SPRITE_8, (int16_t)(2 + i));
        M_AddScript(anm, 533 + i, PLAY_SETUP_SPRITE_16, (int16_t)(2 + i));
    }
    M_AddScript(anm, 536, PLAY_SETUP_SPRITE_32, 3);
    M_AddScript(anm, 682, PLAY_SETUP_SPRITE_64, 3);
    AnmBuilder_WriteFile(&builder, m_Directory, "data/etama.anm");
}

static void M_WriteOneSprite(const char *const name)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    AnmBuilder_End(anm, AnmBuilder_BeginScript(anm, 0));
    AnmBuilder_WriteFile(&builder, m_Directory, name);
}

const char *PlaySetup_SetUp(const char *const case_name, const uint16_t seed)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    ASSERT(SDL_CreateDirectory(data));
    Mem_Free(data);
    M_WriteEtama();
    M_WriteOneSprite("data/eff01.anm");
    M_WriteOneSprite("data/stg1enm.anm");
    ASSERT(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, seed);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    g_GameManager.run.difficulty = 1;
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Stage, 0, sizeof(g_Stage));
    memset(&g_Gui, 0, sizeof(g_Gui));
    memset(&g_AsciiText, 0, sizeof(g_AsciiText));
    g_GameSpeed = 1.0f;
    ASSERT(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
    ASSERT(Result_Log(Effect_Register(), LOG_LEVEL_ERROR));
    ASSERT(Result_Log(Enemy_Register(), LOG_LEVEL_ERROR));
    for (int32_t i = 0x2ab; i <= 0x304; i++) {
        Anm_SetScriptEntry(i, Anm_GetScript(0x2dc));
    }
    Sound_Reset();
    return m_Directory;
}

void PlaySetup_TearDown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}
