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
uniform vec3 uPrimaryColor;
uniform vec3 uSecondaryColor;
uniform float uColorBlend;

const float PI = 3.14159265359;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

vec2 hash2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)),
             dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
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

float fbm(vec2 p) {
    float value = 0.0;
    float amp = 0.5;
    mat2 m = mat2(1.7, 1.2, -1.2, 1.7);
    for (int i = 0; i < 5; ++i) {
        value += amp * noise(p);
        p = m * p + vec2(0.21, 0.17);
        amp *= 0.5;
    }
    return value;
}

vec2 rotate(vec2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c) * p;
}

vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
    return a + b * cos(2.0 * PI * (c * t + d));
}

float voronoi(vec2 p, out float edge, out float cellSeed) {
    vec2 n = floor(p);
    vec2 f = fract(p);
    float min1 = 10.0;
    float min2 = 10.0;
    vec2 best = vec2(0.0);

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 g = vec2(float(x), float(y));
            vec2 offset = hash2(n + g);
            vec2 r = g + offset - f;
            float d = dot(r, r);
            if (d < min1) {
                min2 = min1;
                min1 = d;
                best = g + offset;
            } else if (d < min2) {
                min2 = d;
            }
        }
    }

    edge = min2 - min1;
    cellSeed = hash(n + best);
    return sqrt(min1);
}

float mapScene(vec3 p, float time, float energy, float bass, float mid, float high) {
    vec3 q = p;
    q.xz = rotate(q.xz, time * 0.4 + bass * 0.8);
    q.xy = rotate(q.xy, time * 0.25 + mid * 0.5);
    float displacement = fbm(q.xz * (2.2 + high * 0.6) + time * 0.3) * (0.12 + energy * 0.15);
    float sphere = length(q) - (0.65 + bass * 0.35) - displacement;
    float torus = length(vec2(length(q.xz) - (0.9 + mid * 0.4), q.y)) - (0.23 + high * 0.12) - displacement * 0.5;
    return min(sphere, torus);
}

vec3 estimateNormal(vec3 p, float time, float energy, float bass, float mid, float high) {
    float eps = 0.0015;
    vec3 ex = vec3(eps, 0.0, 0.0);
    vec3 ey = vec3(0.0, eps, 0.0);
    vec3 ez = vec3(0.0, 0.0, eps);
    float dx = mapScene(p + ex, time, energy, bass, mid, high) - mapScene(p - ex, time, energy, bass, mid, high);
    float dy = mapScene(p + ey, time, energy, bass, mid, high) - mapScene(p - ey, time, energy, bass, mid, high);
    float dz = mapScene(p + ez, time, energy, bass, mid, high) - mapScene(p - ez, time, energy, bass, mid, high);
    return normalize(vec3(dx, dy, dz));
}

vec4 renderNebula(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    float warp = time * (0.15 + tempo * 0.05);
    float n = fbm(p * (1.8 + energy * 0.6) + warp);
    float glow = fbm(p * 4.5 + warp * 0.6);
    vec3 base = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    vec3 accent = mix(uSecondaryColor, uPrimaryColor, n);
    vec3 color = mix(base, accent, 0.5 + 0.5 * n);
    color += vec3(0.15, 0.10, 0.20) * glow * (0.6 + energy * 0.6);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.25 + n * 0.6 + energy * 0.35, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderASCIIOcean(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    p.x += sin(st.y * 2.5 + time * 0.6) * 0.12;
    float surface = sin(p.x * 5.0 + time * (1.1 + tempo * 0.2));
    surface += sin((p.x + p.y) * 4.0 - time * (0.8 + mid * 0.3));
    surface += sin(p.y * 7.0 + time * 1.4) * 0.5;
    surface = surface * 0.3 + 0.5 + energy * 0.25 + bass * 0.15;
    surface = clamp(surface, 0.0, 1.0);
    float crest = smoothstep(0.35, 0.75, surface);
    vec3 deep = mix(uPrimaryColor, vec3(0.02, 0.05, 0.12), 0.7);
    vec3 foam = mix(uSecondaryColor, vec3(0.85, 0.95, 1.0), 0.5);
    vec3 color = mix(deep, foam, crest);
    color += vec3(0.04, 0.06, 0.10) * sin((st.y + time * 0.5) * 40.0) * 0.2;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.35 + crest * 0.45 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderSacredGeometry(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float angle = atan(st.y, st.x);
    float petals = 6.0 + floor(mid * 6.0);
    float radial = pow(abs(cos(petals * angle)), 2.5);
    float rings = sin(r * (22.0 + high * 8.0) + time * (1.2 + tempo * 0.3)) * 0.5 + 0.5;
    float mask = radial * rings * exp(-r * (1.4 - energy * 0.3));
    mask = clamp(mask, 0.0, 1.0);
    vec3 inner = mix(uSecondaryColor, vec3(0.85, 0.55, 0.35), 0.4 + high * 0.2);
    vec3 outer = mix(uPrimaryColor, vec3(0.05, 0.03, 0.12), 0.6);
    vec3 color = mix(outer, inner, mask);
    color += vec3(0.20, 0.10, 0.30) * mask * high;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(mask * (0.7 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderGlitchGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 grid = (st + 0.5) * 8.0;
    vec2 cell = floor(grid);
    vec2 cellUV = fract(grid) - 0.5;
    float jitter = hash(cell + floor(time * (1.5 + tempo)));
    float mask = smoothstep(0.45 + high * 0.2, 0.0, length(cellUV + (jitter - 0.5) * 0.3));
    float pulse = sin(time * (4.0 + tempo * 1.2) + cell.x * 0.8 + cell.y * 0.6) * 0.5 + 0.5;
    vec3 base = mix(uPrimaryColor, uSecondaryColor, hash(cell));
    vec3 color = base * (0.3 + 0.7 * pulse);
    color = mix(color, color.bgr, high * 0.4);
    color *= mask * (0.6 + energy * 0.6);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(mask * (0.45 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderChemicalFlow(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, time * (0.25 + tempo * 0.2));
    float flow = fbm(p * (3.0 + energy));
    float swirl = sin(p.x * 5.0 + time * 1.4) + cos(p.y * 7.0 - time * 1.1);
    vec3 color = mix(uPrimaryColor * 0.5, uSecondaryColor + vec3(bass * 0.2, mid * 0.15, high * 0.3), flow);
    color += vec3(0.08, 0.02, 0.12) * swirl * 0.2;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.30 + flow * 0.5 + energy * 0.3, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderCrystalLattice(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, time * 0.1);
    p *= 3.0;
    vec2 cell = fract(p) - 0.5;
    float node = exp(-12.0 * dot(cell, cell));
    float cross = exp(-30.0 * abs(cell.x)) + exp(-30.0 * abs(cell.y));
    float glow = node + 0.2 * cross;
    vec3 base = mix(uPrimaryColor, uSecondaryColor, 0.5 + 0.5 * bass);
    vec3 color = base * (0.5 + glow * (0.8 + energy * 0.5));
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(glow * (0.6 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderPhantomFractals(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float bands = sin(r * (18.0 + bass * 8.0) - time * (1.5 + tempo * 0.5));
    float fract = fbm(st * 6.0 + time * 0.4);
    float mask = smoothstep(0.0, 1.0, bands * 0.5 + 0.5) * (0.4 + fract);
    vec3 color = mix(uPrimaryColor * 0.4, uSecondaryColor, mask);
    color += vec3(0.12, 0.20, 0.30) * fract * high;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(mask * (0.6 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderFractalObject(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, time * 0.2);
    float r = length(p);
    float angle = atan(p.y, p.x);
    float warp = sin(angle * 3.0 + time * (1.0 + tempo * 0.2));
    float layers = fbm(p * 5.0 + warp);
    vec3 color = mix(uPrimaryColor, uSecondaryColor, layers);
    color += vec3(0.20, 0.10, 0.35) * warp * high;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.4 + (1.0 - smoothstep(0.0, 1.2, r)) * (0.5 + energy * 0.3) + layers * 0.3, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderPulsarTunnel(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float angle = atan(st.y, st.x);
    float spiral = sin(r * 12.0 - time * (2.0 + tempo) + angle * 4.0);
    vec3 color = mix(uPrimaryColor, uSecondaryColor, 0.4 + 0.3 * bass);
    color *= (0.6 + spiral * 0.4);
    color += vec3(0.20, 0.10, 0.30) * (1.0 - r) * (0.6 + energy * 0.4);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp((1.0 - r) * (0.8 + energy * 0.4) + spiral * 0.1, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderAuroraBloom(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    float band = sin((p.x + time * (0.6 + tempo * 0.2)) * 6.0);
    float curtain = smoothstep(-0.4 - high * 0.2, 0.4 + high * 0.2, p.y + 0.2 * band);
    vec3 base = mix(uPrimaryColor, vec3(0.10, 0.20, 0.35), 0.5);
    vec3 glow = mix(uSecondaryColor, vec3(0.95, 0.90, 1.0), 0.5 + high * 0.2);
    vec3 color = mix(base, glow, curtain);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(curtain * (0.5 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderRibbonScanlines(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = rotate(st, 0.3);
    float stripes = sin(p.y * (80.0 + high * 40.0) + time * (4.0 + tempo)) * 0.5 + 0.5;
    float ribbons = sin(p.x * 6.0 + time * (1.2 + tempo * 0.4)) * 0.5 + 0.5;
    vec3 colorA = mix(uPrimaryColor, uSecondaryColor, stripes);
    vec3 colorB = mix(uSecondaryColor, uPrimaryColor, ribbons);
    vec3 color = mix(colorA, colorB, 0.5);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.30 + stripes * 0.4 + ribbons * 0.3, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderKaleidoscopeFractal(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float r = length(st);
    float angle = atan(st.y, st.x);
    float sectors = 6.0 + floor(mid * 10.0);
    float sectorAngle = 2.0 * PI / max(sectors, 1.0);
    angle = mod(angle, sectorAngle);
    angle = abs(angle - sectorAngle * 0.5);
    vec2 dir = vec2(cos(angle), sin(angle));
    vec2 p = dir * r;
    vec2 warp = vec2(
        fbm(p * (3.2 + high * 2.4) + time * 0.35),
        fbm(p * (2.6 + mid * 1.8) - time * 0.28)
    );
    p += warp * (0.6 + high * 0.7 + energy * 0.4);
    float n = fbm(p * (2.4 + energy * 1.3) + time * 0.25);
    float bloom = fbm(p * 5.0 - time * 0.45);
    vec3 base = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    vec3 accent = mix(uSecondaryColor, base.bgr, clamp(high * 0.7 + mid * 0.2, 0.0, 1.0));
    vec3 color = mix(base, accent, n);
    color += vec3(0.3, 0.15, 0.45) * bloom * (0.4 + high * 0.6);
    color += vec3(0.12, 0.19, 0.25) * smoothstep(0.0, 1.2, r) * (0.3 + energy * 0.4);
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.3 + n * 0.6 + bloom * 0.25 + energy * 0.25, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderVoronoiCells(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float scale = mix(1.4, 3.8, clamp(bass * 0.9, 0.0, 1.0));
    vec2 p = st * (3.0 + energy * 1.2);
    p *= scale;
    float edge;
    float cellSeed;
    float distance = voronoi(p + time * 0.12, edge, cellSeed);
    float interior = smoothstep(0.0, 0.9, distance * 1.5);
    float edgeGlow = smoothstep(0.02, 0.2, 1.0 - edge);
    vec3 cellColor = mix(uPrimaryColor * (0.4 + bass * 0.4),
                         uSecondaryColor * (0.6 + high * 0.5),
                         interior);
    cellColor += vec3(0.15, 0.25, 0.2) * cellSeed * 0.6;
    cellColor += vec3(0.4, 0.55, 0.65) * edgeGlow * (0.4 + high * 0.7);
    cellColor = clamp(cellColor, 0.0, 1.0);
    float alpha = clamp(0.25 + interior * 0.5 + edgeGlow * (0.35 + high * 0.2) + energy * 0.2, 0.0, 1.0);
    return vec4(cellColor, alpha);
}

vec4 renderRaymarchedObject(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 target = vec3(0.0);
    vec3 forward = normalize(target - ro);
    vec3 right = normalize(vec3(forward.z, 0.0, -forward.x));
    vec3 up = normalize(cross(right, forward));
    vec3 rd = normalize(forward + right * st.x * 1.4 + up * st.y * 1.0);
    float t = 0.0;
    float d = 0.0;
    bool hit = false;
    for (int i = 0; i < 48; ++i) {
        vec3 pos = ro + rd * t;
        d = mapScene(pos, time, energy, bass, mid, high);
        if (d < 0.0015) {
            hit = true;
            break;
        }
        t += d * 0.85;
        if (t > 12.0) {
            break;
        }
    }

    vec3 color;
    float alpha;
    if (hit) {
        vec3 pos = ro + rd * t;
        vec3 normal = estimateNormal(pos, time, energy, bass, mid, high);
        vec3 lightDir = normalize(vec3(0.6, 0.8, -0.4));
        float diff = max(dot(normal, lightDir), 0.0);
        float spec = pow(max(dot(reflect(-lightDir, normal), -rd), 0.0), 24.0);
        float rim = pow(1.0 - max(dot(normal, -rd), 0.0), 3.0);
        vec3 base = mix(uPrimaryColor, uSecondaryColor, 0.45 + high * 0.35);
        color = base * (0.25 + diff * (0.9 + energy * 0.4));
        color += vec3(0.6, 0.4, 1.0) * spec * (0.3 + high * 0.6);
        color += base.bgr * rim * (0.3 + energy * 0.5);
        color = clamp(color, 0.0, 1.0);
        alpha = clamp(0.35 + diff * 0.4 + rim * 0.4 + energy * 0.25, 0.0, 1.0);
    } else {
        float fade = clamp(1.0 - t / 12.0, 0.0, 1.0);
        vec3 bg = mix(uPrimaryColor * 0.15, uSecondaryColor * 0.45, fade);
        bg += vec3(0.08, 0.1, 0.14) * fbm(st * 3.0 + time * 0.2);
        color = clamp(bg, 0.0, 1.0);
        alpha = clamp(fade * 0.4, 0.0, 0.6);
    }
    return vec4(color, alpha);
}

vec4 renderReactionDiffusionPattern(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st * (3.2 + energy * 1.4);
    vec2 warp = vec2(
        fbm(p + time * 0.35),
        fbm(p + vec2(-4.2, 3.8) - time * 0.27)
    );
    p += warp * (0.7 + mid * 0.7);
    float pattern = fbm(p * (2.6 + tempo * 0.4) + time * 0.12);
    float threshold = mix(0.42, 0.32, clamp(bass * 0.9, 0.0, 1.0));
    float blot = smoothstep(threshold, threshold + 0.18 + high * 0.15, pattern);
    float detail = fbm(p * 5.5 - time * 0.2);
    float veins = smoothstep(0.55, 0.72, detail) * smoothstep(0.35, 0.6, 1.0 - detail);
    vec3 base = mix(uPrimaryColor * 0.5, uSecondaryColor, blot);
    base = mix(base, vec3(0.95, 0.86, 0.68), veins * (0.3 + high * 0.5));
    base += vec3(0.1, 0.06, 0.12) * warp.x * (0.4 + energy * 0.5);
    base = clamp(base, 0.0, 1.0);
    float alpha = clamp(0.3 + blot * 0.5 + veins * 0.3 + energy * 0.2, 0.0, 1.0);
    return vec4(base, alpha);
}

vec4 renderLiquidRefraction(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    vec2 offset = vec2(
        fbm(p * (4.0 + energy) + time * 0.6),
        fbm(p * (4.3 + mid * 1.2) - time * 0.55)
    );
    float strength = (0.12 + energy * 0.12 + bass * 0.05);
    vec2 uv = p + offset * (0.18 + high * 0.12) * strength;
    float bg = fbm(uv * (3.2 + tempo * 0.5) - time * 0.2);
    vec3 base = mix(uPrimaryColor, uSecondaryColor, clamp(0.5 + bg * 0.5, 0.0, 1.0));
    vec2 grad = vec2(dFdx(bg), dFdy(bg));
    float caustic = clamp(length(grad) * (0.7 + high * 0.6), 0.0, 1.2);
    vec3 highlight = vec3(0.8, 0.9, 1.1) * caustic;
    vec3 color = base + highlight + vec3(0.05, 0.04, 0.03) * offset.x;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.25 + bg * 0.3 + caustic * 0.4 + energy * 0.2, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderStarfieldWarp(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    float speed = 0.35 + tempo * 0.3 + energy * 0.25;
    float stretch = 1.2 + bass * 0.8;
    float accum = 0.0;
    float glow = 0.0;
    for (int i = 0; i < 6; ++i) {
        float depth = fract(float(i) / 6.0 + time * speed * 0.18);
        float fade = smoothstep(0.05, 0.25, depth) * (1.0 - depth);
        vec2 dir = uv / (depth * stretch + 0.25);
        vec2 cell = floor(dir);
        vec2 local = fract(dir) - 0.5;
        float seed = hash(cell + float(i));
        vec2 jitter = (seed - 0.5) * vec2(0.4, 0.2);
        float dist = length(local + jitter);
        float star = smoothstep(0.4, 0.0, dist);
        accum += star * fade;
        glow += star * fade * (0.4 + seed);
    }
    vec3 color = mix(uPrimaryColor, vec3(1.0, 0.95, 0.8), clamp(high + energy * 0.5, 0.0, 1.0));
    color *= accum * (1.2 + high * 0.7);
    color += vec3(0.05, 0.08, 0.12) * (0.8 - length(uv)) * (0.4 + energy * 0.3);
    color += vec3(0.4, 0.5, 0.7) * glow * 0.25;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(accum * (0.65 + energy * 0.4), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderPlasmaClassic(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st;
    float v =
        sin(p.x * (10.0 + tempo * 1.8) + time) +
        sin(p.y * (10.0 + high * 4.0) + time * 1.3) +
        sin((p.x + p.y) * (10.0 + tempo) + time * 0.7);
    v = v / 3.0;
    float wave = sin(time * 1.5 + v * 4.0);
    vec3 base = mix(uPrimaryColor, uSecondaryColor, 0.5 + 0.5 * v);
    base += vec3(0.15, 0.10, 0.20) * wave * (0.4 + mid * 0.4);
    base = clamp(base, 0.0, 1.0);
    float alpha = clamp(0.35 + abs(v) * 0.4 + energy * 0.25, 0.0, 1.0);
    return vec4(base, alpha);
}

vec4 renderDomainWarpedFractal(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 base = st * (2.4 + energy * 0.8);
    vec2 q = vec2(
        fbm(base * (3.0 + tempo * 0.3) + time * 0.4),
        fbm(base * (3.0 + tempo * 0.3) - time * 0.45)
    );
    vec2 p = base + q * (0.8 + high * 0.5);
    float f = fbm(p * (4.0 + mid * 0.5));
    float ridge = fbm(p * 8.0 - time * 0.3);
    vec3 color = mix(uPrimaryColor, uSecondaryColor, clamp(0.5 + f * 0.5, 0.0, 1.0));
    color += vec3(0.18, 0.10, 0.30) * ridge * (0.4 + high * 0.6);
    color += vec3(0.05, 0.09, 0.12) * q.x;
    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(0.3 + f * 0.5 + ridge * 0.2 + energy * 0.2, 0.0, 1.0);
    return vec4(color, alpha);
}

void main() {
    vec2 st = (vUV - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    vec4 color;
    if (uMode == 0) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    } else if (uMode == 1) {
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
    } else if (uMode == 11) {
        color = renderNebula(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 12) {
        color = renderKaleidoscopeFractal(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 13) {
        color = renderVoronoiCells(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 14) {
        color = renderRaymarchedObject(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 15) {
        color = renderReactionDiffusionPattern(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 16) {
        color = renderLiquidRefraction(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 17) {
        color = renderStarfieldWarp(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 18) {
        color = renderPlasmaClassic(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 19) {
        color = renderDomainWarpedFractal(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else {
        color = renderDomainWarpedFractal(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
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

void ModularLayer::setColorPalette(const float primary[3], const float secondary[3], float blend) {
    if (primary) {
        colorPrimary_[0] = primary[0];
        colorPrimary_[1] = primary[1];
        colorPrimary_[2] = primary[2];
    }
    if (secondary) {
        colorSecondary_[0] = secondary[0];
        colorSecondary_[1] = secondary[1];
        colorSecondary_[2] = secondary[2];
    }
    if (blend < 0.0f) {
        colorBlend_ = 0.0f;
    } else if (blend > 1.0f) {
        colorBlend_ = 1.0f;
    } else {
        colorBlend_ = blend;
    }
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
    proceduralShader_->setUniform3f("uPrimaryColor", colorPrimary_[0], colorPrimary_[1], colorPrimary_[2]);
    proceduralShader_->setUniform3f("uSecondaryColor", colorSecondary_[0], colorSecondary_[1], colorSecondary_[2]);
    proceduralShader_->setUniform1f("uColorBlend", colorBlend_);

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
