#pragma once

// Umbrella for the boss/ module's private parts.

#include "th02/boss.h"
#include "th02/boss/private_meira.h"
#include "th02/boss/private_rika.h"

// Writable view of the live boss or midboss state for the module's parts.
extern BOSS_STATE *const p_BossState;
