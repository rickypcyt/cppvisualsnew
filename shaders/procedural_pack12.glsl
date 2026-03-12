// Crystal Tetrahedron - Simplified raymarched crystal
// Audio-reactive version optimized for performance

// Simplified tetrahedron SDF
float sdTetrahedron(vec3 p, float s) {
    float k = sqrt(2.0);
    p = abs(p);
    return max(p.x - p.y * k, -p.y * k) + max(p.z - p.x * k, -p.x * k) - s;
}

// Rotation helper using existing function
vec3 rotate3D(vec3 p, vec2 angles) {
    // Use existing rotate function from header
    p.xy = rotate(p.xy, angles.x);
    p.xz = rotate(p.xz, angles.y);
    return p;
}

// Simplified normal calculation
vec3 getNormal(vec3 p, float time) {
    float e = 0.001;
    vec2 h = vec2(e, 0);
    return normalize(vec3(
        sdTetrahedron(p + h.xyy, 0.3) - sdTetrahedron(p - h.xyy, 0.3),
        sdTetrahedron(p + h.yxy, 0.3) - sdTetrahedron(p - h.yxy, 0.3),
        sdTetrahedron(p + h.yyx, 0.3) - sdTetrahedron(p - h.yyx, 0.3)
    ));
}

// Simplified raymarch
float raymarch(vec3 ro, vec3 rd, float time, float maxDist) {
    float t = 0.0;
    for(int i = 0; i < 60; i++) {
        vec3 p = ro + rd * t;
        float d = sdTetrahedron(p, 0.3);
        if(d < 0.001) return t;
        if(t > maxDist) break;
        t += d * 0.8;
    }
    return -1.0;
}

// Lighting calculation
vec3 calcLighting(vec3 p, vec3 rd, vec3 normal, vec3 baseColor, float energy) {
    vec3 lightDir = normalize(vec3(1.0, 1.0, -1.0));
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(rd, reflectDir), 0.0), 16.0);
    
    vec3 ambient = baseColor * 0.3;
    vec3 diffuse = baseColor * diff * 0.7;
    vec3 specular = vec3(1.0) * spec * 0.5;
    
    return ambient + diffuse + specular;
}

vec4 renderCrystalTetrahedron(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Audio-reactive parameters
    float audioMod = 1.0 + energy * 0.5 + bass * 0.3;
    float rotSpeed = time * 0.5 + energy * 0.2;
    
    // Camera setup
    vec3 ro = vec3(0.0, 0.0, -3.0);
    vec3 rd = normalize(vec3(st, 1.0));
    
    // Audio-reactive camera movement
    ro.x += sin(rotSpeed) * 0.2 * energy;
    ro.y += cos(rotSpeed * 0.7) * 0.15 * bass;
    
    // Rotate camera based on audio
    vec2 camRot = vec2(rotSpeed * 0.3, rotSpeed * 0.2);
    rd = normalize(rotate3D(rd, camRot));
    
    // Raymarch
    float t = raymarch(ro, rd, time, 10.0);
    
    vec3 col = vec3(0.02, 0.02, 0.05); // Dark background
    float alpha = 0.0;
    
    if(t > 0.0) {
        // Hit the crystal
        vec3 p = ro + rd * t;
        vec3 normal = getNormal(p, time);
        
        // Audio-reactive color
        vec3 baseColor = mix(uPrimaryColor, uSecondaryColor, uColorBlend);
        
        // Color shifting based on audio frequencies
        vec3 audioColor = vec3(bass, mid, high) * 0.4;
        baseColor = mix(baseColor, audioColor, energy * 0.4);
        
        // Add some color variation based on position
        float colorVariation = sin(p.x * 3.0 + time) * 0.5 + 0.5;
        baseColor = mix(baseColor, vec3(0.8, 0.4, 1.0), colorVariation * 0.3);
        
        // Calculate lighting
        col = calcLighting(p, rd, normal, baseColor, energy);
        
        // Add glow for high energy moments
        if(energy > 0.6) {
            float glowStrength = (energy - 0.6) * 2.5;
            col += vec3(0.3, 0.5, 1.0) * glowStrength;
        }
        
        // Alpha based on energy and hit
        alpha = 0.7 + energy * 0.3;
        alpha = clamp(alpha, 0.0, 1.0);
        
        // Add some edge glow
        float edgeFactor = 1.0 - abs(dot(normal, -rd));
        col += vec3(0.2, 0.4, 0.8) * edgeFactor * 0.3 * energy;
    }
    
    return vec4(col, alpha);
}
