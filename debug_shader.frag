
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSourceTexture;
uniform vec2 uSourceResolution;
uniform vec2 uTargetResolution;

void main() {
    // Calculate aspect ratios
    float sourceAspect = uSourceResolution.x / uSourceResolution.y;
    float targetAspect = uTargetResolution.x / uTargetResolution.y;

    // Adjust texture coordinates to maintain aspect ratio (letterbox/pillarbox elimination)
    vec2 uv = vTexCoord;
    if (sourceAspect > targetAspect) {
        // Source is wider than target - crop sides
        float scale = targetAspect / sourceAspect;
        uv.x = (uv.x - 0.5) / scale + 0.5;
    } else {
        // Source is taller than target - crop top/bottom
        float scale = sourceAspect / targetAspect;
        uv.y = (uv.y - 0.5) / scale + 0.5;
    }

    // Sample with adjusted coordinates
    vec3 color = texture(uSourceTexture, uv).rgb;
    FragColor = vec4(color, 1.0);
}
