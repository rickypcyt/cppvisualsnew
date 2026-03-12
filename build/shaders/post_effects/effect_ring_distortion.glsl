uniform sampler2D uTexture;
uniform float uTime;
uniform float uStrength;
uniform vec2 uResolution;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;

in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    vec2 res = uResolution.xy;
    float aspect = res.x / res.y;
    vec2 uv = vTexCoord;
    
    // Audio-reactive parameters
    float ring = 5.0 + uEnergy * 3.0;  // Ring density responds to energy
    float div = 0.5;
    float t = uTime * 0.05 + uBass * 0.1;  // Time responds to bass
    
    vec2 p = vec2(uv.x * aspect, uv.y);
    
    // Center point with audio-reactive movement
    vec2 center = vec2(0.5, 0.5);
    center.x += sin(uTime * 0.3) * uMid * 0.1;
    center.y += cos(uTime * 0.4) * uHigh * 0.1;
    
    float r = distance(p, center * vec2(aspect, 1.0));
    r -= t;
    r = fract(r * ring) / div;
    
    // Apply distortion
    uv = -1.0 + 2.0 * uv;
    uv *= r;
    uv = uv * 0.5 + 0.5;
    
    // Apply strength control
    uv = mix(vTexCoord, uv, uStrength);
    
    // Sample with audio-reactive color enhancement
    vec4 color = texture(uTexture, uv);
    
    // Enhance colors based on audio
    color.rgb *= 1.0 + uEnergy * 0.2;
    color.r *= 1.0 + uBass * 0.3;
    color.g *= 1.0 + uMid * 0.2;
    color.b *= 1.0 + uHigh * 0.3;
    
    FragColor = color;
}
