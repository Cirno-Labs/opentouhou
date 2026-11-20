#pragma once

#include "core/base/result.h"
#include "core/base/types.h"
#include "core/io/reader.h"

// The PC-98's font ROM, from which the games draw their text: 256
// half-width 8x16 cells by JIS X 0201 code, and full-width 16x16 cells by
// JIS X 0208 code, 0x2121 through 0x7E7E. A dump of the ROM or a free font
// fills it; a cell that its source lacks is blank. Rows run top to bottom
// with the leftmost pixel in bit 15, and a half-width row uses bits 15
// through 8. A cell lands on screen as it is, without a baseline or
// scaling.
typedef struct FONT_ROM FONT_ROM;

// Reads FONT.ROM as T98Tools dumps it: 288768 bytes, with the 8x16 cells
// at 0x800 and, from 0x1800, a block for each first JIS byte from 0x21
// through 0x7C of 96 cells, second bytes 0x20 through 0x7F, each sixteen
// bytes of left half and sixteen of right half. Its 8x8 cells are left
// out.
RESULT FontRom_ReadT98(READER *reader, FONT_ROM **out);

// Reads a font in Anex86's format, as NEC.BMP, ANEX86.BMP and FREECG98.BMP
// hold it: an uncompressed bottom-up 2048x2048 one-bit BMP whose clear bits
// are ink. Its top 16 lines hold the 8x16 cells by code; each 16-line row
// below holds 128 cells, JIS code (first, second) in row second and column
// first - 0x20.
RESULT FontRom_ReadAnex86(READER *reader, FONT_ROM **out);

// Reads 16-pixel BDF fonts: full, of registry JISX0208.1983 or
// JISX0208.1990, gives the 16x16 cells, and half, of registry
// JISX0201.1976, the 8x16 cells; half can be nullptr. Each has PIXEL_SIZE
// 16 and a FONT_ASCENT and FONT_DESCENT that add up to 16; a glyph's BBX
// places it against FONT_ASCENT, clipped to its cell.
RESULT FontRom_ReadBdf(READER *full, READER *half, FONT_ROM **out);

// Opens the font ROM that the font registry (core/text/font_registry.h)
// keeps as the file name: a FONT.ROM dump, a BMP in Anex86's format or a
// full-width BDF, whose 8x16 cells come from the BDF half when it is not
// nullptr. Tells the format from the data. Reports failure when the
// registry has no such file or it does not read.
RESULT FontRom_Open(const char *name, const char *half, FONT_ROM **out);

void FontRom_Free(FONT_ROM *rom);

// Gives the cell of a Unicode character as the original's CP932 bytes
// read it from the ROM: U+0020 through U+007E and U+FF61 through U+FF9F
// take the 8x16 cell of their byte, so '\' draws the ROM's yen sign, and
// other characters the 16x16 cell of their JIS X 0208 code, NEC's row 13
// included. Returns whether the ROM has a cell for code_point; CP932's IBM
// extensions and characters outside CP932 have none. Allocates while it
// converts, so call it while loading.
bool FontRom_Glyph(
    const FONT_ROM *rom, uint32_t code_point, uint16_t rows[16],
    uint8_t *width);
