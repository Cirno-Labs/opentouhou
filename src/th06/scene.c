#include "th06/scene.h"

#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/stubs.h"

static SCENE m_Current;
static SCENE m_Requested;
static SCENE m_Previous;
static int32_t m_Frames;
static CHAIN_ELEM m_Elem;

static CHAIN_STATUS M_GoToTitle(const int32_t mode)
{
    m_Requested = SCENE_TITLE;
    return IS_OK(TitleScene_SetUp(mode)) ? CHAIN_CONTINUE : CHAIN_QUIT;
}

// Mirrors App::OnUpdate's switch on the current scene and the one
// requested, with its gotos as direct calls to M_GoToTitle.
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
            return IS_OK(GameManager_Register()) ? CHAIN_CONTINUE : CHAIN_QUIT;
        case SCENE_RESET:
            return CHAIN_RESET;
        case SCENE_SCORES:
            return IS_OK(ResultScreen_Register(0)) ? CHAIN_CONTINUE
                                                   : CHAIN_QUIT;
        case SCENE_MUSIC_ROOM:
            return IS_OK(MusicRoom_Register()) ? CHAIN_CONTINUE : CHAIN_QUIT;
        case SCENE_ENDING:
            GameManager_Remove();
            return IS_OK(Ending_Register()) ? CHAIN_CONTINUE : CHAIN_QUIT;
        default:
            return CHAIN_CONTINUE;
        }

    case SCENE_SCORES:
        switch (m_Requested) {
        case SCENE_QUIT:
            return CHAIN_QUIT;
        case SCENE_TITLE:
            return M_GoToTitle(0);
        default:
            return CHAIN_CONTINUE;
        }

    case SCENE_GAMEPLAY:
        switch (m_Requested) {
        case SCENE_QUIT:
            return CHAIN_QUIT;
        case SCENE_TITLE:
            GameManager_Remove();
            Replay_Save();
            return M_GoToTitle(0);
        case SCENE_GAME_RESULT:
            GameManager_Remove();
            return IS_OK(ResultScreen_Register(1)) ? CHAIN_CONTINUE
                                                   : CHAIN_QUIT;
        case SCENE_NEXT_STAGE:
            GameManager_Remove();
            if (!IS_OK(GameManager_Register())) {
                return CHAIN_QUIT;
            }
            if (m_Requested == SCENE_TITLE) {
                GameManager_Remove();
                Replay_Save();
                return M_GoToTitle(0);
            }
            m_Requested = SCENE_GAMEPLAY;
            return CHAIN_CONTINUE;
        case SCENE_REPLAY_END:
            GameManager_Remove();
            Replay_Save();
            m_Requested = SCENE_TITLE;
            return IS_OK(TitleScene_SetUp(1)) ? CHAIN_CONTINUE : CHAIN_QUIT;
        case SCENE_ENDING:
            GameManager_Remove();
            return IS_OK(Ending_Register()) ? CHAIN_CONTINUE : CHAIN_QUIT;
        default:
            return CHAIN_CONTINUE;
        }

    case SCENE_GAME_RESULT:
        switch (m_Requested) {
        case SCENE_QUIT:
            Replay_Save();
            return CHAIN_QUIT;
        case SCENE_TITLE:
            Replay_Save();
            return M_GoToTitle(0);
        default:
            return CHAIN_CONTINUE;
        }

    case SCENE_MUSIC_ROOM:
        switch (m_Requested) {
        case SCENE_QUIT:
            return CHAIN_QUIT;
        case SCENE_TITLE:
            return M_GoToTitle(0);
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
            return IS_OK(ResultScreen_Register(1)) ? CHAIN_CONTINUE
                                                   : CHAIN_QUIT;
        default:
            return CHAIN_CONTINUE;
        }

    default:
        return CHAIN_CONTINUE;
    }
}

static CHAIN_STATUS M_Calc(void *const arg)
{
    Buttons_Update(&g_Buttons, &g_Input);
    if (m_Current != m_Requested) {
        m_Previous = m_Current;
        const CHAIN_STATUS status = M_Switch();
        if (status == CHAIN_QUIT || status == CHAIN_RESET) {
            return status;
        }
        Buttons_Clear(&g_Buttons);
        m_Current = m_Requested;
    }
    m_Frames++;
    return CHAIN_CONTINUE;
}

RESULT Scene_Register(void)
{
    m_Current = SCENE_BOOT;
    m_Requested = SCENE_NONE;
    m_Previous = SCENE_NONE;
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

SCENE Scene_GetRequested(void)
{
    return m_Requested;
}
