#include "visualizer.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <random>
#include "audio_capture.h"
#include "imgui.h"

namespace {
constexpr int kProceduralModeCount = 20;
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

out float vSize;
out float vAngle;
out float vActivation;
out float vProfile;

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
}
)";

const char* cornerFragmentShaderSource = R"(
#version 330 core
in float vSize;
in float vAngle;
in float vActivation;
in float vProfile;

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

    vec3 baseColor = vec3(0.14 + uBass * 0.42,
                          0.2 + uMid * 0.52,
                          0.28 + uHigh * 0.68);
    vec3 glowColor = vec3(0.68 + uHigh * 0.72,
                          0.36 + uMid * 0.46,
                          1.08 + uBass * 0.42);
    vec3 ringColor = vec3(0.88 + 0.18 * spectral,
                          0.48 + 0.2 * uMid,
                          1.24 + 0.26 * uBass);

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

const char* sparkVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPolar;   // angle (rad), radial jitter
layout (location = 1) in float aSeed;   // random seed [0,1)

out float vGlow;
out float vHueShift;
out float vSeed;

uniform vec2 uResolution;
uniform float uBaseRadius;
uniform float uTime;
uniform float uTempo;
uniform float uPulse;
uniform float uEnergy;
uniform float uBass;
uniform float uMid;
uniform float uHigh;

const float TAU = 6.28318530718;

void main() {
    float angle = aPolar.x;
    float radialLayer = aPolar.y;

    float baseRadius = uBaseRadius * (1.35 + radialLayer * 0.8);
    float audioStretch = (0.4 + uPulse * 0.55 + uEnergy * 0.35 + uBass * 0.45);
    float sparkRadius = baseRadius + audioStretch * (45.0 + radialLayer * 120.0);

    float swirl = sin(angle * (3.0 + uTempo * 0.4) + uTime * (0.6 + radialLayer * 0.8));
    float harmonicWarp = sin(angle * (12.0 + uHigh * 4.0) + uTime * (3.2 + uTempo)) * (18.0 + uHigh * 25.0);
    float kickBurst = sin(angle * 5.0 + uTime * (5.0 + uTempo)) * uPulse * 20.0;

    sparkRadius += swirl * 18.0 + harmonicWarp + kickBurst;

    vec2 dir = vec2(cos(angle), sin(angle));
    vec2 pos = dir * sparkRadius;

    float flicker = sin(uTime * (6.0 + uTempo * 0.5) + aSeed * TAU) * 0.5 + 0.5;
    float burst = sin(angle * (9.0 + uHigh * 5.0) + uTime * (7.0 + uTempo * 1.4));
    vGlow = clamp(flicker * (0.4 + uEnergy * 0.6) + max(0.0, burst) * (0.3 + uHigh * 0.5), 0.0, 1.6);
    vHueShift = aSeed;
    vSeed = aSeed;

    vec2 scale = vec2(2.0 / uResolution.x, 2.0 / uResolution.y);
    gl_Position = vec4(pos * scale, 0.0, 1.0);
    gl_PointSize = 6.0 + vGlow * 18.0 + uPulse * 10.0;
}
)";

const char* sparkFragmentShaderSource = R"(
#version 330 core
in float vGlow;
in float vHueShift;
in float vSeed;
out vec4 FragColor;

uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;

vec3 hsv2rgb(vec3 c) {
    vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return c.z * mix(vec3(1.0), rgb, c.y);
}

void main() {
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float dist = length(uv);
    if (dist > 1.0) {
        discard;
    }

    float falloff = pow(1.0 - dist, 2.0);
    float core = smoothstep(0.6, 0.0, dist);
    float ring = smoothstep(1.0, 0.2, dist) * (1.0 - core);

    float hue = fract(vHueShift + uHigh * 0.35 + uMid * 0.18);
    float sat = clamp(0.6 + uEnergy * 0.35, 0.0, 1.0);
    float val = clamp(0.45 + uEnergy * 0.55 + vGlow * 0.5, 0.0, 1.5);

    vec3 innerColor = hsv2rgb(vec3(hue, sat, val));
    vec3 outerColor = hsv2rgb(vec3(fract(hue + 0.1), clamp(sat * 0.6, 0.0, 1.0), clamp(val * 0.9, 0.0, 1.0)));

    vec3 color = innerColor * core + outerColor * ring;

    float alpha = (core * 0.75 + ring * 0.55) * clamp(0.4 + vGlow, 0.0, 1.5);
    FragColor = vec4(color, alpha);
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

void Visualizer::renderProceduralLayer() {
    if (!showProceduralLayer_ && !proceduralLayerDebug_) {
        return;
    }

    LayerContext context{
        windowWidth_,
        windowHeight_,
        time_,
        tempoMultiplier_,
        &audioFeatures_
    };

    proceduralLayer_.setEnabled(showProceduralLayer_);
    proceduralLayer_.setDebugPreview(proceduralLayerDebug_);
    proceduralLayer_.render(context);

    float opacity = std::clamp(proceduralLayerOpacity_, 0.0f, 1.0f);
    proceduralLayer_.composite(context, opacity);
}

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

uniform vec2  uResolution;
uniform float uBaseRadius, uPulse, uKick, uHarmonic, uGrowth, uMaturity;
uniform float uEnergy, uBass, uMid, uHigh;
uniform float uTime, uTempo, uBeat;
uniform float uIdlePulse, uIdleWarp, uIdleSpin, uOnset;
uniform vec4  uLegacyCircleFill;
uniform vec4  uLegacyCircleOutline;
uniform vec4  uLegacyBloomInner;
uniform vec4  uLegacyBloomOuter;

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

    // --- Outputs ---
    vAngle      = angle;
    vRadial     = aRadial;
    vWarp       = warp;
    vPetalMask  = petalMask;
    vVoidMask   = clamp(voidMask, 0.0, 1.0);
    vFacetFlow  = clamp(facet * (0.7 + uPulse * 0.4), -1.6, 1.6);
    vRadiusNorm = clamp(length(pos) / (uBaseRadius * (3.4 + uPulse * 0.9 + uGrowth * 0.45)), 0.0, 1.9);

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
uniform float uShowBase;
uniform float uShowCorona;
uniform float uShowSpokes;
uniform float uShowRunes;
uniform float uShowSparkles;
uniform float uShowBloom;
uniform vec4  uLegacyCircleFill;
uniform vec4  uLegacyCircleOutline;
uniform vec4  uLegacyBloomInner;
uniform vec4  uLegacyBloomOuter;

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
    basePalette[0] = vec3(0.28, 0.45, 0.85);
    basePalette[1] = vec3(0.85, 0.45, 0.62);
    basePalette[2] = vec3(0.30, 0.80, 0.55);
    basePalette[3] = vec3(0.92, 0.70, 0.28);

    float paletteBlend = clamp(hueBase * 3.0, 0.0, 3.0);
    int paletteIndex = int(paletteBlend);
    int nextIndex = (paletteIndex + 1) % 4;
    float paletteMix = fract(paletteBlend);
    vec3 baseHue = mix(basePalette[paletteIndex], basePalette[nextIndex], paletteMix);

    vec3 accentHue = mix(baseHue.yzx, baseHue.zxy, 0.35 + hueAccent * 0.4);
    vec3 sparkHue = mix(vec3(1.0, 0.9, 0.6), baseHue, 0.2 + hueSpark * 0.5);

    vec3 coreColor = mix(baseHue, vec3(0.15, 0.22, 0.30), 0.3 - uBass * 0.2);
    coreColor += vec3(uBass * 0.55, uMid * 0.42, uHigh * 0.62);

    vec3 petalColor = mix(accentHue, vec3(0.6, 0.3, 0.9), 0.35 + uHigh * 0.4);
    vec3 runeColor = mix(accentHue.yzx, vec3(0.9, 0.6, 0.2), 0.25 + uMid * 0.3);
    vec3 sparkColor = mix(sparkHue, vec3(1.0, 0.95, 0.75), 0.6 + uHigh * 0.3);
    vec3 chemColor = mix(baseHue.xzy, vec3(0.25, 0.9, 1.1), 0.5 + uEnergy * 0.3);
    vec3 bondColor = mix(accentHue.zxy, vec3(1.0, 0.7, 0.4), 0.4 + uMid * 0.35);
    vec3 ribbonColor = mix(baseHue, vec3(1.0, 0.3, 0.5), 0.5 + uHigh * 0.35);
    vec3 facetColor = mix(baseHue.zxy, vec3(0.2, 0.8, 1.0), 0.45 + uEnergy * 0.3);
    vec3 shardColor = mix(accentHue, vec3(1.0, 0.85, 0.6), 0.5 + uPulse * 0.3);

    vec3 legacyCircleFill = vec3(uLegacyCircleFill.rgb);
    vec3 legacyCircleOutline = vec3(uLegacyCircleOutline.rgb);
    vec3 legacyBloomInner = vec3(uLegacyBloomInner.rgb);
    vec3 legacyBloomOuter = vec3(uLegacyBloomOuter.rgb);

    coreColor = mix(coreColor, legacyCircleFill, 0.65);
    petalColor = mix(petalColor, legacyBloomOuter, 0.55);
    runeColor = mix(runeColor, legacyCircleOutline, 0.6);
    facetColor = mix(facetColor, legacyCircleOutline, 0.4);
    shardColor = mix(shardColor, legacyBloomInner, 0.5);

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
    float growthShell = smoothstep(0.15, 0.85, vRadiusNorm + growthSpiral * 0.12);
    float growthPulse = smoothstep(0.2, 0.95, sin(vAngle * (2.4 + uGrowth) + uTime * (0.9 + uTempo * 0.4)) * 0.5 + 0.5);
    vec3 growthColor = mix(vec3(0.25 + uBass * 0.7, 0.45 + uMid * 0.6, 0.9 + uHigh * 0.4),
                           vec3(0.95, 0.5 + uMid * 0.4, 0.35 + uBass * 0.4),
                           clamp(uGrowth * 0.35, 0.0, 1.0));

    float maturityVeins = smoothstep(0.3, 0.92, sin(vAngle * (7.0 + uMaturity * 1.4) - uTime * (1.5 + uTempo * 0.4)) * 0.5 + 0.5);
    float maturityHalo = smoothstep(0.45, 1.15, vRadiusNorm + sin(vAngle * 3.2 + uTime * 0.6) * 0.08);
    vec3 maturityColor = vec3(1.05 + uHigh * 0.4, 0.84 + uMid * 0.35, 0.65 + uBass * 0.28);

    color += coreColor * baseIntensity * uShowBase;
    color += filamentColor * filamentMask * (0.7 + uPulse * 0.5) * uShowBase;
    color += growthColor * growthShell * growthPulse * (0.6 + uGrowth * 0.4) * uShowCorona;
    color += petalColor * petals * (0.45 + uPulse * 0.28 + spiral * 0.2) * uShowCorona;
    color += runeColor * runeMask * (0.18 + uHarmonic * 0.35) * uShowRunes;
    color += sparkColor * spark * uShowSparkles;
    color += petalColor * bloom * uShowBloom;
    color += maturityColor * maturityVeins * (0.18 + uMaturity * 0.25) * uShowRunes;
    color += ribbonColor * (ribbonStrand * (0.55 + uEnergy * 0.25) + ribbonSpark * 0.85) * uShowCorona;
    color += facetColor * facetPrism * (0.32 + uEnergy * 0.28 + uShowBase * 0.25) * uShowBase;
    color += shardColor * facetShard * (0.26 + uHigh * 0.35 + uPulse * 0.25) * uShowSparkles;

    float chemAura = smoothstep(0.2, 1.4, chemStir + nodePulse * 0.5);
    float arterial = smoothstep(0.3, 0.95, vRadiusNorm + sin(vAngle * 5.0 + uTime * 0.8) * 0.1);
    float bondVeins = smoothstep(0.15, 0.85, bondCore * outerLayer);
    vec3 phaseColor = mix(chemColor, chemColor.zyx, clamp(uHigh * 0.4 + uOnset * 0.3, 0.0, 1.0));
    vec3 bondGlow = mix(bondColor, bondColor.yzx, clamp(uMid * 0.5, 0.0, 1.0));

    color += phaseColor * chemAura * (0.3 + uShowCorona * 0.5 + uEnergy * 0.25);
    color += bondGlow * bondVeins * (0.35 + nodePulse * 0.3) * uShowRunes;
    color += chemColor * arterial * outerLayer * (0.22 + uPulse * 0.3) * uShowBase;
    color += ribbonColor * ribbonFuse * 0.28 * uShowBloom;
    color += facetColor * facetPrism * 0.22 * uShowRunes;

    float spokeMask = smoothstep(0.6, 1.05, vRadiusNorm + sin(vAngle * 12.0) * 0.08);
    color += runeColor * spokeMask * (0.25 + uHigh * 0.35) * uShowSpokes;
    color += maturityColor * maturityHalo * (0.15 + uMaturity * 0.18) * uShowSpokes;

    color = max(color, vec3(0.0));

    float alpha = 0.1
                + baseIntensity * uShowBase
                + filamentMask * (0.35 + uPulse * 0.3) * uShowBase
                + growthShell * growthPulse * 0.35 * uShowCorona
                + petals * (0.25 + uPulse * 0.28) * uShowCorona
                + spokeMask * 0.25 * uShowSpokes
                + maturityVeins * 0.22 * uShowRunes
                + spark * 0.42 * uShowSparkles
                + bloom * 0.32 * uShowBloom
                + chemAura * 0.28 * uShowCorona
                + bondVeins * 0.26 * uShowRunes
                + ribbonStrand * 0.32 * uShowCorona
                + ribbonSpark * 0.28 * uShowSparkles
                + facetPrism * 0.22 * uShowBase;
    alpha = clamp(alpha, 0.08, 0.9);

    FragColor = vec4(max(color, vec3(0.0)), alpha);
}
)";

void Visualizer::setupCoreMesh() {
    if (coreVAO_) {
        glDeleteVertexArrays(1, &coreVAO_);
        coreVAO_ = 0;
    }
    if (coreVBO_) {
        glDeleteBuffers(1, &coreVBO_);
        coreVBO_ = 0;
    }

    const int segments = 360;
    std::vector<float> data;
    data.reserve((segments + 1) * 2 * 4);

    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float angle = t * 6.28318530718f;
        float c = std::cos(angle);
        float s = std::sin(angle);

        // inner vertex
        data.push_back(c);
        data.push_back(s);
        data.push_back(0.0f);
        data.push_back(angle);

        // outer vertex
        data.push_back(c);
        data.push_back(s);
        data.push_back(1.0f);
        data.push_back(angle);
    }

    coreVertexCount_ = static_cast<GLsizei>((segments + 1) * 2);

    glGenVertexArrays(1, &coreVAO_);
    glGenBuffers(1, &coreVBO_);

    glBindVertexArray(coreVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, coreVBO_);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Visualizer::initializeDynamicSystems() {
    core_ = {};
    core_.baseRadius = std::max(16.0f, std::min(windowWidth_, windowHeight_) * 0.02f);
    core_.radius = core_.baseRadius;
    core_.energy = 0.0f;
    core_.kickEnvelope = 0.0f;

    gears_.clear();
    std::uniform_real_distribution<float> distPhase(0.0f, 6.28318f);
    std::uniform_real_distribution<float> distOffset(0.85f, 1.15f);
    std::uniform_int_distribution<int> distTeeth(9, 16);

    const float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));
    const int ringCount = 3;
    const float ringBase = minDim * 0.16f;
    const float ringSpacing = minDim * 0.11f;

    auto starAngles = [](int starPoints) {
        std::vector<float> result;
        float base = 6.28318f / std::max(1, starPoints);
        for (int i = 0; i < starPoints; ++i) {
            result.push_back(base * i);
        }
        return result;
    };

    auto anchorAngles = starAngles(5);

    for (int ring = 0; ring < ringCount; ++ring) {
        int gearsOnRing = 6 + ring * 4;
        float orbitRadius = ringBase + ringSpacing * ring;
        float anchorRadius = orbitRadius * (0.6f + 0.25f * ring);
        for (int i = 0; i < gearsOnRing; ++i) {
            float phase = static_cast<float>(i) / gearsOnRing * 6.28318f;
            GearNode gear{};
            gear.orbitAngle = phase;
            gear.orbitSpeed = 0.18f + 0.12f * ring;
            gear.ringRadius = orbitRadius * distOffset(rng_);
            gear.baseRadius = minDim * (0.018f + 0.014f * ring) * distOffset(rng_);
            gear.radius = gear.baseRadius;
            gear.angle = distPhase(rng_);
            gear.angularVelocity = 0.0f;
            gear.teeth = distTeeth(rng_);
            gear.jitterPhase = distPhase(rng_);
            gear.x = 0.0f;
            gear.y = 0.0f;
            gear.orbitDirection = (i % 2 == 0) ? 1.0f : -1.0f;
            gear.anchorAngle = phase;
            gear.anchorRadius = gear.ringRadius;
            gear.ringIndex = ring;
            gear.baseOffsetRadius = anchorRadius;
            gear.baseOffsetAngle = anchorAngles[(i + ring) % anchorAngles.size()] + distPhase(rng_) * 0.12f;
            gear.baseCenterX = windowWidth_ * 0.5f + std::cos(gear.baseOffsetAngle) * gear.baseOffsetRadius;
            gear.baseCenterY = windowHeight_ * 0.5f + std::sin(gear.baseOffsetAngle) * gear.baseOffsetRadius;
            gears_.push_back(gear);
        }
    }

    gearSpawnRadius_ = ringBase + ringSpacing * (ringCount - 1);
    lifeGrid_.cells.fill(0);
    lifeGrid_.next.fill(0);
    lifeTimeAccumulator_ = 0.0f;
    lastLifeSeedTime_ = 0.0f;
}

void Visualizer::updateCore(float dt, const AudioAnalyzer::AudioFeatures& features) {
    if (!std::isfinite(dt) || dt <= 0.0f) dt = 1.0f / 60.0f;

    const float tdt = dt * tempoMultiplier_;
    const float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));

    // --- Audio inputs normalizados ---
    const float bass   = std::clamp(features.bassEnergy, 0.0f, 2.0f);
    const float mid    = std::clamp(features.midEnergy,  0.0f, 2.0f);
    const float high   = std::clamp(features.highEnergy, 0.0f, 2.0f);
    const float energy = std::clamp(features.energy,     0.0f, 3.0f);

    // --- Idle state ---
    float activity = std::clamp(std::max({features.kick, features.clap,
                                          features.hiHat, features.beat,
                                          features.onset}) * 1.2f, 0.0f, 1.5f);
    idleState_ = std::clamp(idleState_ * std::pow(0.2f, tdt)
                           + (1.0f - activity) * tdt * 0.8f, 0.0f, 1.0f);
    idlePhase_ += dt * tempoMultiplier_ * (0.3f + 0.4f * idleState_);
    const float idle = idleState_;

    // --- Envelopes ---
    auto expDecay = [](float val, float base, float tdtv) {
        return val * std::pow(base, tdtv);
    };

    core_.kickEnvelope = std::clamp(
        expDecay(core_.kickEnvelope, 0.08f, tdt) + features.kick * 0.9f, 0.0f, 2.5f);

    float harmonicIn = std::clamp(mid * 0.6f + high * 0.85f
                                 + features.clap * 0.7f + features.hiHat * 0.75f
                                 + features.onset * 0.4f, 0.0f, 2.5f);
    float hRise = 1.0f - std::pow(0.04f, tdt);
    core_.harmonicEnvelope = std::clamp(
        harmonicIn > core_.harmonicEnvelope
            ? core_.harmonicEnvelope + (harmonicIn - core_.harmonicEnvelope) * hRise
            : expDecay(core_.harmonicEnvelope, 0.35f, tdt) + harmonicIn * (1.0f - std::pow(0.35f, tdt)),
        0.0f, 2.2f);

    float magnitude = (bass * 0.6f + mid * 0.35f + high * 0.25f + energy * 0.4f
                      + features.kick * 0.9f + features.beat * 0.7f + features.onset * 0.45f)
                     * std::clamp(1.0f - idle * 0.6f, 0.25f, 1.0f);

    core_.growthTrend = std::clamp(
        core_.growthTrend + (magnitude - core_.growthTrend) * (1.0f - std::pow(0.18f, tdt)),
        0.0f, 6.0f);
    core_.growthEnvelope = std::max(expDecay(core_.growthEnvelope, 0.12f, tdt),
                                    core_.growthTrend * 0.85f);

    // --- Radio y pulse ---
    core_.pulse      = energy;
    core_.baseRadius = std::max(12.0f,
        minDim * (0.015f + 0.018f * std::clamp(energy, 0.0f, 1.5f)
                         + 0.010f * std::clamp(mid,    0.0f, 1.0f)
                         + 0.0045f* std::clamp(core_.harmonicEnvelope, 0.0f, 1.5f)));

    float growthFactor = 0.12f + 0.32f * std::tanh(core_.growthEnvelope * 0.35f);
    float growthWave   = 0.35f + 0.25f * std::sin(time_ * (0.6f + tempoMultiplier_ * 0.3f));
    core_.radius = core_.baseRadius
                 * (1.0f + 0.38f * core_.kickEnvelope
                         + 0.20f * features.beat
                         + 0.22f * std::clamp(core_.harmonicEnvelope, 0.0f, 1.6f)
                         + growthWave * growthFactor);

    // --- Energy / maturity ---
    float injection = (energy * 0.55f + bass * 0.75f + mid * 0.45f + high * 0.4f
                      + core_.kickEnvelope * 1.1f
                      + core_.harmonicEnvelope * 1.05f + harmonicIn * 0.4f)
                     * std::clamp(1.0f - idle * 0.75f, 0.2f, 1.0f);

    core_.energy   = std::clamp((core_.energy + injection * tdt) * std::pow(0.45f, tdt), 0.0f, 8.0f);
    core_.maturity = std::clamp(expDecay(core_.maturity, 0.35f, tdt)
                               + std::min(1.0f, injection * 0.12f), 0.0f, 10.0f);

    // --- Idle visuals ---
    core_.idlePulse = std::clamp(
        0.06f + idle * 0.24f + std::sin(idlePhase_ * (0.85f + idle * 0.35f)) * (0.18f + 0.12f * idle),
        0.05f, 0.6f);

    core_.idleWarp = (std::sin(idlePhase_ * (0.80f + idle * 0.50f)) * 0.22f
                    + std::sin(idlePhase_ * (1.65f + idle * 0.75f) + 1.2f) * 0.18f) * idle;

    core_.idleSpin = std::fmod(idlePhase_ * (0.35f + idle * 0.65f), 6.2831853f);
    if (core_.idleSpin < 0.0f) core_.idleSpin += 6.2831853f;
}

float Visualizer::sampleCoreEnergyField(float x, float y) const {
    float centerX = windowWidth_ * 0.5f;
    float centerY = windowHeight_ * 0.5f;
    float dx = x - centerX;
    float dy = y - centerY;
    float dist = std::sqrt(dx * dx + dy * dy);
    float radius = std::max(core_.radius, 1.0f);

    float falloff = std::exp(-dist / (radius * 1.6f));
    float oscillation = 0.7f + 0.3f * std::sin(dist / std::max(radius * 0.6f, 1.0f) + core_.pulse * 0.4f);
    float kickBoost = 1.0f + 0.8f * core_.kickEnvelope * std::exp(-dist / (radius * 0.8f));

    return core_.energy * falloff * oscillation * kickBoost;
}

void Visualizer::updateGears(float dt, const AudioAnalyzer::AudioFeatures& features) {
    if (gears_.empty()) {
        return;
    }

    if (!std::isfinite(dt) || dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }

    float tempoDt = dt * tempoMultiplier_;

    float bass = std::clamp(features.bassEnergy, 0.0f, 2.0f);
    float mid = std::clamp(features.midEnergy, 0.0f, 2.0f);
    float high = std::clamp(features.highEnergy, 0.0f, 2.0f);
    float minDimension = static_cast<float>(std::min(windowWidth_, windowHeight_));

    for (auto& gear : gears_) {
        float syncPhase = idlePhase_ * 0.6f + core_.pulse * 0.4f + gear.ringIndex * 0.25f;
        float orientationBase = gear.baseOffsetAngle
                                 + gear.orbitDirection * (0.18f * core_.pulse + 0.22f * high)
                                 + 0.12f * std::sin(syncPhase + gear.jitterPhase * 0.6f);
        float radialBase = gear.anchorRadius + core_.baseRadius * (0.65f + 0.18f * gear.ringIndex);
        float radialMod = minDimension * (0.012f + 0.025f * mid + 0.03f * core_.harmonicEnvelope + 0.02f * bass);
        float radial = radialBase + radialMod;

        gear.x = gear.baseCenterX + std::cos(orientationBase) * radial;
        gear.y = gear.baseCenterY + std::sin(orientationBase) * radial;
        gear.orbitAngle = orientationBase;

        float targetRadius = gear.baseRadius * (1.0f + 0.25f * mid + 0.32f * core_.harmonicEnvelope + 0.2f * core_.kickEnvelope);
        gear.radius = gear.radius + (targetRadius - gear.radius) * std::min(1.0f, tempoDt * 4.5f);

        gear.angularVelocity = gear.angularVelocity * std::pow(0.55f, tempoDt);
        gear.jitterPhase += (0.6f + high * 1.1f) * tempoDt;
        gear.angle = orientationBase;
    }
}

void Visualizer::renderGears(float animatedTime) const {
    float centerX = windowWidth_ * 0.5f;
    float centerY = windowHeight_ * 0.5f;
    float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));
    float safeMinDim = std::max(minDim, 1.0f);

    float bass = std::clamp(audioFeatures_.bassEnergy, 0.0f, 2.0f);
    float mid = std::clamp(audioFeatures_.midEnergy, 0.0f, 2.0f);
    float high = std::clamp(audioFeatures_.highEnergy, 0.0f, 2.0f);
    float energy = std::clamp(audioFeatures_.energy, 0.0f, 3.0f);
    float idleAttenuation = std::clamp(1.0f - idleState_ * 0.55f, 0.35f, 1.0f);
    bass *= idleAttenuation;
    mid *= idleAttenuation;
    high *= idleAttenuation;
    energy *= idleAttenuation;

    float harmonic = std::clamp(core_.harmonicEnvelope, 0.0f, 2.2f);
    float kick = std::clamp(core_.kickEnvelope, 0.0f, 2.5f);
    float pulse = std::clamp(core_.pulse, 0.0f, 3.0f);
    float glitchPhase = animatedTime * (2.6f + harmonic * 0.8f) + kick * 1.5f;
    float scanPhase = animatedTime * (3.2f + high * 0.5f) + energy * 0.8f;
    float runeRotation = animatedTime * (0.35f + pulse * 0.12f);
    float prismShift = 0.5f + 0.5f * std::sin(animatedTime * 1.8f + pulse * 0.6f);

    float radius = std::max(core_.radius, safeMinDim * 0.05f);
    struct FractalPoint {
        float x;
        float y;
        float angle;
    };

    std::vector<FractalPoint> contour;
    std::vector<FractalPoint> scratch;

    auto generateContour = [&](int detailLevel, float radiusScale, float warpStrength, std::vector<FractalPoint>& out) {
        int segments = std::max(96, detailLevel * 64);
        out.clear();
        out.reserve(segments + 1);

        float baseRadius = radius * radiusScale;
        float warpBase = warpStrength * (0.35f + 0.45f * harmonic);
        float timePhase = animatedTime * (0.8f + 0.45f * energy);

        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            float angle = t * 6.28318f;
            float offset = 0.0f;
            float amplitude = warpBase;
            float freq = 2.0f;

            for (int octave = 0; octave < detailLevel; ++octave) {
                float phase = timePhase * (1.0f + 0.23f * octave)
                              + pulse * 0.35f
                              + harmonic * (0.16f + 0.08f * octave);
                offset += std::sin(angle * freq + phase + octave * 1.3f) * amplitude;
                freq *= 2.2f;
                amplitude *= 0.55f;
            }

            float glitchStep = 0.5f + 0.5f * std::sin(angle * (6.0f + harmonic * 1.7f) + glitchPhase);
            offset += std::sin(angle * (13.0f + high * 2.5f) + glitchPhase * 1.3f) * warpStrength * 0.12f * glitchStep;
            offset += std::sin(angle * (3.0f + harmonic * 1.5f) + timePhase * 1.6f) * warpStrength * 0.25f * kick;
            float radial = baseRadius * (1.0f + offset);
            float x = centerX + std::cos(angle) * radial;
            float y = centerY + std::sin(angle) * radial;
            out.push_back({x, y, angle});
        }
    };

    auto drawLayer = [&](int detailLevel,
                         float radiusScale,
                         float warpStrength,
                         float innerScale,
                         float fillIntensity,
                         const ColorAdjust& fillAdjust,
                         const ColorAdjust& outlineAdjust) {
        generateContour(detailLevel, radiusScale, warpStrength, contour);

        glBegin(GL_TRIANGLE_STRIP);
        for (const auto& point : contour) {
            float dx = point.x - centerX;
            float dy = point.y - centerY;
            float innerX = centerX + dx * innerScale;
            float innerY = centerY + dy * innerScale;
            float baseOsc = std::sin(point.angle * (3.2f + harmonic) + animatedTime * 2.1f);
            float glitchOsc = std::sin(point.angle * 16.0f + glitchPhase);
            float sparkle = 0.55f + 0.45f * baseOsc * (0.6f + 0.4f * std::fabs(glitchOsc));
            float glitchPulse = 0.5f + 0.5f * glitchOsc;

            float innerAlpha = std::clamp(0.14f + fillIntensity * 0.22f, 0.12f, 0.58f);
            float outerAlpha = std::clamp(0.26f + fillIntensity * 0.55f + sparkle * 0.18f + glitchPulse * 0.22f, 0.2f, 0.95f);

            setColorWithAdjust(0.28f + bass * 0.2f + prismShift * 0.08f,
                               0.4f + mid * 0.24f + prismShift * 0.06f,
                               0.6f + high * 0.18f + prismShift * 0.12f,
                               innerAlpha,
                               fillAdjust);
            glVertex2f(innerX, innerY);

            setColorWithAdjust(0.48f + bass * 0.32f + sparkle * 0.18f + prismShift * 0.12f,
                               0.55f + mid * 0.38f + sparkle * 0.16f + prismShift * 0.10f,
                               0.88f + high * 0.48f + sparkle * 0.22f + prismShift * 0.18f,
                               outerAlpha,
                               fillAdjust);
            glVertex2f(point.x, point.y);
        }
        glEnd();

        glLineWidth(1.4f + fillIntensity * 2.0f + high * 0.6f);
        glBegin(GL_LINE_LOOP);
        for (const auto& point : contour) {
            float edgeGlow = 0.3f + 0.7f * std::sin(point.angle * 4.0f + animatedTime * 3.0f + pulse);
            setColorWithAdjust(0.75f + edgeGlow * 0.2f + bass * 0.1f,
                               0.82f + edgeGlow * 0.15f + mid * 0.1f,
                               1.2f + edgeGlow * 0.3f + high * 0.2f,
                               0.28f + fillIntensity * 0.4f,
                               outlineAdjust);
            glVertex2f(point.x, point.y);
        }
        glEnd();
    };

    float layerEnergy = std::clamp(energy * 0.4f + pulse * 0.35f, 0.0f, 2.0f);
    drawLayer(3,
              0.95f + layerEnergy * 0.18f,
              0.28f + harmonic * 0.12f,
              0.38f,
              0.65f + layerEnergy * 0.35f,
              legacyColorAdjust_.circleFill,
              legacyColorAdjust_.circleOutline);

    drawLayer(2,
              0.62f + bass * 0.22f,
              0.35f + harmonic * 0.14f,
              0.42f,
              0.45f + mid * 0.3f,
              legacyColorAdjust_.bloomInner,
              legacyColorAdjust_.circleOutline);

    drawLayer(1,
              0.38f + std::clamp(idleState_, 0.0f, 1.0f) * 0.25f,
              0.42f + high * 0.1f,
              0.52f,
              0.32f + bass * 0.25f,
              legacyColorAdjust_.bloomOuter,
              legacyColorAdjust_.circleOutline);

    generateContour(5,
                    1.12f + layerEnergy * 0.2f,
                    0.46f + harmonic * 0.18f,
                    scratch);
    size_t shardCount = scratch.size();
    if (shardCount >= 3) {
        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < shardCount; ++i) {
            size_t j = (i + 1) % shardCount;
            const auto& a = scratch[i];
            const auto& b = scratch[j];
            float midAngle = 0.5f * (a.angle + b.angle);
            float gate = 0.5f + 0.5f * std::sin(midAngle * (8.0f + harmonic * 1.5f) + glitchPhase * 1.6f);
            float burst = 0.5f + 0.5f * std::sin(midAngle * 14.0f + scanPhase);
            if (gate < 0.72f && j != 0) {
                continue;
            }

            float extrude = radius * (0.08f + 0.15f * gate + 0.12f * burst + 0.08f * high);
            float cx = centerX + std::cos(midAngle) * (radius * 0.42f + extrude);
            float cy = centerY + std::sin(midAngle) * (radius * 0.42f + extrude);
            float alpha = std::clamp(0.18f + gate * 0.35f + burst * 0.25f, 0.16f, 0.88f);

            setColorWithAdjust(0.65f + bass * 0.28f + prismShift * 0.2f,
                               0.82f + mid * 0.35f + prismShift * 0.18f,
                               1.25f + high * 0.5f + gate * 0.25f,
                               alpha,
                               legacyColorAdjust_.bloomOuter);
            glVertex2f(cx, cy);
            glVertex2f(a.x, a.y);
            glVertex2f(b.x, b.y);
        }
        glEnd();
    }

    int runeBranches = 6;
    glLineWidth(2.2f + prismShift * 1.4f + harmonic * 0.4f);
    glBegin(GL_LINES);
    for (int i = 0; i < runeBranches; ++i) {
        float baseAngle = runeRotation + (static_cast<float>(i) / runeBranches) * 6.28318f;
        float jitter = 0.12f * std::sin(glitchPhase + i * 1.37f + pulse * 0.5f);
        float inner = radius * (0.16f + 0.08f * bass + 0.04f * prismShift);
        float outer = radius * (0.62f + 0.28f * harmonic + 0.12f * energy);

        setColorWithAdjust(0.78f + prismShift * 0.22f + bass * 0.1f,
                           0.85f + prismShift * 0.18f + mid * 0.12f,
                           1.3f + prismShift * 0.26f + high * 0.22f,
                           0.32f + energy * 0.35f,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + std::cos(baseAngle - jitter) * inner,
                   centerY + std::sin(baseAngle - jitter) * inner);
        glVertex2f(centerX + std::cos(baseAngle + jitter) * outer,
                   centerY + std::sin(baseAngle + jitter) * outer);
    }
    glEnd();

    glLineWidth(1.1f + high * 0.5f + pulse * 0.3f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 8; ++i) {
        float t = static_cast<float>(i) / 8.0f;
        float angle = runeRotation * 1.5f + t * 6.28318f + 0.12f * std::sin(glitchPhase + i * 0.9f);
        float radial = radius * (0.28f + 0.1f * ((i % 2 == 0) ? prismShift : harmonic * 0.35f));
        setColorWithAdjust(0.6f + bass * 0.25f + prismShift * 0.15f,
                           0.78f + mid * 0.3f + prismShift * 0.12f,
                           1.15f + high * 0.42f + prismShift * 0.2f,
                           0.26f + energy * 0.28f,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(centerX + std::cos(angle) * radial,
                   centerY + std::sin(angle) * radial);
    }
    glEnd();

    glLineWidth(1.0f + high * 0.5f);
    glBegin(GL_LINES);
    for (int i = -2; i <= 2; ++i) {
        float stripeT = static_cast<float>(i) / 2.0f;
        float offset = std::sin(scanPhase + stripeT * 3.6f) * radius * 0.45f;
        float y = centerY + offset;
        float alpha = std::clamp(0.18f + energy * 0.2f + std::cos(scanPhase * 1.7f + i) * 0.12f, 0.08f, 0.45f);

        setColorWithAdjust(0.45f + bass * 0.2f + prismShift * 0.15f,
                           0.65f + mid * 0.3f + prismShift * 0.1f,
                           0.95f + high * 0.4f + prismShift * 0.2f,
                           alpha,
                           legacyColorAdjust_.bloomInner);
        glVertex2f(centerX - radius * (1.15f + stripeT * 0.2f), y);
        glVertex2f(centerX + radius * (1.15f + stripeT * 0.2f), y);
    }
    glEnd();

    int spokeCount = 10 + static_cast<int>(high * 7.0f) + static_cast<int>(pulse * 1.5f) + static_cast<int>(prismShift * 6.0f);
    glLineWidth(1.5f + harmonic * 0.5f + energy * 0.4f + prismShift * 0.6f);
    glBegin(GL_LINES);
    for (int i = 0; i < spokeCount; ++i) {
        float t = static_cast<float>(i) / spokeCount;
        float angle = t * 6.28318f + animatedTime * (0.6f + 0.3f * energy);
        float innerRadius = radius * (0.24f + 0.12f * std::sin(angle * 2.3f + pulse));
        float outerRadius = radius * (0.92f + 0.55f * std::sin(angle * 3.6f + harmonic * 1.3f));

        float innerX = centerX + std::cos(angle) * innerRadius;
        float innerY = centerY + std::sin(angle) * innerRadius;
        float outerX = centerX + std::cos(angle) * outerRadius;
        float outerY = centerY + std::sin(angle) * outerRadius;

        float spokeAlpha = std::clamp(0.22f + energy * 0.35f + high * 0.2f + prismShift * 0.25f, 0.18f, 0.95f);
        setColorWithAdjust(0.55f + bass * 0.25f + prismShift * 0.15f,
                           0.78f + mid * 0.3f + prismShift * 0.12f,
                           1.15f + high * 0.45f + prismShift * 0.2f,
                           spokeAlpha,
                           legacyColorAdjust_.circleOutline);
        glVertex2f(innerX, innerY);
        glVertex2f(outerX, outerY);
    }
    glEnd();

    generateContour(4,
                    1.08f + layerEnergy * 0.16f,
                    0.34f + harmonic * 0.14f,
                    scratch);
    glPointSize(3.0f + std::clamp(energy, 0.0f, 1.2f) * 6.0f + high * 3.0f + prismShift * 2.5f);
    glBegin(GL_POINTS);
    size_t step = std::max<size_t>(1, scratch.size() / (18 + static_cast<int>(high * 12.0f)));
    for (size_t i = 0; i < scratch.size(); i += step) {
        const auto& point = scratch[i];
        float sparkle = 0.6f + 0.4f * std::sin(point.angle * 5.0f + animatedTime * 4.0f + kick);
        setColorWithAdjust(0.65f + bass * 0.25f + sparkle * 0.2f + prismShift * 0.15f,
                           0.82f + mid * 0.35f + sparkle * 0.2f + prismShift * 0.12f,
                           1.25f + high * 0.5f + sparkle * 0.25f + prismShift * 0.22f,
                           0.4f + 0.45f * sparkle,
                           legacyColorAdjust_.bloomInner);
        glVertex2f(point.x, point.y);
    }
    glEnd();
}

void Visualizer::seedLifeFromCore(float amount) {
    (void)amount;
}

void Visualizer::updateLife(float dt, const AudioAnalyzer::AudioFeatures& features) {
    (void)dt;
    (void)features;
}

void Visualizer::renderLife(float animatedTime) const {
    (void)animatedTime;
}

void Visualizer::renderIdleSpinner(float animatedTime) const {
    float idleEnergy = std::clamp(audioFeatures_.energy, 0.0f, 1.0f);
    if (idleEnergy > 0.08f) {
        return;
    }

    float centerX = windowWidth_ * 0.5f;
    float centerY = windowHeight_ * 0.5f;
    float minDim = static_cast<float>(std::min(windowWidth_, windowHeight_));
    float spinnerRadius = std::clamp(minDim * 0.08f, 18.0f, 64.0f);

    float visibility = std::clamp(1.0f - idleEnergy * 6.0f, 0.0f, 1.0f);
    float phase = animatedTime * (1.2f + 0.8f * visibility);
    int segments = 12;

    glPushMatrix();
    glTranslatef(centerX, centerY, 0.0f);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angle = phase + t * 6.28318f;
        float intensity = std::pow(t, 1.8f) * visibility;

        float innerRadius = spinnerRadius * (0.45f + 0.25f * intensity);
        float outerRadius = spinnerRadius * (0.8f + 0.35f * intensity);

        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        float r = 0.2f + 0.6f * intensity;
        float g = 0.4f + 0.4f * intensity;
        float b = 0.9f;
        float a = 0.18f + 0.55f * intensity;
        glColor4f(r, g, b, a);

        glVertex2f(cosA * innerRadius, sinA * innerRadius);
        glVertex2f(cosA * outerRadius, sinA * outerRadius);
    }
    glEnd();

    glDisable(GL_BLEND);
    glPopMatrix();
}

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform float uTime;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uEnergy;
uniform float uOnset;
uniform float uBeat;
uniform vec2 uResolution;

// Raymarching SDF functions
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sceneSDF(vec3 p) {
    float bassPulse = 1.0 + uBass * 3.0;
    float midWobble = sin(uTime * 2.0 + uMid * 10.0) * 0.2;
    float highNoise = uHigh * 0.1;
    
    // Reactive geometry
    vec3 p1 = p;
    p1.y += sin(uTime + p1.x * 2.0) * uMid * 0.5;
    
    float sphere = sdSphere(p1, 0.5 + bassPulse * 0.3);
    float box = sdBox(p + vec3(0.0, sin(uTime) * uMid, 0.0), vec3(0.3 + uBass));
    float torus = sdTorus(p.xzy, vec2(0.8 + uBass * 0.5, 0.1 + uHigh * 0.2));
    
    // Combine shapes
    float result = min(sphere, box);
    result = min(result, torus);
    
    // Add some fractal-like detail when there's high energy
    if (uEnergy > 0.3) {
        float detail = sdSphere(p * 3.0 + vec3(sin(uTime * 5.0), cos(uTime * 3.0), sin(uTime * 7.0)), 0.1);
        result = mix(result, detail, uHigh * 0.3);
    }
    
    return result;
}

vec3 getNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
        sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
        sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)
    ));
}

vec3 raymarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    int maxSteps = 100;
    float maxDist = 10.0;
    
    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t;
        float d = sceneSDF(p);
        
        if (d < 0.001) {
            // Hit!
            vec3 normal = getNormal(p);
            
            // Psychedelic lighting based on audio
            vec3 lightDir = normalize(vec3(
                sin(uTime * 0.5) * uMid,
                cos(uTime * 0.3) * uBass,
                sin(uTime * 0.7) * uHigh
            ));
            
            float diff = max(dot(normal, lightDir), 0.0);
            
            // Color based on frequency bands
            vec3 baseColor = vec3(uBass, uMid, uHigh);
            vec3 color = baseColor * diff;

            float distToCenter = length(p);
            float coreEnvelope = exp(-distToCenter * (2.2 - uBass * 0.6)) * (0.6 + uEnergy * 0.8);
            float massGlow = smoothstep(0.0, 0.8 + uEnergy * 0.4, coreEnvelope);
            vec3 coreColor = mix(baseColor, vec3(1.0), 0.25 + uHigh * 0.35);
            color = mix(color, coreColor * (0.8 + uBass * 0.9), massGlow);
            color += coreColor * coreEnvelope * 0.35;

            // Add glow on beats
            if (uBeat > 0.5) {
                color *= 2.0;
            }
            
            // Add onset flash
            if (uOnset > 0.5) {
                color = mix(color, vec3(1.0), 0.5);
            }
            
            return color;
        }
        
        t += d;
        if (t > maxDist) break;
    }
    
    // Background - dark with subtle audio-reactive gradient
    return vec3(0.02 + uEnergy * 0.05, 0.01 + uMid * 0.03, 0.03 + uHigh * 0.04);
}

void main() {
    vec2 uv = (TexCoord - 0.5) * 2.0;
    uv.x *= uResolution.x / uResolution.y;
    
    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 rd = normalize(vec3(uv, -1.0));
    
    // Camera movement based on audio
    ro.x += sin(uTime * 0.5) * uMid * 0.5;
    ro.y += cos(uTime * 0.3) * uBass * 0.3;
    
    vec3 color = raymarch(ro, rd);
    
    // Post-processing
    color = pow(color, vec3(0.8)); // Gamma correction
    color = smoothstep(0.0, 1.0, color); // Contrast
    
    // Add scanlines for retro rave feel
    float scanline = sin(TexCoord.y * uResolution.y * 2.0) * 0.02;
    color -= scanline;
    
    FragColor = vec4(color, 1.0);
}
)";

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
      colorRandomTimer_(0.0f), deltaTime_(0.0f), rng_(std::random_device{}()), currentPresetIndex_(0),
      onsetColorCyclingEnabled_(true), onsetTriggerCount_(0), lastOnsetActive_(false),
      tempoMultiplier_(1.0f),
      mixColorSchemes_(true),
      overlayLegacyOnModern_(false),
      coreShowBase_(false),
      coreShowCorona_(false),
      coreShowSpokes_(true),
      coreShowRunes_(true),
      coreShowSparkles_(true),
      coreShowBloom_(true),
      audioInputGain_(1.0f),
      showCornerOrbs_(true),
      showProceduralLayer_(true),
      proceduralLayerDebug_(false),
      proceduralLayerOpacity_(0.85f),
      proceduralLayerMode_(3),
      postProcessMode_(2),
      postProcessStrength_(1.0f),
      legacyMotionBlend_(0.0f), legacyMotionPhase_(0.0f), legacySensitivity_(1.0f),
      showLegacyCore_(true), showLegacyArcs_(true),
      showLegacyWaveforms_(false),
      useModernPipeline_(false) {
    waveformBuffer_.resize(512); // Same as audio buffer size
    buildColorPresets();
    if (!colorPresets_.empty()) {
        applyLegacyPreset(0);
    } else {
        legacyColorAdjust_ = LegacyColorAdjust{};
    }
    setupDeviceList();
    core_ = {};
    idleState_ = 0.0f;
    idlePhase_ = 0.0f;
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
        useModernPipeline_ = false;
    } else {
        useModernPipeline_ = shader_ != nullptr;
    }

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

void Visualizer::setColorWithAdjust(float r, float g, float b, float a, const ColorAdjust& adjust) const {
    auto clampComponent = [](float value) {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    };

    glColor4f(
        clampComponent(r * adjust.r),
        clampComponent(g * adjust.g),
        clampComponent(b * adjust.b),
        clampComponent(a * adjust.a)
    );
}

void Visualizer::buildColorPresets() {
    colorPresets_.clear();

    auto makeAdjust = [](float r, float g, float b, float a) {
        ColorAdjust adjust;
        adjust.r = r;
        adjust.g = g;
        adjust.b = b;
        adjust.a = a;
        return adjust;
    };

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(1.55f, 0.45f, 1.65f, 1.0f);
        preset.circleOutline = makeAdjust(1.70f, 0.50f, 1.85f, 1.0f);
        preset.bloomInner = makeAdjust(1.25f, 0.55f, 1.70f, 0.92f);
        preset.bloomOuter = makeAdjust(0.55f, 0.60f, 1.50f, 0.50f);
        preset.bassBars = makeAdjust(1.45f, 0.50f, 1.65f, 1.0f);
        preset.midBars = makeAdjust(1.35f, 0.60f, 1.70f, 1.0f);
        preset.highBars = makeAdjust(1.20f, 0.70f, 1.80f, 1.0f);
        preset.beatExplosion = makeAdjust(1.60f, 0.50f, 1.80f, 1.0f);
        preset.waveform = makeAdjust(1.20f, 0.60f, 1.60f, 1.0f);
        colorPresets_.push_back({"Neon Mirage", preset});
    }

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(0.55f, 1.25f, 1.85f, 1.0f);
        preset.circleOutline = makeAdjust(0.60f, 1.30f, 1.90f, 1.0f);
        preset.bloomInner = makeAdjust(0.40f, 1.20f, 1.70f, 0.90f);
        preset.bloomOuter = makeAdjust(0.30f, 0.90f, 1.40f, 0.48f);
        preset.bassBars = makeAdjust(0.50f, 1.40f, 1.80f, 1.0f);
        preset.midBars = makeAdjust(0.45f, 1.50f, 1.90f, 1.0f);
        preset.highBars = makeAdjust(0.40f, 1.60f, 1.95f, 1.0f);
        preset.beatExplosion = makeAdjust(1.30f, 0.60f, 1.70f, 1.0f);
        preset.waveform = makeAdjust(0.45f, 1.35f, 1.85f, 1.0f);
        colorPresets_.push_back({"Synthwave Cyan", preset});
    }

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(1.60f, 0.55f, 0.95f, 1.0f);
        preset.circleOutline = makeAdjust(1.70f, 0.60f, 0.80f, 1.0f);
        preset.bloomInner = makeAdjust(1.50f, 0.70f, 0.60f, 0.88f);
        preset.bloomOuter = makeAdjust(1.20f, 0.55f, 0.40f, 0.48f);
        preset.bassBars = makeAdjust(1.50f, 0.65f, 0.60f, 1.0f);
        preset.midBars = makeAdjust(1.45f, 0.80f, 0.55f, 1.0f);
        preset.highBars = makeAdjust(1.60f, 0.70f, 0.50f, 1.0f);
        preset.beatExplosion = makeAdjust(1.80f, 0.90f, 0.45f, 1.0f);
        preset.waveform = makeAdjust(1.35f, 0.70f, 0.55f, 1.0f);
        colorPresets_.push_back({"Magenta Ember", preset});
    }

    {
        LegacyColorAdjust preset;
        preset.circleFill = makeAdjust(0.65f, 0.90f, 1.50f, 1.0f);
        preset.circleOutline = makeAdjust(0.60f, 1.00f, 1.60f, 1.0f);
        preset.bloomInner = makeAdjust(0.50f, 0.80f, 1.40f, 0.88f);
        preset.bloomOuter = makeAdjust(0.35f, 0.60f, 1.20f, 0.48f);
        preset.bassBars = makeAdjust(0.60f, 1.20f, 1.60f, 1.0f);
        preset.midBars = makeAdjust(0.55f, 1.30f, 1.70f, 1.0f);
        preset.highBars = makeAdjust(0.50f, 1.40f, 1.85f, 1.0f);
        preset.beatExplosion = makeAdjust(1.40f, 0.60f, 1.70f, 1.0f);
        preset.waveform = makeAdjust(0.55f, 1.20f, 1.70f, 1.0f);
        colorPresets_.push_back({"Night Circuit", preset});
    }
}

void Visualizer::resetLegacyColorAdjustments() {
    if (currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int>(colorPresets_.size())) {
        legacyColorAdjust_ = colorPresets_[currentPresetIndex_].adjust;
    } else {
        legacyColorAdjust_ = LegacyColorAdjust{};
    }
    colorRandomTimer_ = 0.0f;
}

void Visualizer::applyLegacyPreset(int index) {
    if (index < 0 || index >= static_cast<int>(colorPresets_.size())) {
        return;
    }

    legacyColorAdjust_ = colorPresets_[index].adjust;
    currentPresetIndex_ = index;
    colorRandomTimer_ = 0.0f;
}

void Visualizer::randomizeLegacyColors() {
    if (colorPresets_.empty()) {
        return;
    }

    std::uniform_int_distribution<int> dist(0, static_cast<int>(colorPresets_.size()) - 1);

    if (!mixColorSchemes_ || colorPresets_.size() == 1) {
        int nextIndex = dist(rng_);
        if (colorPresets_.size() > 1) {
            int guard = 0;
            while (nextIndex == currentPresetIndex_ && guard < 8) {
                nextIndex = dist(rng_);
                ++guard;
            }
        }
        applyLegacyPreset(nextIndex);
        return;
    }

    // Mix different presets across visual layers
    LegacyColorAdjust mixed = legacyColorAdjust_;

    auto pickAdjust = [&](ColorAdjust LegacyColorAdjust::*member) {
        int idx = dist(rng_);
        const auto& preset = colorPresets_[idx].adjust;
        ColorAdjust value = preset.*member;
        mixed.*member = value;
    };

    pickAdjust(&LegacyColorAdjust::circleFill);
    pickAdjust(&LegacyColorAdjust::circleOutline);
    pickAdjust(&LegacyColorAdjust::bloomInner);
    pickAdjust(&LegacyColorAdjust::bloomOuter);
    pickAdjust(&LegacyColorAdjust::bassBars);
    pickAdjust(&LegacyColorAdjust::midBars);
    pickAdjust(&LegacyColorAdjust::highBars);
    pickAdjust(&LegacyColorAdjust::beatExplosion);
    pickAdjust(&LegacyColorAdjust::waveform);

    legacyColorAdjust_ = mixed;
    colorRandomTimer_ = 0.0f;
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
            randomizeLegacyColors();
        }
    }

    if (onsetColorCyclingEnabled_) {
        bool onsetActive = audioFeatures_.onset > 0.5f;
        if (onsetActive && !lastOnsetActive_) {
            ++onsetTriggerCount_;
            if (onsetTriggerCount_ >= 2) {
                onsetTriggerCount_ = 0;
                if (!colorPresets_.empty()) {
                    int nextIndex = (currentPresetIndex_ + 1) % static_cast<int>(colorPresets_.size());
                    applyLegacyPreset(nextIndex);
                } else {
                    randomizeLegacyColors();
                }
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

    float targetTempo = 1.0f;
    float bpm = features.bpm;
    if (bpm > 30.0f) {
        targetTempo = std::clamp(bpm / 120.0f, 0.35f, 1.8f);
    } else {
        float energy = std::clamp(features.energy, 0.0f, 1.2f);
        targetTempo = 0.75f + energy * 0.35f;
    }

    tempoMultiplier_ = std::clamp(tempoMultiplier_ * 0.9f + targetTempo * 0.1f, 0.3f, 2.0f);
}

void Visualizer::updateAudioBuffer(const std::vector<float>& audioBuffer) {
    if (audioBuffer.size() >= waveformBuffer_.size()) {
        std::copy(audioBuffer.begin(), audioBuffer.begin() + waveformBuffer_.size(), waveformBuffer_.begin());
    }
}

void Visualizer::render() {
    overlayLegacyOnModern_ = true;

    bool usePost = showPostProcess_ && postProcessMode_ != 0 && postProcessStrength_ > 0.0f;

    if (usePost) {
        postProcessor_.beginCapture(windowWidth_, windowHeight_);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    renderLegacyVisualization();

    renderProceduralLayer();

    renderIdleSpinner(time_);

    if (showCornerOrbs_) {
        renderCornerOrbs();
    }

    if (imguiInitialized_ && showImGuiWindow_) {
        renderImGui();
    } else if (!imguiInitialized_) {
        renderGUI();
    }

    if (usePost) {
        postProcessor_.endCapture();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        float strength = std::clamp(postProcessStrength_, 0.0f, 1.0f);
        postProcessor_.apply(postProcessMode_, strength, time_);
    }
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

    // clip space corner positions with size factors
    struct CornerVertex {
        float x;
        float y;
        float size;
        float orbitRadius;
        float orbitPhase;
        float profile;
    };

    std::array<CornerVertex, 4> corners = {
        CornerVertex{-0.88f,  0.88f, 0.075f, 0.020f,  0.0f, 0.10f},
        CornerVertex{ 0.88f,  0.88f, 0.072f, 0.022f,  1.3f, 0.35f},
        CornerVertex{-0.88f, -0.88f, 0.078f, 0.024f, -1.6f, 0.55f},
        CornerVertex{ 0.88f, -0.88f, 0.074f, 0.021f,  2.2f, 0.78f}
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

    renderModernCore();
    if (showCornerOrbs_) {
        renderCornerOrbs();
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
    coreShader_->setUniform1f("uShowBase", coreShowBase_ ? 1.0f : 0.0f);
    coreShader_->setUniform1f("uShowCorona", coreShowCorona_ ? 1.0f : 0.0f);
    coreShader_->setUniform1f("uShowSpokes", 0.0f);
    coreShader_->setUniform1f("uShowRunes", 0.0f);
    coreShader_->setUniform1f("uShowSparkles", 0.0f);
    coreShader_->setUniform1f("uShowBloom", 0.0f);
    coreShader_->setUniform4f("uLegacyCircleFill",
                              legacyColorAdjust_.circleFill.r,
                              legacyColorAdjust_.circleFill.g,
                              legacyColorAdjust_.circleFill.b,
                              legacyColorAdjust_.circleFill.a);
    coreShader_->setUniform4f("uLegacyCircleOutline",
                              legacyColorAdjust_.circleOutline.r,
                              legacyColorAdjust_.circleOutline.g,
                              legacyColorAdjust_.circleOutline.b,
                              legacyColorAdjust_.circleOutline.a);
    coreShader_->setUniform4f("uLegacyBloomInner",
                              legacyColorAdjust_.bloomInner.r,
                              legacyColorAdjust_.bloomInner.g,
                              legacyColorAdjust_.bloomInner.b,
                              legacyColorAdjust_.bloomInner.a);
    coreShader_->setUniform4f("uLegacyBloomOuter",
                              legacyColorAdjust_.bloomOuter.r,
                              legacyColorAdjust_.bloomOuter.g,
                              legacyColorAdjust_.bloomOuter.b,
                              legacyColorAdjust_.bloomOuter.a);

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
    
    // Handle number key presses
    for (int i = 0; i < 9; ++i) {
        if (glfwGetKey(window_, GLFW_KEY_1 + i) == GLFW_PRESS) {
            // Find the actual device index
            int actualIndex = -1;
            int count = 0;
            for (int j = 0; j < deviceNames_.size(); ++j) {
                if (!deviceNames_[j].empty()) {
                    if (count == i) {
                        actualIndex = j;
                        break;
                    }
                    count++;
                }
            }
            
            if (actualIndex >= 0) {
                selectedDevice_ = actualIndex;
                showDeviceMenu_ = false;
                return true; // Device changed
            }
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
