// Uses symlink of POSIX 2008.
#define _XOPEN_SOURCE 700

#include "dosbox.h"

#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
    #include <unistd.h>
#endif

#define M_ARENA_SIZE (1024 * 1024)
#define M_NO_SITE UINT32_MAX
#define M_RUNNING_MAX 8
#define M_SAMPLES_MAX 64
#define M_SAMPLES_SHOWN 8
#define M_PENDING_SPINS 1000
#define M_SCREEN_WIDTH 640
#define M_SCREEN_HEIGHT 400

// The BIOS key bitmap and key buffer in segment 0.
#define M_KEY_BITMAP 0x52a
#define M_KEY_BITMAP_SIZE 16
#define M_KEY_BUFFER 0x502
#define M_KEY_BUFFER_END 0x522
#define M_KEY_HEAD 0x524
#define M_KEY_TAIL 0x526
#define M_KEY_COUNT 0x528

#define M_FLAG_CARRY 0x0001
#define M_FLAG_PARITY 0x0004
#define M_FLAG_ZERO 0x0040
#define M_FLAG_SIGN 0x0080
#define M_FLAG_OVERFLOW 0x0800

#define M_OVERLAY_MARKER ".DBOVERLAY"

typedef enum {
    M_JUMP_NOT_TAKEN,
    M_JUMP_TAKEN,
    M_JUMP_UNKNOWN,
} M_JUMP;

typedef struct {
    int32_t program;
    uint16_t psp;
} M_RUNNING;

typedef struct {
    uint16_t cs;
    uint16_t ip;
    uint32_t count;
} M_SAMPLE;

static struct {
    const DOSBOX_PORT *port;
    const DOSBOX_HOST *host;
    ARENA arena;
    RUN run;
    DRIVER *driver;
    char *work;
    // Holds a copy of the staged files, which work/ holds too.
    STAGED_FILE *staged;
    size_t staged_count;
    // Gives base/ of out_dir, which links to each file of game_dir, and the
    // links.
    const char *base;
    char **links;
    size_t link_count;
    int status;
    // Tells whether the clock runs, from the first exec of a port program.
    bool started;
    // Gives the end of the current frame: anchor plus since + 1 frames.
    uint64_t anchor;
    uint32_t since;
    uint32_t frame;
    uint32_t held;
    bool vblank;
    // Counts the arrivals at a wait that let a pending VSync in.
    uint32_t pending_spins;
    // Tells whether no VSync wait has let its jump run since the guest took
    // the last VSync.
    bool vsync_unseen;
    // Gives the poll site whose jump has run since the guest last reached
    // a VSync wait or a delay, or -1.
    int32_t polling;
    // Gives the delay in progress and the emulated time when it returns.
    uint32_t delay_site;
    uint64_t delay_end;
    // Tells whether the guest has reached no wait since the last delay
    // returned, at chain_end; the next delay then starts there, so that the
    // work between two delays takes no time.
    bool delay_chained;
    uint64_t chain_end;
    int32_t last_wait;
    M_RUNNING running[M_RUNNING_MAX];
    int32_t running_count;
    // Gives the emulated time of the last site, for the watchdog.
    uint64_t progress;
    M_SAMPLE samples[M_SAMPLES_MAX];
    int32_t sample_count;
} m_Dosbox;

static uint16_t M_Read16(const uint32_t address)
{
    uint16_t value;
    m_Dosbox.host->read(address, &value, sizeof(value));
    return value;
}

static void M_Write16(const uint32_t address, const uint16_t value)
{
    m_Dosbox.host->write(address, &value, sizeof(value));
}

static uint32_t M_Linear(const uint16_t segment, const uint16_t offset)
{
    return (uint32_t)segment * 16 + offset;
}

static bool M_Condition(const uint8_t code, const uint16_t flags)
{
    const bool carry = (flags & M_FLAG_CARRY) != 0;
    const bool zero = (flags & M_FLAG_ZERO) != 0;
    const bool sign = (flags & M_FLAG_SIGN) != 0;
    const bool overflow = (flags & M_FLAG_OVERFLOW) != 0;
    bool holds = false;
    switch (code >> 1) {
    case 0:
        holds = overflow;
        break;
    case 1:
        holds = carry;
        break;
    case 2:
        holds = zero;
        break;
    case 3:
        holds = carry || zero;
        break;
    case 4:
        holds = sign;
        break;
    case 5:
        holds = (flags & M_FLAG_PARITY) != 0;
        break;
    case 6:
        holds = sign != overflow;
        break;
    default:
        holds = zero || sign != overflow;
        break;
    }
    return (code & 1) != 0 ? !holds : holds;
}

// Tells whether the jump at address goes to its target with regs.
static M_JUMP M_Jump(const uint32_t address, const DOSBOX_REGS *const regs)
{
    uint8_t code[2];
    m_Dosbox.host->read(address, code, sizeof(code));
    if (code[0] >= 0x70 && code[0] <= 0x7f) {
        return M_Condition(code[0] & 0x0f, regs->flags) ? M_JUMP_TAKEN
                                                        : M_JUMP_NOT_TAKEN;
    }
    if (code[0] == 0x0f && code[1] >= 0x80 && code[1] <= 0x8f) {
        return M_Condition(code[1] & 0x0f, regs->flags) ? M_JUMP_TAKEN
                                                        : M_JUMP_NOT_TAKEN;
    }
    switch (code[0]) {
    case 0xe9:
    case 0xea:
    case 0xeb:
        return M_JUMP_TAKEN;
    case 0xe2:
        return regs->cx != 1 ? M_JUMP_TAKEN : M_JUMP_NOT_TAKEN;
    case 0xe3:
        return regs->cx == 0 ? M_JUMP_TAKEN : M_JUMP_NOT_TAKEN;
    default:
        return M_JUMP_UNKNOWN;
    }
}

static uint64_t M_Deadline(void)
{
    const double rate = m_Dosbox.port->port.game->frame_rate;
    return m_Dosbox.anchor + (uint64_t)((m_Dosbox.since + 1) * 1e6 / rate);
}

// Closes the driver, which writes its outputs only after a completed run,
// and returns the run's exit status.
static int M_Close(const bool completed)
{
    DRIVER *const driver = m_Dosbox.driver;
    m_Dosbox.driver = nullptr;
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

static void M_Finish(const bool completed)
{
    m_Dosbox.status = M_Close(completed);
    m_Dosbox.host->quit(m_Dosbox.status);
}

// Ends the run with status 2 after a failure. Returns whether result is a
// success.
static bool M_Check(const RESULT result)
{
    if (Result_Log(result, LOG_LEVEL_ERROR)) {
        return true;
    }
    if (m_Dosbox.driver != nullptr) {
        M_Finish(false);
    }
    m_Dosbox.status = 2;
    m_Dosbox.host->quit(2);
    return false;
}

static void M_PushKey(const uint16_t code)
{
    const uint16_t head = M_Read16(M_KEY_HEAD);
    const uint16_t tail = M_Read16(M_KEY_TAIL);
    uint16_t next = (uint16_t)(tail + 2);
    if (next >= M_KEY_BUFFER_END) {
        next = M_KEY_BUFFER;
    }
    if (next == head) {
        return;
    }
    M_Write16(tail, code);
    M_Write16(M_KEY_TAIL, next);
    uint8_t count;
    m_Dosbox.host->read(M_KEY_COUNT, &count, sizeof(count));
    count++;
    m_Dosbox.host->write(M_KEY_COUNT, &count, sizeof(count));
}

static void M_SetKeys(const uint32_t buttons)
{
    const DOSBOX_PORT *const port = m_Dosbox.port;
    uint8_t bitmap[M_KEY_BITMAP_SIZE] = {};
    for (size_t i = 0; i < port->key_count; i++) {
        const DOSBOX_KEY *const key = &port->keys[i];
        if ((buttons & key->mask) != 0) {
            bitmap[key->code >> 3] |= (uint8_t)(1u << (key->code & 7));
        }
    }
    m_Dosbox.host->write(M_KEY_BITMAP, bitmap, sizeof(bitmap));
    const uint32_t pressed = buttons & ~m_Dosbox.held;
    for (size_t i = 0; i < port->key_count; i++) {
        const DOSBOX_KEY *const key = &port->keys[i];
        if ((pressed & key->mask) != 0) {
            M_PushKey((uint16_t)(key->code << 8 | key->ascii));
        }
    }
    m_Dosbox.held = buttons;
}

// Starts a frame: sets its buttons, moves the clock on and, when the frame
// has a VSync, writes its row and raises the VSync. Returns whether the
// frame has a VSync.
static bool M_BeginFrame(void)
{
    const DOSBOX_PORT *const port = m_Dosbox.port;
    M_SetKeys(Driver_Buttons(m_Dosbox.driver, 0));
    if (!port->advance(port->port.context)) {
        return false;
    }
    Driver_EndCalc(m_Dosbox.driver);
    m_Dosbox.host->raise_vsync();
    m_Dosbox.vblank = true;
    m_Dosbox.vsync_unseen = true;
    return true;
}

static RESULT M_Shot(IMAGE *const out)
{
    const size_t count = (size_t)M_SCREEN_WIDTH * M_SCREEN_HEIGHT;
    uint32_t *const pixels = Mem_Alloc(count * sizeof(*pixels));
    m_Dosbox.host->render(pixels);
    *out = Image_Create(M_SCREEN_WIDTH, M_SCREEN_HEIGHT, &g_HeapAllocator);
    for (size_t i = 0; i < count; i++) {
        uint8_t *const rgba = out->pixels + i * 4;
        rgba[0] = (uint8_t)(pixels[i] >> 16);
        rgba[1] = (uint8_t)(pixels[i] >> 8);
        rgba[2] = (uint8_t)pixels[i];
        rgba[3] = 0xff;
    }
    Mem_Free(pixels);
    return OK;
}

// Ends the current frame and returns whether the run goes on.
static bool M_EndFrame(void)
{
    DRIVER *const driver = m_Dosbox.driver;
    if (m_Dosbox.vblank) {
        m_Dosbox.host->clear_vblank();
        m_Dosbox.vblank = false;
    }
    IMAGE screen = {};
    const bool shot = Driver_WantsShot(driver);
    if (shot && !M_Check(M_Shot(&screen))) {
        return false;
    }
    const RESULT result = Driver_EndFrame(driver, shot ? &screen : nullptr);
    if (shot) {
        Image_Free(&screen);
    }
    if (!M_Check(result)) {
        return false;
    }
    m_Dosbox.frame++;
    if (Driver_Done(driver)) {
        M_Finish(true);
        return false;
    }
    m_Dosbox.since++;
    const uint64_t now = m_Dosbox.host->now();
    if (M_Deadline() <= now) {
        m_Dosbox.anchor = now;
        m_Dosbox.since = 0;
    }
    return true;
}

// Waits at a site: halts until the end of the frame, then ends it and,
// with until_vsync, the frames after it that have no VSync. Lets the jump
// run while the guest has not taken the VSync yet, since a halt would wake
// on it and leave the jump with the flags from before its handler, unless
// the guest has masked it for M_PENDING_SPINS arrivals. With until_vsync,
// lets the jump run once more after the guest takes the VSync, since the
// handler can run between the loop's test and its jump, and the jump then
// has the flags from before the handler. Without until_vsync, lets the
// jump run once when the loop starts, so that the loop reads the input of
// the frame that it starts in.
static void M_Wait(const bool until_vsync, const int32_t index)
{
    m_Dosbox.delay_chained = false;
    const bool pending = m_Dosbox.host->vsync_pending();
    if (pending && m_Dosbox.pending_spins++ < M_PENDING_SPINS) {
        return;
    }
    if (until_vsync) {
        m_Dosbox.polling = -1;
    } else if (m_Dosbox.polling != index) {
        m_Dosbox.polling = index;
        return;
    }
    if (!pending && until_vsync && m_Dosbox.vsync_unseen) {
        m_Dosbox.vsync_unseen = false;
        return;
    }
    m_Dosbox.pending_spins = 0;
    for (;;) {
        const uint64_t deadline = M_Deadline();
        if (m_Dosbox.host->now() < deadline) {
            m_Dosbox.host->halt(deadline);
            return;
        }
        if (until_vsync) {
            m_Dosbox.last_wait = index;
        }
        if (!M_EndFrame()) {
            return;
        }
        if (M_BeginFrame() || !until_vsync) {
            return;
        }
    }
}

static void M_Return(const bool far)
{
    DOSBOX_REGS regs;
    m_Dosbox.host->get_regs(&regs);
    const uint32_t stack = M_Linear(regs.ss, regs.sp);
    regs.ip = M_Read16(stack);
    if (far) {
        regs.cs = M_Read16(stack + 2);
    }
    regs.sp = (uint16_t)(regs.sp + (far ? 4 : 2));
    m_Dosbox.host->set_regs(&regs);
}

// Halts until the delay's end, ending each frame whose end comes first, and
// then returns from the call.
static void M_Delay(const DOSBOX_SITE *const site, const uint32_t address)
{
    const DOSBOX_PORT *const port = m_Dosbox.port;
    m_Dosbox.polling = -1;
    if (m_Dosbox.delay_site != address) {
        m_Dosbox.delay_site = address;
        m_Dosbox.delay_end =
            (m_Dosbox.delay_chained ? m_Dosbox.chain_end : m_Dosbox.host->now())
            + port->delay_time(port->port.context, site->id);
    }
    for (;;) {
        const uint64_t now = m_Dosbox.host->now();
        if (now >= m_Dosbox.delay_end) {
            break;
        }
        const uint64_t deadline = M_Deadline();
        if (now < deadline) {
            m_Dosbox.host->halt(
                deadline < m_Dosbox.delay_end ? deadline : m_Dosbox.delay_end);
            return;
        }
        if (!M_EndFrame()) {
            return;
        }
        M_BeginFrame();
    }
    m_Dosbox.delay_site = M_NO_SITE;
    m_Dosbox.delay_chained = true;
    m_Dosbox.chain_end = m_Dosbox.delay_end;
    M_Return(site->far);
}

static void M_SetSites(const bool set)
{
    if (m_Dosbox.running_count == 0) {
        return;
    }
    const M_RUNNING *const running =
        &m_Dosbox.running[m_Dosbox.running_count - 1];
    const DOSBOX_PORT *const port = m_Dosbox.port;
    for (size_t i = 0; i < port->site_count; i++) {
        const DOSBOX_SITE *const site = &port->sites[i];
        if (site->program != running->program) {
            continue;
        }
        const uint32_t address = M_Linear(
            (uint16_t)(running->psp + 0x10 + site->segment), site->offset);
        if (set) {
            m_Dosbox.host->set_site(address);
        } else {
            m_Dosbox.host->clear_site(address);
        }
    }
}

// Returns the index of the running program's site at address, or -1.
static int32_t M_FindSite(const uint32_t address)
{
    if (m_Dosbox.running_count == 0) {
        return -1;
    }
    const M_RUNNING *const running =
        &m_Dosbox.running[m_Dosbox.running_count - 1];
    const DOSBOX_PORT *const port = m_Dosbox.port;
    for (size_t i = 0; i < port->site_count; i++) {
        const DOSBOX_SITE *const site = &port->sites[i];
        if (site->program == running->program
            && M_Linear(
                   (uint16_t)(running->psp + 0x10 + site->segment),
                   site->offset)
                == address) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int M_CompareSamples(const void *const a, const void *const b)
{
    const M_SAMPLE *const x = a;
    const M_SAMPLE *const y = b;
    return (x->count < y->count) - (x->count > y->count);
}

// Counts the CS:IP that runs, and fails the run once no site ran for
// DOSBOX_WATCHDOG.
static void M_OnTimer(void *const context)
{
    if (!m_Dosbox.started || m_Dosbox.driver == nullptr) {
        return;
    }
    DOSBOX_REGS regs;
    m_Dosbox.host->get_regs(&regs);
    const uint16_t ip = regs.ip;
    const uint16_t cs = regs.cs;
    int32_t i = 0;
    while (i < m_Dosbox.sample_count
           && (m_Dosbox.samples[i].cs != cs || m_Dosbox.samples[i].ip != ip)) {
        i++;
    }
    if (i == m_Dosbox.sample_count && i < M_SAMPLES_MAX) {
        m_Dosbox.samples[i] = (M_SAMPLE) { .cs = cs, .ip = ip };
        m_Dosbox.sample_count++;
    }
    if (i < M_SAMPLES_MAX) {
        m_Dosbox.samples[i].count++;
    }
    const uint64_t now = m_Dosbox.host->now();
    if (now - m_Dosbox.progress < DOSBOX_WATCHDOG) {
        return;
    }
    LOG_ERROR(
        "no site ran for %" PRIu64 " us of emulated time, at frame %u",
        now - m_Dosbox.progress, (unsigned)m_Dosbox.frame);
    qsort(
        m_Dosbox.samples, (size_t)m_Dosbox.sample_count,
        sizeof(m_Dosbox.samples[0]), M_CompareSamples);
    for (int32_t j = 0; j < m_Dosbox.sample_count && j < M_SAMPLES_SHOWN; j++) {
        const M_SAMPLE *const sample = &m_Dosbox.samples[j];
        LOG_ERROR(
            "  %04x:%04x %u", sample->cs, sample->ip, (unsigned)sample->count);
    }
    M_Finish(false);
}

static void M_OnSite(void *const context, const uint32_t address)
{
    if (m_Dosbox.driver == nullptr) {
        return;
    }
    const int32_t index = M_FindSite(address);
    if (index < 0) {
        return;
    }
    const DOSBOX_PORT *const port = m_Dosbox.port;
    const DOSBOX_SITE *const site = &port->sites[index];
    m_Dosbox.progress = m_Dosbox.host->now();
    m_Dosbox.sample_count = 0;
    if (site->kind == DOSBOX_SITE_DELAY) {
        M_Delay(site, address);
        return;
    }
    if (site->kind == DOSBOX_SITE_CALL) {
        port->on_site(port->port.context, site->id);
        return;
    }
    DOSBOX_REGS regs;
    m_Dosbox.host->get_regs(&regs);
    const M_JUMP jump = M_Jump(address, &regs);
    if (jump == M_JUMP_UNKNOWN) {
        M_Check(FAIL(
            "site %u at %04x:%04x is no jump", (unsigned)index, regs.cs,
            regs.ip));
        return;
    }
    if (jump == M_JUMP_NOT_TAKEN) {
        return;
    }
    switch (site->kind) {
    case DOSBOX_SITE_VSYNC:
        M_Wait(true, index);
        return;
    case DOSBOX_SITE_POLL:
        M_Wait(false, index);
        return;
    case DOSBOX_SITE_VBLANK_END:
        m_Dosbox.host->clear_vblank();
        m_Dosbox.vblank = false;
        return;
    case DOSBOX_SITE_DELAY:
    case DOSBOX_SITE_CALL:
        return;
    }
}

static void M_Start(void)
{
    const PORT *const port = &m_Dosbox.port->port;
    m_Dosbox.started = true;
    m_Dosbox.host->own_vsync();
    m_Dosbox.anchor = m_Dosbox.host->now();
    m_Dosbox.since = 0;
    if (port->seed != nullptr) {
        port->seed(port->context);
    }
    m_Dosbox.host->set_timer(DOSBOX_WATCHDOG_TICK);
    M_BeginFrame();
}

static void M_OnExec(
    void *const context, const char *const name, const uint16_t psp)
{
    const DOSBOX_PORT *const port = m_Dosbox.port;
    if (m_Dosbox.driver == nullptr) {
        return;
    }
    // An overlay load, such as Borland's exec, replaces the program at psp.
    if (m_Dosbox.running_count > 0
        && m_Dosbox.running[m_Dosbox.running_count - 1].psp == psp) {
        M_SetSites(false);
        m_Dosbox.running_count--;
        M_SetSites(true);
        m_Dosbox.delay_site = M_NO_SITE;
        m_Dosbox.delay_chained = false;
    }
    int32_t program = -1;
    for (size_t i = 0; i < port->program_count; i++) {
        if (strcmp(port->programs[i], name) == 0) {
            program = (int32_t)i;
        }
    }
    if (program < 0) {
        return;
    }
    if (m_Dosbox.running_count == M_RUNNING_MAX) {
        M_Check(FAIL("%s runs nested too deep", name));
        return;
    }
    M_SetSites(false);
    m_Dosbox.running[m_Dosbox.running_count++] =
        (M_RUNNING) { .program = program, .psp = psp };
    M_SetSites(true);
    m_Dosbox.progress = m_Dosbox.host->now();
    if (!m_Dosbox.started) {
        M_Start();
    }
    if (port->on_exec != nullptr) {
        port->on_exec(port->port.context, (uint8_t)program);
    }
}

static void M_OnExit(
    void *const context, const uint16_t psp, const uint8_t status)
{
    int32_t i = m_Dosbox.running_count;
    while (i > 0 && m_Dosbox.running[i - 1].psp != psp) {
        i--;
    }
    if (i == 0 || m_Dosbox.driver == nullptr) {
        return;
    }
    M_SetSites(false);
    m_Dosbox.running_count = i - 1;
    M_SetSites(true);
    m_Dosbox.delay_site = M_NO_SITE;
    m_Dosbox.delay_chained = false;
}

// Tells whether two DOS paths name the same file, whatever their case and
// separators.
static bool M_SamePath(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        const char x = *a == '\\' ? '/' : (char)toupper((unsigned char)*a);
        const char y = *b == '\\' ? '/' : (char)toupper((unsigned char)*b);
        if (x != y) {
            return false;
        }
    }
    return *a == *b;
}

static bool M_IsStaged(const char *const path)
{
    for (size_t i = 0; i < m_Dosbox.staged_count; i++) {
        if (M_SamePath(m_Dosbox.staged[i].path, path)) {
            return true;
        }
    }
    return false;
}

static void M_OnStart(void *const context)
{
    const DOSBOX_PORT *const port = m_Dosbox.port;
    const char **const hidden = Arena_AllocArrZ(
        &m_Dosbox.arena, port->private_count + 1, sizeof(char *));
    size_t count = 0;
    for (size_t i = 0; i < port->private_count; i++) {
        if (!M_IsStaged(port->private_paths[i])) {
            hidden[count++] = port->private_paths[i];
        }
    }
    if (!m_Dosbox.host->mount(
            'C', m_Dosbox.base, m_Dosbox.work, hidden, count)) {
        M_Check(FAIL("cannot mount %s", m_Dosbox.run.game_dir));
    }
}

// Returns the path in work/ of a staged file: its DOS name in upper case.
static char *M_WorkPath(const char *const path)
{
    char *const name = Arena_Alloc(&m_Dosbox.arena, strlen(path) + 1);
    for (size_t i = 0;; i++) {
        name[i] = path[i] == '\\' ? '/' : (char)toupper((unsigned char)path[i]);
        if (path[i] == '\0') {
            break;
        }
    }
    return Path_Join(Arena_Allocator(&m_Dosbox.arena), m_Dosbox.work, name);
}

// Copies the staged files and writes them into work/.
static RESULT M_WriteStaged(void)
{
    ARENA *const arena = &m_Dosbox.arena;
    size_t count;
    const STAGED_FILE *const staged = Driver_Staged(m_Dosbox.driver, &count);
    m_Dosbox.staged = Arena_AllocArrZ(arena, count + 1, sizeof(STAGED_FILE));
    m_Dosbox.staged_count = count;
    for (size_t i = 0; i < count; i++) {
        uint8_t *const data = Arena_Alloc(arena, staged[i].size + 1);
        memcpy(data, staged[i].data, staged[i].size);
        m_Dosbox.staged[i] = (STAGED_FILE) {
            .path = Str_DupWith(Arena_Allocator(arena), staged[i].path),
            .data = data,
            .size = staged[i].size,
        };
        char *const path = M_WorkPath(staged[i].path);
        char *const slash = strrchr(path, '/');
        *slash = '\0';
        MUST(File_MakeDir(path));
        *slash = '/';
        WRITER writer;
        MUST(Writer_OpenFile(&writer, path));
        Writer_WriteBytes(&writer, staged[i].data, staged[i].size);
        MUST(Writer_Close(&writer));
    }
    return OK;
}

// Links each entry of game_dir into base/, under its UTF-8 name when its
// name is CP932: DOSBox-X looks a DOS name up by its UTF-8 form, and its
// overlay drive does not find a name past ASCII in the overlay. The backend
// runs on Linux only; a Windows build, for the tests, mounts game_dir.
static RESULT M_LinkBase(void)
{
#ifdef _WIN32
    m_Dosbox.base = m_Dosbox.run.game_dir;
    return OK;
#else
    ARENA *const arena = &m_Dosbox.arena;
    char *const base =
        Path_Join(Arena_Allocator(arena), m_Dosbox.run.out_dir, "base");
    m_Dosbox.base = base;
    MUST(File_MakeDir(base));
    FILE_LIST list;
    MUST(File_ListDir(m_Dosbox.run.game_dir, &g_HeapAllocator, &list));
    m_Dosbox.links =
        Arena_AllocArrZ(arena, (size_t)list.count + 1, sizeof(char *));
    RESULT result = OK;
    for (int32_t i = 0; IS_OK(result) && i < list.count; i++) {
        const char *const name = list.items[i];
        char *utf8;
        result = Cp932_DecodeText(name, strlen(name), &utf8);
        if (!IS_OK(result)) {
            break;
        }
        char *const target =
            Path_Join(Arena_Allocator(arena), m_Dosbox.run.game_dir, name);
        char *const link = Path_Join(Arena_Allocator(arena), base, utf8);
        Mem_Free(utf8);
        if (symlink(target, link) != 0) {
            result = FAIL("cannot link %s", link);
        } else {
            m_Dosbox.links[m_Dosbox.link_count++] = link;
        }
    }
    File_FreeList(&list);
    return result;
#endif
}

static bool M_FileHolds(
    const char *const path, const uint8_t *const data, const size_t size)
{
    READER reader;
    if (!Result_Ignore(Reader_OpenFile(&reader, path))) {
        return false;
    }
    bool same = Reader_Remaining(&reader) == size;
    if (same && size > 0) {
        uint8_t *const bytes = Mem_Alloc(size);
        Reader_ReadBytes(&reader, bytes, size);
        same = memcmp(bytes, data, size) == 0;
        Mem_Free(bytes);
    }
    Reader_Close(&reader);
    return same;
}

// Removes DOSBox-X's deletion markers below dir.
static RESULT M_RemoveMarkers(const char *const dir)
{
    FILE_LIST list;
    const RESULT listed = File_ListDir(dir, &g_HeapAllocator, &list);
    RESULT result = listed;
    for (int32_t i = 0; IS_OK(result) && i < list.count; i++) {
        char *const path = Path_Join(&g_HeapAllocator, dir, list.items[i]);
        FILE_TYPE type;
        result = File_GetType(path, &type);
        if (IS_OK(result) && type == FILE_TYPE_DIR) {
            result = M_RemoveMarkers(path);
        } else if (
            IS_OK(result) && Str_StartsWith(list.items[i], M_OVERLAY_MARKER)) {
            result = File_Remove(path);
        }
        Mem_Free(path);
    }
    File_FreeList(&list);
    return result;
}

// Removes base/ and leaves in work/ only the files that the game wrote: no
// deletion markers and no staged file that the game left unchanged.
static RESULT M_CleanWork(void)
{
    for (size_t i = 0; i < m_Dosbox.link_count; i++) {
        MUST(File_Remove(m_Dosbox.links[i]));
    }
    if (m_Dosbox.links != nullptr) {
        MUST(File_Remove(m_Dosbox.base));
    }
    const STAGED_FILE *const staged = m_Dosbox.staged;
    for (size_t i = 0; i < m_Dosbox.staged_count; i++) {
        char *const path = M_WorkPath(staged[i].path);
        if (M_FileHolds(path, staged[i].data, staged[i].size)) {
            MUST(File_Remove(path));
        }
    }
    return M_RemoveMarkers(m_Dosbox.work);
}

static int M_OnShutdown(void *const context, const int status)
{
    if (m_Dosbox.driver != nullptr) {
        LOG_ERROR("the game quit before the run ended");
        m_Dosbox.status = M_Close(false);
    }
    if (!Result_Log(M_CleanWork(), LOG_LEVEL_ERROR)) {
        m_Dosbox.status = 2;
    }
    Arena_Free(&m_Dosbox.arena);
    return m_Dosbox.status;
}

static const DOSBOX_GUEST m_Guest = {
    .on_start = M_OnStart,
    .on_exec = M_OnExec,
    .on_exit = M_OnExit,
    .on_site = M_OnSite,
    .on_timer = M_OnTimer,
    .on_shutdown = M_OnShutdown,
};

static RESULT M_Open(const int argc, char **const argv)
{
    const DOSBOX_PORT *const port = m_Dosbox.port;
    char program[32];
    snprintf(program, sizeof(program), "%sdosbox", port->port.game->id);
    bool help;
    MUST(Run_Parse(program, argc, argv, &m_Dosbox.arena, &m_Dosbox.run, &help));
    FAIL_IF(help, "the library takes no --help");
    FAIL_IF(m_Dosbox.run.live, "the dosbox backend takes no --live");
    m_Dosbox.work = Path_Join(
        Arena_Allocator(&m_Dosbox.arena), m_Dosbox.run.out_dir, "work");
    MUST(Driver_Open(&port->port, &m_Dosbox.run, "dosbox", &m_Dosbox.driver));
    MUST(File_MakeDir(m_Dosbox.work));
    MUST(M_LinkBase());
    return M_WriteStaged();
}

const DOSBOX_GUEST *Dosbox_Start(
    const DOSBOX_PORT *const port, const DOSBOX_HOST *const host,
    const int argc, char **const argv)
{
    memset(&m_Dosbox, 0, sizeof(m_Dosbox));
    m_Dosbox.port = port;
    m_Dosbox.host = host;
    m_Dosbox.status = 2;
    m_Dosbox.delay_site = M_NO_SITE;
    m_Dosbox.last_wait = -1;
    m_Dosbox.polling = -1;
    Arena_Init(&m_Dosbox.arena, "dosbox run", M_ARENA_SIZE);
    if (host->abi != DOSBOX_HOOK_ABI) {
        LOG_ERROR(
            "the emulator has hook ABI %u, not %u", host->abi, DOSBOX_HOOK_ABI);
        return nullptr;
    }
    const RESULT result = M_Open(argc, argv);
    if (!Result_Log(result, LOG_LEVEL_ERROR)) {
        if (m_Dosbox.driver != nullptr) {
            M_Close(false);
        }
        Arena_Free(&m_Dosbox.arena);
        return nullptr;
    }
    return &m_Guest;
}

int32_t Dosbox_Program(void)
{
    if (m_Dosbox.running_count == 0) {
        return -1;
    }
    return m_Dosbox.running[m_Dosbox.running_count - 1].program;
}

uint16_t Dosbox_LoadSegment(void)
{
    if (m_Dosbox.running_count == 0) {
        return 0;
    }
    return (uint16_t)(m_Dosbox.running[m_Dosbox.running_count - 1].psp + 0x10);
}

int32_t Dosbox_LastWait(void)
{
    return m_Dosbox.last_wait;
}

uint32_t Dosbox_Buttons(void)
{
    return m_Dosbox.held;
}

void Dosbox_Read(
    const uint16_t segment, const uint16_t offset, void *const out,
    const size_t size)
{
    m_Dosbox.host->read(M_Linear(segment, offset), out, size);
}

void Dosbox_Write(
    const uint16_t segment, const uint16_t offset, const void *const data,
    const size_t size)
{
    m_Dosbox.host->write(M_Linear(segment, offset), data, size);
}

const START_VALUE *Dosbox_Starts(size_t *const count)
{
    return Driver_Starts(m_Dosbox.driver, count);
}

DOSBOX_REGS Dosbox_Regs(void)
{
    DOSBOX_REGS regs;
    m_Dosbox.host->get_regs(&regs);
    return regs;
}
