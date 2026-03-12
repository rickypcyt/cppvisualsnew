#include "post_common.glsl"

void main() {
    vec4 sceneColor = texture(uScene, vUV);
    float segments = 6.0 + uStrength * 10.0;
    vec2 uv = kaleido(vUV, segments, uResolution);
    vec3 result = texture(uScene, uv).rgb;
    FragColor = vec4(result, sceneColor.a);
}
