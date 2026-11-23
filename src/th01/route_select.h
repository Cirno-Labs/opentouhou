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

typedef struct {
    ROUTE_SELECT_PHASE phase;
    uint8_t selection;
    bool holding_up, holding_down;
} ROUTE_SELECT;

typedef struct {
    PALETTE_STATE *palette;
    const ROUTE_SELECT_ASSETS *assets;
    uint8_t *route;
    bool *stage_cleared;
} ROUTE_SELECT_CONTEXT;

// Caches the five route labels from rom. Glyphs belong to allocator. Load
// both real scene descriptors before ticks, then validate every borrowed
// asset.
RESULT RouteSelect_LoadFont(
    const FONT_ROM *rom, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);
RESULT RouteSelect_ValidateAssets(const ROUTE_SELECT_ASSETS *assets);

// Begins immediately after SinGyoku's defeat recovery, in the same tick.
// Composes the actual page-1 glyph rows, transparent doubled text and Orb
// cursor, resets game input, then waits 50 count1 VSyncs. Zero-initialize once
// and begin only when done. Reuses count1 immediately, including a ready
// initial delay. Keep context and borrowed assets stable until completion.
bool RouteSelect_Begin(
    ROUTE_SELECT *selection, const ROUTE_SELECT_CONTEXT *context,
    const TH01_INPUT *raw);

// Senses the supplied input only at ready source polling boundaries. New up
// or down toggles once, even when both become pressed. Held arrows do not
// repeat; held shot/OK may confirm at the first poll. Nonconfirm polls wait
// five count1 VSyncs. Confirmation posts effect 4, activates the real cached
// scene, then writes route, stage_cleared and player.is_hit in that order.
// Retains the input layer's paused-Return-shot hit side effect. Consumers may
// record sound events but must not mutate borrowed state. Allocates nothing,
// reads no clock/device/file and advances no VSync or gameplay counters.
bool RouteSelect_Resume(
    ROUTE_SELECT *selection, const ROUTE_SELECT_CONTEXT *context,
    const TH01_INPUT *raw);
