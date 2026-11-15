#ifdef GL_ES
precision highp float;
precision highp int;
precision highp sampler2D;
#endif

in vec4 v_color;
in vec2 v_uv;
flat in vec4 v_rect;
in float v_fog;

uniform sampler2D u_texture;
uniform sampler2D u_palette;
uniform int u_indexed;
uniform int u_textured;
uniform int u_color_add;
uniform int u_alpha_test;
uniform int u_fog;
uniform vec3 u_fog_color;

out vec4 out_color;

void main()
{
    vec4 color = v_color;
    if (u_textured != 0) {
        vec4 texel = texture(u_texture, clamp(v_uv, v_rect.xy, v_rect.zw));
        if (u_indexed != 0) {
            int index = int(floor(texel.r * 255.0 + 0.5));
            texel = texelFetch(u_palette, ivec2(index, 0), 0);
        }
        color.rgb = u_color_add != 0
            ? min(texel.rgb + v_color.rgb, vec3(1.0))
            : texel.rgb * v_color.rgb;
        color.a = texel.a * v_color.a;
    }
    // Keep byte alpha 4 despite sampler normalization error.
    const float alpha_ref = uintBitsToFloat(floatBitsToUint(4.0 / 255.0) - 2u);
    if (u_alpha_test != 0 && color.a < alpha_ref) {
        discard;
    }
    if (u_fog != 0) {
        color.rgb = mix(u_fog_color, color.rgb, clamp(v_fog, 0.0, 1.0));
    }
    out_color = color;
}
