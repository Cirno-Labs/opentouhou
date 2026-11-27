#include "replay/dosbox/dosbox.h"
#include "tests/test.h"
#include "tests/test_dir.h"

#include "core/base/utils.h"

#include <string.h>

// Drives the dosbox backend's library with a fake emulator: a guest that
// waits like Borland's frame_delay on a counter of its IRQ 2 handler, and
// the sites of the other kinds reached one at a time.

#define M_MEMORY_SIZE 0x110000
#define M_PSP 0x1000
#define M_LOAD ((M_PSP + 0x10) * 16)
#define M_TICK 8333

#define M_FLAG_CARRY 0x0001
#define M_FLAG_ZERO 0x0040

typedef enum {
    M_SITE_VSYNC = 0x10,
    M_SITE_POLL = 0x20,
    M_SITE_VBLANK_END = 0x30,
    M_SITE_DELAY = 0x40,
    M_SITE_CALL = 0x50,
} M_SITE;

static struct {
    uint8_t memory[M_MEMORY_SIZE];
    DOSBOX_REGS regs;
    uint64_t now;
    bool halted;
    uint64_t deadline;
    bool armed;
    bool pending;
    bool vblank;
    bool quit;
    int status;
    uint64_t timer;
    int32_t sites;
    // The guest's IRQ 2 counter.
    uint32_t count1;
    // What the port saw.
    uint32_t rows;
    uint32_t vsync_frames;
    uint32_t calls;
    // Counts the execs of FAKE.COM and holds the start value at the last.
    uint32_t execs;
    uint32_t start;
    uint8_t keys_seen;
    uint8_t keys_last;
} m_Fake;

static uint32_t m_Phase;

static void M_Read(const uint32_t address, void *const out, const size_t size)
{
    memcpy(out, m_Fake.memory + address, size);
}

static void M_Write(
    const uint32_t address, const void *const data, const size_t size)
{
    memcpy(m_Fake.memory + address, data, size);
}

static void M_GetRegs(DOSBOX_REGS *const out)
{
    *out = m_Fake.regs;
}

static void M_SetRegs(const DOSBOX_REGS *const regs)
{
    m_Fake.regs = *regs;
}

static void M_SetSite(const uint32_t address)
{
    m_Fake.sites++;
}

static void M_ClearSite(const uint32_t address)
{
    m_Fake.sites--;
}

static void M_OwnVsync(void)
{
}

static void M_RaiseVsync(void)
{
    m_Fake.vblank = true;
    m_Fake.pending = m_Fake.armed;
}

static void M_ClearVblank(void)
{
    m_Fake.vblank = false;
}

static bool M_VsyncPending(void)
{
    return m_Fake.pending;
}

static void M_Halt(const uint64_t deadline)
{
    m_Fake.halted = true;
    m_Fake.deadline = deadline;
}

static uint64_t M_Now(void)
{
    return m_Fake.now;
}

static void M_Render(uint32_t *const pixels)
{
    memset(pixels, 0, 640 * 400 * sizeof(*pixels));
}

static bool M_Mount(
    const char drive, const char *const base, const char *const overlay,
    const char *const *const hidden, const size_t count)
{
    return drive == 'C';
}

static void M_Quit(const int status)
{
    m_Fake.quit = true;
    m_Fake.status = status;
}

static void M_SetTimer(const uint64_t period)
{
    m_Fake.timer = period;
}

static const DOSBOX_HOST m_Host = {
    .abi = DOSBOX_HOOK_ABI,
    .read = M_Read,
    .write = M_Write,
    .get_regs = M_GetRegs,
    .set_regs = M_SetRegs,
    .set_site = M_SetSite,
    .clear_site = M_ClearSite,
    .own_vsync = M_OwnVsync,
    .raise_vsync = M_RaiseVsync,
    .clear_vblank = M_ClearVblank,
    .vsync_pending = M_VsyncPending,
    .halt = M_Halt,
    .now = M_Now,
    .render = M_Render,
    .mount = M_Mount,
    .quit = M_Quit,
    .set_timer = M_SetTimer,
};

static const BINDING m_Bindings[] = { { "shot", 0x1 } };
static const COLUMN m_Columns[] = { { "count1", COLUMN_U32 } };
static const COLUMN m_Starts[] = { { "level", COLUMN_U32 } };

static const HARNESS_GAME m_Game = {
    .id = "fake",
    .version = "1",
    .frame_rate = 60.0,
    .width = 640,
    .height = 400,
    .draw_frames = 1,
    .bindings = m_Bindings,
    .binding_count = ARRAY_SIZE(m_Bindings),
    .columns = m_Columns,
    .column_count = ARRAY_SIZE(m_Columns),
    .columns_version = 1,
    .starts = m_Starts,
    .start_count = ARRAY_SIZE(m_Starts),
};

static void M_Sample(void *const context, uint32_t *const row)
{
    row[0] = m_Fake.count1;
    m_Fake.rows++;
    m_Fake.keys_last = m_Fake.memory[0x52a + 5];
    m_Fake.keys_seen |= m_Fake.keys_last;
}

static bool M_Idle(void *const context)
{
    return false;
}

static void M_Seed(void *const context)
{
    m_Phase = 60000 - 56423;
}

static bool M_Advance(void *const context)
{
    m_Phase += 56423;
    if (m_Phase < 60000) {
        return false;
    }
    m_Phase -= 60000;
    m_Fake.vsync_frames++;
    return true;
}

static uint64_t M_DelayTime(void *const context, const uint8_t id)
{
    const DOSBOX_REGS regs = Dosbox_Regs();
    uint16_t ms;
    Dosbox_Read(regs.ss, (uint16_t)(regs.sp + 4), &ms, sizeof(ms));
    return (uint64_t)ms * 1000;
}

static void M_OnSite(void *const context, const uint8_t id)
{
    m_Fake.calls++;
}

static void M_OnExec(void *const context, const uint8_t program)
{
    size_t count;
    const START_VALUE *const starts = Dosbox_Starts(&count);
    m_Fake.execs++;
    m_Fake.start = count == 1 ? starts[0].value : 0;
}

static const char *const m_Programs[] = { "FAKE.COM" };

static const DOSBOX_SITE m_Sites[] = {
    { 0, DOSBOX_SITE_VSYNC, 0, M_SITE_VSYNC, false, 0 },
    { 0, DOSBOX_SITE_POLL, 0, M_SITE_POLL, false, 1 },
    { 0, DOSBOX_SITE_VBLANK_END, 0, M_SITE_VBLANK_END, false, 2 },
    { 0, DOSBOX_SITE_DELAY, 0, M_SITE_DELAY, true, 3 },
    { 0, DOSBOX_SITE_CALL, 0, M_SITE_CALL, false, 4 },
};

static const DOSBOX_KEY m_Keys[] = { { 0x1, 0x29, 'z' } };

static const DOSBOX_PORT m_Port = {
    .port = {
        .game = &m_Game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    },
    .programs = m_Programs,
    .program_count = ARRAY_SIZE(m_Programs),
    .sites = m_Sites,
    .site_count = ARRAY_SIZE(m_Sites),
    .keys = m_Keys,
    .key_count = ARRAY_SIZE(m_Keys),
    .advance = M_Advance,
    .delay_time = M_DelayTime,
    .on_site = M_OnSite,
    .on_exec = M_OnExec,
};

static void M_Write16(const uint32_t address, const uint16_t value)
{
    memcpy(m_Fake.memory + address, &value, sizeof(value));
}

static uint16_t M_Read16(const uint32_t address)
{
    uint16_t value;
    memcpy(&value, m_Fake.memory + address, sizeof(value));
    return value;
}

// Starts a capture of the script text in the directory of name and runs
// the guest's exec. Returns the guest.
static const DOSBOX_GUEST *M_Start(const char *const name, const char *script)
{
    memset(&m_Fake, 0, sizeof(m_Fake));
    m_Fake.armed = true;
    m_Fake.regs.cs = M_PSP;
    m_Fake.regs.ss = M_PSP;
    m_Fake.regs.sp = 0xfffe;
    M_Write16(0x524, 0x502);
    M_Write16(0x526, 0x502);
    m_Fake.memory[M_LOAD + M_SITE_VSYNC] = 0x72;
    m_Fake.memory[M_LOAD + M_SITE_POLL] = 0x75;
    m_Fake.memory[M_LOAD + M_SITE_VBLANK_END] = 0x75;

    char *const root = TestDir_Prepare(name);
    char *const script_path = TestDir_Join(root, "inputs.txt");
    char *const case_path = TestDir_Join(root, "case.othtc");
    char *const out = TestDir_Join(root, "out");
    TestDir_Write(script_path, script, strlen(script));
    char *argv[] = {
        "fakedosbox", "capture", case_path,  "--data",    root,
        "--out",      out,       "--script", script_path,
    };
    const DOSBOX_GUEST *const guest =
        Dosbox_Start(&m_Port, &m_Host, (int)ARRAY_SIZE(argv), argv);
    CHECK(guest != nullptr);
    guest->on_start(guest->context);
    guest->on_exec(guest->context, "FAKE.COM", M_PSP);
    return guest;
}

static void M_Deliver(void)
{
    if (m_Fake.pending) {
        m_Fake.pending = false;
        m_Fake.count1++;
    }
}

// Reaches the site at offset of the load image until the library lets the
// instruction run. A halt ends at its deadline, at the next 120 Hz tick or
// at once when a VSync waits.
static void M_Arrive(const DOSBOX_GUEST *const guest, const uint32_t offset)
{
    for (;;) {
        m_Fake.halted = false;
        guest->on_site(guest->context, M_LOAD + offset);
        if (m_Fake.quit || !m_Fake.halted) {
            break;
        }
        if (!m_Fake.pending) {
            const uint64_t tick = (m_Fake.now / M_TICK + 1) * M_TICK;
            m_Fake.now = tick < m_Fake.deadline ? tick : m_Fake.deadline;
        }
        M_Deliver();
    }
    M_Deliver();
}

// Waits for one VSync as frame_delay(1) does.
static void M_FrameDelay(const DOSBOX_GUEST *const guest)
{
    m_Fake.count1 = 0;
    for (;;) {
        M_Deliver();
        if (m_Fake.quit || m_Fake.count1 >= 1) {
            return;
        }
        m_Fake.regs.flags = M_FLAG_CARRY;
        M_Arrive(guest, M_SITE_VSYNC);
    }
}

// Returns the emulated time at the end of frame.
static uint64_t M_FrameEnd(const uint32_t frame)
{
    return (uint64_t)((frame + 1) * 1e6 / 60.0);
}

TEST(frames_end_at_vsync_waits_on_the_virtual_clock)
{
    const DOSBOX_GUEST *const guest =
        M_Start("dosbox_frames", "frames 12\ninput 3 shot\ninput 5 none\n");
    uint32_t loops = 0;
    while (!m_Fake.quit && loops < 100) {
        M_FrameDelay(guest);
        loops += !m_Fake.quit;
    }
    CHECK(m_Fake.quit);
    CHECK_EQ_INT(m_Fake.status, 0);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(11));
    CHECK_EQ_INT(m_Fake.rows, m_Fake.vsync_frames);
    CHECK(m_Fake.rows < 12);
    CHECK_EQ_INT(loops, m_Fake.rows);
    CHECK_EQ_INT(m_Fake.keys_seen, 0x02);
    CHECK_EQ_INT(m_Fake.keys_last, 0);
    CHECK_EQ_INT(M_Read16(0x502), 0x297a);
    CHECK_EQ_INT(M_Read16(0x526), 0x504);
    CHECK_EQ_INT(m_Fake.memory[0x528], 1);
    CHECK_EQ_INT(Dosbox_LastWait(), 0);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 0);
}

// Calls the far delay of ms with the return address 2345:1234.
static void M_CallDelay(const DOSBOX_GUEST *const guest, const uint16_t ms)
{
    m_Fake.regs.cs = M_PSP;
    m_Fake.regs.sp = 0xfff0;
    M_Write16(M_PSP * 16 + 0xfff0, 0x1234);
    M_Write16(M_PSP * 16 + 0xfff2, 0x2345);
    M_Write16(M_PSP * 16 + 0xfff4, ms);
    m_Fake.regs.ip = M_SITE_DELAY;
    M_Arrive(guest, M_SITE_DELAY);
    CHECK_EQ_INT(m_Fake.regs.ip, 0x1234);
    CHECK_EQ_INT(m_Fake.regs.cs, 0x2345);
    CHECK_EQ_INT(m_Fake.regs.sp, 0xfff4);
}

TEST(a_delay_waits_its_time_and_returns_far)
{
    const DOSBOX_GUEST *const guest = M_Start("dosbox_delay", "frames 100\n");
    M_FrameDelay(guest);
    M_FrameDelay(guest);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(1));
    const uint32_t rows = m_Fake.rows;
    M_CallDelay(guest, 10);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(1) + 10000);
    CHECK_EQ_INT(m_Fake.rows, rows);
    M_FrameDelay(guest);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(2));
    M_CallDelay(guest, 50);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(5));
    CHECK_EQ_INT(m_Fake.rows, rows + 3);
    CHECK(!m_Fake.quit);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(back_to_back_delays_leave_the_work_between_them_out)
{
    const DOSBOX_GUEST *const guest = M_Start("dosbox_chain", "frames 100\n");
    M_FrameDelay(guest);
    M_FrameDelay(guest);
    M_CallDelay(guest, 40);
    const uint64_t first = M_FrameEnd(1) + 40000;
    CHECK_EQ_INT(m_Fake.now, first);
    m_Fake.now += 5000;
    M_CallDelay(guest, 40);
    CHECK_EQ_INT(m_Fake.now, first + 40000);
    M_FrameDelay(guest);
    const uint64_t waited = m_Fake.now;
    m_Fake.now += 5000;
    M_CallDelay(guest, 10);
    CHECK_EQ_INT(m_Fake.now, waited + 15000);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(polls_end_one_frame_each_and_other_sites_none)
{
    const DOSBOX_GUEST *const guest = M_Start("dosbox_poll", "frames 100\n");
    M_FrameDelay(guest);
    CHECK_EQ_INT(m_Fake.now, 0);
    m_Fake.regs.flags = 0;
    M_Arrive(guest, M_SITE_POLL);
    M_Arrive(guest, M_SITE_POLL);
    M_Arrive(guest, M_SITE_POLL);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(1));
    m_Fake.regs.flags = M_FLAG_ZERO;
    M_Arrive(guest, M_SITE_POLL);
    M_Arrive(guest, M_SITE_VBLANK_END);
    M_Arrive(guest, M_SITE_CALL);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(1));
    CHECK_EQ_INT(m_Fake.calls, 1);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(a_poll_lets_its_jump_run_once_after_a_vsync_wait)
{
    const DOSBOX_GUEST *const guest =
        M_Start("dosbox_poll_start", "frames 100\n");
    M_FrameDelay(guest);
    m_Fake.regs.flags = 0;
    m_Fake.halted = false;
    guest->on_site(guest->context, M_LOAD + M_SITE_POLL);
    CHECK(!m_Fake.halted);
    guest->on_site(guest->context, M_LOAD + M_SITE_POLL);
    CHECK(m_Fake.halted);
    M_Arrive(guest, M_SITE_POLL);
    CHECK_EQ_INT(m_Fake.now, M_FrameEnd(0));
    M_FrameDelay(guest);
    M_FrameDelay(guest);
    const uint64_t now = m_Fake.now;
    m_Fake.regs.flags = 0;
    m_Fake.halted = false;
    guest->on_site(guest->context, M_LOAD + M_SITE_POLL);
    CHECK(!m_Fake.halted);
    CHECK_EQ_INT(m_Fake.now, now);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(a_wait_lets_a_pending_vsync_in_before_it_halts)
{
    const DOSBOX_GUEST *const guest = M_Start("dosbox_pending", "frames 100\n");
    M_FrameDelay(guest);
    m_Fake.pending = true;
    m_Fake.regs.flags = M_FLAG_CARRY;
    m_Fake.halted = false;
    guest->on_site(guest->context, M_LOAD + M_SITE_VSYNC);
    CHECK(!m_Fake.halted);
    CHECK_EQ_INT(m_Fake.now, 0);
    for (int32_t i = 0; i < 1000 && !m_Fake.halted; i++) {
        guest->on_site(guest->context, M_LOAD + M_SITE_VSYNC);
    }
    CHECK(m_Fake.halted);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(a_wait_lets_its_jump_run_once_after_the_vsync)
{
    const DOSBOX_GUEST *const guest = M_Start("dosbox_stale", "frames 100\n");
    M_FrameDelay(guest);
    CHECK_EQ_INT(m_Fake.count1, 1);
    const uint64_t now = m_Fake.now;
    m_Fake.regs.flags = M_FLAG_CARRY;
    m_Fake.halted = false;
    guest->on_site(guest->context, M_LOAD + M_SITE_VSYNC);
    CHECK(!m_Fake.halted);
    guest->on_site(guest->context, M_LOAD + M_SITE_VSYNC);
    CHECK(m_Fake.halted);
    CHECK_EQ_INT(m_Fake.now, now);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(a_vblank_end_wait_clears_the_vblank)
{
    const DOSBOX_GUEST *const guest = M_Start("dosbox_vblank", "frames 100\n");
    M_FrameDelay(guest);
    CHECK(m_Fake.vblank);
    m_Fake.regs.flags = 0;
    M_Arrive(guest, M_SITE_VBLANK_END);
    CHECK(!m_Fake.vblank);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(sites_follow_the_running_program)
{
    const DOSBOX_GUEST *const guest =
        M_Start("dosbox_exec", "frames 100\nstart level 7\n");
    CHECK_EQ_INT(m_Fake.execs, 1);
    CHECK_EQ_INT(m_Fake.start, 7);
    CHECK_EQ_INT(m_Fake.sites, (int32_t)ARRAY_SIZE(m_Sites));
    CHECK_EQ_INT(m_Fake.timer, DOSBOX_WATCHDOG_TICK);
    CHECK_EQ_INT(Dosbox_Program(), 0);
    CHECK_EQ_INT(Dosbox_LoadSegment(), M_PSP + 0x10);
    guest->on_exec(guest->context, "OTHER.EXE", 0x2000);
    CHECK_EQ_INT(Dosbox_Program(), 0);
    CHECK_EQ_INT(m_Fake.execs, 1);
    guest->on_exec(guest->context, "FAKE.COM", M_PSP);
    CHECK_EQ_INT(m_Fake.execs, 2);
    CHECK_EQ_INT(Dosbox_Program(), 0);
    CHECK_EQ_INT(m_Fake.sites, (int32_t)ARRAY_SIZE(m_Sites));
    guest->on_exit(guest->context, M_PSP, 0);
    CHECK_EQ_INT(m_Fake.sites, 0);
    CHECK_EQ_INT(Dosbox_Program(), -1);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}

TEST(the_watchdog_fails_a_run_with_no_site)
{
    const DOSBOX_GUEST *const guest =
        M_Start("dosbox_watchdog", "frames 100\n");
    M_FrameDelay(guest);
    m_Fake.regs.ip = 0x0123;
    const uint64_t start = m_Fake.now;
    while (!m_Fake.quit && m_Fake.now - start < 2 * DOSBOX_WATCHDOG) {
        m_Fake.now += m_Fake.timer;
        guest->on_timer(guest->context);
    }
    CHECK(m_Fake.quit);
    CHECK_EQ_INT(m_Fake.status, 2);
    CHECK(m_Fake.now - start >= DOSBOX_WATCHDOG);
    CHECK(m_Fake.now - start < DOSBOX_WATCHDOG + DOSBOX_WATCHDOG_TICK);
    CHECK_EQ_INT(guest->on_shutdown(guest->context, 0), 2);
}
