#include "core/math/vec.h"

#include "test.h"

#include <math.h>

static bool M_Near(const float a, const float b)
{
    return fabsf(a - b) < 0.0005f;
}

TEST(add_sub_and_scale)
{
    const VEC3 a = { 1.0f, 2.0f, 3.0f };
    const VEC3 b = { 4.0f, 5.0f, 6.0f };
    const VEC3 sum = Vec3_Add(a, b);
    CHECK(sum.x == 5.0f && sum.y == 7.0f && sum.z == 9.0f);
    const VEC3 diff = Vec3_Sub(b, a);
    CHECK(diff.x == 3.0f && diff.y == 3.0f && diff.z == 3.0f);
    const VEC3 scaled = Vec3_Scale(a, 2.0f);
    CHECK(scaled.x == 2.0f && scaled.y == 4.0f && scaled.z == 6.0f);
}

TEST(dot_and_length)
{
    const VEC3 a = { 1.0f, 2.0f, 3.0f };
    const VEC3 b = { 4.0f, 5.0f, 6.0f };
    CHECK(Vec3_Dot(a, b) == 32.0f);
    const VEC3 v = { 3.0f, 4.0f, 0.0f };
    CHECK(Vec3_Length(v) == 5.0f);
}

TEST(normalize_gives_unit_length)
{
    const VEC3 v = { 3.0f, 4.0f, 0.0f };
    const VEC3 n = Vec3_Normalize(v);
    CHECK(M_Near(n.x, 0.6f));
    CHECK(M_Near(n.y, 0.8f));
    CHECK(M_Near(Vec3_Length(n), 1.0f));
}

TEST(identity_leaves_a_point_unchanged)
{
    const MAT4 identity = Mat4_Identity();
    const VEC3 v = { 1.0f, -2.0f, 3.0f };
    const VEC3 r = Vec3_TransformCoord(v, &identity);
    CHECK(r.x == v.x && r.y == v.y && r.z == v.z);
}

TEST(multiply_composes_left_to_right)
{
    // a translates by (1,0,0), b translates by (0,2,0); v*(a*b) should
    // apply a's translation then b's.
    MAT4 a = Mat4_Identity();
    a.m[3][0] = 1.0f;
    MAT4 b = Mat4_Identity();
    b.m[3][1] = 2.0f;
    const MAT4 ab = Mat4_Multiply(&a, &b);
    const VEC3 v = { 0.0f, 0.0f, 0.0f };
    const VEC3 r = Vec3_TransformCoord(v, &ab);
    CHECK(r.x == 1.0f && r.y == 2.0f && r.z == 0.0f);
}

TEST(lookat_places_the_camera_at_the_origin)
{
    const VEC3 eye = { 0.0f, 0.0f, -5.0f };
    const VEC3 at = { 0.0f, 0.0f, 0.0f };
    const VEC3 up = { 0.0f, 1.0f, 0.0f };
    const MAT4 view = Mat4_LookAtLH(eye, at, up);
    // The eye transforms to the origin of view space.
    const VEC3 r = Vec3_TransformCoord(eye, &view);
    CHECK(M_Near(r.x, 0.0f));
    CHECK(M_Near(r.y, 0.0f));
    CHECK(M_Near(r.z, 0.0f));
    // A point straight ahead of the camera lands on the +z axis.
    const VEC3 ahead = { 0.0f, 0.0f, 0.0f };
    const VEC3 ra = Vec3_TransformCoord(ahead, &view);
    CHECK(M_Near(ra.x, 0.0f));
    CHECK(M_Near(ra.y, 0.0f));
    CHECK(ra.z > 0.0f);
}

TEST(perspective_matches_the_documented_formula)
{
    const float fov_y = 1.0471976f; // pi/3
    const float aspect = 1.3333333f;
    const float near_z = 1.0f;
    const float far_z = 100.0f;
    const MAT4 m = Mat4_PerspectiveFovLH(fov_y, aspect, near_z, far_z);
    const float y_scale = 1.0f / tanf(fov_y / 2.0f);
    const float x_scale = y_scale / aspect;
    CHECK(M_Near(m.m[0][0], x_scale));
    CHECK(M_Near(m.m[1][1], y_scale));
    CHECK(M_Near(m.m[2][2], far_z / (far_z - near_z)));
    CHECK(M_Near(m.m[3][2], -near_z * far_z / (far_z - near_z)));
    CHECK(m.m[2][3] == 1.0f);
    CHECK(m.m[0][1] == 0.0f && m.m[1][0] == 0.0f);
}

TEST(project_maps_the_origin_to_the_viewport_centre)
{
    const VIEWPORT viewport = { 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f };
    const MAT4 world = Mat4_Identity();
    const VEC3 eye = { 0.0f, 0.0f, -5.0f };
    const VEC3 at = { 0.0f, 0.0f, 0.0f };
    const VEC3 up = { 0.0f, 1.0f, 0.0f };
    const MAT4 view = Mat4_LookAtLH(eye, at, up);
    const MAT4 projection =
        Mat4_PerspectiveFovLH(1.0471976f, 640.0f / 480.0f, 1.0f, 100.0f);
    const VEC3 v = { 0.0f, 0.0f, 0.0f };
    const VEC3 r = Vec3_Project(v, &viewport, &projection, &view, &world);
    CHECK(M_Near(r.x, 320.0f));
    CHECK(M_Near(r.y, 240.0f));
}
