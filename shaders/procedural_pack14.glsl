// Phi Fields - Simplified mathematical field patterns
// Audio-reactive version optimized for visual appeal

// Simplified field pattern generator
float fieldPattern(vec2 p, float time, float frequency) {
    // Create interference patterns
    float pattern1 = sin(p.x * frequency + time) * cos(p.y * frequency - time * 0.7);
    float pattern2 = sin(length(p) * frequency * 1.5 - time * 1.3);
    
    // Mix patterns with phi ratio (golden ratio)
    float phi = 1.61803398875;
    float interference = pattern1 + pattern2 / phi;
    
    return interference * 0.5 + 0.5;
}

// Spiral field generator
float spiralField(vec2 p, float time, float turns) {
    float l = length(p);
    float a = atan(p.y, p.x);
    
    // Create spiral with audio modulation
    float spiral = sin(a * turns + l * 10.0 - time * 2.0);
    float radialFade = 1.0 - smoothstep(0.0, 2.0, l);
    
    return spiral * radialFade * 0.5 + 0.5;
}

// Metallic gradient effect
vec3 metallicGradient(float value, vec3 baseColor) {
    // Create metallic-like color transitions
    vec3 gold = mix(vec3(1.0, 0.8, 0.3), vec3(1.0, 0.6, 0.0), value);
    vec3 silver = mix(vec3(0.9, 0.9, 0.95), vec3(0.7, 0.7, 0.8), value);
    vec3 blue = mix(vec3(0.3, 0.5, 0.9), vec3(0.1, 0.3, 0.7), value);
    
    // Blend with base color
    return mix(baseColor, mix(gold, mix(silver, blue, value), 0.5), 0.7);
}

vec4 renderPhiFields(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Audio-reactive parameters
    float audioMod = 1.0 + energy * 0.8;
    float timeMod = time * audioMod;
    
    // Create coordinate system with audio-reactive scaling
    vec2 uv = (st - 0.5) * 4.0;
    uv *= 1.0 + bass * 0.3; // Scale with bass
    
    // Rotate based on tempo and energy
    float rotation = timeMod * 0.1 + energy * 0.5;
    float cos_r = cos(rotation);
    float sin_r = sin(rotation);
    uv = vec2(uv.x * cos_r - uv.y * sin_r, uv.x * sin_r + uv.y * cos_r);
    
    // Generate multiple field layers
    vec3 color = vec3(0.0);
    
    // Layer 1: Main field pattern
    float field1 = fieldPattern(uv, timeMod, 3.0 + energy * 2.0);
    vec3 fieldColor1 = metallicGradient(field1, uPrimaryColor);
    
    // Layer 2: Spiral overlay
    float spiral = spiralField(uv, timeMod, 5.0 + bass * 3.0);
    vec3 spiralColor = metallicGradient(spiral, uSecondaryColor);
    
    // Layer 3: High frequency details
    vec2 uv_detail = uv * 8.0;
    float field2 = fieldPattern(uv_detail, timeMod * 2.0, 2.0 + high * 3.0);
    vec3 detailColor = metallicGradient(field2, mix(uPrimaryColor, uSecondaryColor, 0.5));
    
    // Combine layers with audio-reactive mixing
    color = fieldColor1 * 0.6;
    color += spiralColor * 0.3 * (1.0 + energy);
    color += detailColor * 0.1 * (1.0 + high);
    
    // Add frequency-based color modulation
    color.r *= (1.0 + bass * 0.3);
    color.g *= (1.0 + mid * 0.3);
    color.b *= (1.0 + high * 0.3);
    
    // Add glow effect for high energy
    if (energy > 0.5) {
        float glowIntensity = (energy - 0.5) * 2.0;
        vec3 glowColor = vec3(0.3, 0.4, 1.0) * glowIntensity;
        color += glowColor;
    }
    
    // Add center brightness
    float centerDist = length(uv);
    float centerGlow = 1.0 - smoothstep(0.0, 1.5, centerDist);
    color += centerGlow * energy * 0.5;
    
    // Ensure colors are in valid range
    color = clamp(color, 0.0, 1.5);
    
    // Alpha based on field intensity and energy
    float alpha = 0.7 + length(color) * 0.2 + energy * 0.3;
    alpha = clamp(alpha, 0.0, 1.0);
    
    return vec4(color, alpha);
}
