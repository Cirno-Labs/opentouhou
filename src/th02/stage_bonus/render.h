#pragma once

#include "th02/stage_bonus/assets.h"
#include "th02/stage_bonus/logic.h"

// Returns a view that draws the bonus with assets, with six total digits
// in normal play or seven in Extra.
STAGE_BONUS_VIEW StageBonusRender_View(const STAGE_BONUS_ASSETS *assets);
