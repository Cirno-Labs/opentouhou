#pragma once

// Vectors, matrices, and the subset of D3DX8's vector and matrix maths
// that the games call (D3DXVec3Normalize, D3DXMatrixLookAtLH,
// D3DXMatrixPerspectiveFovLH, D3DXVec3Project, and the pieces they build
// on). Built on core/math/x87 for sqrt and tan, so these reproduce the
// x87 at 24-bit precision too.
//
// Operation order within each function follows the formulas in the D3DX8
// documentation (Wine's d3dx8/d3dx9 maths is a readable reference for
// those formulas, under LGPL-2.1-or-later; the code here is written from
// the documented formulas, not from Wine's). Native D3DX8's exact
// operation order is NOT yet verified against the real DLL: D3DX8 also
// picks CPU-specific code paths at runtime (for example, an SSE
// reciprocal-square-root approximation inside D3DXVec3Normalize), which
// this does not reproduce. Functions below whose order is a guess say so;
// replay traces will settle it later.

typedef struct {
    float x, y;
} VEC2;

typedef struct {
    float x, y, z;
} VEC3;

typedef struct {
    float x, y, z, w;
} VEC4;

// Row-major, row-vector, like D3D: a point transforms as v * m, and
// Mat4_Multiply(a, b) composes them so that v * Mat4_Multiply(a, b) ==
// (v * a) * b, the same order D3DXMatrixMultiply uses.
typedef struct {
    float m[4][4];
} MAT4;

// A D3DVIEWPORT8 (x, y, width, height in pixels; min_z and max_z the depth
// range, usually 0 and 1).
typedef struct {
    float x, y, width, height, min_z, max_z;
} VIEWPORT;

VEC3 Vec3_Add(VEC3 a, VEC3 b);
VEC3 Vec3_Sub(VEC3 a, VEC3 b);
VEC3 Vec3_Scale(VEC3 v, float s);
float Vec3_Dot(VEC3 a, VEC3 b);

// Returns sqrt(Vec3_Dot(v, v)), as D3DXVec3Length does.
float Vec3_Length(VEC3 v);

// Returns v scaled by 1 / Vec3_Length(v). The result is unspecified for a
// zero vector, as D3DXVec3Normalize's is.
VEC3 Vec3_Normalize(VEC3 v);

// Transforms v as a point (w = 1) by m and divides by the transformed w,
// as D3DXVec3TransformCoord does.
VEC3 Vec3_TransformCoord(VEC3 v, const MAT4 *m);

// Transforms v by world, then view, then projection (D3DXMatrixMultiply
// composes the three in that order into one matrix, then
// Vec3_TransformCoord applies it), and maps the result from clip space
// into the viewport: x and y from [-1, 1] to [viewport.x, viewport.x +
// viewport.width] (and the y equivalent, flipped as D3D's viewport is),
// and z from [0, 1] to [viewport.min_z, viewport.max_z]. This is what
// D3DXVec3Project does.
VEC3 Vec3_Project(
    VEC3 v, const VIEWPORT *viewport, const MAT4 *projection, const MAT4 *view,
    const MAT4 *world);

MAT4 Mat4_Identity(void);

// Returns a * b, so that a point transforms the same through the result
// as through a and then b: v * Mat4_Multiply(a, b) == (v * a) * b.
MAT4 Mat4_Multiply(const MAT4 *a, const MAT4 *b);

// Builds the rotation matrix of the unit quaternion q (x, y, z the
// vector part, w the scalar), as D3DXMatrixRotationQuaternion does. The
// operation order is a guess.
MAT4 Mat4_RotationQuaternion(VEC4 q);

// Builds a left-handed view matrix looking from eye toward at, with up
// giving the camera's up direction, as D3DXMatrixLookAtLH does.
MAT4 Mat4_LookAtLH(VEC3 eye, VEC3 at, VEC3 up);

// Builds a left-handed perspective projection matrix, as
// D3DXMatrixPerspectiveFovLH does: fov_y is the vertical field of view in
// radians, aspect is width / height, and near_z and far_z are the depth
// clip planes.
MAT4 Mat4_PerspectiveFovLH(
    float fov_y, float aspect, float near_z, float far_z);
