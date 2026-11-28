#pragma once

#include "th01/stage/stage.h"

// Shares the Orb composite state between stage_orb.c and its tests.
extern STAGE_ORB_COMPOSITE p_OrbComposite;

// Writable view of g_Stage for the module's own parts and their tests.
extern STAGE_GAMEPLAY *const p_Stage;
