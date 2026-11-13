#pragma once

#include "core/base/types.h"
#include "core/math/vec.h"

typedef struct GFX_TEXTURE GFX_TEXTURE;

typedef struct {
    uint8_t red, green, blue, alpha;
} GFX_COLOR;

typedef struct {
    int32_t x, y, width, height;
} GFX_RECT;

typedef struct {
    VEC3 position; // in the space of the view that draws it
    GFX_COLOR color;
    VEC2 uv;
} GFX_VERTEX;

typedef enum {
    GFX_BLEND_NONE,
    GFX_BLEND_ALPHA,
    GFX_BLEND_ADD,
} GFX_BLEND;

// Combines the texture colour with the vertex colour; alpha always
// multiplies. Without a texture, the vertex colour is drawn as it is.
typedef enum {
    GFX_COLOR_MODULATE,
    GFX_COLOR_ADD,
} GFX_COLOR_OP;

typedef enum {
    GFX_DEPTH_OFF,
    GFX_DEPTH_LESS_EQUAL,
    GFX_DEPTH_ALWAYS,
} GFX_DEPTH_TEST;

// Picks the texel that a pixel shows: the nearest, as the original's
// sprites need, or the blend of the four nearest, which keeps thin lines of
// a texture drawn smaller than its size.
typedef enum {
    GFX_FILTER_NEAREST,
    GFX_FILTER_LINEAR,
} GFX_FILTER;

// Holds everything that changes how a quad is drawn, apart from its
// texture. Small and plain, so a backend can build one pipeline per state.
typedef struct {
    GFX_BLEND blend;
    GFX_COLOR_OP color_op;
    GFX_DEPTH_TEST depth_test;
    bool depth_write;
    bool alpha_test; // discards pixels with alpha 0
    bool repeat; // texture coordinates wrap instead of clamping
    GFX_FILTER filter;
} GFX_STATE;

typedef struct {
    bool enabled;
    float begin, end; // view-space depth
    GFX_COLOR color;
} GFX_FOG;

// Places drawing on the logical screen. Positions go through view and then
// projection as row vectors; viewport maps the result to logical-screen
// pixels and clips to it.
typedef struct {
    MAT4 view, projection;
    VIEWPORT viewport;
    GFX_FOG fog;
    // Stays the same across ticks; 0 is never blended. The nth view of an
    // id in a tick blends with the nth view of that id in the tick before.
    uint32_t id;
} GFX_VIEW;

// Describes a 2D sprite's placement so that rotation and scale blend
// exactly between ticks.
typedef struct {
    VEC2 center; // logical-screen pixels
    VEC2 scale;
    // Turns around center, in radians: a positive angle turns +x toward
    // -y, as x' = x cos + y sin, y' = -x sin + y cos.
    float rotation;
} GFX_MOTION;

typedef struct {
    int32_t width, height; // window pixels
    bool integer_scale; // whole multiples when they fit
    GFX_COLOR border;
} GFX_OUTPUT;
