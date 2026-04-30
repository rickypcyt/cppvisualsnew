// @EFFECT name="Wave Circles" index=95 desc="Concentric wave circles with boxes" author="p5.js port"

// Project 3D point to 2D with rotation
vec2 project3D(vec3 p, float rotation) {
    // Rotate around Y axis
    float c = cos(rotation);
    float s = sin(rotation);
    
    float x = p.x * c - p.z * s;
    float z = p.x * s + p.z * c;
    float y = p.y;
    
    // Simple perspective projection
    float fov = 2.0;
    float dist = 3.0;
    float scale = fov / (dist + z);
    
    return vec2(x * scale, y * scale);
}

// HSB to RGB conversion
vec3 hsbToRgb(vec3 hsb) {
    vec3 rgb = clamp(abs(mod(hsb.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    return hsb.z * mix(vec3(1.0), rgb, hsb.y);
}

vec4 renderWaveCircles(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from original code
    int layerNum = 25;
    float distance = 0.02;
    int boxNum = 40;
    float amplitude = 0.15;
    float waveSpeed = 1.0;
    
    // Audio-reactive parameters
    layerNum = int(float(layerNum) * (0.8 + energy * 0.4));
    distance *= (0.8 + bass * 0.4);
    amplitude *= (0.8 + mid * 0.4);
    waveSpeed *= (0.5 + high * 0.5);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 uv = (st - center) * 2.0;
    
    float brightness = 0.0;
    
    // Scene rotation
    float rotation = time * waveSpeed * 10.0;
    rotation += bass * 5.0;
    
    // Draw boxes in concentric layers
    for (int layer = 1; layer <= 25; layer++) {
        if (layer > layerNum) continue;
        
        // Layer radius
        float r = float(layer) * distance;
        
        // Draw boxes on this layer
        for (int i = 0; i < 40; i++) {
            float theta = float(i) * (360.0 / float(boxNum));
            
            // Convert spherical to rectangular
            vec3 posi = sphericalToRectangular(r, theta, 90.0);
            
            // Add wave motion
            float wavePhase = waveSpeed * time * 50.0 + float(layer) / float(layerNum) * 360.0;
            posi.y += amplitude * sin(radians(wavePhase));
            
            // Apply audio-reactive offset
            posi.x += sin(time + float(layer) * 0.5) * bass * 0.05;
            posi.z += cos(time + float(i) * 0.5) * mid * 0.05;
            
            // Project to 2D
            vec2 projected = project3D(posi, rotation);
            
            // Box size based on layer
            float boxSize = (360.0 / float(boxNum) * 0.9 / 360.0 * 3.14159 * r) * 0.5;
            
            // Draw box (simple circle/box representation)
            float distToBox = length(uv - projected);
            float boxHit = smoothstep(boxSize, boxSize * 0.5, distToBox);
            
            if (boxHit > 0.0) {
                // HSB color based on layer
                float hue = map(float(layer), 0.0, float(layerNum), 0.0, 1.0);
                float saturation = 0.75 + (1.0 - 0.75) * sin(radians(mod(time * 50.0, 360.0)));
                saturation *= (0.8 + energy * 0.4);
                float brightnessVal = 1.0 + (1.0 - 1.0) * cos(radians(mod(time * 25.0, 360.0)));
                brightnessVal *= (0.8 + high * 0.4);
                
                vec3 hsb = vec3(hue, saturation, brightnessVal);
                vec3 boxColor = hsbToRgb(hsb);
                
                brightness += boxHit * 0.6;
                color += boxColor * boxHit;
            }
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    color = clamp(color, 0.0, 1.0);
    
    // Background
    vec3 bgColor = vec3(0.0, 0.0, 0.05);
    color = mix(bgColor, color, brightness);
    
    alpha = brightness + 0.1;
    
    return vec4(color, clamp(alpha, 0.0, 1.0));
}
