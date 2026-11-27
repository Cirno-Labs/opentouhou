#include "dosbox.h"
#include "harness.h"
#include "state.h"

#include "core/base/utils.h"
#include "pc98/vsync.h"
#include "th02/clock.h"

#include <ctype.h>
#include <string.h>
#include <time.h>

static VSYNC_CLOCK m_Clock;
static bool m_Started;
static uint32_t m_PmdBase;
static bool m_ClockOrigin;
static uint32_t m_ClockOrdinals[TH02_CLOCK_KIND_COUNT];
static uint32_t m_EffectOrdinal;
static struct {
    uint32_t address;
    uint32_t key;
} m_PmdStart;
static uint8_t m_Graphics[2][640 * 400];
static uint8_t m_Palette[16 * 4];
// MAIN's last tile source index, as its source setup starts.
static uint8_t m_LastSource;
enum {
    M_SITE_RIKA = 21,
    M_SITE_MEIRA,
    M_SITE_STONES,
    M_SITE_MARISA,
    M_SITE_SIGMA,
    M_SITE_SIGMA_PHASE,
    M_SITE_MIMA_FIRST,
    M_SITE_MIMA_MIDDLE,
    M_SITE_MIMA_FOURTH,
    M_SITE_MIMA_FINAL,
    M_SITE_MEASURE = 32,
    M_SITE_TILE_SOURCES,
};
static const char *const m_Programs[] = { "ZUN.COM", "OP.EXE", "MAIN.EXE",
                                          "MAINE.EXE" };
static const DOSBOX_SITE m_Sites[] = {
    { 0, DOSBOX_SITE_VBLANK_END, 0, 0x058c, false, 0 },
    { 0, DOSBOX_SITE_VSYNC, 0, 0x0594, false, 1 },
    { 1, DOSBOX_SITE_VBLANK_END, 0, 0x269b, false, 2 },
    { 1, DOSBOX_SITE_VSYNC, 0, 0x26a5, false, 3 },
    { 1, DOSBOX_SITE_VSYNC, 0, 0x26af, false, 4 },
    { 1, DOSBOX_SITE_VSYNC, 0, 0xaeca, false, 5 },
    { 1, DOSBOX_SITE_VSYNC, 0, 0xb2e1, false, 6 },
    { 2, DOSBOX_SITE_VBLANK_END, 0, 0x2845, false, 7 },
    { 2, DOSBOX_SITE_VSYNC, 0, 0x284f, false, 8 },
    { 2, DOSBOX_SITE_VSYNC, 0, 0x2859, false, 9 },
    { 2, DOSBOX_SITE_VSYNC, 0, 0xf627, false, 10 },
    { 3, DOSBOX_SITE_VBLANK_END, 0, 0x2443, false, 11 },
    { 3, DOSBOX_SITE_VSYNC, 0, 0x244d, false, 12 },
    { 3, DOSBOX_SITE_VSYNC, 0, 0x2457, false, 13 },
    { 3, DOSBOX_SITE_VSYNC, 0, 0xb52f, false, 14 },
    { 2, DOSBOX_SITE_VSYNC, 0, 0xb8ba, false, 15 },
    { 2, DOSBOX_SITE_VSYNC, 0, 0xbef6, false, 16 },
    { 2, DOSBOX_SITE_POLL, 0, 0xf7f5, false, 17 },
    { 1, DOSBOX_SITE_CALL, 0, 0x9434, false, 18 },
    { 2, DOSBOX_SITE_CALL, 0, 0x4c43, false, 19 },
    { 3, DOSBOX_SITE_CALL, 0, 0x3043, false, 20 },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x3b0e, false, M_SITE_RIKA },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x52c7, false, M_SITE_MEIRA },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x2795, false, M_SITE_STONES },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0xa9dc, false, M_SITE_MARISA },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x69da, false, M_SITE_SIGMA },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x6734, false, M_SITE_SIGMA_PHASE },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x95c8, false, M_SITE_MIMA_FIRST },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x9740, false, M_SITE_MIMA_MIDDLE },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x9802, false, M_SITE_MIMA_FOURTH },
    { 2, DOSBOX_SITE_CALL, 0x1000, 0x98f0, false, M_SITE_MIMA_FINAL },
    { 2, DOSBOX_SITE_POLL, 0, 0xc405, false, 31 },
    { 3, DOSBOX_SITE_CALL, 0, 0xb878, false, M_SITE_MEASURE },
    { 2, DOSBOX_SITE_CALL, 0, 0x4344, false, M_SITE_TILE_SOURCES },
};
#define M_BUTTON(name, mask) M_##name = mask,
enum { TH02_BUTTONS(M_BUTTON) };
#undef M_BUTTON
static const DOSBOX_KEY m_Keys[] = {
    { M_extra_group_0, 0x01, '1' },
    { M_extra_group_3, 0x18, 'o' },
    { M_extra_group_5, 0x28, ']' },
    { M_extra_group_1, 0x08, '8' },
    { M_extra_group_2, 0x11, 'w' },
    { M_extra_group_4, 0x20, 'f' },
    { M_extra_group_6, 0x30, ',' },
    { M_extra_group_7, 0x38, 0 },
    { M_logo_aliases, 0x0d, '\\' },
    { M_up, 0x3a, 0 },
    { M_down, 0x3d, 0 },
    { M_left, 0x3b, 0 },
    { M_right, 0x3c, 0 },
    { M_up_left, 0x42, '7' },
    { M_up_right, 0x44, '9' },
    { M_down_left, 0x4a, '1' },
    { M_down_right, 0x4c, '3' },
    { M_shot, 0x29, 'z' },
    { M_space, 0x34, ' ' },
    { M_bomb, 0x2a, 'x' },
    { M_cancel, 0, 0x1b },
    { M_keypad_up, 0x43, '8' },
    { M_keypad_down, 0x4b, '2' },
    { M_keypad_left, 0x46, '4' },
    { M_keypad_right, 0x48, '6' },
    { M_ok, 0x1c, '\r' },
    { M_q, 0x10, 'q' },
};
static const char *const m_Private[] = { "HUUMA.CFG", "HUUHI.DAT" };

static uint8_t M_Byte(const uint16_t segment, const uint16_t offset)
{
    uint8_t value;
    Dosbox_Read(segment, offset, &value, sizeof(value));
    return value;
}

static uint16_t M_Word(const uint16_t segment, const uint16_t offset)
{
    uint16_t value;
    Dosbox_Read(segment, offset, &value, sizeof(value));
    return value;
}

static uint32_t M_Dword(const uint16_t segment, const uint16_t offset)
{
    uint32_t value;
    Dosbox_Read(segment, offset, &value, sizeof(value));
    return value;
}

static bool M_Idle(void *context)
{
    const uint16_t segment = (uint16_t)(Dosbox_LoadSegment() + 0x0d63);
    return Dosbox_Program() == 1 && M_Byte(segment, 0x160) != 0
        && M_Byte(segment, 0x91) == 0;
}

static void M_Seed(void *context)
{
    memset(&m_Clock, 0, sizeof(m_Clock));
    memset(&m_PmdStart, 0, sizeof(m_PmdStart));
    m_Started = false;
}

static bool M_Advance(void *context)
{
    memset(m_ClockOrdinals, 0, sizeof(m_ClockOrdinals));
    m_EffectOrdinal = 0;
    Dosbox_RecordClock(TH02_CLOCK_FRAME, Dosbox_Now());
    const time_t now = time(nullptr);
    const struct tm *const date = localtime(&now);
    if (date != nullptr) {
        // PC-98 INT 1Ch reads localtime; DOS expands its two-digit year.
        uint32_t year = 1900 + (uint32_t)date->tm_year % 100;
        if (year < 1980) {
            year += 100;
        }
        Dosbox_RecordDate(
            year * 10000 + (uint32_t)(date->tm_mon + 1) * 100
            + (uint32_t)date->tm_mday);
    }
    return VSync_Advance(&m_Clock);
}

static bool M_AcceptExec(
    void *context, const uint8_t program, const uint16_t psp)
{
    if (program != 0) {
        return true;
    }
    uint8_t size;
    Dosbox_Read(psp, 0x80, &size, sizeof(size));
    char args[128] = {};
    Dosbox_Read(psp, 0x81, args, size < 127 ? size : 127);
    char *start = args;
    while (*start == ' ') {
        start++;
    }
    const size_t length = strcspn(start, " \r");
    if (length != 7 && length != 11) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (toupper((unsigned char)start[i]) != "ZUNSOFT.COM"[i]) {
            return false;
        }
    }
    return true;
}

static void M_Start(void)
{
    if (m_Started) {
        return;
    }
    const DOSBOX_REGS regs = Dosbox_Regs();
    const uint16_t name_offset = M_Word(regs.ss, (uint16_t)(regs.sp + 4));
    const uint16_t name_segment = M_Word(regs.ss, (uint16_t)(regs.sp + 6));
    char name[5];
    Dosbox_Read(name_segment, name_offset, name, sizeof(name));
    if (memcmp(name, "main", sizeof(name)) != 0) {
        return;
    }
    m_Started = true;
    static const struct {
        uint16_t offset;
        uint16_t also;
        uint8_t size;
    } fields[TH02_START_COUNT] = {
        [TH02_START_stage] = { 11, 0, 1 },
        [TH02_START_shot_type] = { 38, 0, 1 },
        [TH02_START_rank] = { 22, 0, 1 },
        [TH02_START_score] = { 14, 0, 4 },
        [TH02_START_continues] = { 18, 0, 2 },
        [TH02_START_lives] = { 21, 26, 1 },
        [TH02_START_bombs] = { 20, 25, 1 },
        [TH02_START_power] = { 23, 0, 1 },
        [TH02_START_skill] = { 40, 0, 2 },
    };
    const uint16_t segment = (uint16_t)(Dosbox_LoadSegment() + 0x0d63);
    const uint16_t resident = M_Word(segment, 0x1dae);
    const uint16_t offset = M_Word(segment, 0x1dac);
    size_t count;
    const START_VALUE *const starts = Dosbox_Starts(&count);
    for (size_t i = 0; i < count; i++) {
        const uint32_t field = starts[i].field;
        if (field == TH02_START_boss_hp) {
            continue;
        }
        Dosbox_Write(
            resident, (uint16_t)(offset + fields[field].offset),
            &starts[i].value, fields[field].size);
        if (fields[field].also != 0) {
            Dosbox_Write(
                resident, (uint16_t)(offset + fields[field].also),
                &starts[i].value, fields[field].size);
        }
    }
    if (M_Byte(resident, (uint16_t)(offset + 11)) == 127) {
        // Reuse the unused debug-launcher name for the ending executable.
        const uint16_t ending_name[] = { 0x1ac, segment };
        Dosbox_Write(segment, ending_name[0], "maine", sizeof("maine"));
        Dosbox_Write(
            regs.ss, (uint16_t)(regs.sp + 4), ending_name, sizeof(ending_name));
        Dosbox_Write(
            regs.ss, (uint16_t)(regs.sp + 8), ending_name, sizeof(ending_name));
    }
}

static void M_BossHp(const uint8_t id)
{
    const DOSBOX_REGS regs = Dosbox_Regs();
    uint16_t offset = 0x2bea;
    int16_t threshold;
    switch (id) {
    case M_SITE_RIKA:
        threshold = 2261;
        break;
    case M_SITE_MEIRA:
        threshold = 2400;
        break;
    case M_SITE_STONES:
        offset = (uint16_t)(0x2bd4 + regs.bx);
        threshold = regs.si == 4 ? 930 : regs.si <= 1 ? 140 : 120;
        break;
    case M_SITE_MARISA:
        threshold = 900;
        break;
    case M_SITE_SIGMA:
        threshold = 1800;
        break;
    case M_SITE_SIGMA_PHASE:
        threshold = M_Byte(regs.ds, 0x7b42) == 7 ? 1300 : 1800;
        break;
    case M_SITE_MIMA_FIRST:
        threshold = 600;
        break;
    case M_SITE_MIMA_MIDDLE:
        threshold = (int16_t)M_Word(regs.ds, 0x9280);
        break;
    case M_SITE_MIMA_FOURTH:
        threshold = 1500;
        break;
    case M_SITE_MIMA_FINAL:
        threshold = 1100;
        break;
    default:
        return;
    }
    size_t count;
    const START_VALUE *const starts = Dosbox_Starts(&count);
    for (size_t i = 0; i < count; i++) {
        if (starts[i].field == TH02_START_boss_hp) {
            const int16_t damage =
                (int16_t)((uint16_t)threshold - (uint16_t)starts[i].value);
            Dosbox_Write(regs.ds, offset, &damage, sizeof(damage));
        }
    }
}

static void M_OnSite(void *context, const uint8_t id)
{
    if (id == M_SITE_TILE_SOURCES) {
        m_LastSource = M_Byte(Dosbox_Regs().ds, 0x089d);
    } else if (id == M_SITE_MEASURE) {
        const uint32_t key =
            m_ClockOrdinals[TH02_CLOCK_MEASURE]++ * TH02_CLOCK_KIND_COUNT
            + TH02_CLOCK_MEASURE;
        Dosbox_RecordClock(key, Dosbox_Now());
    } else if (id == 18) {
        M_Start();
    } else if (id >= M_SITE_RIKA) {
        M_BossHp(id);
    } else {
        Dosbox_Date();
    }
}

static void M_OnLoad(void *context, const char *const name, const uint16_t psp)
{
    if (strcmp(name, "MAIN.EXE") == 0) {
        m_LastSource = 0;
    }
    if (strcmp(name, "PMD86.COM") == 0) {
        m_PmdBase = (uint32_t)psp * 16;
        m_ClockOrigin = false;
        Dosbox_ObserveSite(m_PmdBase + 0x53b5);
        Dosbox_ObserveSite(m_PmdBase + 0x116);
    }
}

static void M_ObserveSite(void *context, const uint32_t address)
{
    if (m_PmdBase == 0) {
        return;
    }
    if (m_PmdStart.address != 0 && address == m_PmdStart.address) {
        Dosbox_RecordClock(m_PmdStart.key, Dosbox_Now());
        Dosbox_RemoveObserver(address);
        memset(&m_PmdStart, 0, sizeof(m_PmdStart));
        return;
    }
    const DOSBOX_REGS regs = Dosbox_Regs();
    if (!m_ClockOrigin && address == m_PmdBase + 0x53b5 && regs.dx == 0x18a
        && (uint8_t)regs.ax == 0x3f) {
        m_ClockOrigin = true;
        Dosbox_RecordClock(TH02_CLOCK_ORIGIN, Dosbox_Now());
        Dosbox_RemoveObserver(address);
    } else if (address == m_PmdBase + 0x116) {
        TH02_CLOCK_KIND kind;
        switch (regs.ax >> 8) {
        case 0:
            m_PmdStart.address =
                (uint32_t)M_Word(regs.ss, (uint16_t)(regs.sp + 2)) * 16
                + M_Word(regs.ss, regs.sp);
            m_PmdStart.key =
                m_ClockOrdinals[TH02_CLOCK_START]++ * TH02_CLOCK_KIND_COUNT
                + TH02_CLOCK_START;
            Dosbox_ObserveSite(m_PmdStart.address);
            return;
        case 0x0c:
            Dosbox_RecordClock(
                TH02_CLOCK_EFFECT_ENTRY_KEY + m_EffectOrdinal, Dosbox_Now());
            m_PmdStart.address =
                (uint32_t)M_Word(regs.ss, (uint16_t)(regs.sp + 2)) * 16
                + M_Word(regs.ss, regs.sp);
            m_PmdStart.key = TH02_CLOCK_EFFECT_KEY + m_EffectOrdinal++;
            Dosbox_ObserveSite(m_PmdStart.address);
            return;
        case 1:
            kind = TH02_CLOCK_STOP;
            break;
        case 6:
            kind = TH02_CLOCK_LOAD;
            break;
        default:
            return;
        }
        const uint32_t key =
            m_ClockOrdinals[kind]++ * TH02_CLOCK_KIND_COUNT + kind;
        Dosbox_RecordClock(key, Dosbox_Now());
    }
}

static void M_SampleVideo(const bool ending, uint32_t *const row)
{
    Dosbox_ReadGraphics(m_Graphics[0], m_Palette);
    const uint8_t *const pages[2] = { m_Graphics[0], m_Graphics[1] };
    row[TH02_COLUMN_vram_hash] =
        Th02_HashGraphics(pages, ending ? SIZE_MAX : m_LastSource + 1u);
    row[TH02_COLUMN_palette_hash] =
        State_HashBytes(STATE_HASH_INIT, m_Palette, sizeof(m_Palette));
}

static void M_Sample(void *context, uint32_t *const row)
{
    memset(row, 0, TH02_COLUMN_COUNT * sizeof(*row));
    row[TH02_COLUMN_font_profile] = TH02_FONT_PROFILE_SHINONOME;
    row[TH02_COLUMN_buttons] = Dosbox_Buttons();
    const int32_t program = Dosbox_Program();
    row[TH02_COLUMN_program] = (uint32_t)(program + 1);
    const uint16_t load = Dosbox_LoadSegment();
    if (program == 0) {
        row[TH02_COLUMN_logo_frame] = M_Word(load, 0x2850);
        return;
    }
    if (program == 2 || program == 3) {
        M_SampleVideo(program == 3, row);
    }
    if (program == 3) {
        const uint16_t segment = (uint16_t)(load + 0x0cf8);
        row[TH02_COLUMN_count1] = M_Word(segment, 0x2100);
        row[TH02_COLUMN_count2] = M_Word(segment, 0x2102);
        return;
    }
    if (program == 2) {
        const uint16_t segment = (uint16_t)(load + 0x1da7);
        row[TH02_COLUMN_stage] = M_Byte(segment, 0x8f5);
        row[TH02_COLUMN_score] = M_Dword(segment, 0xb28);
        row[TH02_COLUMN_score_lives] =
            (uint32_t)(int32_t)(int8_t)M_Byte(segment, 0xb2c);
        row[TH02_COLUMN_score_bombs] =
            (uint32_t)(int32_t)(int8_t)M_Byte(segment, 0xb2d);
        row[TH02_COLUMN_power] = M_Byte(segment, 0xaa6);
        row[TH02_COLUMN_rng] = M_Dword(segment, 0x568);
        row[TH02_COLUMN_rng_cursor] = M_Byte(segment, 0x27fa);
        row[TH02_COLUMN_player_x] =
            (uint32_t)(int32_t)(int16_t)M_Word(segment, 0x2b86);
        row[TH02_COLUMN_player_y] =
            (uint32_t)(int32_t)(int16_t)M_Word(segment, 0x2b88);
        return;
    }
    if (program != 1) {
        return;
    }
    const uint16_t segment = (uint16_t)(load + 0x0d63);
    row[TH02_COLUMN_count1] = M_Word(segment, 0x1328);
    row[TH02_COLUMN_count2] = M_Word(segment, 0x132a);
    row[TH02_COLUMN_selection] =
        (uint32_t)(int32_t)(int8_t)M_Byte(segment, 0x90);
    row[TH02_COLUMN_in_options] = M_Byte(segment, 0x91);
    row[TH02_COLUMN_idle] = M_Word(segment, 0x12cc);
    row[TH02_COLUMN_main_ready] = M_Byte(segment, 0x160);
    row[TH02_COLUMN_main_input] = M_Byte(segment, 0x12c8);
    row[TH02_COLUMN_options_ready] = M_Byte(segment, 0x162);
    row[TH02_COLUMN_options_input] = M_Byte(segment, 0x161);
    row[TH02_COLUMN_difficulty] = M_Byte(segment, 0x604);
    row[TH02_COLUMN_music] = M_Byte(segment, 0x12c9);
    row[TH02_COLUMN_bombs] = M_Byte(segment, 0x1db2);
    row[TH02_COLUMN_lives] = M_Byte(segment, 0x1db1);
    const uint16_t resident = M_Word(segment, 0x1dae);
    const uint16_t offset = M_Word(segment, 0x1dac);
    uint32_t frame;
    Dosbox_Read(resident, (uint16_t)(offset + 0x1c), &frame, sizeof(frame));
    row[TH02_COLUMN_frame] = frame;
    row[TH02_COLUMN_reduce_effects] =
        M_Byte(resident, (uint16_t)(offset + 0x24));
}

static void M_FrameEnd(void *context)
{
    Dosbox_RecordClock(TH02_CLOCK_SCANOUT_KEY, Dosbox_Now());
}

static const DOSBOX_PORT m_Port = {
    .port = { .game = &g_Th02Harness,
              .sample = M_Sample,
              .idle = M_Idle,
              .seed = M_Seed },
    .programs = m_Programs,
    .program_count = ARRAY_SIZE(m_Programs),
    .sites = m_Sites,
    .site_count = ARRAY_SIZE(m_Sites),
    .keys = m_Keys,
    .key_count = ARRAY_SIZE(m_Keys),
    .private_paths = m_Private,
    .private_count = ARRAY_SIZE(m_Private),
    .advance = M_Advance,
    .accept_exec = M_AcceptExec,
    .on_site = M_OnSite,
    .on_load = M_OnLoad,
    .observe_site = M_ObserveSite,
    .on_frame_end = M_FrameEnd,
};

[[gnu::visibility("default")]] const DOSBOX_GUEST *Dosbox_Attach(
    const DOSBOX_HOST *const host, const int argc, char **const argv)
{
    return Dosbox_Start(&m_Port, host, argc, argv);
}
