#include "addresses.h"
#include "dosbox.h"
#include "dosbox_state.h"
#include "files.h"
#include "harness.h"

#include "core/base/utils.h"

// Describes th01 to the dosbox backend and runs it as th01dosbox.so. The
// VSyncs follow the native Frame_Advance, and a delay waits its
// milliseconds of emulated time, as TC4's delay() on the ARTIC counter does.

static const char *const m_Programs[] = {
#define M_PROGRAM(program, name, sha1) [program] = name,
    RETAIL_PROGRAMS(M_PROGRAM)
#undef M_PROGRAM
};

static const DOSBOX_SITE m_Sites[] = {
#define M_SITE(program, site, kind, segment, offset)                           \
    { program,                                                                 \
      DOSBOX_SITE_##kind,                                                      \
      segment,                                                                 \
      offset,                                                                  \
      DOSBOX_SITE_##kind == DOSBOX_SITE_DELAY,                                 \
      site },
    RETAIL_SITES(M_SITE)
#undef M_SITE
};

#define M_MASK(name, mask) M_##name = mask,
enum { TH01_BUTTONS(M_MASK) };
#undef M_MASK

static const DOSBOX_KEY m_Keys[] = {
    { M_up, 0x3a, 0 },
    { M_down, 0x3d, 0 },
    { M_left, 0x3b, 0 },
    { M_right, 0x3c, 0 },
    { M_keypad_up, 0x43, '8' },
    { M_keypad_down, 0x4b, '2' },
    { M_keypad_left, 0x46, '4' },
    { M_keypad_right, 0x48, '6' },
    { M_shot, 0x29, 'z' },
    { M_strike, 0x2a, 'x' },
    { M_ok, 0x1c, '\r' },
    { M_cancel, 0x00, 0x1b },
    { M_memory_enter, 0x36, 0 },
    { M_memory_leave, 0x37, 0 },
};

static const char *const m_Private[] = {
    "REIIDEN.CFG", "REYHIES.DAT", "REYHINO.DAT", "REYHIHA.DAT", "REYHILU.DAT",
};

static TH01_DOSBOX m_Th01;

static bool M_Idle(void *const context)
{
    const TH01_DOSBOX *const th01 = context;
    return th01->menu_frame == th01->frames;
}

static void M_Seed(void *const context)
{
    TH01_DOSBOX *const th01 = context;
    *th01 = (TH01_DOSBOX) { .menu_frame = UINT32_MAX };
}

static bool M_Advance(void *const context)
{
    TH01_DOSBOX *const th01 = context;
    th01->frames++;
    return Frame_Advance(&th01->clock);
}

// Reads the delay's milliseconds above its far return address.
static uint64_t M_DelayTime(void *const context, const uint8_t id)
{
    const DOSBOX_REGS regs = Dosbox_Regs();
    uint16_t ms = 0;
    Dosbox_Read(regs.ss, (uint16_t)(regs.sp + 4), &ms, sizeof(ms));
    return (uint64_t)ms * 1000;
}

static void M_OnSite(void *const context, const uint8_t id)
{
    TH01_DOSBOX *const th01 = context;
    if (id == RETAIL_SITE_OP_MENU) {
        th01->menu_frame = th01->frames;
    }
}

static const DOSBOX_PORT m_Port = {
    .port = {
        .game = &g_Th01Harness,
        .context = &m_Th01,
        .sample = Th01Dosbox_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
        .files = g_Th01Files,
        .file_count = ARRAY_SIZE(g_Th01Files),
    },
    .programs = m_Programs,
    .program_count = ARRAY_SIZE(m_Programs),
    .sites = m_Sites,
    .site_count = ARRAY_SIZE(m_Sites),
    .keys = m_Keys,
    .key_count = ARRAY_SIZE(m_Keys),
    .private_paths = m_Private,
    .private_count = ARRAY_SIZE(m_Private),
    .advance = M_Advance,
    .delay_time = M_DelayTime,
    .on_site = M_OnSite,
};

[[gnu::visibility("default")]] const DOSBOX_GUEST *Dosbox_Attach(
    const DOSBOX_HOST *const host, const int argc, char **const argv)
{
    return Dosbox_Start(&m_Port, host, argc, argv);
}
