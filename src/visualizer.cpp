#include "visualizer.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <array>
#include "audio_capture.h"
#include "imgui.h"

namespace {
constexpr int kProceduralModeCount = 24;
constexpr int kPostProcessModeCount = 17;
constexpr int kKaleidoscopeModeIndex = 23;
constexpr int kPostProcessKaleidoscopeModeIndex = 7;

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
}

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* cornerVertexShaderSource = R"(
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
    float size = baseSize * (0.55 + eased * (0.7 + profile * 0.35)) + baseScale * 0.01f;

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

const char* cornerFragmentShaderSource = R"(
#version 330 core
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

void main() {
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float dist = length(uv);
    if (dist > 1.0) discard;

    float activation = clamp(vActivation, 0.0, 1.0);
    float profile = clamp(vProfile, 0.0, 1.0);
    float eased = smoothstep(0.0, 1.0, activation);

    float coreRadius = 0.32 + profile * 0.08 + eased * 0.04;
    float auraRadius = 0.78 + profile * 0.09;
    float ringCenter = 0.55 + profile * 0.05;

    float core = exp(-pow(dist / coreRadius, 2.2));
    float aura = exp(-pow(dist / auraRadius, 4.0));
    float ring = exp(-pow((dist - ringCenter) * (3.4 + eased * 1.8), 2.0));

    float wave = sin(vAngle + uTime * (0.9 + uTempo * 0.35));
    float shimmer = 0.5 + 0.5 * sin(uTime * (1.6 + uTempo * 0.4) + profile * 3.1);
    float spectral = clamp(uBass * 0.32 + uMid * 0.44 + uHigh * 0.58, 0.0, 2.0);

    vec3 baseColor = mix(vPalettePrimary, vPaletteSecondary, clamp(vPaletteBlend, 0.0, 1.0));
    vec3 glowColor = mix(vPalettePrimary, vPaletteSecondary, clamp(vPaletteBlend + 0.2, 0.0, 1.0));
    vec3 ringColor = mix(vPalettePrimary, vPaletteSecondary, clamp(vPaletteBlend + 0.4, 0.0, 1.0));

    vec3 color = baseColor * core * (0.62 + eased * 0.48 + spectral * 0.12);
    color += glowColor * aura * (0.3 + eased * (0.4 + 0.25 * shimmer));
    color += ringColor * ring * (0.22 + eased * 0.4 + 0.12 * wave);

    float alpha = core * (0.58 + eased * 0.34)
                + aura * (0.26 + eased * 0.32)
                + ring * (0.18 + eased * 0.24);
    alpha = clamp(alpha, 0.0, 0.92);

    FragColor = vec4(color, alpha);
}
)";

const char* fragmentShaderSource = R"(
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

const char* sparkVertexShaderSource = R"(
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

const char* sparkFragmentShaderSource = R"(
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

const char* coreVertexShaderSource = R"(
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

const char* coreFragmentShaderSource = R"(
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

    GLboolean pointSpriteEnabled = glIsEnabled(GL_POINT_SPRITE);
    if (!pointSpriteEnabled) {
        glEnable(GL_POINT_SPRITE);
    }

    cornerShader_->use();
    cornerShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
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
    cornerShader_->setUniform3f("uScenePrimary",
                                scenePrimaryColor_[0],
                                scenePrimaryColor_[1],
                                scenePrimaryColor_[2]);
    cornerShader_->setUniform3f("uSceneSecondary",
                                sceneSecondaryColor_[0],
                                sceneSecondaryColor_[1],
                                sceneSecondaryColor_[2]);
    cornerShader_->setUniform1f("uSceneBlend", scenePaletteBlend_);

    glBindVertexArray(cornerVAO_);
    glDrawArrays(GL_POINTS, 0, cornerVertexCount_);
    glBindVertexArray(0);

    glUseProgram(0);

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
    coreShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
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
    coreShader_->setUniform3f("uScenePrimary",
                              scenePrimaryColor_[0],
                              scenePrimaryColor_[1],
                              scenePrimaryColor_[2]);
    coreShader_->setUniform3f("uSceneSecondary",
                              sceneSecondaryColor_[0],
                              sceneSecondaryColor_[1],
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
    : window_(nullptr), windowWidth_(800), windowHeight_(600), time_(0.0f),
      quadVAO_(0), quadVBO_(0), waveformVAO_(0), waveformVBO_(0),
      coreVAO_(0), coreVBO_(0), coreVertexCount_(0),
      sparkVAO_(0), sparkVBO_(0), sparkVertexCount_(0),
      cornerVAO_(0), cornerVBO_(0), cornerVertexCount_(0),
      audioFeatures_{},
      selectedDevice_(-1), showDeviceMenu_(false), showDiagnostic_(false), consoleMode_(false),
      showImGuiWindow_(true), showDeviceSelector_(false), showDiagnosticInfo_(false), showConsoleMode_(false),
      showImGuiVisualWindow_(true),
      imguiInitialized_(false), autoRandomizeColors_(true), colorRandomInterval_(12.0f),
      colorRandomTimer_(0.0f), deltaTime_(0.0f), rng_(std::random_device{}()),
      onsetColorCyclingEnabled_(true), onsetTriggerCount_(0), lastOnsetActive_(false),
      tempoMultiplier_(1.0f),
      scenePalettes_(),
      currentScenePaletteIndex_(-1),
      scenePaletteHueSeed_(0.0f),
      scenePrimaryColor_{0.25f, 0.32f, 0.58f},
      sceneSecondaryColor_{0.35f, 0.65f, 0.92f},
      scenePaletteBlend_(0.6f),
      rgbChannelEnabled_{true, true, true},
      globalIntensityEnvelope_(0.0f),
      coreShowSpokes_(true),
      coreShowRunes_(true),
      coreShowSparkles_(true),
      coreShowBloom_(true),
      showCornerOrbs_(true),
      showProceduralLayer_(true),
      proceduralLayerDebug_(false),
      proceduralLayerOpacity_(0.85f),
      proceduralLayerMode_(3),
      postProcessMode_(2),
      postProcessStrength_(1.0f),
      postProcessRgbAdjust_{1.0f, 1.0f, 1.0f},
      audioInputGain_(1.0f),
      visualSensitivity_(1.0f) {
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
    
    // Remove self-reference
    // scenePalettes_ = scenePalettes_;
}

Visualizer::~Visualizer() {
    shutdown();
}

bool Visualizer::initialize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;

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
        std::cout << "Procedural layer initialization failed, disabling procedural overlay" << std::endl;
        showProceduralLayer_ = false;
        proceduralLayerDebug_ = false;
    } else {
        proceduralLayerMode_ = std::clamp(proceduralLayerMode_, 0, kProceduralModeCount - 1);
        proceduralLayer_.setMode(proceduralLayerMode_);
    }

    if (!postProcessor_.initialize(windowWidth_, windowHeight_)) {
        std::cout << "Post processor initialization failed, disabling post effects" << std::endl;
        postProcessMode_ = 0;
        postProcessStrength_ = 0.0f;
    }

    if (!loadShaders()) {
        std::cout << "Failed to load shaders, using fallback rendering" << std::endl;
        shader_.reset(); // Will trigger fallback triangle
    }
    useModernPipeline_ = shader_ != nullptr;

    if (!loadCoreShader()) {
        coreShader_.reset();
    }

    if (!loadSparkShader()) {
        sparkShader_.reset();
    }

    if (!loadCornerShader()) {
        cornerShader_.reset();
    }

    if (setupImGui()) {
        imguiInitialized_ = true;
    } else {
        std::cout << "ImGui initialization failed, continuing without ImGui interface" << std::endl;
        showImGuiWindow_ = false;
        imguiInitialized_ = false;
    }

    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    rendererName_ = renderer ? renderer : "Unknown";

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    openglVersion_ = version ? version : "Unknown";

    initializeDynamicSystems();

    return true;
}


void Visualizer::buildScenePalettes() {
    scenePalettes_.clear();

    scenePalettes_.push_back({
        "Sesión Azul",
        {0.10f, 0.24f, 0.58f},
        {0.28f, 0.70f, 0.98f},
        0.62f
    });

    scenePalettes_.push_back({
        "Sesión Roja",
        {0.58f, 0.12f, 0.16f},
        {0.94f, 0.36f, 0.30f},
        0.48f
    });

    scenePalettes_.push_back({
        "Sesión Verde",
        {0.10f, 0.32f, 0.18f},
        {0.30f, 0.82f, 0.52f},
        0.56f
    });

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

void Visualizer::setUnifiedPalette(const std::array<float, 3>& primary,
                                   const std::array<float, 3>& secondary,
                                   float blend) {
    scenePrimaryColor_ = primary;
    sceneSecondaryColor_ = secondary;
    scenePaletteBlend_ = std::clamp(blend, 0.0f, 1.0f);

    (void)secondary;
    proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);
}

void Visualizer::applyScenePalette(int index) {
    if (index < 0 || scenePalettes_.empty()) {
        currentScenePaletteIndex_ = -1;
        setDefaultScenePalette();
        return;
    }

    int clamped = std::clamp(index, 0, static_cast<int>(scenePalettes_.size()) - 1);
    currentScenePaletteIndex_ = clamped;

    const ScenePalette& palette = scenePalettes_[clamped];
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

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool Visualizer::shouldClose() {
    return window_ ? glfwWindowShouldClose(window_) : true;
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
        bool tabDown = glfwGetKey(window_, GLFW_KEY_TAB) == GLFW_PRESS;
        ImGuiIO* io = ImGui::GetCurrentContext() ? &ImGui::GetIO() : nullptr;
        bool allowToggle = !tabDown ? false
                          : (!io || !io->WantCaptureKeyboard || !showImGuiWindow_);

        if (allowToggle && (now - lastTabToggle) > 0.25) {
            showImGuiWindow_ = !showImGuiWindow_;
            if (!showImGuiWindow_) {
                showDeviceSelector_ = false;
                showDiagnosticInfo_ = false;
                showConsoleMode_ = false;
            }
            lastTabToggle = now;
        }

        if (!io || !io->WantCaptureKeyboard) {
            static double lastModeToggle = 0.0;
            static double lastOpacityAdjust = 0.0;
            static double lastPostProcessToggle = 0.0;
            if ((now - lastModeToggle) > 0.15) {
                if (glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                    proceduralLayerMode_ = (proceduralLayerMode_ + 1) % kProceduralModeCount;
                    proceduralLayer_.setMode(proceduralLayerMode_);
                    lastModeToggle = now;
                } else if (glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS) {
                    proceduralLayerMode_ = (proceduralLayerMode_ + kProceduralModeCount - 1) % kProceduralModeCount;
                    proceduralLayer_.setMode(proceduralLayerMode_);
                    lastModeToggle = now;
                }
            }

            if ((now - lastOpacityAdjust) > 0.12) {
                constexpr float kOpacityStep = 0.05f;
                bool adjusted = false;
                if (glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS) {
                    proceduralLayerOpacity_ += kOpacityStep;
                    adjusted = true;
                } else if (glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS) {
                    proceduralLayerOpacity_ -= kOpacityStep;
                    adjusted = true;
                }

                if (adjusted) {
                    if (proceduralLayerOpacity_ < 0.0f) {
                        proceduralLayerOpacity_ = 0.0f;
                    } else if (proceduralLayerOpacity_ > 1.0f) {
                        proceduralLayerOpacity_ = 1.0f;
                    }
                    lastOpacityAdjust = now;
                }
            }

            if ((now - lastPostProcessToggle) > 0.25) {
                if (glfwGetKey(window_, GLFW_KEY_P) == GLFW_PRESS) {
                    postProcessMode_ = (postProcessMode_ + 1) % kPostProcessModeCount;
                    showPostProcess_ = postProcessMode_ > 0;
                    lastPostProcessToggle = now;
                }
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
}

void Visualizer::updateAudioData(const AudioAnalyzer::AudioFeatures& features) {
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

    tempoMultiplier_ = std::clamp(tempoMultiplier_ * 0.9f + targetTempo * 0.1f, 0.3f, 2.0f);

    audioFeatures_.energy = std::clamp(audioFeatures_.energy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.bassEnergy = std::clamp(audioFeatures_.bassEnergy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.midEnergy = std::clamp(audioFeatures_.midEnergy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.highEnergy = std::clamp(audioFeatures_.highEnergy * sensitivity, 0.0f, 3.0f);
    audioFeatures_.onset = std::clamp(audioFeatures_.onset * sensitivity, 0.0f, 2.0f);
    audioFeatures_.beat = std::clamp(audioFeatures_.beat * sensitivity, 0.0f, 2.0f);
    audioFeatures_.kick = std::clamp(audioFeatures_.kick * sensitivity, 0.0f, 2.0f);
}

void Visualizer::updateAudioBuffer(const std::vector<float>& audioBuffer) {
    if (audioBuffer.size() >= waveformBuffer_.size()) {
        std::copy(audioBuffer.begin(), audioBuffer.begin() + waveformBuffer_.size(), waveformBuffer_.begin());
    }
}

void Visualizer::render() {
    bool usePost = showPostProcess_ && postProcessor_.isInitialized() && postProcessMode_ > 0 && postProcessStrength_ > 0.0f;

    float rawEnergy = std::clamp(audioFeatures_.energy, 0.0f, 2.5f);
    float bass = std::clamp(audioFeatures_.bassEnergy, 0.0f, 2.0f);
    float excitement = std::clamp(audioFeatures_.onset * 0.6f + audioFeatures_.beat * 0.8f
                                  + audioFeatures_.kick * 0.5f, 0.0f, 1.6f);
    float targetIntensity = std::clamp(rawEnergy * 0.55f + bass * 0.45f + excitement * 0.65f,
                                       0.0f, 2.5f);

    float dt = std::max(deltaTime_, 1.0f / 120.0f);
    float rise = 1.0f - std::pow(0.04f, dt * tempoMultiplier_);
    float decayBase = std::pow(0.18f, dt * tempoMultiplier_);

    if (targetIntensity > globalIntensityEnvelope_) {
        globalIntensityEnvelope_ += (targetIntensity - globalIntensityEnvelope_) * rise;
    } else {
        globalIntensityEnvelope_ = globalIntensityEnvelope_ * decayBase + targetIntensity * (1.0f - decayBase);
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
                rgbChannelEnabled_[2] ? GL_TRUE : GL_FALSE,
                GL_TRUE);

    if (currentScenePaletteIndex_ < 0) {
        updateDynamicScenePalette();
    }

    if (showProceduralLayer_) {
        renderProceduralLayer();
    }

    float intensityScale = std::clamp(globalIntensityEnvelope_, 0.0f, 2.0f);
    renderModernCore();
    if (showCornerOrbs_) {
        renderCornerOrbs();
    }


    renderIdleSpinner(time_);

    if (usePost) {
        postProcessor_.endCapture();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        float strength = std::clamp(postProcessStrength_, 0.0f, 1.0f);
        strength *= std::clamp(0.35f + intensityScale * 0.65f, 0.3f, 1.0f);
        postProcessor_.apply(postProcessMode_, strength, time_, postProcessRgbAdjust_, audioFeatures_.bassEnergy);
    }

    glColorMask(previousMask[0], previousMask[1], previousMask[2], previousMask[3]);

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
    static bool orbsWasDown = false;
    static bool kaleidoWasDown = false;

    bool orbsKeyDown = (glfwGetKey(window_, GLFW_KEY_1) == GLFW_PRESS ||
                        glfwGetKey(window_, GLFW_KEY_KP_1) == GLFW_PRESS);
    bool kaleidoKeyDown = glfwGetKey(window_, GLFW_KEY_K) == GLFW_PRESS;

    auto processToggle = [&](bool keyDown, bool& wasDown, double& lastToggle, auto&& action) {
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

    processToggle(orbsKeyDown, orbsWasDown, lastOrbsToggle, [this]() {
        showCornerOrbs_ = !showCornerOrbs_;
    });

    processToggle(kaleidoKeyDown, kaleidoWasDown, lastKaleidoToggle, [this]() {
        showPostProcess_ = true;
        postProcessMode_ = kPostProcessKaleidoscopeModeIndex;
    });
}

bool Visualizer::setupOpenGL() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);  // Use OpenGL 2.1 for compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);  // Don't force core profile
    
    // Try with OpenGL ES if desktop OpenGL fails
    bool useGLES = false;

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, "Audio Visualizer", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }

    glfwMakeContextCurrent(window_);

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
    
    std::cout << "OpenGL setup successful!" << std::endl;
    return true;
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
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
        CornerVertex{-0.88f,  0.88f, 0.075f, 0.020f,  0.0f, 0.10f},  // Superior izquierda
        CornerVertex{ 0.88f,  0.88f, 0.072f, 0.022f,  1.3f, 0.35f},  // Superior derecha
        CornerVertex{-0.88f, -0.88f, 0.078f, 0.024f, -1.6f, 0.55f},  // Inferior izquierda
        CornerVertex{ 0.88f, -0.88f, 0.074f, 0.021f,  2.2f, 0.78f},  // Inferior derecha
        
        // Nuevos orbes en el medio
        CornerVertex{ 0.00f,  0.88f, 0.068f, 0.018f,  0.8f, 0.25f},  // Centro superior
        CornerVertex{ 0.00f, -0.88f, 0.070f, 0.019f, -0.8f, 0.65f},  // Centro inferior
        CornerVertex{-0.88f,  0.00f, 0.069f, 0.017f,  2.8f, 0.45f},  // Centro izquierda
        CornerVertex{ 0.88f,  0.00f, 0.071f, 0.020f, -2.8f, 0.85f}   // Centro derecha
    };

    cornerVertexCount_ = static_cast<GLsizei>(corners.size());

    glGenVertexArrays(1, &cornerVAO_);
    glGenBuffers(1, &cornerVBO_);

    glBindVertexArray(cornerVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CornerVertex), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex), reinterpret_cast<void*>(2 * sizeof(float)));
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
    return cornerShader_->loadFromSource(cornerVertexShaderSource, cornerFragmentShaderSource);
}

void Visualizer::setupQuad() {
    float vertices[] = {
        // positions    // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // tex coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void Visualizer::setupWaveform() {
    glGenVertexArrays(1, &waveformVAO_);
    glGenBuffers(1, &waveformVBO_);
    
    glBindVertexArray(waveformVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, waveformVBO_);
    
    // Allocate buffer memory (will be updated dynamically)
    glBufferData(GL_ARRAY_BUFFER, waveformBuffer_.size() * sizeof(float) * 2, nullptr, GL_DYNAMIC_DRAW);
    
    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void Visualizer::renderWaveform(const std::vector<float>& audioBuffer) {
    if (audioBuffer.empty()) return;
    
    // Create vertices for waveform
    std::vector<float> vertices;
    vertices.reserve(audioBuffer.size() * 2);
    
    float waveHeight = 100.0f; // Height of waveform display
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
    shader_->setUniform2f("uResolution",
                          static_cast<float>(windowWidth_),
                          static_cast<float>(windowHeight_));
    shader_->setUniform3f("uScenePrimary",
                          scenePrimaryColor_[0],
                          scenePrimaryColor_[1],
                          scenePrimaryColor_[2]);
    shader_->setUniform3f("uSceneSecondary",
                          sceneSecondaryColor_[0],
                          sceneSecondaryColor_[1],
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
    sparkShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
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
    coreShader_->setUniform2f("uResolution", static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
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
    coreShader_->setUniform3f("uScenePrimary",
                              scenePrimaryColor_[0],
                              scenePrimaryColor_[1],
                              scenePrimaryColor_[2]);
    coreShader_->setUniform3f("uSceneSecondary",
                              sceneSecondaryColor_[0],
                              sceneSecondaryColor_[1],
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
    if (err != paNoError) return;
    
    int numDevices = Pa_GetDeviceCount();
    deviceNames_.clear();
    deviceIsInternal_.clear();
    
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            deviceNames_.push_back(deviceInfo->name);
            bool isInternalLoopback = false;
            if (const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi)) {
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
    // Check for 'D' key toggle
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showDeviceMenu_ = !showDeviceMenu_;
            lastPress = currentTime;
        }
    }
    
    // Check for 'I' key toggle for diagnostic mode
    if (glfwGetKey(window_, GLFW_KEY_I) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showDiagnostic_ = !showDiagnostic_;
            lastPress = currentTime;
        }
    }
    
    // Check for 'C' key toggle for console mode
    if (glfwGetKey(window_, GLFW_KEY_C) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            consoleMode_ = !consoleMode_;
            lastPress = currentTime;
        }
    }
    
    // Check for '1' key toggle for corner orbs (main row or keypad) when device menu is closed
    if (!showDeviceMenu_ &&
        (glfwGetKey(window_, GLFW_KEY_1) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_KP_1) == GLFW_PRESS)) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            showCornerOrbs_ = !showCornerOrbs_;
            lastPress = currentTime;
        }
    }
    
    // Check for 'K' key to activate kaleidoscope mode
    if (glfwGetKey(window_, GLFW_KEY_K) == GLFW_PRESS) {
        static double lastPress = 0.0;
        double currentTime = glfwGetTime();
        if (currentTime - lastPress > 0.5) { // 500ms debounce
            proceduralLayerMode_ = kKaleidoscopeModeIndex;
            proceduralLayer_.setMode(proceduralLayerMode_);
            showProceduralLayer_ = true; // Ensure procedural layer is visible
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
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        showDeviceMenu_ = false;
    }
    
    return false;
}

void Visualizer::renderText(const std::string& text, float x, float y) {
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
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f
    };
    
    glBindVertexArray(coreVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, coreVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
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

    proceduralLayer_.setEnabled(showProceduralLayer_);
    proceduralLayer_.setDebugPreview(proceduralLayerDebug_);
    proceduralLayer_.setMode(proceduralLayerMode_);
    proceduralLayer_.setColorPalette(scenePrimaryColor_.data(), sceneSecondaryColor_.data(), scenePaletteBlend_);

    proceduralLayer_.render(context);
    proceduralLayer_.composite(context, proceduralLayerOpacity_);
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
    
    for (const auto& gear : gears_) {
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

void Visualizer::updateCore(float dt, const AudioAnalyzer::AudioFeatures& features) {
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

void Visualizer::updateGears(float dt, const AudioAnalyzer::AudioFeatures& features) {
    for (auto& gear : gears_) {
        gear.angle += gear.angularVelocity * dt * (1.0f + features.energy * 2.0f);
        gear.orbitAngle += gear.orbitSpeed * gear.orbitDirection * dt * (1.0f + features.bassEnergy * 1.5f);
        gear.jitterPhase += dt * 3.0f;
        
        float jitter = std::sin(gear.jitterPhase) * 0.02f * features.energy;
        gear.x = std::cos(gear.orbitAngle) * gearSpawnRadius_ + jitter;
        gear.y = std::sin(gear.orbitAngle) * gearSpawnRadius_ + jitter;
    }
    
    gearSpawnRadius_ = 50.0f + features.bassEnergy * 100.0f;
}

void Visualizer::updateLife(float dt, const AudioAnalyzer::AudioFeatures& features) {
    lifeTimeAccumulator_ += dt;
    
    if (lifeTimeAccumulator_ > 0.1f) {
        lifeTimeAccumulator_ = 0.0f;
        
        // Simple Conway's Game of Life rules
        for (int y = 0; y < LifeCellGrid::HEIGHT; ++y) {
            for (int x = 0; x < LifeCellGrid::WIDTH; ++x) {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
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
