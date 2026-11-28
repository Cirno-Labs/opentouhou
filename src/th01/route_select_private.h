#pragma once

#include "th01/route_select.h"

typedef struct {
    ROUTE_SELECT_PHASE phase;
    uint8_t selection;
    bool holding_up;
    bool holding_down;
} ROUTE_SELECT;

// Writable view of the route selection screen for tests.
extern ROUTE_SELECT *const p_Selection;
