#pragma once

#include "core/math/vec.h"

// Returns a + b brought into [-pi, pi]. The two loops that bring it there
// stop after 18 turns between them, so a sum further out stays outside.
float Angle_AddNormalize(float a, float b);

// Rotates in by angle in the x-y plane into out and leaves out->z
// unchanged. out may be in: the new y is then computed from the new x,
// as in the original.
void Angle_Rotate2D(VEC3 *out, const VEC3 *in, float angle);
