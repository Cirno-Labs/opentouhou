#pragma once

#include "pc98/pmd_data.h"

#define PMD_TRACK_COUNT 11
#define PMD_RHYTHM_LIMIT 128
#define PMD_MEMO_LIMIT 134

struct PMD_SONG {
    const uint8_t *bytes;
    size_t size;
    uint16_t tracks[PMD_TRACK_COUNT];
    uint16_t rhythm[PMD_RHYTHM_LIMIT];
    uint16_t rhythm_count;
    uint16_t instrument_offset;
    uint16_t instrument_count;
    uint16_t command_end;
};

struct PMD_EFFECTS {
    const uint8_t *bytes;
    size_t size;
    uint16_t offsets[PMD_EFFECT_COUNT];
    uint16_t instrument_offset;
    uint16_t instrument_count;
};

const uint8_t *PmdData_Instrument(const PMD_EFFECTS *bank, uint8_t id);

const uint8_t *PmdData_SongInstrument(const PMD_SONG *song, uint8_t id);
