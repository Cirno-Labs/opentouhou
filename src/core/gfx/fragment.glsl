#ifdef GL_ES
precision highp float;
precision highp int;
#endif

in vec4 v_color;
in vec2 v_uv;
in float v_depth;

uniform sampler2D u_texture;
uniform int u_textured;
uniform int u_color_add;
uniform int u_alpha_test;
uniform int u_fog;
uniform vec2 u_fog_range;
uniform vec3 u_fog_color;

out vec4 out_color;

void main()
{
    vec4 color = v_color;
    if (u_textured != 0) {
        vec4 texel = texture(u_texture, v_uv);
        color.rgb = u_color_add != 0
            ? min(texel.rgb + v_color.rgb, vec3(1.0))
            : texel.rgb * v_color.rgb;
        color.a = texel.a * v_color.a;
    }
    if (u_alpha_test != 0 && color.a < 0.5 / 255.0) {
        discard;
    }
    if (u_fog != 0) {
        float factor = clamp(
            (u_fog_range.y - v_depth) / (u_fog_range.y - u_fog_range.x),
            0.0, 1.0);
        color.rgb = mix(u_fog_color, color.rgb, factor);
    }
    out_color = color;
}
