#include "th07/scene.h"

#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/replay.h"
#include "th07/screens/main.h"
#include "th07/screens/result/screen.h"
#include "th07/settings.h"
#include "th07/stubs.h"

static SCENE m_Current;
static SCENE m_Requested;
static SCENE m_Previous;
static int32_t m_Frames;
static CHAIN_ELEM m_Elem;

static CHAIN_STATUS M_Status(const RESULT result)
{
    return IS_OK(result) ? CHAIN_CONTINUE : CHAIN_QUIT;
}

static CHAIN_STATUS M_GoToTitle(const int32_t mode)
{
    m_Requested = SCENE_TITLE;
    return M_Status(TitleScene_SetUp(mode));
}

// The original passes through boot on the way to the title, so the
// replay sees that request.
static CHAIN_STATUS M_DiscardToTitle(const int32_t mode)
{
    m_Requested = SCENE_BOOT;
    Replay_Discard();
    return M_GoToTitle(mode);
}

static CHAIN_STATUS M_ReregisterGameplay(void)
{
    if (!IS_OK(GameManager_Register())) {
        return CHAIN_QUIT;
    }
    m_Requested = SCENE_GAMEPLAY;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_FromGameplay(void)
{
    switch (m_Requested) {
    case SCENE_QUIT:
        return CHAIN_QUIT;
    case SCENE_TITLE:
        GameManager_Remove();
        return M_DiscardToTitle(0);
    case SCENE_GAME_RESULT:
        GameManager_Remove();
        return M_Status(ResultScreen_Register(1));
    case SCENE_RETRY:
        GameManager_Remove();
        if (!g_GameManager.mode.in_practice
            && g_GameManager.run.difficulty < 4) {
            g_GameManager.run.stage = 0;
        } else {
            g_GameManager.run.stage--;
        }
        Replay_Discard();
        return M_ReregisterGameplay();
    case SCENE_RESTART_STAGE:
        m_Requested = SCENE_NEXT_STAGE;
        GameManager_Remove();
        g_GameManager.run.stage--;
        return M_ReregisterGameplay();
    case SCENE_NEXT_STAGE_AGAIN:
        m_Requested = SCENE_NEXT_STAGE;
        GameManager_Remove();
        return M_ReregisterGameplay();
    case SCENE_NEXT_STAGE:
        GameManager_Remove();
        return M_ReregisterGameplay();
    case SCENE_REPLAY_END:
        GameManager_Remove();
        return M_DiscardToTitle(1);
    case SCENE_ENDING:
        g_GameManager.records.play_stats.counts[g_GameManager.run.difficulty]
            .clears++;
        GameManager_Remove();
        return M_Status(Ending_Register());
    default:
        return CHAIN_CONTINUE;
    }
}

static CHAIN_STATUS M_Switch(void)
{
    switch (m_Current) {
    case SCENE_BOOT:
        return M_GoToTitle(0);

    case SCENE_TITLE:
        switch (m_Requested) {
        case SCENE_QUIT:
            return CHAIN_QUIT;
        case SCENE_GAMEPLAY:
            return M_Status(GameManager_Register());
        case SCENE_RESET:
            return CHAIN_RESET;
        case SCENE_SCORES:
            return M_Status(ResultScreen_Register(0));
        case SCENE_MUSIC_ROOM:
            return M_Status(MusicScreen_Register());
        case SCENE_ENDING:
            GameManager_Remove();
            return M_Status(Ending_Register());
        default:
            return CHAIN_CONTINUE;
        }

    case SCENE_SCORES:
    case SCENE_MUSIC_ROOM:
        switch (m_Requested) {
        case SCENE_QUIT:
            return CHAIN_QUIT;
        case SCENE_TITLE:
            return M_GoToTitle(0);
        default:
            return CHAIN_CONTINUE;
        }

    case SCENE_GAMEPLAY:
        return M_FromGameplay();

    case SCENE_GAME_RESULT:
        switch (m_Requested) {
        case SCENE_QUIT:
            Replay_Discard();
            return CHAIN_QUIT;
        case SCENE_TITLE:
            return M_DiscardToTitle(0);
        default:
            return CHAIN_CONTINUE;
        }

    case SCENE_ENDING:
        switch (m_Requested) {
        case SCENE_QUIT:
            return CHAIN_QUIT;
        case SCENE_TITLE:
            return M_GoToTitle(0);
        case SCENE_GAME_RESULT:
            return M_Status(ResultScreen_Register(1));
        default:
            return CHAIN_CONTINUE;
        }

    default:
        return CHAIN_CONTINUE;
    }
}

static CHAIN_STATUS M_Calc(void *const arg)
{
    if (!g_GameManager.mode.calc_dropped) {
        Buttons_Update(&g_Buttons, &g_Input, g_Settings.shoot_focuses);
    } else {
        Buttons_Merge(&g_Buttons, &g_Input, g_Settings.shoot_focuses);
    }
    if (m_Current != m_Requested) {
        m_Previous = m_Current;
        const CHAIN_STATUS status = M_Switch();
        if (status == CHAIN_QUIT || status == CHAIN_RESET) {
            return status;
        }
        Buttons_Clear(&g_Buttons);
    }
    m_Current = m_Requested;
    m_Frames++;
    return CHAIN_CONTINUE;
}

RESULT Scene_Register(void)
{
    m_Current = SCENE_BOOT;
    m_Requested = SCENE_QUIT;
    m_Frames = 0;
    Chain_InitElem(&m_Elem, M_Calc, nullptr);
    return Chain_AddCalc(&g_Chain, &m_Elem, 0);
}

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
}

SCENE Scene_Get(void)
{
    return m_Current;
}

SCENE Scene_GetPrevious(void)
{
    return m_Previous;
}

SCENE Scene_GetRequested(void)
{
    return m_Requested;
}
