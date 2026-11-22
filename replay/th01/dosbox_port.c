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

#define M_DATA(name, type, offset) M_DATA_##name = offset,
enum { RETAIL_REIIDEN_DATA(M_DATA) };
#undef M_DATA

// Writes the case's boss_hp over the HP that a boss's setup has just
// stored.
static void M_BossHp(const uint16_t offset)
{
    size_t count;
    const START_VALUE *const starts = Dosbox_Starts(&count);
    for (size_t i = 0; i < count; i++) {
        if (starts[i].field == TH01_START_boss_hp) {
            const int16_t hp = (int16_t)starts[i].value;
            Dosbox_Write(
                (uint16_t)(Dosbox_LoadSegment() + RETAIL_REIIDEN_DGROUP),
                offset, &hp, sizeof(hp));
        }
    }
}

static void M_OnSite(void *const context, const uint8_t id)
{
    TH01_DOSBOX *const th01 = context;
    switch (id) {
    case RETAIL_SITE_OP_MENU:
        th01->menu_frame = th01->frames;
        break;
    case RETAIL_SITE_SONG_START:
        th01->track_starts++;
        break;
    case RETAIL_SITE_BOSS_HP:
        M_BossHp(M_DATA_boss_hp);
        break;
    case RETAIL_SITE_SINGYOKU_HP:
        M_BossHp(M_DATA_singyoku_hp);
        break;
    }
}

// Applies the case's start values to the resident block before REIIDEN's
// first instruction, once: REIIDEN reads its stage, route, rank, lives,
// bombs, score and continues there on entry.
static void M_OnExec(void *const context, const uint8_t program)
{
    TH01_DOSBOX *const th01 = context;
    th01->cards_set = false;
    if (program == RETAIL_PROGRAM_OP) {
        th01->track_starts = 0;
    }
    RETAIL_RESIDENT resident;
    if (program != RETAIL_PROGRAM_REIIDEN || th01->reiiden_ran
        || !Th01Dosbox_Resident(th01, &resident)) {
        return;
    }
    th01->reiiden_ran = true;
    size_t count;
    const START_VALUE *const starts = Dosbox_Starts(&count);
    bool scene_set = false;
    uint16_t scene_continues = 0;
    for (size_t i = 0; i < count; i++) {
        const uint32_t value = starts[i].value;
        switch (starts[i].field) {
        case TH01_START_stage:
            resident.stage_id = (uint16_t)value;
            break;
        case TH01_START_route:
            resident.route = (int8_t)value;
            break;
        case TH01_START_difficulty:
            resident.rank = (int8_t)value;
            break;
        case TH01_START_lives:
            resident.rem_lives = (int8_t)value;
            break;
        case TH01_START_bombs:
            resident.rem_bombs = (int8_t)value;
            break;
        case TH01_START_score:
            resident.score = (int32_t)value;
            break;
        case TH01_START_continues:
            resident.continues_total = (int32_t)value;
            break;
        case TH01_START_pellet_speed:
            resident.pellet_speed = (int16_t)value;
            break;
        case TH01_START_boss_hp:
            break;
        case TH01_START_scene_continues:
            scene_set = true;
            scene_continues = (uint16_t)value;
            break;
        }
    }
    const size_t scene = (size_t)resident.stage_id / 5;
    if (scene_set && scene < ARRAY_SIZE(resident.continues_per_scene)) {
        resident.continues_per_scene[scene] = scene_continues;
    }
    Dosbox_Write(th01->resident, 0, &resident, sizeof(resident));
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
    .on_exec = M_OnExec,
};

[[gnu::visibility("default")]] const DOSBOX_GUEST *Dosbox_Attach(
    const DOSBOX_HOST *const host, const int argc, char **const argv)
{
    return Dosbox_Start(&m_Port, host, argc, argv);
}
