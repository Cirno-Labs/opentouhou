#ifdef GL_ES
precision highp float;
precision highp int;
#endif

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec4 u_output;

out vec4 v_color;
out vec2 v_uv;
out float v_depth;

void main()
{
    vec4 view_position = u_view * vec4(a_position, 1.0);
    vec4 clip = u_projection * view_position;
    gl_Position = vec4(
        clip.xy * u_output.xy + clip.w * u_output.zw,
        clip.z * 2.0 - clip.w, clip.w);
    v_color = a_color;
    v_uv = a_uv;
    v_depth = view_position.z;
}
