#pragma once

#include "th02/input.h"
#include "th02/session.h"
#include "th02/shot_select/render.h"

typedef enum {
    SHOT_SELECT_INPUT,
    SHOT_SELECT_FRAME,
    SHOT_SELECT_FADE_OUT,
    SHOT_SELECT_COMPLETE,
} SHOT_SELECT_WAIT;

typedef struct {
    SHOT_SELECT_WAIT wait;
    uint8_t release_images;
} SHOT_SELECT_EVENTS;

void ShotSelect_Init(void);

// Draws the shot selection with the mode's clear flags and starts the
// black-in fade.
void ShotSelect_Begin(
    TH02_SESSION *session, const SHOT_SELECT_ASSETS *assets,
    const bool cleared[3]);

// Starts input on the middle choice after the black-in fade.
void ShotSelect_BeginInput(void);

// Runs one frame for keys; releases the portraits before the black-out
// fade.
SHOT_SELECT_EVENTS ShotSelect_Update(uint16_t keys);

void ShotSelect_BeginFadeOut(void);
bool ShotSelect_VSync(void);
