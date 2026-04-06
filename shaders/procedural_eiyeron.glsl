// @EFFECT name="Eiyeron Deform" index=46 desc="DEMOS AND COLORS plane deformation by Eiyeron" author="Eiyeron"

/**
DEMOS AND COLORS
By @Eiyeron
    Based on Illogical from Matrefeytontias, plane deformations on TI-83/84
    And some tunnel effects.

Use : Comment/Uncomment the defines as you wants, they'll enable/disable various effects in the shader.
**/

#define EIYERON_SPEED 0.25

vec3 eiyeronGetColors(vec2 position, float time) {
    return vec3(cos(position.x), sin(position.y), 1.0 - 0.5 * cos(time));
}

float eiyeronGetRainbowValue(vec2 position) {
    position.x = fract(0.16666 * abs(position.x));
    if (position.x > 0.5) position.x = 1.0 - position.x;
    return smoothstep(0.166666, 0.333333, position.x) * 0.5;
}

vec3 eiyeronGetRainbow(vec2 position) {
    return vec3(
        eiyeronGetRainbowValue(position + 3.0),
        eiyeronGetRainbowValue(position + 1.0),
        eiyeronGetRainbowValue(position + 5.0)
    );
}

float eiyeronGetCheckerboardColor(vec2 position, float time) {
    float xpos = floor(20.0 * position.x);
    float ypos = floor(10.0 * position.y);
    float col = mod(xpos, 2.0);
    if (mod(ypos, 2.0) > 0.0) {
        col = cos(xpos * ypos + time * 5.0);
    } else {
        col = sin(xpos * ypos + time * 5.0);
    }
    return col;
}

vec4 renderEiyeronDeform(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 position = st;

    position.y -= 0.10 * cos(position.x);
    position.y += 0.2 * cos(position.x / 2.0 + time * 0.37);

    float r = length(position);
    float a = atan(position.y, position.x);
    float factor = sin(time) / 2.0 + 0.5;

    a += sin(time / 20.0);

    float blend = clamp(energy + bass * 0.5, 0.0, 1.0);

    float u_plane = position.x / abs(position.y + 0.0001);
    float v_plane = 1.0 / abs(position.y + 0.0001);

    float u_tunnel = a;
    float v_tunnel = 1.0 / (r + 0.0001);

    float u = factor * u_plane + (1.0 - factor) * u_tunnel;
    float v = factor * v_plane + (1.0 - factor) * v_tunnel;

    u = mix(u, u_tunnel, blend * 0.5);
    v = mix(v, v_tunnel, blend * 0.5);

    vec2 p = vec2(u, v);
    p += vec2(EIYERON_SPEED * cos(time), EIYERON_SPEED * time);
    p += vec2(bass * 0.1, high * 0.05);

    vec3 color = vec3(1.0);
    color = eiyeronGetRainbow(p);
    color *= eiyeronGetColors(p, time);
    color *= vec3(
        sin(dot(p, position)),
        cos(dot(p, position)),
        sin(dot(p, position))
    );
    color *= vec3(eiyeronGetCheckerboardColor(p, time));

    float col = 0.0;
    for (float i = 0.0; i < 5.0; i += 1.0) {
        float ang = i * (kTwoPI / 5.0) * 61.95;
        col += cos(kTwoPI * (p.y * cos(ang) + p.x * sin(ang) + sin(time * 0.004) * 100.0));
    }
    col /= 3.0;
    color *= vec3(col);

    color *= 1.0 / (abs(v) + 0.1);
    color *= (2.0 - r);

    vec3 paletteMix = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + high * 0.2, 0.0, 1.0));
    color *= paletteMix;

    float intensity = 1.0 + energy * 1.2 + bass * 0.6 + high * 0.4;
    vec3 bandGlow = vec3(bass * 0.25, mid * 0.2, high * 0.35);
    color = color * intensity + bandGlow;

    float scanY = gl_FragCoord.y;
    color *= mod(scanY, 2.0);

    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.6 + energy * 0.2, 0.0, 1.0);
    return vec4(color, alpha);
}
