// @EFFECT name="Recursive Grids" index=77 desc="Recursive square grids with mouse-reactive rotation" author="#WCCChallenge p5.js port"

vec4 renderRecursiveGrids(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec3 color = vec3(0.0);
    
    // Yellow color from p5.js #FFC00020 (RGB: 1.0, 0.75, 0.0, alpha: 0.125)
    vec3 gridColor = vec3(1.0, 0.75, 0.0);
    
    // Use camera offset as mouse position equivalent
    vec2 shift = vec2(uCameraOffsetX, uCameraOffsetY) * 0.5;
    
    // Grid positions (3x3 grid at -0.3, 0, 0.3)
    for (int gy = -1; gy <= 1; gy++) {
        for (int gx = -1; gx <= 1; gx++) {
            vec2 gridPos = vec2(float(gx) * 0.3, float(gy) * 0.3);
            
            // Initial square size and position
            float d = 0.29; // 0.29 * height equivalent
            vec2 pos = gridPos;
            
            // Recursive/iterative square drawing (max iterations)
            const int maxIterations = 8;
            float s = 10.0;
            
            for (int i = 0; i < maxIterations; i++) {
                if (d < 0.016) break; // height/60 equivalent
                
                // Calculate rotation based on position relative to shift
                vec2 toShift = pos - shift;
                float reach = atan(toShift.y, toShift.x);
                
                // Draw square at current position
                vec2 rectSt = (st - pos) / d;
                float rect = smoothstep(0.5, 0.48, max(abs(rectSt.x), abs(rectSt.y)));
                color += gridColor * rect * 0.125; // Alpha from p5.js
                
                // Calculate next position and size
                pos = pos - vec2(cos(reach), sin(reach)) * s * 0.5 * d;
                d = (s - 1.0) * d / s;
            }
        }
    }
    
    // Add background
    color += vec3(1.0) * (1.0 - smoothstep(0.0, 0.1, length(st)));
    
    // Audio reactivity - modulate rotation speed
    color *= 0.8 + bass * 0.3;
    
    return vec4(color, 1.0);
}
