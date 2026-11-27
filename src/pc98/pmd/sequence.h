#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "pc98/pmd/data.h"
#include "pc98/pmd/types.h"

#include <stdint.h>

#define MUSIC_PART_COUNT 14
#define MUSIC_WORK_SIZE 65536

typedef struct {
    uint16_t position;
    uint16_t repeat_position;
    uint8_t remaining;
    uint8_t loop_check;
    uint8_t mask;
    uint8_t slots;
    uint8_t key_ons;
    bool active;
} MUSIC_PART;

typedef struct {
    const PMD_SONG *music;
    uint8_t *work;
    MUSIC_PART parts[MUSIC_PART_COUNT];
    uint64_t step;
    uint16_t rhythm_position;
    uint16_t measure;
    uint8_t timer_b;
    uint8_t tempo;
    uint8_t measure_length;
    uint8_t measure_tick;
    uint8_t status;
    uint8_t loops;
    bool playing;
    bool failed;
} MUSIC_SEQUENCE;

void MusicSeq_Init(PMD *pmd, const ALLOCATOR *allocator);
RESULT MusicSeq_Load(PMD *pmd, const PMD_SONG *music);
RESULT MusicSeq_Start(PMD *pmd);
void MusicSeq_Stop(PMD *pmd);

void MusicSeq_SetMask(MUSIC_SEQUENCE *sequence, uint8_t part, uint8_t mask);

// Runs SSG parts before FM parts. Drum takeover precedes slur/glide parsing;
// inline effects keep the issuing part's mask until dispatch ends.
RESULT MusicSeq_Step(PMD *pmd);
