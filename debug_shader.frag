
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSourceTexture;
uniform vec2 uSourceResolution;
uniform vec2 uTargetResolution;

void main() {
    // Simple bilinear sampling - OpenGL's built-in interpolation handles this
    // when we set texture parameters to LINEAR
    vec3 color = texture(uSourceTexture, vTexCoord).rgb;
    FragColor = vec4(color, 1.0);
}
