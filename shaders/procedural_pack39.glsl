// @EFFECT name="Ikeda Grid" index=65 desc="Data grid with crosses and animated digits" author="Patricio Gonzalez Vivo"

float randomIkedaGrid(in float x) { 
    return fract(sin(x) * 43758.5453); 
}

float randomIkedaGrid(in vec2 st) { 
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453); 
}

float gridIkeda(vec2 st, float res) {
    vec2 grid = fract(st * res);
    return 1.0 - (step(res, grid.x) * step(res, grid.y));
}

float boxIkeda(in vec2 st, in vec2 size) {
    size = vec2(0.5) - size * 0.5;
    vec2 uv = smoothstep(size, size + vec2(0.001), st);
    uv *= smoothstep(size, size + vec2(0.001), vec2(1.0) - st);
    return uv.x * uv.y;
}

float crossIkeda(in vec2 st, vec2 size) {
    return clamp(boxIkeda(st, vec2(size.x * 0.5, size.y * 0.125)) +
            boxIkeda(st, vec2(size.y * 0.125, size.x * 0.5)), 0.0, 1.0);
}

vec4 renderIkedaGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    st.x *= uResolution.x / uResolution.y;

    vec3 color = vec3(0.0);

    // Grid - intensity controlled by energy
    vec2 grid_st = st * 300.0;
    color += vec3(0.5, 0.0, 0.0) * gridIkeda(grid_st, 0.01);
    color += vec3(0.2, 0.0, 0.0) * gridIkeda(grid_st, 0.02);
    color += vec3(0.2) * gridIkeda(grid_st, 0.1) * (0.5 + energy * 0.5);

    // Crosses - react to bass
    vec2 crosses_st = st + 0.5;
    crosses_st *= 3.0 * (1.0 + bass);
    vec2 crosses_st_f = fract(crosses_st);
    color *= 1.0 - crossIkeda(crosses_st_f, vec2(0.3, 0.3));
    color += vec3(0.9) * crossIkeda(crosses_st_f, vec2(0.2, 0.2));

    // Digits - animation speed controlled by tempo
    vec2 blocks_st = floor(st * 6.0);
    float t = time * (0.8 + tempo) + randomIkedaGrid(blocks_st);
    float time_i = floor(t);
    float time_f = fract(t);
    color.rgb += step(0.9, randomIkedaGrid(blocks_st + time_i)) * (1.0 - time_f);

    // Mix with user palette
    vec3 gridColor = mix(uPrimaryColor, vec3(0.8, 0.0, 0.0), 0.5);
    vec3 accentColor = mix(uSecondaryColor, vec3(0.9, 0.9, 0.0), 0.3);
    
    color *= gridColor;
    color += accentColor * high * 0.3;

    return vec4(color, 1.0);
}
