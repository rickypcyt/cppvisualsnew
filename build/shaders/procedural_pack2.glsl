vec4 renderFractalObject(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, time * 0.2);
    float r = length(p);
    float angle = atan(p.y, p.x);
    float warp = sin(angle * 3.0 + time * (1.0 + tempo * 0.2));
    float layers = fbm(p * 5.0 + warp);
    vec3 color = mix(uPrimaryColor, uSecondaryColor, layers);
    color += vec3(0.20, 0.10, 0.35) * warp * high;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.4 + (1.0 - smoothstep(0.0, 1.2, r)) * (0.5 + energy * 0.3) + layers * 0.3, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderPulsarTunnel(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float angle = atan(st.y, st.x);
    float spiral = sin(r * 12.0 - time * (2.0 + tempo) + angle * 4.0);
    vec3 color = mix(uPrimaryColor, uSecondaryColor, 0.4 + 0.3 * bass);
    color *= (0.6 + spiral * 0.4);
    color += vec3(0.20, 0.10, 0.30) * (1.0 - r) * (0.6 + energy * 0.4);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp((1.0 - r) * (0.8 + energy * 0.4) + spiral * 0.1, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderAuroraBloom(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    float band = sin((p.x + time * (0.6 + tempo * 0.2)) * 6.0);
    float curtain = smoothstep(-0.4 - high * 0.2, 0.4 + high * 0.2, p.y + 0.2 * band);
    vec3 base = mix(uPrimaryColor, vec3(0.10, 0.20, 0.35), 0.5);
    vec3 glow = mix(uSecondaryColor, vec3(0.95, 0.90, 1.0), 0.5 + high * 0.2);
    vec3 color = mix(base, glow, curtain);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(curtain * (0.5 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderRibbonScanlines(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, 0.3);
    float stripes = sin(p.y * (80.0 + high * 40.0) + time * (4.0 + tempo)) * 0.5 + 0.5;
    float ribbons = sin(p.x * 6.0 + time * (1.2 + tempo * 0.4)) * 0.5 + 0.5;
    vec3 colorA = mix(uPrimaryColor, uSecondaryColor, stripes);
    vec3 colorB = mix(uSecondaryColor, uPrimaryColor, ribbons);
    vec3 color = mix(colorA, colorB, 0.5);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.30 + stripes * 0.4 + ribbons * 0.3, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderKaleidoscopeFractal(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float angle = atan(st.y, st.x);
    float sectors = 6.0 + floor(mid * 10.0);
    float sectorAngle = 2.0 * PI / max(sectors, 1.0);
    angle = mod(angle, sectorAngle);
    angle = abs(angle - sectorAngle * 0.5);
    vec2 dir = vec2(cos(angle), sin(angle));
    vec2 p = dir * r;
    vec2 warp = vec2(
        fbm(p * (3.2 + high * 2.4) + time * 0.35),
        fbm(p * (2.6 + mid * 1.8) - time * 0.28)
    );
    p += warp * (0.6 + high * 0.7 + energy * 0.4);
    float n = fbm(p * (2.4 + energy * 1.3) + time * 0.25);
    float bloom = fbm(p * 5.0 - time * 0.45);
    vec3 base = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    vec3 accent = mix(uSecondaryColor, base.bgr, clamp(high * 0.7 + mid * 0.2, 0.0, 1.0));
    vec3 color = mix(base, accent, n);
    color += vec3(0.3, 0.15, 0.45) * bloom * (0.4 + high * 0.6);
    color += vec3(0.12, 0.19, 0.25) * smoothstep(0.0, 1.2, r) * (0.3 + energy * 0.4);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.3 + n * 0.6 + bloom * 0.25 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}
