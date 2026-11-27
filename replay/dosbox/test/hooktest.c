// Checks every call of hook.h against the patched DOSBox-X: hooktest.asm
// runs as HOOKTEST.COM on drive C, which mounts BASE with OVERLAY over it
// and SECRET.TXT hidden. Takes BASE, OVERLAY and an output PPM file after
// "--", prints one line per check and exits with status 0 when every check
// passes.

#include "hook.h"

#include <stdio.h>
#include <string.h>

#define M_HALT_US 100000
#define M_FRAME_US 16667
#define M_FRAMES 5
#define M_EXIT_STATUS 7
#define M_REGS_VALUE 0x1234
#define M_FLAG_ZERO 0x40

// Mirrors the table at 102h of hooktest.asm.
typedef struct {
    uint16_t site_regs;
    uint16_t site_vsync;
    uint16_t site_vblank_end;
    uint16_t regs_value;
    uint16_t frames;
    uint16_t irqs;
    uint16_t limit;
    uint16_t site_timer_a_start;
    uint16_t site_timer_a_end;
    uint16_t site_timer_b_start;
    uint16_t site_timer_b_end;
} M_TABLE;

typedef struct {
    const DOSBOX_HOST *host;
    const char *base;
    const char *overlay;
    const char *shot;
    bool mounted;
    uint16_t psp;
    M_TABLE table;
    int failures;
    int checks;
    bool finished;
    // The VSync wait's state: the first wait halts with no VSync, each
    // later one halts a frame and then raises.
    bool first_wait_done;
    bool halting;
    bool raised;
    uint64_t halt_start;
    uint64_t timer_start;
    int wakes;
    int raises;
    int vblank_ends;
    bool pending_after_raise;
} M_TEST;

static M_TEST m_Test;
static uint32_t m_Pixels[640 * 400];
static uint8_t m_Graphics[2][640 * 400];
static uint8_t m_Palette[16 * 4];

static void M_Check(const bool ok, const char *what)
{
    m_Test.checks++;
    if (!ok) {
        m_Test.failures++;
    }
    printf("hooktest: %s %s\n", ok ? "ok  " : "FAIL", what);
    fflush(stdout);
}

static uint32_t M_Linear(const uint16_t offset)
{
    return (uint32_t)m_Test.psp * 16 + offset;
}

static uint16_t M_ReadWord(const uint16_t offset)
{
    uint16_t value;
    m_Test.host->read(M_Linear(offset), &value, sizeof(value));
    return value;
}

static void M_OnStart(void *context)
{
    static const char *const hidden[] = { "SECRET.TXT" };
    m_Test.mounted =
        m_Test.host->mount('C', m_Test.base, m_Test.overlay, hidden, 1);
    M_Check(m_Test.mounted, "mount");
}

static void M_OnExec(void *context, const char *name, const uint16_t psp)
{
    if (strcmp(name, "HOOKTEST.COM") != 0) {
        return;
    }
    m_Test.psp = psp;
    m_Test.host->read(M_Linear(0x102), &m_Test.table, sizeof(m_Test.table));
    M_Check(m_Test.table.site_regs > 0x100, "exec and read");
    const uint16_t limit = M_FRAMES;
    m_Test.host->write(M_Linear(m_Test.table.limit), &limit, sizeof(limit));
    m_Test.host->set_site(M_Linear(m_Test.table.site_regs));
    m_Test.host->set_site(M_Linear(m_Test.table.site_vsync));
    m_Test.host->set_site(M_Linear(m_Test.table.site_vblank_end));
    m_Test.host->set_site(M_Linear(m_Test.table.site_timer_a_start));
    m_Test.host->set_site(M_Linear(m_Test.table.site_timer_a_end));
    m_Test.host->set_site(M_Linear(m_Test.table.site_timer_b_start));
    m_Test.host->set_site(M_Linear(m_Test.table.site_timer_b_end));
    m_Test.host->own_vsync();
}

static void M_OnRegsSite(void)
{
    DOSBOX_REGS regs;
    m_Test.host->get_regs(&regs);
    M_Check(
        regs.cs == m_Test.psp && regs.ip == m_Test.table.site_regs
            && regs.ax == 0,
        "get_regs");
    regs.ax = M_REGS_VALUE;
    m_Test.host->set_regs(&regs);
}

static void M_OnVsyncSite(void)
{
    DOSBOX_REGS regs;
    m_Test.host->get_regs(&regs);
    if (m_Test.raised || (regs.flags & M_FLAG_ZERO) == 0) {
        return;
    }
    const uint64_t now = m_Test.host->now();
    if (!m_Test.first_wait_done && !m_Test.halting) {
        m_Test.halting = true;
        m_Test.halt_start = now;
    }
    // An interrupt ends a halt early, so the wait halts again until the
    // deadline.
    if (!m_Test.first_wait_done && now < m_Test.halt_start + M_HALT_US) {
        m_Test.wakes++;
        m_Test.host->halt(m_Test.halt_start + M_HALT_US);
        return;
    }
    if (!m_Test.first_wait_done) {
        printf("hooktest: halted %d times\n", m_Test.wakes);
        M_Check(m_Test.wakes < 1000, "halt until deadline");
        M_Check(
            M_ReadWord(m_Test.table.frames) == 0
                && M_ReadWord(m_Test.table.irqs) == 0,
            "own_vsync stops the emulator's VSync");
        m_Test.first_wait_done = true;
        m_Test.halting = false;
    }
    if (!m_Test.halting) {
        m_Test.halting = true;
        m_Test.host->halt(now + M_FRAME_US);
        return;
    }
    m_Test.halting = false;
    m_Test.raised = true;
    m_Test.raises++;
    m_Test.host->raise_vsync();
    if (m_Test.raises == 1) {
        m_Test.pending_after_raise = m_Test.host->vsync_pending();
    }
}

static bool M_IsBlue(const uint32_t pixel)
{
    return (pixel & 0xffff00) == 0 && (pixel & 0xff) != 0;
}

static void M_WriteShot(void)
{
    FILE *const file = fopen(m_Test.shot, "wb");
    if (file == nullptr) {
        return;
    }
    fprintf(file, "P6\n640 400\n255\n");
    for (size_t i = 0; i < 640 * 400; i++) {
        const uint8_t rgb[3] = { (uint8_t)(m_Pixels[i] >> 16),
                                 (uint8_t)(m_Pixels[i] >> 8),
                                 (uint8_t)m_Pixels[i] };
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}

static void M_Render(void)
{
    m_Test.host->read_graphics(m_Graphics[0], m_Palette);
    bool pages = true;
    for (unsigned x = 0; x < 8; x++) {
        pages = pages && m_Graphics[0][200 * 640 + x] == 1
            && m_Graphics[1][200 * 640 + x] == 0
            && m_Graphics[0][200 * 640 + 16 + x] == 0
            && m_Graphics[1][200 * 640 + 16 + x] == 2;
    }
    M_Check(pages, "read both raw graphics pages");
    M_Check(
        m_Palette[8] == 85 && m_Palette[9] == 51 && m_Palette[10] == 153
            && m_Palette[11] == 255,
        "read analog palette as RGBA");
    m_Test.host->render(m_Pixels);
    bool text = false;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 8; x++) {
            text = text || m_Pixels[y * 640 + x] == 0xffffff;
        }
    }
    bool graphics = true;
    for (int x = 0; x < 8; x++) {
        graphics = graphics && M_IsBlue(m_Pixels[200 * 640 + x]);
    }
    printf(
        "hooktest: pixel 0,200 %06x, 8,200 %06x, 320,300 %06x\n",
        (unsigned)m_Pixels[200 * 640], (unsigned)m_Pixels[200 * 640 + 8],
        (unsigned)m_Pixels[300 * 640 + 320]);
    M_Check(text, "render text layer");
    bool underlines = true;
    for (unsigned row = 5; row <= 23; row += 9) {
        const uint32_t *const line = m_Pixels + (row * 16 + 15) * 640;
        for (unsigned x = 36; x < 44; x++) {
            underlines =
                underlines && line[x] == (x < 40 ? 0xffffff : 0x00ff00);
        }
    }
    M_Check(underlines, "render underlines on each text row");
    M_Check(
        graphics && m_Pixels[200 * 640 + 8] == 0
            && m_Pixels[300 * 640 + 320] == 0,
        "render graphics layer");
    M_WriteShot();
}

static void M_OnVblankEndSite(void)
{
    DOSBOX_REGS regs;
    m_Test.host->get_regs(&regs);
    if ((regs.flags & M_FLAG_ZERO) != 0) {
        return;
    }
    m_Test.host->clear_vblank();
    m_Test.raised = false;
    m_Test.vblank_ends++;
    if (m_Test.vblank_ends == 3) {
        M_Render();
    }
}

static void M_TimerEnd(const uint32_t clocks, const char *const name)
{
    const uint64_t elapsed = m_Test.host->now() - m_Test.timer_start;
    const uint64_t expected = (uint64_t)clocks * 1000000 / 7987200;
    printf(
        "hooktest: %s elapsed %llu us, expected %llu us\n", name,
        (unsigned long long)elapsed, (unsigned long long)expected);
    M_Check(elapsed >= expected && elapsed <= expected + 10, name);
}

static void M_OnSite(void *context, const uint32_t address)
{
    if (address == M_Linear(m_Test.table.site_regs)) {
        M_OnRegsSite();
    } else if (address == M_Linear(m_Test.table.site_vsync)) {
        M_OnVsyncSite();
    } else if (address == M_Linear(m_Test.table.site_vblank_end)) {
        M_OnVblankEndSite();
    } else if (
        address == M_Linear(m_Test.table.site_timer_a_start)
        || address == M_Linear(m_Test.table.site_timer_b_start)) {
        m_Test.timer_start = m_Test.host->now();
    } else if (address == M_Linear(m_Test.table.site_timer_a_end)) {
        M_TimerEnd(144 * 1024, "YM2608 timer A");
    } else if (address == M_Linear(m_Test.table.site_timer_b_end)) {
        M_TimerEnd(2304 * 256, "YM2608 timer B");
    }
}

static bool M_FileHolds(const char *dir, const char *name, const char *text)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *const file = fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    char data[16] = {};
    const size_t size = fread(data, 1, sizeof(data) - 1, file);
    fclose(file);
    return text == nullptr || (size == strlen(text) && strcmp(data, text) == 0);
}

static void M_OnExit(void *context, const uint16_t psp, const uint8_t status)
{
    if (psp != m_Test.psp) {
        return;
    }
    const uint16_t frames = M_ReadWord(m_Test.table.frames);
    const uint16_t irqs = M_ReadWord(m_Test.table.irqs);
    printf(
        "hooktest: status %u, frames %u, irqs %u, raises %d\n",
        (unsigned)status, (unsigned)frames, (unsigned)irqs, m_Test.raises);
    M_Check(status == M_EXIT_STATUS, "exit status");
    M_Check(M_ReadWord(m_Test.table.regs_value) == M_REGS_VALUE, "set_regs");
    M_Check(
        frames == M_FRAMES && m_Test.raises == M_FRAMES,
        "frames follow raise_vsync");
    M_Check(irqs == M_FRAMES, "raise_vsync raises IRQ 2");
    M_Check(m_Test.pending_after_raise, "vsync_pending");
    M_Check(
        M_FileHolds(m_Test.overlay, "OUT.TXT", "1"),
        "hidden file and overlay write");
    M_Check(
        !M_FileHolds(m_Test.base, "OUT.TXT", nullptr), "base stays unchanged");
    m_Test.finished = true;
    m_Test.host->quit(m_Test.failures == 0 ? 0 : 1);
}

static int M_OnShutdown(void *context, const int status)
{
    M_Check(m_Test.finished, "quit");
    printf(
        "hooktest: %d of %d checks pass\n", m_Test.checks - m_Test.failures,
        m_Test.checks);
    return m_Test.failures == 0 && m_Test.finished ? status : 1;
}

static const DOSBOX_GUEST m_Guest = {
    .on_start = M_OnStart,
    .on_exec = M_OnExec,
    .on_exit = M_OnExit,
    .on_site = M_OnSite,
    .on_shutdown = M_OnShutdown,
};

[[gnu::visibility("default")]] const DOSBOX_GUEST *Dosbox_Attach(
    const DOSBOX_HOST *host, int argc, char **argv)
{
    if (host->abi != DOSBOX_HOOK_ABI || argc != 3) {
        fprintf(stderr, "hooktest: needs -- BASE OVERLAY SHOT\n");
        return nullptr;
    }
    m_Test.host = host;
    m_Test.base = argv[0];
    m_Test.overlay = argv[1];
    m_Test.shot = argv[2];
    return &m_Guest;
}
