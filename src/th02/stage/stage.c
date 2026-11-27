#include "th02/stage/private.h"
#include "th02/background.h"
#include "th02/pillars.h"
#include "th02/stage_effects.h"

#include <string.h>

static STAGE_STATE m_Stage;
const STAGE_STATE *const g_Stage = &m_Stage;
STAGE_STATE *const p_Stage = &m_Stage;

void Stage_Init(void)
{
    memset(&m_Stage, 0, sizeof(m_Stage));
    Background_Init();
    Pillars_Init();
    StageEffects_Init();
}

void Stage_Begin(const uint8_t stage, const bool reduce_effects)
{
    m_Stage.index = stage;
    m_Stage.reduce_effects = reduce_effects;
    Pillars_ResetStage(stage);
    StageEffects_ResetStage(stage);
}

void Stage_Reset(void)
{
    memset(&m_Stage.world, 0, sizeof(m_Stage.world));
    memset(&m_Stage.scroll, 0, sizeof(m_Stage.scroll));
}

void Stage_SetKeys(const uint16_t keys)
{
    m_Stage.world.keys = keys;
}

void Stage_SetSlowdown(const uint8_t frames)
{
    m_Stage.world.slowdown = frames;
}

void Stage_SetMidbossRow(const int16_t row)
{
    m_Stage.world.midboss_row = row;
}

void Stage_RestartFrames(void)
{
    m_Stage.world.frame = 0;
}

void Stage_CountExtraBossFrame(void)
{
    m_Stage.world.extra_boss_frames++;
}

void Stage_Quit(void)
{
    m_Stage.world.quit = 1;
}

void Stage_ClearQuit(void)
{
    m_Stage.world.quit = 0;
}

void Stage_SetVSyncCount(const uint16_t count)
{
    m_Stage.world.vsync_count = count;
}

void Stage_ClearRowChange(void)
{
    m_Stage.scroll.row_changed = 0;
}

void Stage_SetMidbossActive(const bool active)
{
    m_Stage.world.midboss_active = active;
}

void Stage_AdvanceFrame(void)
{
    m_Stage.world.frame++;
}

void Stage_SetProgression(const uint8_t progression)
{
    m_Stage.world.progression = progression;
}
