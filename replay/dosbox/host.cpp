// Implements hook.h's host side inside DOSBox-X, as host.h gives it. The
// build copies this file into the DOSBox-X tree with hook.h and host.h.

#include "dosbox.h"
#include "cpu.h"
#include "dos_inc.h"
#include "mem.h"
#include "pic.h"
#include "regs.h"
#include "cpu/lazyflags.h"
#include "dos/drives.h"

#include "hook.h"
#include "host.h"

#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

// Covers every real-mode linear address, up to FFFF:FFFF.
#define M_SITE_LIMIT 0x110000u
#define M_NO_SITE 0xffffffffu

extern bool GDC_vsync_interrupt;
extern Bitu cycle_count;
bool PIC_IRQRequested(Bitu irq);
void runMount(const char *str);

bool g_HarnessSites = false;
bool g_HarnessOwnsVsync = false;
bool g_HarnessVblank = false;

static const DOSBOX_GUEST *m_Guest = nullptr;
static uint8_t m_Sites[M_SITE_LIMIT / 8];
static uint32_t m_SiteCount = 0;
// Holds the site that last ran on_site and the instruction count then, so
// that the instruction runs once on_site returns.
static uint32_t m_Fired = M_NO_SITE;
static Bitu m_FiredCount = 0;
static bool m_RegsSet = false;
static bool m_Halted = false;
static pic_tickindex_t m_HaltUntil = 0;
static bool m_Quit = false;
static int m_Status = 0;
static bool m_InStart = false;
static pic_tickindex_t m_TimerPeriod = 0;

static void M_Read(uint32_t address, void *out, size_t size)
{
    MEM_BlockRead(address, out, size);
}

static void M_Write(uint32_t address, const void *data, size_t size)
{
    MEM_BlockWrite(address, data, size);
}

static void M_GetRegs(DOSBOX_REGS *out)
{
    FillFlags();
    out->ax = reg_ax;
    out->bx = reg_bx;
    out->cx = reg_cx;
    out->dx = reg_dx;
    out->si = reg_si;
    out->di = reg_di;
    out->bp = reg_bp;
    out->sp = reg_sp;
    out->cs = SegValue(cs);
    out->ds = SegValue(ds);
    out->es = SegValue(es);
    out->ss = SegValue(ss);
    out->ip = reg_ip;
    out->flags = (uint16_t)reg_flags;
}

static void M_SetRegs(const DOSBOX_REGS *regs)
{
    reg_ax = regs->ax;
    reg_bx = regs->bx;
    reg_cx = regs->cx;
    reg_dx = regs->dx;
    reg_si = regs->si;
    reg_di = regs->di;
    reg_bp = regs->bp;
    reg_sp = regs->sp;
    SegSet16(cs, regs->cs);
    SegSet16(ds, regs->ds);
    SegSet16(es, regs->es);
    SegSet16(ss, regs->ss);
    reg_eip = regs->ip;
    reg_flags = (reg_flags & ~0xffffu) | regs->flags | 2u;
    m_RegsSet = true;
}

static void M_SetSite(uint32_t address)
{
    if (address >= M_SITE_LIMIT) {
        E_Exit("harness: site %08x out of range", (unsigned)address);
    }
    const uint8_t bit = (uint8_t)(1u << (address & 7));
    if ((m_Sites[address >> 3] & bit) == 0) {
        m_Sites[address >> 3] |= bit;
        m_SiteCount++;
    }
    g_HarnessSites = true;
}

static void M_ClearSite(uint32_t address)
{
    if (address >= M_SITE_LIMIT) {
        return;
    }
    const uint8_t bit = (uint8_t)(1u << (address & 7));
    if ((m_Sites[address >> 3] & bit) != 0) {
        m_Sites[address >> 3] &= (uint8_t)~bit;
        m_SiteCount--;
    }
    g_HarnessSites = m_SiteCount != 0;
}

static void M_OwnVsync(void)
{
    g_HarnessOwnsVsync = true;
    g_HarnessVblank = false;
}

static void M_RaiseVsync(void)
{
    g_HarnessVblank = true;
    if (GDC_vsync_interrupt) {
        GDC_vsync_interrupt = false;
        PIC_ActivateIRQ(2);
    }
}

static void M_ClearVblank(void)
{
    g_HarnessVblank = false;
}

static bool M_VsyncPending(void)
{
    return PIC_IRQRequested(2);
}

// Waits as HLT_Decode does, and also ends the wait at the deadline.
static Bits M_HaltDecode(void)
{
    if (reg_eip != cpu.hlt.eip || SegValue(cs) != cpu.hlt.cs
        || PIC_FullIndex() >= m_HaltUntil) {
        cpudecoder = cpu.hlt.old_decoder;
        m_Fired = M_NO_SITE;
    } else {
        CPU_IODelayRemoved += CPU_Cycles;
        CPU_Cycles = 0;
    }
    return 0;
}

static void M_Halt(uint64_t deadline)
{
    m_HaltUntil = (pic_tickindex_t)deadline / 1000.0;
    m_Halted = true;
}

static uint64_t M_Now(void)
{
    return (uint64_t)floor(PIC_FullIndex() * 1000.0);
}

static bool M_Mount(
    char drive, const char *base, const char *overlay,
    const char *const *hidden, size_t count)
{
    if (!m_InStart || drive < 'A' || drive > 'Z') {
        return false;
    }
    const std::string letter(1, drive);
    runMount((letter + " \"" + base + "\" -q").c_str());
    runMount((letter + " \"" + overlay + "\" -t overlay -q").c_str());
    Overlay_Drive *const mounted
        = dynamic_cast<Overlay_Drive *>(Drives[drive - 'A']);
    if (mounted == nullptr) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        mounted->Hide(hidden[i]);
    }
    return true;
}

static void M_Quit(int status)
{
    m_Quit = true;
    m_Status = status;
}

static void M_RenderPixels(uint32_t *pixels)
{
    Harness_Render(pixels);
}

static void M_CheckQuit(void);

static void M_TimerEvent(Bitu val)
{
    if (m_Guest == nullptr || m_TimerPeriod <= 0) {
        return;
    }
    PIC_AddEvent(M_TimerEvent, m_TimerPeriod);
    if (m_Guest->on_timer != nullptr) {
        m_Guest->on_timer(m_Guest->context);
        M_CheckQuit();
    }
}

static void M_SetTimer(uint64_t period)
{
    PIC_RemoveEvents(M_TimerEvent);
    m_TimerPeriod = (pic_tickindex_t)period / 1000.0;
    if (period != 0) {
        PIC_AddEvent(M_TimerEvent, m_TimerPeriod);
    }
}

static const DOSBOX_HOST m_Host = {
    DOSBOX_HOOK_ABI,
    M_Read,
    M_Write,
    M_GetRegs,
    M_SetRegs,
    M_SetSite,
    M_ClearSite,
    M_OwnVsync,
    M_RaiseVsync,
    M_ClearVblank,
    M_VsyncPending,
    M_Halt,
    M_Now,
    M_RenderPixels,
    M_Mount,
    M_Quit,
    M_SetTimer,
    Harness_ReadGraphics,
};

// Ends emulation through DOSBox-X's kill switch once quit was called.
static void M_CheckQuit(void)
{
    if (m_Quit) {
        throw int(0);
    }
}

void Harness_Load(int *argc, char **argv)
{
    const char *library = nullptr;
    int out = 1;
    int guest_argc = 0;
    char **guest_argv = nullptr;
    for (int i = 1; i < *argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            guest_argc = *argc - i - 1;
            guest_argv = argv + i + 1;
            break;
        }
        if (strcmp(argv[i], "-harness") == 0 && i + 1 < *argc) {
            library = argv[++i];
            continue;
        }
        argv[out++] = argv[i];
    }
    // Keeps the guest's arguments after the emulator's, past the end that
    // DOSBox-X reads.
    if (guest_argv != nullptr) {
        memmove(argv + out, guest_argv, sizeof(*argv) * (size_t)guest_argc);
        guest_argv = argv + out;
    }
    *argc = out;
    argv[out + guest_argc] = nullptr;
    if (library == nullptr) {
        return;
    }
    void *const handle = dlopen(library, RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        fprintf(stderr, "harness: %s\n", dlerror());
        exit(2);
    }
    const DOSBOX_ATTACH_FN attach
        = (DOSBOX_ATTACH_FN)dlsym(handle, DOSBOX_ATTACH_SYMBOL);
    if (attach == nullptr) {
        fprintf(stderr, "harness: %s has no %s\n", library,
            DOSBOX_ATTACH_SYMBOL);
        exit(2);
    }
    m_Guest = attach(&m_Host, guest_argc, guest_argv);
    if (m_Guest == nullptr) {
        exit(2);
    }
}

bool Harness_Site(uint32_t address)
{
    if (address >= M_SITE_LIMIT
        || (m_Sites[address >> 3] & (1u << (address & 7))) == 0) {
        return false;
    }
    if (address == m_Fired && cycle_count == m_FiredCount) {
        m_Fired = M_NO_SITE;
        return false;
    }
    m_Fired = address;
    m_FiredCount = cycle_count;
    m_RegsSet = false;
    m_Halted = false;
    FillFlags();
    m_Guest->on_site(m_Guest->context, address);
    M_CheckQuit();
    if (m_Halted) {
        m_Halted = false;
        CPU_IODelayRemoved += CPU_Cycles;
        CPU_Cycles = 0;
        cpu.hlt.cs = SegValue(cs);
        cpu.hlt.eip = reg_eip;
        cpu.hlt.old_decoder = cpudecoder;
        cpudecoder = &M_HaltDecode;
        return true;
    }
    return m_RegsSet;
}

void Harness_Start(void)
{
    if (m_Guest != nullptr && m_Guest->on_start != nullptr) {
        m_InStart = true;
        m_Guest->on_start(m_Guest->context);
        m_InStart = false;
        M_CheckQuit();
    }
}

void Harness_Exec(const char *path, uint16_t psp)
{
    if (m_Guest == nullptr || m_Guest->on_exec == nullptr) {
        return;
    }
    const char *name = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/' || *p == ':') {
            name = p + 1;
        }
    }
    char upper[16] = {};
    for (size_t i = 0; i + 1 < sizeof(upper) && name[i] != '\0'; i++) {
        upper[i] = (char)toupper((unsigned char)name[i]);
    }
    m_Guest->on_exec(m_Guest->context, upper, psp);
    M_CheckQuit();
}

void Harness_Exit(uint16_t psp, uint8_t status)
{
    if (m_Guest != nullptr && m_Guest->on_exit != nullptr) {
        m_Guest->on_exit(m_Guest->context, psp, status);
        M_CheckQuit();
    }
}

int Harness_Shutdown(int status)
{
    if (m_Quit) {
        status = m_Status;
    }
    if (m_Guest != nullptr && m_Guest->on_shutdown != nullptr) {
        status = m_Guest->on_shutdown(m_Guest->context, status);
    }
    m_Guest = nullptr;
    return status;
}
