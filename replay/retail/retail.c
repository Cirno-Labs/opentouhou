#include "retail.h"

#include "direct3d.h"
#include "overlay.h"

#include "core/base/log.h"
#include "core/io/path.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

#define M_ARENA_SIZE (1024 * 1024)

// GAMEretail.dll lays work/ over game_dir, serves the staged files and
// the port's from memory above game_dir, puts the clock imports on the
// frame clock and hooks Direct3D 8: each Present after the first calc
// that no calc encloses ends a frame, with the screenshot read back from
// the back buffer, and the devices drop the draws of each frame that
// Driver_WantsDraw rejects. A live capture also waits for real time at
// each frame. The process exits with the driver's status once
// Driver_Done.

// Holds the run; the hooked program calls in without context.
static struct {
    const RETAIL_PORT *port;
    ARENA arena;
    RUN run;
    DRIVER *driver;
    bool in_calc;
    // Tells whether the first calc started; the presentations before it
    // are the program's start-up and end no frame.
    bool started;
} m_Retail;

// Reads the backend program's command line, which it writes to standard
// input as NUL-terminated arguments and then closes, into argv.
static RESULT M_ReadArgs(
    ARENA *const arena, int *const argc, char ***const argv)
{
    const HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    ARRAY(char) text;
    Array_Init(&text, Arena_Allocator(arena));
    char buffer[4096];
    DWORD read;
    while (ReadFile(in, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        Array_AppendSlice(&text, buffer, (int32_t)read);
    }
    FAIL_IF(
        text.count == 0 || text.items[text.count - 1] != '\0',
        "standard input holds no command line");
    int count = 0;
    for (int32_t i = 0; i < text.count; i++) {
        count += text.items[i] == '\0';
    }
    char **const args =
        Arena_AllocArrZ(arena, (size_t)count + 1, sizeof(char *));
    char *arg = text.items;
    for (int i = 0; i < count; i++) {
        args[i] = arg;
        arg += strlen(arg) + 1;
    }
    *argc = count;
    *argv = args;
    return OK;
}

static RESULT M_FullPathW(const char *const path, wchar_t out[MAX_PATH])
{
    wchar_t wide[MAX_PATH];
    FAIL_IF(
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, MAX_PATH) == 0,
        "bad path %s", path);
    const DWORD length = GetFullPathNameW(wide, MAX_PATH, out, nullptr);
    FAIL_IF(length == 0 || length >= MAX_PATH, "bad path %s", path);
    return OK;
}

static void M_Exit(const int status)
{
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), (UINT)status);
}

static void M_Check(const RESULT result)
{
    if (!Result_Log(result, LOG_LEVEL_ERROR)) {
        M_Exit(2);
    }
}

// Closes the driver, which writes its outputs only after a completed run,
// and returns the run's exit status.
static int M_Close(const bool completed)
{
    DRIVER *const driver = m_Retail.driver;
    m_Retail.driver = nullptr;
    bool differs = false;
    if (!Result_Log(
            Driver_Close(driver, completed, &differs), LOG_LEVEL_ERROR)) {
        return 2;
    }
    if (!completed) {
        return 2;
    }
    return differs ? 1 : 0;
}

static void M_Present(IDirect3DDevice8 *const device)
{
    DRIVER *const driver = m_Retail.driver;
    if (m_Retail.in_calc || !m_Retail.started || driver == nullptr) {
        return;
    }
    IMAGE screen = {};
    const bool shot = Driver_WantsShot(driver);
    if (shot) {
        M_Check(Direct3d_ReadBackBuffer(device, &screen));
    }
    const RESULT result = Driver_EndFrame(driver, shot ? &screen : nullptr);
    if (shot) {
        Image_Free(&screen);
    }
    M_Check(result);
    Clock_Advance();
    if (Driver_Done(driver)) {
        M_Exit(M_Close(true));
    }
    Direct3d_SetDrawing(Driver_WantsDraw(driver));
}

static RESULT M_Attach(const RETAIL_PORT *const port)
{
    m_Retail.port = port;
    Arena_Init(&m_Retail.arena, "retail run", M_ARENA_SIZE);
    int argc;
    char **argv;
    MUST(M_ReadArgs(&m_Retail.arena, &argc, &argv));
    bool help;
    MUST(Run_Parse(argv[0], argc, argv, &m_Retail.arena, &m_Retail.run, &help));
    FAIL_IF(help, "the game takes no --help");
    const RUN *const run = &m_Retail.run;

    // The overlay sends every write to work/, so work/ must not lie in
    // game_dir, where the writes would land in the game's files.
    wchar_t game_dir[MAX_PATH];
    wchar_t work_dir[MAX_PATH];
    MUST(M_FullPathW(run->game_dir, game_dir));
    MUST(M_FullPathW(
        Path_Join(Arena_Allocator(&m_Retail.arena), run->out_dir, "work"),
        work_dir));
    const size_t length = wcslen(game_dir);
    FAIL_IF(
        _wcsnicmp(game_dir, work_dir, length) == 0
            && (work_dir[length] == L'\\' || work_dir[length] == L'\0'),
        "out_dir lies in game_dir");
    MUST(Driver_Open(&port->port, run, "retail", &m_Retail.driver));
    size_t count;
    const STAGED_FILE *const staged = Driver_Staged(m_Retail.driver, &count);
    MUST(Overlay_Install(
        game_dir, work_dir, port->private_paths, port->private_count, staged,
        count));
    const HMODULE module = GetModuleHandleW(nullptr);
    FAIL_IF(
        !Clock_Install(
            module, port->port.game->frame_rate, port->pacing,
            port->pacing_count, run->live),
        "the program imports no clock");
    FAIL_IF(
        !Direct3d_Install(module, M_Present),
        "the program does not import Direct3DCreate8");
    Direct3d_SetDrawing(Driver_WantsDraw(m_Retail.driver));
    return port->hook(port->port.context);
}

void Retail_Attach(const RETAIL_PORT *const port)
{
    M_Check(M_Attach(port));
}

void Retail_Detach(void)
{
    DRIVER *const driver = m_Retail.driver;
    if (driver == nullptr) {
        return;
    }
    const bool cut_short = !m_Retail.run.live && !Driver_Done(driver);
    if (cut_short) {
        LOG_ERROR("the game quit before the run ended");
    }
    M_Exit(M_Close(!cut_short));
}

uint32_t Retail_Buttons(uint32_t (*const read)(void))
{
    return Driver_Buttons(m_Retail.driver, m_Retail.run.live ? read() : 0);
}

void Retail_StartCalc(void)
{
    m_Retail.in_calc = true;
    if (!m_Retail.started) {
        m_Retail.started = true;
        const PORT *const port = &m_Retail.port->port;
        port->seed(port->context);
    }
}

void Retail_EndCalc(void)
{
    Driver_EndCalc(m_Retail.driver);
    m_Retail.in_calc = false;
}

void Retail_RestartCalc(void)
{
    Driver_EndCalc(m_Retail.driver);
}

FPS Retail_Fps(const FPS measured)
{
    return Driver_Fps(m_Retail.driver, measured);
}
