// @EFFECT name="Nebula" index=11 desc="Nebula cloud effect with fbm noise" author="System"
// @EFFECT name="ASCII Ocean" index=1 desc="ASCII-style ocean waves" author="System"
// @EFFECT name="Sacred Geometry" index=2 desc="Sacred geometry flower pattern" author="System"
// @EFFECT name="Glitch Grid" index=3 desc="Digital glitch grid pattern" author="System"
// @EFFECT name="Chemical Flow" index=4 desc="Flowing chemical reaction diffusion" author="System"
// @EFFECT name="Crystal Lattice" index=5 desc="Crystal lattice structure" author="System"
// @EFFECT name="Phantom Fractals" index=6 desc="Phantom fractal bands" author="System"

vec4 renderNebula(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    float warp = time * (0.15 + tempo * 0.05);
    float n = fbm(p * (1.8 + energy * 0.6) + warp);
    float glow = fbm(p * 4.5 + warp * 0.6);
    vec3 base = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    vec3 accent = mix(uSecondaryColor, uPrimaryColor, n);
    vec3 color = mix(base, accent, 0.5 + 0.5 * n);
    color += vec3(0.15, 0.10, 0.20) * glow * (0.6 + energy * 0.6);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.25 + n * 0.6 + energy * 0.35, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderASCIIOcean(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    p.x += sin(st.y * 2.5 + time * 0.6) * 0.12;
    float surface = sin(p.x * 5.0 + time * (1.1 + tempo * 0.2));
    surface += sin((p.x + p.y) * 4.0 - time * (0.8 + mid * 0.3));
    surface += sin(p.y * 7.0 + time * 1.4) * 0.5;
    surface = surface * 0.3 + 0.5 + energy * 0.25 + bass * 0.15;
    surface = clamp(surface, 0.0, 1.0);
    float crest = smoothstep(0.35, 0.75, surface);
    vec3 deep = mix(uPrimaryColor, vec3(0.02, 0.05, 0.12), 0.7);
    vec3 foam = mix(uSecondaryColor, vec3(0.85, 0.95, 1.0), 0.5);
    vec3 color = mix(deep, foam, crest);
    color += vec3(0.04, 0.06, 0.10) * sin((st.y + time * 0.5) * 40.0) * 0.2;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.35 + crest * 0.45 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderSacredGeometry(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float angle = atan(st.y, st.x);
    float petals = 6.0 + floor(mid * 6.0);
    float radial = pow(abs(cos(petals * angle)), 2.5);
    float rings = sin(r * (22.0 + high * 8.0) + time * (1.2 + tempo * 0.3)) * 0.5 + 0.5;
    float mask = radial * rings * exp(-r * (1.4 - energy * 0.3));
    mask = clamp(mask, 0.0, 1.0);
    vec3 inner = mix(uSecondaryColor, vec3(0.85, 0.55, 0.35), 0.4 + high * 0.2);
    vec3 outer = mix(uPrimaryColor, vec3(0.05, 0.03, 0.12), 0.6);
    vec3 color = mix(outer, inner, mask);
    color += vec3(0.20, 0.10, 0.30) * mask * high;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(mask * (0.7 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderGlitchGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 grid = (st + 0.5) * 8.0;
    vec2 cell = floor(grid);
    vec2 cellUV = fract(grid) - 0.5;
    float jitter = hash(cell + floor(time * (1.5 + tempo)));
    float mask = smoothstep(0.45 + high * 0.2, 0.0, length(cellUV + (jitter - 0.5) * 0.3));
    float pulse = sin(time * (4.0 + tempo * 1.2) + cell.x * 0.8 + cell.y * 0.6) * 0.5 + 0.5;
    vec3 base = mix(uPrimaryColor, uSecondaryColor, hash(cell));
    vec3 color = base * (0.3 + 0.7 * pulse);
    color = mix(color, color.bgr, high * 0.4);
    color *= mask * (0.6 + energy * 0.6);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(mask * (0.45 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderChemicalFlow(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, time * (0.25 + tempo * 0.2));
    float flow = fbm(p * (3.0 + energy));
    float swirl = sin(p.x * 5.0 + time * 1.4) + cos(p.y * 7.0 - time * 1.1);
    vec3 color = mix(uPrimaryColor * 0.5, uSecondaryColor + vec3(bass * 0.2, mid * 0.15, high * 0.3), flow);
    color += vec3(0.08, 0.02, 0.12) * swirl * 0.2;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.30 + flow * 0.5 + energy * 0.3, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderCrystalLattice(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, time * 0.1);
    p *= 3.0;
    vec2 cell = fract(p) - 0.5;
    float node = exp(-12.0 * dot(cell, cell));
    float cross = exp(-30.0 * abs(cell.x)) + exp(-30.0 * abs(cell.y));
    float glow = node + 0.2 * cross;
    vec3 base = mix(uPrimaryColor, uSecondaryColor, 0.5 + 0.5 * bass);
    vec3 color = base * (0.5 + glow * (0.8 + energy * 0.5));
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(glow * (0.6 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderPhantomFractals(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float bands = sin(r * (18.0 + bass * 8.0) - time * (1.5 + tempo * 0.5));
    float fract = fbm(st * 6.0 + time * 0.4);
    float mask = smoothstep(0.0, 1.0, bands * 0.5 + 0.5) * (0.4 + fract);
    vec3 color = mix(uPrimaryColor * 0.4, uSecondaryColor, mask);
    color += vec3(0.12, 0.20, 0.30) * fract * high;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(mask * (0.6 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}
