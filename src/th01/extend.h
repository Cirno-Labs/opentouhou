#pragma once

#include "th01/hud.h"
#include "th01/pellets.h"

// Holds the extend threshold index and the life count that survives a
// Continue or a program switch.
typedef struct {
    int16_t next;
    int8_t resident_lives;
} EXTEND;

extern EXTEND g_Extend;

// Reconstructs the next threshold from the signed resident score bits at
// main-scene restart: signed division by 400000 plus one, wrapped to 16 bits.
// Fresh games start with next=1; the gameplay owner retains that field.
int16_t Extend_NextForScore(uint32_t resident_score);

// Handles one threshold per call, using the unsigned live score and the
// wrapped 32-bit product of signed next and 400000. Below the six-life cap,
// adds a life, draws it, plays effect 15 and raises the pellet speed by one
// raw unit. Advances next even at the cap.
void Extend_UpdateAndRender(void);
