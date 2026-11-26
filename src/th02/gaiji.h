#pragma once

// Gaiji: the 256 user-defined 16x16 glyphs that the game loads into the
// text RAM font and shows like characters.

#include "pc98/zun/bfnt.h"

// The glyphs that the game draws by ID, named by where it uses them; runs
// of glyphs give their first, and the comment says how many follow.
// clang-format off
typedef enum {
    GAIJI_LIFE         = 0x02, // A life in the HUD tally.
    GAIJI_BOMB         = 0x03, // A bomb in the HUD tally.
    GAIJI_POWER_BAR    = 0x30, // 16: a power segment n/16 full, 0x3f whole.
    GAIJI_MAX          = 0x40, // 5: MAX, for full power.
    GAIJI_DIGIT_0      = 0xa0, // 10: the digits 0 to 9.
    GAIJI_LETTER_A     = 0xaa, // 26: the letters A to Z.
    GAIJI_SCORE_LABEL  = 0xc4, // 3: the HUD's score label.
    GAIJI_POWER_LABEL  = 0xc7, // 2: the HUD's power label.
    GAIJI_LIVES_LABEL  = 0xc9, // 2: the HUD's lives label.
    GAIJI_BOMBS_LABEL  = 0xcc, // 2: the HUD's bombs label.
    GAIJI_HI_LABEL     = 0xce, // The HUD's high-score prefix.
    GAIJI_BLANK        = 0xcf, // An empty cell.
    GAIJI_WIPE_CLOSE   = 0xcf, // 9: the cels of the closing playfield wipe.
    GAIJI_NOTE         = 0xd8, // The music title's note.
    GAIJI_ALL          = 0xf0, // The score table's all-clear stage mark.
    GAIJI_WIPE_OPEN    = 0xf1, // 9: the cels of the opening playfield wipe.
} GAIJI_ID;
// clang-format on

// Gaiji masks by ID, bit 15 leftmost.
typedef struct {
    uint16_t rows[256][16];
} GAIJI;

// Registers all 256 16x16 two-color glyphs of font.
RESULT Gaiji_Register(GAIJI *bank, const BFNT *font);
