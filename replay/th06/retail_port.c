#include "replay/retail/patch.h"
#include "replay/retail/retail.h"
#include "replay/th06/addresses.h"
#include "replay/th06/harness.h"
#include "replay/th06/retail_state.h"

#include "core/base/utils.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// Hooks retail th06 1.02h for the retail backend, as th06retail.dll.

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef int(__attribute__((thiscall)) * M_RUN_CALC_CHAIN_FN)(void *chain);
typedef void(__attribute__((thiscall)) * M_ADD_STRING_FN)(
    void *text, void *pos, const char *string);
#pragma GCC diagnostic pop

typedef int(__cdecl *M_CALLBACK_FN)(void *arg);
typedef uint16_t(__cdecl *M_READ_BUTTONS_FN)(void);

static const CLOCK_RANGE m_Pacing[] = {
    { RETAIL_RUN_FRAME, RETAIL_RUN_FRAME_SIZE },
    { RETAIL_START_TITLE, RETAIL_START_TITLE_SIZE },
};

static const char *const m_Private[] = { "score.dat", "replay" };

static bool M_Idle(void *const context)
{
    const int32_t state = RETAIL_MAIN_MENU_AT->state;
    return RETAIL_APP_AT->cur_scene == RETAIL_SCENE_TITLE
        && (state == RETAIL_MAIN_MENU_WAIT_FOR_KEY
            || state == RETAIL_MAIN_MENU_TITLE_MENU
            || state == RETAIL_MAIN_MENU_REPLAYS);
}

static void M_Seed(void *const context)
{
    RETAIL_RNG_AT->seed = 0;
    RETAIL_RNG_AT->generated = 0;
}

static void M_RequestFullRepaint(void *const context)
{
    RETAIL_APP_AT->repaint_frames = RETAIL_REPAINT_FRAMES;
}

static uint32_t M_ReadHeld(void)
{
    return ((M_READ_BUTTONS_FN)RETAIL_READ_BUTTONS)();
}

static uint16_t __cdecl M_ReadButtons(void)
{
    return (uint16_t)Retail_Buttons(M_ReadHeld);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
static int __attribute__((thiscall)) M_RunCalcChain(void *const chain)
{
    Retail_StartCalc();
    const int result = ((M_RUN_CALC_CHAIN_FN)RETAIL_RUN_CALC_CHAIN)(chain);
    Retail_EndCalc();
    return result;
}

// Shows the driver's rate in place of the one that DrawFps formatted.
static void __attribute__((thiscall)) M_AddFps(
    void *const text, void *const pos, const char *const string)
{
    const FPS measured = {
        .available = string[0] != '\0',
        .rate = strtof(string, nullptr),
    };
    const FPS shown = Retail_Fps(measured);
    char buffer[32] = "";
    if (shown.available) {
        snprintf(buffer, sizeof(buffer), "%.02ffps", (double)shown.rate);
    }
    ((M_ADD_STRING_FN)RETAIL_ADD_STRING)(text, pos, buffer);
}
#pragma GCC diagnostic pop

// Ends the calc's row when the chain restarts within the frame.
static int __cdecl M_PlayOnUpdate2(void *const replay)
{
    const int result = ((M_CALLBACK_FN)RETAIL_PLAY_ON_UPDATE_2)(replay);
    if (result == RETAIL_CHAIN_RESTART) {
        Retail_RestartCalc();
    }
    return result;
}

static RESULT M_Hook(void *const context)
{
    MUST(Patch_Call(
        RETAIL_RUN_CALC_CHAIN_CALL, RETAIL_RUN_CALC_CHAIN,
        (uintptr_t)M_RunCalcChain));
    MUST(Patch_Value32(
        RETAIL_PLAY_ON_UPDATE_2_SITE, RETAIL_PLAY_ON_UPDATE_2,
        (uint32_t)(uintptr_t)M_PlayOnUpdate2));
    MUST(Patch_CallsIn(
        RETAIL_APP_ON_UPDATE, RETAIL_APP_ON_UPDATE_SIZE, RETAIL_READ_BUTTONS,
        (uintptr_t)M_ReadButtons, 1));
    return Patch_CallsIn(
        RETAIL_DRAW_FPS, RETAIL_DRAW_FPS_SIZE, RETAIL_ADD_STRING,
        (uintptr_t)M_AddFps, 1);
}

// Holds 東方紅魔郷.cfg: the shipped pad mapping, windowed, 32-bit color,
// no music or sound, no frame skip and color-add emulation.
// The literal's terminating NUL is not part of the file.
static const char m_Config[] =
    // Pad buttons of shoot, bomb, focus, menu, up, down, left, right, skip.
    "\x00\x00\x01\x00\x02\x00\x04\x00\xff\xff"
    "\xff\xff\xff\xff\xff\xff\x03\x00\x00\x00"
    // Version 0x102.
    "\x02\x01\x00\x00"
    // Lives, bombs, color, music, sound, difficulty, windowed, frame skip.
    "\x02\x03\x00\x00\x00\x01\x01\x00"
    // Pad thresholds x and y.
    "\x58\x02\x58\x02"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    // Flags.
    "\x01\x00\x00\x00";
static_assert(sizeof(m_Config) - 1 == 0x38);

static const STAGED_FILE m_Files[] = {
    { "東方紅魔郷.cfg", (const uint8_t *)m_Config, sizeof(m_Config) - 1 },
};

static const RETAIL_PORT m_Port = {
    .port = {
        .game = &g_Th06Harness,
        .sample = Th06Retail_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
        .request_full_repaint = M_RequestFullRepaint,
        .files = m_Files,
        .file_count = ARRAY_SIZE(m_Files),
    },
    .pacing = m_Pacing,
    .pacing_count = ARRAY_SIZE(m_Pacing),
    .private_paths = m_Private,
    .private_count = ARRAY_SIZE(m_Private),
    .hook = M_Hook,
};

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        Retail_Attach(&m_Port);
    } else if (reason == DLL_PROCESS_DETACH) {
        Retail_Detach();
    }
    return TRUE;
}
