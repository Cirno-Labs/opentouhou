#include "core/text/font_rom.h"

#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/text/font_registry.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define M_CELL_ROWS 16
#define M_JIS_FIRST 0x21
#define M_JIS_LAST 0x7E
#define M_JIS_COUNT (M_JIS_LAST - M_JIS_FIRST + 1)

#define M_T98_SIZE 288768
#define M_T98_HALF 0x800
#define M_T98_FULL 0x1800
#define M_T98_LAST_FIRST 0x7C
#define M_T98_SECONDS 96

#define M_ANEX86_SIDE 2048
#define M_ANEX86_PITCH (M_ANEX86_SIDE / 8)

#define M_BDF_MAX_BBX 64

struct FONT_ROM {
    // Takes JIS rows 0x29 and 0x2a and 0x2b21 from the ANK cells (BDF).
    bool borrows_ank_rows;
    uint16_t half[256][M_CELL_ROWS];
    uint16_t full[M_JIS_COUNT][M_JIS_COUNT][M_CELL_ROWS];
};

typedef struct {
    int32_t cell_width;
    const char *const *registries;
    int32_t registry_count;
    int32_t ascent;
    int32_t descent;
    int32_t pixel_size;
    char registry[32];
    bool checked;
} M_BDF_FONT;

typedef struct {
    int32_t encoding;
    int32_t bbx[4];
    bool in_char;
    bool has_bbx;
    int32_t row_count;
    bool in_bitmap;
    uint16_t rows[M_CELL_ROWS];
} M_BDF_GLYPH;

static const char *const m_FullRegistries[] = {
    "JISX0208.1983",
    "JISX0208.1990",
};
static const char *const m_HalfRegistries[] = { "JISX0201.1976" };

static const char *M_Name(const READER *const reader)
{
    const char *const name = Reader_GetName(reader);
    return name != nullptr ? name : "font";
}

static bool M_IsJis(const int32_t byte)
{
    return byte >= M_JIS_FIRST && byte <= M_JIS_LAST;
}

static uint16_t *M_FullCell(
    FONT_ROM *const rom, const int32_t first, const int32_t second)
{
    return rom->full[first - M_JIS_FIRST][second - M_JIS_FIRST];
}

static RESULT M_ReadT98(READER *const reader, FONT_ROM *const rom)
{
    FAIL_IF(
        Reader_Remaining(reader) != M_T98_SIZE,
        "%s: a FONT.ROM dump has %d bytes, not %zu", M_Name(reader), M_T98_SIZE,
        Reader_Remaining(reader));
    const size_t start = Reader_Tell(reader);
    Reader_Seek(reader, start + M_T98_HALF);
    for (int32_t code = 0; code < 256; code++) {
        for (int32_t row = 0; row < M_CELL_ROWS; row++) {
            rom->half[code][row] = (uint16_t)(Reader_ReadU8(reader) << 8);
        }
    }
    Reader_Seek(reader, start + M_T98_FULL);
    for (int32_t first = M_JIS_FIRST; first <= M_T98_LAST_FIRST; first++) {
        for (int32_t second = 0x20; second < 0x20 + M_T98_SECONDS; second++) {
            uint8_t cell[2][M_CELL_ROWS];
            Reader_ReadBytes(reader, cell, sizeof(cell));
            if (!M_IsJis(second)) {
                continue;
            }
            uint16_t *const rows = M_FullCell(rom, first, second);
            for (int32_t row = 0; row < M_CELL_ROWS; row++) {
                rows[row] = (uint16_t)(cell[0][row] << 8 | cell[1][row]);
            }
        }
    }
    return Reader_Result(reader);
}

static void M_PutAnex86Line(
    FONT_ROM *const rom, const int32_t y, const uint8_t *const line)
{
    const int32_t row = y % M_CELL_ROWS;
    if (y < M_CELL_ROWS) {
        for (int32_t code = 0; code < 256; code++) {
            rom->half[code][row] = (uint16_t)((uint8_t)~line[code] << 8);
        }
        return;
    }
    const int32_t second = y / M_CELL_ROWS;
    if (!M_IsJis(second)) {
        return;
    }
    for (int32_t first = M_JIS_FIRST; first <= M_JIS_LAST; first++) {
        const uint8_t *const cell = line + (first - 0x20) * 2;
        M_FullCell(rom, first, second)[row] =
            (uint16_t)~(cell[0] << 8 | cell[1]);
    }
}

static RESULT M_ReadAnex86(READER *const reader, FONT_ROM *const rom)
{
    const char *const name = M_Name(reader);
    const size_t start = Reader_Tell(reader);
    uint8_t magic[2];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    Reader_Skip(reader, 8);
    const uint32_t pixels = Reader_ReadU32(reader);
    const uint32_t header_size = Reader_ReadU32(reader);
    const int32_t width = Reader_ReadI32(reader);
    const int32_t height = Reader_ReadI32(reader);
    const uint16_t planes = Reader_ReadU16(reader);
    const uint16_t bits = Reader_ReadU16(reader);
    const uint32_t compression = Reader_ReadU32(reader);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "BM", 2) != 0, "%s: not a BMP", name);
    FAIL_IF(header_size < 40, "%s: unknown BMP header", name);
    FAIL_IF(
        width != M_ANEX86_SIDE || height != M_ANEX86_SIDE,
        "%s: an Anex86 font is %dx%d, not %dx%d", name, M_ANEX86_SIDE,
        M_ANEX86_SIDE, width, height);
    FAIL_IF(
        planes != 1 || bits != 1, "%s: an Anex86 font has 1 bit, not %d", name,
        bits);
    FAIL_IF(compression != 0, "%s: compressed BMP", name);
    Reader_Seek(reader, start + pixels);
    for (int32_t i = 0; i < M_ANEX86_SIDE && !Reader_Failed(reader); i++) {
        uint8_t line[M_ANEX86_PITCH];
        Reader_ReadBytes(reader, line, sizeof(line));
        M_PutAnex86Line(rom, M_ANEX86_SIDE - 1 - i, line);
    }
    return Reader_Result(reader);
}

// Parses count integers from text and points rest past them.
static bool M_ParseInts(
    const char *text, int32_t *const values, const int32_t count,
    const char **const rest)
{
    for (int32_t i = 0; i < count; i++) {
        char *end;
        errno = 0;
        const long long value = strtoll(text, &end, 10);
        if (end == text || errno != 0 || value < INT32_MIN
            || value > INT32_MAX) {
            return false;
        }
        values[i] = (int32_t)value;
        text = end;
    }
    *rest = text;
    return true;
}

static bool M_IsBlank(const char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return *text == '\0';
}

static int32_t M_HexDigit(const char digit)
{
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    return -1;
}

static RESULT M_CheckBdfFont(M_BDF_FONT *const font, const char *const name)
{
    FAIL_IF(
        font->pixel_size != 16, "%s: PIXEL_SIZE is %d, not 16", name,
        font->pixel_size);
    FAIL_IF(
        font->ascent + font->descent != 16,
        "%s: FONT_ASCENT and FONT_DESCENT add up to %d, not 16", name,
        font->ascent + font->descent);
    bool known = false;
    for (int32_t i = 0; i < font->registry_count; i++) {
        known =
            known || Str_EqualIgnoreCase(font->registry, font->registries[i]);
    }
    FAIL_IF(
        !known, "%s: CHARSET_REGISTRY \"%s\" is not %s", name, font->registry,
        font->registries[0]);
    font->checked = true;
    return OK;
}

static void M_PutBdfGlyph(
    FONT_ROM *const rom, const M_BDF_FONT *const font,
    const M_BDF_GLYPH *const glyph)
{
    uint16_t *cell;
    if (font->cell_width == 8) {
        if (glyph->encoding < 0 || glyph->encoding > 0xFF) {
            return;
        }
        cell = rom->half[glyph->encoding];
    } else {
        const int32_t first = glyph->encoding >> 8;
        const int32_t second = glyph->encoding & 0xFF;
        if (glyph->encoding < 0 || !M_IsJis(first) || !M_IsJis(second)) {
            return;
        }
        cell = M_FullCell(rom, first, second);
    }
    memcpy(cell, glyph->rows, sizeof(glyph->rows));
}

// Places one BITMAP row of glyph at its BBX offset within the cell.
static RESULT M_ReadBdfRow(
    const M_BDF_FONT *const font, M_BDF_GLYPH *const glyph,
    const char *const text, const char *const name, const int32_t line)
{
    const int32_t width = glyph->bbx[0];
    FAIL_IF(
        glyph->row_count >= glyph->bbx[1], "%s:%d: more BITMAP rows than BBX",
        name, line);
    const int32_t digits = (width + 7) / 8 * 2;
    FAIL_IF(
        (int32_t)strlen(text) < digits, "%s:%d: short BITMAP row", name, line);
    uint16_t bits = 0;
    for (int32_t x = 0; x < width; x++) {
        const int32_t digit = M_HexDigit(text[x / 4]);
        FAIL_IF(digit < 0, "%s:%d: bad BITMAP row", name, line);
        const int32_t column = glyph->bbx[2] + x;
        if ((digit & (8 >> (x % 4))) != 0 && column >= 0
            && column < font->cell_width) {
            bits |= (uint16_t)(0x8000 >> column);
        }
    }
    const int32_t row =
        font->ascent - (glyph->bbx[1] + glyph->bbx[3]) + glyph->row_count;
    if (row >= 0 && row < M_CELL_ROWS) {
        glyph->rows[row] = bits;
    }
    glyph->row_count++;
    return OK;
}

static RESULT M_ReadBdfLine(
    FONT_ROM *const rom, M_BDF_FONT *const font, M_BDF_GLYPH *const glyph,
    char *const text, const char *const name, const int32_t line)
{
    char *value = text;
    while (*value != '\0' && *value != ' ' && *value != '\t') {
        value++;
    }
    if (*value != '\0') {
        *value++ = '\0';
    }
    const char *const key = text;
    const char *rest;
    if (glyph->in_bitmap && strcmp(key, "ENDCHAR") != 0) {
        return M_ReadBdfRow(font, glyph, key, name, line);
    }
    if (strcmp(key, "PIXEL_SIZE") == 0) {
        FAIL_IF(
            !M_ParseInts(value, &font->pixel_size, 1, &rest),
            "%s:%d: bad PIXEL_SIZE", name, line);
    } else if (strcmp(key, "FONT_ASCENT") == 0) {
        FAIL_IF(
            !M_ParseInts(value, &font->ascent, 1, &rest),
            "%s:%d: bad FONT_ASCENT", name, line);
    } else if (strcmp(key, "FONT_DESCENT") == 0) {
        FAIL_IF(
            !M_ParseInts(value, &font->descent, 1, &rest),
            "%s:%d: bad FONT_DESCENT", name, line);
    } else if (strcmp(key, "CHARSET_REGISTRY") == 0) {
        while (*value == ' ' || *value == '"') {
            value++;
        }
        size_t size = strcspn(value, "\" \t");
        size =
            size < sizeof(font->registry) ? size : sizeof(font->registry) - 1;
        memcpy(font->registry, value, size);
        font->registry[size] = '\0';
    } else if (strcmp(key, "STARTCHAR") == 0) {
        if (!font->checked) {
            MUST(M_CheckBdfFont(font, name));
        }
        memset(glyph, 0, sizeof(*glyph));
        glyph->encoding = -1;
        glyph->in_char = true;
    } else if (!glyph->in_char) {
        return OK;
    } else if (strcmp(key, "ENCODING") == 0) {
        FAIL_IF(
            !M_ParseInts(value, &glyph->encoding, 1, &rest),
            "%s:%d: bad ENCODING", name, line);
    } else if (strcmp(key, "BBX") == 0) {
        FAIL_IF(
            !M_ParseInts(value, glyph->bbx, 4, &rest) || !M_IsBlank(rest)
                || glyph->bbx[0] < 0 || glyph->bbx[0] > M_BDF_MAX_BBX
                || glyph->bbx[1] < 0 || glyph->bbx[1] > M_BDF_MAX_BBX,
            "%s:%d: bad BBX", name, line);
        glyph->has_bbx = true;
    } else if (strcmp(key, "BITMAP") == 0) {
        FAIL_IF(!glyph->has_bbx, "%s:%d: BITMAP before BBX", name, line);
        glyph->in_bitmap = true;
    } else if (strcmp(key, "ENDCHAR") == 0) {
        FAIL_IF(
            glyph->row_count != glyph->bbx[1], "%s:%d: %d BITMAP rows, not %d",
            name, line, glyph->row_count, glyph->bbx[1]);
        M_PutBdfGlyph(rom, font, glyph);
        memset(glyph, 0, sizeof(*glyph));
    }
    return OK;
}

static RESULT M_ParseBdf(
    FONT_ROM *const rom, M_BDF_FONT *const font, char *text,
    const char *const name)
{
    FAIL_IF(!Str_StartsWith(text, "STARTFONT"), "%s: not a BDF font", name);
    M_BDF_GLYPH glyph = {};
    for (int32_t line = 1; text != nullptr; line++) {
        char *const end = strchr(text, '\n');
        if (end != nullptr) {
            *end = '\0';
        }
        const size_t size = strlen(text);
        if (size > 0 && text[size - 1] == '\r') {
            text[size - 1] = '\0';
        }
        MUST(M_ReadBdfLine(rom, font, &glyph, text, name, line));
        text = end != nullptr ? end + 1 : nullptr;
    }
    if (!font->checked) {
        MUST(M_CheckBdfFont(font, name));
    }
    return OK;
}

static RESULT M_ReadBdf(
    READER *const reader, FONT_ROM *const rom, const int32_t cell_width,
    const char *const *const registries, const int32_t registry_count)
{
    M_BDF_FONT font = {
        .cell_width = cell_width,
        .registries = registries,
        .registry_count = registry_count,
    };
    size_t size;
    char *const text = Reader_ReadCString(reader, &g_HeapAllocator, &size);
    MUST(Reader_Result(reader));
    const RESULT result = M_ParseBdf(rom, &font, text, M_Name(reader));
    Allocator_Free(&g_HeapAllocator, text, size + 1);
    return result;
}

static RESULT M_Finish(
    const RESULT result, FONT_ROM *const rom, FONT_ROM **const out)
{
    if (!IS_OK(result)) {
        FontRom_Free(rom);
        return result;
    }
    *out = rom;
    return OK;
}

RESULT FontRom_ReadT98(READER *const reader, FONT_ROM **const out)
{
    *out = nullptr;
    FONT_ROM *const rom = Mem_AllocZ(sizeof(FONT_ROM));
    return M_Finish(M_ReadT98(reader, rom), rom, out);
}

RESULT FontRom_ReadAnex86(READER *const reader, FONT_ROM **const out)
{
    *out = nullptr;
    FONT_ROM *const rom = Mem_AllocZ(sizeof(FONT_ROM));
    return M_Finish(M_ReadAnex86(reader, rom), rom, out);
}

RESULT FontRom_ReadBdf(
    READER *const full, READER *const half, FONT_ROM **const out)
{
    *out = nullptr;
    FONT_ROM *const rom = Mem_AllocZ(sizeof(FONT_ROM));
    rom->borrows_ank_rows = true;
    RESULT result = M_ReadBdf(full, rom, 16, m_FullRegistries, 2);
    if (IS_OK(result) && half != nullptr) {
        result = M_ReadBdf(half, rom, 8, m_HalfRegistries, 1);
    }
    return M_Finish(result, rom, out);
}

RESULT FontRom_Open(
    const char *const name, const char *const half, FONT_ROM **const out)
{
    *out = nullptr;
    const void *data;
    size_t size;
    FAIL_IF(
        !FontRegistry_FindFile(name, &data, &size), "No font file %s", name);
    READER reader;
    Reader_OpenBuffer(&reader, data, size, name);
    const bool bdf = size >= 9 && memcmp(data, "STARTFONT", 9) == 0;
    RESULT result;
    if (size == M_T98_SIZE) {
        result = FontRom_ReadT98(&reader, out);
    } else if (size >= 2 && memcmp(data, "BM", 2) == 0) {
        result = FontRom_ReadAnex86(&reader, out);
    } else if (bdf && half == nullptr) {
        result = FontRom_ReadBdf(&reader, nullptr, out);
    } else if (bdf) {
        const void *half_data;
        size_t half_size;
        if (FontRegistry_FindFile(half, &half_data, &half_size)) {
            READER half_reader;
            Reader_OpenBuffer(&half_reader, half_data, half_size, half);
            result = FontRom_ReadBdf(&reader, &half_reader, out);
            Reader_Close(&half_reader);
        } else {
            result = FAIL("No font file %s", half);
        }
    } else {
        result = FAIL("%s: unknown font format", name);
    }
    Reader_Close(&reader);
    return result;
}

void FontRom_Free(FONT_ROM *const rom)
{
    Mem_Free(rom);
}

const uint16_t *FontRom_Cell(
    const FONT_ROM *const rom, const uint16_t code, const bool wide)
{
    if (!wide) {
        return code < 256 ? rom->half[code] : nullptr;
    }
    const int32_t first = code >> 8;
    const int32_t second = code & 255;
    if (!M_IsJis(first) || !M_IsJis(second)) {
        return nullptr;
    }
    if (rom->borrows_ank_rows) {
        if (first == 0x29) {
            return rom->half[second];
        }
        if (first == 0x2a && second <= 0x5f) {
            return rom->half[second + 0x80];
        }
        if (code == 0x2b21) {
            return rom->half[0x20];
        }
    }
    return rom->full[first - M_JIS_FIRST][second - M_JIS_FIRST];
}

bool FontRom_Glyph(
    const FONT_ROM *const rom, const uint32_t code_point, uint16_t rows[16],
    uint8_t *const width)
{
    memset(rows, 0, sizeof(uint16_t) * M_CELL_ROWS);
    if ((code_point >= 0x20 && code_point <= 0x7E)
        || (code_point >= 0xFF61 && code_point <= 0xFF9F)) {
        const uint32_t code =
            code_point <= 0x7E ? code_point : code_point - 0xFF61 + 0xA1;
        memcpy(rows, rom->half[code], sizeof(rom->half[code]));
        *width = 8;
        return true;
    }
    *width = 16;
    if (code_point < 0x80 || code_point >= 0x10000
        || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
        return false;
    }
    const char utf8[] = {
        (char)(code_point < 0x800 ? 0xC0 | code_point >> 6
                                  : 0xE0 | code_point >> 12),
        (char)(code_point < 0x800 ? 0x80 | (code_point & 0x3F)
                                  : 0x80 | (code_point >> 6 & 0x3F)),
        (char)(0x80 | (code_point & 0x3F)),
    };
    char *bytes;
    if (!Result_Ignore(
            Cp932_Encode(utf8, code_point < 0x800 ? 2 : 3, &bytes))) {
        return false;
    }
    const size_t size = strlen(bytes);
    const int32_t lead = (uint8_t)bytes[0];
    const int32_t trail = (uint8_t)bytes[1];
    Mem_Free(bytes);
    if (size != 2) {
        return false;
    }
    // Shift-JIS packs two JIS rows into each lead byte.
    int32_t first = ((lead >= 0xE0 ? lead - 0x40 : lead) - 0x81) * 2 + 0x21;
    int32_t second;
    if (trail >= 0x9F) {
        first++;
        second = trail - 0x7E;
    } else {
        second = trail - (trail >= 0x80 ? 0x20 : 0x1F);
    }
    if (!M_IsJis(first) || !M_IsJis(second)) {
        return false;
    }
    memcpy(
        rows, rom->full[first - M_JIS_FIRST][second - M_JIS_FIRST],
        sizeof(uint16_t) * M_CELL_ROWS);
    return true;
}
