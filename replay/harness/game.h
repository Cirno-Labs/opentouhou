#pragma once

#include <stddef.h>
#include <stdint.h>

// Describes one game to the harness. Shared code knows a game only
// through this table and the game's port (backend.h).

// Names one bit of the button word that scripts hold.
// On the native backend, the name is also the action in the game's
// INPUT_SCHEMA.
typedef struct {
    const char *name;
    uint32_t mask;
} BINDING;

typedef enum {
    COLUMN_U32,
    COLUMN_I32,
    COLUMN_F32,
    COLUMN_HASH,
} COLUMN_KIND;

// Names one state value that the game samples after every calc, or one
// choice of the run, such as its difficulty.
typedef struct {
    const char *name;
    COLUMN_KIND kind;
} COLUMN;

// Gives one value of a case's [start]: the index of the game's start field
// and its value, held as a row holds a column of the field's kind.
typedef struct {
    uint32_t field;
    uint32_t value;
} START_VALUE;

typedef struct {
    // Names the game in cases and programs: "th06".
    const char *id;
    // Gives the retail version that the game matches, such as "1.02h".
    const char *version;
    // Gives the frames per second that the frame clock advances by.
    double frame_rate;
    // Gives the size of screenshots, the game's logical screen.
    int32_t width;
    int32_t height;
    // Gives the frames that a fast run draws before each screenshot,
    // enough for the screen to hold nearly what the screenshot shows.
    uint32_t draw_frames;
    const BINDING *bindings;
    size_t binding_count;
    const COLUMN *columns;
    size_t column_count;
    // Lists the run's choices, which a port samples after the columns and
    // a case keeps once, as the last row has them.
    const COLUMN *choices;
    size_t choice_count;
    // Changes whenever a column or choice is added, removed or changes
    // meaning.
    uint32_t columns_version;
    // Lists the fields of the state that a case can set as the game starts
    // its first run, such as the stage, each of kind U32 or I32. Every
    // backend's port applies them at the same point, in memory only.
    const COLUMN *starts;
    size_t start_count;
    // Tells whether the game shows a frame rate, which its ports then
    // route through Driver_Fps.
    bool shows_fps;
} HARNESS_GAME;

// Returns the binding named name, or nullptr.
const BINDING *Game_FindBinding(const HARNESS_GAME *game, const char *name);

// Returns the index of the start field named name, or -1.
int32_t Game_FindStart(const HARNESS_GAME *game, const char *name);

// Reads text as a value of kind, U32 or I32, into *out. Returns false when
// it is not one.
bool Game_ParseValue(COLUMN_KIND kind, const char *text, uint32_t *out);
