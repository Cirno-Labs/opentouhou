#include "../core/session_native.h"

#include "th06/buttons.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"
#include "th06/main.h"
#include "th06/settings.h"

static const SCRIPT_BINDING m_Bindings[] = {
#define M_BUTTON(field_, bit_, ...) { #field_, bit_ },
#define M_STICK(...)
    TH06_ACTIONS(M_BUTTON, M_STICK)
#undef M_BUTTON
#undef M_STICK
};

static void M_Buttons(void *const input, const uint16_t word)
{
    *(TH06_INPUT *)input = Buttons_FromWord(word);
}

static void M_Prepare(void)
{
    Rng_SetSeed(&g_Rng, 0);
    g_Settings.sound = false;
    g_Settings.music = SETTINGS_MUSIC_OFF;
}

static bool M_CaseReady(void)
{
    return g_GameManager.mode.playing && !g_GameManager.mode.in_demo;
}

static bool M_CaseEnded(void)
{
    return false;
}

int main(const int argc, char **const argv)
{
    const SESSION_GAME game = {
        .game = &g_GameDesc,
        .bindings = m_Bindings,
        .binding_count = ARRAY_SIZE(m_Bindings),
        .replay_copy = "replay/th6_ud0000.rpy",
        .set_buttons = M_Buttons,
        .prepare = M_Prepare,
        .comparison = &g_RunnerGame,
        .case_ready = M_CaseReady,
        .case_ended = M_CaseEnded,
    };
    return Session_NativeMain(&game, argc, argv);
}
