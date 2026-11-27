#include "replay/dosbox/test/port.h"

#include "replay/dosbox/dosbox.h"

#include "core/base/utils.h"

// Describes FRAMES.COM, frames.asm, to the dosbox backend as the game
// dostest, and runs it as dostestdosbox.so. Its VSyncs follow a 56.4 Hz
// refresh on the 60 Hz clock, and its delay waits its milliseconds.

#define M_REFRESH 56423
#define M_CLOCK 60000
#define M_IDLE_FRAMES 24

typedef enum {
    M_SITE_VSYNC,
    M_SITE_CALL,
    M_SITE_POLL,
    M_SITE_VBLANK_END,
    M_SITE_GDC_VSYNC,
    M_SITE_DELAY,
} M_SITE;

// Gives the words of frames.asm at 104h, as offsets in its load image.
typedef enum {
    M_DATA_FRAMES = 0x04,
    M_DATA_VSYNCS = 0x06,
    M_DATA_COUNT1 = 0x08,
    M_DATA_KEYS = 0x0a,
    M_DATA_LAST_KEY = 0x0c,
    M_DATA_PRESSES = 0x0e,
    M_DATA_POLLS = 0x10,
    M_DATA_GDC_WAITS = 0x12,
    M_DATA_DELAYS = 0x14,
    M_DATA_STAGED = 0x16,
} M_DATA;

static const BINDING m_Bindings[] = {
    { "shot", 0x1 },
    { "ok", 0x2 },
};

static const COLUMN m_Columns[] = {
    { "program", COLUMN_I32 },  { "wait", COLUMN_I32 },
    { "frames", COLUMN_U32 },   { "vsyncs", COLUMN_U32 },
    { "count1", COLUMN_U32 },   { "keys", COLUMN_U32 },
    { "last_key", COLUMN_U32 }, { "presses", COLUMN_U32 },
    { "polls", COLUMN_U32 },    { "gdc_waits", COLUMN_U32 },
    { "delays", COLUMN_U32 },   { "staged", COLUMN_U32 },
    { "calls", COLUMN_U32 },
};

// Sums the run up, for the recipe to check in case.ini.
static const COLUMN m_Choices[] = {
    { "staged", COLUMN_U32 }, { "presses", COLUMN_U32 },
    { "polls", COLUMN_U32 },  { "gdc_waits", COLUMN_U32 },
    { "delays", COLUMN_U32 }, { "calls", COLUMN_U32 },
};

const HARNESS_GAME g_DostestHarness = {
    .id = "dostest",
    .version = "1",
    .frame_rate = 60.0,
    .width = 640,
    .height = 400,
    .draw_frames = 1,
    .bindings = m_Bindings,
    .binding_count = ARRAY_SIZE(m_Bindings),
    .columns = m_Columns,
    .column_count = ARRAY_SIZE(m_Columns),
    .choices = m_Choices,
    .choice_count = ARRAY_SIZE(m_Choices),
    .columns_version = 1,
};

static const char *const m_Programs[] = { "FRAMES.COM" };

static const DOSBOX_SITE m_Sites[] = {
    { 0, DOSBOX_SITE_VSYNC, 0, 0x9a, false, M_SITE_VSYNC },
    { 0, DOSBOX_SITE_CALL, 0, 0xa0, false, M_SITE_CALL },
    { 0, DOSBOX_SITE_POLL, 0, 0xda, false, M_SITE_POLL },
    { 0, DOSBOX_SITE_VBLANK_END, 0, 0xe8, false, M_SITE_VBLANK_END },
    { 0, DOSBOX_SITE_VSYNC, 0, 0xee, false, M_SITE_GDC_VSYNC },
    { 0, DOSBOX_SITE_DELAY, 0, 0x159, true, M_SITE_DELAY },
};

static const DOSBOX_KEY m_Keys[] = {
    { 0x1, 0x29, 'z' },
    { 0x2, 0x1c, '\r' },
};

static const char *const m_Private[] = { "SAVE.DAT" };

static const char m_Stage[] = "S";

static const STAGED_FILE m_Files[] = {
    { "stage.txt", (const uint8_t *)m_Stage, sizeof(m_Stage) - 1 },
};

static struct {
    uint32_t phase;
    uint32_t calls;
} m_Test;

static uint16_t M_Word(const uint16_t offset)
{
    uint16_t value = 0;
    if (Dosbox_Program() >= 0) {
        Dosbox_Read(Dosbox_LoadSegment(), offset, &value, sizeof(value));
    }
    return value;
}

static void M_Sample(void *const context, uint32_t *const row)
{
    static const uint16_t words[] = {
        M_DATA_FRAMES,   M_DATA_VSYNCS,  M_DATA_COUNT1, M_DATA_KEYS,
        M_DATA_LAST_KEY, M_DATA_PRESSES, M_DATA_POLLS,  M_DATA_GDC_WAITS,
        M_DATA_DELAYS,   M_DATA_STAGED,
    };
    size_t column = 0;
    row[column++] = (uint32_t)Dosbox_Program();
    row[column++] = (uint32_t)Dosbox_LastWait();
    for (size_t i = 0; i < ARRAY_SIZE(words); i++) {
        row[column++] = M_Word(words[i]);
    }
    row[column++] = m_Test.calls;
    row[column++] = M_Word(M_DATA_STAGED);
    row[column++] = M_Word(M_DATA_PRESSES);
    row[column++] = M_Word(M_DATA_POLLS);
    row[column++] = M_Word(M_DATA_GDC_WAITS);
    row[column++] = M_Word(M_DATA_DELAYS);
    row[column++] = m_Test.calls;
}

static bool M_Idle(void *const context)
{
    return M_Word(M_DATA_FRAMES) >= M_IDLE_FRAMES;
}

static void M_Seed(void *const context)
{
    m_Test.phase = M_CLOCK - M_REFRESH;
    m_Test.calls = 0;
}

static bool M_Advance(void *const context)
{
    m_Test.phase += M_REFRESH;
    if (m_Test.phase < M_CLOCK) {
        return false;
    }
    m_Test.phase -= M_CLOCK;
    return true;
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
    m_Test.calls++;
}

const DOSBOX_PORT g_DostestPort = {
    .port = {
        .game = &g_DostestHarness,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
        .files = m_Files,
        .file_count = ARRAY_SIZE(m_Files),
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
