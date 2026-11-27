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
    int16_t left;
    int16_t top;
    int16_t previous_left;
    int16_t previous_top;
    int16_t rotation_frame;
    int16_t force_frame;
    int16_t frames_outside_portal;
    bool in_portal;
    ORB_VELOCITY_X velocity_x;
    double force;
    double velocity_y;
} ORB;

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
    int16_t left;
    int16_t top;
    bool invincible;
    bool invincible_against_orb;
} ORB_PLAYER;

extern const ORB *const g_Orb;

// Zeroes the Orb for a new world.
void Orb_Init(void);

// Restarts flight at (600, 280) with force -8 and leftward speed 4.
// Zero-initialize the state once before first use.
void Orb_ResetFlight(void);

// Restarts flight as Orb_ResetFlight does, but keeps the previous position.
void Orb_Relaunch(void);

// Takes the orb out of a portal.
void Orb_LeavePortal(void);

// Moves the orb to a portal destination and takes it out of the portal.
void Orb_Teleport(int16_t left, int16_t top);

// Puts the orb in a portal, which hides it.
void Orb_EnterPortal(void);

// Moves the orb to top, unclamped.
void Orb_SetTop(int16_t top);

// Sets the horizontal velocity, as a deflection does.
void Orb_SetVelocityX(ORB_VELOCITY_X velocity);

// Stores the position as the previous one, which the next frame erases.
void Orb_KeepPosition(void);

// Advances the main-loop force counter, including while in a portal.
// Call before the composite, rather than as part of its motion phase.
void Orb_AdvanceForceFrame(void);

// Stores the gravity velocity clamped to +/-16 and returns the unclamped
// pixel displacement. Forces must have magnitude below 2^62.
int16_t Orb_UpdateVelocityY(void);

// Starts a new force and resets the force frame. Draws the shared RNG
// twice for an early bounce from zero speed; the second draw can win.
void Orb_NewForce(ORB_FORCE mode, double immediate);
void Orb_ReflectX(void);
void Orb_ReflectY(void);

// Moves by the requested speed, but reflects the stored speed at walls.
// Leaves overshoot in place and tests both walls independently.
void Orb_MoveX(ORB_VELOCITY_X requested);
int16_t Orb_Image(void);

// Moves, rotates and bounces the Orb unless it is in a portal.
// Resets card_combo on a bottom bounce.
void Orb_AdvanceMotion(int16_t *card_combo);

// Returns whether contact kills the player, and repels it on near contact,
// even while invincible; repel_friction sets the repulsion force.
bool Orb_PlayerHitTest(const ORB_PLAYER *player, int16_t repel_friction);

// Draws the Orb's current image, with X rounded down to eight pixels.
// Requires at least ORB_FIRST_IMAGE + ORB_IMAGE_COUNT images.
void Orb_Put(const PTN *sprites, int16_t left, int16_t top, int16_t image);
void Orb_Erase(const PTN *sprites, int16_t left, int16_t top, int16_t image);
