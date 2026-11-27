#pragma once

#include "replay/harness/game.h"
#include "replay/th02/fields.h"

#include <stddef.h>
#include <stdint.h>

#define M_COLUMN(name, kind) TH02_COLUMN_##name,
enum { TH02_COLUMNS(M_COLUMN) TH02_COLUMN_COUNT };
#undef M_COLUMN

#define M_START(name, kind) TH02_START_##name,
enum { TH02_START_FIELDS(M_START) TH02_START_COUNT };
#undef M_START

#define TH02_BUTTONS(X)                                                        \
    X(up, 0x1)                                                                 \
    X(down, 0x2)                                                               \
    X(left, 0x4)                                                               \
    X(right, 0x8)                                                              \
    X(up_left, 0x10)                                                           \
    X(up_right, 0x20)                                                          \
    X(down_left, 0x40)                                                         \
    X(down_right, 0x80)                                                        \
    X(shot, 0x100)                                                             \
    X(space, 0x200)                                                            \
    X(bomb, 0x400)                                                             \
    X(cancel, 0x800)                                                           \
    X(ok, 0x1000)                                                              \
    X(q, 0x2000)                                                               \
    X(keypad_up, 0x4000)                                                       \
    X(keypad_down, 0x8000)                                                     \
    X(keypad_left, 0x10000)                                                    \
    X(keypad_right, 0x20000)                                                   \
    X(extra_group_0, 0x40000)                                                  \
    X(extra_group_3, 0x80000)                                                  \
    X(extra_group_5, 0x100000)                                                 \
    X(extra_group_1, 0x200000)                                                 \
    X(extra_group_2, 0x400000)                                                 \
    X(extra_group_4, 0x800000)                                                 \
    X(extra_group_6, 0x1000000)                                                \
    X(extra_group_7, 0x2000000)                                                \
    X(logo_aliases, 0x4000000)

extern const HARNESS_GAME g_Th02Harness;

// Hashes both 640 by 400 color-index pages for vram_hash, leaving out the
// tile sources from index loaded_sources on, which the original fills from
// past its tile buffer.
uint32_t Th02_HashGraphics(
    const uint8_t *const pages[2], size_t loaded_sources);
