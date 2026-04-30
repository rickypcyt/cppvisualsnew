// @EFFECT name="Game of Life" index=78 desc="Conway's Game of Life cellular automaton" author="p5.js port"


// Get cell state at grid position with toroidal wrapping
float getCellState(vec2 gridPos, vec2 gridSize, float time) {
    vec2 wrapped = mod(gridPos, gridSize);
    float seed = hash21(wrapped + floor(time * 0.1));
    return step(0.5, seed);
}

// Count neighbors with toroidal wrapping
int countNeighbors(vec2 gridPos, vec2 gridSize, float time) {
    int sum = 0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            vec2 neighborPos = gridPos + vec2(float(i), float(j));
            sum += int(getCellState(neighborPos, gridSize, time));
        }
    }
    return sum;
}

// Apply Game of Life rules
float applyGameOfLifeRules(float state, int neighbors) {
    if (state < 0.5) {
        // Dead cell becomes alive if exactly 3 neighbors
        return float(neighbors == 3);
    } else {
        // Live cell stays alive if 2 or 3 neighbors
        return float(neighbors == 2 || neighbors == 3);
    }
}

vec4 renderGameOfLife(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    // Grid resolution - matches p5.js resolution of 20
    float resolution = 20.0;
    
    // Calculate grid dimensions
    vec2 aspect = uResolution.xy / min(uResolution.x, uResolution.y);
    vec2 gridSize = aspect * resolution;
    
    // Convert screen position to grid coordinates
    vec2 gridPos = st * 0.5 + 0.5;
    gridPos *= gridSize;
    
    // Audio-reactive evolution speed
    float evolutionSpeed = 0.5 + bass * 0.5;
    float timeStep = floor(time * evolutionSpeed);
    
    // Get current cell state based on hash (simulating state persistence)
    vec2 cellIndex = floor(gridPos);
    float seed = hash21(cellIndex + timeStep * 0.01);
    float currentState = step(0.5, seed);
    
    // Count neighbors
    int neighbors = countNeighbors(cellIndex, gridSize, timeStep);
    
    // Apply Game of Life rules
    float nextState = applyGameOfLifeRules(currentState, neighbors);
    
    // Blend between current and next state for smooth transitions
    float blend = fract(time * evolutionSpeed);
    float cellState = mix(currentState, nextState, blend);
    
    // Color palette from p5.js (black and white)
    vec3 deadColor = vec3(0.88, 0.88, 0.88); // Light gray background
    vec3 aliveColor = vec3(0.0, 0.0, 0.0);     // Black cells
    
    // Add audio-reactive color modulation
    vec3 color = mix(deadColor, aliveColor, cellState);
    
    // Add subtle glow to alive cells based on energy
    float glow = cellState * energy * 0.3;
    color += glow * uPrimaryColor;
    
    // Add grid lines
    vec2 gridUV = fract(gridPos);
    float gridLine = smoothstep(0.02, 0.0, min(gridUV.x, gridUV.y)) +
                     smoothstep(0.98, 1.0, max(gridUV.x, gridUV.y));
    color += gridLine * 0.1;
    
    // Alpha based on cell state
    float alpha = cellState * 0.9 + 0.1;
    
    return vec4(color, alpha);
}
