#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Tells the update chain what to do after a callback.
typedef enum {
    CHAIN_REMOVE = 0, // removes the element and continues
    CHAIN_CONTINUE = 1, // keeps the element and continues
    CHAIN_AGAIN = 2, // runs the same callback again
    CHAIN_STOP = 3, // stops the run; the frame goes on
    CHAIN_QUIT = 4, // stops the run; the game quits
    CHAIN_RESET = 5, // stops the run; the game starts again
    CHAIN_RESTART = 6, // runs the update chain again from the start
} CHAIN_STATUS;

// Tells the frame loop how a run of the update chain ended.
typedef enum {
    CHAIN_RUN_CONTINUE,
    CHAIN_RUN_QUIT,
    CHAIN_RUN_RESET,
} CHAIN_RUN;

typedef struct CHAIN_ELEM {
    CHAIN_STATUS (*callback)(void *arg);
    // Called when the element is added. A failure is returned by the add.
    RESULT (*added)(void *arg);
    // Called once when the element is removed.
    void (*deleted)(void *arg);
    void *arg;
    int16_t priority;
    struct CHAIN_ELEM *prev;
    struct CHAIN_ELEM *next;
} CHAIN_ELEM;

// Holds th06's update and draw chains. Each lists elements by priority,
// lower first; an element added at an existing priority runs after the
// elements already there.
typedef struct {
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    // Runs after each calc, including before a restart. Returns whether a
    // pending restart may run. False stops at that completed calc and
    // returns CHAIN_RUN_CONTINUE. Set by the runner; nullptr in the game.
    bool (*on_calc_end)(void *arg);
    void *on_calc_end_arg;
} CHAIN;

void Chain_Init(CHAIN *chain);
// Sets callback and arg and clears everything else.
void Chain_InitElem(
    CHAIN_ELEM *elem, CHAIN_STATUS (*callback)(void *), void *arg);
// Inserts elem by priority, which must be at least 0, then calls added.
RESULT Chain_AddCalc(CHAIN *chain, CHAIN_ELEM *elem, int16_t priority);
RESULT Chain_AddDraw(CHAIN *chain, CHAIN_ELEM *elem, int16_t priority);
// Unlinks elem and calls deleted. Does nothing when elem is in no chain.
void Chain_Remove(CHAIN_ELEM *elem);
// Runs the update chain. A run in which no callback ran also quits.
CHAIN_RUN Chain_RunCalc(CHAIN *chain);
// Runs the draw chain. CHAIN_RESTART counts as CHAIN_CONTINUE there.
void Chain_RunDraw(CHAIN *chain);
// Removes every element: the update chain's first, in order.
void Chain_Release(CHAIN *chain);
