#include "th01/main.h"

#include "shell/game/entry.h"

#include <SDL3/SDL.h>

#include "test.h"

SDL_AppResult SDL_AppIterate(void *appstate);

static SHELL_STATUS m_Status;
static int m_Ticks, m_Flushes;
static bool m_InsideTick;
static char m_Order[8];
static int m_Count;

static bool M_Tick(const void *input)
{
    m_InsideTick = true;
    m_Ticks++;
    m_Order[m_Count++] = 'T';
    m_InsideTick = false;
    return true;
}

const GAME_DESC g_GameDesc = { .tick = M_Tick };

SHELL_STATUS Shell_Init(const GAME_DESC *desc, int argc, char *argv[])
{
    return SHELL_CONTINUE;
}

SHELL_STATUS Shell_Event(const SDL_Event *event)
{
    return SHELL_CONTINUE;
}

SHELL_STATUS Shell_Iterate(void)
{
    for (int i = 0; i < 3; i++) {
        g_GameDesc.tick(nullptr);
    }
    return m_Status;
}

void Shell_Quit(void)
{
}

void Th01_FlushSaves(void)
{
    CHECK(!m_InsideTick);
    CHECK_EQ_INT(m_Ticks, 3);
    m_Flushes++;
    m_Order[m_Count++] = 'F';
}

TEST(host_flushes_after_all_ticks_including_the_final_iteration)
{
    const SHELL_STATUS statuses[] = {
        SHELL_CONTINUE,
        SHELL_SUCCESS,
        SHELL_FAILURE,
    };
    for (size_t i = 0; i < 3; i++) {
        m_Status = statuses[i];
        m_Ticks = 0;
        m_Flushes = 0;
        m_Count = 0;
        memset(m_Order, 0, sizeof(m_Order));
        CHECK_EQ_INT(SDL_AppIterate(nullptr), (SDL_AppResult)m_Status);
        CHECK_EQ_INT(m_Flushes, 1);
        CHECK_EQ_STR(m_Order, "TTTF");
    }
}
