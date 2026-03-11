#include "modular_layer.h"

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

constexpr int kKaleidoscopeModeIndex = 23;

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
uniform float uIntensity;
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

const float kTwoPI = 6.28318530718;
const float kVoxelFallInterval = 100.0;
const float kVoxelHeightRangeBase = 5.0;
const float kVoxelBumpFactorBase = 2.0;
const float kVoxelFovDegrees = 60.0;
const float kVoxelBpm = 114.0;
const int kVoxelSamples = 6;
const int kVoxelMaxDepth = 5;

float hash13(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

float hash11f(float x) {
    return fract(sin(x) * 43758.5453);
}

float hash12f(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float voxelRandom(inout float seed) {
    seed = fract(seed * 43758.5453123 + 0.12345);
    return seed;
}

mat2 voxelRotate(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

vec3 voxelHSV(float h, float s, float v) {
    vec3 res = fract(h + vec3(0.0, 2.0, 1.0) / 3.0);
    res = clamp(abs(res * 6.0 - 3.0) - 1.0, 0.0, 1.0);
    res = (res - 1.0) * s + 1.0;
    res *= v;
    return res;
}

float voxelMap(vec3 p, vec3 camPos, float time, float fallSpeed, float heightRange, out vec3 ID) {
    float res = 1.0;
    ID = floor(p);
    float h = hash12f(ID.xz);
    ID.y -= floor(max(ID.x * ID.x * 0.1, 5.0) - h * heightRange);
    float T = time * fallSpeed / kVoxelFallInterval + hash11f(h);
    if (ID.y > 0.0) {
        float tmp = ID.y / kVoxelFallInterval + T;
        if (fract(tmp) * kVoxelFallInterval > 1.0) {
            res = 0.0;
        }
        ID.y = floor(tmp);
    } else {
        ID.y += floor(T);
    }

    vec3 offset = p + 0.5 - camPos;
    if (dot(offset, offset) < 1.0) {
        res = 0.0;
    }
    return res;
}

float voxelCastRay(vec3 ro, vec3 rd, int itr, vec3 camPos, float time, float fallSpeed, float heightRange,
                   out vec3 ID, out vec3 normal, out vec3 pos, out float seed) {
    pos = floor(ro);
    vec3 ri = 1.0 / rd;
    vec3 rs = sign(rd);
    vec3 dis = (pos - ro + 0.5 + rs * 0.5) * ri;

    float res = -1.0;
    vec3 stepDir = vec3(0.0);

    for (int i = 0; i < itr; ++i) {
        if (voxelMap(pos, camPos, time, fallSpeed, heightRange, ID) > 0.5) {
            res = 1.0;
            break;
        }
        vec3 mm = step(dis.xyz, dis.yzx) * step(dis.xyz, dis.zxy);
        stepDir = mm * rs;
        dis += stepDir * ri;
        pos += stepDir;
    }

    normal = -stepDir;
    vec3 mini = (pos - ro + 0.5 - 0.5 * rs) * ri;
    float t = max(mini.x, max(mini.y, mini.z));

    seed = hash13(ID);

    return t * res;
}

float voxelEmission(float seed) {
    return step(hash11f(seed), 0.08) * 10.0;
}

vec3 voxelObjectColor(float seed) {
    return voxelHSV(hash11f(seed), 0.6, 1.0);
}

float voxelBumpFunc(vec3 p, vec3 n, float seed) {
    float nSeed = dot(abs(n), vec3(1.0, 2.0, 4.0));
    vec2 uv = abs(n.x) > 0.5 ? p.yz : (abs(n.y) > 0.5 ? p.xz : p.xy);
    uv = fract(uv) - 0.5;

    if (hash11f(seed + nSeed) < 0.5) {
        uv.y = -uv.y;
    }
    if (uv.y < -uv.x) {
        uv = -uv.yx;
    }
    float d = abs(length(uv - 0.5) - 0.5);
    const float w = 0.15;
    float tmp = w * w - d * d;
    return tmp > 0.0 ? -sqrt(tmp) : 0.0;
}

vec3 voxelBumpMap(vec3 p, vec3 n, float seed, float bumpFactor) {
    const vec2 e = vec2(0.002, 0.0);
    float ref = voxelBumpFunc(p, n, seed);
    vec3 grad = (vec3(voxelBumpFunc(p - e.xyy, n, seed),
                      voxelBumpFunc(p - e.yxy, n, seed),
                      voxelBumpFunc(p - e.yyx, n, seed)) - ref) / e.x;
    grad -= n * dot(n, grad);
    return normalize(n + grad * bumpFactor);
}

vec3 voxelJitter(vec3 v, float phi, float sinTheta, float cosTheta) {
    vec3 xAxis = normalize(cross(abs(v.yzx) + 0.001, v));
    vec3 yAxis = cross(v, xAxis);
    vec3 zAxis = v;
    return (xAxis * cos(phi) + yAxis * sin(phi)) * sinTheta + zAxis * cosTheta;
}

vec3 voxelPathTrace(vec3 ro, vec3 rd, float time, float fallSpeed, float heightRange, float bumpFactor,
                    float bass, float mid, float high, inout float pathSeed) {
    vec3 acc = vec3(0.0);
    vec3 mask = vec3(1.0);

    vec3 ID;
    vec3 normal;
    vec3 pos;
    float seed;

    float t = voxelCastRay(ro, rd, 80, ro, time, fallSpeed, heightRange, ID, normal, pos, seed);
    if (t < 0.0) {
        return vec3(0.0);
    }
    ro += t * rd;

    vec3 f = ro - pos - 0.5;
    vec3 n = normalize(f * pow(abs(f), vec3(8.0)) + 0.0001);
    n = normalize(n + voxelBumpMap(ro, normal, seed, bumpFactor));

    acc += mask * voxelEmission(seed) * float(kVoxelSamples);
    mask *= voxelObjectColor(seed);

    vec3 ro0 = ro + n * 0.0008;
    vec3 n0 = n;
    vec3 mask0 = mask;
    for (int i = 0; i < kVoxelSamples; ++i) {
        ro = ro0;
        n = n0;
        mask = mask0;
        for (int depth = 1; depth < kVoxelMaxDepth; ++depth) {
            float ur = voxelRandom(pathSeed);
            rd = voxelJitter(n, voxelRandom(pathSeed) * kTwoPI, sqrt(1.0 - ur), sqrt(ur));

            t = voxelCastRay(ro, rd, 20, ro, time, fallSpeed, heightRange, ID, normal, pos, seed);
            if (t < 0.0) {
                break;
            }
            ro += t * rd;

            f = ro - pos - 0.5;
            n = normalize(f * pow(abs(f), vec3(8.0)) + 0.0001);
            n = normalize(n + voxelBumpMap(ro, normal, seed, bumpFactor));

            float emissionBoost = 1.0 + bass * 1.2 + high * 0.8;
            acc += mask * voxelEmission(seed) * emissionBoost;
            mask *= voxelObjectColor(seed);
            ro += n * 0.0008;
        }
    }

    acc /= float(kVoxelSamples);
    return clamp(acc, 0.0, 1.0);
}

vec4 renderVoxelPathTracer(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;

    float tempoFactor = max(0.4, tempo * 0.8 + 0.4);
    float fallSpeed = (kVoxelBpm / 60.0) * (0.6 + tempoFactor * 0.6 + energy * 0.4);
    float heightRange = kVoxelHeightRangeBase * (1.0 + energy * 0.6 + high * 0.5);
    float bumpFactor = kVoxelBumpFactorBase * (1.0 + high * 0.7);

    vec3 camPos = vec3(0.5 + sin(time * 0.25) * tempoFactor * 0.35,
                       6.0 + energy * 3.0,
                       -time * fallSpeed * 1.2);
    camPos.x += cos(time * 0.3) * bass * 1.4;
    camPos.z += sin(time * 0.21) * mid * 0.9;

    vec3 dir = normalize(vec3(0.3, -0.4, -1.0));
    dir.xy = voxelRotate(time * 0.18 + tempoFactor * 0.3) * dir.xy;
    vec3 side = normalize(cross(dir, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(side, dir);
    float fovScale = 1.0 / tan(radians(kVoxelFovDegrees) * 0.5);
    vec3 rd = normalize(uv.x * side + uv.y * up + dir * fovScale);

    float pathSeed = hash12f(uv * uResolution.xy + vec2(time * 21.37, time * 17.53)) * 500.0;

    vec3 col = voxelPathTrace(camPos, rd, time, fallSpeed, heightRange, bumpFactor, bass, mid, high, pathSeed);
    col = pow(col, vec3(0.4545));

    vec3 base = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    col = mix(base, col, 0.7);
    float luminance = clamp((col.r + col.g + col.b) / 3.0, 0.0, 1.0);
    float alpha = clamp(0.35 + luminance * 0.4 + energy * 0.2, 0.0, 1.0);
    return vec4(col, alpha);
}

const float kVolIterations = 17;
const int kVolSteps = 20;
const float kVolStepSize = 0.1;
const float kVolZoom = 0.8;
const float kVolTile = 0.85;
const float kVolSpeed = 0.01;
const float kVolBrightness = 0.0015;
const float kVolDarkMatter = 0.3;
const float kVolDistFading = 0.73;
const float kVolSaturation = 0.85;
const float kVolFormuParam = 0.53;
const int kSnowLayers = 8;
const vec2 kSnowOffset = vec2(0.02, -0.2);

float happyStar(vec2 uv, float anim) {
    uv = abs(uv);
    vec2 denom = max(abs(uv.yx), vec2(0.0005));
    vec2 pos = min(uv.xy / denom, vec2(anim));
    float p = 2.0 - pos.x - pos.y;
    float denomSum = max(uv.x + uv.y, 0.0005);
    return (2.0 + p * (p * p - 1.5)) / denomSum;
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

vec4 renderVolumetricStarfield(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float aspect = max(uResolution.x / uResolution.y, 0.0001);
    vec2 uv = vec2(st.x / aspect, st.y / aspect);

    float temporalSpeed = kVolSpeed + tempo * 0.004 + energy * 0.008;
    float twist = sin(time * (12.0 + high * 6.0)) * 0.1 + 1.0;

    vec3 dir = normalize(vec3(uv * (kVolZoom + energy * 0.25), 1.0));

    vec3 color = vec3(0.0);
    for (int layer = 0; layer < kSnowLayers; ++layer) {
        float layerFrac = float(layer) / float(kSnowLayers);
        float d = fract(layerFrac + time * temporalSpeed);
        float s = mix(40.0, 0.5, d);
        float layerFade = d * smoothstep(1.0, 0.8, d);

        vec2 snowUV = kSnowOffset + uv - vec2(0.25 * sin(time * 0.2 + layerFrac * 3.14), 0.0);
        vec2 id = floor(snowUV * s + layerFrac * 50.0);
        vec2 p = fract(snowUV * s + layerFrac * 50.0) - 0.5;

        float randSeed = hash(id + layerFrac);
        float randPhase = randSeed * 6.28318;
        p += vec2(sin(time + randPhase), cos(time + randPhase)) * 0.3;

        float starValue = happyStar(p * 10.0, twist);
        float flake = smoothstep(1.5, 0.0, starValue);

        vec3 snowColor = mix(uPrimaryColor, uSecondaryColor, clamp(0.5 + randSeed * 0.5, 0.0, 1.0));
        color += snowColor * 0.05 * layerFade * flake;
        color += clamp(uv.y * -0.08 + uv.y * abs(uv.x * 2.5) * -0.07, 0.0, 1.0);
    }
    color *= max(1.0 - dot(uv, uv) * 20.6, 0.0);

    vec3 baseMix = mix(uPrimaryColor, uSecondaryColor, clamp(0.35 + high * 0.4, 0.0, 1.0));
    vec3 from = baseMix * color;

    float s = 0.1;
    float fade = 1.0;
    vec3 accum = vec3(0.0);
    vec3 v = vec3(0.0);
    float rotationAngle = time * (0.01 + mid * 0.01);
    mat2 rot = mat2(cos(rotationAngle), sin(rotationAngle), -sin(rotationAngle), cos(rotationAngle));

    for (int r = 0; r < kVolSteps; ++r) {
        vec3 pos = from + s * dir * 0.5;
        pos = abs(vec3(kVolTile) - mod(pos, vec3(kVolTile * 2.0)));
        vec3 pp = pos;
        float pa = 0.0;
        float a = 0.0;

        for (int i = 0; i < kVolIterations; ++i) {
            float denom = max(dot(pp, pp), 0.0001);
            pp = abs(pp) / denom - vec3(kVolFormuParam);
            pp.xy = rot * pp.xy;
            float len = length(pp);
            a += abs(len - pa);
            pa = len;
        }

        float dm = max(0.0, kVolDarkMatter - a * a * 0.001);
        a *= a * a;

        if (r > 6) {
            fade *= 1.5 - dm;
        }

        vec3 weight = vec3(s, s * s, s * s * s * s);
        v += weight * a * (kVolBrightness + energy * 0.0008) * fade;
        fade *= kVolDistFading + bass * 0.05;
        s += kVolStepSize;
    }

    v = mix(vec3(length(v)), v, kVolSaturation + high * 0.05);
    vec3 result = v * 0.00501 + color * 0.1;
    result = clamp(result, 0.0, 1.0);
    float alpha = clamp(length(result) * 0.9 + energy * 0.35, 0.0, 1.0);
    return vec4(result, alpha);
}

vec4 renderFractalTunnel(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st * (1.1 + energy * 0.25);
    float twist = sin(time * 0.3) * 0.4 + mid * 0.6;
    uv = rotate(uv, twist);
    vec3 dir = normalize(vec3(uv, 1.5));
    float speed = 1.05 + tempo * 0.7 + energy * 0.45;
    float travel = time * speed;
    vec3 accum = vec3(0.0);
    float alpha = 0.0;
    float fade = 1.0;

    for (int i = 0; i < 36; ++i) {
        float t = float(i) * 0.18;
        vec3 pos = dir * (t + travel);
        pos.z = mod(pos.z, 4.0) - 2.0;

        float radius = length(pos.xy);
        float angle = atan(pos.y, pos.x);
        float sectors = 6.0 + floor(mid * 8.0);
        float sectorAngle = 2.0 * PI / max(sectors, 1.0);
        float folded = abs(mod(angle, sectorAngle) - sectorAngle * 0.5);
        float petals = exp(-folded * folded * (18.0 + high * 18.0));

        vec2 coord1 = pos.xy * (2.8 + high * 1.3) + vec2(pos.z * 0.8, pos.z * 0.5);
        vec2 coord2 = pos.yz * (2.2 + energy * 0.8) - vec2(time * 0.18, time * 0.22);
        float warpA = fbm(coord1 + vec2(time * 0.25, -time * 0.21));
        float warpB = fbm(coord2);
        float shell = sin((pos.z + travel) * (4.0 + bass * 3.5) + warpA * 4.0);
        float rings = smoothstep(0.25, 0.85, shell * 0.5 + 0.5);

        float targetRadius = 0.85 + warpA * 0.35 + sin((pos.z + travel) * 1.6) * 0.12;
        float radial = exp(-pow(radius - targetRadius, 2.0) * (10.0 + bass * 9.0));

        float density = petals * rings * (0.55 + warpB * 0.45);
        density *= radial;
        density = clamp(density, 0.0, 1.0);

        float distFade = exp(-t * (0.45 + energy * 0.18));
        float beat = 0.5 + 0.5 * sin(travel * 1.5 + bass * 3.0);

        vec3 layerColor = mix(uPrimaryColor, uSecondaryColor, clamp(warpA * 0.5 + 0.5, 0.0, 1.0));
        layerColor += vec3(0.32, 0.14, 0.45) * petals * (0.35 + high * 0.6);
        layerColor += vec3(0.08, 0.13, 0.18) * rings * (0.3 + bass * 0.45);
        layerColor = clamp(layerColor, 0.0, 1.0);

        accum += layerColor * density * distFade * fade;
        alpha += density * distFade * 0.06 * fade;
        fade *= (0.96 - high * 0.01);
    }

    float coreGlow = exp(-dot(uv, uv) * (3.0 + energy * 1.4));
    vec3 coreColor = mix(uPrimaryColor, uSecondaryColor, clamp(0.5 + 0.5 * sin(travel + high * 2.5), 0.0, 1.0));
    accum += coreColor * coreGlow * (0.4 + high * 0.4 + energy * 0.3);
    alpha += coreGlow * (0.18 + energy * 0.15);

    accum = clamp(accum, 0.0, 1.0);
    alpha = clamp(alpha, 0.0, 1.0);
    return vec4(accum, alpha);
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
    } else if (uMode == 20) {
        color = renderFractalTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 21) {
        color = renderVolumetricStarfield(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 22) {
        color = renderVoxelPathTracer(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
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
    kaleidoscopeShader_.reset();
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

    kaleidoscopeShader_.reset();
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

bool ModularLayer::ensureKaleidoscopeShader() {
    if (kaleidoscopeShader_) {
        return true;
    }

    const std::array<const char*, 3> searchPaths = {
        "shaders/procedural_kaleidoscope.glsl",
        "../shaders/procedural_kaleidoscope.glsl",
        "../../shaders/procedural_kaleidoscope.glsl"
    };

    std::string fragmentSource;
    bool loaded = false;
    for (const char* path : searchPaths) {
        std::ifstream file(path, std::ios::in);
        if (!file.is_open()) {
            continue;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        fragmentSource = buffer.str();
        loaded = true;
        break;
    }

    if (!loaded) {
        std::cerr << "ModularLayer: unable to locate procedural_kaleidoscope.glsl" << std::endl;
        return false;
    }

    kaleidoscopeShader_ = std::make_unique<Shader>();
    if (!kaleidoscopeShader_->loadFromSource(kQuadVertexShader, fragmentSource)) {
        std::cerr << "ModularLayer: failed to compile kaleidoscope shader" << std::endl;
        kaleidoscopeShader_.reset();
        return false;
    }
    return true;
}

void ModularLayer::render(const LayerContext& context) {
    if (!initialized_ || (!enabled_ && !debugPreview_)) {
        return;
    }

    Shader* activeShader = nullptr;
    if (mode_ == kKaleidoscopeModeIndex) {
        if (!ensureKaleidoscopeShader()) {
            return;
        }
        activeShader = kaleidoscopeShader_.get();
    } else {
        if (!ensureShader()) {
            return;
        }
        activeShader = proceduralShader_.get();
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

    activeShader->use();
    activeShader->setUniform2f("uResolution", static_cast<float>(width_), static_cast<float>(height_));
    activeShader->setUniform1f("uTime", context.time);
    activeShader->setUniform1f("uTempo", context.tempo);

    const auto* audio = context.audio;
    float energy = audio ? audio->energy : 0.0f;
    float bass = audio ? audio->bassEnergy : 0.0f;
    float mid = audio ? audio->midEnergy : 0.0f;
    float high = audio ? audio->highEnergy : 0.0f;

    activeShader->setUniform1f("uEnergy", energy);
    activeShader->setUniform1f("uBass", bass);
    activeShader->setUniform1f("uMid", mid);
    activeShader->setUniform1f("uHigh", high);
    activeShader->setUniform1f("uIntensity", context.intensity);
    activeShader->setUniform3f("uPrimaryColor", colorPrimary_[0], colorPrimary_[1], colorPrimary_[2]);
    activeShader->setUniform3f("uSecondaryColor", colorSecondary_[0], colorSecondary_[1], colorSecondary_[2]);
    activeShader->setUniform1f("uColorBlend", colorBlend_);

    if (activeShader == proceduralShader_.get()) {
        activeShader->setUniform1i("uMode", mode_);
    }

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
