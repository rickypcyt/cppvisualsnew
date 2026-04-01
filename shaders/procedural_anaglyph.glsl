// @EFFECT name="Anaglyph Assembly" index=40 desc="Stereoscopic anaglyph with assembly/disassembly" author="Leon Denise"
// Audio-reactive stereoscopic anaglyph inspired by Leon Denise's "Anaglyph Quick Sketch"
// Adapted to the Cascade procedural pipeline with assembly/disassembly behaviour similar to the head shader.

const int kAnaglyphLayerCount = 5;
const int kAnaglyphMarchSteps = 96;
const float kAnaglyphRange = 1.0;
const float kAnaglyphRadius = 0.3;
const float kAnaglyphBlend = 1.5;
const float kAnaglyphBalance = 1.5;
const float kAnaglyphFalloff = 1.9;
const float kAnaglyphDivergence = 0.1;
const float kAnaglyphFieldOfView = 1.5;

float anaglyphRandom(vec2 p) {
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

mat2 anaglyphRot(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

float anaglyphSmoothMin(float a, float b, float r) {
    float h = clamp(0.5 + 0.5 * (b - a) / r, 0.0, 1.0);
    return mix(b, a, h) - r * h * (1.0 - h);
}

float anaglyphHash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float anaglyphNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = anaglyphHash(i + vec3(0, 0, 0));
    float n100 = anaglyphHash(i + vec3(1, 0, 0));
    float n010 = anaglyphHash(i + vec3(0, 1, 0));
    float n110 = anaglyphHash(i + vec3(1, 1, 0));
    float n001 = anaglyphHash(i + vec3(0, 0, 1));
    float n101 = anaglyphHash(i + vec3(1, 0, 1));
    float n011 = anaglyphHash(i + vec3(0, 1, 1));
    float n111 = anaglyphHash(i + vec3(1, 1, 1));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);

    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);

    return mix(nxy0, nxy1, u.z);
}

float anaglyphAudioEnergy() {
    float energy = uBass * 0.5 + uMid * 0.3 + uHigh * 0.2;
    return max(energy, 0.25);
}

float anaglyphAssemblyFactor() {
    return smoothstep(0.12, 0.85, anaglyphAudioEnergy());
}

vec3 anaglyphApplyCamera(vec3 pos) {
    float tiltY = -PI * 0.25 + (sin(uTime * 0.35) + uMid * 0.8) * 0.25;
    float tiltX = -PI * 0.5 + (cos(uTime * 0.27) + uBass * 1.2) * 0.2;
    float twist = sin(uTime * 0.18 + uHigh * 1.8) * 0.35;

    pos.yz *= anaglyphRot(tiltY);
    pos.xz *= anaglyphRot(tiltX);
    pos.xy *= anaglyphRot(twist);
    return pos;
}

float anaglyphCoreGeometry(vec3 pos) {
    pos = anaglyphApplyCamera(pos);
    float a = 1.0;
    float scene = 1.0;
    float t = uTime * 0.2;
    float wave = 1.0 + 0.2 * sin(t * 8.0 - length(pos) * 2.0 + anaglyphAudioEnergy() * 2.5);
    t = floor(t) + pow(fract(t), 0.5);

    for (int i = kAnaglyphLayerCount; i > 0; --i) {
        float rotSeed = cos(t) * kAnaglyphBalance / a + a * 2.0 + t;
        pos.xy *= anaglyphRot(rotSeed);
        pos.zy *= anaglyphRot(sin(t) * kAnaglyphBalance / a + a * 2.0 + t);
        pos.zx *= anaglyphRot(sin(t + float(i)) * kAnaglyphBalance / a + a * 2.0 + t);
        pos = abs(pos) - kAnaglyphRange * a * wave;
        scene = anaglyphSmoothMin(scene, length(pos) - kAnaglyphRadius * a, kAnaglyphBlend * a);
        a /= kAnaglyphFalloff;
    }

    return scene;
}

float anaglyphZoneThreshold(vec3 pos, float assemblyFactor) {
    float normalizedHeight = clamp((pos.y + 2.5) / 5.0, 0.0, 1.0);
    float threshold;
    if (normalizedHeight > 0.75) {
        threshold = 0.45;
    } else if (normalizedHeight > 0.45) {
        threshold = 0.32;
    } else if (normalizedHeight > 0.2) {
        threshold = 0.2;
    } else {
        threshold = 0.08;
    }

    float n = anaglyphNoise(pos * 2.0 + vec3(0.0, uTime * 0.35, uTime * 0.52));
    threshold += (n - 0.5) * 0.12 * (1.0 - assemblyFactor);

    return threshold;
}

float anaglyphMap(vec3 pos) {
    float assemblyFactor = anaglyphAssemblyFactor();
    float threshold = anaglyphZoneThreshold(pos, assemblyFactor);

    if (assemblyFactor <= threshold) {
        return 1000.0;
    }

    float core = anaglyphCoreGeometry(pos);
    float transition = smoothstep(threshold - 0.1, threshold + 0.1, assemblyFactor);
    return mix(1000.0, core, transition);
}

vec3 anaglyphCalcNormal(vec3 pos) {
    const float eps = 0.0015;
    vec3 ex = vec3(eps, 0.0, 0.0);
    vec3 ey = vec3(0.0, eps, 0.0);
    vec3 ez = vec3(0.0, 0.0, eps);

    float dx = anaglyphMap(pos + ex) - anaglyphMap(pos - ex);
    float dy = anaglyphMap(pos + ey) - anaglyphMap(pos - ey);
    float dz = anaglyphMap(pos + ez) - anaglyphMap(pos - ez);

    return normalize(vec3(dx, dy, dz));
}

vec3 anaglyphLook(vec3 eye, vec3 target, vec2 anchor, float fov) {
    vec3 forward = normalize(target - eye);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = normalize(cross(right, forward));
    return normalize(forward * fov + right * anchor.x + up * anchor.y);
}

vec4 anaglyphShadeEye(vec3 eye, vec3 ray, vec2 anchor) {
    float dither = anaglyphRandom(ray.xy + fract(vec2(uTime)));
    float travel = 0.02 + dither * 0.05;

    for (int i = 0; i < kAnaglyphMarchSteps; ++i) {
        vec3 pos = eye + ray * travel;
        float dist = anaglyphMap(pos);

        if (dist < 0.0015) {
            vec3 normal = anaglyphCalcNormal(pos);
            vec3 lightDir = normalize(vec3(-0.6, 0.8, 0.4));
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 halfVec = normalize(lightDir - ray);
            float spec = pow(max(dot(normal, halfVec), 0.0), 32.0);
            float rim = pow(clamp(1.0 + dot(normal, ray), 0.0, 1.0), 3.0);

            float assemblyFactor = anaglyphAssemblyFactor();
            vec3 basePalette = mix(uPrimaryColor, uSecondaryColor, clamp(0.35 + assemblyFactor * 0.5, 0.0, 1.0));

            vec3 color = basePalette * (0.25 + diff * (0.9 + uEnergy * 0.4));
            color += vec3(0.6, 0.5, 0.9) * spec * (0.35 + uHigh * 0.6);
            color += basePalette.bgr * rim * (0.25 + uMid * 0.4);

            float fog = exp(-travel * 0.35);
            vec3 ambient = mix(uPrimaryColor, uSecondaryColor, 0.5) * 0.08;
            color = mix(ambient, color, fog);

            float alpha = clamp(0.4 + diff * 0.4 + spec * 0.2 + assemblyFactor * 0.3, 0.0, 1.0);
            return vec4(clamp(color, 0.0, 1.0), alpha);
        }

        travel += dist * (0.85 + uBass * 0.05);
        if (travel > 18.0) {
            break;
        }
    }

    float assemblyFactor = anaglyphAssemblyFactor();
    float horizon = clamp(anchor.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 bg = mix(uPrimaryColor * 0.1, uSecondaryColor * 0.25, horizon);
    bg += vec3(0.05, 0.08, 0.12) * (0.8 - clamp(length(anchor), 0.0, 1.2)) * (0.3 + assemblyFactor * 0.5);

    return vec4(clamp(bg, 0.0, 1.0), 0.15 + assemblyFactor * 0.2);
}

vec4 renderAnaglyphAssembly(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 anchor = st * 2.0;
    vec3 target = vec3(0.0);

    vec3 eyeLeft = vec3(-kAnaglyphDivergence, 0.0, 5.0);
    vec3 eyeRight = vec3(kAnaglyphDivergence, 0.0, 5.0);

    vec3 rayLeft = anaglyphLook(eyeLeft, target, anchor, kAnaglyphFieldOfView);
    vec3 rayRight = anaglyphLook(eyeRight, target, anchor, kAnaglyphFieldOfView);

    vec4 leftSample = anaglyphShadeEye(eyeLeft, rayLeft, anchor);
    vec4 rightSample = anaglyphShadeEye(eyeRight, rayRight, anchor);

    vec3 color = vec3(leftSample.r, rightSample.g, rightSample.b);

    float assemblyFactor = anaglyphAssemblyFactor();
    color += vec3(0.08, 0.05, 0.1) * assemblyFactor * 0.2;
    color = clamp(color, 0.0, 1.0);
    color = pow(color, vec3(1.0 / 2.2));

    float alpha = clamp(max(leftSample.a, rightSample.a), 0.0, 1.0);
    return vec4(color, alpha);
}
