#include "modular_layer.h"

#include <array>
#include <iomanip>
#include <iostream>

namespace {

const char* kQuadVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}

float hash1(float x) {
    return fract(sin(x * 133.3f) * 13.13f);
}

vec4 renderRibbonScanlines(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    float angle = 0.5f + 0.6f * tempo + 0.4f * energy;
    float t = floor(time * (4.0f + tempo * 3.0f)) / 8.0f * 3.1415926f;
    float si = sin(angle + t);
    float co = cos(angle + t);
    uv *= mat2(co, -si, si, co);

    float v = 1.0f - sin(hash1(floor((uv.x + energy * 0.4f) * 120.0f)) * 11.0f);
    float freq = 5.0f / (2.0f + v + 0.4f * energy);
    float ribbon = sin((20.0f + tempo * 12.0f) * 0.75f * v + uv.y * freq);
    float band = clamp(abs(ribbon) - 0.92f, 0.0f, 1.0f) * 18.0f;

    vec3 baseColor = vec3(0.6f, 0.7f, 0.8f) * (0.4f + energy * 0.6f);
    vec3 tint = vec3(0.4f + bass * 0.3f,
                     0.55f + mid * 0.35f,
                     0.75f + high * 0.45f);
    vec3 color = baseColor * v * band * tint;

    float alpha = clamp(max(color.r, max(color.g, color.b)) * 1.2f, 0.0f, 1.0f);
    return vec4(color, alpha);
}

vec4 renderAuroraBloom(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    vec3 color = vec3(0.0);

    float intensity = 0.01f + 0.02f * clamp(energy, 0.0f, 1.5f);
    float size = 0.6f + 0.6f * abs(cos(time * (0.8f + tempo * 0.2f)));
    float offset = 0.35f + 0.4f * abs(sin(time * (0.6f + energy * 0.4f)));

    vec3 colorFull = vec3(abs(sin(time * (1.0f + bass * 0.6f))),
                          abs(cos(time * (0.7f + mid * 0.5f))),
                          abs(sin(time * (1.3f + high * 0.7f))));

    for (int i = 0; i < 10; ++i) {
        float fi = float(i);
        float angleBass = time * fi * PI * 0.011f;
        float angleMid = time * fi * PI * 0.09f;
        float x = cos(angleBass) * offset * (1.0f + bass * 0.4f);
        float y = sin(angleMid) * offset * (1.0f + mid * 0.3f);
        float len = length(p + vec2(x, y));
        float envelope = pow(intensity / max(abs(len - size), 0.0005f), 11.0f);
        color += envelope;
    }

    // Soft radial envelope tied to audio energy
    float radius = length(st);
    float radialGlow = exp(-radius * radius * (28.0f - energy * 12.0f));
    color = color * colorFull * (0.8f + energy * 0.4f) + radialGlow * vec3(0.12f, 0.18f, 0.26f);

    float alpha = clamp(max(color.r, max(color.g, color.b)) * 0.65f, 0.0f, 1.0f);
    return vec4(color, alpha);
}

vec4 renderPulsarTunnel(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 v = st;
    float timer = 0.5f * time + tempo * 0.35f;
    float sinVal = sin(timer);
    float cosVal = cos(timer);
    mat2 m = mat2(cosVal, sinVal, -sinVal, cosVal);

    float scale = 0.85f + 0.1f * sin(2.243f * time + energy * 1.4f);
    float attenuation = 0.0f;
    vec2 p = v;

    for (int i = 0; i < 18; ++i) {
        float dynamic = 0.7f + 0.1f * sin(2.243f * time + float(i) * 1.1f + bass * 2.0f);
        scale *= dynamic;
        p = m * p;
        p.y = abs(p.y) - scale;
        attenuation += exp(-5.0f * abs(p.y));
    }

    float falloff = dot(p, p);
    float energyTint = clamp(energy * 0.6f + tempo * 0.3f, 0.0f, 2.0f);
    vec3 baseColor = vec3(0.7f + high * 0.3f,
                          0.9f + mid * 0.25f,
                          1.0f + bass * 0.18f);
    vec3 pulseColor = vec3(0.4f + bass * 0.4f,
                           0.6f + mid * 0.35f,
                           1.1f + high * 0.5f);

    float glow = exp(-12000.0f * falloff) * (1.0f + energyTint * 0.4f);
    glow += attenuation * 0.015f;

    vec3 color = baseColor * glow + pulseColor * (attenuation * 0.01f);
    float alpha = clamp(glow * 1.4f, 0.0f, 1.0f);

    return vec4(color, alpha);
}

float fractalMap(vec3 p, vec4 light, mat2 m, mat2 n, mat2 nn) {
    float d = length(p - light.xyz) - light.w;
    d = min(d, max(12.0 - p.z, 0.0));
    float t = 2.5;
    for (int i = 0; i < 13; ++i) {
        t *= 0.66;
        p.xy = m * p.xy;
        p.yz = n * p.yz;
        p.zx = nn * p.zx;
        p.xz = abs(p.xz) - t;
    }
    d = min(d, length(p) - 1.4 * t);
    return d;
}

vec3 fractalDive(vec3 ro, vec3 rd, vec4 light, mat2 m, mat2 n, mat2 nn) {
    vec3 p = ro;
    for (int i = 0; i < 24; ++i) {
        p += rd * fractalMap(p, light, m, n, nn);
    }
    return p;
}

vec3 fractalNormal(vec3 p, vec4 light, mat2 m, mat2 n, mat2 nn) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        fractalMap(p + e.xyy, light, m, n, nn) - fractalMap(p - e.xyy, light, m, n, nn),
        fractalMap(p + e.yxy, light, m, n, nn) - fractalMap(p - e.yxy, light, m, n, nn),
        fractalMap(p + e.yyx, light, m, n, nn) - fractalMap(p - e.yyx, light, m, n, nn)
    ));
}

vec4 renderFractalObject(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 v = st;
    float ui = 100.0 * time;

    float y1 = -0.001 * ui + bass * 0.15;
    mat2 m = mat2(sin(y1), cos(y1), -cos(y1), sin(y1));
    float y2 = 0.0035 * ui + mid * 0.12;
    mat2 n = mat2(sin(y2), cos(y2), -cos(y2), sin(y2));
    float y3 = 0.0023 * ui + high * 0.18;
    mat2 nn = mat2(sin(y3), cos(y3), -cos(y3), sin(y3));

    vec3 ro = vec3(0.0, 0.0, -15.0 + 2.0 * sin(0.01 * ui + energy * 0.6));
    vec4 light = vec4(10.0 * sin(0.01 * ui),
                      2.0 + mid * 1.5,
                      -23.0 + bass * 4.0,
                      1.2 + energy * 0.8);

    vec3 camDir = normalize(vec3(0.0, 0.0, 1.0));
    vec3 camUp = normalize(vec3(1.0, 1.4, 0.0));
    vec3 camSide = normalize(cross(camDir, camUp));
    vec3 rd = normalize(camSide * v.x + camUp * v.y + camDir);

    vec3 p = fractalDive(ro, rd, light, m, n, nn);
    vec3 lightDir = normalize(light.xyz - p);
    vec3 normal = fractalNormal(p, light, m, n, nn);

    float diffuse = clamp(dot(normal, lightDir), 0.0, 1.0);
    vec3 baseColor = vec3(0.7, 0.8, 0.9);
    vec3 color = baseColor * (0.4 + diffuse * (0.7 + energy * 0.4));

    vec3 bounce = fractalDive(p + 0.01 * lightDir, lightDir, light, m, n, nn);
    if (length(bounce - light.xyz) > light.w + 0.12) {
        color *= 0.2;
    }

    color *= 0.6 + energy * 0.5 + bass * 0.25;
    color += vec3(bass * 0.12, mid * 0.08, high * 0.18);

    return vec4(color, 1.0);
}

vec3 hash33(vec3 p3) {
    p3 = fract(p3 * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return fract((p3.xxy + p3.yxx) * p3.zyx);
}

float sdRoundBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float softMin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float softMax(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (a - b) / k, 0.0, 1.0);
    return mix(b, a, h) + k * h * (1.0 - h);
}

mat2 rotation2D(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

float latticeOctave(vec3 p, float scale) {
    vec3 fp = p * scale;
    vec3 cell = floor(fp);
    vec3 fractP = fract(fp);
    float minDist = 5.0;
    for (int i = 0; i <= 1; ++i) {
        for (int j = 0; j <= 1; ++j) {
            for (int k = 0; k <= 1; ++k) {
                vec3 n = vec3(float(i), float(j), float(k));
                vec3 rnd = hash33(cell + n + 12.0);
                vec3 boxSize = rnd * vec3(0.4, 0.5, 0.4);
                float d = sdRoundBox(fractP - n - rnd * 0.4, boxSize, 0.1 * rnd.x);
                minDist = min(minDist, d);
            }
        }
    }
    return minDist / scale;
}

float latticeField(vec3 p, float time, float energy, float high) {
    p.y -= 1.0;
    float base = p.y + 0.45;
    float scale = 1.0;
    float threshold = 0.24;
    float result = base;
    vec3 pos = p;

    for (int octave = 0; octave < 6; ++octave) {
        float field = latticeOctave(pos, scale);
        float falloff = threshold / scale;
        float smooth = 0.1 / scale;

        if ((octave % 2) == 0) {
            result = softMin(field, result, smooth);
        } else {
            result = softMax(-field, result, smooth);
        }

        scale *= 1.75;
        pos.xz *= rotation2D(PI * 0.5);
        pos += vec3(143.543145);
        threshold *= 1.1;
    }

    float highMask = clamp(high * 1.8 + energy * 0.6, 0.0, 2.5);
    return result - highMask * 0.05;
}

vec3 latticeNormal(vec3 p, float time, float energy, float high) {
    const float eps = 0.0015;
    vec2 h = vec2(1.0, -1.0) * eps;
    return normalize(vec3(
        latticeField(p + vec3(h.x, h.y, h.y), time, energy, high) - latticeField(p + vec3(h.y, h.x, h.x), time, energy, high),
        latticeField(p + vec3(h.y, h.x, h.y), time, energy, high) - latticeField(p + vec3(h.x, h.y, h.x), time, energy, high),
        latticeField(p + vec3(h.y, h.y, h.x), time, energy, high) - latticeField(p + vec3(h.x, h.x, h.y), time, energy, high)
    ));
}

vec4 renderCrystalLattice(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    vec3 ro = vec3(0.4 * sin(time * 0.25), 0.3 + energy * 0.4, -2.4);
    vec3 rd = normalize(vec3(uv, 1.5));

    float tempoDrive = clamp(tempo * 0.8 + energy * 0.4, 0.0, 3.0);
    float swirl = time * (0.2 + tempoDrive * 0.12);
    mat2 rot = rotation2D(swirl);
    ro.xz = rot * ro.xz;
    rd.xz = rot * rd.xz;

    float t = 0.0;
    float dist = 0.0;
    vec3 p;
    bool hit = false;
    for (int i = 0; i < 96; ++i) {
        p = ro + rd * t;
        float field = latticeField(p, time, energy, high);
        dist = field;
        if (field < 0.0008) {
            hit = true;
            break;
        }
        if (t > 12.0) {
            break;
        }
        t += max(0.02, field * 0.85);
    }

    vec3 color = vec3(0.0);
    float alpha = 0.0;
    if (hit) {
        vec3 normal = latticeNormal(p, time, energy, high);
        vec3 lightDir = normalize(vec3(0.6, 0.9, 0.4));
        float diff = clamp(dot(normal, lightDir), 0.0, 1.0);

        vec3 baseColor = mix(vec3(0.08, 0.2, 0.35), vec3(0.56, 0.9, 1.1), clamp(diff * 1.4 + high * 0.5, 0.0, 1.0));
        vec3 accent = mix(vec3(0.95, 0.5, 0.2), vec3(0.2, 0.4, 1.0), clamp(bass * 0.6 + mid * 0.4, 0.0, 1.0));
        float spec = pow(clamp(dot(reflect(-lightDir, normal), -rd), 0.0, 1.0), 12.0 + high * 18.0);

        float nodeGlow = clamp(1.0 - length(p) / (1.6 + energy * 0.4), 0.0, 1.0);
        nodeGlow = pow(nodeGlow, 1.8) * (0.6 + tempoDrive * 0.35);

        color = baseColor * diff * (0.9 + energy * 0.4);
        color += accent * nodeGlow;
        color += vec3(0.85, 0.92, 1.1) * spec * (0.8 + high * 0.6);

        float latticeSheen = abs(dot(normal, normalize(vec3(1.0, 0.2, -0.4))));
        color = mix(color, color.zyx, clamp(high * 0.4, 0.0, 1.0));
        color += vec3(0.12, 0.28, 0.45) * latticeSheen * (0.4 + tempoDrive * 0.25);

        alpha = clamp(0.45 + nodeGlow * 0.5 + spec * 0.3, 0.35, 0.95);
    }

    float fog = exp(-0.08 * t * t);
    color *= fog;
    color += vec3(0.02, 0.04, 0.06) * (1.0 - fog);

    return vec4(color, alpha * fog);
}
)";

const char* kProceduralFragmentShader = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform float uTempo;
uniform float uEnergy;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform int uMode;

const float PI = 3.14159265359;
const float TAU = 6.28318530718;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float hash13(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.3, 0.7));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.3, 0.6);
    vec3 b = vec3(0.5, 0.4, 0.4);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.0, 0.33, 0.67);
    return a + b * cos(TAU * (c * t + d));
}

vec4 renderNebula(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float baseFreq = mix(1.5, 4.5, clamp(energy, 0.0, 1.0));
    float timeWarp = time * (0.2 + tempo * 0.1 + high * 0.2);

    float n = noise(st * baseFreq + timeWarp);
    float n2 = noise(st * (baseFreq * 1.8) - timeWarp * 0.6);
    float combined = mix(n, n2, 0.5 + 0.5 * sin(time * 0.8 + bass * 3.0));

    float poster = floor(combined * 8.0) / 8.0;
    float dither = fract(sin(dot(st + time, vec2(12.9898, 78.233))) * 43758.5453);
    float intensity = clamp(poster + dither * 0.02, 0.0, 1.0);

    vec3 color = palette(intensity + mid * 0.2);
    color *= vec3(0.6 + bass * 0.8, 0.6 + mid * 0.7, 0.7 + high * 0.9);

    float alpha = clamp(0.35 + intensity * 0.55 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderASCIIOcean(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 waves = st;
    waves.x += sin(st.y * 2.0 + time * 0.4) * 0.1;
    float surface = sin(waves.x * 3.2 + time * (0.9 + tempo * 0.2));
    surface += sin((waves.x + waves.y) * 5.5 + time * 1.6) * 0.6;
    surface += sin(waves.y * 7.5 - time * 1.1) * 0.4;
    surface *= 0.35;
    surface += 0.5 + energy * 0.25 + bass * 0.15;
    surface = clamp(surface, 0.0, 1.0);

    float levels = 12.0;
    float idx = floor(surface * levels);
    float asciiIntensity = idx / max(levels - 1.0, 1.0);
    float edge = smoothstep(0.1, 0.9, fract(surface * levels));

    vec3 deep = vec3(0.03, 0.08, 0.18);
    vec3 crest = vec3(0.22 + high * 0.35, 0.55 + mid * 0.3, 0.85 + bass * 0.2);
    vec3 foam = vec3(0.8 + high * 0.2, 0.9, 0.95);

    vec3 color = mix(deep, crest, asciiIntensity);
    color = mix(color, foam, edge * (0.4 + energy * 0.2));
    color += vec3(0.05, 0.07, 0.1) * sin((st.y + time * 0.5) * 40.0) * 0.2;

    float alpha = clamp(0.35 + asciiIntensity * 0.4 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderSacredGeometry(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 q = st * 1.2;
    float r = length(q);
    float angle = atan(q.y, q.x);

    float petals = 6.0 + floor(mid * 6.0);
    float radial = pow(abs(cos(petals * angle)), 2.5);
    float lattice = abs(cos((petals * 0.5 + 2.0) * angle + time * 0.6));
    float concentric = sin(r * (28.0 + high * 12.0) + time * (1.4 + tempo * 0.3)) * 0.5 + 0.5;
    float spiral = sin(angle * 4.0 + time * 1.6 + r * 12.0);

    float sacred = mix(radial, concentric, 0.6) + lattice * 0.2 + spiral * 0.2;
    sacred *= exp(-r * (1.6 - energy * 0.4));
    sacred = clamp(sacred, 0.0, 1.2);

    vec3 inner = vec3(0.95 + high * 0.3, 0.75 + mid * 0.2, 0.55 + bass * 0.2);
    vec3 outer = vec3(0.1 + bass * 0.25, 0.05 + mid * 0.2, 0.12 + high * 0.2);
    vec3 aura = vec3(0.6 + high * 0.3, 0.25 + mid * 0.2, 0.7 + bass * 0.25);

    float auraMask = smoothstep(0.25, 0.75, sacred) * (0.6 + energy * 0.3);
    vec3 color = mix(outer, inner, sacred);
    color += aura * auraMask;

    float alpha = clamp(0.4 + sacred * 0.5 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec3 glitchPaletteColor(float seed, float energy, float bass, float high) {
    vec3 c0 = vec3(0.95, 0.25, 0.32);
    vec3 c1 = vec3(0.2, 0.85, 0.92);
    vec3 c2 = vec3(0.95, 0.8, 0.2);
    vec3 c3 = vec3(0.58, 0.28, 0.9);
    vec3 c4 = vec3(0.18, 0.95, 0.42);
    vec3 c5 = vec3(0.95, 0.48, 0.12);

    float band = floor(seed * 6.0);
    vec3 color = c5;
    if (band < 1.0) {
        color = c0;
    } else if (band < 2.0) {
        color = c1;
    } else if (band < 3.0) {
        color = c2;
    } else if (band < 4.0) {
        color = c3;
    } else if (band < 5.0) {
        color = c4;
    }

    vec3 gain = vec3(0.7 + energy * 0.4, 0.7 + bass * 0.35, 0.7 + high * 0.45);
    return color * gain;
}

vec4 renderGlitchGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 jittered = st;
    float energyDrive = clamp(energy * 1.3 + tempo * 0.35, 0.0, 2.5);
    float bassDrive = clamp(bass * 1.8, 0.0, 2.2);
    float midDrive = clamp(mid * 1.7, 0.0, 2.0);
    float highDrive = clamp(high * 2.0, 0.0, 2.4);

    float driftSeed = hash(vec2(floor(time * 5.0), floor(st.x * 28.0)));
    jittered.x += (driftSeed - 0.5) * (0.02 + 0.12 * highDrive);
    jittered.y += sin(time * (1.6 + tempo * 0.45) + st.x * (14.0 + bassDrive * 4.0)) * (0.015 + 0.05 * midDrive);
    jittered.y += cos(time * (0.8 + tempo * 0.2) + st.y * 18.0) * (0.01 + 0.04 * energyDrive);
    jittered.x += cos(time * (0.9 + tempo * 0.3) + st.y * (10.0 + highDrive * 5.0)) * (0.008 + 0.035 * midDrive);

    vec3 accumColor = vec3(0.0);
    float accumWeight = 0.0;

    for (int i = 0; i < 3; ++i) {
        float fi = float(i);
        float scaleSeed = hash(vec2(fi * 7.11, floor(time * 3.1)) + jittered);
        float baseScale = mix(6.0 + fi * 3.0,
                              14.0 + energyDrive * 9.0 + bassDrive * 6.0 + fi * (3.5 + energyDrive * 1.6),
                              scaleSeed);
        float scale = baseScale + tempo * (1.0 + fi * 0.75) + highDrive * 2.0;

        vec2 grid = jittered * scale;
        vec2 cell = floor(grid);
        vec2 cellUV = fract(grid);

        float seed = hash(cell + fi * 19.31 + floor(time * (2.0 + tempo * 1.2)));
        float sizeX = mix(0.18, 1.08, clamp(seed + bassDrive * 0.25, 0.0, 1.0));
        float sizeY = mix(0.18, 1.08, clamp(hash(cell.yx + fi * 7.91) + midDrive * 0.25, 0.0, 1.0));
        float blockMask = step(cellUV.x, sizeX) * step(cellUV.y, sizeY);

        float flickerSeed = hash(cell + vec2(fi * 11.3, floor(time * (8.0 + tempo * 3.0))));
        float flickerThreshold = mix(0.78, 0.22, clamp(highDrive * 0.45 + energyDrive * 0.35, 0.0, 1.0));
        float flicker = step(flickerThreshold, flickerSeed);

        float densityGate = smoothstep(0.12, 0.85, sizeX * sizeY * (0.6 + energyDrive * 0.3 + bassDrive * 0.2));
        float glitch = blockMask * flicker * densityGate;

        if (glitch > 0.0) {
            vec3 color = glitchPaletteColor(seed, energy, bass, high);
            float pulse = 0.55 + 0.5 * sin(time * (6.0 + tempo * 1.4 + bassDrive * 0.8)
                                           + fi * 1.7 + cell.x * 0.8 + cell.y * 0.4);
            float shimmerBoost = 0.35 + 0.65 * clamp(highDrive * 0.5 + energyDrive * 0.2, 0.0, 1.1);
            color *= pulse * shimmerBoost;
            color = mix(color, color.zyx, clamp(highDrive * 0.3, 0.0, 1.0));
            color += vec3(0.08, 0.05, 0.12) * (bassDrive * 0.2);
            accumColor += color * glitch;
            accumWeight += glitch * (0.8 + energyDrive * 0.4 + bassDrive * 0.3);
        }
    }

    if (accumWeight > 0.0) {
        accumColor /= accumWeight;
    }

    float scanline = sin((st.y + time * 1.6) * (120.0 + highDrive * 40.0)) * (0.03 + 0.04 * highDrive);
    accumColor += vec3(scanline * 0.4, scanline * 0.22, scanline * 0.5);

    float bassWave = sin((st.x + time * 0.8) * (14.0 + bassDrive * 6.0));
    vec3 bassTint = vec3(0.18 + bassDrive * 0.22,
                         0.08 + midDrive * 0.18,
                         0.22 + highDrive * 0.2);
    accumColor += bassTint * bassWave * 0.2 * clamp(energyDrive * 0.4 + bassDrive * 0.4, 0.0, 1.0);

    float shimmer = hash(vec2(floor(st.y * 160.0), floor(time * 24.0)))
                    * (0.08 + energy * 0.35 + highDrive * 0.2);
    accumColor += vec3(shimmer * (0.6 + high * 0.4));

    accumColor = clamp(accumColor, 0.0, 1.0);
    accumColor = pow(accumColor, vec3(0.9));
    accumColor = floor(accumColor * 8.0) / 8.0;

    float weightFactor = clamp(accumWeight * (0.35 + energyDrive * 0.3)
                               + energyDrive * 0.35 + bassDrive * 0.25 + highDrive * 0.2,
                               0.0,
                               1.35);
    float alpha = clamp(0.35 + weightFactor, 0.0, 1.0);
    return vec4(accumColor, alpha);
}

mat3 rotationY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        vec3(c, 0.0, -s),
        vec3(0.0, 1.0, 0.0),
        vec3(s, 0.0, c)
    );
}

float smoothMin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float chemistryNoise(vec3 p, float time) {
    vec3 np = normalize(p + 0.0001);
    float a = noise(np.xy * 1.7 + time * 0.08);
    float b = noise(np.yz * 1.9 - time * 0.05 + 0.77);
    float c = noise(np.zx * 2.1 + time * 0.11 - 0.33);
    float n = (a + b + c) * 0.7;
    float mixBias = clamp(abs(np.y) * 0.6 + abs(np.xz) * 0.2, 0.0, 1.0);
    return mix(n, 0.5, mixBias);
}

float chemistryField(vec3 p, float time, float energyDrive) {
    float dOuter = (-length(p) + 2.3) + 1.6 * chemistryNoise(p, time * 0.7);
    float dInner = (length(p) - (1.1 + energyDrive * 0.4)) + 1.2 * chemistryNoise(p * 1.4, time * 0.9);
    float d = min(dOuter, dInner);

    float links = 999.0;
    float thickness = 0.06;
    vec3 q = p * 0.9;
    links = smoothMin(links, max(abs(q.x) - thickness, abs(q.y + q.z * 0.25) - 0.18), 1.2);
    links = smoothMin(links, max(abs(q.z) - thickness, abs(q.x + q.y * 0.5) - 0.18), 1.2);
    links = smoothMin(links, max(abs(q.z - q.y * 0.3) - thickness, abs(q.x - q.y * 0.2) - 0.18), 1.2);
    links = smoothMin(links, max(abs(q.z * 0.3 - q.y) - thickness, abs(q.x + q.z) - 0.18), 1.2);
    links = smoothMin(links, max(abs(q.z * -0.25 + q.y) - thickness, abs(-q.x + q.z) - 0.18), 1.2);

    return min(d, links);
}

vec4 renderChemicalFlow(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    float aspect = 1.0;
    vec3 ro = vec3(0.0, 0.0, -2.6);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.3));

    float tempoDrive = clamp(tempo * 0.8 + energy * 0.6, 0.0, 2.5);
    float energyDrive = clamp(energy * 1.2 + bass * 0.5 + high * 0.35, 0.0, 3.0);
    float swirlPhase = time * (0.35 + tempo * 0.25) + energyDrive * 0.12;

    mat3 rot = rotationY(swirlPhase * 0.7);
    ro = rot * ro;
    rd = rot * rd;

    vec3 accumColor = vec3(0.0);
    float accumAlpha = 0.0;

    float t = 0.0;
    for (int i = 0; i < 72; ++i) {
        vec3 p = ro + rd * t;
        float mask = clamp(1.0 - length(p) / 3.2, 0.0, 1.0);

        float wobble = sin(dot(p, vec3(1.7, 1.3, 1.9)) + time * 0.9) * 0.35;
        float twist = sin(time * 0.6 + p.y * 2.6) * mask * 0.8;
        p = rotationY(wobble + twist) * p;
        p.y += sin(time + p.x * 1.7) * mask * 0.4;
        p *= 1.05 + sin(time * 0.5 + length(p)) * mask * 0.25;

        float d = chemistryField(p, time, energyDrive);

        if (d < 0.01 || i == 71) {
            float iter = float(i) / 72.0;
            float ao = 1.0 - iter;
            ao = 1.0 - ao * ao;

            float chemMask = clamp(1.0 - length(p) / 2.4, 0.0, 1.0);
            float phase = abs(sin(time * -1.5 + length(p) + p.x));
            float ripple = max(0.0, chemistryNoise(p * 1.8, time) * 4.0 - 2.6);

            vec3 chemColor = mix(vec3(0.12, 0.9, 0.75), vec3(0.7, 0.2, 0.8), clamp(high * 0.6 + mid * 0.3, 0.0, 1.0));
            vec3 phaseTint = mix(vec3(0.95, 0.65, 0.35), vec3(0.2, 0.45, 0.85), clamp(bass * 0.7, 0.0, 1.0));

            vec3 layerColor = chemColor * ripple * chemMask;
            layerColor += vec3(0.11, 0.45, 0.58) * ao * 6.0;
            layerColor += phaseTint * (t * 0.14) * (0.6 + energyDrive * 0.2);

            layerColor *= 1.6 + energyDrive * 0.35;
            layerColor -= vec3(0.12);

            float alpha = clamp(chemMask * (0.5 + ripple * 0.4) + ao * 0.3, 0.1, 1.0);

            accumColor = layerColor;
            accumAlpha = alpha;
            break;
        }

        float stepSize = max(0.02, d * 0.45);
        t += stepSize;
    }

    vec2 vignetteUV = (uv * 0.5 + 0.5) * vec2(aspect, 1.0);
    vignetteUV *= 1.0 - vignetteUV.yx;
    float vig = pow(clamp(vignetteUV.x * vignetteUV.y * 18.0, 0.0, 1.0), 0.3);
    accumColor *= vig;

    accumColor.g *= 0.8;
    accumColor.r *= 1.4;

    return vec4(accumColor, clamp(accumAlpha * vig, 0.0, 1.0));
}

vec2 polarMod(vec2 p, float repetitions) {
    float angle = atan(p.x, p.y) + PI / repetitions;
    float fullCircle = TAU / repetitions;
    angle = floor(angle / fullCircle) * fullCircle;
    return p * mat2(cos(-angle), sin(-angle), -sin(-angle), cos(-angle));
}

float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

vec3 fractalFold(vec3 p) {
    for (int i = 0; i < 5; ++i) {
        p = abs(p) - 1.0;
        float t = iTime * (0.25 + 0.05 * float(i));
        p.xy *= rotation2D(t * 0.9);
        p.xz *= rotation2D(t * 0.3 + 0.4);
    }
    p.xz *= rotation2D(iTime * 0.8);
    return p;
}

float phantomField(vec3 p) {
    vec3 q = p;
    q.x = mod(q.x - 5.0, 10.0) - 5.0;
    q.y = mod(q.y - 5.0, 10.0) - 5.0;
    q.z = mod(q.z, 16.0) - 8.0;
    q.xy = polarMod(q.xy, 5.0);
    vec3 folded = fractalFold(q);
    return sdBox(folded, vec3(0.4, 0.8, 0.3));
}

vec4 renderPhantomFractals(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    float minRes = 1.0;
    vec3 camPos = vec3(0.3 * sin(time * 0.8 + bass * 2.2),
                       0.4 * cos(time * 0.3 + mid * 1.6),
                       -3.2 - time * (0.5 + tempo * 0.35 + energy * 0.2));
    vec3 camDir = normalize(vec3(0.0, 0.0, -1.0));
    vec3 camUp = normalize(vec3(1.0 + sin(time) * 0.5, 1.4 + energy * 0.4, 0.2 + high * 0.3));
    vec3 camSide = normalize(cross(camDir, camUp));

    vec3 ray = normalize(camSide * (uv.x + bass * 0.12) + camUp * (uv.y + mid * 0.08) + camDir);

    float acc = 0.0;
    float accHighlight = 0.0;
    float radiusInfluence = pow(length(st) * 1.2, 5.0);
    float t = 0.0;
    float maxDist = 48.0;
    float beatBoost = clamp(tempo * 0.25 + energy * 0.6 + bass * 0.45, 0.0, 2.8);

    for (int i = 0; i < 90; ++i) {
        vec3 pos = camPos + ray * t;
        float dist = phantomField(pos);
        dist = max(abs(dist), 0.015);
        float falloff = exp(-dist * (2.6 + beatBoost));

        float band = mod(length(pos) + 24.0 * time, 30.0);
        if (band < 3.0) {
            float highlight = falloff * (1.0 + high * 0.6 + bass * 0.4);
            accHighlight += highlight;
        }

        float radialFalloff = clamp(1.0 - radiusInfluence * 0.000000004, 0.0, 1.0);
        acc += falloff * radialFalloff;
        t += dist * 0.5;
        if (t > maxDist) {
            break;
        }
    }

    vec3 baseColor = vec3(0.65, 0.8, 0.95) * (0.01 + acc * 0.012);
    vec3 accent = vec3(0.25 + bass * 0.2, 0.5 + mid * 0.18, 0.9 + high * 0.22) * (0.012 + accHighlight * 0.006);

    vec3 color = baseColor + accent;
    color.r += acc * 0.003 + bass * 0.1;
    color.g += acc * 0.0025 + mid * 0.06;
    color.b += acc * 0.0032 + high * 0.08;

    float alpha = clamp(1.0 - t * 0.03 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

void main() {
    vec2 st = (vUV - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    vec4 color;
    if (uMode == 1) {
        color = renderASCIIOcean(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 2) {
        color = renderSacredGeometry(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 3) {
        color = renderGlitchGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 4) {
        color = renderChemicalFlow(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 5) {
        color = renderCrystalLattice(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 6) {
        color = renderPhantomFractals(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 7) {
        color = renderFractalObject(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 8) {
        color = renderPulsarTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 9) {
        color = renderAuroraBloom(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 10) {
        color = renderRibbonScanlines(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else {
        color = renderNebula(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    }

    FragColor = color;
}
)";

const char* kCompositeFragmentShader = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uLayerTex;
uniform float uOpacity;

void main() {
    vec4 color = texture(uLayerTex, vUV);
    FragColor = vec4(color.rgb, color.a * uOpacity);
}
)";

} // namespace

ModularLayer::ModularLayer() = default;

ModularLayer::~ModularLayer() {
    shutdown();
}

bool ModularLayer::initialize(int width, int height) {
    if (initialized_) {
        resize(width, height);
        return true;
    }

    if (!createResources(width, height)) {
        std::cerr << "ModularLayer: failed to create framebuffer resources" << std::endl;
        return false;
    }

    if (!ensureShader() || !ensureCompositeShader()) {
        destroyResources();
        return false;
    }

    initialized_ = true;
    width_ = width;
    height_ = height;
    return true;
}

void ModularLayer::shutdown() {
    destroyResources();
    proceduralShader_.reset();
    compositeShader_.reset();
    initialized_ = false;
}

void ModularLayer::resize(int width, int height) {
    if (!initialized_) {
        initialize(width, height);
        return;
    }

    if (width == width_ && height == height_) {
        return;
    }

    destroyResources();
    if (!createResources(width, height)) {
        std::cerr << "ModularLayer: failed to resize framebuffer" << std::endl;
        return;
    }
    width_ = width;
    height_ = height;
}

bool ModularLayer::createResources(int width, int height) {
    destroyResources();

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ModularLayer: framebuffer incomplete (status=" << std::hex << status << std::dec << ")" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroyResources();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Fullscreen quad
    std::array<float, 16> quadData = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);

    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, quadData.size() * sizeof(float), quadData.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    width_ = width;
    height_ = height;

    return true;
}

void ModularLayer::destroyResources() {
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (colorTexture_) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

bool ModularLayer::ensureShader() {
    if (proceduralShader_) {
        return true;
    }

    proceduralShader_ = std::make_unique<Shader>();
    if (!proceduralShader_->loadFromSource(kQuadVertexShader, kProceduralFragmentShader)) {
        std::cerr << "ModularLayer: failed to compile procedural shader" << std::endl;
        proceduralShader_.reset();
        return false;
    }
    return true;
}

bool ModularLayer::ensureCompositeShader() {
    if (compositeShader_) {
        return true;
    }

    compositeShader_ = std::make_unique<Shader>();
    if (!compositeShader_->loadFromSource(kQuadVertexShader, kCompositeFragmentShader)) {
        std::cerr << "ModularLayer: failed to compile composite shader" << std::endl;
        compositeShader_.reset();
        return false;
    }
    return true;
}

void ModularLayer::render(const LayerContext& context) {
    if (!initialized_ || (!enabled_ && !debugPreview_)) {
        return;
    }

    if (!ensureShader()) {
        return;
    }

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    proceduralShader_->use();
    proceduralShader_->setUniform2f("uResolution", static_cast<float>(width_), static_cast<float>(height_));
    proceduralShader_->setUniform1f("uTime", context.time);
    proceduralShader_->setUniform1f("uTempo", context.tempo);

    const auto* audio = context.audio;
    float energy = audio ? audio->energy : 0.0f;
    float bass = audio ? audio->bassEnergy : 0.0f;
    float mid = audio ? audio->midEnergy : 0.0f;
    float high = audio ? audio->highEnergy : 0.0f;

    proceduralShader_->setUniform1f("uEnergy", energy);
    proceduralShader_->setUniform1f("uBass", bass);
    proceduralShader_->setUniform1f("uMid", mid);
    proceduralShader_->setUniform1f("uHigh", high);
    proceduralShader_->setUniform1i("uMode", mode_);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

void ModularLayer::composite(const LayerContext& context, float opacity) {
    if (!initialized_ || (!enabled_ && !debugPreview_)) {
        return;
    }

    if (!ensureCompositeShader()) {
        return;
    }

    if (opacity <= 0.0f && !debugPreview_) {
        return;
    }

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    compositeShader_->use();
    float finalOpacity = enabled_ ? opacity : opacity * 0.6f;
    compositeShader_->setUniform1f("uOpacity", finalOpacity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    compositeShader_->setUniform1i("uLayerTex", 0);

    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glViewport(0, 0, context.screenWidth, context.screenHeight);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}
