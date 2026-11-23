#pragma once

#include "th01/ptn.h"
#include "th01/video.h"

#define ORB_WIDTH 32
#define ORB_HEIGHT 32
#define ORB_VISUAL_WIDTH 25
#define ORB_VISUAL_HEIGHT 25
#define ORB_HITBOX_MIN_X 8
#define ORB_HITBOX_MIN_Y 8
#define ORB_HITBOX_MAX_X 24
#define ORB_HITBOX_MAX_Y 24
#define ORB_BOUNDS_MIN_X 0
#define ORB_BOUNDS_MAX_X 608
#define ORB_BOUNDS_MIN_Y 64
#define ORB_BOUNDS_MAX_Y 368
#define ORB_IMAGE_COUNT 4
#define ORB_FRAMES_PER_IMAGE 3
#define ORB_FIRST_IMAGE 3

typedef enum {
    ORB_X_STILL = 0,
    ORB_X_LEFT_4 = 1,
    ORB_X_RIGHT_4 = 2,
    ORB_X_LEFT_8 = 3,
    ORB_X_RIGHT_8 = 4,
} ORB_VELOCITY_X;

typedef enum {
    ORB_FORCE_SURFACE = 0,
    ORB_FORCE_TOP = 1,
    ORB_FORCE_SHOT = 2,
    ORB_FORCE_IMMEDIATE = 3,
} ORB_FORCE;

typedef struct {
    int16_t left, top, previous_left, previous_top;
    int16_t rotation_frame, force_frame, frames_outside_portal;
    bool in_portal;
    ORB_VELOCITY_X velocity_x;
    double force, velocity_y;
} ORB;

// Restarts flight at (600, 280), force -8, frame zero and leftward speed 4.
// Sets both coordinate pairs. Preserves rotation, portal state, elapsed
// outside-portal frames and the last stored Y velocity. Zero-initialize
// the state once before its first use; stage restarts retain these fields.
void Orb_ResetFlight(ORB *orb);

// Advances the main-loop force counter, including while in a portal.
// Call before the composite, rather than as part of its motion phase.
void Orb_AdvanceForceFrame(ORB *orb);

// Stores the binary64 gravity result clamped to +/-16. Returns the
// separately evaluated, unclamped pixel displacement, truncated directly
// from 64-significant-bit arithmetic. Finite forces must have magnitude
// below 2^62 so that the original signed 64-bit conversion is defined.
int16_t Orb_UpdateVelocityY(ORB *orb);

// Consumes the shared master.lib RNG only for an early surface bounce
// from zero horizontal speed. Both draws occur even if the first changes
// the speed; the second can overwrite it. Resets the force frame.
void Orb_NewForce(ORB *orb, ORB_FORCE mode, double immediate, uint32_t *rng);
void Orb_ReflectX(ORB *orb);
void Orb_ReflectY(ORB *orb, uint32_t *rng);

// Moves by the requested speed, but reflects the stored speed at walls.
// Leaves overshoot in place and tests both walls independently.
void Orb_MoveX(ORB *orb, ORB_VELOCITY_X requested);
int16_t Orb_Image(const ORB *orb);

// Performs motion, rotation and playfield bounces before the VSync wait.
// Skips them while in a portal. Resets the shared card combo on a bottom
// bounce. Rotation uses the speed after wall reversal; image 3 lasts one
// frame. Allocates nothing and reads only the supplied state and RNG.
void Orb_AdvanceMotion(ORB *orb, uint32_t *rng, int16_t *card_combo);

typedef enum {
    ORB_REPEL_NONE = 0,
    ORB_REPEL_MAX = 26,
    ORB_REPEL_3_X_UNCHANGED = 100,
    ORB_REPEL_3_X_LEFT_4 = 101,
    ORB_REPEL_3_X_RIGHT_4 = 102,
    ORB_REPEL_3_X_RIGHT_8 = 103,
    ORB_REPEL_3_X_LEFT_8 = 104,
} ORB_REPEL;

typedef struct {
    int16_t left, top;
    bool invincible, invincible_against_orb;
} ORB_PLAYER;

// Returns whether contact kills the player. Uses strict top-left delta
// bounds of 24 by 16 for lethal contact and 40 by 32 for repulsion.
// Repulsion ignores invincibility. Friction below 100 gives force
// -13 + friction/2; 100 and above gives -10, with X overrides for 101..104.
// At equal X, ordinary repulsion draws twice; either successful check
// selects left 4. Coordinates may include normal playfield wall overshoot.
bool Orb_PlayerHitTest(
    ORB *orb, const ORB_PLAYER *player, int16_t repel_friction, uint32_t *rng);

typedef enum {
    ORB_ACTION_DONE,
    ORB_ACTION_DELAY_ONE,
    ORB_ACTION_PELLETS,
    ORB_ACTION_CARDS_HITTEST,
    ORB_ACTION_OBSTACLES,
    ORB_ACTION_PLAYER_HITTEST,
    ORB_ACTION_ERASE,
    ORB_ACTION_CARDS_RENDER,
    ORB_ACTION_PUT,
} ORB_ACTION_KIND;

typedef struct {
    ORB_ACTION_KIND kind;
    int16_t left, top, image;
} ORB_ACTION;

typedef struct {
    int32_t phase;
    bool boss;
} ORB_COMPOSITE;

// Begins the composite after motion, with the zero-based source stage ID.
// Requires the caller to execute every yielded action before requesting
// the next. Stage executes collision and rendering actions; the iterator
// itself implements no collision behavior.
// The delay consumes count1 when ready; yield only while count1 is zero.
// Simulation timing belongs to the caller; no forced next-edge wait applies.
ORB_COMPOSITE Orb_BeginComposite(
    ORB *orb, int16_t stage_id, uint32_t *rng, int16_t *card_combo);

// Reads portal and player-hit state afresh at each phase, including after
// obstacle callbacks. Erase uses the current image at the previous position.
// Updates previous coordinates when yielding DONE, also in a portal.
// Do not start another composite until DONE has been returned.
ORB_ACTION Orb_NextAction(ORB *orb, ORB_COMPOSITE *composite, bool player_hit);

// Executes only PUT or ERASE actions using immutable stage PTN images.
// Requires at least ORB_FIRST_IMAGE + ORB_IMAGE_COUNT images. Quantizes X down
// to eight pixels. ERASE restores opaque sprite pixels from page 1 to page 0;
// PUT uses the selected drawing page. Horizontal overshoot wraps into the
// adjacent row as in linear planar memory. Discards pixels outside the
// visible page rather than aliasing another memory region. Allocates nothing.
void Orb_RenderAction(const PTN *sprites, ORB_ACTION action);
