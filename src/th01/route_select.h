#pragma once

#include "th01/frame.h"
#include "th01/player.h"
#include "th01/palette.h"
#include "th01/scene.h"
#include "th01/text.h"

typedef struct {
    const GAME_TEXT_FONT *font;
    const PTN *stage;
    const SCENE_DATA *makai, *jigoku;
} ROUTE_SELECT_ASSETS;

typedef enum {
    ROUTE_SELECT_DONE,
    ROUTE_SELECT_INITIAL_WAIT,
    ROUTE_SELECT_CHOOSE,
    ROUTE_SELECT_REPEAT_WAIT,
} ROUTE_SELECT_PHASE;

// Caches the five route labels from rom. Glyphs belong to allocator. Load
// both real scene descriptors before ticks, then validate every borrowed
// asset.
RESULT RouteSelect_LoadFont(
    const FONT_ROM *rom, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);
RESULT RouteSelect_ValidateAssets(const ROUTE_SELECT_ASSETS *assets);

// Requires completed selection state and stable assets, reusing count1
// immediately for the initial fifty-count wait.
bool RouteSelect_Begin(
    const ROUTE_SELECT_ASSETS *assets, const TH01_INPUT *raw);

// Retains held-confirm acceptance and paused-Return-shot effects, polling
// input only at completed fifty-count or five-count waits.
bool RouteSelect_Resume(
    const ROUTE_SELECT_ASSETS *assets, const TH01_INPUT *raw);

void RouteSelect_Init(void);
