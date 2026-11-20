#pragma once

#include <stddef.h>
#include <stdint.h>

// Describes the original programs that the decomp's targets.sha256 pins.
// Every offset is one in an original program's load image, taken from the
// link maps of a rebuild whose load images are byte-identical to the
// originals. Data offsets are in each program's DGROUP, whose segment is
// its load segment (PSP + 10h) plus the value below. Code sites are
// load-image segment:offset pairs. ZUNSOFT.COM is one segment based at its
// PSP, so its offsets are the map's minus 100h.

typedef enum {
    RETAIL_PROGRAM_ZUNSOFT,
    RETAIL_PROGRAM_OP,
    RETAIL_PROGRAM_REIIDEN,
    RETAIL_PROGRAM_FUUIN,
    RETAIL_PROGRAM_COUNT,
} RETAIL_PROGRAM;

// X(program, name, sha1) for each program that a run checks.
#define RETAIL_PROGRAMS(X)                                                     \
    X(RETAIL_PROGRAM_ZUNSOFT, "ZUNSOFT.COM",                                   \
      "9455c32597aa227251ef385a90b8d39da09b298c")                              \
    X(RETAIL_PROGRAM_OP, "OP.EXE", "84ff5b9019721ddd616c3c57d2998c9c26c33f74") \
    X(RETAIL_PROGRAM_REIIDEN, "REIIDEN.EXE",                                   \
      "938241ec9bfa3a6a9225c9e49acbef7467a93c57")                              \
    X(RETAIL_PROGRAM_FUUIN, "FUUIN.EXE",                                       \
      "bcaebe42d56a7a5db8d77dc5657a6d75988bc6a4")

// The music driver, which GAME.BAT loads with -M7.
#define RETAIL_MDRV98_SHA1 "93c42378ddca33ea04a45d8e391b0c1bb29b0605"

#define RETAIL_ZUNSOFT_DGROUP 0x0000
#define RETAIL_OP_DGROUP 0x1229
#define RETAIL_REIIDEN_DGROUP 0x349a
#define RETAIL_FUUIN_DGROUP 0x12aa

// X(name, type, offset) for each ZUNSOFT global that a column reads.
#define RETAIL_ZUNSOFT_DATA(X) X(frame, int16_t, 0x2850)

// X(name, type, offset) for each OP global that a column reads.
#define RETAIL_OP_DATA(X)                                                      \
    X(z_vsync_Count1, int16_t, 0x12cc)                                         \
    X(frame_rand, uint32_t, 0x1188)                                            \
    X(random_seed, uint32_t, 0x086c)                                           \
    X(menu_id, uint8_t, 0x009a)                                                \
    X(menu_sel, uint8_t, 0x0095)

// X(name, type, offset) for each REIIDEN global that a column reads.
// SinGyoku keeps its HP, phase and phase frame in statics of b05.cpp, which
// the map leaves out; the singyoku_ offsets come from its code. Kikuri keeps
// its phase in kikuri_phase of b15j.cpp.
#define RETAIL_REIIDEN_DATA(X)                                                 \
    X(z_vsync_Count1, int16_t, 0x3e72)                                         \
    X(frame_rand, uint32_t, 0x2276)                                            \
    X(random_seed, uint32_t, 0x0bfa)                                           \
    X(frame_since_start_of_binary, uint32_t, 0x2280)                           \
    X(rank, int8_t, 0x0090)                                                    \
    X(stage_num, int8_t, 0x0094)                                               \
    X(route, int8_t, 0x59de)                                                   \
    X(score, uint32_t, 0x00ba)                                                 \
    X(rem_lives, int16_t, 0x00e0)                                              \
    X(rem_bombs, int8_t, 0x0092)                                               \
    X(stage_timer, uint16_t, 0x540c)                                           \
    X(player_left, int16_t, 0x2286)                                            \
    X(player_is_hit, uint8_t, 0x00af)                                          \
    X(player_invincible, int16_t, 0x00d8)                                      \
    X(player_deflecting, uint8_t, 0x00b7)                                      \
    X(player_sliding, uint8_t, 0x00b9)                                         \
    X(player_invincibility_time, int16_t, 0x2284)                              \
    X(bomb_frame, uint32_t, 0x00c2)                                            \
    X(orb_in_portal, int16_t, 0x00e6)                                          \
    X(orb_cur_left, int16_t, 0x2288)                                           \
    X(orb_cur_top, int16_t, 0x228a)                                            \
    X(orb_velocity_x, int16_t, 0x00dc)                                         \
    X(orb_velocity_y, double, 0x00f4)                                          \
    X(orb_force, double, 0x00fc)                                               \
    X(orb_force_frame, int16_t, 0x228c)                                        \
    X(cardcombo_cur, int16_t, 0x00e4)                                          \
    X(card_flip_cycle, uint8_t, 0x124e)                                        \
    X(boss_id, int8_t, 0x013f)                                                 \
    X(boss_hp, int16_t, 0x5d28)                                                \
    X(boss_phase, int8_t, 0x5d2e)                                              \
    X(boss_phase_frame, int16_t, 0x5d2a)                                       \
    X(singyoku_hp, int16_t, 0x59e3)                                            \
    X(singyoku_phase, int8_t, 0x133e)                                          \
    X(singyoku_phase_frame, int16_t, 0x59df)                                   \
    X(kikuri_phase, int8_t, 0x5a1e)

// X(name, offset, size) for each REIIDEN object that a hash column reads:
// the pellets (100 records of 42 bytes, then 27 more bytes), the shots and
// the cards. cards holds far pointers to its arrays on the heap.
#define RETAIL_REIIDEN_OBJECTS(X)                                              \
    X(Pellets, 0x2c95, 0x1083)                                                 \
    X(Shots, 0x3d18, 0x015a)                                                   \
    X(cards, 0x54e4, 0x0016)

// X(name, type, offset) for each FUUIN global that a column reads.
#define RETAIL_FUUIN_DATA(X)                                                   \
    X(z_vsync_Count1, int16_t, 0x15a4)                                         \
    X(random_seed, uint32_t, 0x0b44)                                           \
    X(rank, uint8_t, 0x1582)                                                   \
    X(end_flag, uint8_t, 0x1581)                                               \
    X(score, int32_t, 0x0090)                                                  \
    X(score_highest, int32_t, 0x0094)

// The block that OP creates as five paragraphs and every program finds by
// its id at the start of its MCB's data.
#define RETAIL_RESIDENT_ID "ReiidenConfig"
typedef struct [[gnu::packed]] {
    char id[sizeof(RETAIL_RESIDENT_ID)];
    int8_t rank;
    int8_t bgm_mode;
    int8_t rem_bombs;
    int8_t credit_lives_extra;
    int8_t end_flag;
    int8_t unused_1;
    int8_t route;
    int8_t rem_lives;
    int8_t snd_need_init;
    int8_t unused_2;
    int8_t debug_mode;
    int16_t pellet_speed;
    uint32_t rand;
    int32_t score;
    int32_t continues_total;
    uint16_t continues_per_scene[4];
    int32_t bonus_per_stage[4];
    uint16_t stage_id;
    uint32_t hiscore;
    int32_t score_highest;
    uint16_t point_value;
} RETAIL_RESIDENT;
static_assert(offsetof(RETAIL_RESIDENT, rand) == 0x1b);
static_assert(offsetof(RETAIL_RESIDENT, continues_total) == 0x23);
static_assert(offsetof(RETAIL_RESIDENT, stage_id) == 0x3f);
static_assert(sizeof(RETAIL_RESIDENT) == 0x4b);

typedef enum {
    RETAIL_SITE_LOGO_VBLANK_END,
    RETAIL_SITE_LOGO_VSYNC,
    RETAIL_SITE_FRAME_DELAY,
    RETAIL_SITE_VSYNC_WAIT_END,
    RETAIL_SITE_VSYNC_WAIT,
    RETAIL_SITE_SCROLLUP_END,
    RETAIL_SITE_SCROLLUP,
    RETAIL_SITE_DELAY,
    RETAIL_SITE_OP_MENU,
    RETAIL_SITE_DEBUG_HEAP,
    RETAIL_SITE_STAGEBONUS_RELEASE,
    RETAIL_SITE_TOTLE_RELEASE,
    RETAIL_SITE_REGIST_IDLE,
    RETAIL_SITE_REGIST_RELEASE,
    RETAIL_SITE_COUNT,
} RETAIL_SITE;

// X(program, site, kind, segment, offset) for each site, where kind names a
// DOSBOX_SITE_ member. A VSYNC or VBLANK_END site is the jump that repeats
// its poll; DELAY is the entry of Borland's far delay(ms); OP_MENU calls
// frame_delay(1) at the end of OP's menu loop. A POLL site is the jump of a
// loop that reads input and has no wait.
#define RETAIL_SITES(X)                                                        \
    X(RETAIL_PROGRAM_ZUNSOFT, RETAIL_SITE_LOGO_VBLANK_END, VBLANK_END, 0x0000, \
      0x058c)                                                                  \
    X(RETAIL_PROGRAM_ZUNSOFT, RETAIL_SITE_LOGO_VSYNC, VSYNC, 0x0000, 0x0594)   \
    X(RETAIL_PROGRAM_OP, RETAIL_SITE_FRAME_DELAY, VSYNC, 0x0b26, 0x0012)       \
    X(RETAIL_PROGRAM_OP, RETAIL_SITE_VSYNC_WAIT_END, VBLANK_END, 0x0b27,       \
      0x00cd)                                                                  \
    X(RETAIL_PROGRAM_OP, RETAIL_SITE_VSYNC_WAIT, VSYNC, 0x0b27, 0x00d5)        \
    X(RETAIL_PROGRAM_OP, RETAIL_SITE_DELAY, DELAY, 0x0000, 0x9bf8)             \
    X(RETAIL_PROGRAM_OP, RETAIL_SITE_OP_MENU, CALL, 0x0a1c, 0x1054)            \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_FRAME_DELAY, VSYNC, 0x0e36, 0x000d)  \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_VSYNC_WAIT_END, VBLANK_END, 0x0e37,  \
      0x00c8)                                                                  \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_VSYNC_WAIT, VSYNC, 0x0e37, 0x00d0)   \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_SCROLLUP_END, VBLANK_END, 0x1173,    \
      0x0057)                                                                  \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_SCROLLUP, VSYNC, 0x1173, 0x005f)     \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_DELAY, DELAY, 0x0000, 0xaf20)        \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_DEBUG_HEAP, POLL, 0x0b50, 0x1edf)    \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_STAGEBONUS_RELEASE, POLL, 0x12ba,    \
      0x08c6)                                                                  \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_TOTLE_RELEASE, POLL, 0x12ba, 0x0d9f) \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_REGIST_IDLE, POLL, 0x1395, 0x0f29)   \
    X(RETAIL_PROGRAM_REIIDEN, RETAIL_SITE_REGIST_RELEASE, POLL, 0x1395,        \
      0x1273)                                                                  \
    X(RETAIL_PROGRAM_FUUIN, RETAIL_SITE_FRAME_DELAY, VSYNC, 0x0c73, 0x010e)    \
    X(RETAIL_PROGRAM_FUUIN, RETAIL_SITE_VSYNC_WAIT_END, VBLANK_END, 0x0ca4,    \
      0x00c1)                                                                  \
    X(RETAIL_PROGRAM_FUUIN, RETAIL_SITE_VSYNC_WAIT, VSYNC, 0x0ca4, 0x00c9)     \
    X(RETAIL_PROGRAM_FUUIN, RETAIL_SITE_DELAY, DELAY, 0x0000, 0x997d)          \
    X(RETAIL_PROGRAM_FUUIN, RETAIL_SITE_REGIST_IDLE, POLL, 0x0a16, 0x143f)     \
    X(RETAIL_PROGRAM_FUUIN, RETAIL_SITE_REGIST_RELEASE, POLL, 0x0a16, 0x179a)
