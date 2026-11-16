#pragma once

#include "th01/frame.h"
#include "th01/player.h"

typedef enum {
    MISS_DONE,
    MISS_SHAKE_WAIT,
    MISS_SHAKE_DELAY,
    MISS_SCROLL_RESET_WAIT,
    MISS_EFFECT_DELAY,
} MISS_PHASE;

typedef struct {
    MISS_PHASE phase;
    FRAME_WAIT wait;
    int16_t frame, effect_left, target_right, target_left;
} MISS;

typedef struct {
    VIDEO *video;
    PLAYER *player;
    GAME_INPUT *input;
    const ORB *orb;
    SHOTS *shots;
    uint32_t *rng;
    const PTN *miko, *stage;
    int32_t difficulty;
    int16_t remaining_lives, extra_bombs;
    void (*pellets_decay)(void *user);
    void (*hud_lives)(void *user, int16_t previous);
    void (*hud_bombs)(void *user, int16_t previous);
    void (*timer_extend)(void *user);
    void (*pellet_speed_lower)(void *user, float minimum, float delta);
    void *user;
} MISS_CONTEXT;

// Validates all images used by the sequence before ticks. Assets belong to
// the caller's grouped lifetime and remain immutable throughout the miss.
RESULT Miss_ValidateSprites(const PTN *miko, const PTN *stage);

// Zero-initialize once. Begin restores only 32 by 32, redraws the default
// Orb and a randomly selected miss image, then waits for a fresh VSync.
// Caller must have decremented lives already. Required callbacks provide
// real pellet, HUD and timer operations; none has a default behavior.
// Keep supplied state alive and ordinary actor updates suspended until
// Resume returns true. Only logical VSync counters advance while waiting.
// The source's variable delays reuse count1, including nested raw waits.
// Successful updates allocate nothing and poll no device or host clock.
void Miss_Begin(
    MISS *miss, const MISS_CONTEXT *context, const FRAME_VSYNC *vsync);

// Resumes every ready phase in source order. Returns true at completion;
// repeated calls after completion do nothing. Preserves integer return
// drift, retained hit/invincibility/frame fields and the live Orb state.
// After completion, the outer life loop clears player.is_hit and sets
// invincibility_time to 150. These steps, invincibility sprites, life
// decrement and gameplay scene dispatch remain external. Player_Reset alone
// does not implement this sequence.
bool Miss_Resume(MISS *miss, const MISS_CONTEXT *context, FRAME_VSYNC *vsync);
