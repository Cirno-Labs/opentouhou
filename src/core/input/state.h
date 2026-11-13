// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/ini.h"
#include "core/input/schema.h"

#include <stddef.h>
#include <stdint.h>

typedef struct INPUT_STATE INPUT_STATE;

// Describes one connected controller.
typedef struct {
    uint32_t id;
    INPUT_DEVICE device;
    // Sets the name that the platform reports, in UTF-8.
    const char *name;
    // Sets the player that the controller feeds, or -1 for none.
    int32_t player;
} INPUT_CONTROLLER;

// Creates the input state for schema, with every player's bindings set to
// the defaults. The schema must stay valid while the input exists. More
// than INPUT_MAX_ACTIONS actions, a default that does not parse, or a
// default chord with a whole stick in it, is fatal.
//
// Player 1 gets every device's defaults. Player 2, when the schema declares
// two players, gets the gamepad and joystick defaults but no keyboard
// bindings, so that both players can use the keyboard's defaults only by
// rebinding; this keeps one game's two players from fighting over the same
// keys by default.
INPUT_STATE *Input_Create(
    const INPUT_SCHEMA *schema, const ALLOCATOR *allocator);

void Input_Destroy(INPUT_STATE *input);

// Adds another schema whose actions are read from the same devices, such as
// core's hotkeys. Chords suppress shorter bindings across every schema. An
// added schema may have only one player, who reads the controllers of
// player 1. Adding the same schema twice, or more than INPUT_MAX_SCHEMAS
// schemas, is fatal.
//
// The functions that take a schema accept only the pointers given to
// Input_Create or Input_AddSchema; any other pointer is fatal.
void Input_AddSchema(INPUT_STATE *input, const INPUT_SCHEMA *schema);

// Reads the threshold, the deadzone and the controller assignments from
// [input], and the bindings of every schema from the sections under its
// prefix: [<prefix>.keyboard], [<prefix>.gamepad] and [<prefix>.joystick]
// for player 1, and [<prefix>.p2.keyboard] and so on for player 2. Each value
// is a comma-separated list of binding names. A missing key keeps the defaults.
// An empty value means no bindings. An unknown name, or a chord with a whole
// stick in it, is a warning and is skipped.
void Input_Load(INPUT_STATE *input, const INI *ini);

// Writes the bindings of every schema, the threshold, the deadzone and the
// controller assignments into ini, in the sections that Input_Load reads.
void Input_Save(const INPUT_STATE *input, INI *ini);

// Reads every device once, evaluates every schema from that reading, and
// writes each player's input struct of the schema of Input_Create into out,
// one after another. A tap that starts and ends between two updates is not
// seen.
void Input_Update(INPUT_STATE *input, void *out);

// Writes the input of schema, from the last Input_Update, into out.
void Input_Read(
    const INPUT_STATE *input, const INPUT_SCHEMA *schema, void *out);

// Writes into out the input of the schema of Input_Create that one kind of
// device gave in the last update, as if no other device existed.
void Input_GetDevice(const INPUT_STATE *input, INPUT_DEVICE device, void *out);

// Sets each button of out that is true in current and false in previous.
// Every stick of out is 0. The function is pure, so the same previous and
// current give the same out, also in a replay.
void Input_Debounce(
    const INPUT_SCHEMA *schema, const void *previous, const void *current,
    void *out);

// Lists the connected controllers into out, up to capacity, and returns
// how many there are.
size_t Input_ListControllers(
    const INPUT_STATE *input, INPUT_CONTROLLER *out, size_t capacity);

// Assigns a controller to a player, or to none with -1. By default, a
// one-player game assigns every controller to player 1, and a two-player
// game assigns them in the order they connect. An assignment is remembered
// by the controller's GUID.
void Input_AssignController(INPUT_STATE *input, uint32_t id, int32_t player);
