#pragma once

#include "th02/entity.h"

// Clears the fire target.
void Shots_ResetTarget(void);

// Sets the weapon level directly, for item pickups and penalties.
void Shots_SetWeaponLevel(uint8_t level);

// Points homing fire at a point, for boss intros and attacks.
void Shots_SetTarget(ENTITY_POINT target);

// Fires for keys, keeping retail's diagonal precedence, signed speed
// quirks and first-cycle firing priority.
void Shots_Fire(uint8_t back, uint16_t keys);
