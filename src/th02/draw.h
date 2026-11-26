#pragma once

// Drawing into the graphics pages: shapes, dots and circles go through the
// charger (th02/grcg.h), sprites and glyphs write their pixels directly, and
// each leaves the charger as the original leaves it. DrawText reads the text
// that the glyph draws take.

#include "th02/draw/circles.h"
#include "th02/draw/glyphs.h"
#include "th02/draw/shapes.h"
#include "th02/draw/sprites.h"
#include "th02/draw/text.h"
