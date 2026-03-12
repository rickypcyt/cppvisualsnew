// Breathing Layer - Rhythmic expansion effect
// Based on glslify quintic-out easing function

float qinticOut(float t) {
  return 1.0 - (pow(abs(t - 1.0), 5.0));
}

vec4 renderBreathing(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    uv *= 3.0;
	
    float l = length(uv);
    float t = qinticOut(fract(time * 0.5));
    
    float breath = sin(t * 6.28 - l * 3.0);
    breath *= 1.0 - clamp(l, 0.0, 1.0);
    
    // React to audio
    float audioReactivity = energy * 0.3 + bass * 0.4 + high * 0.3;
    breath *= (1.0 + audioReactivity * 2.0);
    
    // Apply scene colors
    vec3 color = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0)) * breath;
    float alpha = clamp(0.25 + breath * 0.75 + energy * 0.25, 0.0, 1.0);
    
    return vec4(color, alpha);
}
