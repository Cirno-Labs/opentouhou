#include "th01/stage.h"

#include "th01/boss.h"
#include "th01/gameplay/private.h"
#include "th01/hooks.h"
#include "th01/hud.h"
#include "th01/items.h"
#include "th01/orb.h"
#include "th01/palette.h"
#include "th01/pellets.h"
#include "th01/player.h"
#include "th01/scene.h"
#include "th01/session.h"
#include "th01/shots.h"
#include "th01/sound.h"
#include "th01/sprite.h"
#include "th01/stage/objects.h"
#include "th01/text.h"
#include "th01/timer.h"
#include "th01/tram.h"
#include "th01/video.h"
#include "th01/vsync_irq.h"

#include "core/base/assert.h"

#include <string.h>

typedef enum {
    GP_STAGE_BEGIN,
    GP_STAGE_BOSS_SETUP,
    GP_STAGE_FINAL_ENTRANCE,
    GP_STAGE_OBJECT_ENTRANCE,
    GP_STAGE_TITLE,
    GP_STAGE_LABEL,
    GP_STAGE_NUMBER,
} GAMEPLAY_STAGE_PHASE;

static GAMEPLAY_STAGE_PHASE m_StagePhase;

static void M_HookBossHp(void)
{
    Th01Hooks_CallOnBoss(&(*g_Boss)->hp);
}

static void M_BossSetup(const uint64_t host_tick)
{
    const BOSS_STRATEGY *const boss = Gameplay_Boss();
    if (boss == nullptr) {
        Boss_Deselect();
        return;
    }
    ROUTE_SELECT_ASSETS route;
    const BOSS_CONTEXT ctx = Gameplay_BossContext(&route, nullptr, host_tick);
    boss->setup(Gameplay_BossAssets(), &ctx);
    if (boss->resume_setup == nullptr) {
        M_HookBossHp();
    }
}

static void M_Line(const char *const text)
{
    const GAME_TEXT_GLYPH *glyphs[5];
    int32_t count = 0;
    for (; text[count] != '\0'; count++) {
        ASSERT(count < 5);
        glyphs[count] =
            GameText_Find(&g_GameplayAssets->menu_font, (uint8_t)text[count]);
    }
    Tram_PutLine16(glyphs, count, TRAM_RED, TRAM_BLACK);
    Tram_Fill((GFX_RECT) { 0, 336, 640, 64 }, TRAM_BLACK);
}

static void M_TitleChar(void)
{
    const uint32_t chars[] = { 0x6771, 0x65b9, 0x2605, 0x9748, 0x7570, 0x4f1d };
    Sound_Effect(SOUND_EFFECT_TITLE_TEXT);
    Tram_PutKanji16Reverse(GameText_Find(
        &g_GameplayAssets->menu_font, chars[g_Gameplay->animation_frame]));
}

static void M_StageBackground(void)
{
    const GAMEPLAY_ASSETS *const a = g_GameplayAssets;
    const BOSS_STRATEGY *const boss = Gameplay_Boss();
    const PI_IMAGE *const background =
        boss != nullptr && boss->background != nullptr
        ? boss->background(Gameplay_BossAssets())
        : &a->backgrounds[Session_Scene()];
    Video_SelectDrawingPage(0);
    if (g_Stage->first_stage) {
        memcpy(g_Video.pages[0], background->pixels, VIDEO_PAGE_SIZE);
        Palette_Import(&g_StagePalette, background->palette);
        Palette_Set(&g_Palette, &g_StagePalette);
        Gameplay_CopyPage(0, 1);
    } else {
        Gameplay_CopyPage(1, 0);
        Sprite_PtnPut(&a->miko, 0, -1, g_Player->left, PLAYER_TOP);
    }
    Scene_BeginStage(
        g_Session.stage_id, g_Stage->first_stage, Gameplay_StageSprites());
    m_StagePhase = GP_STAGE_OBJECT_ENTRANCE;
}

static void M_StageBegin(const uint64_t host_tick)
{
    Pellets_EraseAndResetMoving();
    Shots_EraseAndReset();
    const int16_t stage = g_Session.stage_id;
    p_Gameplay->boss_stage = stage % 5 == 4;
    Stage_SetBoss(
        stage == 4       ? BOSS_SINGYOKU
            : stage == 9 ? (g_Session.route == 0 ? BOSS_YUUGENMAGAN : BOSS_MIMA)
            : stage == 14 ? (g_Session.route == 0 ? BOSS_ELIS : BOSS_KIKURI)
            : stage == 19 ? (g_Session.route == 0 ? BOSS_SARIEL : BOSS_KONNGARA)
                          : BOSS_NONE);
    Hud_Load(
        &g_GameplayAssets->hud, Gameplay_StageSprites(),
        &g_GameplayAssets->hud_font);
    Player_EndInvincibility();
    M_BossSetup(host_tick);
    if (g_Gameplay->boss_stage) {
        Stage_SetFirstStage(true);
        Items_Reset();
    }
    const BOSS_STRATEGY *const boss = Gameplay_Boss();
    if (boss != nullptr && boss->resume_setup != nullptr) {
        m_StagePhase = GP_STAGE_BOSS_SETUP;
    } else if (boss != nullptr && boss->begin_entrance != nullptr) {
        boss->begin_entrance(Gameplay_BossAssets());
        m_StagePhase = GP_STAGE_FINAL_ENTRANCE;
    } else {
        M_StageBackground();
    }
}

static void M_StageReady(void)
{
    StageTimer_Redraw(
        g_Stage->first_stage, g_Stage->timer_initialized, g_Session.route);
    Stage_SetMaximumCombo(0);
    Orb_LeavePortal();
    Player_SetBombFrame(0);
    Stage_SetFrame(0);
    Orb_ResetFlight();
    if (g_Session.stage_id % 5 == 0 || g_Gameplay->boss_stage) {
        Player_Center();
    }
    Player_StopActions();
    Stage_SetCombo(0);
    GameplayLife_Begin();
}

void GameplayStage_Begin(void)
{
    p_Gameplay->sequence = GAMEPLAY_SEQUENCE_STAGE;
    m_StagePhase = GP_STAGE_BEGIN;
}

bool GameplayStage_Step(const uint64_t host_tick)
{
    switch (m_StagePhase) {
    case GP_STAGE_BEGIN:
        M_StageBegin(host_tick);
        return true;
    case GP_STAGE_BOSS_SETUP:
        if (!Gameplay_BossCall(
                Gameplay_Boss()->resume_setup, nullptr, host_tick)) {
            return false;
        }
        M_HookBossHp();
        M_StageBackground();
        return true;
    case GP_STAGE_FINAL_ENTRANCE:
        if (!Gameplay_BossCall(
                Gameplay_Boss()->resume_entrance, nullptr, host_tick)) {
            return false;
        }
        M_StageReady();
        return true;
    case GP_STAGE_OBJECT_ENTRANCE:
        if (!StageObjects_ResumeEntrance(Gameplay_StageSprites())) {
            return false;
        }
        if (g_Stage->first_stage) {
            Gameplay_CopyPage(0, 1);
        } else {
            StageObjects_CopyToBackground();
            Sprite_PtnPut(
                &g_GameplayAssets->miko, 0, -1, g_Player->left, PLAYER_TOP);
            Items_Render(Gameplay_StageSprites());
        }
        if (Gameplay_Boss() != nullptr
            && Gameplay_Boss()->finish_entrance != nullptr) {
            Gameplay_Boss()->finish_entrance();
        }
        p_Gameplay->animation_frame = 0;
        if (g_Session.stage_id % 5 == 0 || g_Gameplay->boss_stage) {
            M_TitleChar();
            m_StagePhase = GP_STAGE_TITLE;
        } else {
            M_Line("STAGE");
            m_StagePhase = GP_STAGE_LABEL;
        }
        return true;
    case GP_STAGE_TITLE:
        if (!VSyncIrq_DelayReady(8)) {
            return false;
        }
        if (++p_Gameplay->animation_frame < 6) {
            M_TitleChar();
        } else {
            M_Line("STAGE");
            m_StagePhase = GP_STAGE_LABEL;
        }
        return true;
    case GP_STAGE_LABEL:
        if (!VSyncIrq_DelayReady(35)) {
            return false;
        }
        {
            const uint32_t number = (uint32_t)(g_Session.stage_id + 1);
            const GAME_TEXT_GLYPH *glyphs[] = {
                GameText_Find(&g_GameplayAssets->menu_font, '0' + number / 10),
                GameText_Find(&g_GameplayAssets->menu_font, '0' + number % 10),
            };
            Tram_PutLine16(glyphs, 2, TRAM_RED, TRAM_BLACK);
        }
        m_StagePhase = GP_STAGE_NUMBER;
        return true;
    case GP_STAGE_NUMBER:
        if (!VSyncIrq_DelayReady(35)) {
            return false;
        }
        Tram_Clear();
        M_StageReady();
        return true;
    }
    return true;
}
