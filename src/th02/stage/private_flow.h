#pragma once

// logic.c's own state, exposed for tests; nothing outside the stage/
// module sees this.

#include "th02/stage/flow.h"

typedef enum {
    STAGE_FLOW_SCROLL,
    STAGE_FLOW_ENTER,
    STAGE_FLOW_FIGHT,
    STAGE_FLOW_LEAVE,
    STAGE_FLOW_DONE,
} STAGE_FLOW_PHASE;

typedef struct {
    STAGE_FLOW_PHASE phase;
    bool enemy_updates;
    bool after_tile_effect;
} STAGE_FLOW;

// A writable view of flow.c's private state, for tests.
extern STAGE_FLOW *const p_Flow;
