#pragma once

// One-color shapes drawn through the charger: boxes, lines, boxes with
// round corners and polygons, clipped the way the original clips them.

#include "th02/clip.h"
#include "th02/grcg.h"

#include "pc98/zun/math16.h"

// Fills an inclusive box, clipped in signed-word coordinates that wrap.
void Draw_BoxFill(
    const CLIP *clip, int16_t left, int16_t top, int16_t right, int16_t bottom);

// Draws a horizontal line; unlike Draw_BoxFill, X orders after the clip
// origin.
void Draw_HLine(const CLIP *clip, int16_t left, int16_t right, int16_t y);

// Draws a clipped vertical line.
void Draw_VLine(const CLIP *clip, int16_t x, int16_t top, int16_t bottom);

// Draws a clipped line; the clip intersections must fit a signed word.
void Draw_Line(
    const CLIP *clip, int16_t left, int16_t top, int16_t right, int16_t bottom);

// Fills a box with corners of radius up to half its size; corners wrap
// their signed-word counter, as in retail.
void Draw_RoundBoxFill(
    const CLIP *clip, int16_t left, int16_t top, int16_t right, int16_t bottom,
    uint16_t radius);

// Fills a clipped polygon, bottom row included; with charging disabled,
// its edges replace whole blue-plane words.
void Draw_PolygonFill(
    const CLIP *clip, const MATH16_VECTOR *vertices, int32_t count);
