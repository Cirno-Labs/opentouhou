#include "th01/boss.h"
#include "th01/extend.h"
#include "th01/gameplay/private.h"
#include "th01/miss.h"
#include "th01/player.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/stage.h"
#include "th01/video.h"
#include "th01/vsync_irq.h"

typedef enum {
    GP_EXIT_WAIT,
    GP_EXIT_MISS,
} GAMEPLAY_EXIT_PHASE;

static GAMEPLAY_EXIT_PHASE m_ExitPhase;

void GameplayExit_Begin(void)
{
    VSyncIrq_BeginWait(&p_Gameplay->wait);
    p_Gameplay->sequence = GAMEPLAY_SEQUENCE_EXIT;
    m_ExitPhase = GP_EXIT_WAIT;
}

bool GameplayExit_Step(const TH01_INPUT *const raw, const uint64_t host_tick)
{
    switch (m_ExitPhase) {
    case GP_EXIT_WAIT:
        if (!VSyncIrq_WaitReady(&p_Gameplay->wait)) {
            return false;
        }
        Video_SetScroll(0);
        Session_MarkExitCheckpoint();
        Boss_ClearTestDamage();
        Player_SetBombFrame(200);
        if (Stage_TakeCleared()) {
            Player_ClearHit();
            if (g_Stage->boss != BOSS_NONE) {
                p_Gameplay->wait_for_shot = true;
                Stage_SetFirstStage(true);
            }
            Session_AdvanceStage();
            GameplayEnd_BeginBonus(g_Session->stage_id, raw, host_tick);
        } else if (g_Session->lives <= 0) {
            GameplayEnd_BeginGameover();
        } else {
            Sound_Effect(SOUND_EFFECT_MISS);
            Session_LoseLife();
            Extend_SyncResidentLives(g_Session->lives);
            Miss_Begin(&g_GameplayAssets->miko, Gameplay_StageSprites());
            m_ExitPhase = GP_EXIT_MISS;
        }
        return true;
    case GP_EXIT_MISS:
        if (!Miss_Resume(&g_GameplayAssets->miko, Gameplay_StageSprites(), 1)) {
            return false;
        }
        Player_ClearHit();
        Player_GrantInvincibility(150);
        GameplayLife_Begin();
        return true;
    }
    return true;
}
