// SPDX-License-Identifier: GPL-3.0-only

#include "core/math/vec.h"

#include "core/base/types.h"
#include "core/math/x87.h"

VEC3 Vec3_Add(const VEC3 a, const VEC3 b)
{
    return (VEC3) { a.x + b.x, a.y + b.y, a.z + b.z };
}

VEC3 Vec3_Sub(const VEC3 a, const VEC3 b)
{
    return (VEC3) { a.x - b.x, a.y - b.y, a.z - b.z };
}

VEC3 Vec3_Scale(const VEC3 v, const float s)
{
    return (VEC3) { v.x * s, v.y * s, v.z * s };
}

float Vec3_Dot(const VEC3 a, const VEC3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Vec3_Length(const VEC3 v)
{
    return X87_Sqrt(Vec3_Dot(v, v));
}

VEC3 Vec3_Normalize(const VEC3 v)
{
    return Vec3_Scale(v, 1.0f / Vec3_Length(v));
}

static VEC3 M_Cross(const VEC3 a, const VEC3 b)
{
    return (VEC3) {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

VEC3 Vec3_TransformCoord(const VEC3 v, const MAT4 *const m)
{
    const float x =
        v.x * m->m[0][0] + v.y * m->m[1][0] + v.z * m->m[2][0] + m->m[3][0];
    const float y =
        v.x * m->m[0][1] + v.y * m->m[1][1] + v.z * m->m[2][1] + m->m[3][1];
    const float z =
        v.x * m->m[0][2] + v.y * m->m[1][2] + v.z * m->m[2][2] + m->m[3][2];
    const float w =
        v.x * m->m[0][3] + v.y * m->m[1][3] + v.z * m->m[2][3] + m->m[3][3];
    return (VEC3) { x / w, y / w, z / w };
}

VEC3 Vec3_Project(
    const VEC3 v, const VIEWPORT *const viewport, const MAT4 *const projection,
    const MAT4 *const view, const MAT4 *const world)
{
    MAT4 m = Mat4_Multiply(world, view);
    m = Mat4_Multiply(&m, projection);
    const VEC3 clip = Vec3_TransformCoord(v, &m);
    return (VEC3) {
        viewport->x + (1.0f + clip.x) * viewport->width / 2.0f,
        viewport->y + (1.0f - clip.y) * viewport->height / 2.0f,
        viewport->min_z + clip.z * (viewport->max_z - viewport->min_z),
    };
}

MAT4 Mat4_Identity(void)
{
    MAT4 m = { 0 };
    m.m[0][0] = 1.0f;
    m.m[1][1] = 1.0f;
    m.m[2][2] = 1.0f;
    m.m[3][3] = 1.0f;
    return m;
}

MAT4 Mat4_Multiply(const MAT4 *const a, const MAT4 *const b)
{
    MAT4 result;
    for (int32_t i = 0; i < 4; i++) {
        for (int32_t j = 0; j < 4; j++) {
            float sum = 0.0f;
            for (int32_t k = 0; k < 4; k++) {
                sum += a->m[i][k] * b->m[k][j];
            }
            result.m[i][j] = sum;
        }
    }
    return result;
}

MAT4 Mat4_RotationQuaternion(const VEC4 q)
{
    MAT4 m = Mat4_Identity();
    m.m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    m.m[0][1] = 2.0f * (q.x * q.y + q.z * q.w);
    m.m[0][2] = 2.0f * (q.x * q.z - q.y * q.w);
    m.m[1][0] = 2.0f * (q.x * q.y - q.z * q.w);
    m.m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    m.m[1][2] = 2.0f * (q.y * q.z + q.x * q.w);
    m.m[2][0] = 2.0f * (q.x * q.z + q.y * q.w);
    m.m[2][1] = 2.0f * (q.y * q.z - q.x * q.w);
    m.m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    return m;
}

MAT4 Mat4_LookAtLH(const VEC3 eye, const VEC3 at, const VEC3 up)
{
    const VEC3 z_axis = Vec3_Normalize(Vec3_Sub(at, eye));
    const VEC3 x_axis = Vec3_Normalize(M_Cross(up, z_axis));
    const VEC3 y_axis = M_Cross(z_axis, x_axis);

    MAT4 m = Mat4_Identity();
    m.m[0][0] = x_axis.x;
    m.m[1][0] = x_axis.y;
    m.m[2][0] = x_axis.z;
    m.m[0][1] = y_axis.x;
    m.m[1][1] = y_axis.y;
    m.m[2][1] = y_axis.z;
    m.m[0][2] = z_axis.x;
    m.m[1][2] = z_axis.y;
    m.m[2][2] = z_axis.z;
    m.m[3][0] = -Vec3_Dot(x_axis, eye);
    m.m[3][1] = -Vec3_Dot(y_axis, eye);
    m.m[3][2] = -Vec3_Dot(z_axis, eye);
    return m;
}

MAT4 Mat4_PerspectiveFovLH(
    const float fov_y, const float aspect, const float near_z,
    const float far_z)
{
    const float y_scale = 1.0f / X87_Tan(fov_y / 2.0f);
    const float x_scale = y_scale / aspect;

    MAT4 m = { 0 };
    m.m[0][0] = x_scale;
    m.m[1][1] = y_scale;
    m.m[2][2] = far_z / (far_z - near_z);
    m.m[3][2] = -near_z * far_z / (far_z - near_z);
    m.m[2][3] = 1.0f;
    return m;
}
