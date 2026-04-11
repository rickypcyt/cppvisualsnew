#include "visualizer.h"
#include "imgui.h"
#include "audio_capture.h"
#include "shader_loader.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <set>
#include <vector>

namespace {
constexpr int kPostProcessModeCount = 22;
constexpr int kKaleidoscopeModeIndex = 29;
constexpr int kPostProcessKaleidoscopeModeIndex = 7;
constexpr int kPostProcessGrayscaleModeIndex = 1;
constexpr int kDefaultPostProcessMode = 2;
constexpr float kDefaultPostProcessStrength = 0.65f;
constexpr int kKaleidoscopeSlotIndex = 1;
constexpr float kDefaultKaleidoscopeStrength = 0.75f;

std::array<float, 3> hsvToRgb(float h, float s, float v) {
    h = std::fmod(h, 1.0f);
    if (h < 0.0f) {
        h += 1.0f;
    }
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float c = v * s;
    float hh = h * 6.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hh, 2.0f) - 1.0f));

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    if (hh < 1.0f) {
        r = c;
        g = x;
    } else if (hh < 2.0f) {
        r = x;
        g = c;
    } else if (hh < 3.0f) {
        g = c;
        b = x;
    } else if (hh < 4.0f) {
        g = x;
        b = c;
    } else if (hh < 5.0f) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }

    float m = v - c;
    return {r + m, g + m, b + m};
}
} // namespace

const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char *doodadVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

out vec2 vUV;

void main() {
    vUV = (aPos + 1.0) * 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char *doodadFragmentShaderSource = R"(
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
uniform vec3 uPrimaryColor;
uniform vec3 uSecondaryColor;
uniform float uColorBlend;

#define PI 3.14159265359
#define iTime uTime
#define iResolution vec3(uResolution, 1.0)

const float BUMP_AMP = 6.0;
const float HUE_MIX = 0.7;
const float BPM = 160.0;
const int MAX_STEPS = 100;
const float EPS = 0.01;
const float FAR_DIST = 1e5;

vec3 hueRotate(vec3 col, float h) {
    const vec3 k = vec3(0.57735);
    float a = h * 6.28318;
    float c = cos(a);
    float s = sin(a);
    return col * c + cross(k, col) * s + k * dot(k, col) * (1.0 - c);
}

vec3 erot(vec3 p, vec3 ax, float ro) {
    return mix(dot(p, ax) * ax, p, cos(ro)) + sin(ro) * cross(ax, p);
}

float smin(float a, float b, float k) {
    float h = max(0.0, k - abs(b - a)) / k;
    return min(a, b) - h * h * h * k / 6.0;
}

vec4 wrot(vec4 p) {
    return vec4(dot(p, vec4(1.0)), p.yzw + p.zwy - p.wyz - p.xxx) * 0.5;
}

float t;
float doodad;
vec3 p2;

float doodadDist(vec3 p, float t_offset) {
    float d_time = t_offset + iTime;

    p2 = erot(p, vec3(0.0, 1.0, 0.0), d_time);
    p2 = erot(p2, vec3(0.0, 0.0, 1.0), d_time / 3.0);
    p2 = erot(p2, vec3(1.0, 0.0, 0.0), d_time / 5.0);

    float morph_speed = 3.0;
    float bpt = d_time / 60.0 * BPM;
    vec4 p4 = vec4(p2, 0.0);
    p4 = mix(p4, wrot(p4), smoothstep(-0.55, 0.55, sin(bpt / 4.0)));
    p4 = abs(p4);
    p4 = mix(p4, wrot(p4), smoothstep(-0.5, 0.5, sin(bpt / morph_speed)));

    float fctr = smoothstep(-0.5, 0.5, sin(bpt / 4.0));

    float num_faciness = mix(0.09, 0.3, fctr);
    float roundness = mix(-0.1, 0.22, fctr);
    float scale = mix(0.15, 0.45, fctr * fctr);
    float shapeniess = 0.0;

    doodad = length(max(abs(p4) - num_faciness, shapeniess) + roundness) - scale;

    p.x += asin(sin(d_time / 80.0) * 0.99) * 80.0;

    return doodad;
}

float scene(vec3 p) {
    float d = 1e5;
    const int numel = 13;
    float period = 45.0;
    float startX = -6.0 * float(numel);
    float endX = 10.0;
    float maxRadius = 12.0;
    float minRadius = 0.01;

    for (int i = 0; i < numel; ++i) {
        float fi = float(i);
        float phaseOffset = period / float(numel);
        float travel = fract((iTime + fi * phaseOffset) / period);

        float xPos = mix(startX, endX, travel);
        float angle = travel * 4.0 * PI + fi;
        float radius = mix(maxRadius, minRadius, travel);
        vec3 offset = vec3(xPos, cos(angle) * radius, sin(angle) * radius);

        float dtemp = doodadDist(p + offset, fi * 4.5);
        float fade = smoothstep(0.8, 1.0, travel);
        dtemp = mix(dtemp, 1e5, fade);
        d = smin(d, dtemp, 0.1);
    }

    return d;
}

vec3 norm(vec3 p) {
    float precis = length(p) < 1.0 ? 0.005 : 0.01;
    mat3 k = mat3(p, p, p) - mat3(precis);
    return normalize(scene(p) - vec3(scene(k[0]), scene(k[1]), scene(k[2])));
}

void main() {
    vec2 fragCoord = vUV * uResolution;
    vec2 uv = (fragCoord - 0.5 * uResolution) / uResolution.y;

    float bpt = iTime / 60.0 * BPM;
    float bp = mix(pow(sin(fract(bpt) * PI * 0.5), 20.0) + floor(bpt), bpt, 0.4);
    t = bp;

    vec3 cam = normalize(vec3(2.2, uv));
    vec3 init = vec3(BUMP_AMP * sin(0.5 * bp * PI), 0.0, 0.0);

    vec3 p = init;
    bool hit = false;
    float atten = 1.4 + clamp(uEnergy * 0.35 + uTempo * 0.25, 0.0, 1.5);
    float tlen = 0.0;
    float dist;

    for (int i = 0; i < MAX_STEPS; ++i) {
        dist = scene(p);
        if (dist < EPS) {
            hit = true;
            break;
        }
        if (tlen > FAR_DIST) {
            break;
        }
        p += cam * dist;
        tlen += dist;
    }

    if (!hit) {
        vec3 paletteColor = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
        FragColor = vec4(paletteColor * 0.08, 0.0);
        return;
    }

    vec3 n = norm(p);
    vec3 r = reflect(cam, n);

    float fact = length(sin(r * 4.0) * 0.5 + 0.5) / sqrt(3.0) * 0.7 + 0.3;
    fact += pow(fact, 15.0);
    float levels = 3.0;
    fact = floor(fact * levels) / levels;

    vec3 matcol = mix(vec3(0.0, 0.8, 0.8), vec3(0.8, 0.0, 0.8), HUE_MIX);
    float hueDrift = bpt / 4.0 + clamp(uTempo * 0.3 + uHigh * 0.25, 0.0, 1.0);
    matcol = hueRotate(matcol, hueDrift);

    vec3 col = matcol * fact;
    col *= atten;
    col = sqrt(max(col, 0.0));
    col = smoothstep(vec3(0.0), vec3(1.2), col);

    vec3 paletteColor = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    float mixAmount = clamp(0.45 + uEnergy * 0.35 + uTempo * 0.2, 0.0, 1.0);
    vec3 finalColor = mix(paletteColor, col, mixAmount);
    float alpha = clamp(0.35 + length(finalColor) * 0.25, 0.0, 1.0);

    FragColor = vec4(finalColor, alpha);
}
)";
const char *cornerVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aCornerPos;
layout (location = 1) in vec4 aParams; // size, orbitRadius, orbitPhase, profile

uniform vec2 uResolution;
uniform float uTime;
uniform float uTempo;
uniform float uPulse;
uniform float uEnergy;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uOnset;
uniform float uBeat;
uniform float uKick;
uniform vec3 uScenePrimary;
uniform vec3 uSceneSecondary;
uniform float uSceneBlend;

out float vSize;
out float vAngle;
out float vActivation;
out float vProfile;
out vec3 vPalettePrimary;
out vec3 vPaletteSecondary;
out float vPaletteBlend;

void main() {
    float baseScale = min(uResolution.x, uResolution.y);
    float profile = aParams.w;

    float spectralDrive = uEnergy * 0.35 + uBass * 0.28 + uMid * 0.24 + uHigh * 0.2;
    float rhythmDrive = uOnset * 0.45 + uBeat * 0.55 + uPulse * 0.3 + uKick * 0.25;
    float blended = spectralDrive * (0.65 + profile * 0.15) + rhythmDrive * (0.45 + profile * 0.1);
    float activation = smoothstep(0.08, 0.95 + profile * 0.1, clamp(blended, 0.0, 1.6));
    activation = mix(activation, pow(activation, 0.7), 0.6);

    vec2 basePos = aCornerPos;
    vec2 drift = vec2(sin(uTime * 0.55 + basePos.x * 2.8),
                      cos(uTime * 0.5 + basePos.y * 2.4)) * (0.012 + 0.01 * activation);

    float orbitRadius = aParams.y * (0.25 + activation * 1.1);
    float orbitSpeed = 0.35 + uTempo * 0.25 + profile * 0.2;
    float orbitPhase = aParams.z + uTime * orbitSpeed + uBeat * (0.2 + profile * 0.15);
    vec2 orbit = vec2(cos(orbitPhase), sin(orbitPhase)) * orbitRadius;

    vec2 pos = basePos + drift + orbit;

    float baseSize = baseScale * aParams.x;
    float eased = mix(activation, smoothstep(0.0, 1.0, activation), 0.7);
    float size = baseSize * (0.72 + eased * (0.9 + profile * 0.45)) + baseScale * (0.02f + profile * 0.01f);

    gl_Position = vec4(pos, 0.0, 1.0);
    gl_PointSize = size;

    vSize = size;
    vAngle = orbitPhase;
    vActivation = eased;
    vProfile = profile;
    vPalettePrimary = uScenePrimary;
    vPaletteSecondary = uSceneSecondary;
    vPaletteBlend = uSceneBlend;
}
)";

const char *cornerFragmentShaderSource = R"(
#version 330 core
#define time uTime*1.25

in float vSize;
in float vAngle;
in float vActivation;
in float vProfile;
in vec3 vPalettePrimary;
in vec3 vPaletteSecondary;
in float vPaletteBlend;

out vec4 FragColor;

uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;
uniform float uPulse;
uniform float uKick;
uniform float uTime;
uniform float uTempo;
uniform vec3 uScenePrimary;
uniform vec3 uSceneSecondary;
uniform float uSceneBlend;

const float numParticles = 25.;
const float numRings = 5.;
const float offsetMult = 30.;
const float tau = 6.23813;

// Use scene palette instead of hardcoded colors
vec3 getSceneColor(float t)
{
    return mix(uScenePrimary, uSceneSecondary, clamp(t + uSceneBlend, 0.0, 1.0));
}

vec3 particleColor(vec2 uv, float radius, float offset, float periodOffset)
{
    // Use scene colors with variation based on offset
    vec3 color = getSceneColor(0.3 + offset * 0.4);
    uv /= pow(periodOffset, .75) * sin(periodOffset * uTime) + sin(periodOffset + uTime);
    vec2 pos = vec2(cos(offset * offsetMult + time + periodOffset),
        		sin(offset * offsetMult + time * 5. + periodOffset * tau));
    
    float dist = radius / distance(uv, pos);
    return color * pow(dist, 2.) * 1.75;
} 

void main() {
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float dist = length(uv);
    if (dist > 1.0) discard;

    vec3 particleColorResult = vec3(0.);
    
    for (float n = 0.; n <= numRings; n++)
    {
        for (float i = 0.; i <= numParticles; i++) {
        	particleColorResult += particleColor(uv, .03, i / numParticles, n / 2.);
    	}
    }
    
    float activation = clamp(vActivation * 1.15, 0.0, 1.0);
    vec3 baseColor = mix(vPalettePrimary, vPaletteSecondary, clamp(vPaletteBlend, 0.0, 1.0)) * 1.1;
    vec3 glowColor = particleColorResult * mix(1.05, 1.35, activation);
    vec3 finalColor = mix(baseColor, glowColor, activation);
    
    float alpha = clamp((1.0 - pow(dist, 0.85)) * (0.85 + activation * 0.6), 0.0, 1.0);
    FragColor = vec4(finalColor, alpha);
}
)";

const char *fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uTexture;
uniform float uTime;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;

void main() {
    vec4 texColor = texture(uTexture, TexCoord);
    
    // Simple audio-reactive effect
    float pulse = uBass * 0.3 + uMid * 0.2 + uHigh * 0.1;
    vec3 color = texColor.rgb + vec3(pulse * 0.2, pulse * 0.1, pulse * 0.3);
    
    FragColor = vec4(color, texColor.a);
}
)";

const char *sparkVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in float aSize;
layout (location = 2) in float aBrightness;

out float vBrightness;

uniform vec2 uResolution;
uniform float uTime;

void main() {
    vBrightness = aBrightness;
    vec2 pos = aPos + vec2(sin(uTime + aPos.x) * 0.01, cos(uTime + aPos.y) * 0.01);
    gl_Position = vec4(pos * vec2(2.0 / uResolution.x, 2.0 / uResolution.y), 0.0, 1.0);
    gl_PointSize = aSize;
}
)";

const char *sparkFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in float vBrightness;

uniform float uTime;
uniform vec3 uColor;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) discard;
    
    float alpha = (1.0 - dist * 2.0) * vBrightness;
    FragColor = vec4(uColor, alpha);
}
)";

const char *coreVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aDir;
layout (location = 1) in float aRadial;
layout (location = 2) in float aAngle;

out float vAngle;
out float vRadial;
out float vWarp;
out float vFacetFlow;
out float vRadiusNorm;
out float vVoidMask;
out float vPetalMask;
out float vSpiral;
out float vInnerMask;
out float vOuterMask;
out float vNodePulse;
out float vBondMask;
out float vRibbonMask;
out vec2 vChemFlow;
out vec3 vPalettePrimary;
out vec3 vPaletteSecondary;
out float vPaletteBlend;

uniform vec2  uResolution;
uniform float uBaseRadius, uPulse, uKick, uHarmonic, uGrowth, uMaturity;
uniform float uEnergy, uBass, uMid, uHigh;
uniform float uTime, uTempo, uBeat;
uniform float uIdlePulse, uIdleWarp, uIdleSpin, uOnset;
uniform vec3 uScenePrimary;
uniform vec3 uSceneSecondary;
uniform float uSceneBlend;

void main() {
    float angle = aAngle + uIdleSpin;
    vec2  dir     = vec2(cos(angle), sin(angle));
    vec2  tangent = vec2(-dir.y, dir.x);

    // --- Warp radial ---
    float warp = sin(angle * (4.5 + uHarmonic * 1.9) + uTime * (1.7 + uTempo * 0.5))
                 * (0.16 + uHarmonic * 0.22);
    warp += uIdleWarp * (0.45 + 0.35 * aRadial);

    // --- Radio base ---
    float radius = uBaseRadius * (0.85 + aRadial * 0.7 + uPulse * 0.35 + uIdlePulse * 0.5)
                   * (1.0 + warp);
    radius += (uBass * 65.0 + uMid * 42.0 + uHigh * 30.0 + uIdlePulse * 24.0) * aRadial;

    // --- Pétalo ---
    float petalWave = cos(angle * (6.0 + uHarmonic * 4.0) + uTime * (1.2 + uTempo * 0.35));
    float petalMask = smoothstep(-0.25, 0.65, petalWave + uPulse * 0.25 + uIdlePulse * 0.3);
    radius += petalMask * (24.0 + uPulse * 40.0 + uIdlePulse * 28.0);

    // --- Facetas (tri + hex combinados) ---
    float facet = cos(angle * 3.0) * 0.68 + cos(angle * 6.0 + uTime * 0.6) * 0.32;
    radius *= (1.0 + facet * (0.26 + uHarmonic * 0.2) * (0.45 + aRadial * 0.55));

    // --- Flujo orbital ---
    float spiral = sin(angle * 2.0 + uTime * 0.9);
    vec2 pos = dir * radius
             + tangent * spiral * (18.0 + uHigh * 20.0)
             + dir     * (uGrowth * 12.0 + uIdlePulse * 18.0);

    // --- Void mask ---
    float voidWave = sin(angle * 3.0 + uTime * 2.2) + sin(angle * 1.35 + uTime * 0.9);
    float voidMask = smoothstep(0.25, 1.1, abs(voidWave) - (0.2 + uPulse * 0.2));

    float radiusNorm = clamp(length(pos) / (uBaseRadius * (3.4 + uPulse * 0.9 + uGrowth * 0.45)), 0.0, 1.9);
    float innerMask = smoothstep(0.08, 0.75, radiusNorm);
    float outerMask = smoothstep(0.25, 1.25, radiusNorm + warp * 0.15 + uGrowth * 0.1);

    float nodePulse = sin(angle * (5.0 + uHarmonic * 2.1) + uTime * (2.0 + uTempo * 0.6));
    nodePulse = pow(clamp(nodePulse * 0.5 + 0.5, 0.0, 1.0), 1.4);

    float bondMask = smoothstep(0.1, 0.9, abs(sin(angle * 3.0 + uTime * 1.7)) * (0.7 + uMid * 0.5));
    float ribbonWave = sin(angle * (3.5 + uTempo * 0.6) + uTime * (1.8 + uPulse * 0.5));
    float ribbonMask = (ribbonWave * 0.5 + 0.5) * smoothstep(0.2, 1.1, radiusNorm);

    vec2 chemFlow = vec2(cos(angle * 1.1 + uTime * 0.6), sin(angle * 0.9 - uTime * 0.4));
    chemFlow *= 0.3 + radiusNorm * 0.7;
    chemFlow += tangent * (0.18 + uHigh * 0.25);
    chemFlow += dir * (0.12 + uBass * 0.2);

    // --- Outputs ---
    vAngle      = angle;
    vRadial     = aRadial;
    vWarp       = warp;
    vFacetFlow  = clamp(facet * (0.7 + uPulse * 0.4), -1.6, 1.6);
    vRadiusNorm = radiusNorm;
    vVoidMask   = clamp(voidMask, 0.0, 1.0);
    vPetalMask  = petalMask;
    vSpiral     = spiral;
    vInnerMask  = clamp(innerMask, 0.0, 1.0);
    vOuterMask  = clamp(outerMask, 0.0, 1.0);
    vNodePulse  = clamp(nodePulse, 0.0, 1.0);
    vBondMask   = clamp(bondMask, 0.0, 1.5);
    vRibbonMask = clamp(ribbonMask, 0.0, 1.5);
    vChemFlow   = chemFlow;
    vPalettePrimary = uScenePrimary;
    vPaletteSecondary = uSceneSecondary;
    vPaletteBlend = uSceneBlend;

    gl_Position = vec4(pos * vec2(2.0 / uResolution.x, 2.0 / uResolution.y), 0.0, 1.0);
}
)";

const char *coreFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in float vAngle;
in float vRadial;
in float vWarp;
in float vPetalMask;
in float vSpiral;
in float vInnerMask;
in float vOuterMask;
in float vVoidMask;
in float vRadiusNorm;
in float vNodePulse;
in float vBondMask;
in float vFacetFlow;
in float vRibbonMask;
in vec2 vChemFlow;
in vec3 vPalettePrimary;
in vec3 vPaletteSecondary;
in float vPaletteBlend;

uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;
uniform float uPulse;
uniform float uKick;
uniform float uHarmonic;
uniform float uGrowth;
uniform float uMaturity;
uniform float uTime;
uniform float uTempo;
uniform float uOnset;
uniform float uBeat;
uniform float uShowSpokes;
uniform float uShowRunes;
uniform float uShowSparkles;
uniform float uShowBloom;

float hash(float n) {
    return fract(sin(n) * 43758.5453);
}

void main() {
    float innerLayer = vInnerMask;
    float outerLayer = vOuterMask;
    float petals = smoothstep(0.0, 1.0, vPetalMask);
    float spiral = vSpiral * (0.6 + uHarmonic * 0.4);

    float runeWave = sin(vAngle * (8.0 + uHarmonic * 3.1) + uTime * (3.6 + uTempo * 0.7));
    float runeMask = smoothstep(-0.3, 0.6, runeWave + outerLayer * 0.4);

    float sparkSeed = hash(floor((vAngle + uTime) * 7.5));
    float sparkPulse = sin(vAngle * (16.0 + uHigh * 5.5) + uTime * (5.4 + uTempo * 1.3));
    float spark = pow(max(0.0, sparkPulse), 2.2) * (0.22 + uHigh * 0.6) * outerLayer * sparkSeed;

    float voidMask = vVoidMask;
    float bloom = (0.35 + uPulse * 0.45) * outerLayer * (0.6 + petals * 0.4) * (1.0 - voidMask);
    float chemStir = clamp(length(vChemFlow) * 1.2, 0.0, 3.5);
    float nodePulse = clamp(vNodePulse * (0.45 + uOnset * 0.6 + uBeat * 0.5), 0.0, 2.8);
    float bondCore = clamp(vBondMask * (0.5 + uPulse * 0.3 + uBeat * 0.4), 0.0, 2.0);
    float facetMagnitude = clamp(abs(vFacetFlow) * (1.35 + uPulse * 0.45 + uBeat * 0.35), 0.0, 2.4);
    float facetPrism = smoothstep(0.25, 1.15, facetMagnitude);
    float facetShard = smoothstep(0.55, 1.35, facetMagnitude * (0.9 + uHigh * 0.45));
    float ribbonFuse = smoothstep(0.25, 0.92, clamp(vRibbonMask, 0.0, 1.0));
    float ribbonStrand = ribbonFuse * smoothstep(0.2, 0.95, sin(vAngle * (5.0 + uTempo * 0.6) + uTime * (3.6 + uPulse * 0.9)) * 0.5 + 0.5);
    float ribbonSpark = ribbonFuse * pow(clamp(vRibbonMask, 0.0, 1.0), 2.2) * (0.55 + uPulse * 0.45 + uBeat * 0.6);

    float timeShift = uTime * (0.15 + uTempo * 0.1) + uOnset * 1.2;
    float hueBase = fract(sin(timeShift * 0.37) * 43758.5453);
    float hueAccent = fract(sin((timeShift + 2.17) * 0.53) * 32768.2143);
    float hueSpark = fract(sin((timeShift - 1.91) * 0.43) * 54731.1243);

    vec3 basePalette[4];
    basePalette[0] = vPalettePrimary;
    basePalette[1] = vPaletteSecondary;
    basePalette[2] = mix(vPalettePrimary, vPaletteSecondary, 0.5);
    basePalette[3] = mix(vPalettePrimary, vPaletteSecondary, 0.8);

    float paletteBlend = clamp(hueBase * 3.0, 0.0, 3.0);
    int paletteIndex = int(paletteBlend);
    int nextIndex = (paletteIndex + 1) % 4;
    float paletteMix = fract(paletteBlend);
    vec3 baseHue = mix(basePalette[paletteIndex], basePalette[nextIndex], paletteMix);

    vec3 accentHue = mix(baseHue.yzx, baseHue.zxy, 0.35 + hueAccent * 0.4);
    vec3 sparkHue = mix(vec3(1.0, 0.9, 0.6), baseHue, 0.2 + hueSpark * 0.5);

    vec3 coreColor = mix(baseHue, basePalette[nextIndex], 0.4 + uEnergy * 0.2);
    coreColor += mix(vec3(0.0), vPaletteSecondary, clamp(uBass * 0.6, 0.0, 1.0));
    coreColor += mix(vec3(0.0), vPalettePrimary.zyx, clamp(uHigh * 0.5, 0.0, 1.0));

    vec3 petalColor = mix(accentHue, vPaletteSecondary, 0.25 + uHigh * 0.35);
    vec3 runeColor = mix(accentHue.yzx, vPalettePrimary, 0.35 + uMid * 0.3);
    vec3 sparkColor = mix(sparkHue, vPaletteSecondary, 0.4 + uHigh * 0.3);
    vec3 chemColor = mix(baseHue.xzy, vPaletteSecondary.zyx, 0.45 + uEnergy * 0.35);
    vec3 bondColor = mix(accentHue.zxy, vPalettePrimary, 0.4 + uMid * 0.35);
    vec3 ribbonColor = mix(baseHue, vPaletteSecondary, 0.55 + uHigh * 0.25);
    vec3 facetColor = mix(baseHue.zxy, vPalettePrimary.zyx, 0.45 + uEnergy * 0.3);
    vec3 shardColor = mix(accentHue, vPaletteSecondary.yzx, 0.5 + uPulse * 0.3);

    vec3 color = vec3(0.0);

    float baseIntensity = innerLayer * (0.35 + uEnergy * 0.25);
    float filamentCount = 18.0 + uHarmonic * 6.0 + uTempo * 2.0 + uGrowth * 1.5;
    float filamentPhase = uTime * (0.7 + uTempo * 0.5) + vRadiusNorm * 1.6;
    float filamentWave = sin(vAngle * filamentCount + filamentPhase);
    float filamentCore = 1.0 - smoothstep(0.0, 0.35, abs(filamentWave));
    float filamentCross = sin((vAngle + vRadiusNorm * 2.2) * (filamentCount * 0.45) - uTime * (1.4 + uHigh * 0.7));
    float filamentNoise = smoothstep(0.1, 0.8, filamentCross * 0.5 + 0.5);
    float filamentMask = pow(clamp(filamentCore * filamentNoise, 0.0, 1.0), 2.2);
    filamentMask *= innerLayer;
    filamentMask *= smoothstep(0.0, 0.65, vRadiusNorm) * (1.0 - smoothstep(0.9, 1.6, vRadiusNorm));
    filamentMask *= 1.0 + uGrowth * 0.25;

    vec3 filamentColor = mix(coreColor * 1.25,
                             vec3(0.9 + uHigh * 0.7,
                                  0.5 + uMid * 0.6,
                                  1.2 + uBass * 0.5),
                             clamp(vRadiusNorm, 0.0, 1.0));

    float growthSpiral = sin(vAngle * (4.0 + uGrowth * 2.2) + uTime * (1.6 + uGrowth * 0.35));
    float growthShell = smoothstep(0.18, 0.88, vRadiusNorm + growthSpiral * 0.1);
    float growthPulse = smoothstep(0.25, 0.9, sin(vAngle * (2.2 + uGrowth) + uTime * (0.85 + uTempo * 0.35)) * 0.5 + 0.5);
    vec3 growthColor = mix(vec3(0.22 + uBass * 0.55, 0.42 + uMid * 0.55, 0.85 + uHigh * 0.35),
                           vec3(0.78, 0.46 + uMid * 0.35, 0.38 + uBass * 0.3),
                           clamp(uGrowth * 0.3, 0.0, 1.0));
    growthColor = mix(growthColor, vec3(0.0), 0.1);

    float maturityVeins = smoothstep(0.3, 0.92, sin(vAngle * (7.0 + uMaturity * 1.4) - uTime * (1.5 + uTempo * 0.4)) * 0.5 + 0.5);
    float maturityHalo = smoothstep(0.45, 1.15, vRadiusNorm + sin(vAngle * 3.2 + uTime * 0.6) * 0.08);
    vec3 maturityColor = vec3(1.05 + uHigh * 0.4, 0.84 + uMid * 0.35, 0.65 + uBass * 0.28);

    color += runeColor * runeMask * (0.18 + uHarmonic * 0.35) * uShowRunes;
    color += sparkColor * spark * uShowSparkles;
    color += petalColor * bloom * uShowBloom;
    color += maturityColor * maturityVeins * (0.18 + uMaturity * 0.25) * uShowRunes;
    color += shardColor * facetShard * (0.26 + uHigh * 0.35 + uPulse * 0.25) * uShowSparkles;

    float chemAura = smoothstep(0.2, 1.4, chemStir + nodePulse * 0.5);
    float arterial = smoothstep(0.3, 0.95, vRadiusNorm + sin(vAngle * 5.0 + uTime * 0.8) * 0.1);
    float bondVeins = smoothstep(0.15, 0.85, bondCore * outerLayer);
    vec3 phaseColor = mix(chemColor, chemColor.zyx, clamp(uHigh * 0.4 + uOnset * 0.3, 0.0, 1.0));
    vec3 bondGlow = mix(bondColor, bondColor.yzx, clamp(uMid * 0.5, 0.0, 1.0));

    color += phaseColor * chemAura * (0.3 + 0.42 + uEnergy * 0.22);
    color += bondGlow * bondVeins * (0.35 + nodePulse * 0.3) * uShowRunes;
    color += ribbonColor * ribbonFuse * 0.22 * uShowBloom;
    color += facetColor * facetPrism * 0.22 * uShowRunes;

    float spokeMask = smoothstep(0.6, 1.05, vRadiusNorm + sin(vAngle * 12.0) * 0.08);
    color += runeColor * spokeMask * (0.25 + uHigh * 0.35) * uShowSpokes;
    color += maturityColor * maturityHalo * (0.15 + uMaturity * 0.18) * uShowSpokes;

    color = max(color, vec3(0.0));

    float alpha = 0.1
                + spokeMask * 0.25 * uShowSpokes
                + maturityVeins * 0.22 * uShowRunes
                + spark * 0.42 * uShowSparkles
                + bloom * 0.32 * uShowBloom
                + chemAura * 0.18
                + bondVeins * 0.26 * uShowRunes
                + ribbonStrand * 0.18
                + ribbonSpark * 0.18 * uShowSparkles
                + facetPrism * 0.22;
    alpha = clamp(alpha, 0.08, 0.9);

    FragColor = vec4(max(color, vec3(0.0)), alpha);
}
)";

void Visualizer::renderCornerOrbs() {
    if (!cornerShader_ || cornerVAO_ == 0) {
        std::cerr << "Corner orbs cannot render: shader=" << (cornerShader_ ? "valid" : "null") 
                  << " VAO=" << cornerVAO_ << std::endl;
        return;
    }

    static int debugCounter = 0;
    if (debugCounter++ % 300 == 0) { // Print every 5 seconds at 60fps
        std::cout << "Rendering corner orbs - showCornerOrbs_: " << showCornerOrbs_ << std::endl;
    }

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }

    // Save current color mask and restore full RGB for corner orbs
    GLboolean previousMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, previousMask);
    
    // Debug: Show RGB channel state (reuse existing debugCounter)
    if (debugCounter++ % 300 == 0) { // Print every 5 seconds at 60fps
        std::cout << "RGB Channels enabled: R=" << rgbChannelEnabled_[0] 
                  << " G=" << rgbChannelEnabled_[1] 
                  << " B=" << rgbChannelEnabled_[2] << std::endl;
        std::cout << "Current colors: Primary={" << scenePrimaryColor_[0] << "," << scenePrimaryColor_[1] << "," << scenePrimaryColor_[2] 
                  << "} Secondary={" << sceneSecondaryColor_[0] << "," << sceneSecondaryColor_[1] << "," << sceneSecondaryColor_[2] << "}" << std::endl;
    }
    
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    GLboolean programPointEnabled = glIsEnabled(GL_PROGRAM_POINT_SIZE);
    if (!programPointEnabled) {
        glEnable(GL_PROGRAM_POINT_SIZE);
    }

    GLboolean pointSpriteEnabled = glIsEnabled(GL_POINT_SPRITE);
    if (!pointSpriteEnabled) {
        glEnable(GL_POINT_SPRITE);
    }

    cornerShader_->use();
    cornerShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_),
                                static_cast<float>(windowHeight_));
    cornerShader_->setUniform1f("uTime", time_);
    cornerShader_->setUniform1f("uTempo", tempoMultiplier_);
    cornerShader_->setUniform1f("uPulse", core_.pulse);
    cornerShader_->setUniform1f("uEnergy", audioFeatures_.energy);
    cornerShader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    cornerShader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    cornerShader_->setUniform1f("uHigh", audioFeatures_.highEnergy);
    cornerShader_->setUniform1f("uKick", core_.kickEnvelope);
    cornerShader_->setUniform1f("uOnset", audioFeatures_.onset);
    cornerShader_->setUniform1f("uBeat", audioFeatures_.beat);
    cornerShader_->setUniform3f("uScenePrimary", scenePrimaryColor_[0], scenePrimaryColor_[1],
                                scenePrimaryColor_[2]);
    cornerShader_->setUniform3f("uSceneSecondary", sceneSecondaryColor_[0], sceneSecondaryColor_[1],
                                sceneSecondaryColor_[2]);
    cornerShader_->setUniform1f("uSceneBlend", scenePaletteBlend_);

    glBindVertexArray(cornerVAO_);
    glDrawArrays(GL_POINTS, 0, cornerVertexCount_);
    glBindVertexArray(0);

    glUseProgram(0);

    // Restore original color mask
    glColorMask(previousMask[0], previousMask[1], previousMask[2], previousMask[3]);

    if (!pointSpriteEnabled) {
        glDisable(GL_POINT_SPRITE);
    }
    if (!programPointEnabled) {
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void Visualizer::renderCore() {
    if (!coreShader_ || coreVAO_ == 0) {
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
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    coreShader_->use();
    coreShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_),
                              static_cast<float>(windowHeight_));
    coreShader_->setUniform1f("uBaseRadius", core_.baseRadius);
    coreShader_->setUniform1f("uPulse", core_.pulse);
    coreShader_->setUniform1f("uKick", core_.kickEnvelope);
    coreShader_->setUniform1f("uHarmonic", core_.harmonicEnvelope);
    coreShader_->setUniform1f("uGrowth", core_.growthEnvelope);
    coreShader_->setUniform1f("uMaturity", core_.maturity);
    coreShader_->setUniform1f("uEnergy", core_.energy);
    coreShader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    coreShader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    coreShader_->setUniform1f("uHigh", audioFeatures_.highEnergy);
    coreShader_->setUniform1f("uTime", time_);
    coreShader_->setUniform1f("uTempo", tempoMultiplier_);
    coreShader_->setUniform1f("uOnset", audioFeatures_.onset);
    coreShader_->setUniform1f("uBeat", audioFeatures_.beat);
    coreShader_->setUniform1f("uIdlePulse", core_.idlePulse);
    coreShader_->setUniform1f("uIdleWarp", core_.idleWarp);
    coreShader_->setUniform1f("uIdleSpin", core_.idleSpin);
    coreShader_->setUniform3f("uScenePrimary", scenePrimaryColor_[0], scenePrimaryColor_[1],
                              scenePrimaryColor_[2]);
    coreShader_->setUniform3f("uSceneSecondary", sceneSecondaryColor_[0], sceneSecondaryColor_[1],
                              sceneSecondaryColor_[2]);
    coreShader_->setUniform1f("uSceneBlend", scenePaletteBlend_);
    coreShader_->setUniform1f("uShowSpokes", coreShowSpokes_);
    coreShader_->setUniform1f("uShowRunes", coreShowRunes_);
    coreShader_->setUniform1f("uShowSparkles", coreShowSparkles_);
    coreShader_->setUniform1f("uShowBloom", coreShowBloom_);

    glBindVertexArray(coreVAO_);
    glDrawArrays(GL_TRIANGLES, 0, coreVertexCount_);
    glBindVertexArray(0);

    glUseProgram(0);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

Visualizer::Visualizer()
    : window_(nullptr), imguiWindow_(nullptr), windowWidth_(800), windowHeight_(600), time_(0.0f), quadVAO_(0),
      quadVBO_(0), waveformVAO_(0), waveformVBO_(0), coreVAO_(0), coreVBO_(0), coreVertexCount_(0),
      sparkVAO_(0), sparkVBO_(0), sparkVertexCount_(0), cornerVAO_(0), cornerVBO_(0),
      cornerVertexCount_(0), audioFeatures_{}, selectedDevice_(-1), showDeviceMenu_(false),
      showDiagnostic_(false), consoleMode_(false), showImGuiWindow_(true),
      showDeviceSelector_(false), showDiagnosticInfo_(false), showConsoleMode_(false),
      showImGuiVisualWindow_(true), imguiInitialized_(false), autoRandomizeColors_(true),
      colorRandomInterval_(12.0f), colorRandomTimer_(0.0f), deltaTime_(0.0f),
      rng_(std::random_device{}()), onsetColorCyclingEnabled_(true), onsetTriggerCount_(0),
      lastOnsetActive_(false), tempoMultiplier_(1.0f), scenePalettes_(),
      currentScenePaletteIndex_(-1), scenePaletteHueSeed_(0.0f),
      scenePrimaryColor_{0.25f, 0.32f, 0.58f}, sceneSecondaryColor_{0.35f, 0.65f, 0.92f},
      scenePaletteBlend_(0.6f), rgbChannelEnabled_{true, true, true},
      globalIntensityEnvelope_(0.0f), autoRandomizeRgbChannels_(false), rgbRandomTimer_(0.0f), 
      rgbRandomInterval_(8.0f), lastRgbRandomTime_(0.0f), lastOnsetCount_(0), coreShowSpokes_(true), coreShowRunes_(true),
      coreShowSparkles_(true), coreShowBloom_(true), showCornerOrbs_(true),
      showProceduralLayer_(true), proceduralLayerDebug_(false), proceduralLayerOpacity_(0.85f),
      showPostProcess_(true),
      proceduralLayerMode_(1), audioInputGain_(1.0f), visualSensitivity_(1.0f),
      settingsManager_(std::make_unique<SettingsManager>()),
    midiController_(std::make_unique<MidiController>()),
    midiEnabled_(false) {
    waveformBuffer_.resize(512);
    colorRandomTimer_ = 0.0f;
    buildScenePalettes();

    std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
    scenePaletteHueSeed_ = hueDist(rng_);
    setDefaultScenePalette();
    setupDeviceList();
    core_ = {};
    idleState_ = 0.0f;
    idlePhase_ = 0.0f;
    
    // Note: initializeRandomPostProcess() is called later in initialize() after loading enabled states
    
    // Ensure Slot 1 is always enabled by default
    if (kMaxProceduralSlots > 0) {
        proceduralSlots_[0].enabled = true;
        if (proceduralSlots_[0].mode == 0) {
            proceduralSlots_[0].mode = proceduralLayerMode_; // Use current mode if slot mode is invalid
        }
        if (proceduralSlots_[0].opacity <= 0.0f) {
            proceduralSlots_[0].opacity = proceduralLayerOpacity_; // Use current opacity if invalid
        }
    }

    // Remove self-reference
    // scenePalettes_ = scenePalettes_;
}

Visualizer::~Visualizer() { 
    // Save current settings before shutdown
    saveCurrentSettings();
    shutdown(); 
}

bool Visualizer::initialize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;

    // Load settings first
    settingsManager_->loadSettings();
    
    // Apply loaded settings to visualizer state
    selectedDevice_ = settingsManager_->getSelectedDevice();
    audioInputGain_ = settingsManager_->getAudioInputGain();
    visualSensitivity_ = settingsManager_->getVisualSensitivity();
    showImGuiWindow_ = settingsManager_->getShowImGuiWindow();
    showCornerOrbs_ = settingsManager_->getShowCornerOrbs();
    showProceduralLayer_ = settingsManager_->getShowProceduralLayer();
    showCurrentEffects_ = settingsManager_->getShowCurrentEffects();
    proceduralLayerDebug_ = settingsManager_->getProceduralLayerDebug();
    proceduralLayerOpacity_ = settingsManager_->getProceduralLayerOpacity();
    proceduralLayerMode_ = settingsManager_->getProceduralLayerMode();
    
    // Apply post-process slots
    postProcessSlots_ = settingsManager_->getPostProcessSlots();
    showPostProcess_ = false;
    for (const auto &slot : postProcessSlots_) {
        if (slot.enabled && slot.mode > 0 && slot.strength > 0.0f) {
            showPostProcess_ = true;
            break;
        }
    }
    
    // Apply procedural slots
    proceduralSlots_ = settingsManager_->getProceduralSlots();
    
    // Apply random settings
    randomPostProcessEnabled_ = settingsManager_->getRandomPostProcessEnabled();
    randomPostProcessInterval_ = settingsManager_->getRandomPostProcessInterval();
    randomPostProcessSlotCount_ = settingsManager_->getRandomPostProcessSlotCount();
    randomProceduralEnabled_ = settingsManager_->getRandomProceduralEnabled();
    randomProceduralInterval_ = settingsManager_->getRandomProceduralInterval();
    
    // Apply hot-reload setting
    hotReloadEnabled_ = settingsManager_->getHotReloadEnabled();
    
    // Apply colors
    scenePrimaryColor_ = settingsManager_->getScenePrimaryColor();
    sceneSecondaryColor_ = settingsManager_->getSceneSecondaryColor();
    scenePaletteBlend_ = settingsManager_->getScenePaletteBlend();
    currentScenePaletteIndex_ = settingsManager_->getCurrentScenePaletteIndex();
    scenePaletteHueSeed_ = settingsManager_->getScenePaletteHueSeed();
    
    // Apply animation settings
    autoRandomizeColors_ = settingsManager_->getAutoRandomizeColors();
    colorRandomInterval_ = settingsManager_->getColorRandomInterval();
    onsetColorCyclingEnabled_ = settingsManager_->getOnsetColorCyclingEnabled();
    autoRandomizeRgbChannels_ = settingsManager_->getAutoRandomizeRgbChannels();
    manualBPMMode_ = settingsManager_->getManualBPMMode();
    manualBPM_ = settingsManager_->getManualBPM();
    
    // Apply RGB channels
    for (int i = 0; i < 3; ++i) {
        rgbChannelEnabled_[i] = settingsManager_->getRgbChannelEnabled(i);
    }
    
    // Apply per-shader enabled states
    proceduralShaderEnabled_ = settingsManager_->getAllProceduralShaderEnabled();
    
    // Apply per-post-processing-effect enabled states
    postProcessEffectEnabled_ = settingsManager_->getAllPostProcessEffectEnabled();
    
    // Initialize random post process AFTER loading enabled states
    initializeRandomPostProcess();

    // Initialize random procedural layer AFTER loading shader enabled states
    initializeRandomProcedural();

    if (!setupOpenGL()) {
        return false;
    }

    if (!setupGeometry()) {
        return false;
    }

    setupCoreMesh();
    setupSparkField();
    setupCornerQuad();

    if (!proceduralLayer_.initialize(windowWidth_, windowHeight_)) {
        std::cout << "Procedural layer initialization failed, disabling procedural overlay"
                  << std::endl;
        // showProceduralLayer_ = false;  // Commented out to preserve saved UI state
        proceduralLayerDebug_ = false;
    } else {
        applyMainProceduralMode(std::clamp(proceduralLayerMode_, 0, kProceduralModeCount - 1), "initialize");
    }
    
    // Initialize hot-reload if enabled
    if (hotReloadEnabled_) {
        proceduralLayer_.enableHotReload(true);
    }

    std::cout << "[DEBUG] Initializing post processor..." << std::endl;
    if (!postProcessor_.initialize(windowWidth_, windowHeight_)) {
        std::cout << "Post processor initialization failed" << std::endl;
    }
    std::cout << "[DEBUG] Post processor initialized" << std::endl;

    std::cout << "[DEBUG] Loading main shaders..." << std::endl;
    if (!loadShaders()) {
        std::cout << "Failed to load shaders, using fallback rendering" << std::endl;
        shader_.reset(); // Will trigger fallback triangle
    }
    std::cout << "[DEBUG] Main shaders loaded" << std::endl;
    useModernPipeline_ = shader_ != nullptr;

    std::cout << "[DEBUG] Loading core shader..." << std::endl;
    if (!loadCoreShader()) {
        coreShader_.reset();
    }
    std::cout << "[DEBUG] Core shader loaded" << std::endl;

    std::cout << "[DEBUG] Loading spark shader..." << std::endl;
    if (!loadSparkShader()) {
        sparkShader_.reset();
    }
    std::cout << "[DEBUG] Spark shader loaded" << std::endl;

    std::cout << "[DEBUG] Loading corner shader..." << std::endl;
    if (!loadCornerShader()) {
        cornerShader_.reset();
    }
    std::cout << "[DEBUG] Corner shader loaded" << std::endl;

    std::cout << "[DEBUG] Setting up ImGui..." << std::endl;
    if (setupImGui()) {
        imguiInitialized_ = true;
    } else {
        std::cout << "ImGui initialization failed, continuing without ImGui interface" << std::endl;
        showImGuiWindow_ = false;
        imguiInitialized_ = false;
    }
    std::cout << "[DEBUG] ImGui setup complete" << std::endl;

    // Set up scroll callback for mouse wheel zoom AFTER ImGui init
    // Only zoom when mouse is over the ImGui window, not the main visualizer window
    glfwSetScrollCallback(window_, [](GLFWwindow* window, double xoffset, double yoffset) {
        Visualizer* vis = static_cast<Visualizer*>(glfwGetWindowUserPointer(window));
        if (vis) {
            // Only process zoom if ImGui wants to capture mouse (meaning mouse is over ImGui window)
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantCaptureMouse) {
                vis->handleMouseScroll(xoffset, yoffset);
            }
        }
    });

    std::cout << "[DEBUG] Initializing MIDI controller..." << std::endl;
    initializeMIDI();
    std::cout << "[DEBUG] MIDI controller initialized" << std::endl;

    std::cout << "[DEBUG] Getting OpenGL info..." << std::endl;
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    rendererName_ = renderer ? renderer : "Unknown";

    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    openglVersion_ = version ? version : "Unknown";
    std::cout << "[DEBUG] OpenGL info: " << rendererName_ << " - " << openglVersion_ << std::endl;

    std::cout << "[DEBUG] Initializing dynamic systems..." << std::endl;
    initializeDynamicSystems();
    std::cout << "[DEBUG] Dynamic systems initialized" << std::endl;

    std::cout << "[DEBUG] Saving initial settings..." << std::endl;
    // Save initial settings
    saveCurrentSettings();
    std::cout << "[DEBUG] Initialization complete!" << std::endl;

    return true;
}

void Visualizer::buildScenePalettes() {
    scenePalettes_.clear();

    scenePalettes_.push_back({"Sesión Azul", {0.10f, 0.24f, 0.58f}, {0.28f, 0.70f, 0.98f}, 0.62f});

    scenePalettes_.push_back({"Sesión Roja", {0.58f, 0.12f, 0.16f}, {0.94f, 0.36f, 0.30f}, 0.48f});

    scenePalettes_.push_back({"Sesión Verde", {0.10f, 0.32f, 0.18f}, {0.30f, 0.82f, 0.52f}, 0.56f});

    if (scenePalettes_.empty()) {
        currentScenePaletteIndex_ = -1;
        setDefaultScenePalette();
        return;
    }

    if (currentScenePaletteIndex_ >= static_cast<int>(scenePalettes_.size())) {
        currentScenePaletteIndex_ = static_cast<int>(scenePalettes_.size()) - 1;
    }

    if (currentScenePaletteIndex_ < 0) {
        setDefaultScenePalette();
    }
}

void Visualizer::setUnifiedPalette(const std::array<float, 3> &primary,
                                   const std::array<float, 3> &secondary, float blend) {
    scenePrimaryColor_ = primary;
    sceneSecondaryColor_ = secondary;
    scenePaletteBlend_ = std::clamp(blend, 0.0f, 1.0f);

    (void)secondary;
    proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(),
                                     scenePaletteBlend_);
}

void Visualizer::applyScenePalette(int index) {
    if (index < 0 || scenePalettes_.empty()) {
        currentScenePaletteIndex_ = -1;
        setDefaultScenePalette();
        return;
    }

    int clamped = std::clamp(index, 0, static_cast<int>(scenePalettes_.size()) - 1);
    currentScenePaletteIndex_ = clamped;

    const ScenePalette &palette = scenePalettes_[clamped];
    setUnifiedPalette(palette.primary, palette.secondary, palette.blend);
}

void Visualizer::setDefaultScenePalette() {
    float baseHue = std::fmod(scenePaletteHueSeed_, 1.0f);
    float accentHue = std::fmod(baseHue + 0.27f, 1.0f);

    auto primaryRgb = hsvToRgb(baseHue, 0.78f, 0.95f);
    auto secondaryRgb = hsvToRgb(accentHue, 0.65f, 0.88f);

    currentScenePaletteIndex_ = -1;
    setUnifiedPalette(primaryRgb, secondaryRgb, scenePaletteBlend_);
}

void Visualizer::randomizeScenePalette() {
    if (scenePalettes_.empty() || currentScenePaletteIndex_ < 0) {
        std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
        scenePaletteHueSeed_ = hueDist(rng_);
        setDefaultScenePalette();
        return;
    }

    std::uniform_int_distribution<int> dist(0, static_cast<int>(scenePalettes_.size()) - 1);
    int nextIndex = dist(rng_);
    if (scenePalettes_.size() > 1) {
        int guard = 0;
        while (nextIndex == currentScenePaletteIndex_ && guard < 6) {
            nextIndex = dist(rng_);
            ++guard;
        }
    }
    applyScenePalette(nextIndex);
}

void Visualizer::cycleScenePaletteSequential() {
    if (scenePalettes_.empty()) {
        randomizeScenePalette();
        return;
    }

    if (currentScenePaletteIndex_ < 0) {
        applyScenePalette(0);
        return;
    }

    int lastIndex = static_cast<int>(scenePalettes_.size()) - 1;
    if (currentScenePaletteIndex_ >= lastIndex) {
        randomizeScenePalette();
        return;
    }

    applyScenePalette(currentScenePaletteIndex_ + 1);
}

void Visualizer::updateDynamicScenePalette() {
    if (currentScenePaletteIndex_ >= 0) {
        return;
    }

    float hueDrift = deltaTime_ * std::clamp(0.08f + audioFeatures_.energy * 0.12f, 0.05f, 0.4f);
    if (hueDrift <= 0.0f) {
        return;
    }

    scenePaletteHueSeed_ = std::fmod(scenePaletteHueSeed_ + hueDrift, 1.0f);
    setDefaultScenePalette();
}

void Visualizer::randomizeRgbChannels() {
    // Count currently enabled channels
    int enabledCount = 0;
    for (int i = 0; i < 3; ++i) {
        if (rgbChannelEnabled_[i]) {
            enabledCount++;
        }
    }
    
    // If all channels are currently enabled, turn off one random channel
    if (enabledCount == 3) {
        std::uniform_int_distribution<int> channelDist(0, 2);
        int channelToDisable = channelDist(rng_);
        rgbChannelEnabled_[channelToDisable] = false;
    }
    // If no channels are enabled, turn on exactly one
    else if (enabledCount == 0) {
        std::uniform_int_distribution<int> channelDist(0, 2);
        int channelToEnable = channelDist(rng_);
        rgbChannelEnabled_[channelToEnable] = true;
    }
    // If 1 or 2 channels are enabled, randomly toggle one channel
    else {
        std::uniform_int_distribution<int> channelDist(0, 2);
        int channelToToggle = channelDist(rng_);
        
        // If we're about to turn off the last enabled channel, turn on a different one instead
        if (enabledCount == 1 && rgbChannelEnabled_[channelToToggle]) {
            // Find a different channel to turn on
            int differentChannel = (channelToToggle + 1) % 3;
            while (differentChannel == channelToToggle) {
                differentChannel = (differentChannel + 1) % 3;
            }
            rgbChannelEnabled_[differentChannel] = true;
        } else {
            rgbChannelEnabled_[channelToToggle] = !rgbChannelEnabled_[channelToToggle];
        }
    }
    
    // Auto-save when RGB channels are randomized
    saveCurrentSettings();
}

void Visualizer::shutdown() {
    if (imguiInitialized_) {
        shutdownImGui();
        imguiInitialized_ = false;
        showImGuiWindow_ = false;
    }

    proceduralLayer_.shutdown();
    postProcessor_.shutdown();

    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    if (coreVAO_) {
        glDeleteVertexArrays(1, &coreVAO_);
        coreVAO_ = 0;
    }
    if (coreVBO_) {
        glDeleteBuffers(1, &coreVBO_);
        coreVBO_ = 0;
    }
    if (sparkVAO_) {
        glDeleteVertexArrays(1, &sparkVAO_);
        sparkVAO_ = 0;
    }
    if (sparkVBO_) {
        glDeleteBuffers(1, &sparkVBO_);
        sparkVBO_ = 0;
    }
    if (cornerVAO_) {
        glDeleteVertexArrays(1, &cornerVAO_);
        cornerVAO_ = 0;
    }
    if (cornerVBO_) {
        glDeleteBuffers(1, &cornerVBO_);
        cornerVBO_ = 0;
    }
    if (waveformVAO_) {
        glDeleteVertexArrays(1, &waveformVAO_);
        waveformVAO_ = 0;
    }
    if (waveformVBO_) {
        glDeleteBuffers(1, &waveformVBO_);
        waveformVBO_ = 0;
    }

    shader_.reset();
    coreShader_.reset();
    sparkShader_.reset();

    // Destroy ImGui window first (if it exists)
    if (imguiWindow_) {
        glfwDestroyWindow(imguiWindow_);
        imguiWindow_ = nullptr;
    }

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool Visualizer::shouldClose() {
    // Close if main visuals window is closed
    if (window_ && glfwWindowShouldClose(window_)) {
        return true;
    }
    // Also close if ImGui controls window is closed (if it exists)
    if (imguiWindow_ && glfwWindowShouldClose(imguiWindow_)) {
        return true;
    }
    return false;
}

void Visualizer::beginFrame() {
    glfwPollEvents();

    if (window_) {
        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0 && (fbWidth != windowWidth_ || fbHeight != windowHeight_)) {
            windowWidth_ = fbWidth;
            windowHeight_ = fbHeight;
            glViewport(0, 0, windowWidth_, windowHeight_);
            proceduralLayer_.resize(windowWidth_, windowHeight_);
            postProcessor_.resize(windowWidth_, windowHeight_);
        }
    }

    if (imguiInitialized_ && window_) {
        static double lastTabToggle = 0.0;
        double now = glfwGetTime();
        bool tabDown = isKeyPressed(GLFW_KEY_TAB);
        ImGuiIO *io = ImGui::GetCurrentContext() ? &ImGui::GetIO() : nullptr;
        bool allowToggle =
            !tabDown ? false : (!io || !io->WantCaptureKeyboard || !showImGuiWindow_);

        if (allowToggle && (now - lastTabToggle) > 0.25) {
            showImGuiWindow_ = !showImGuiWindow_;
            if (!showImGuiWindow_) {
                showDeviceSelector_ = false;
                showDiagnosticInfo_ = false;
                showConsoleMode_ = false;
                // Note: showCurrentEffects_ remains independent and is not affected by TAB
            }
            lastTabToggle = now;
        }

        if (!io || !io->WantCaptureKeyboard) {
            static double lastModeToggle = 0.0;
            static double lastOpacityAdjust = 0.0;
            static double lastPostProcessToggle = 0.0;
            if ((now - lastModeToggle) > 0.15) {
                if (isKeyPressed(GLFW_KEY_RIGHT)) {
                    // Change Slot 1 mode - skip disabled shaders
                    if (kMaxProceduralSlots > 0) {
                        int currentMode = proceduralSlots_[0].mode;
                        int newMode = findNextEnabledMode(currentMode, true); // forward
                        if (newMode != currentMode) {
                            applyMainProceduralMode(newMode, "keyboard-right");
                        }
                    }
                    lastModeToggle = now;
                } else if (isKeyPressed(GLFW_KEY_LEFT)) {
                    // Change Slot 1 mode - skip disabled shaders
                    if (kMaxProceduralSlots > 0) {
                        int currentMode = proceduralSlots_[0].mode;
                        int newMode = findNextEnabledMode(currentMode, false); // backward
                        if (newMode != currentMode) {
                            applyMainProceduralMode(newMode);
                        }
                    }
                    lastModeToggle = now;
                }
            }

            if ((now - lastOpacityAdjust) > 0.12) {
                constexpr float kOpacityStep = 0.05f;
                bool adjusted = false;
                if (isKeyPressed(GLFW_KEY_UP)) {
                    // Change Slot 1 opacity
                    if (kMaxProceduralSlots > 0) {
                        proceduralSlots_[0].opacity += kOpacityStep;
                        proceduralLayerOpacity_ = proceduralSlots_[0].opacity;
                        proceduralSlots_[0].enabled = true; // Always ensure Slot 1 is enabled
                        showProceduralLayer_ = true;
                    }
                    adjusted = true;
                } else if (isKeyPressed(GLFW_KEY_DOWN)) {
                    // Change Slot 1 opacity
                    if (kMaxProceduralSlots > 0) {
                        proceduralSlots_[0].opacity -= kOpacityStep;
                        proceduralLayerOpacity_ = proceduralSlots_[0].opacity;
                        proceduralSlots_[0].enabled = true; // Always ensure Slot 1 is enabled
                        showProceduralLayer_ = true;
                    }
                    adjusted = true;
                }

                if (adjusted) {
                    if (kMaxProceduralSlots > 0) {
                        // Clamp Slot 1 opacity
                        if (proceduralSlots_[0].opacity < 0.0f) {
                            proceduralSlots_[0].opacity = 0.0f;
                        } else if (proceduralSlots_[0].opacity > 1.0f) {
                            proceduralSlots_[0].opacity = 1.0f;
                        }
                        proceduralLayerOpacity_ = proceduralSlots_[0].opacity;
                    } else {
                        // Fallback to global opacity
                        if (proceduralLayerOpacity_ < 0.0f) {
                            proceduralLayerOpacity_ = 0.0f;
                        } else if (proceduralLayerOpacity_ > 1.0f) {
                            proceduralLayerOpacity_ = 1.0f;
                        }
                    }
                    lastOpacityAdjust = now;
                }
            }

            if ((now - lastPostProcessToggle) > 0.25) {
                if (isKeyPressed(GLFW_KEY_P)) {
                    // Enable slot 1 (index 0) and cycle its mode forward
                    auto& slot = postProcessSlots_[0];
                    slot.enabled = true;
                    slot.mode = (slot.mode + 1) % kPostProcessModeCount;
                    if (slot.mode == 0) {
                        slot.mode = 1; // Skip mode 0 (no effect)
                    }
                    slot.strength = 1.0f;
                    slot.rgbAdjust = {1.0f, 1.0f, 1.0f};
                    lastPostProcessToggle = now;
                }
                // 'O' key now controls corner orbs instead of post-processing
            }
        }
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Visualizer::endFrame() {
    glfwSwapBuffers(window_);

    // Update time
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    time_ += deltaTime;
    deltaTime_ = deltaTime;
    
    // Update FPS counter
    frameCount_++;
    fpsUpdateTimer_ += deltaTime;
    if (fpsUpdateTimer_ >= 0.5f) { // Update every 0.5 seconds
        currentFPS_ = frameCount_ / fpsUpdateTimer_;
        frameCount_ = 0;
        fpsUpdateTimer_ = 0.0f;
    }

    if (autoRandomizeColors_) {
        if (colorRandomInterval_ < 0.5f) {
            colorRandomInterval_ = 0.5f;
        }
        colorRandomTimer_ += deltaTime;
        if (colorRandomTimer_ >= colorRandomInterval_) {
            randomizeScenePalette();
            colorRandomTimer_ = 0.0f;
        }
    }

    if (onsetColorCyclingEnabled_) {
        bool onsetActive = audioFeatures_.onset > 0.5f;
        if (onsetActive && !lastOnsetActive_) {
            ++onsetTriggerCount_;
            if (onsetTriggerCount_ >= 2) {
                onsetTriggerCount_ = 0;
                cycleScenePaletteSequential();
            }
        }
        lastOnsetActive_ = onsetActive;
    } else {
        onsetTriggerCount_ = 0;
        lastOnsetActive_ = audioFeatures_.onset > 0.5f;
    }
    
    // RGB channel randomization based on music (onsets)
    if (autoRandomizeRgbChannels_) {
        bool onsetActive = audioFeatures_.onset > 0.5f;
        if (onsetActive && !lastOnsetActive_) {
            ++lastOnsetCount_;
            // Randomize RGB channels every 3 onsets
            if (lastOnsetCount_ >= 3) {
                lastOnsetCount_ = 0;
                randomizeRgbChannels();
            }
        } else if (!onsetActive) {
            // Reset counter when there's no onset
            lastOnsetCount_ = 0;
        }
        
        // Also randomize by time interval
        rgbRandomTimer_ += deltaTime;
        if (rgbRandomTimer_ >= rgbRandomInterval_) {
            rgbRandomTimer_ = 0.0f;
            randomizeRgbChannels();
        }
    }
}

void Visualizer::updateAudioData(const AudioAnalyzer::AudioFeatures &features) {
    audioFeatures_ = features;

    float sensitivity = std::clamp(visualSensitivity_, 0.25f, 3.0f);

    float targetTempo = 1.0f;
    float bpm = features.bpm;
    if (bpm > 30.0f) {
        targetTempo = std::clamp(bpm / 120.0f, 0.35f, 1.8f);
    } else {
        float energy = std::clamp(features.energy, 0.0f, 1.2f);
        targetTempo = 0.75f + energy * 0.35f;
    }
    
    // Use manual BPM if enabled
    if (manualBPMMode_) {
        // Convert BPM to tempo multiplier (120 BPM = 1.0)
        tempoMultiplier_ = manualBPM_ / 120.0f;
    } else {
        tempoMultiplier_ = std::clamp(tempoMultiplier_ * 0.9f + targetTempo * 0.1f, 0.3f, 2.0f);
    }

    audioFeatures_.energy = std::clamp(audioFeatures_.energy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.bassEnergy = std::clamp(audioFeatures_.bassEnergy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.midEnergy = std::clamp(audioFeatures_.midEnergy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.highEnergy = std::clamp(audioFeatures_.highEnergy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.onset = std::clamp(audioFeatures_.onset * sensitivity, 0.0f, 2.0f);
    audioFeatures_.beat = std::clamp(audioFeatures_.beat * sensitivity, 0.0f, 2.0f);
    audioFeatures_.kick = std::clamp(audioFeatures_.kick * sensitivity, 0.0f, 2.0f);
}

void Visualizer::updateAudioBuffer(const std::vector<float> &audioBuffer) {
    if (audioBuffer.size() >= waveformBuffer_.size()) {
        std::copy(audioBuffer.begin(), audioBuffer.begin() + waveformBuffer_.size(),
                  waveformBuffer_.begin());
    }
}

void Visualizer::render() {
    // Check if any post-process slots are active
    bool hasActivePostProcess = false;
    for (const auto &slot : postProcessSlots_) {
        if (slot.enabled && slot.mode > 0 && slot.strength > 0.0f) {
            hasActivePostProcess = true;
            break;
        }
    }

    bool usePost = showPostProcess_ && postProcessor_.isInitialized() && hasActivePostProcess;

    float rawEnergy = std::clamp(audioFeatures_.energy, 0.0f, 2.5f);
    float bass = std::clamp(audioFeatures_.bassEnergy, 0.0f, 2.0f);
    float excitement = std::clamp(audioFeatures_.onset * 0.6f + audioFeatures_.beat * 0.8f +
                                      audioFeatures_.kick * 0.5f,
                                  0.0f, 1.6f);
    float targetIntensity =
        std::clamp(rawEnergy * 0.55f + bass * 0.45f + excitement * 0.65f, 0.0f, 2.5f);

    float dt = std::max(deltaTime_, 1.0f / 120.0f);
    
    // Update random post process
    updateRandomPostProcess(dt);
    
    // Update random procedural layer first, then sync Slot 1 so the render uses
    // the latest procedural mode for this frame.
    updateRandomProcedural(dt);
    
    // Sync procedural layer with Slot 1 state after any automatic updates
    syncProceduralLayerWithSlot1();
    
    // Update random corner orbs
    updateRandomCornerOrbs(dt);
    
    float rise = 1.0f - std::pow(0.04f, dt * tempoMultiplier_);
    float decayBase = std::pow(0.18f, dt * tempoMultiplier_);

    if (targetIntensity > globalIntensityEnvelope_) {
        globalIntensityEnvelope_ += (targetIntensity - globalIntensityEnvelope_) * rise;
    } else {
        globalIntensityEnvelope_ =
            globalIntensityEnvelope_ * decayBase + targetIntensity * (1.0f - decayBase);
    }
    globalIntensityEnvelope_ = std::clamp(globalIntensityEnvelope_, 0.0f, 2.5f);

    if (usePost) {
        postProcessor_.beginCapture(windowWidth_, windowHeight_);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    GLboolean previousMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, previousMask);
    glColorMask(rgbChannelEnabled_[0] ? GL_TRUE : GL_FALSE,
                rgbChannelEnabled_[1] ? GL_TRUE : GL_FALSE,
                rgbChannelEnabled_[2] ? GL_TRUE : GL_FALSE, GL_TRUE);

    if (currentScenePaletteIndex_ < 0) {
        updateDynamicScenePalette();
    }

    if (showProceduralLayer_) {
        renderProceduralLayer();
    }

    float intensityScale = std::clamp(globalIntensityEnvelope_, 0.0f, 2.0f);
    renderModernCore();

    renderIdleSpinner(time_);

    if (showCornerOrbs_) {
        renderCornerOrbs();
    }

    if (usePost) {
        postProcessor_.endCapture();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Apply all active post-process slots in cascade
        std::vector<PostProcessor::PostEffectPass> activePasses;
        for (const auto &slot : postProcessSlots_) {
            if (slot.enabled && slot.mode > 0 && slot.strength > 0.0f) {
                PostProcessor::PostEffectPass pass;
                pass.mode = slot.mode;
                float strength = std::clamp(slot.strength, 0.0f, 1.0f);
                strength *= std::clamp(0.35f + intensityScale * 0.65f, 0.3f, 1.0f);
                pass.strength = strength;
                pass.rgbAdjust = slot.rgbAdjust;
                activePasses.push_back(pass);
            }
        }
        
        // Apply all passes at once for proper cascading
        if (!activePasses.empty()) {
            postProcessor_.applyChain(activePasses, time_, audioFeatures_.bassEnergy);
        }
    }

    glColorMask(previousMask[0], previousMask[1], previousMask[2], previousMask[3]);

    handleKeyboardInput();
    handleVisualizationShortcuts();

    if (imguiInitialized_ && showImGuiWindow_) {
        renderImGui();
    } else if (!imguiInitialized_) {
        renderGUI();
    }
}

void Visualizer::handleVisualizationShortcuts() {
    if (!window_) {
        return;
    }

    constexpr double kToggleCooldown = 0.35;
    double now = glfwGetTime();

    static double lastOrbsToggle = 0.0;
    static double lastKaleidoToggle = 0.0;
    static double lastGToggle = 0.0;
    static double lastRToggle = 0.0;
    static double lastBToggle = 0.0;
    static double lastIToggle = 0.0;
    static bool orbsWasDown = false;
    static bool kaleidoWasDown = false;
    static bool gWasDown = false;
    static bool rWasDown = false;
    static bool bWasDown = false;
    static bool iWasDown = false;

    // Post-processing effects are handled in render() to avoid conflicts
    
    bool kaleidoKeyDown = isKeyPressed(GLFW_KEY_K);
    bool gKeyDown = isKeyPressed(GLFW_KEY_G);
    bool rKeyDown = isKeyPressed(GLFW_KEY_R);
    bool bKeyDown = isKeyPressed(GLFW_KEY_B);
    bool iKeyDown = isKeyPressed(GLFW_KEY_I);

    auto updatePostProcessState = [this]() {
        bool anyActive = false;
        for (const auto &slot : postProcessSlots_) {
            if (slot.enabled && slot.mode > 0 && slot.strength > 0.0f) {
                anyActive = true;
                break;
            }
        }
        showPostProcess_ = anyActive;
    };

    auto togglePostProcessEffect = [&](int mode, float defaultStrength) {
        for (auto &slot : postProcessSlots_) {
            if (slot.enabled && slot.mode == mode) {
                slot.enabled = false;
                updatePostProcessState();
                return;
            }
        }

        for (auto &slot : postProcessSlots_) {
            if (!slot.enabled) {
                slot.enabled = true;
                slot.mode = mode;
                slot.strength = defaultStrength;
                slot.rgbAdjust = {1.0f, 1.0f, 1.0f};
                updatePostProcessState();
                return;
            }
        }
    };

    auto processToggle = [&](bool keyDown, bool &wasDown, double &lastToggle, auto &&action) {
        if (keyDown) {
            if (!wasDown && (now - lastToggle) > kToggleCooldown) {
                action();
                lastToggle = now;
            }
            wasDown = true;
        } else {
            wasDown = false;
        }
    };

    // Post-processing effects are handled in render() to avoid conflicts

    processToggle(kaleidoKeyDown, kaleidoWasDown, lastKaleidoToggle, [this]() {
        applyMainProceduralMode(kKaleidoscopeModeIndex, "keyboard-k-toggle");
    });

    // RGB channel toggles
    processToggle(rKeyDown, rWasDown, lastRToggle,
                  [this]() { rgbChannelEnabled_[0] = !rgbChannelEnabled_[0]; });

    processToggle(gKeyDown, gWasDown, lastGToggle,
                  [this]() { rgbChannelEnabled_[1] = !rgbChannelEnabled_[1]; });

    processToggle(bKeyDown, bWasDown, lastBToggle,
                  [this]() { rgbChannelEnabled_[2] = !rgbChannelEnabled_[2]; });

    // Toggle effects display window
    processToggle(iKeyDown, iWasDown, lastIToggle,
                  [this]() { showCurrentEffects_ = !showCurrentEffects_; });
}

void Visualizer::updateSettingsFromCurrentState() {
    if (!settingsManager_) return;

    std::cout << "[SAVE DEBUG] Syncing Visualizer -> SettingsManager"
              << " proceduralLayerMode_=" << proceduralLayerMode_
              << " slot0.mode=" << (kMaxProceduralSlots > 0 ? proceduralSlots_[0].mode : -1)
              << " slot0.enabled=" << (kMaxProceduralSlots > 0 ? proceduralSlots_[0].enabled : false)
              << " slot0.opacity=" << (kMaxProceduralSlots > 0 ? proceduralSlots_[0].opacity : 0.0f)
              << std::endl;

    if (kMaxProceduralSlots > 0 && proceduralSlots_[0].mode != proceduralLayerMode_) {
        std::cout << "[SAVE DEBUG] Forcing slot0.mode to match proceduralLayerMode_ before save: "
                  << proceduralSlots_[0].mode << " -> " << proceduralLayerMode_ << std::endl;
        proceduralSlots_[0].mode = proceduralLayerMode_;
    }
    
    // Update settings manager with current visualizer state
    settingsManager_->setSelectedDevice(selectedDevice_);
    settingsManager_->setAudioInputGain(audioInputGain_);
    settingsManager_->setVisualSensitivity(visualSensitivity_);
    settingsManager_->setShowImGuiWindow(showImGuiWindow_);
    settingsManager_->setShowCornerOrbs(showCornerOrbs_);
    settingsManager_->setShowProceduralLayer(showProceduralLayer_);
    settingsManager_->setShowCurrentEffects(showCurrentEffects_);
    settingsManager_->setProceduralLayerDebug(proceduralLayerDebug_);
    settingsManager_->setProceduralLayerOpacity(proceduralLayerOpacity_);
    settingsManager_->setProceduralLayerMode(proceduralLayerMode_);
    
    // Update post-process slots
    settingsManager_->setPostProcessSlots(postProcessSlots_);
    
    // Update procedural slots
    settingsManager_->setProceduralSlots(proceduralSlots_);
    
    // Update random settings
    settingsManager_->setRandomPostProcessEnabled(randomPostProcessEnabled_);
    settingsManager_->setRandomPostProcessInterval(randomPostProcessInterval_);
    settingsManager_->setRandomPostProcessSlotCount(randomPostProcessSlotCount_);
    settingsManager_->setRandomProceduralEnabled(randomProceduralEnabled_);
    settingsManager_->setRandomProceduralInterval(randomProceduralInterval_);
    
    // Update hot-reload setting
    settingsManager_->setHotReloadEnabled(hotReloadEnabled_);
    
    // Update colors
    settingsManager_->setScenePrimaryColor(scenePrimaryColor_);
    settingsManager_->setSceneSecondaryColor(sceneSecondaryColor_);
    settingsManager_->setScenePaletteBlend(scenePaletteBlend_);
    settingsManager_->setCurrentScenePaletteIndex(currentScenePaletteIndex_);
    settingsManager_->setScenePaletteHueSeed(scenePaletteHueSeed_);
    
    // Update animation settings
    settingsManager_->setAutoRandomizeColors(autoRandomizeColors_);
    settingsManager_->setColorRandomInterval(colorRandomInterval_);
    settingsManager_->setOnsetColorCyclingEnabled(onsetColorCyclingEnabled_);
    settingsManager_->setAutoRandomizeRgbChannels(autoRandomizeRgbChannels_);
    settingsManager_->setManualBPMMode(manualBPMMode_);
    settingsManager_->setManualBPM(manualBPM_);
    
    // Update RGB channels
    for (int i = 0; i < 3; ++i) {
        settingsManager_->setRgbChannelEnabled(i, rgbChannelEnabled_[i]);
    }
    
    // Update per-shader enabled states
    for (const auto& [modeIndex, enabled] : proceduralShaderEnabled_) {
        settingsManager_->setProceduralShaderEnabled(modeIndex, enabled);
    }
    
    // Update per-post-processing-effect enabled states
    for (const auto& [modeIndex, enabled] : postProcessEffectEnabled_) {
        settingsManager_->setPostProcessEffectEnabled(modeIndex, enabled);
    }

    std::cout << "[SAVE DEBUG] SettingsManager updated"
              << " proceduralLayerMode=" << settingsManager_->getProceduralLayerMode()
              << " slot0.mode=" << (settingsManager_->getProceduralSlots().empty() ? -1 : settingsManager_->getProceduralSlots()[0].mode)
              << std::endl;
}

void Visualizer::saveCurrentSettings() {
    std::cout << "[SAVE DEBUG] saveCurrentSettings() called" << std::endl;
    updateSettingsFromCurrentState();
    if (settingsManager_) {
        std::cout << "[SAVE DEBUG] Persisting settings to disk" << std::endl;
        settingsManager_->saveSettings();
    }
}

bool Visualizer::setupOpenGL() {
    std::cout << "[DEBUG] Initializing OpenGL..." << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    std::cout << "[DEBUG] GLFW initialized successfully" << std::endl;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); // Use OpenGL 2.1 for compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE); // Don't force core profile
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE); // Keep fullscreen window rendering when losing focus

    // Try with OpenGL ES if desktop OpenGL fails
    bool useGLES = false;

    // === CREATE MAIN VISUALS WINDOW ===
    std::cout << "[DEBUG] Creating main visuals window (" << windowWidth_ << "x" << windowHeight_ << ")..." << std::endl;
    window_ = glfwCreateWindow(windowWidth_, windowHeight_, "Audio Visualizer - Visuals", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create main GLFW window" << std::endl;
        return false;
    }
    std::cout << "[DEBUG] Main visuals window created successfully" << std::endl;

    glfwMakeContextCurrent(window_);
    std::cout << "[DEBUG] OpenGL context made current on main window" << std::endl;

    // Set window user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);

    // Initialize GLEW with experimental features
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInit()) << std::endl;

        // Try without GLEW for basic functionality
        std::cout << "Attempting to continue without GLEW..." << std::endl;
    }

    // Clear any potential GL errors from GLEW initialization
    glGetError();

    glViewport(0, 0, windowWidth_, windowHeight_);

    // === CREATE SEPARATE IMGUI CONTROLS WINDOW ===
    // The second window shares the context with the first window
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE); // Don't steal focus when showing controls window
    std::cout << "[DEBUG] Creating ImGui controls window (" << imguiWindowWidth_ << "x" << imguiWindowHeight_ << ")..." << std::endl;
    imguiWindow_ = glfwCreateWindow(imguiWindowWidth_, imguiWindowHeight_, "Audio Visualizer - Controls", nullptr, window_);
    if (!imguiWindow_) {
        std::cerr << "Failed to create ImGui GLFW window, continuing without controls window" << std::endl;
        // Continue without the controls window - non-critical
    } else {
        std::cout << "[DEBUG] ImGui controls window created successfully" << std::endl;

        // Position ImGui window to the right of the main window
        int mainX, mainY;
        glfwGetWindowPos(window_, &mainX, &mainY);
        glfwSetWindowPos(imguiWindow_, mainX + windowWidth_ + 50, mainY);

        // Set up scroll callback for mouse wheel zoom in ImGui window
        glfwSetScrollCallback(imguiWindow_, [](GLFWwindow* window, double xoffset, double yoffset) {
            Visualizer* vis = static_cast<Visualizer*>(glfwGetWindowUserPointer(window));
            if (vis) {
                vis->handleMouseScroll(xoffset, yoffset);
            }
        });
    }

    // Make main window current again
    glfwMakeContextCurrent(window_);

    // Disable vsync to prevent compositor from pausing rendering when window not visible
    // This is critical for Hyprland/Wayland where frame callbacks stop on inactive workspaces
    glfwSwapInterval(0);

    // Detect available monitors for multi-monitor support
    detectMonitors();

    std::cout << "OpenGL setup successful!" << std::endl;
    return true;
}

bool Visualizer::isKeyPressed(int key) const {
    if (window_ && glfwGetKey(window_, key) == GLFW_PRESS) {
        return true;
    }
    if (imguiWindow_ && glfwGetKey(imguiWindow_, key) == GLFW_PRESS) {
        return true;
    }
    return false;
}

bool Visualizer::setupGeometry() {
    setupQuad();
    setupWaveform();
    return true;
}

void Visualizer::setupSparkField() {
    if (sparkVAO_) {
        glDeleteVertexArrays(1, &sparkVAO_);
        sparkVAO_ = 0;
    }
    if (sparkVBO_) {
        glDeleteBuffers(1, &sparkVBO_);
        sparkVBO_ = 0;
    }

    const int layers = 6;
    const int sparksPerLayer = 96;
    const int totalSparks = layers * sparksPerLayer;
    std::vector<float> data;
    data.reserve(totalSparks * 3);

    std::uniform_real_distribution<float> randomAngle(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> randomSeed(0.0f, 1.0f);

    for (int layer = 0; layer < layers; ++layer) {
        float radialLayer = static_cast<float>(layer) / static_cast<float>(std::max(1, layers - 1));
        for (int i = 0; i < sparksPerLayer; ++i) {
            float angle = (static_cast<float>(i) / sparksPerLayer) * 6.28318530718f;
            angle += randomAngle(rng_) * 0.08f;
            float seed = randomSeed(rng_);
            data.push_back(angle);
            data.push_back(radialLayer);
            data.push_back(seed);
        }
    }

    sparkVertexCount_ = static_cast<GLsizei>(data.size() / 3);

    glGenVertexArrays(1, &sparkVAO_);
    glGenBuffers(1, &sparkVBO_);

    glBindVertexArray(sparkVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, sparkVBO_);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    GLsizei stride = 3 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Visualizer::setupCornerQuad() {
    if (cornerVAO_) {
        glDeleteVertexArrays(1, &cornerVAO_);
        cornerVAO_ = 0;
    }
    if (cornerVBO_) {
        glDeleteBuffers(1, &cornerVBO_);
        cornerVBO_ = 0;
    }

    struct CornerVertex {
        float x;
        float y;
        float size;
        float orbitRadius;
        float orbitPhase;
        float profile;
    };

    std::array<CornerVertex, 8> corners = {
        // Esquinas originales
        CornerVertex{-0.88f, 0.88f, 0.075f, 0.020f, 0.0f, 0.10f},   // Superior izquierda
        CornerVertex{0.88f, 0.88f, 0.072f, 0.022f, 1.3f, 0.35f},    // Superior derecha
        CornerVertex{-0.88f, -0.88f, 0.078f, 0.024f, -1.6f, 0.55f}, // Inferior izquierda
        CornerVertex{0.88f, -0.88f, 0.074f, 0.021f, 2.2f, 0.78f},   // Inferior derecha

        // Nuevos orbes en el medio
        CornerVertex{0.00f, 0.88f, 0.068f, 0.018f, 0.8f, 0.25f},   // Centro superior
        CornerVertex{0.00f, -0.88f, 0.070f, 0.019f, -0.8f, 0.65f}, // Centro inferior
        CornerVertex{-0.88f, 0.00f, 0.069f, 0.017f, 2.8f, 0.45f},  // Centro izquierda
        CornerVertex{0.88f, 0.00f, 0.071f, 0.020f, -2.8f, 0.85f}   // Centro derecha
    };

    cornerVertexCount_ = static_cast<GLsizei>(corners.size());

    glGenVertexArrays(1, &cornerVAO_);
    glGenBuffers(1, &cornerVBO_);

    glBindVertexArray(cornerVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                          reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Visualizer::loadShaders() {
    shader_ = std::make_unique<Shader>();
    return shader_->loadFromSource(vertexShaderSource, fragmentShaderSource);
}

bool Visualizer::loadCoreShader() {
    coreShader_ = std::make_unique<Shader>();
    return coreShader_->loadFromSource(coreVertexShaderSource, coreFragmentShaderSource);
}

bool Visualizer::loadSparkShader() {
    sparkShader_ = std::make_unique<Shader>();
    return sparkShader_->loadFromSource(sparkVertexShaderSource, sparkFragmentShaderSource);
}

bool Visualizer::loadCornerShader() {
    cornerShader_ = std::make_unique<Shader>();
    bool result = cornerShader_->loadFromSource(cornerVertexShaderSource, cornerFragmentShaderSource);
    if (result) {
        std::cout << "Corner shader loaded successfully" << std::endl;
    } else {
        std::cerr << "Failed to load corner shader" << std::endl;
    }
    return result;
}

void Visualizer::setupQuad() {
    float vertices[] = {// positions    // texCoords
                        -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
                        1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f};

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);

    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // tex coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Visualizer::setupWaveform() {
    glGenVertexArrays(1, &waveformVAO_);
    glGenBuffers(1, &waveformVBO_);

    glBindVertexArray(waveformVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, waveformVBO_);

    // Allocate buffer memory (will be updated dynamically)
    glBufferData(GL_ARRAY_BUFFER, waveformBuffer_.size() * sizeof(float) * 2, nullptr,
                 GL_DYNAMIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Visualizer::renderWaveform(const std::vector<float> &audioBuffer) {
    if (audioBuffer.empty())
        return;

    // Create vertices for waveform
    std::vector<float> vertices;
    vertices.reserve(audioBuffer.size() * 2);

    float waveHeight = 100.0f;                        // Height of waveform display
    float waveY = windowHeight_ - waveHeight - 20.0f; // Position at bottom

    for (size_t i = 0; i < audioBuffer.size(); ++i) {
        float x = (float)i / (audioBuffer.size() - 1) * windowWidth_;
        float y = waveY + audioBuffer[i] * waveHeight * 0.5f;
        vertices.push_back(x);
        vertices.push_back(y);
    }

    // Update VBO with new waveform data
    glBindBuffer(GL_ARRAY_BUFFER, waveformVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());

    // Simple shader for waveform (colored line)
    glUseProgram(0); // Use fixed function pipeline for simplicity
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);

    // Draw waveform as line strip
    glColor3f(0.0f, 1.0f, 0.5f); // Cyan color
    glLineWidth(2.0f);

    glBindVertexArray(waveformVAO_);
    glDrawArrays(GL_LINE_STRIP, 0, audioBuffer.size());
    glBindVertexArray(0);

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Visualizer::renderFallbackTriangle() {
    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(0);
    glUseProgram(0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_TRIANGLES);
    glColor4f(0.8f, 0.2f, 0.4f, 0.9f);
    glVertex2f(0.0f, 0.8f);

    glColor4f(0.2f, 0.6f, 0.9f, 0.9f);
    glVertex2f(-0.8f, -0.6f);

    glColor4f(0.2f, 0.9f, 0.4f, 0.9f);
    glVertex2f(0.8f, -0.6f);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void Visualizer::renderModernVisualization() {
    if (!shader_ || quadVAO_ == 0) {
        renderFallbackTriangle();
        return;
    }

    shader_->use();
    shader_->setUniform1f("uTime", time_);
    shader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    shader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    shader_->setUniform1f("uHigh", audioFeatures_.highEnergy);
    shader_->setUniform1f("uEnergy", audioFeatures_.energy);
    shader_->setUniform1f("uOnset", audioFeatures_.onset);
    shader_->setUniform1f("uBeat", audioFeatures_.beat);
    shader_->setUniform2f("uResolution", static_cast<float>(windowWidth_),
                          static_cast<float>(windowHeight_));
    shader_->setUniform3f("uScenePrimary", scenePrimaryColor_[0], scenePrimaryColor_[1],
                          scenePrimaryColor_[2]);
    shader_->setUniform3f("uSceneSecondary", sceneSecondaryColor_[0], sceneSecondaryColor_[1],
                          sceneSecondaryColor_[2]);
    shader_->setUniform1f("uSceneBlend", std::clamp(scenePaletteBlend_, 0.0f, 1.0f));

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) {
        glDisable(GL_DEPTH_TEST);
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (!blendWasEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void Visualizer::renderShaderSparkles() {
    if (!sparkShader_ || sparkVAO_ == 0) {
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
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    GLboolean programPointEnabled = glIsEnabled(GL_PROGRAM_POINT_SIZE);
    if (!programPointEnabled) {
        glEnable(GL_PROGRAM_POINT_SIZE);
    }

    sparkShader_->use();
    sparkShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_),
                               static_cast<float>(windowHeight_));
    sparkShader_->setUniform1f("uBaseRadius", core_.baseRadius);
    sparkShader_->setUniform1f("uTime", time_);
    sparkShader_->setUniform1f("uTempo", tempoMultiplier_);
    sparkShader_->setUniform1f("uPulse", core_.pulse);
    sparkShader_->setUniform1f("uEnergy", audioFeatures_.energy);
    sparkShader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    sparkShader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    sparkShader_->setUniform1f("uHigh", audioFeatures_.highEnergy);

    glBindVertexArray(sparkVAO_);
    glDrawArrays(GL_POINTS, 0, sparkVertexCount_);
    glBindVertexArray(0);

    glUseProgram(0);

    if (!programPointEnabled) {
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void Visualizer::renderModernCore() {
    if (!coreShader_ || coreVAO_ == 0) {
        renderGears(time_);
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
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    coreShader_->use();
    coreShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_),
                              static_cast<float>(windowHeight_));
    coreShader_->setUniform1f("uBaseRadius", core_.baseRadius);
    coreShader_->setUniform1f("uPulse", core_.pulse);
    coreShader_->setUniform1f("uKick", core_.kickEnvelope);
    coreShader_->setUniform1f("uHarmonic", core_.harmonicEnvelope);
    coreShader_->setUniform1f("uGrowth", std::clamp(core_.growthEnvelope * 0.35f, 0.0f, 3.5f));
    coreShader_->setUniform1f("uMaturity", std::clamp(core_.maturity * 0.2f, 0.0f, 3.0f));
    coreShader_->setUniform1f("uEnergy", audioFeatures_.energy);
    coreShader_->setUniform1f("uBass", audioFeatures_.bassEnergy);
    coreShader_->setUniform1f("uMid", audioFeatures_.midEnergy);
    coreShader_->setUniform1f("uHigh", audioFeatures_.highEnergy);
    coreShader_->setUniform1f("uTime", time_);
    coreShader_->setUniform1f("uTempo", tempoMultiplier_);
    coreShader_->setUniform1f("uOnset", audioFeatures_.onset);
    coreShader_->setUniform1f("uBeat", audioFeatures_.beat);
    coreShader_->setUniform1f("uIdlePulse", core_.idlePulse);
    coreShader_->setUniform1f("uIdleWarp", core_.idleWarp);
    coreShader_->setUniform1f("uIdleSpin", core_.idleSpin);
    coreShader_->setUniform1f("uShowSpokes", coreShowSpokes_ ? 1.0f : 0.0f);
    coreShader_->setUniform1f("uShowRunes", coreShowRunes_ ? 1.0f : 0.0f);
    coreShader_->setUniform1f("uShowSparkles", coreShowSparkles_ ? 1.0f : 0.0f);
    coreShader_->setUniform1f("uShowBloom", coreShowBloom_ ? 1.0f : 0.0f);
    coreShader_->setUniform3f("uScenePrimary", scenePrimaryColor_[0], scenePrimaryColor_[1],
                              scenePrimaryColor_[2]);
    coreShader_->setUniform3f("uSceneSecondary", sceneSecondaryColor_[0], sceneSecondaryColor_[1],
                              sceneSecondaryColor_[2]);
    coreShader_->setUniform1f("uSceneBlend", std::clamp(scenePaletteBlend_, 0.0f, 1.0f));

    glBindVertexArray(coreVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, coreVertexCount_);
    glBindVertexArray(0);

    glUseProgram(0);

    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void Visualizer::setupDeviceList() {
    // Get device names
    PaError err = Pa_Initialize();
    if (err != paNoError)
        return;

    int numDevices = Pa_GetDeviceCount();
    deviceNames_.clear();
    deviceIsInternal_.clear();

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            deviceNames_.push_back(deviceInfo->name);
            bool isInternalLoopback = false;
            if (const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi)) {
                const std::string hostName(hostInfo->name ? hostInfo->name : "");
                const std::string deviceName(deviceInfo->name ? deviceInfo->name : "");
                if (hostName.find("WASAPI") != std::string::npos) {
                    if (deviceName.find("(loopback)") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                } else if (hostName.find("WDM-KS") != std::string::npos) {
                    if (deviceName.find("Loopback") != std::string::npos ||
                        deviceName.find("(loopback)") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                } else if (hostName.find("Core Audio") != std::string::npos) {
                    if (deviceName.find("Loopback") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                } else if (hostName.find("ALSA") != std::string::npos) {
                    if (deviceName.find("Monitor") != std::string::npos) {
                        isInternalLoopback = true;
                    }
                }
            }
            deviceIsInternal_.push_back(isInternalLoopback);
        } else {
            deviceNames_.push_back(""); // No input channels
            deviceIsInternal_.push_back(false);
        }
    }

    Pa_Terminate();
}

void Visualizer::renderGUI() {
    // Check for 'D' key toggle (only for device menu when ImGui is not initialized)
    if (isKeyPressed(GLFW_KEY_D)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showDeviceMenu_ = !showDeviceMenu_;
            lastPress = currentTime;
        }
    }

    if (showDeviceMenu_) {
        showDeviceSelector();
    }

    // Show current device info
    std::string deviceInfo = "Device: ";
    if (selectedDevice_ >= 0 && selectedDevice_ < deviceNames_.size()) {
        deviceInfo += deviceNames_[selectedDevice_];
    } else {
        deviceInfo += "Default";
    }
    deviceInfo += " (D:devices I:diagnostics C:console)";
    renderText(deviceInfo, 10, 30);

    // Show diagnostic info if enabled
    if (showDiagnostic_) {
        renderDiagnosticInfo();
    }

    // Show console visualization if enabled
    if (consoleMode_) {
        renderConsoleVisualization();
    }
}

bool Visualizer::showDeviceSelector() {
    // Simple device selector using text rendering
    float menuX = 50.0f;
    float menuY = 100.0f;
    float lineHeight = 25.0f;

    renderText("=== Select Audio Device ===", menuX, menuY);
    renderText("Use number keys 1-9 to select", menuX, menuY + lineHeight);
    renderText("Press ESC to cancel", menuX, menuY + lineHeight * 2);
    renderText("", menuX, menuY + lineHeight * 3);

    // Show available devices
    int displayCount = 0;
    for (int i = 0; i < deviceNames_.size() && displayCount < 9; ++i) {
        if (!deviceNames_[i].empty()) {
            std::string deviceText = std::to_string(displayCount + 1) + ". " + deviceNames_[i];
            if (i == selectedDevice_) {
                deviceText += " [CURRENT]";
            }
            renderText(deviceText, menuX, menuY + lineHeight * (4 + displayCount));
            displayCount++;
        }
    }

    // Handle ESC
    if (isKeyPressed(GLFW_KEY_ESCAPE)) {
        showDeviceMenu_ = false;
    }

    return false;
}

void Visualizer::renderText(const std::string &text, float x, float y) {
    // Simple text rendering using bitmap characters (basic implementation)
    // For now, we'll use a very simple approach with line segments

    glUseProgram(0); // Use fixed function pipeline

    // Setup 2D projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);

    // Set text color
    glColor3f(1.0f, 1.0f, 1.0f);

    // Very basic text rendering (just show the text as a placeholder)
    // In a real implementation, you'd use a proper font rendering system
    glRasterPos2f(x, y);

    // For now, just print to console as a fallback
    // This is a placeholder - proper text rendering would require a font library
    static std::string lastText;
    if (text != lastText) {
        std::cout << "GUI: " << text << std::endl;
        lastText = text;
    }

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Visualizer::setupCoreMesh() {
    // Placeholder implementation
    if (coreVAO_ == 0) {
        glGenVertexArrays(1, &coreVAO_);
        glGenBuffers(1, &coreVBO_);
    }

    // Simple quad for now
    float vertices[] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};

    glBindVertexArray(coreVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, coreVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glBindVertexArray(0);

    coreVertexCount_ = 4;
}

void Visualizer::initializeDynamicSystems() {
    // Initialize gears
    gears_.clear();
    for (int i = 0; i < 8; ++i) {
        GearNode gear;
        gear.x = 0.0f;
        gear.y = 0.0f;
        gear.radius = 0.1f + i * 0.05f;
        gear.baseRadius = gear.radius;
        gear.angle = 0.0f;
        gear.angularVelocity = 1.0f + i * 0.2f;
        gear.orbitAngle = i * 3.14159f / 4.0f;
        gear.orbitSpeed = 0.5f + i * 0.1f;
        gear.teeth = 8 + i * 2;
        gear.jitterPhase = 0.0f;
        gear.orbitDirection = (i % 2 == 0) ? 1.0f : -1.0f;
        gear.anchorAngle = 0.0f;
        gear.anchorRadius = 0.0f;
        gear.ringIndex = i % 3;
        gear.baseOffsetRadius = 0.0f;
        gear.baseOffsetAngle = 0.0f;
        gear.baseCenterX = 0.0f;
        gear.baseCenterY = 0.0f;
        gears_.push_back(gear);
    }

    // Initialize life grid
    lifeGrid_.cells.fill(0);
    lifeGrid_.next.fill(0);

    // Seed some initial life
    for (int i = 0; i < 100; ++i) {
        int x = std::uniform_int_distribution<int>(0, LifeCellGrid::WIDTH - 1)(rng_);
        int y = std::uniform_int_distribution<int>(0, LifeCellGrid::HEIGHT - 1)(rng_);
        lifeGrid_.cells[y * LifeCellGrid::WIDTH + x] = 1;
    }

    lifeTimeAccumulator_ = 0.0f;
    lastLifeSeedTime_ = 0.0f;
    gearSpawnRadius_ = 0.0f;
    idleState_ = 0.0f;
    idlePhase_ = 0.0f;
}

void Visualizer::renderProceduralLayer() {
    LayerContext context{};
    context.screenWidth = windowWidth_;
    context.screenHeight = windowHeight_;
    context.time = time_;
    context.tempo = tempoMultiplier_;
    context.audio = &audioFeatures_;
    context.intensity = std::clamp(globalIntensityEnvelope_, 0.0f, 1.5f);

    if (!showProceduralLayer_) {
        proceduralLayer_.setEnabled(false);
        return;
    }

    proceduralLayer_.setEnabled(true);
    proceduralLayer_.setDebugPreview(proceduralLayerDebug_);

    auto renderSlot = [&](int mode, float opacity, const std::array<float, 3>& colorAdjust, bool clearFramebuffer = true) {
        float adjustedPrimary[3];
        float adjustedSecondary[3];
        for (int i = 0; i < 3; ++i) {
            adjustedPrimary[i] = std::clamp(scenePrimaryColor_[i] * colorAdjust[i], 0.0f, 1.0f);
            adjustedSecondary[i] = std::clamp(sceneSecondaryColor_[i] * colorAdjust[i], 0.0f, 1.0f);
        }

        // Set camera zoom and offset specific to this slot's mode
        float zoom = getZoomForShaderMode(mode);
        proceduralLayer_.setCameraZoom(zoom);
        // Set custom offset for text marquee modes (58-61), reset for others
        // LUPERFUT (61) has slightly higher offset to move it up
        if (mode >= 58 && mode <= 61) {
            float yOffset = (mode == 61) ? 1.60f : 1.80f;
            proceduralLayer_.setCameraOffset(0.0f, yOffset);
        } else {
            proceduralLayer_.setCameraOffset(0.0f, 0.0f);
        }

        proceduralLayer_.setMode(std::clamp(mode, 0, kProceduralModeCount - 1));
        proceduralLayer_.setColorPalette(adjustedPrimary, adjustedSecondary, scenePaletteBlend_);
        proceduralLayer_.render(context, clearFramebuffer);
        proceduralLayer_.composite(context, std::clamp(opacity, 0.0f, 1.0f));
    };

    bool baseRendered = false;

    if (!proceduralSlots_.empty()) {
        const auto& baseSlot = proceduralSlots_[0];
        if (baseSlot.enabled && baseSlot.opacity > 0.001f) {
            int baseMode = std::clamp(proceduralLayerMode_, 0, kProceduralModeCount - 1);
            const bool alreadyLogged = lastRenderLoggedBaseActive_
                && lastRenderLoggedMode_ == baseMode
                && std::abs(lastRenderLoggedOpacity_ - baseSlot.opacity) < 1e-4f;
            if (!alreadyLogged) {
                std::cout << "[PROC RENDER] Base slot mode=" << baseMode
                          << " opacity=" << baseSlot.opacity << std::endl;
                lastRenderLoggedBaseActive_ = true;
                lastRenderLoggedMode_ = baseMode;
                lastRenderLoggedOpacity_ = baseSlot.opacity;
            }
            if (proceduralSlots_[0].mode != baseMode) {
                std::cout << "[PROC RENDER] Divergence detected: slot0.mode="
                          << proceduralSlots_[0].mode << " but proceduralLayerMode_="
                          << proceduralLayerMode_ << " -> mirroring global into slot0" << std::endl;
                proceduralSlots_[0].mode = baseMode;
            }
            proceduralLayerOpacity_ = baseSlot.opacity;
            renderSlot(baseMode, baseSlot.opacity, baseSlot.colorAdjust);
            baseRendered = true;
        }
    }

    if (!baseRendered) {
        lastRenderLoggedBaseActive_ = false;
        std::array<float, 3> neutralAdjust{1.0f, 1.0f, 1.0f};
        std::cout << "[PROC RENDER] No active base slot, falling back to proceduralLayerMode_="
                  << proceduralLayerMode_ << " opacity=" << proceduralLayerOpacity_ << std::endl;
        renderSlot(proceduralLayerMode_, proceduralLayerOpacity_, neutralAdjust);
    }

    for (size_t i = 1; i < proceduralSlots_.size(); ++i) {
        const auto& slot = proceduralSlots_[i];
        if (!slot.enabled || slot.opacity <= 0.001f) {
            continue;
        }
        std::cout << "[PROC RENDER] Secondary slot " << i
                  << " mode=" << slot.mode
                  << " opacity=" << slot.opacity << std::endl;
        // Slots secundarios no limpian el framebuffer para acumular sobre el slot anterior
        renderSlot(slot.mode, slot.opacity, slot.colorAdjust, false);
    }
}

void Visualizer::renderIdleSpinner(float animatedTime) const {
    // Simple spinning triangle as placeholder
    glUseProgram(0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(windowWidth_ / 2.0f, windowHeight_ / 2.0f, 0.0f);
    glRotatef(animatedTime * 45.0f, 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(0.0f, -50.0f);
    glVertex2f(-43.3f, 25.0f);
    glVertex2f(43.3f, 25.0f);
    glEnd();

    glPopMatrix();
}

void Visualizer::renderGears(float animatedTime) const {
    // Placeholder gear rendering
    glUseProgram(0);
    glMatrixMode(GL_MODELVIEW);

    for (const auto &gear : gears_) {
        glPushMatrix();
        glTranslatef(windowWidth_ / 2.0f + gear.x, windowHeight_ / 2.0f + gear.y, 0.0f);
        glRotatef(gear.angle + animatedTime * gear.angularVelocity * 45.0f, 0.0f, 0.0f, 1.0f);

        glBegin(GL_LINE_LOOP);
        glColor3f(0.8f, 0.8f, 0.8f);
        for (int i = 0; i < gear.teeth; ++i) {
            float angle = 2.0f * 3.14159f * i / gear.teeth;
            float r = (i % 2 == 0) ? gear.radius : gear.radius * 0.8f;
            glVertex2f(cosf(angle) * r * 50.0f, sinf(angle) * r * 50.0f);
        }
        glEnd();

        glPopMatrix();
    }
}

void Visualizer::updateCore(float dt, const AudioAnalyzer::AudioFeatures &features) {
    core_.radius = 0.3f + features.bassEnergy * 0.4f;
    core_.baseRadius = core_.radius;
    core_.pulse = features.onset ? 1.0f : 0.0f;
    core_.energy = features.energy;
    core_.kickEnvelope = features.kick;
    core_.harmonicEnvelope = features.hiHat;
    core_.growthTrend = features.bassShare;
    core_.growthEnvelope = features.midShare;
    core_.maturity = features.highShare;
    core_.idlePulse = std::sin(time_ * 2.0f) * 0.5f + 0.5f;
    core_.idleWarp = std::cos(time_ * 1.5f) * 0.3f + 0.7f;
    core_.idleSpin = time_ * 0.5f;
}

void Visualizer::updateGears(float dt, const AudioAnalyzer::AudioFeatures &features) {
    for (auto &gear : gears_) {
        gear.angle += gear.angularVelocity * dt * (1.0f + features.energy * 2.0f);
        gear.orbitAngle +=
            gear.orbitSpeed * gear.orbitDirection * dt * (1.0f + features.bassEnergy * 1.5f);
        gear.jitterPhase += dt * 3.0f;

        float jitter = std::sin(gear.jitterPhase) * 0.02f * features.energy;
        gear.x = std::cos(gear.orbitAngle) * gearSpawnRadius_ + jitter;
        gear.y = std::sin(gear.orbitAngle) * gearSpawnRadius_ + jitter;
    }

    gearSpawnRadius_ = 50.0f + features.bassEnergy * 100.0f;
}

void Visualizer::updateLife(float dt, const AudioAnalyzer::AudioFeatures &features) {
    lifeTimeAccumulator_ += dt;

    if (lifeTimeAccumulator_ > 0.1f) {
        lifeTimeAccumulator_ = 0.0f;

        // Simple Conway's Game of Life rules
        for (int y = 0; y < LifeCellGrid::HEIGHT; ++y) {
            for (int x = 0; x < LifeCellGrid::WIDTH; ++x) {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0)
                            continue;
                        int nx = (x + dx + LifeCellGrid::WIDTH) % LifeCellGrid::WIDTH;
                        int ny = (y + dy + LifeCellGrid::HEIGHT) % LifeCellGrid::HEIGHT;
                        neighbors += lifeGrid_.cells[ny * LifeCellGrid::WIDTH + nx];
                    }
                }

                int idx = y * LifeCellGrid::WIDTH + x;
                if (lifeGrid_.cells[idx] == 1) {
                    lifeGrid_.next[idx] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
                } else {
                    lifeGrid_.next[idx] = (neighbors == 3) ? 1 : 0;
                }
            }
        }

        lifeGrid_.cells.swap(lifeGrid_.next);
    }

    // Seed new life based on audio
    if (features.onset && time_ - lastLifeSeedTime_ > 0.5f) {
        lastLifeSeedTime_ = time_;
        seedLifeFromCore(10);
    }
}

void Visualizer::seedLifeFromCore(float amount) {
    for (int i = 0; i < static_cast<int>(amount); ++i) {
        int x = LifeCellGrid::WIDTH / 2 + std::uniform_int_distribution<int>(-10, 10)(rng_);
        int y = LifeCellGrid::HEIGHT / 2 + std::uniform_int_distribution<int>(-10, 10)(rng_);
        if (x >= 0 && x < LifeCellGrid::WIDTH && y >= 0 && y < LifeCellGrid::HEIGHT) {
            lifeGrid_.cells[y * LifeCellGrid::WIDTH + x] = 1;
        }
    }
}

float Visualizer::sampleCoreEnergyField(float x, float y) const {
    float dx = x - windowWidth_ / 2.0f;
    float dy = y - windowHeight_ / 2.0f;
    float dist = std::sqrt(dx * dx + dy * dy);
    return std::exp(-dist * dist / (2.0f * core_.radius * core_.radius * 10000.0f)) * core_.energy;
}

void Visualizer::renderLife(float animatedTime) const {
    glUseProgram(0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float cellSize = 5.0f;
    float offsetX = (windowWidth_ - LifeCellGrid::WIDTH * cellSize) / 2.0f;
    float offsetY = (windowHeight_ - LifeCellGrid::HEIGHT * cellSize) / 2.0f;

    glBegin(GL_QUADS);
    for (int y = 0; y < LifeCellGrid::HEIGHT; ++y) {
        for (int x = 0; x < LifeCellGrid::WIDTH; ++x) {
            if (lifeGrid_.cells[y * LifeCellGrid::WIDTH + x] == 1) {
                float px = offsetX + x * cellSize;
                float py = offsetY + y * cellSize;
                glColor3f(0.2f, 0.8f, 0.3f);
                glVertex2f(px, py);
                glVertex2f(px + cellSize, py);
                glVertex2f(px + cellSize, py + cellSize);
                glVertex2f(px, py + cellSize);
            }
        }
    }
    glEnd();

    glPopMatrix();
}

// Random Post Process Methods
void Visualizer::initializeRandomPostProcess() {
    // Initialize available post process modes (exclude "None" and "Random Cycle", and disabled effects)
    availablePostProcessModes_.clear();
    for (int i = 1; i < kPostProcessModeCount - 1; ++i) { // Skip "None"(0) and "Random Cycle"(last)
        // Only add enabled effects
        if (isPostProcessEffectEnabled(i)) {
            availablePostProcessModes_.push_back(i);
        }
    }
    
    randomPostProcessTimer_ = 0.0f;
    
    // Get current mode from Slot 1
    if (kMaxPostProcessSlots > 0) {
        currentRandomPostProcess_ = postProcessSlots_[0].mode;
    } else {
        currentRandomPostProcess_ = 0;
    }
    
    if (!availablePostProcessModes_.empty() && 
        (currentRandomPostProcess_ == 0 || currentRandomPostProcess_ >= kPostProcessModeCount - 1 ||
         !isPostProcessEffectEnabled(currentRandomPostProcess_))) {
        // If current mode is invalid or disabled, select a random one
        std::uniform_int_distribution<int> dist(0, availablePostProcessModes_.size() - 1);
        currentRandomPostProcess_ = availablePostProcessModes_[dist(rng_)];
    }
}

void Visualizer::selectRandomPostProcess() {
    if (availablePostProcessModes_.empty()) return;
    
    std::uniform_int_distribution<int> dist(0, availablePostProcessModes_.size() - 1);
    
    // Randomize the specified number of slots
    int slotsToRandomize = std::clamp(randomPostProcessSlotCount_, 1, kMaxPostProcessSlots);
    
    for (int slotIndex = 0; slotIndex < slotsToRandomize && slotIndex < kMaxPostProcessSlots; ++slotIndex) {
        int newIndex = availablePostProcessModes_[dist(rng_)];
        
        // Avoid selecting the same mode twice in a row for the same slot
        // (compare with current mode in that slot, or global if slot 0)
        int currentMode = (slotIndex == 0) ? currentRandomPostProcess_ : 
                         ((slotIndex < kMaxPostProcessSlots) ? postProcessSlots_[slotIndex].mode : 0);
        
        int attempts = 0;
        while (availablePostProcessModes_.size() > 1 && newIndex == currentMode && attempts < 10) {
            newIndex = availablePostProcessModes_[dist(rng_)];
            ++attempts;
        }
        
        // Update the slot
        postProcessSlots_[slotIndex].mode = newIndex;
        postProcessSlots_[slotIndex].enabled = true; // Ensure slot is enabled
        
        // Update the tracking variable for slot 0
        if (slotIndex == 0) {
            currentRandomPostProcess_ = newIndex;
        }
    }
}

void Visualizer::updateRandomPostProcess(float deltaTime) {
    if (!randomPostProcessEnabled_) return;
    
    randomPostProcessTimer_ += deltaTime;
    
    if (randomPostProcessTimer_ >= randomPostProcessInterval_) {
        selectRandomPostProcess();
        randomPostProcessTimer_ = 0.0f;
    }
}

// Random Procedural Layer Methods
void Visualizer::initializeRandomProcedural() {
    // Initialize available procedural modes (exclude "None" and disabled shaders)
    availableProceduralModes_.clear();
    auto effects = GetEffectRegistry().getAllEffects();
    std::cout << "[RANDOM INIT] Total effects from registry: " << effects.size() << std::endl;
    for (const auto& effect : effects) {
        if (effect.modeIndex <= 0) {
            continue;
        }

        // Check if shader is enabled (default to true if not in map)
        auto it = proceduralShaderEnabled_.find(effect.modeIndex);
        bool enabled = (it == proceduralShaderEnabled_.end()) ? true : it->second;
        std::cout << "[RANDOM INIT] Mode " << effect.modeIndex << " (" << effect.name << "): " << (enabled ? "enabled" : "disabled") << std::endl;
        if (enabled) {
            availableProceduralModes_.push_back(effect.modeIndex);
        }
    }
    
    std::cout << "[RANDOM INIT] Available modes count: " << availableProceduralModes_.size() << std::endl;
    
    randomProceduralTimer_ = 0.0f;
    
    // Get current mode from Slot 1
    if (kMaxProceduralSlots > 0) {
        currentRandomProcedural_ = proceduralSlots_[0].mode;
    } else {
        currentRandomProcedural_ = proceduralLayerMode_;
    }
    
    std::cout << "[RANDOM INIT] Current mode: " << currentRandomProcedural_ << std::endl;
    
    if (!availableProceduralModes_.empty() && currentRandomProcedural_ == 0) {
        // If current mode is invalid, select a random one
        std::uniform_int_distribution<int> dist(0, availableProceduralModes_.size() - 1);
        currentRandomProcedural_ = availableProceduralModes_[dist(rng_)];
        std::cout << "[RANDOM INIT] Selected initial random mode: " << currentRandomProcedural_ << std::endl;
    }
}

void Visualizer::selectRandomProcedural() {
    std::cout << "[RANDOM SELECT] Called. Available modes: " << availableProceduralModes_.size() << std::endl;
    if (availableProceduralModes_.empty()) {
        std::cout << "[RANDOM SELECT] ERROR: No available modes!" << std::endl;
        return;
    }

    // Get slot 2 mode to exclude from randomization
    int slot2Mode = 0;
    if (kMaxProceduralSlots > 1 && proceduralSlots_[1].enabled) {
        slot2Mode = proceduralSlots_[1].mode;
        std::cout << "[RANDOM SELECT] Slot 2 has mode " << slot2Mode << ", excluding from random pool" << std::endl;
    }

    std::uniform_int_distribution<int> dist(0, availableProceduralModes_.size() - 1);
    int newIndex = availableProceduralModes_[dist(rng_)];

    // Avoid selecting the same mode twice in a row, and avoid slot 2's mode
    int attempts = 0;
    while (availableProceduralModes_.size() > 1 && attempts < 20) {
        bool isSameAsCurrent = (newIndex == currentRandomProcedural_);
        bool isSlot2Mode = (slot2Mode > 0 && newIndex == slot2Mode);
        
        if (!isSameAsCurrent && !isSlot2Mode) {
            break; // Found a valid mode
        }
        
        newIndex = availableProceduralModes_[dist(rng_)];
        attempts++;
    }

    currentRandomProcedural_ = newIndex;
    std::cout << "[RANDOM SELECT] Selected mode: " << currentRandomProcedural_ << std::endl;

    // Update Slot 1 (main procedural slot) instead of global mode
    if (kMaxProceduralSlots > 0) {
        applyMainProceduralMode(currentRandomProcedural_, "random-procedural");
    } else {
        std::cout << "[RANDOM SELECT] ERROR: No procedural slots available!" << std::endl;
    }
}

void Visualizer::syncProceduralLayerWithSlot1() {
    if (kMaxProceduralSlots > 0 && proceduralSlots_[0].enabled) {
        // Skip work if nothing changed since the last sync to avoid redundant loops/logs.
        const float opacity = proceduralSlots_[0].opacity;
        const bool alreadySynced = lastSyncedSlot0Enabled_
            && lastSyncedSlot0Mode_ == proceduralLayerMode_
            && std::abs(lastSyncedSlot0Opacity_ - opacity) < 1e-4f;
        if (alreadySynced) {
            return;
        }

        lastSyncedSlot0Enabled_ = true;
        lastSyncedSlot0Mode_ = proceduralLayerMode_;
        lastSyncedSlot0Opacity_ = opacity;

        std::cout << "[SYNC DEBUG] Before sync: proceduralSlots_[0].mode=" << proceduralSlots_[0].mode
                  << " proceduralLayerMode_=" << proceduralLayerMode_ << std::endl;
        proceduralLayerOpacity_ = opacity;
        if (proceduralLayerMode_ != proceduralSlots_[0].mode) {
            std::cout << "[SYNC DEBUG] Divergence detected: slot0.mode=" << proceduralSlots_[0].mode
                      << " but proceduralLayerMode_=" << proceduralLayerMode_
                      << " -> mirroring global into slot0" << std::endl;
            proceduralSlots_[0].mode = proceduralLayerMode_;
        }
        proceduralLayer_.setMode(proceduralLayerMode_);
        proceduralLayer_.setCameraZoom(getZoomForShaderMode(proceduralLayerMode_));
        // Set custom offset for text marquee modes (58-61), reset for others
        if (proceduralLayerMode_ >= 58 && proceduralLayerMode_ <= 61) {
            proceduralLayer_.setCameraOffset(0.0f, 1.80f);
        } else {
            proceduralLayer_.setCameraOffset(0.0f, 0.0f);
        }
        showProceduralLayer_ = true;
        std::cout << "[SYNC DEBUG] After sync: proceduralLayerMode_=" << proceduralLayerMode_ << std::endl;
    } else {
        lastSyncedSlot0Enabled_ = false;
        // Fallback to global values if Slot 1 is disabled
        proceduralLayer_.setMode(proceduralLayerMode_);
    }
}

void Visualizer::updateRandomProcedural(float deltaTime) {
    if (!randomProceduralEnabled_) return;
    
    // Ensure modes are initialized (in case registry wasn't ready at startup)
    if (availableProceduralModes_.empty()) {
        initializeRandomProcedural();
    }
    
    randomProceduralTimer_ += deltaTime;
    
    if (randomProceduralTimer_ >= randomProceduralInterval_) {
        std::cout << "[RANDOM DEBUG] Selecting random procedural (current=" << currentRandomProcedural_ << ")" << std::endl;
        selectRandomProcedural();
        std::cout << "[RANDOM DEBUG] New random mode=" << currentRandomProcedural_ << std::endl;
        randomProceduralTimer_ = 0.0f;
    }
}

void Visualizer::updateRandomCornerOrbs(float deltaTime) {
    if (!randomCornerOrbsEnabled_) return;
    
    randomCornerOrbsTimer_ += deltaTime;
    
    if (randomCornerOrbsTimer_ >= randomCornerOrbsInterval_) {
        showCornerOrbs_ = (rand() % 2) == 1;
        saveCurrentSettings();
        std::cout << "[RANDOM] Corner Orbs: " << (showCornerOrbs_ ? "ENABLED" : "DISABLED") << std::endl;
        randomCornerOrbsTimer_ = 0.0f;
    }
}

// MIDI Implementation
bool Visualizer::initializeMIDI() {
    std::cout << "[MIDI DEBUG] Initializing MIDI in Visualizer..." << std::endl;
    
    if (midiController_->initialize()) {
        setupMIDIMappings();
        midiEnabled_ = true;
        std::cout << "[MIDI DEBUG] MIDI controller initialized successfully" << std::endl;
        
        // Auto-connect all available MIDI devices
        midiController_->connectAllDevices();
        
        if (midiController_->start()) {
            std::cout << "[MIDI DEBUG] MIDI processing started successfully" << std::endl;
        } else {
            std::cout << "[MIDI DEBUG] Failed to start MIDI processing" << std::endl;
        }
        
        return true;
    } else {
        std::cout << "[MIDI DEBUG] Failed to initialize MIDI controller" << std::endl;
        midiEnabled_ = false;
        return false;
    }
}

void Visualizer::shutdownMIDI() {
    if (midiEnabled_ && midiController_) {
        midiController_->stop();
        midiController_->shutdown();
        midiEnabled_ = false;
        std::cout << "MIDI controller shutdown" << std::endl;
    }
}

void Visualizer::setupMIDIMappings() {
    if (!midiController_) return;

    auto updatePostProcessState = [this]() {
        bool anyActive = false;
        for (const auto& slot : postProcessSlots_) {
            if (slot.enabled && slot.mode > 0 && slot.strength > 0.001f) {
                anyActive = true;
                break;
            }
        }
        showPostProcess_ = anyActive;
    };
    
    // CC 1-16: Common parameters
    midiController_->bindControl(1, [this](float value) {
        visualSensitivity_ = 0.2f + value * 2.8f; // 0.2 to 3.0
    });
    
    midiController_->bindControl(2, [this](float value) {
        audioInputGain_ = 0.1f + value * 4.9f; // 0.1 to 5.0
    });
    
    midiController_->bindControl(3, [this](float value) {
        proceduralLayerOpacity_ = value;
    });
    
    midiController_->bindControl(4, [this](float value) {
        postProcessStrength_ = value;
    });
    
    // CC 5-8: Color controls
    midiController_->bindControl(5, [this](float value) {
        scenePrimaryColor_[0] = value; // Red
    });
    
    midiController_->bindControl(6, [this](float value) {
        scenePrimaryColor_[1] = value; // Green
    });
    
    midiController_->bindControl(7, [this](float value) {
        scenePrimaryColor_[2] = value; // Blue
    });
    
    midiController_->bindControl(8, [this](float value) {
        scenePaletteBlend_ = value;
    });
    
    // CC 9-12: Post-processing slots
    for (int i = 0; i < 4; i++) {
        midiController_->bindControl(9 + i, [this, i](float value) {
            if (i < postProcessSlots_.size()) {
                postProcessSlots_[i].strength = value;
            }
        });
    }
    
    // CC 13-16: Procedural slots
    for (int i = 0; i < 4; i++) {
        midiController_->bindControl(13 + i, [this, i](float value) {
            if (i < proceduralSlots_.size()) {
                proceduralSlots_[i].opacity = value;
            }
        });
    }
    
    // CC 17: Manual BPM
    midiController_->bindControl(17, [this](float value) {
        manualBPM_ = 5.0f + value * 195.0f; // 5 to 200 BPM
    });
    
    // CC 18: Toggle manual BPM mode
    midiController_->bindControl(18, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            manualBPMMode_ = !manualBPMMode_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(19, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            showProceduralLayer_ = !showProceduralLayer_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(71, [this](float value) {
        scenePaletteBlend_ = std::clamp(value, 0.0f, 1.0f);
        proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
    });

    midiController_->bindControl(72, [this](float value) {
        proceduralLayerOpacity_ = value;
        if (!proceduralSlots_.empty()) {
            proceduralSlots_[0].opacity = value;
            proceduralSlots_[0].enabled = value > 0.01f;
        }
        showProceduralLayer_ = value > 0.01f;
    });

    midiController_->bindControl(73, [this](float value) {
        randomPostProcessInterval_ = 2.0f + value * 28.0f; // 2s - 30s
    });

    midiController_->bindControl(74, [this](float value) {
        currentScenePaletteIndex_ = -1;
        scenePaletteHueSeed_ = std::clamp(value, 0.0f, 1.0f);
        setDefaultScenePalette();
    });

    midiController_->bindControl(75, [this](float value) {
        randomProceduralInterval_ = 2.0f + value * 28.0f; // 2s - 30s
    });

    midiController_->bindControl(79, [this](float value) {
        colorRandomInterval_ = 2.0f + value * 28.0f; // 2s - 30s
    });

    auto bindPostProcessSlot = [this, updatePostProcessState](int cc, int slotIndex) {
        midiController_->bindControl(cc, [this, slotIndex, updatePostProcessState](float value) mutable {
            if (slotIndex < static_cast<int>(postProcessSlots_.size())) {
                auto& slot = postProcessSlots_[slotIndex];
                slot.strength = value;
                slot.enabled = value > 0.01f;
                updatePostProcessState();
            }
        });
    };

    bindPostProcessSlot(76, 0);
    bindPostProcessSlot(77, 1);
    bindPostProcessSlot(91, 2);
    bindPostProcessSlot(93, 3);

    midiController_->bindControl(22, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            coreShowBloom_ = !coreShowBloom_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(23, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            coreShowSparkles_ = !coreShowSparkles_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(24, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            coreShowRunes_ = !coreShowRunes_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(25, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            coreShowSpokes_ = !coreShowSpokes_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(26, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            showCornerOrbs_ = !showCornerOrbs_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(27, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            showProceduralLayer_ = !showProceduralLayer_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(28, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            randomizeScenePalette();
        }
        lastState = currentState;
    });

    midiController_->bindControl(29, [this, updatePostProcessState](float value) mutable {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            selectRandomPostProcess();
            updatePostProcessState();
        }
        lastState = currentState;
    });

    midiController_->bindControl(113, [this](float value) {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            showWaveformOverlay_ = !showWaveformOverlay_;
        }
        lastState = currentState;
    });

    midiController_->bindControl(115, [this, updatePostProcessState](float value) mutable {
        static bool lastState = false;
        bool currentState = value > 0.5f;
        if (currentState && !lastState) {
            showPostProcess_ = !showPostProcess_;
            updatePostProcessState();
        }
        lastState = currentState;
    });

    std::cout << "MIDI mappings configured" << std::endl;
}

void Visualizer::renderMIDIControls() {
    // Get current time for this frame
    auto now = std::chrono::steady_clock::now();
    
    if (!ImGui::CollapsingHeader("🎛️ MIDI Controls")) return;
    
    // Static variables for MIDI learning and monitoring
    static int lastMovedCC = -1;
    static float lastMovedValue = 0.0f;
    static std::chrono::steady_clock::time_point lastMoveTime;
    static std::set<int> recentlyUsedCCs;
    
    // MIDI Input Monitor (always visible at top)
    if (ImGui::CollapsingHeader("📡 Real-time MIDI Input Monitor", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Show connection status
        bool anyActive = false;
        for (int cc = 1; cc <= 127; cc++) {
            if (midiController_->isControlActive(cc)) {
                anyActive = true;
                break;
            }
        }
        
        if (anyActive) {
            ImGui::Text("🟢 MIDI Status: Connected and Receiving Input");
        } else {
            ImGui::Text("🔴 MIDI Status: No Input Detected");
        }
        
        ImGui::Separator();
        ImGui::Text("Live MIDI Input Stream:");
        
        // Create a scrolling buffer for recent MIDI messages
        static std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> midiMessages;
        static const int MAX_MESSAGES = 50;
        
        // Add new messages from the learning system
        for (int cc = 1; cc <= 127; cc++) {
            float currentValue = midiController_->getControlValue(cc);
            static std::vector<float> previousValues(128, -1.0f);
            
            if (previousValues[cc] >= 0.0f && std::abs(currentValue - previousValues[cc]) > 0.01f) {
                lastMovedCC = cc;
                lastMovedValue = currentValue;
                lastMoveTime = now;
                
                // Add to message buffer
                std::string msg = "CC " + std::to_string(cc) + " → " + 
                                 std::to_string(static_cast<int>(currentValue * 127)) + 
                                 " (" + std::to_string(currentValue).substr(0, 4) + ")";
                
                midiMessages.push_back({msg, now});
                
                // Keep only recent messages
                while (midiMessages.size() > MAX_MESSAGES) {
                    midiMessages.erase(midiMessages.begin());
                }
            }
            previousValues[cc] = currentValue;
        }
        
        // Display messages with color coding
        ImGui::BeginChild("MIDIMessages", ImVec2(0, 120), true);
        for (const auto& msgPair : midiMessages) {
            auto age = now - msgPair.second;
            float alpha = std::max(0.0f, 1.0f - std::chrono::duration<float>(age).count() / 5.0f); // Fade over 5 seconds
            
            if (alpha > 0.01f) {
                ImVec4 color = ImVec4(0.0f, 1.0f, 0.0f, alpha); // Green with fade
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::Text("%s", msgPair.first.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
        
        // Show current values for first 16 CC controls
        ImGui::Text("Current CC Values (1-16):");
        for (int cc = 1; cc <= 16; cc++) {
            float value = midiController_->getControlValue(cc);
            bool active = midiController_->isControlActive(cc);
            
            if (active || value > 0.01f) {
                ImVec4 color = value > 0.5f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::Text("CC %2d: %.3f %s", cc, value, active ? "[✓]" : "[ ]");
                ImGui::PopStyleColor();
            } else {
                ImGui::Text("CC %2d: %.3f [ ]", cc, value);
            }
        }
        
        // Show current activity status
        if ((now - lastMoveTime) < std::chrono::milliseconds(500)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            ImGui::Text("🟢 MIDI ACTIVE - Receiving data");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
            ImGui::Text("🟡 MIDI IDLE - No recent input");
            ImGui::PopStyleColor();
        }
        
        ImGui::Separator();
    }
    
    // MIDI Device Selector
    if (ImGui::CollapsingHeader("🎹 MIDI Device Selection")) {
        ImGui::Text("MIDI Status: %s", midiEnabled_ ? "Connected" : "Disconnected");
        
        // Get available devices
        static std::vector<std::string> midiDevices;
        static int selectedDevice = -1;
        static bool devicesLoaded = false;
        
        if (!devicesLoaded && midiController_) {
            midiDevices = midiController_->getAvailableDevices();
            devicesLoaded = true;
            
            // Find current device
            for (size_t i = 0; i < midiDevices.size(); i++) {
                if (midiDevices[i].find("Arturia") != std::string::npos) {
                    selectedDevice = i;
                    break;
                }
            }
        }
        
        // Device selection combo
        if (!midiDevices.empty()) {
            const char* preview = (selectedDevice >= 0 && selectedDevice < midiDevices.size()) 
                                ? midiDevices[selectedDevice].c_str() 
                                : "Select MIDI Device...";
            
            if (ImGui::BeginCombo("MIDI Device", preview)) {
                for (int i = 0; i < midiDevices.size(); i++) {
                    bool isSelected = (selectedDevice == i);
                    if (ImGui::Selectable(midiDevices[i].c_str(), isSelected)) {
                        selectedDevice = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Connect")) {
                if (selectedDevice >= 0 && selectedDevice < midiDevices.size()) {
                    std::cout << "[MIDI DEBUG] Connecting to device: " << midiDevices[selectedDevice] << std::endl;
                    shutdownMIDI();
                    initializeMIDI();
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Refresh")) {
                devicesLoaded = false;
                midiDevices.clear();
                selectedDevice = -1;
                if (midiController_) {
                    midiDevices = midiController_->getAvailableDevices();
                    devicesLoaded = true;
                }
            }
        } else {
            ImGui::Text("No MIDI devices found");
            if (ImGui::Button("Scan for Devices")) {
                devicesLoaded = false;
                midiDevices.clear();
                if (midiController_) {
                    midiDevices = midiController_->getAvailableDevices();
                    devicesLoaded = true;
                }
            }
        }
        
        ImGui::Separator();
    }
    
    if (midiEnabled_ && midiController_) {
        // MIDI Learning Tool
        if (ImGui::CollapsingHeader("🎓 MIDI Learning Tool")) {
            ImGui::Text("Move any physical control to identify its CC number:");
            ImGui::Separator();
            
            // Show last moved control with highlight
            if (lastMovedCC >= 0 && (now - lastMoveTime) < std::chrono::seconds(3)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                ImGui::Text("🎯 LAST MOVED: CC %d = %.3f", lastMovedCC, lastMovedValue);
                ImGui::PopStyleColor();
                
                // Show what this CC controls
                const char* mapping = getMIDIMappingName(lastMovedCC);
                if (mapping) {
                    ImGui::Text("📋 Mapped to: %s", mapping);
                } else {
                    ImGui::Text("⚠️  Not mapped to any parameter");
                }
                
                ImGui::Separator();
            }
            
            // MiniLab mkII Physical Layout Reference
            ImGui::Text("🎹 Arturia MiniLab mkII Layout Reference:");
            ImGui::Text("Knobs (top row): CC 1-8");
            ImGui::Text("Knobs (bottom row): CC 9-16");
            ImGui::Text("Pads: CC 20-27 (typically)");
            ImGui::Text("Transport buttons: Various CCs");
            
            ImGui::Separator();
            
            // Create a visual map of recently used CCs
            if (lastMovedCC >= 0) {
                recentlyUsedCCs.insert(lastMovedCC);
                // Keep only last 20
                if (recentlyUsedCCs.size() > 20) {
                    recentlyUsedCCs.erase(recentlyUsedCCs.begin());
                }
            }
            
            if (!recentlyUsedCCs.empty()) {
                ImGui::Text("📊 Recently Used Controls:");
                for (int cc : recentlyUsedCCs) {
                    float value = midiController_->getControlValue(cc);
                    const char* mapping = getMIDIMappingName(cc);
                    
                    ImGui::PushID(cc);
                    ImGui::BeginGroup();
                    
                    // Color code based on whether it's mapped
                    ImVec4 color = mapping ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    
                    ImGui::Text("CC %02d: %.3f", cc, value);
                    if (mapping) {
                        ImGui::Text("  → %s", mapping);
                    } else {
                        ImGui::Text("  → Unmapped");
                    }
                    
                    ImGui::PopStyleColor();
                    ImGui::EndGroup();
                    
                    // Click to test this CC
                    if (ImGui::IsItemClicked()) {
                        std::cout << "[MIDI TEST] Testing CC " << cc << " - Current value: " << value << std::endl;
                    }
                    
                    ImGui::PopID();
                }
            }
            
            ImGui::Separator();
            if (ImGui::Button("Clear Learning History")) {
                recentlyUsedCCs.clear();
                lastMovedCC = -1;
            }
        }
        
        // MIDI Mapper Section
        if (ImGui::CollapsingHeader("🎹 MIDI Mapper")) {
            ImGui::Text("Move MIDI controls to see their values:");
            ImGui::Text("Current CC Values:");
            
            // Create a grid of CC values for easy visualization
            const int cols = 8;
            for (int cc = 1; cc <= 24; cc++) {
                float value = midiController_->getControlValue(cc);
                bool isActive = midiController_->isControlActive(cc);
                
                // Highlight recently moved CC
                bool isRecentlyMoved = (cc == lastMovedCC) && 
                    ((std::chrono::steady_clock::now() - lastMoveTime) < std::chrono::seconds(2));
                
                // Color based on value and recent movement
                ImVec4 color = ImVec4(value, value * 0.5f, 1.0f - value, 1.0f);
                if (isRecentlyMoved) {
                    color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Bright green for recently moved
                } else if (isActive) {
                    color.x = 0.2f;
                    color.y = 0.8f;
                    color.z = 0.2f;
                }
                
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                
                if ((cc - 1) % cols != 0) {
                    ImGui::SameLine();
                }
                
                ImGui::BeginGroup();
                ImGui::Text("CC%02d", cc);
                ImGui::ProgressBar(value, ImVec2(40, 8));
                ImGui::Text("%.2f", value);
                ImGui::EndGroup();
                
                ImGui::PopStyleColor();
                
                // Tooltip with mapping info
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Control Change %d", cc);
                    ImGui::Text("Value: %.3f", value);
                    ImGui::Text("Status: %s", isActive ? "Mapped" : "Unmapped");
                    
                    // Show what this CC controls
                    const char* mapping = getMIDIMappingName(cc);
                    if (mapping) {
                        ImGui::Text("Mapped to: %s", mapping);
                    }
                    ImGui::EndTooltip();
                }
            }
        }
        
        ImGui::Separator();
        ImGui::Text("Quick Status:");
        
        // Show some key control values in a more compact way
        ImGui::Columns(4, "MIDIValues");
        ImGui::Text("CC 1"); ImGui::NextColumn();
        ImGui::Text("CC 2"); ImGui::NextColumn();
        ImGui::Text("CC 3"); ImGui::NextColumn();
        ImGui::Text("CC 4"); ImGui::NextColumn();
        
        ImGui::Text("%.2f", midiController_->getControlValue(1)); ImGui::NextColumn();
        ImGui::Text("%.2f", midiController_->getControlValue(2)); ImGui::NextColumn();
        ImGui::Text("%.2f", midiController_->getControlValue(3)); ImGui::NextColumn();
        ImGui::Text("%.2f", midiController_->getControlValue(4)); ImGui::NextColumn();
        ImGui::Separator();
        
        ImGui::Text("CC 5-7 (RGB)"); ImGui::NextColumn();
        ImGui::Text("CC 8"); ImGui::NextColumn();
        ImGui::Text("CC 17"); ImGui::NextColumn();
        ImGui::Text("CC 18"); ImGui::NextColumn();
        
        ImGui::Text("%.2f,%.2f,%.2f", 
                   midiController_->getControlValue(5),
                   midiController_->getControlValue(6),
                   midiController_->getControlValue(7)); ImGui::NextColumn();
        ImGui::Text("%.2f", midiController_->getControlValue(8)); ImGui::NextColumn();
        ImGui::Text("%.0f", 60.0f + midiController_->getControlValue(17) * 140.0f); ImGui::NextColumn();
        ImGui::Text("%s", midiController_->getControlValue(18) > 0.5f ? "ON" : "OFF"); ImGui::NextColumn();
        ImGui::Columns(1);
        
        ImGui::Separator();
        ImGui::Text("MIDI Control Mapping:");
        ImGui::Text("CC 1: Visual Sensitivity");
        ImGui::Text("CC 2: Audio Input Gain");
        ImGui::Text("CC 3: Procedural Layer Opacity");
        ImGui::Text("CC 4: Post Process Strength");
        ImGui::Text("CC 5-7: Primary Color (RGB)");
        ImGui::Text("CC 8: Color Palette Blend");
        ImGui::Text("CC 9-12: Post Process Slots Strength 1-4");
        ImGui::Text("CC 13-16: Procedural Slots Opacity 1-4");
        ImGui::Text("CC 17: Manual BPM (60-200)");
        ImGui::Text("CC 18: Toggle Manual BPM Mode");
        ImGui::Text("CC 19: Toggle Procedural Layer");
        ImGui::Text("CC 22: Toggle Core Bloom");
        ImGui::Text("CC 23: Toggle Core Sparkles");
        ImGui::Text("CC 24: Toggle Core Runes");
        ImGui::Text("CC 25: Toggle Core Spokes");
        ImGui::Text("CC 26: Toggle Corner Orbs");
        ImGui::Text("CC 27: Toggle Procedural Layer Visibility");
        ImGui::Text("CC 28: Randomize Scene Palette");
        ImGui::Text("CC 29: Random Post Process Effect");
        ImGui::Text("CC 71: Scene Palette Blend");
        ImGui::Text("CC 72: Procedural Layer Opacity (live)");
        ImGui::Text("CC 73: Random Post Process Interval");
        ImGui::Text("CC 74: Scene Palette Hue Seed");
        ImGui::Text("CC 75: Random Procedural Interval");
        ImGui::Text("CC 76-77: Post Process Slots Strength 1-2");
        ImGui::Text("CC 79: Color Random Interval");
        ImGui::Text("CC 91: Post Process Slot Strength 3");
        ImGui::Text("CC 93: Post Process Slot Strength 4");
        ImGui::Text("CC 113: Toggle Waveform Overlay");
        ImGui::Text("CC 115: Toggle Post Processing");
    }
}

// Helper function to get MIDI mapping names
const char* Visualizer::getMIDIMappingName(int cc) {
    switch (cc) {
        case 1: return "Visual Sensitivity";
        case 2: return "Audio Input Gain";
        case 3: return "Procedural Layer Opacity";
        case 4: return "Post Process Strength";
        case 5: return "Primary Color (Red)";
        case 6: return "Primary Color (Green)";
        case 7: return "Primary Color (Blue)";
        case 8: return "Color Palette Blend";
        case 9: return "Post Process Slot 1";
        case 10: return "Post Process Slot 2";
        case 11: return "Post Process Slot 3";
        case 12: return "Post Process Slot 4";
        case 13: return "Procedural Slot 1";
        case 14: return "Procedural Slot 2";
        case 15: return "Procedural Slot 3";
        case 16: return "Procedural Slot 4";
        case 17: return "Manual BPM";
        case 18: return "Toggle Manual BPM";
        case 19: return "Toggle Procedural Layer";
        case 22: return "Toggle Core Bloom";
        case 23: return "Toggle Core Sparkles";
        case 24: return "Toggle Core Runes";
        case 25: return "Toggle Core Spokes";
        case 26: return "Toggle Corner Orbs";
        case 27: return "Toggle Procedural Layer Visibility";
        case 28: return "Randomize Scene Palette";
        case 29: return "Random Post Process Effect";
        case 71: return "Scene Palette Blend";
        case 72: return "Procedural Layer Opacity";
        case 73: return "Random Post Process Interval";
        case 74: return "Scene Palette Hue Seed";
        case 75: return "Random Procedural Interval";
        case 76: return "Post Process Slot 1 Strength";
        case 77: return "Post Process Slot 2 Strength";
        case 79: return "Color Random Interval";
        case 91: return "Post Process Slot 3 Strength";
        case 93: return "Post Process Slot 4 Strength";
        case 113: return "Toggle Waveform Overlay";
        case 115: return "Toggle Post Processing";
        default: return nullptr;
    }
}

void Visualizer::reloadProceduralShaders() {
    std::cout << "Reloading procedural shaders..." << std::endl;
    proceduralLayer_.reloadShaders();
    std::cout << "Shaders reloaded successfully!" << std::endl;
}

void Visualizer::applyMainProceduralMode(int mode, const char* source, bool ensureVisible, bool updateZoom) {
    const int clampedMode = std::clamp(mode, 0, kProceduralModeCount - 1);
    const char* origin = (source && source[0] != '\0') ? source : "unspecified";

    lastProceduralModeSource_ = origin;
    lastProceduralModeRequested_ = mode;
    lastProceduralModeApplied_ = clampedMode;

    const int previousSlotMode = (kMaxProceduralSlots > 0) ? proceduralSlots_[0].mode : -1;
    const int previousLayerMode = proceduralLayerMode_;

    std::cout << "[PROC MODE APPLY] source=" << origin
              << " requested=" << mode
              << " clamped=" << clampedMode
              << " previousSlot0=" << previousSlotMode
              << " previousLayer=" << previousLayerMode << std::endl;

    if (kMaxProceduralSlots > 0) {
        proceduralSlots_[0].mode = clampedMode;
        proceduralSlots_[0].enabled = true;
    }

    proceduralLayerMode_ = clampedMode;
    proceduralLayer_.setMode(clampedMode);

    if (updateZoom) {
        float zoom = getZoomForShaderMode(clampedMode);
        proceduralLayer_.setCameraZoom(zoom);
    }
    // Set custom offset for text marquee modes (58-61), reset for others
    if (clampedMode >= 58 && clampedMode <= 61) {
        proceduralLayer_.setCameraOffset(0.0f, 1.80f);
    } else {
        proceduralLayer_.setCameraOffset(0.0f, 0.0f);
    }

    if (ensureVisible) {
        showProceduralLayer_ = true;
    }

    std::cout << "[PROC MODE APPLY] source=" << origin
              << " appliedSlot0=" << ((kMaxProceduralSlots > 0) ? proceduralSlots_[0].mode : -1)
              << " appliedLayer=" << proceduralLayerMode_
              << " visible=" << showProceduralLayer_
              << " zoom=" << (updateZoom ? "updated" : "unchanged") << std::endl;
}

bool Visualizer::isProceduralShaderEnabled(int modeIndex) const {
    auto it = proceduralShaderEnabled_.find(modeIndex);
    if (it != proceduralShaderEnabled_.end()) {
        return it->second;
    }
    return true; // Default to enabled if not explicitly set
}

void Visualizer::setProceduralShaderEnabled(int modeIndex, bool enabled) {
    proceduralShaderEnabled_[modeIndex] = enabled;
    
    // If disabling the current mode, check if we need to switch to None
    if (!enabled) {
        int currentMode = proceduralSlots_[0].mode;
        if (currentMode == modeIndex) {
            // Current mode is being disabled, switch to None (0)
            applyMainProceduralMode(0, "disable-current-procedural-shader");
            std::cout << "[SHADER] Current mode " << modeIndex << " disabled, switching to None" << std::endl;
        }
    }
}

int Visualizer::findNextEnabledMode(int currentMode, bool forward) const {
    std::vector<int> enabledModes;
    auto effects = GetEffectRegistry().getAllEffects();
    for (const auto& effect : effects) {
        if (effect.modeIndex <= 0) {
            continue;
        }
        if (isProceduralShaderEnabled(effect.modeIndex)) {
            enabledModes.push_back(effect.modeIndex);
        }
    }

    if (enabledModes.empty()) {
        return 0; // None
    }

    auto it = std::find(enabledModes.begin(), enabledModes.end(), currentMode);
    if (it == enabledModes.end()) {
        return forward ? enabledModes.front() : enabledModes.back();
    }

    size_t index = static_cast<size_t>(std::distance(enabledModes.begin(), it));
    if (forward) {
        index = (index + 1) % enabledModes.size();
    } else {
        index = (index + enabledModes.size() - 1) % enabledModes.size();
    }

    return enabledModes[index];

    // Fallback to None if no enabled shader found
    return 0;
}

// Post-processing effect enable/disable methods
bool Visualizer::isPostProcessEffectEnabled(int modeIndex) const {
    // Check local state first, fall back to settings manager
    auto it = postProcessEffectEnabled_.find(modeIndex);
    if (it != postProcessEffectEnabled_.end()) {
        return it->second;
    }
    // Default to enabled if not in map
    return true;
}

void Visualizer::setPostProcessEffectEnabled(int modeIndex, bool enabled) {
    postProcessEffectEnabled_[modeIndex] = enabled;
    
    // If disabling the current mode in slot 0, switch to None
    if (!enabled) {
        if (postProcessSlots_[0].mode == modeIndex) {
            postProcessSlots_[0].mode = 0;
            saveCurrentSettings();
        }
    }
}

// Multi-monitor support implementation
void Visualizer::detectMonitors() {
    monitors_.clear();
    monitorNames_.clear();
    
    int count;
    GLFWmonitor** glfwMonitors = glfwGetMonitors(&count);
    
    if (!glfwMonitors || count == 0) {
        std::cerr << "[MONITOR] No monitors detected" << std::endl;
        return;
    }
    
    std::cout << "[MONITOR] Detected " << count << " monitor(s):" << std::endl;
    
    for (int i = 0; i < count; ++i) {
        GLFWmonitor* monitor = glfwMonitors[i];
        monitors_.push_back(monitor);
        
        const char* name = glfwGetMonitorName(monitor);
        monitorNames_.push_back(name ? name : "Unknown");
        
        int x, y;
        glfwGetMonitorPos(monitor, &x, &y);
        
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        int width = mode ? mode->width : 0;
        int height = mode ? mode->height : 0;
        
        std::cout << "  [" << i << "] " << monitorNames_.back() 
                  << " @ (" << x << "," << y << ") " 
                  << width << "x" << height << std::endl;
    }
    
    // Find which monitor the window is currently on
    if (window_) {
        int wx, wy;
        glfwGetWindowPos(window_, &wx, &wy);
        
        for (size_t i = 0; i < monitors_.size(); ++i) {
            int mx, my;
            glfwGetMonitorPos(monitors_[i], &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors_[i]);
            
            if (wx >= mx && wx < mx + mode->width &&
                wy >= my && wy < my + mode->height) {
                currentMonitorIndex_ = static_cast<int>(i);
                break;
            }
        }
    }
    
    std::cout << "[MONITOR] Current monitor index: " << currentMonitorIndex_ << std::endl;
}

void Visualizer::moveToMonitor(int monitorIndex) {
    if (!window_ || monitorIndex < 0 || monitorIndex >= static_cast<int>(monitors_.size())) {
        std::cerr << "[MONITOR] Invalid monitor index: " << monitorIndex << std::endl;
        return;
    }
    
    GLFWmonitor* target = monitors_[monitorIndex];
    int mx, my;
    glfwGetMonitorPos(target, &mx, &my);
    const GLFWvidmode* mode = glfwGetVideoMode(target);
    
    if (!mode) {
        std::cerr << "[MONITOR] Failed to get video mode for monitor " << monitorIndex << std::endl;
        return;
    }
    
    std::cout << "[MONITOR] Moving window to monitor " << monitorIndex 
              << " (" << monitorNames_[monitorIndex] << ")" << std::endl;
    
    // For Hyprland, we set windowed mode first, then position
    // The compositor will handle the actual placement
    glfwSetWindowMonitor(window_, nullptr, mx + 50, my + 50, 
                         windowWidth_, windowHeight_, 0);
    
    currentMonitorIndex_ = monitorIndex;
    
    // Position ImGui window relative to main window
    if (imguiWindow_ && !multiMonitorMode_) {
        glfwSetWindowPos(imguiWindow_, mx + windowWidth_ + 100, my + 50);
    }
}

void Visualizer::toggleMultiMonitorMode() {
    multiMonitorMode_ = !multiMonitorMode_;
    
    if (monitors_.size() < 2) {
        std::cout << "[MONITOR] Multi-monitor mode requires 2+ monitors" << std::endl;
        multiMonitorMode_ = false;
        return;
    }
    
    if (multiMonitorMode_) {
        // Enable multi-monitor: render on external (last monitor), controls on primary
        std::cout << "[MONITOR] Enabling multi-monitor mode" << std::endl;
        
        // Move main window to external monitor (last one)
        int externalIndex = static_cast<int>(monitors_.size()) - 1;
        moveToMonitor(externalIndex);
        
        // Optionally fullscreen the external monitor window
        GLFWmonitor* external = monitors_[externalIndex];
        const GLFWvidmode* mode = glfwGetVideoMode(external);
        
        // Store window size before fullscreen
        windowWidth_ = mode->width;
        windowHeight_ = mode->height;
        
        glfwSetWindowMonitor(window_, external, 0, 0, mode->width, mode->height, mode->refreshRate);
        
        // Position ImGui window on primary monitor
        if (imguiWindow_) {
            int px, py;
            glfwGetMonitorPos(monitors_[0], &px, &py);
            const GLFWvidmode* primaryMode = glfwGetVideoMode(monitors_[0]);
            
            // Center ImGui window on primary monitor
            int imguiX = px + (primaryMode->width - imguiWindowWidth_) / 2;
            int imguiY = py + (primaryMode->height - imguiWindowHeight_) / 2;
            glfwSetWindowPos(imguiWindow_, imguiX, imguiY);
            glfwShowWindow(imguiWindow_);
        }
        
        std::cout << "[MONITOR] Render on external, controls on primary" << std::endl;
    } else {
        // Disable multi-monitor: return to single window mode
        std::cout << "[MONITOR] Disabling multi-monitor mode" << std::endl;
        
        // Exit fullscreen if in it
        GLFWmonitor* monitor = glfwGetWindowMonitor(window_);
        if (monitor) {
            glfwSetWindowMonitor(window_, nullptr, 100, 100, 1280, 720, 0);
        }
        
        // Move both windows to primary monitor
        if (!monitors_.empty()) {
            moveToMonitor(0);
        }
    }
}

void Visualizer::renderMonitorSelector() {
    if (monitors_.empty()) {
        detectMonitors();
    }
    
    if (ImGui::CollapsingHeader("🖥️ Multi-Monitor Setup")) {
        ImGui::Text("Detected %zu monitor(s):", monitors_.size());
        
        for (size_t i = 0; i < monitors_.size(); ++i) {
            bool isCurrent = (currentMonitorIndex_ == static_cast<int>(i));
            bool isSelected = ImGui::RadioButton(
                (std::to_string(i) + ": " + monitorNames_[i]).c_str(), 
                &currentMonitorIndex_, static_cast<int>(i));
            
            if (isSelected && !isCurrent) {
                moveToMonitor(static_cast<int>(i));
            }
        }
        
        ImGui::Separator();
        
        if (monitors_.size() >= 2) {
            const char* modeLabel = multiMonitorMode_ ? "Disable" : "Enable";
            if (ImGui::Button((std::string(modeLabel) + " Multi-Monitor Mode").c_str())) {
                toggleMultiMonitorMode();
            }
            
            if (multiMonitorMode_) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), 
                    "Active: Render on external, controls on laptop");
            }
        } else {
            ImGui::TextDisabled("Connect 2+ monitors for multi-display mode");
        }
        
        ImGui::Separator();
        ImGui::TextWrapped("Tip: Press 'M' to toggle multi-monitor mode quickly");
    }
}

// Fullscreen support implementations
void Visualizer::toggleMainWindowFullscreen() {
    if (!window_) return;

    GLFWmonitor* currentMonitor = glfwGetWindowMonitor(window_);
    if (currentMonitor) {
        // Currently fullscreen - switch to windowed
        glfwSetWindowMonitor(window_, nullptr, 
                             windowedPosX_, windowedPosY_, 
                             windowedWidth_, windowedHeight_, 0);
        std::cout << "[FULLSCREEN] Main window: switched to windowed mode" << std::endl;
    } else {
        // Currently windowed - save position/size and go fullscreen
        glfwGetWindowPos(window_, &windowedPosX_, &windowedPosY_);
        glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
        
        // Get the monitor the window is currently on
        int wx, wy;
        glfwGetWindowPos(window_, &wx, &wy);
        
        GLFWmonitor* targetMonitor = nullptr;
        for (size_t i = 0; i < monitors_.size(); ++i) {
            int mx, my;
            glfwGetMonitorPos(monitors_[i], &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors_[i]);
            if (wx >= mx && wx < mx + mode->width && wy >= my && wy < my + mode->height) {
                targetMonitor = monitors_[i];
                break;
            }
        }
        
        // Fallback to primary monitor if not found
        if (!targetMonitor) {
            targetMonitor = glfwGetPrimaryMonitor();
        }
        
        if (targetMonitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
            glfwSetWindowMonitor(window_, targetMonitor, 0, 0, 
                                 mode->width, mode->height, mode->refreshRate);
            std::cout << "[FULLSCREEN] Main window: switched to fullscreen on monitor" << std::endl;
        }
    }
}

void Visualizer::toggleImGuiWindowFullscreen() {
    if (!imguiWindow_) return;

    GLFWmonitor* currentMonitor = glfwGetWindowMonitor(imguiWindow_);
    if (currentMonitor) {
        // Currently fullscreen - switch to windowed
        glfwSetWindowMonitor(imguiWindow_, nullptr, 
                             imguiWindowedPosX_, imguiWindowedPosY_, 
                             imguiWindowedWidth_, imguiWindowedHeight_, 0);
        std::cout << "[FULLSCREEN] ImGui window: switched to windowed mode" << std::endl;
    } else {
        // Currently windowed - save position/size and go fullscreen
        glfwGetWindowPos(imguiWindow_, &imguiWindowedPosX_, &imguiWindowedPosY_);
        glfwGetWindowSize(imguiWindow_, &imguiWindowedWidth_, &imguiWindowedHeight_);
        
        // Get the monitor the window is currently on
        int wx, wy;
        glfwGetWindowPos(imguiWindow_, &wx, &wy);
        
        GLFWmonitor* targetMonitor = nullptr;
        for (size_t i = 0; i < monitors_.size(); ++i) {
            int mx, my;
            glfwGetMonitorPos(monitors_[i], &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors_[i]);
            if (wx >= mx && wx < mx + mode->width && wy >= my && wy < my + mode->height) {
                targetMonitor = monitors_[i];
                break;
            }
        }
        
        // Fallback to primary monitor if not found
        if (!targetMonitor) {
            targetMonitor = glfwGetPrimaryMonitor();
        }
        
        if (targetMonitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
            glfwSetWindowMonitor(imguiWindow_, targetMonitor, 0, 0, 
                                 mode->width, mode->height, mode->refreshRate);
            std::cout << "[FULLSCREEN] ImGui window: switched to fullscreen on monitor" << std::endl;
        }
    }
}

bool Visualizer::isMainWindowFullscreen() const {
    if (!window_) return false;
    return glfwGetWindowMonitor(window_) != nullptr;
}

bool Visualizer::isImGuiWindowFullscreen() const {
    if (!imguiWindow_) return false;
    return glfwGetWindowMonitor(imguiWindow_) != nullptr;
}

void Visualizer::toggleBothWindowsFullscreen() {
    // Check current state from main window
    bool currentlyFullscreen = isMainWindowFullscreen();
    
    if (currentlyFullscreen) {
        // Exit fullscreen on both windows
        std::cout << "[FULLSCREEN] Exiting fullscreen on both windows" << std::endl;
        toggleMainWindowFullscreen();
        toggleImGuiWindowFullscreen();
    } else {
        // Enter fullscreen on both windows
        std::cout << "[FULLSCREEN] Entering fullscreen on both windows" << std::endl;
        toggleMainWindowFullscreen();
        toggleImGuiWindowFullscreen();
    }
}

// Get zoom for shader mode (checks saved zoom first, falls back to shader default)
float Visualizer::getZoomForShaderMode(int modeIndex) const {
    if (modeIndex <= 0) return 1.0f;
    
    // First check if there's a saved zoom value in settings
    if (settingsManager_) {
        float savedZoom = settingsManager_->getProceduralZoom(modeIndex);
        if (savedZoom > 0.0f) {
            return savedZoom;
        }
    }
    
    // Fall back to shader's default zoom
    auto effectMeta = GetEffectRegistry().getEffectByMode(modeIndex);
    if (effectMeta) {
        return effectMeta->defaultZoom;
    }
    
    return 1.0f;
}
