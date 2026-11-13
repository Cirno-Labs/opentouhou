#pragma once

#include "core/base/types.h"

// Declares the CP932 tables that tools/gen_cp932_table generates into
// cp932_table.c. Only core/base/cp932.c uses them.

#define CP932_LEAD_COUNT 60
#define CP932_TRAIL_FIRST 0x40
#define CP932_TRAIL_LAST 0xFC
#define CP932_TRAIL_COUNT (CP932_TRAIL_LAST - CP932_TRAIL_FIRST + 1)

typedef struct {
    uint16_t code_point;
    uint16_t cp932;
} CP932_PAIR;

// Maps each byte that is not a lead byte to its code point. Lead bytes map
// to 0.
extern const uint16_t g_Cp932SingleByte[256];

// Maps a lead byte and a trail byte to a code point, or to 0 if the pair is
// not defined. The rows are the lead bytes 0x81 to 0x9F and 0xE0 to 0xFC.
extern const uint16_t g_Cp932DoubleByte[CP932_LEAD_COUNT][CP932_TRAIL_COUNT];

// Maps every code point from U+0080 up that CP932 can encode to its CP932
// form, sorted by code point. A form below 0x100 is one byte; the others
// are a lead byte and a trail byte.
extern const CP932_PAIR g_Cp932Encode[];
extern const int32_t g_Cp932EncodeCount;
