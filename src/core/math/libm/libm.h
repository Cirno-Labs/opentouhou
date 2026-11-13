// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Declares the vendored double-precision trig functions that core/math/x87
// rounds to float. See libm_internal.h for where they come from. Private to
// core/math; nothing outside it calls these directly.
double otm_Sin(double x);
double otm_Cos(double x);
double otm_Tan(double x);
double otm_Atan2(double y, double x);
