// @EFFECT name="Eiyeron Deform" index=46 desc="Classic plane deformation with tunnel effects by Eiyeron/Matrefeytontias" author="Eiyeron"

#define SPEED 0.25

// Helper functions from the original shader
vec3 eiyeronGetColors(vec2 position, float time) {
    vec3 res = vec3(cos(position.x), sin(position.y), 1.0 - 0.5 * cos(time));
    return res;
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

// Single combined effect with all features active
vec4 renderEiyeronDeform(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // st comes in as normalized coordinates from the header
    vec2 position = st;
    
    // Apply curved view (like original CURVED_VIEW)
    position.y -= 0.10 * cos(position.x);
    
    // Apply wobble effect (like original WOBBLE)
    position.y += 0.2 * cos(position.x / 2.0 + time * 0.37);
    
    // Some calculations
    float r = length(position);
    float a = atan(position.y, position.x);
    
    // Transition factor
    float factor = sin(time) / 2.0 + 0.5;
    
    // Apply rotation
    a += sin(time / 20.0);
    
    // The magic happens right here - blend between PLANE_EFFECT and TUNNEL_PLANE_EFFECT
    // Mode 0 = pure plane, Mode 1 = pure tunnel, blend between them with audio
    float blend = clamp(energy + bass * 0.5, 0.0, 1.0);
    
    float u_plane = position.x / abs(position.y + 0.0001);
    float v_plane = 1.0 / abs(position.y + 0.0001);
    
    float u_tunnel = a;
    float v_tunnel = 1.0 / (r + 0.0001);
    
    // TUNNEL_PLANE_EFFECT blending
    float u = factor * u_plane + (1.0 - factor) * u_tunnel;
    float v = factor * v_plane + (1.0 - factor) * v_tunnel;
    
    // Also blend with audio for dynamic effect
    u = mix(u, u_tunnel, blend * 0.5);
    v = mix(v, v_tunnel, blend * 0.5);
    
    vec2 p = vec2(u, v);
    
    // Apply movement (like original MOVE)
    p += vec2(SPEED * cos(time), SPEED * time);
    
    // Add audio-reactive movement
    p += vec2(bass * 0.1, high * 0.05);
    
    // Start with white
    vec3 color = vec3(1.0);
    
    // Apply rainbow (like original RAINBOW)
    color = eiyeronGetRainbow(p);
    
    // Apply crazy colors
    color *= eiyeronGetColors(p, time);
    
    // Apply tesselated colors
    color *= vec3(
        sin(dot(p, position)),
        cos(dot(p, position)),
        sin(dot(p, position))
    );
    
    // Apply crazy checkerboard motif
    color *= vec3(eiyeronGetCheckerboardColor(p, time));
    
    // Apply blob motif
    float col = 0.0;
    for(float i = 0.0; i < 5.0; i++) {
        float ang = i * (kTwoPI / 5.0) * 61.95;
        col += cos(kTwoPI * (p.y * cos(ang) + p.x * sin(ang) + sin(time * 0.004) * 100.0));
    }
    col /= 3.0;
    color *= vec3(col);
    
    // Apply distance fog
    color *= 1.0 / (abs(v) + 0.1);
    
    // Apply border fog
    color *= (2.0 - r);
    
    // Apply scanline effect
    float scanY = gl_FragCoord.y;
    color *= 1.0 * mod(scanY, 2.0);
    
    // Clamp and return
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.6 + energy * 0.2, 0.0, 1.0);
    
    return vec4(color, alpha);
}
