// @EFFECT name="Koch Snowflake" index=92 desc="Koch snowflake fractal with recursive subdivision" author="p5.js port"



// Line distance with glow
float lineGlow(vec2 p, vec2 a, vec2 b, float width) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float d = length(pa - ba * h);
    return smoothstep(width, width * 0.5, d);
}

// Circle distance with glow
float circleGlow(vec2 p, vec2 center, float radius) {
    float d = length(p - center);
    return smoothstep(radius, radius * 0.5, d);
}

// Koch subdivision - subdivide line into 4 segments with triangular bump
vec2 kochSubdivide(vec2 p0, vec2 p1, float t, float rotAngle) {
    vec2 v = p1 - p0;
    vec2 p = p0 + v * t;
    
    // Add triangular bump at t = 0.5
    if (t >= 0.33 && t <= 0.67) {
        vec2 mid = p0 + v * 0.5;
        vec2 perp = vec2(-v.y, v.x);
        perp = normalize(perp) * length(v) * 0.2887; // tan(30°) / 3
        perp = rotate2D(perp, rotAngle);
        p = mid + perp * (t - 0.5) * 3.0;
    }
    
    return p;
}

vec4 renderKochSnowflake(
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
    
    // Parameters from p5.js
    int angle = 3; // Number of sides (triangle by default)
    int layer = 3; // Recursion depth
    
    // Audio-reactive parameters
    angle = int(3.0 + bass * 3.0);
    layer = int(3.0 + mid * 2.0);
    if (layer > 4) layer = 4; // Limit for performance
    
    float radius = 0.35;
    radius *= (0.8 + energy * 0.4);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    
    // Rotation for the peak point (simulating mouse interaction)
    float rotAngle = time * 0.5 + energy * 1.0;
    
    float brightness = 0.0;
    
    // Generate initial polygon vertices
    const int maxVertices = 15;
    vec2 vertices[maxVertices];
    int vertexCount = angle;
    
    for (int i = 0; i < maxVertices; i++) {
        if (i >= angle) break;
        float a = float(i) * (6.28318 / float(angle)) + 1.5708; // Start at top
        vertices[i] = vec2(cos(a), sin(a)) * radius;
    }
    
    // Draw Koch snowflake by sampling along lines
    const int samplesPerLine = 50;
    const int maxSegments = 256; // 4^4 = 256 max segments
    
    for (int i = 0; i < maxVertices; i++) {
        if (i >= vertexCount) break;
        
        vec2 p0 = vertices[i];
        vec2 p1 = vertices[(i + 1) % vertexCount];
        
        // Sample along line
        for (int s = 0; s < samplesPerLine; s++) {
            float t = float(s) / float(samplesPerLine - 1);
            
            // Apply Koch subdivision iteratively
            vec2 pt = p0 + (p1 - p0) * t;
            
            for (int l = 0; l < 4; l++) {
                if (l >= layer) break;
                
                // Determine which segment we're in
                float segment = t * 4.0;
                int seg = int(segment);
                float localT = fract(segment);
                
                // Recalculate points for this level
                vec2 v0, v1;
                if (l == 0) {
                    v0 = p0;
                    v1 = p1;
                } else {
                    // For simplicity, just use linear interpolation with noise
                    // Real Koch subdivision would require storing all segments
                    vec2 mid = p0 + (p1 - p0) * 0.5;
                    vec2 perp = vec2(-(p1.y - p0.y), p1.x - p0.x);
                    perp = normalize(perp) * length(p1 - p0) * 0.2887 * (1.0 - float(l) * 0.2);
                    perp = rotate2D(perp, rotAngle * (float(l) + 1.0));
                    
                    if (seg == 1 || seg == 2) {
                        pt = mid + perp * (localT - 0.5) * 2.0;
                    } else {
                        pt = p0 + (p1 - p0) * t;
                    }
                }
                
                t = localT;
            }
            
            // Draw point glow
            float pointDist = length(fromCenter - pt);
            if (pointDist < 0.01) {
                brightness += (1.0 - smoothstep(0.0, 0.01, pointDist)) * 0.2;
            }
        }
        
        // Draw the main line
        float line = lineGlow(fromCenter, p0, p1, 0.003);
        brightness += line * 0.5;
        
        // Draw circles at endpoints
        float circle0 = circleGlow(fromCenter, p0, 0.005);
        float circle1 = circleGlow(fromCenter, p1, 0.005);
        brightness += (circle0 + circle1) * 0.3;
    }
    
    // Add more detail for higher iterations
    for (int l = 0; l < layer; l++) {
        float scale = pow(0.33, float(l + 1));
        float brightnessScale = 1.0 - float(l) * 0.2;
        
        for (int i = 0; i < maxVertices; i++) {
            if (i >= vertexCount) break;
            
            vec2 p0 = vertices[i];
            vec2 p1 = vertices[(i + 1) % vertexCount];
            vec2 mid = p0 + (p1 - p0) * 0.5;
            
            // Add triangular bump
            vec2 perp = vec2(-(p1.y - p0.y), p1.x - p0.x);
            perp = normalize(perp) * length(p1 - p0) * 0.2887 * scale;
            perp = rotate2D(perp, rotAngle);
            vec2 peak = mid + perp;
            
            // Draw bump
            float bumpLine1 = lineGlow(fromCenter, p0 + (mid - p0) * 0.33, peak, 0.002);
            float bumpLine2 = lineGlow(fromCenter, peak, p1 - (p1 - mid) * 0.33, 0.002);
            brightness += (bumpLine1 + bumpLine2) * brightnessScale * 0.3;
            
            // Draw circle at peak
            float peakCircle = circleGlow(fromCenter, peak, 0.003 * scale);
            brightness += peakCircle * brightnessScale * 0.2;
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // Color (white to gray gradient based on distance)
    float dist = length(fromCenter);
    vec3 startCol = vec3(1.0);
    vec3 endCol = vec3(0.33, 0.33, 0.33);
    vec3 col = mix(startCol, endCol, smoothstep(0.0, 0.35, dist));
    
    color = col * brightness;
    
    // Audio-reactive color modulation
    color *= (0.8 + energy * 0.4);
    
    // Add subtle hue shift based on audio
    color = mix(color, vec3(color.r * 0.8, color.g, color.b * 1.2), bass * 0.3);
    
    alpha = brightness + 0.1;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, clamp(alpha, 0.0, 1.0));
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}
