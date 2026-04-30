#version 330 core

#line 1 0
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
uniform float uCameraZoom;
uniform float uCameraOffsetX;
uniform float uCameraOffsetY;

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

#line 1 1
const float kVoxelFallInterval = 100.0;
const float kVoxelHeightRangeBase = 5.0;
const float kVoxelBumpFactorBase = 2.0;
const float kVoxelFovDegrees = 60.0;
const float kVoxelBpm = 114.0;
const int kVoxelSamples = 3;
const int kVoxelMaxDepth = 3;

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

    float t = voxelCastRay(ro, rd, 60, ro, time, fallSpeed, heightRange, ID, normal, pos, seed);
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

            t = voxelCastRay(ro, rd, 15, ro, time, fallSpeed, heightRange, ID, normal, pos, seed);
            if (t < 0.0) {
                break;
            }
            ro += t * rd;

            f = ro - pos - 0.5;
            n = normalize(f * pow(abs(f), vec3(8.0)) + 0.0001);

            float emissionBoost = 1.0 + bass * 1.2 + high * 0.8;
            acc += mask * voxelEmission(seed) * emissionBoost;
            mask *= voxelObjectColor(seed);
            ro += n * 0.0008;
        }
    }

    acc /= float(kVoxelSamples);
    return clamp(acc, 0.0, 1.0);
}

const float kVolIterations = 17.0;
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

// Common helper functions for procedural shaders
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float hash31(vec3 p) {
    p = fract(p * vec3(123.34, 456.21, 789.32));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y * p.z);
}

vec3 hsb2rgb(vec3 c) {
    vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    return c.z * mix(vec3(1.0), rgb, c.y);
}

vec3 rotateX(vec3 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec3(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
}

vec3 rotateY(vec3 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec3(p.x * c + p.z * s, p.y, -p.x * s + p.z * c);
}

vec3 rotateZ(vec3 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec3(p.x * c - p.y * s, p.x * s + p.y * c, p.z);
}

vec2 rotate2D(vec2 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec2(p.x * c - p.y * s, p.x * s + p.y * c);
}

vec3 sphericalToRectangular(float rho, float theta, float phi) {
    float phiRad = radians(phi);
    float thetaRad = radians(theta);
    return vec3(
        rho * sin(phiRad) * sin(thetaRad),
        rho * cos(phiRad),
        rho * sin(phiRad) * cos(thetaRad)
    );
}

vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }

vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec2 v) {
    const vec4 C = vec4(0.211324865405187, 0.366025403784439,
            -0.577350269189626, 0.024390243902439);
    vec2 i  = floor(v + dot(v, C.yy) );
    vec2 x0 = v -   i + dot(i, C.xx);
    vec2 i1;
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod(i, 289.0);
    vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
        + i.x + vec3(0.0, i1.x, 1.0 ));
    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
    m = m*m ;
    m = m*m ;
    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
    vec3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

#line 1 2
// Shadertoy compatibility bridge and helper snippets

#ifndef SHADERTOY_BRIDGE_APPLIED
#define SHADERTOY_BRIDGE_APPLIED

#ifndef iTime
#define iTime uTime
#endif

#ifndef iResolution
#define iResolution vec3(uResolution, 1.0)
#endif

#ifndef iChannel0
uniform sampler2D iChannel0;
#endif

#ifndef iChannel1
uniform sampler2D iChannel1;
#endif

#ifndef backbuffer
#define backbuffer iChannel0
#endif
#ifndef resolution
#define resolution (iResolution.xy)
#endif
#ifndef time
#define time iTime
#endif
#ifndef saturate
#define saturate(x) clamp(x, 0.0, 1.0)
#endif
#ifndef linearstep
#define linearstep(edge0, edge1, x) saturate(((x) - (edge0)) / ((edge1) - (edge0)))
#endif
#ifndef remap
#define remap(x, a, b, c, d) mix(c, d, ((x) - (a)) / ((b) - (a)))
#endif
#ifndef remapc
#define remapc(x, a, b, c, d) mix(c, d, linearstep(a, b, x))
#endif

float font(vec2 uv, vec2 id) {
    uv = remapc(uv, vec2(0.0), vec2(1.0), id, id + 1.0) / 16.0;
    const float w = 0.2;
    return smoothstep(0.55, 0.45, textureGrad(iChannel1, uv, dFdx(uv), dFdy(uv)).a);
}

float pinieon(vec2 uv) {
    uv = fract(uv);
    uv = 0.5 + (uv - 0.5) * 0.8;
    vec2 ruv = uv * 5.0;
    vec2 fuv = fract(ruv);
    vec2 iuv = floor(ruv);
    vec2 id = vec2[3](vec2(1.0, 12.0), vec2(15.0, 13.0), vec2(2.0, 12.0))[int(ruv.x + 2.0) % 3];
    return font(0.5 + (fuv - 0.5) * 0.7, id) * float(iuv.y == 2.0) * step(0.5, iuv.x) * step(iuv.x, 3.5);
}

float hjct(vec2 uv) {
    uv = fract(uv);
    uv = 0.5 + (uv - 0.5) * 0.8;
    vec2 ruv = uv * 5.0;
    vec2 fuv = fract(ruv);
    vec2 iuv = floor(ruv);
    vec2 id = vec2[3](vec2(1.0, 12.0), vec2(15.0, 13.0), vec2(2.0, 12.0))[int(ruv.x + 2.0) % 3];
    return font(0.5 + (fuv - 0.5) * 0.7, id) * float(iuv.y == 2.0) * step(0.5, iuv.x) * step(iuv.x, 3.5);
}

#define sat(x) clamp(x, 0.0, 1.0)
#define norm(x) normalize(x)
#define rep(i, n) for (int i = 0; i < (n); ++i)
const float pi = acos(-1.0);
const float tau = 2.0 * pi;

vec3 hash(vec3 x) {
    uvec3 v = floatBitsToUint(x);
    v = v * 20240413u + 1212121212u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return vec3(v) / float(-1u);
}

mat2 rot(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, s, -s, c);
}

mat3 bnt(vec3 T) {
    T = norm(T);
    vec3 N = vec3(0.0, 1.0, 0.0);
    vec3 B = norm(cross(N, T));
    N = norm(cross(T, B));
    return mat3(B, N, T);
}

float iplane(vec3 ro, vec3 rd, vec3 pd, float w) {
    pd = norm(pd);
    float l = -(dot(ro, pd) + w) / dot(rd, pd);
    return (l < 0.0 ? 1e5 : l);
}

float bpm = 125.0;
float alt, lt, tr, bt;
#define sc(x) hash(vec3(1.2, x, bt))

float fui(vec2 suv, float s) {
    s = 1.2;
    suv -= alt * 0.1;
    suv *= rot(floor(sc(3).x * 4.0) * pi / 2.0);
    vec2 ruv = suv;
    rep(i, 4) {
        if (hash(vec3(floor(ruv) + s, i)).x < 0.5) {
            ruv *= 2.0;
        } else {
            break;
        }
    }

    vec3 h = hash(vec3(floor(ruv) + 1.2, s));
    vec2 fuv = fract(ruv);
    float c = 0.0;
    float b = sc(0).x;
    vec2 au = abs((fuv * 2.0 - 1.0) * rot(floor(h.z * 4.0) * pi / 4.0));
    if (b < 0.2) {
        c = pinieon(suv);
    } else if (b < 0.4) {
        c = step(fract(dot(vec2(1.0), suv)), 0.1);
    } else {
        c = step(max(au.x, au.y), 0.4) * step(min(au.x, au.y), 0.05) * step(h.x, 0.5);
    }
    return c;
}

float march(vec3 ro, vec3 rd) {
    int n = 16;
    float l = 1e9;
    rep(i, n) {
        float fi = (float(i) + 0.5) / float(n);
        vec3 pd = norm(tan(hash(vec3(bt, i, 1.2)) * 2.0 - 1.0));
        if (sc(0).z < 0.3) {
            pd = vec3(0.0, 0.0, 1.0);
        }
        float w = mix(-5.0, 5.0, fi);
        float d = iplane(ro, rd, pd, w);
        vec3 rp = rd * d + ro;
        vec2 uv = (rp.x * pd.zy + rp.y * pd.xz + rp.z * pd.xy) * 0.5;
        float s = fui(uv, w);
        if (s > 0.0) {
            l = min(l, d);
        }
    }

    return l;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 fc = fragCoord;
    vec2 res = resolution;
    vec2 uv = fc / res;
    vec2 asp = res / min(res.x, res.y);
    vec2 asp2 = res / max(res.x, res.y);

    alt = lt = time * bpm / 60.0;
    tr = 1.0 - exp(-3.0 * fract(lt));
    bt = floor(lt);
    lt = tr + bt;

    bool tomaru = int(bt / 4.0) % 2 == 0;
    if (tomaru) {
        alt = lt = time * bpm / 60.0 / 4.0;
        tr = fract(lt);
        bt = floor(lt);
    }

    vec2 suv = (uv * 2.0 - 1.0) * asp;

    vec3 ro = vec3(0.0, 0.0, -3.0);
    vec3 dir;
    vec3 rd;
    dir = -ro;
    float sc1y = sc(1).y;
    if (sc1y < 0.3) {
        ro = vec3(mix(-1.0, 1.0, fract(alt)), 0.0, -3.0);
        dir = vec3(0.0, 0.0, 1.0);
    } else if (sc1y < 0.6) {
        ro = vec3(0.0, 0.0, mix(-5.0, -3.0, tr));
        dir = vec3(0.0, 0.0, 1.0);
    } else {
        float a = alt * 0.5;
        ro = vec3(cos(a), 0.0, sin(a)) * 2.0;
        dir = -ro;
    }
    float z = 0.5;
    if (sc(1).x < 0.3) {
        z = mix(0.3, 1.7, tr);
    }
    rd = norm(bnt(dir) * vec3(suv, z));
    float l = march(ro, rd);
    float c = exp(-0.2 * l);

    if (sc(0).y < 0.2) {
        c += hjct(suv * 0.5 + 0.5 + vec2(alt * 0.5, 0.0)) * step(fract(alt * 4.0), 0.5);
        c *= step(abs(suv.y), 0.5);
    }
    if (sc(1).z < 0.3) {
        vec2 ruv = suv * 4.0;
        vec3 h = hash(vec3(floor(ruv), floor(alt * 4.0)));
        vec2 fuv = fract(ruv);
        vec2 au = abs((fuv * 2.0 - 1.0) * rot(floor(h.z * 4.0) * pi / 4.0));
        c += step(max(au.x, au.y), 0.3) * step(min(au.x, au.y), 0.05) * step(h.x, 0.1);
    }
    if (tomaru) {
        float len = length(suv) - mix(0.2, 0.8, tr);
        float nya = step(abs(len), 0.005);
        if (len < 0.0) {
            c = 1.0 - c * 1.5;
        }
        c += nya;
    }
    float ema = 0.7;
    vec3 back = texture(backbuffer, uv).rgb;
    vec3 col = mix(vec3(c, back.rg * 1.5), back, ema);
    fragColor = vec4(col, 1.0);
}

#endif // SHADERTOY_BRIDGE_APPLIED

#line 1 3
// @EFFECT name="Nebula" index=11 desc="Nebula cloud effect with fbm noise" author="System"
// @EFFECT name="ASCII Ocean" index=1 desc="ASCII-style ocean waves" author="System"
// @EFFECT name="Sacred Geometry" index=2 desc="Sacred geometry flower pattern" author="System"
// @EFFECT name="Glitch Grid" index=3 desc="Digital glitch grid pattern" author="System"
// @EFFECT name="Chemical Flow" index=4 desc="Flowing chemical reaction diffusion" author="System"
// @EFFECT name="Crystal Lattice" index=5 desc="Crystal lattice structure" author="System"
// @EFFECT name="Phantom Fractals" index=6 desc="Phantom fractal bands" author="System"

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

    // Doble rotación: capa lenta + capa rápida reactiva al audio
    vec2 p1 = rotate(st, time * (0.25 + tempo * 0.2));
    vec2 p2 = rotate(st, -time * (0.15 + bass * 0.3));

    // FBM en dos capas con escala diferente — más detalle sin coste extra
    float flow1 = fbm(p1 * (3.0 + energy));
    float flow2 = fbm(p2 * (5.5 + mid * 1.5) + vec2(time * 0.1));
    float flow  = mix(flow1, flow2, 0.4 + bass * 0.2);

    // Swirl más orgánico: frecuencias asimétricas y desfase por audio
    float swirl = sin(p1.x * 5.0 + time * 1.4 + bass * 1.2)
                * cos(p1.y * 7.0 - time * 1.1 + mid  * 0.8);

    // Venas — líneas finas que pulsan con los agudos
    float veins = abs(sin(flow * 12.0 + time * 0.3)) * high * 0.4;

    // Brillo central reactivo al bajo
    float radial  = 1.0 - smoothstep(0.0, 0.8, length(st));
    float glow    = radial * bass * 0.5;

    // Color base: tres zonas en vez de dos (sombra / medio / luz)
    vec3 shadow = uPrimaryColor  * 0.2;
    vec3 midCol = mix(uPrimaryColor, uSecondaryColor, 0.5) + vec3(mid * 0.1, bass * 0.05, 0.0);
    vec3 light  = uSecondaryColor + vec3(bass * 0.25, mid * 0.2, high * 0.35);

    vec3 color = flow < 0.45
        ? mix(shadow, midCol, flow / 0.45)
        : mix(midCol, light,  (flow - 0.45) / 0.55);

    // Capas aditivas
    color += vec3(0.08, 0.02, 0.12) * swirl * 0.25;   // tinte swirl
    color += vec3(0.6,  0.3,  0.9)  * veins;           // venas violeta
    color += vec3(0.3,  0.1,  0.5)  * glow;            // halo central

    color = clamp(color, 0.0, 1.0);

    // Alpha: respira con el bajo y se abre con la energía
    float alpha = clamp(0.25 + flow * 0.5 + energy * 0.25 + bass * 0.15, 0.0, 1.0);

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

#line 1 4
// @EFFECT name="Fractal Object" index=7 desc="Rotating fractal object" author="System"
// @EFFECT name="Pulsar Tunnel" index=8 desc="Pulsar spiral tunnel" author="System"
// @EFFECT name="Aurora Bloom" index=9 desc="Aurora curtain effect" author="System"
// @EFFECT name="Ribbon Scanlines" index=10 desc="Ribbon scanline distortion" author="System"
// @EFFECT name="Kaleidoscope Fractal" index=12 desc="Kaleidoscope fractal pattern" author="System"

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

vec3 anaglyphHsv2Rgb(float h) {
    vec3 k = fract(vec3(h, h + 0.333, h + 0.667)) * 6.0 - 3.0;
    return clamp(abs(k) - 1.0, 0.0, 1.0);
}

vec4 renderKaleidoscopeFractal(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {

    float r = length(st);
    float angle = atan(st.y, st.x);

    angle += time * (0.15 + tempo * 0.08);

    float sectors = 6.0 + floor(mid * 8.0);
    float sectorAngle = 2.0 * PI / max(sectors, 1.0);
    angle = mod(angle, sectorAngle);
    angle = abs(angle - sectorAngle * 0.5);

    vec2 dir = vec2(cos(angle), sin(angle));
    vec2 p = dir * r;

    vec2 warp = vec2(
        fbm(p * (3.2 + high * 2.4) + time * 0.35),
        fbm(p * (2.6 + mid  * 1.8) - time * 0.28)
    );
    p += warp * (0.5 + high * 0.6 + energy * 0.35);

    float n1 = fbm(p * (2.4 + energy * 1.3) + time * 0.25);
    float n2 = fbm(p * 4.8 - time * 0.4 + vec2(bass * 0.5));
    float n3 = fbm(p * 9.0 + time * 0.6 + vec2(mid  * 0.3));

    float hShift = time * 0.12 + bass * 0.4;
    float h1 = fract(n1 * 1.5 + hShift);
    float h2 = fract(n2 * 1.2 + hShift + 0.33);
    float h3 = fract(n3 * 0.9 + hShift + 0.66);

    vec3 c1 = anaglyphHsv2Rgb(h1);
    vec3 c2 = anaglyphHsv2Rgb(h2);
    vec3 c3 = anaglyphHsv2Rgb(h3);

    float w1 = n1;
    float w2 = n2 * (0.6 + mid  * 0.5);
    float w3 = n3 * (0.4 + high * 0.6);
    float wTotal = w1 + w2 + w3 + 0.001;

    vec3 color = (c1 * w1 + c2 * w2 + c3 * w3) / wTotal;

    // Darken the overall color palette - make it less colorful
    color *= 0.35; // Reduce brightness significantly
    color = pow(color, vec3(1.4)); // Increase contrast, darkens mid-tones

    vec3 userTint = mix(uPrimaryColor, uSecondaryColor, uColorBlend) * 0.12;
    color = mix(color, color + userTint, 0.25);

    float radialMask = pow(1.0 - smoothstep(0.0, 1.0, r * 0.85), 1.8);
    float noiseMask  = smoothstep(0.02, 0.35, n1); // Sharper threshold for more black areas
    float mask       = radialMask * noiseMask;

    // Create dark hollows where there's no pattern
    float darkness = 1.0 - smoothstep(0.0, 0.5, n1 * n2);
    color *= mask * (0.4 + 0.6 * darkness); // More darkness in empty areas

    float edgeDist = abs(angle) / (sectorAngle * 0.5);
    float edgeLine = exp(-80.0 * pow(1.0 - edgeDist, 2.0)) * (0.25 + high * 0.3);
    color += c1 * edgeLine * mask * 0.6; // Dimmer edges

    float core = exp(-18.0 * r * r) * (0.25 + bass * 0.4);
    color += mix(c2, vec3(0.7), 0.5) * core * 0.5; // Less bright core

    float spark = step(0.92, n3) * step(0.9, n2) * high * 0.5; // Rare, dim sparks
    color += c3 * spark * mask * 0.7;

    color = clamp(color, 0.0, 1.0);

    // Lower alpha for more transparency in dark areas
    float alpha = clamp(mask * (0.35 + n1 * 0.25) + core * 0.5 + spark * 0.3, 0.0, 0.85);

    return vec4(color, alpha);
}

#line 1 5
// @EFFECT name="Voronoi Cells" index=13 desc="Voronoi cell pattern with edge glow" author="System"
// @EFFECT name="Raymarched Object" index=14 desc="3D raymarched metallic object" author="System"
// @EFFECT name="Reaction Diffusion" index=15 desc="Reaction diffusion pattern" author="System"
// @EFFECT name="Liquid Refraction" index=16 desc="Liquid refraction with caustics" author="System"
// @EFFECT name="Starfield Warp" index=17 desc="Warping starfield effect" author="System"
// @EFFECT name="Plasma Classic" index=18 desc="Classic plasma effect" author="System"
// @EFFECT name="Domain Warped Fractal" index=19 desc="Domain warped fractal noise" author="System"
// @EFFECT name="Volumetric Starfield" index=21 desc="Volumetric starfield with depth" author="System"
// @EFFECT name="Fractal Infinity" index=37 desc="Infinite fractal maze" author="System"
// @EFFECT name="Walker" index=38 desc="Walking figure assembly" author="System"
// @EFFECT name="Voxel Path Tracer" index=22 desc="Voxel path tracing scene" author="System"
// @EFFECT name="Fractal Tunnel" index=20 desc="Fractal tunnel journey" author="System"

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
    
    // Fondo negro base
    vec3 color = vec3(0.0);
    
    float temporalSpeed = kVolSpeed + tempo * 0.004 + energy * 0.008;
    float twist = sin(time * (12.0 + high * 6.0)) * 0.1 + 1.0;
    
    // --- ESTRELLAS/FLAKES VISIBLES (luz sobre negro) ---
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
        
        // FORMA VISIBLE: estrella/flake
        float starValue = happyStar(p * 10.0, twist);
        float flake = smoothstep(0.5, 0.0, starValue); // Ajustado para mejor visibilidad
        
        // Color de la estrella - brillante sobre negro
        float paletteBias = clamp(uColorBlend, 0.0, 1.0);
        vec3 starColor = mix(uPrimaryColor, uSecondaryColor, randSeed) * 1.5;
        
        // Variación temporal del brillo
        float twinkle = 0.7 + 0.3 * sin(time * 3.0 + randSeed * 10.0);
        
        // Sumamos luz al fondo negro
        color += starColor * flake * layerFade * twinkle * 0.8;
    }
    
    // --- NEBULOSA VOLUMÉTRICA OSCURA ---
    vec3 dir = normalize(vec3(uv * (kVolZoom + energy * 0.25), 1.0));
    float paletteBias = clamp(uColorBlend, 0.0, 1.0);
    vec3 nebulaColor = mix(uPrimaryColor, uSecondaryColor, paletteBias) * 0.2;
    
    float s = 0.1;
    float fade = 1.0;
    vec3 v = vec3(0.0);
    
    float rotationAngle = time * (0.01 + mid * 0.01);
    mat2 rot = mat2(cos(rotationAngle), sin(rotationAngle),
                   -sin(rotationAngle), cos(rotationAngle));
    
    for (int r = 0; r < kVolSteps; ++r) {
        vec3 pos = nebulaColor + s * dir * 0.5;
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
        
        if (r > 6) fade *= 1.5 - dm;
        
        vec3 weight = vec3(s, s * s, s * s * s * s);
        v += weight * a * (kVolBrightness * 0.5) * fade; // Reducido para no saturar
        
        fade *= kVolDistFading + bass * 0.05;
        s += kVolStepSize;
    }
    
    // Volumen muy oscuro, casi silueta
    v = mix(vec3(length(v) * 0.3), v * 0.5, kVolSaturation);
    color += v * 0.002; // Contribución mínima al color
    
    // --- EFECTOS FINALES ---
    
    // Vignette que oscurece bordes (mantiene centro más visible)
    float vignette = 1.0 - smoothstep(0.3, 1.2, dot(uv, uv));
    color *= vignette;
    
    // Respuesta al audio - pulso de brillo
    color *= 1.0 + energy * 0.3;
    color.r *= 1.0 + bass * 0.2;
    color.g *= 1.0 + mid * 0.15;
    color.b *= 1.0 + high * 0.25;
    
    // Limitamos para evitar saturación
    color = clamp(color, 0.0, 1.0);
    
    // Alpha
    float alpha = clamp(length(color) * 1.5, 0.0, 1.0);
    
    return vec4(color, alpha);
}

float fractalInfinitySDF(vec3 p, mat3 m) {
    float q = 1.0;
    float d = 1e9;
    for(int n = 0; n < 5; n++) {
        p *= m;
        d = min(d,max(max(abs(p.x),max(abs(p.y),abs(p.z))) - 1.0,0.8-min(max(abs(p.x),abs(p.y)),min(max(abs(p.y),abs(p.z)),max(abs(p.z),abs(p.x))))) / q);
        p = abs(p) - 0.9;
        p *= 2.1;
        q *= 2.1;
    }
    return d;
}

vec4 renderFractalInfinity(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 r = uResolution.xy;
    vec3 raypos = vec3(0,0,-5);
    vec3 raydir = normalize(vec3((st + st - r) / sqrt(r.x * r.y),1));
    
    float t = time + tempo * 0.1 + energy * 0.2;
    mat3 rot = mat3(1,0,0,0,1,0,0,0,1);
    for(int j = 0; j < 8; j++) {
        rot *= mat3(cos(t),0,sin(t),0,1,0,-sin(t),0,cos(t));
        rot *= mat3(0,1,0,0,0,1,1,0,0);
        t /= -1.237415;
    }
    
    vec4 c = vec4(1);
    
    for(int i = 0; i < 80; i++) {
        float dist = fractalInfinitySDF(raypos,rot);
        raypos += raydir * dist;
        if(dist < 0.0001) {
            break;
        }
        c /= 1.07;
    }
    
    // Add color based on audio reactivity
    vec3 color = c.rgb;
    color.r *= 1.0 + bass * 0.3;
    color.g *= 1.0 + mid * 0.2;
    color.b *= 1.0 + high * 0.4;
    
    // Mix with user colors
    float paletteBias = clamp(uColorBlend, 0.0, 1.0);
    color = mix(color * uPrimaryColor, color * uSecondaryColor, paletteBias);
    
    float alpha = clamp(length(c.rgb) + energy * 0.3, 0.0, 1.0);
    return vec4(clamp(color, 0.0, 1.0), alpha);
}

vec2 walkerRotate(vec2 inVec, float alpha) {
    float c = cos(alpha);
    float s = sin(alpha);
    return vec2(
        inVec.x * c + inVec.y * s,
        inVec.y * c - inVec.x * s
    );
}

float walkerAudioEnergy(float bass, float mid, float high) {
    float energy = bass * 0.5 + mid * 0.3 + high * 0.2;
    return max(energy, 0.25);
}

float walkerAssemblyFactor(float bass, float mid, float high) {
    return smoothstep(0.15, 0.85, walkerAudioEnergy(bass, mid, high));
}

float walkerZoneThreshold(vec2 uv, float assemblyFactor) {
    float normalizedHeight = clamp((uv.y + 0.9) / 1.8, 0.0, 1.0);
    float threshold;
    if (normalizedHeight > 0.75) {
        threshold = 0.46;
    } else if (normalizedHeight > 0.45) {
        threshold = 0.32;
    } else if (normalizedHeight > 0.2) {
        threshold = 0.2;
    } else {
        threshold = 0.08;
    }

    float noise = sin(uv.x * 11.0 + uTime * 2.1) * cos(uv.y * 7.0 + uTime * 1.6);
    threshold += noise * 0.05 * (1.0 - assemblyFactor);
    return threshold;
}

float walkerBody(vec2 uv, vec2 leftLeg, vec2 rightLeg, vec2 center) {
    float baseRadius = 0.18;
    vec2 leftLeg2 = leftLeg - center;
    vec2 rightLeg2 = rightLeg - center;
    float leftRadius = length(leftLeg2);
    float rightRadius = length(rightLeg2);
    vec2 leftDir = leftLeg2 / max(leftRadius, 1e-4);
    vec2 rightDir = rightLeg2 / max(rightRadius, 1e-4);
    vec2 r = uv - center;
    float lenUV = length(r);

    vec2 uvDir = r / max(lenUV, 1e-4);
    float leftDist = length(uvDir - leftDir);
    float rightDist = length(uvDir - rightDir);
    float leftFactor = pow(max(1.0 - leftDist, 0.0), 3.0);
    float rightFactor = pow(max(1.0 - rightDist, 0.0), 3.0);
    float centerFactor = clamp(1.0 - leftFactor - rightFactor, 0.0, 1.0);

    float radius = leftFactor * leftRadius + rightFactor * rightRadius + centerFactor * baseRadius;
    return lenUV - radius;
}

vec2 getWalkerLegCenter(float angle) {
    vec2 legCenter = vec2(sin(angle), max(cos(angle), 0.0));
    return vec2(0.4, 0.2) * legCenter + vec2(0.0, -0.6);
}

float walkerLeg(vec2 uv, vec2 legCenter) {
    float angle = (legCenter.y + 0.6) * 1.5;
    vec2 diff = uv - legCenter;
    diff = walkerRotate(diff, angle);
    diff.y *= 1.6;
    if (diff.y < 0.0) {
        diff.y *= 5.2;
    }
    return length(diff) - 0.2;
}

float walkerHead(vec2 diff) {
    vec2 diff2 = walkerRotate(diff, -0.4);
    diff2 *= vec2(5.0, 7.0);
    return length(diff2) - 1.0;
}

float walkerCoreDistance(vec2 uv, float time, float energy, float bass, float mid, float high) {
    float progress = 6.66 * time + bass * 0.5 + mid * 0.3;

    vec2 leftLegCenter = getWalkerLegCenter(progress);
    vec2 rightLegCenter = getWalkerLegCenter(progress + PI);

    vec2 achillesOffset = vec2(-0.15, 0.0);

    vec2 bodyCenter = vec2(0.0, -0.05 + 0.1 * sin(progress * 2.0) + energy * 0.1);
    vec2 headCenter = bodyCenter + vec2(0.10 + 0.08 * cos(progress * 2.0 + 0.3), 0.23);

    float leftLegDist = walkerLeg(uv, leftLegCenter);
    float rightLegDist = walkerLeg(uv, rightLegCenter);
    float bodyDist = walkerBody(uv, leftLegCenter + achillesOffset, rightLegCenter + achillesOffset, bodyCenter);
    float headDist = walkerHead(uv - headCenter);

    float dist = min(min(leftLegDist, rightLegDist), min(bodyDist, headDist));

    if (uv.y < -0.6) {
        dist = max(dist, uv.y + 0.6);
    }

    return dist;
}

float walkerDistance(vec2 uv, float time, float energy, float bass, float mid, float high, float assemblyFactor) {
    float baseDist = walkerCoreDistance(uv, time, energy, bass, mid, high);
    float threshold = walkerZoneThreshold(uv, assemblyFactor);
    float gating = smoothstep(threshold - 0.12, threshold + 0.12, assemblyFactor);
    return mix(0.6, baseDist, gating);
}

vec4 renderWalker(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Apply exact raymarched object coordinate system with proper aspect ratio
    vec2 uv = st;
    float aspect = uResolution.x / uResolution.y;
    uv.x *= aspect;
    uv *= 1.6;  // Small change from 1.5 to 1.6

    float assemblyFactor = walkerAssemblyFactor(bass, mid, high);

    float dist = walkerDistance(uv, time, energy, bass, mid, high, assemblyFactor);
    float distY = walkerDistance(uv - vec2(0.0, 0.01), time, energy, bass, mid, high, assemblyFactor);
    float distX = walkerDistance(uv - vec2(0.01, 0.0), time, energy, bass, mid, high, assemblyFactor);

    float mask = smoothstep(0.0, -0.02, dist);
    float edge = abs(mask - smoothstep(0.0, -0.02, distY));
    edge += abs(mask - smoothstep(0.0, -0.02, distX));

    float outline = smoothstep(0.0, 0.025, edge);
    float glow = exp(-35.0 * abs(dist)) * (0.4 + assemblyFactor * 0.6);

    float paletteBias = clamp(uColorBlend, 0.0, 1.0);
    vec3 basePalette = mix(uPrimaryColor, uSecondaryColor, paletteBias);
    vec3 edgeColor = mix(basePalette, vec3(1.0), 0.35 + assemblyFactor * 0.3);

    vec3 color = outline * edgeColor;
    color += glow * mix(vec3(0.1, 0.15, 0.2), edgeColor, clamp(energy * 0.6, 0.0, 1.0));

    color.r *= 1.0 + bass * 0.35;
    color.g *= 1.0 + mid * 0.3;
    color.b *= 1.0 + high * 0.4;

    color = clamp(color, 0.0, 1.0);
    float alpha = clamp(outline * (0.55 + assemblyFactor * 0.45), 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderVoxelPathTracer(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    
    // Dynamic resolution scaling for performance
    float pixelCount = uResolution.x * uResolution.y;
    float resolutionScale = 1.0;
    if (pixelCount > 1920.0 * 1080.0) {
        resolutionScale = 0.5;
    } else if (pixelCount > 1280.0 * 720.0) {
        resolutionScale = 0.7;
    }
    uv *= resolutionScale;

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

vec4 renderFractalTunnel(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st * (0.55 + energy * 0.15);
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

#line 1 6
// @EFFECT name="Etienne Pulse" index=23 desc="Pulse grid by Etienne" author="etiennejcb"
// @EFFECT name="Fractal Runway" index=24 desc="Temporal fractal runway" author="etiennejcb"
// @EFFECT name="Volumetric Tunnel" index=25 desc="Volumetric streak tunnel" author="System"
// @EFFECT name="Chromatic Swirl" index=26 desc="Sine-based chromatic swirl" author="System"

// by @etiennejcb

float etienne_t;
float etienne_pulseTime;
float etienne_pulseTime2;
float etienne_period;
float etienne_alt;
float etienne_lt;
float etienne_tr;
float etienne_bt;

float etienne_pmap(float x, float a, float b, float c, float d) {
    float progress = (x - a) / (b - a);
    return c + (d - c) * progress;
}

float etienne_timeMoves(float x, float duration, float part, float g) {
    float md = mod(x, duration);
    float start = floor(x / duration) * duration;
    float nm = md / duration;
    float change = clamp(etienne_pmap(nm, part, 1.0, 0.0, 1.0), 0.0, 1.0);
    change = 1.0 - pow(1.0 - change, g);
    return start + change * duration;
}

mat2 etienne_rot2D(float a) {
    float cs = cos(a);
    float sn = sin(a);
    return mat2(cs, -sn, sn, cs);
}

vec3 etienne_aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float etienne_discre(float a) {
    return floor(mod(mod(a, 2.0) + 2.0, 2.0));
}

float etienne_discre2(float a) {
    float modulo = mod(mod(a, 2.0) + 2.0, 2.0);
    float type = floor(modulo);
    return type == 0.0 ? 0.0 : mod(8.0 * modulo, 2.0);
}

#define etienne_sc(x) hash(vec3(1.2, x, etienne_bt))

float etienne_field1(vec2 pos0, float q) {
    vec2 pos = pos0;
    pos *= etienne_rot2D(-0.3 * etienne_pulseTime2);
    return q - 2.5 * (1.5 * abs(pos.y) + 1.0 * abs(pos.x) + 0.3 * sin(1.2 * pos.x + 1.3 * etienne_pulseTime2));
}

float etienne_field2(vec2 pos, float q) {
    return q - 3.3 * length(pos);
}

float etienne_field3(vec2 pos, float q) {
    return q + 7.0 * length(pos);
}

float etienne_field4(vec2 pos, float q) {
    return q + 13.0 * abs(pos.x);
}

float etienne_pcol(vec2 uv, float q, float q2) {
    float wavyOffset = 1.3 * sin(3.0 * uv.x + 3.0 * etienne_pulseTime) +
                       1.5 * sin(4.0 * uv.y + 1.1 + 3.0 * etienne_pulseTime);
    float col = etienne_discre(wavyOffset +
                               etienne_discre(etienne_field4(uv, q)) +
                               etienne_discre(etienne_field3(uv, q)) +
                               etienne_discre(etienne_discre2(etienne_field1(uv, q)) +
                                              etienne_discre2(etienne_field2(uv, q))));
    return col;
}

float etienne_fui(vec2 suv, float s) {
    s = 1.2;
    suv -= etienne_alt * 0.1;
    suv *= etienne_rot2D(floor(etienne_sc(3).x * 4.0) * (pi / 2.0));
    vec2 ruv = suv;
    for (int i = 0; i < 4; ++i) {
        if (hash(vec3(floor(ruv) + s, float(i))).x < 0.5) {
            ruv *= 2.0;
        } else {
            break;
        }
    }

    vec3 h = hash(vec3(floor(ruv) + 1.2, s));
    vec2 fuv = fract(ruv);
    float c = 0.0;
    float b = etienne_sc(0).x;
    vec2 au = abs((fuv * 2.0 - 1.0) * etienne_rot2D(floor(h.z * 4.0) * (pi / 4.0)));
    if (b < 0.2) {
        c = pinieon(suv);
    } else if (b < 0.4) {
        c = step(fract(dot(vec2(1.0), suv)), 0.1);
    } else {
        c = step(max(au.x, au.y), 0.4) * step(min(au.x, au.y), 0.05) * step(h.x, 0.5);
    }
    return c;
}

float etienne_march(vec3 ro, vec3 rd) {
    int n = 16;
    float l = 1e9;
    for (int i = 0; i < n; ++i) {
        float fi = (float(i) + 0.5) / float(n);
        vec3 pd = norm(tan(hash(vec3(etienne_bt, float(i), 1.2)) * 2.0 - 1.0));
        if (etienne_sc(0).z < 0.3) {
            pd = vec3(0.0, 0.0, 1.0);
        }
        float w = mix(-5.0, 5.0, fi);
        float d = iplane(ro, rd, pd, w);
        vec3 rp = rd * d + ro;
        vec2 uv = (rp.x * pd.zy + rp.y * pd.xz + rp.z * pd.xy) * 0.5;
        float s = etienne_fui(uv, w);
        if (s > 0.0) {
            l = min(l, d);
        }
    }
    return l;
}

vec4 renderEtiennePulse(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    vec2 fragCoord = (st / aspect + 0.5) * uResolution.xy;
    vec2 s = fragCoord / uResolution.xy;
    vec2 uv = (s - 0.5) * (uResolution.xx / uResolution.yx);

    float bpm = 176.0 / 2.0;
    etienne_period = 1.0 / (bpm / 60.0);
    float TF = 2.0;
    float phase = 0.05;
    etienne_t = TF * time;
    etienne_pulseTime = TF * etienne_timeMoves(time - phase, etienne_period, 0.2, 1.6);
    etienne_pulseTime = mix(etienne_t, etienne_pulseTime, 0.7);

    etienne_pulseTime2 = TF * etienne_timeMoves(time - phase, 2.0 * etienne_period, 0.2, 1.6);
    etienne_pulseTime2 = mix(etienne_t, etienne_pulseTime2, 0.7);

    etienne_alt = etienne_lt = time * bpm / 60.0;
    etienne_tr = 1.0 - exp(-3.0 * fract(etienne_lt));
    etienne_bt = floor(etienne_lt);
    etienne_lt = etienne_tr + etienne_bt;

    bool tomaru = int(etienne_bt / 4.0) % 2 == 0;
    if (tomaru) {
        etienne_alt = etienne_lt = time * bpm / 60.0 / 4.0;
        etienne_tr = fract(etienne_lt);
        etienne_bt = floor(etienne_lt);
    }

    vec3 ro = vec3(0.0, 0.0, -3.0);
    vec3 dir = -ro;
    vec3 rd;

    float sc1y = etienne_sc(1).y;
    if (sc1y < 0.3) {
        ro = vec3(mix(-1.0, 1.0, fract(etienne_alt)), 0.0, -3.0);
        dir = vec3(0.0, 0.0, 1.0);
    } else if (sc1y < 0.6) {
        ro = vec3(0.0, 0.0, mix(-5.0, -3.0, etienne_tr));
        dir = vec3(0.0, 0.0, 1.0);
    } else {
        float a = etienne_alt * 0.5;
        ro = vec3(cos(a), 0.0, sin(a)) * 2.0;
        dir = -ro;
    }

    float z = 0.5;
    if (etienne_sc(1).x < 0.3) {
        z = mix(0.3, 1.7, etienne_tr);
    }
    rd = norm(bnt(dir) * vec3(uv, z));
    float l = etienne_march(ro, rd);
    float c = exp(-0.2 * l);

    if (etienne_sc(0).y < 0.2) {
        c += hjct(uv * 0.5 + 0.5 + vec2(etienne_alt * 0.5, 0.0)) * step(fract(etienne_alt * 4.0), 0.5);
        c *= step(abs(uv.y), 0.5);
    }
    if (etienne_sc(1).z < 0.3) {
        vec2 ruv = uv * 4.0;
        vec3 h = hash(vec3(floor(ruv), floor(etienne_alt * 4.0)));
        vec2 fuv = fract(ruv);
        vec2 au = abs((fuv * 2.0 - 1.0) * etienne_rot2D(floor(h.z * 4.0) * (pi / 4.0)));
        c += step(max(au.x, au.y), 0.3) * step(min(au.x, au.y), 0.05) * step(h.x, 0.1);
    }
    if (tomaru) {
        float len = length(uv) - mix(0.2, 0.8, etienne_tr);
        float nya = step(abs(len), 0.005);
        if (len < 0.0) {
            c = 1.0 - c * 1.5;
        }
        c += nya;
    }

    float dt = 0.035;
    vec3 rgb = vec3(
        etienne_pcol(uv, etienne_pulseTime, etienne_t),
        etienne_pcol(uv, etienne_pulseTime - dt, etienne_t - dt),
        etienne_pcol(uv, etienne_pulseTime - 2.0 * dt, etienne_t - 2.0 * dt)
    );

    float coff = length(uv);
    rgb.xy *= etienne_rot2D(0.57 * etienne_t - coff);
    rgb.yz *= etienne_rot2D(0.87 * etienne_t - coff * 1.2);
    rgb = abs(rgb);

    float scl = 13.0 + 4.0 * sin(0.37 * etienne_t);
    vec2 ruv = uv * etienne_rot2D(0.05 * etienne_t);
    rgb = mix(rgb, vec3(1.0) - rgb, etienne_discre(ruv.x * scl));

    rgb = etienne_aces(etienne_aces(rgb));

    vec3 paletteBase = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    float energyMix = clamp(0.4 + energy * 0.6 + uIntensity * 0.4, 0.0, 2.0);
    rgb = mix(paletteBase, rgb, clamp(energyMix, 0.0, 1.2));

    float alpha = clamp(0.3 + energyMix * 0.35 + c * 0.25, 0.0, 1.0);
    return vec4(clamp(rgb, 0.0, 1.0), alpha);
}

#undef etienne_sc

#line 1 7
// @EFFECT name="Fractal Runway" index=24 desc="Temporal fractal runway raymarch" author="etiennejcb"
// Temporal fractal runway shader adapted from Shadertoy snippet by @etiennejcb

const float runway_defaultStep = 0.025;
const float runway_defaultMaxDist = 15.0;
const float runway_referencePixels = 1280.0 * 720.0;

float runway_time = 0.0;
vec3 runway_lightDir = vec3(0.0);
vec3 runway_colorAccum = vec3(0.0);
vec2 runway_fragCoord = vec2(0.0);
bool runway_accumulateColor = true;
float runway_resolutionPressure = 1.0;
float runway_fractalIterationLimit = 6.0;
float runway_marchIterationLimit = 120.0;

mat2 runway_rot(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, s, -s, c);
}

vec3 runway_fractal(vec2 p) {
    vec2 pos = p;
    float d = 0.0;
    float ml = 100.0;
    vec2 mc = vec2(100.0);
    p = abs(fract(p * 0.1) - 0.5);
    vec2 c = p;
    for (int i = 0; i < 7; ++i) {
        if (float(i) >= runway_fractalIterationLimit) {
            break;
        }
        d = dot(p, p);
        p = abs(p + 1.0) - abs(p - 1.0) - p;
        p = p * -1.5 / clamp(d, 0.5, 1.0) - c;
        mc = min(mc, abs(p));
        if (i > 2) {
            ml = min(ml * (1.0 + float(i) * 0.1), abs(p.y - 0.5));
        }
    }
    mc = max(vec2(0.0), 1.0 - mc);
    float mcLen = length(mc);
    if (mcLen > 0.0) {
        mc = (mc / mcLen) * 0.8;
    }
    ml = pow(max(0.0, 1.0 - ml), 6.0);
    float wave = step(0.7, fract(d * 0.1 + runway_time * 0.5 + pos.x * 0.2));
    return (vec3(mc, d * 0.4) * ml * wave) - ml * 0.1;
}

float runway_map(vec2 p) {
    if (runway_accumulateColor) {
        runway_colorAccum += runway_fractal(p);
    }

    float t = runway_time;
    vec2 p2 = abs(0.5 - fract(p * 8.0 + 4.0));

    float h = sin(length(p) + t);
    vec2 cell = floor(p * 2.0 + 1.0);
    float l = length(p2 * p2);
    h += (cos(cell.x + t) + sin(cell.y + t)) * 0.5;
    h += max(0.0, 5.0 - length(cell - vec2(18.0, 0.0))) * 1.5;
    h += max(0.0, 5.0 - length(cell + vec2(18.0, 0.0))) * 1.5;
    cell = cell * 2.0 + 0.2345;
    t *= 0.5;
    h += (cos(cell.x + t) + sin(cell.y + t)) * 0.3;
    return h;
}

vec3 runway_normal(vec2 p) {
    vec2 eps = vec2(0.0, 0.001);
    bool previous = runway_accumulateColor;
    runway_accumulateColor = false;
    float nx = runway_map(p + eps.yx) - runway_map(p - eps.yx);
    float ny = runway_map(p + eps.xy) - runway_map(p - eps.xy);
    runway_accumulateColor = previous;
    return normalize(vec3(nx, 2.0 * eps.y, ny));
}

vec2 runway_hit(vec3 p) {
    float h = runway_map(p.xz);
    return vec2(step(p.y, h), h);
}

vec3 runway_bsearch(vec3 from, vec3 dir, float td, inout float step, out vec2 hitInfo) {
    vec3 pos = from;
    float previous = 1.0;
    step *= -0.5;
    td += step;
    for (int i = 0; i < 12; ++i) {
        pos = from + td * dir;
        hitInfo = runway_hit(pos);
        if (abs(hitInfo.x - previous) > 0.001) {
            step *= -0.5;
            previous = hitInfo.x;
        }
        td += step;
    }
    return pos;
}

vec3 runway_shade(vec3 p, vec3 dir, float surfaceHeight, float travel) {
    vec3 normal = runway_normal(p.xz);
    float diffuse = max(0.0, dot(runway_lightDir, -normal));
    vec3 reflection = reflect(runway_lightDir, dir);
    float specular = pow(max(0.0, dot(reflection, -normal)), 8.0);
    vec3 base = runway_colorAccum * 0.25;
    vec3 lighting = (diffuse * 0.5 + 0.2 + specular * vec3(1.0, 0.8, 0.5)) * 0.2;
    return base + lighting;
}

vec3 runway_march(vec3 from, vec3 dir, float baseStep, float maxDist) {
    vec3 pos = from;
    vec3 color = vec3(0.0);
    float travel = 0.5;
    float step = baseStep;
    vec2 hitInfo = vec2(0.0);
    int dynamicMaxSteps = int(runway_marchIterationLimit);
    dynamicMaxSteps = max(dynamicMaxSteps, 40);

    for (int i = 0; i < 200; ++i) {
        if (i >= dynamicMaxSteps) {
            break;
        }
        pos = from + dir * travel;
        hitInfo = runway_hit(pos);
        if (hitInfo.x > 0.5 || travel > maxDist) {
            break;
        }
        travel += step;
    }

    if (hitInfo.x > 0.5) {
        vec2 refined;
        vec3 surfPos = runway_bsearch(from, dir, travel, step, refined);
        color = runway_shade(surfPos, dir, refined.y, travel);
        travel = length(surfPos - from);
    }

    float fade = pow(clamp(travel / maxDist, 0.0, 1.0), 3.0);
    vec3 scanline = 2.0 * vec3(mod(runway_fragCoord.y, 4.0) * 0.1);
    color = mix(color, scanline, fade);
    return color * vec3(0.9, 0.8, 1.0);
}

mat3 runway_lookat(vec3 dir, vec3 up) {
    vec3 forward = normalize(dir);
    vec3 right = normalize(cross(forward, normalize(up)));
    return mat3(right, cross(right, forward), forward);
}

vec3 runway_path(float t) {
    return vec3(cos(t) * 5.5, 1.5, sin(t * 2.0)) * 2.5;
}

vec4 renderFractalRunway(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    vec2 fragCoord = (st / aspect + 0.5) * uResolution.xy;
    runway_fragCoord = fragCoord;

    vec2 uv = (fragCoord - 0.5 * uResolution.xy) / max(uResolution.y, 1.0);
    
    // Dynamic resolution scaling for performance
    float pixelCount = uResolution.x * uResolution.y;
    float resolutionScale = 1.0;
    if (pixelCount > 1920.0 * 1080.0) {
        resolutionScale = 0.5;
    } else if (pixelCount > 1280.0 * 720.0) {
        resolutionScale = 0.7;
    }
    uv *= resolutionScale;

    runway_resolutionPressure = clamp(pixelCount / runway_referencePixels, 1.0, 4.0);
    float highResFactor = clamp((runway_resolutionPressure - 1.0) / 3.0, 0.0, 1.0);
    runway_fractalIterationLimit = mix(6.0, 4.0, highResFactor);
    runway_marchIterationLimit = mix(120.0, 80.0, highResFactor);

    runway_time = time;
    runway_colorAccum = vec3(0.0);
    runway_lightDir = normalize(vec3(0.0, -1.0, -1.0 + high * 0.4));

    bool allowAccumulation = highResFactor < 0.98;
    runway_accumulateColor = allowAccumulation;
    vec3 fallbackFractal = vec3(0.0);
    if (!allowAccumulation) {
        // Sample a lightweight fractal preview so we still get color at ultra high resolution
        vec2 fallbackPos = uv * 1.5 + vec2(time * 0.3, time * 0.17);
        fallbackFractal = abs(runway_fractal(fallbackPos)) * 0.25;
    }

    float tempoInfluence = clamp(tempo * 0.2 + bass * 0.3, 0.0, 1.0);
    float resolutionStepBoost = mix(1.0, 2.2, highResFactor);
    float baseStep = runway_defaultStep * mix(1.2, 0.6, clamp(energy + tempoInfluence, 0.0, 1.0)) * resolutionStepBoost;
    float maxDist = runway_defaultMaxDist * mix(1.0, 1.5, clamp(energy * 0.6, 0.0, 1.0));
    maxDist *= mix(1.0, 0.85, highResFactor);

    float travelTime = time * 0.2;
    vec3 from = runway_path(travelTime);
    vec3 dir = normalize(vec3(uv, 0.7));
    vec3 advance = runway_path(travelTime + 0.1) - from;
    vec3 up = vec3(advance.x * 0.1, 1.0, 0.0);
    dir = runway_lookat(advance + vec3(0.0, -0.2 - (1.0 + sin(travelTime * 2.0)), 0.0), up) * dir;

    vec3 coreColor = runway_march(from, dir, baseStep, maxDist) * 1.5;

    vec3 paletteBase = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    float energyMix = clamp(0.45 + energy * 0.4 + uIntensity * 0.3, 0.0, 1.0);
    vec3 motionGlow = (runway_colorAccum + fallbackFractal) * 0.28;
    vec3 vignette = paletteBase * pow(max(0.0, 1.0 - length(uv) * 0.85), 3.0) * 0.2;

    vec3 finalColor = mix(paletteBase, coreColor, energyMix);
    finalColor += motionGlow + vignette;
    finalColor = clamp(finalColor, 0.0, 1.0);

    float alpha = clamp(0.3 + energyMix * 0.55, 0.0, 1.0);
    return vec4(finalColor, alpha);
}

#line 1 8
// @EFFECT name="Volumetric Tunnel" index=25 desc="Volumetric streak tunnel with audio reactivity" author="System"
// Volumetric streak tunnel shader adapted from Shadertoy snippet

vec4 renderVolumetricTunnel(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    vec2 fragCoord = (st / aspect + 0.5) * uResolution.xy;

    float t = time * 0.1;
    vec2 uv = fragCoord / uResolution.xy - 0.5;
    vec2 originalUv = uv;
    uv.x *= uResolution.x / max(uResolution.y, 1.0);

    vec3 rd = normalize(vec3(uv, 2.0));
    float ct = cos(t);
    float stSin = sin(t);
    mat2 rotMat = mat2(ct, stSin, -stSin, ct);
    rd.xy = rotMat * rd.xy;

    vec3 ro = vec3(t + sin(t * 6.53583) * 0.05,
                   0.01 + sin(t * 352.4855) * 0.0015,
                   -t * 3.0);

    vec3 p = ro;
    float v = 0.0;
    float td = -mod(ro.z, 0.005);
    float stepSize = 0.005;
    float falloff = clamp(0.6 + energy * 0.4 + bass * 0.3, 0.1, 2.0);
    float pulse = clamp(0.3 + tempo * 0.4 + high * 0.5, 0.1, 2.5);

    for (int r = 0; r < 150; ++r) {
        float streak = length(abs(0.01 - mod(p, 0.02)));
        float streakIntensity = pow(max(0.0, 0.01 - streak) / 0.01, 10.0);
        float temporal = exp(-falloff * pow((1.0 + td), 2.0));
        v += streakIntensity * temporal;
        p = ro + rd * td;
        td += stepSize;
    }

    float vignette = max(0.0, 1.0 - length(originalUv * originalUv) * 2.5);
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    vec3 tunnel = vec3(v, v * v, v * v * v) * 8.0 * vignette;

    float colorMix = clamp(0.4 + energy * 0.5 + uIntensity * 0.3, 0.0, 1.0);
    vec3 finalColor = mix(palette, tunnel, colorMix);
    float alpha = clamp(0.25 + v * 0.3 + energy * 0.2, 0.0, 1.0);
    return vec4(finalColor, alpha);
}

#line 1 9
// @EFFECT name="Chromatic Swirl" index=26 desc="Sine-based chromatic swirl" author="System"
// Sine-based chromatic swirl shader

vec4 renderChromaticSwirl(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st + 0.5;
    vec2 centered = uv - 0.5;

    vec3 phaseShift = vec3(0.5, 0.2, 0.3) + vec3(0.35 * bass, 0.25 * mid, 0.4 * high);
    vec3 a = sin(time * phaseShift);
    vec3 b = vec3(-500.0, 200.0, 500.0);
    vec3 c = vec3(0.2, 1.4, 0.4) + vec3(tempo * 0.4);

    float angle = atan(centered.y, centered.x);
    vec3 angular = vec3(angle + energy * 0.6 + uIntensity * 0.3);

    vec3 base = 0.2 + sin(0.5 + c * time) + sin(angular * b * a);
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    float vignette = smoothstep(0.9, 0.2, length(centered) * 1.6);

    vec3 color = mix(palette, base, clamp(0.4 + energy * 0.5 + tempo * 0.3, 0.0, 1.0));
    color *= vignette;
    color = clamp(color, 0.0, 1.0);

    float alpha = clamp(0.3 + vignette * 0.6 + energy * 0.2, 0.0, 1.0);
    return vec4(color, alpha);
}

#line 1 10
// @EFFECT name="Hyper Pulse" index=27 desc="Hyper-dimensional raymarch with audio modulation" author="Blackle Mori"
// Hyper-dimensional raymarch shader adapted from Blackle Mori (CC0)

float hyper_comp(vec3 p) {
    p = acos(sin(p) * 0.9);
    return length(p) - 1.0;
}

vec3 hyper_erot(vec3 p, vec3 axis, float angle) {
    return mix(dot(p, axis) * axis, p, cos(angle)) + sin(angle) * cross(axis, p);
}

float hyper_smin(float a, float b, float k) {
    float h = max(0.0, k - abs(b - a)) / k;
    return min(a, b) - h * h * h * k / 6.0;
}

vec4 hyper_wrot(vec4 p) {
    return vec4(dot(p, vec4(1.0)), p.yzw + p.zwy - p.wyz - p.xxx) * 0.5;
}

float hyper_d1, hyper_d2, hyper_d3;
float hyper_lazors, hyper_doodad;
vec3 hyper_p2;
float hyper_timeScaled;

const float kHyperBpm = 125.0;

float hyper_scene(vec3 p, float time, float tempo, float energy, float bass, float mid, float high) {
    hyper_p2 = hyper_erot(p, vec3(0.0, 1.0, 0.0), hyper_timeScaled);
    hyper_p2 = hyper_erot(hyper_p2, vec3(0.0, 0.0, 1.0), hyper_timeScaled / 3.0);
    hyper_p2 = hyper_erot(hyper_p2, vec3(1.0, 0.0, 0.0), hyper_timeScaled / 5.0);

    float beatTime = time / 60.0 * kHyperBpm;
    vec4 p4 = vec4(hyper_p2, 0.0);
    p4 = mix(p4, hyper_wrot(p4), smoothstep(-0.5, 0.5, sin(beatTime / 4.0)));
    p4 = abs(p4);
    p4 = mix(p4, hyper_wrot(p4), smoothstep(-0.5, 0.5, sin(beatTime)));

    float beatFactor = smoothstep(-0.5, 0.5, cos(beatTime / 2.0));
    float beatFactor2 = smoothstep(0.9, 1.0, cos(beatTime / 16.0));
    float extrusion = mix(0.05, 0.07, beatFactor);
    vec4 coreShape = abs(p4) - extrusion;
    coreShape = max(coreShape, 0.0);
    hyper_doodad = length(coreShape + mix(-0.1, 0.2, beatFactor)) - mix(0.15, 0.55, beatFactor * beatFactor) + beatFactor2;

    p.x += asin(sin(hyper_timeScaled / 80.0) * 0.99) * 80.0;

    hyper_lazors = length(asin(sin(hyper_erot(p, vec3(1.0, 0.0, 0.0), hyper_timeScaled * 0.2).yz * 0.5 + 1.0)) / 0.5) - 0.1;
    hyper_d1 = hyper_comp(p);
    hyper_d2 = hyper_comp(hyper_erot(p + 5.0, normalize(vec3(1.0, 3.0, 4.0)), 0.4));
    hyper_d3 = hyper_comp(hyper_erot(p + 10.0, normalize(vec3(1.0, 2.0, 3.0)), 1.0));

    float structural = hyper_smin(hyper_smin(hyper_d1, hyper_d2, 0.05), hyper_d3, 0.05);
    structural = 0.3 - structural;
    float reactive = mix(0.25, 0.15, clamp(energy, 0.0, 1.0));
    float lazorMix = mix(hyper_lazors, reactive, clamp(high, 0.0, 1.0));
    return min(hyper_doodad, min(lazorMix, structural));
}

vec3 hyper_norm(vec3 p, float time, float tempo, float energy, float bass, float mid, float high) {
    float epsilon = length(p) < 1.0 ? 0.005 : 0.01;
    vec3 offset = vec3(epsilon, 0.0, 0.0);
    float sx = hyper_scene(p + offset.xyy, time, tempo, energy, bass, mid, high) -
               hyper_scene(p - offset.xyy, time, tempo, energy, bass, mid, high);
    float sy = hyper_scene(p + offset.yxy, time, tempo, energy, bass, mid, high) -
               hyper_scene(p - offset.yxy, time, tempo, energy, bass, mid, high);
    float sz = hyper_scene(p + offset.yyx, time, tempo, energy, bass, mid, high) -
               hyper_scene(p - offset.yyx, time, tempo, energy, bass, mid, high);
    return normalize(vec3(sx, sy, sz));
}

vec4 renderHyperPulse(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    vec2 fragCoord = (st / aspect + 0.5) * uResolution.xy;
    vec2 uv = (fragCoord - 0.5 * uResolution.xy) / max(uResolution.y, 1.0);

    float beatTime = time / 60.0 * kHyperBpm;
    float bpmBlend = mix(pow(sin(fract(beatTime) * 3.14159 / 2.0), 20.0) + floor(beatTime), beatTime, 0.4);
    hyper_timeScaled = bpmBlend;

    vec3 camDir = normalize(vec3(0.8 + sin(bpmBlend * 3.14159 / 4.0) * 0.3, uv.y, uv.x));
    vec3 camPos = vec3(-1.5 + sin(bpmBlend * 3.14159) * 0.2, 0.0, 0.0) + camDir * 0.2;

    camPos = hyper_erot(camPos, vec3(0.0, 1.0, 0.0), sin(bpmBlend * 0.2) * 0.4);
    camPos = hyper_erot(camPos, vec3(0.0, 0.0, 1.0), cos(bpmBlend * 0.2) * 0.4);
    camDir = hyper_erot(camDir, vec3(0.0, 1.0, 0.0), cos(bpmBlend * 0.2) * 0.4);
    camDir = hyper_erot(camDir, vec3(0.0, 0.0, 1.0), sin(bpmBlend * 0.2) * 0.4);

    vec3 rayPos = camPos;
    vec3 rayDir = camDir;

    bool hit = false;
    float attenuation = 1.0;
    float travel = 0.0;
    float glow = 0.0;
    float doodadGlow = 0.0;
    float fogAccum = 0.0;

    for (int i = 0; i < 40 && !hit; ++i) {
        float dist = hyper_scene(rayPos, time, tempo, energy, bass, mid, high);
        hit = dist * dist < 1e-6;
        glow += 0.2 / (1.0 + hyper_lazors * hyper_lazors * 20.0) * attenuation;
        doodadGlow += 0.2 / (1.0 + hyper_doodad * hyper_doodad * 20.0) * attenuation;

        bool reflective = (sin(hyper_d3 * 45.0) < -0.4 && dist != hyper_doodad) ||
                          (dist == hyper_doodad && cos(pow(length(hyper_p2 * hyper_p2 * hyper_p2), 0.3) * 120.0) > 0.4);
        bool lazorHit = dist == hyper_lazors;

        if (hit && reflective && !lazorHit) {
            vec3 normal = hyper_norm(rayPos, time, tempo, energy, bass, mid, high);
            attenuation *= 1.0 - abs(dot(rayDir, normal)) * 0.98;
            rayDir = reflect(rayDir, normal);
            dist = 0.1;
            hit = false;
        }

        rayPos += rayDir * dist;
        travel += dist;
        fogAccum += dist * attenuation / 30.0;
        if (travel > 18.0) break;
    }

    fogAccum = smoothstep(0.0, 1.0, fogAccum);
    vec3 fogColor = mix(vec3(0.45, 0.7, 1.1), vec3(0.3, 0.5, 0.85), length(uv));

    vec3 finalColor = vec3(0.0);
    if (hit) {
        vec3 normal = hyper_norm(rayPos, time, tempo, energy, bass, mid, high);
        vec3 reflected = reflect(rayDir, normal);
        float spec = length(sin(reflected * 3.0) * 0.5 + 0.5) / sqrt(3.0) * 0.7 + 0.3;
        vec3 material = mix(vec3(0.9, 0.4, 0.3), vec3(0.3, 0.4, 0.8), smoothstep(-1.0, 1.0, float(sin(hyper_d1 * 5.0 + time * 2.0))));
        material = mix(material, vec3(0.5, 0.4, 1.0), smoothstep(0.0, 1.0, float(sin(hyper_d2 * 5.0 + time * 2.0))));
        if (hyper_doodad == hyper_scene(rayPos, time, tempo, energy, bass, mid, high)) {
            material = mix(vec3(1.0), material, 0.1) * 0.2 + 0.1;
        }
        finalColor = material * spec + pow(spec, 10.0);
    }

    finalColor = finalColor * attenuation - glow * glow - fogColor * glow;
    finalColor = mix(finalColor, fogColor, fogAccum);

    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    finalColor = mix(palette, finalColor, clamp(0.5 + energy * 0.4 + tempo * 0.2, 0.0, 1.0));

    if (hyper_doodad == hyper_scene(rayPos, time, tempo, energy, bass, mid, high)) {
        finalColor += doodadGlow * doodadGlow * 0.12 * vec3(0.4, 0.6, 0.9);
    }

    finalColor = sqrt(max(finalColor, 0.0));
    finalColor = smoothstep(vec3(0.0), vec3(1.2), finalColor);
    float alpha = clamp(0.35 + fogAccum * 0.4, 0.0, 1.0);
    return vec4(finalColor, alpha);
}

#line 1 11
// @EFFECT name="Gyroid Reflections" index=28 desc="Gyroid reflection shader with audio/tempo modulation" author="System"
// Gyroid reflection shader adapted from Shadertoy (CC0) with audio/tempo modulation

const float kGyroidPi = acos(-1.0);
const float kGyroidBpm = 125.0;

float gyroidBeatTime;
float gyroidTimeValue;
vec3 gyroidSpherePos;
vec3 gyroidPalette;
float gyroidEnergyValue;
float gyroidTempoValue;

float gyroidSaturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 gyroidSaturate(vec3 x) {
    return clamp(x, vec3(0.0), vec3(1.0));
}

mat2 gyroidRot(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, s, -s, c);
}

void gyroidRotPiOver4(inout vec2 v) {
    v = vec2(v.x + v.y, -v.x + v.y) / sqrt(2.0);
}

float gyroidSqWave(float x) {
    float i = floor(x);
    float s = 0.1;
    float oddCurrent = step(1.0, mod(i, 2.0));
    float oddNext = step(1.0, mod(i + 1.0, 2.0));
    return mix(oddCurrent, oddNext, smoothstep(0.5 - s, 0.5 + s, fract(x)));
}

float gyroidSmin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0);
    return min(a, b) - h * h * 0.25 / k;
}

vec3 gyroidPosition;
float gyroidD1, gyroidD2, gyroidD3;

float gyroidMap(vec3 p) {
    float d;
    vec3 q = p;
    q.y = 0.7 - abs(q.y);

    d = q.y;
    q.zx = fract(q.zx) - 0.5;

    vec2 dq = mix(vec2(-0.2), vec2(0.27, 0.1), gyroidSqWave(gyroidBeatTime * 0.15));
    float scale = 1.0;

    for (int i = 0; i < 4; ++i) {
        vec3 v = q;
        v.zx = abs(v.zx);
        if (v.z > v.x) {
            v.zx = v.xz;
        }
        float branch = max(v.x - 0.1, (v.x * 2.0 + v.y) / sqrt(5.0) - 0.3);
        d = min(d, branch / scale);

        q.zx = abs(q.zx);
        gyroidRotPiOver4(q.xz);
        q.xy -= dq;
        gyroidRotPiOver4(q.yx);

        q *= 2.0;
        scale *= 2.0;
    }

    vec3 g = p - gyroidSpherePos;
    float t = length(g) - 0.3;
    g *= 15.0;
    g.xy = gyroidRot(gyroidTimeValue * 1.3) * g.xy;
    g.yz = gyroidRot(gyroidTimeValue * 1.7) * g.yz;
    float gyroidPattern = abs(dot(sin(g), cos(g.yzx)));
    t = max(t, (gyroidPattern - 0.2) / 15.0);

    gyroidD1 = t;
    gyroidD2 = d;

    return gyroidSmin(d, t, 0.3);
}

vec3 gyroidCalcNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    float dx = gyroidMap(p + e.xyy) - gyroidMap(p - e.xyy);
    float dy = gyroidMap(p + e.yxy) - gyroidMap(p - e.yxy);
    float dz = gyroidMap(p + e.yyx) - gyroidMap(p - e.yyx);
    return normalize(vec3(dx, dy, dz));
}

vec3 gyroidHSV(float h, float s, float v) {
    vec3 res = fract(h + vec3(0.0, 2.0, 1.0) / 3.0) * 6.0 - 3.0;
    res = gyroidSaturate(abs(res) - 1.0);
    res = (res - 1.0) * s + 1.0;
    res *= v;
    return res;
}

vec3 gyroidMarch(inout vec3 rayPos, inout vec3 rayDir, inout vec3 attenuation, out bool hitSomething) {
    vec3 accumulated = vec3(0.0);
    hitSomething = false;
    float t = 0.0;

    for (int i = 0; i < 70; ++i) {
        float dist = gyroidMap(rayPos + rayDir * t);
        if (abs(dist) < 0.0001) {
            hitSomething = true;
            break;
        }
        t += dist;
        if (t > 40.0) {
            break;
        }
    }

    rayPos += rayDir * t;

    vec3 lightDir = normalize(-rayPos);
    vec3 normal = gyroidCalcNormal(rayPos);
    vec3 reflection = reflect(rayDir, normal);

    float diffuse = max(dot(lightDir, normal), 0.0);
    float specular = pow(max(dot(reflect(lightDir, normal), rayDir), 0.0), 20.0);
    float fog = exp(-t * t * 0.2);

    float materialParam = smoothstep(0.01, 0.1, length(rayPos - gyroidSpherePos) - 0.3);
    float phase = length(rayPos) * 4.0 - gyroidTimeValue * 2.0;
    vec3 albedo = gyroidHSV(floor(phase / kGyroidPi) * kGyroidPi * 0.4, 0.8, 1.0);
    albedo = mix(vec3(0.9), albedo, materialParam);

    float f0 = mix(0.01, 0.8, materialParam);
    float metalness = mix(0.01, 0.9, materialParam);
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - dot(reflection, normal), 5.0);
    float lightPower = 3.0 / max(abs(sin(phase)) + 1e-3, 1e-3);

    vec3 color = vec3(0.0);
    color += albedo * diffuse * (1.0 - metalness) * lightPower;
    color += albedo * specular * metalness * lightPower;
    color = mix(vec3(0.0), color, fog);
    color *= attenuation;

    attenuation *= albedo * fresnel * fog;
    rayPos += normal * 0.01;
    rayDir = reflection;

    return color;
}

vec4 renderGyroidReflections(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    vec2 fragCoord = (st / aspect + 0.5) * uResolution.xy;
    
    // Dynamic resolution scaling for performance
    float pixelCount = uResolution.x * uResolution.y;
    float resolutionScale = 1.0;
    if (pixelCount > 1920.0 * 1080.0) {
        resolutionScale = 0.5; // 2x resolution reduction for 4K+
    } else if (pixelCount > 1280.0 * 720.0) {
        resolutionScale = 0.7; // Moderate reduction for 1080p+
    }
    
    vec2 uv = vec2(fragCoord.x / uResolution.x, fragCoord.y / uResolution.y);
    uv -= 0.5;
    uv /= vec2(uResolution.y / max(uResolution.x, 1.0), 1.0) * 0.5;
    uv *= resolutionScale; // Scale down sampling resolution

    gyroidTimeValue = time;
    float bpmMod = kGyroidBpm * clamp(tempo * 0.9 + 0.3, 0.6, 1.4);
    gyroidBeatTime = time / 60.0 * bpmMod;
    gyroidSpherePos = sin(vec3(13.0, 0.0, 7.0) * time * 0.1 + vec3(bass * 0.5, mid * 0.2, high * 0.4));
    gyroidPalette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    gyroidEnergyValue = energy;
    gyroidTempoValue = tempo;

    vec3 color = vec3(0.0);
    vec3 rayOrigin = vec3(0.0, -0.3, 1.9);
    rayOrigin.zx *= gyroidRot(time * 0.1);

    vec3 rayDir = normalize(vec3(uv, -2.0));
    rayDir.zx *= gyroidRot(time * 0.1);

    vec3 attenuation = vec3(1.0);
    bool hit = false;

    color += gyroidMarch(rayOrigin, rayDir, attenuation, hit);
    if (hit) {
        color += gyroidMarch(rayOrigin, rayDir, attenuation, hit);
    }

    color = pow(color, vec3(1.0 / 2.2));
    color = mix(gyroidPalette, color, clamp(0.4 + energy * 0.5 + tempo * 0.3, 0.0, 1.0));
    color = clamp(color, 0.0, 1.0);

    float alpha = clamp(0.4 + energy * 0.3 + length(color) * 0.1, 0.0, 1.0);
    return vec4(color, alpha);
}

#line 1 12
// @EFFECT name="Metal Gyroid Hall" index=30 desc="Metal gyroid hall with audio-reactive tweaks" author="System"
// @EFFECT name="Hex Kaleidoscope" index=31 desc="Hexagonal kaleidoscope pattern with audio reactivity" author="System"
// @EFFECT name="HSV Color Shift" index=32 desc="HSV color shifting with audio-reactive movement" author="System"
// Metal gyroid hall shader adapted from a Shadertoy snippet with audio-reactive tweaks

const vec3 kMetalLightDirection = normalize(vec3(-1.0, 2.0, 4.0));
const vec3 kMetalAmbientColor = vec3(0.2);
const float kMetalFogDensityBase = 0.05;
const float kMetallicBase = 0.8;
const float kMetalF0 = 0.8;
const float kMetalFov = radians(80.0);
const float kMetalPi2 = 2.0 * PI;

mat3 metalRotate3D(float angle, vec3 axis) {
    vec3 a = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float r = 1.0 - c;
    return mat3(
        a.x * a.x * r + c,
        a.y * a.x * r + a.z * s,
        a.z * a.x * r - a.y * s,
        a.x * a.y * r - a.z * s,
        a.y * a.y * r + c,
        a.z * a.y * r + a.x * s,
        a.x * a.z * r + a.y * s,
        a.y * a.z * r - a.x * s,
        a.z * a.z * r + c
    );
}

float metalSdGyroid(vec3 p) {
    return dot(sin(p), cos(p.yzx)) + 1.3;
}

float metalMap(vec3 p) {
    float d = metalSdGyroid(p);
    d = min(d, metalSdGyroid(p + vec3(PI, 0.0, 0.0)));
    d = min(d, metalSdGyroid(p + vec3(PI, PI, 0.0)));
    return d;
}

vec3 metalCalcNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        metalMap(p + e.xyy) - metalMap(p - e.xyy),
        metalMap(p + e.yxy) - metalMap(p - e.yxy),
        metalMap(p + e.yyx) - metalMap(p - e.yyx)
    ));
}

float metalCalcAO(vec3 position, vec3 normal) {
    float total = 0.0;
    float scale = 1.0;
    for (int i = 0; i < 10; ++i) {
        float hr = 0.01 + 0.02 * float(i * i);
        vec3 samplePos = position + normal * hr;
        float dd = metalMap(samplePos);
        float ao = clamp(hr - dd, 0.0, 1.0);
        total += ao * scale;
        scale *= 0.75;
    }
    return 1.0 - clamp(0.5 * total, 0.0, 1.0);
}

float metalCalcShadow(vec3 position, vec3 direction) {
    float h = 0.0;
    float t = 0.001;
    float res = 1.0;
    const float shadowCoef = 0.5;
    for (int i = 0; i < 12; ++i) {
        h = metalMap(position + direction * t);
        if (h < 0.0005) {
            return shadowCoef;
        }
        res = min(res, h * 16.0 / t);
        t += h;
        if (t > 4.0) break;
    }
    return 1.0 - shadowCoef + res * shadowCoef;
}

vec3 metalObjectColor(vec3 p) {
    float thresh = 0.5;
    if (metalSdGyroid(p) < thresh) {
        return vec3(1.0, 0.1, 0.1);
    } else if (metalSdGyroid(p + vec3(PI, 0.0, 0.0)) < thresh) {
        return vec3(0.1, 1.0, 0.1);
    } else if (metalSdGyroid(p + vec3(PI, PI, 0.0)) < thresh) {
        return vec3(0.1, 0.1, 1.0);
    }
    return vec3(0.4, 0.5, 0.7);
}

float metalFresnel(float baseF0, float cosTheta) {
    return baseF0 + (1.0 - baseF0) * pow(1.0 - cosTheta, 5.0);
}

float metalFogFactor(float distance, float density) {
    float s = distance * density;
    return exp(-s * s);
}

vec3 metalToneMapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 metalRaymarch(vec3 camPos, vec3 rayDir, int maxSteps, float fogDensity, vec3 lightClr, inout vec3 attenuation, out bool hit) {
    vec3 accum = vec3(0.0);
    float dist = 0.0;
    hit = false;
    vec3 origin = camPos;

    for (int i = 0; i < maxSteps; ++i) {
        float sceneDist = metalMap(camPos);
        if (abs(sceneDist) < 1e-4) {
            hit = true;
            break;
        }
        camPos += rayDir * sceneDist;
        dist += sceneDist;
        if (dist > 30.0) {
            break;
        }
    }

    vec3 albedo = metalObjectColor(camPos);
    vec3 normal = metalCalcNormal(camPos);
    vec3 reflectionDir = reflect(rayDir, normal);

    float diffuse = max(dot(normal, kMetalLightDirection), 0.0);
    float specular = pow(max(dot(reflect(kMetalLightDirection, normal), rayDir), 0.0), 10.0);
    float ao = metalCalcAO(camPos, normal);
    float shadow = metalCalcShadow(camPos + normal * 0.005, kMetalLightDirection);

    accum += albedo * diffuse * shadow * (1.0 - kMetallicBase) * lightClr;
    accum += albedo * specular * shadow * kMetallicBase * lightClr;
    accum += albedo * ao * kMetalAmbientColor;

    float fog = metalFogFactor(distance(origin, camPos), fogDensity);
    accum = mix(vec3(1.0), accum, fog);

    float fresnel = metalFresnel(kMetalF0, dot(reflectionDir, normal));
    attenuation *= albedo * fresnel * fog;
    camPos += normal * 0.01;
    rayDir = reflectionDir;

    return accum;
}

vec4 renderMetalGyroidHall(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    vec2 fragCoord = (st / aspect + 0.5) * uResolution.xy;
    vec2 uv = (fragCoord * 2.0 - uResolution.xy) / max(uResolution.x, uResolution.y);

    float tempoBoost = clamp(tempo * 0.6 + bass * 0.4, 0.0, 1.5);
    float dynLightPower = mix(12.0, 24.0, clamp(energy + tempoBoost * 0.5, 0.0, 1.0));
    vec3 lightColor = vec3(1.0) * dynLightPower;
    float fogDensity = kMetalFogDensityBase * mix(0.6, 1.4, clamp(high + energy * 0.5, 0.0, 1.2));

    vec3 cameraPos = vec3(0.0, 0.0, -fract(time / kMetalPi2) * kMetalPi2);
    vec3 cameraDir = vec3(0.0, 0.0, -1.0);
    vec3 cameraSide = normalize(cross(cameraDir, vec3(0.0, 1.0, 0.0)));
    vec3 cameraUp = normalize(cross(cameraSide, cameraDir));

    vec3 rayDir = normalize(uv.x * cameraSide + uv.y * cameraUp + cameraDir / tan(kMetalFov * 0.5));
    float rotationSpeed = time * 0.07 * PI * mix(0.8, 1.4, clamp(tempo + high * 0.5, 0.0, 1.5));
    rayDir = metalRotate3D(rotationSpeed, normalize(vec3(5.0, 3.0, 1.0))) * rayDir;

    vec3 rayPos = cameraPos;
    bool hit = false;
    vec3 attenuation = vec3(1.0);
    vec3 color = vec3(0.0);

    color += metalRaymarch(rayPos, rayDir, 100, fogDensity, lightColor, attenuation, hit);
    for (int i = 0; i < 2; ++i) {
        if (!hit) break;
        color += attenuation * metalRaymarch(rayPos, rayDir, 60, fogDensity, lightColor, attenuation, hit);
    }

    color = metalToneMapACES(color * 0.8);
    color = pow(color, vec3(1.0 / 2.2));

    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    color = mix(palette, color, clamp(0.5 + energy * 0.4 + tempo * 0.2, 0.0, 1.0));
    color *= mix(0.8, 1.2, clamp(high + mid * 0.5, 0.0, 1.0));
    color = clamp(color, 0.0, 1.0);

    float alpha = clamp(0.35 + length(color) * 0.2, 0.0, 1.0);
    return vec4(color, alpha);
}

vec4 renderHexKaleidoscope(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st * 2.0 - 1.0;
    uv.x *= uResolution.x / max(uResolution.y, 1.0);
    
    float angle = atan(uv.y, uv.x);
    float radius = length(uv);
    
    // Hexagonal symmetry (6 segments)
    float segment = PI / 3.0;
    angle = mod(angle, segment);
    if (angle > segment * 0.5) {
        angle = segment - angle;
    }
    
    uv = vec2(cos(angle), sin(angle)) * radius;
    
    // Add rotation based on audio
    float rotation = time * 0.5 + tempo * 0.2;
    float s = sin(rotation);
    float c = cos(rotation);
    uv = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
    
    // Create hexagonal pattern
    vec2 hex = vec2(0.5, sqrt(3.0) * 0.5);
    vec2 hexCoord = uv / hex;
    vec2 hexIndex = floor(hexCoord);
    hexCoord = fract(hexCoord) - 0.5;
    
    float hexDist = length(hexCoord);
    
    // Audio-reactive parameters
    float pulse = 1.0 + bass * 0.3 + energy * 0.2;
    float colorShift = mid * 0.5 + high * 0.3;
    
    // Create pattern
    float pattern = sin(hexDist * 10.0 * pulse - time * 2.0) * 0.5 + 0.5;
    pattern *= 1.0 - smoothstep(0.4, 0.6, hexDist);
    
    // Color based on audio and position
    vec3 color = vec3(0.0);
    color.r = pattern * (0.5 + sin(time + colorShift) * 0.5);
    color.g = pattern * (0.5 + cos(time * 1.3 + colorShift * 1.5) * 0.5);
    color.b = pattern * (0.5 + sin(time * 0.7 + colorShift * 2.0) * 0.5);
    
    // Apply palette
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, uColorBlend);
    color = mix(palette, color, pattern);
    
    // Add glow effect for high energy
    float glow = exp(-hexDist * 2.0) * energy;
    color += vec3(glow * 0.2, glow * 0.3, glow * 0.4);
    
    return vec4(color, pattern);
}

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 renderHSVColorShift(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    vec2 tc = uv;
    float aspect = uResolution.x / uResolution.y;
    
    uv = uv * 2.0 - 1.0;
    uv.x *= aspect;
    
    // Audio-reactive movement
    float audioMovement = (bass * 0.5 + mid * 0.3) * 0.05;
    uv += vec2(sin(time + audioMovement), cos(time + audioMovement)) * 0.025;
    
    float s = 1.0 - smoothstep(0.0, 0.6, length(max(abs(uv.x), abs(uv.y))));
    s = sin(s);
    
    tc = 2.0 * tc - 1.0;
    tc *= 0.999;
    tc = tc * 0.5 + 0.5;
    
    // Create a base pattern using previous frame or generated texture
    vec3 h = vec3(0.5 + 0.5 * sin(time + tc.x * 10.0), 
                  0.5 + 0.5 * cos(time + tc.y * 10.0), 
                  0.5 + 0.5 * sin(time * 1.3 + length(tc) * 5.0));
    h = rgb2hsv(h);
    
    // Audio-reactive hue shift
    h.r += time * 0.1 + energy * 0.2;
    
    vec2 texel = 1.0 / uResolution.xy;
    vec2 d = texel * 0.75;
    vec3 p = vec3(0.5 + 0.5 * sin(h.r * 6.283 + time),
                  0.5 + 0.5 * cos(h.r * 6.283 + time * 1.3),
                  0.5 + 0.5 * sin(h.r * 6.283 + time * 0.7));
    
    vec3 c = vec3(s, 0.0, 0.0) + p;
    c = clamp(c, vec3(0.0), vec3(1.0));
    
    c = rgb2hsv(c);
    c.r += 0.002 + high * 0.01; // Audio-reactive hue shift
        
    c = hsv2rgb(c);
    c = c.r > 0.99 ? fract(c.rgb) : c;
    
    // Apply audio-reactive brightness
    float brightness = 1.0 + energy * 0.3 + bass * 0.2;
    c *= brightness;
    
    // Apply palette
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, uColorBlend);
    c = mix(palette, c, 0.7);
    
    return vec4(c, 1.0);
}

// @EFFECT name="Crypt Roots" index=33 desc="Ray marched improvised geometry with curvy shapes" author="System"
// Crypt Roots - Ray marching improvised geometry with curvy shapes and rock textures
// Adapted for audio reactivity
#define repeat(p,r) (mod(p,r)-r/2.)
mat2 rotCrypt (float a) { float c=cos(a), s=sin(a); return mat2(c,s,-s,c); }
float smoothmin (float a, float b, float r) { float h = clamp(.5+.5*(b-a)/r, 0., 1.); return mix(b, a, h)-r*h*(1.-h); }
float random (in vec2 st) { return fract(sin(dot(st.xy,vec2(12.9898,78.233)))*43758.5453123); }
float hash_n(float n) { return fract(sin(n) * 1e4); }
float noise(vec3 x) {
    const vec3 step = vec3(110, 241, 171);
    vec3 i = floor(x);
    vec3 f = fract(x);
    float n = dot(i, step);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix( hash_n(n + dot(step, vec3(0, 0, 0))), hash_n(n + dot(step, vec3(1, 0, 0))), u.x),
                   mix( hash_n(n + dot(step, vec3(0, 1, 0))), hash_n(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
               mix(mix( hash_n(n + dot(step, vec3(0, 0, 1))), hash_n(n + dot(step, vec3(1, 0, 1))), u.x),
                   mix( hash_n(n + dot(step, vec3(0, 1, 1))), hash_n(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}
float fbm (vec3 p) {
  float amplitude = 0.5;
  float result = 0.0;
  for (float index = 0.0; index <= 3.0; ++index) {
    result += noise(p / amplitude) * amplitude;
    amplitude /= 2.;
  }
  return result;
}
vec3 look (vec3 eye, vec3 target, vec2 anchor) {
    vec3 forward = normalize(target-eye);
    vec3 right = normalize(cross(forward, vec3(0,1,0)));
    vec3 up = normalize(cross(right, forward));
    return normalize(forward + right * anchor.x + up * anchor.y);
}
void moda(inout vec2 p, float repetitions) {
	float angle = 2.*PI/repetitions;
	float a = atan(p.y, p.x) + angle/2.;
	a = mod(a,angle) - angle/2.;
	p = vec2(cos(a), sin(a))*length(p);
}

float mapCryptRoots (vec3 pos, float time, float bass, float mid, float high) {
  // Audio-reactive parameters
  float audioScale = 1.0 + bass * 0.3 + mid * 0.2;
  float audioDistortion = high * 0.1;
  
  float chilly = noise(pos * 2. * audioScale);
  float salty = fbm(pos*20. * audioScale);
  
  pos.z -= salty*.04 + audioDistortion * 0.1;
  salty = smoothstep(.3, 1., salty);
  pos.z += salty*.04;
  pos.xy -= (chilly*2.-1.) * (.2 + bass * 0.1);
    
  vec3 p = pos;
  vec2 cell = vec2(1., .5);
  vec2 id = floor(p.xz/cell);
  p.xy *= rotCrypt(id.y * .5 + time * 0.1 + bass * 0.2);
  p.y += sin(p.x + .5 + time * 2.0 + mid * 0.5);
  p.xz = repeat(p.xz, cell);
    
  vec3 pp = p;
  moda(p.yz, 5.0 + high * 2.0);
  p.y -= .1;
  float scene = length(p.yz)-.02;
    
  vec3 ppp = pos;
  pp.xz *= rotCrypt(pp.y * 5. + time * 0.5);
  ppp = repeat(ppp, .1);
  moda(pp.xz, 3.0 + bass);
  pp.x -= .04 + .02*sin(pp.y*5. + time * 3.0 + mid);
  scene = smoothmin(length(pp.xz)-.01, scene, .2);

  p = pos;
  p.xy *= rotCrypt(-p.z + time * 0.2 + high * 0.3);
  moda(p.xy, 8.0 + mid);
  p.x -= .7 + bass * 0.1;
  p.xy *= rotCrypt(p.z*8. + time);
  p.xy = abs(p.xy)-.02;
  scene = smoothmin(scene, length(p.xy)-.005, .2);

  return scene;
}

vec3 getNormalCryptRoots (vec3 pos, float time, float bass, float mid, float high) {
  vec2 e = vec2(1.0,-1.0)*0.5773*0.0005;
  return normalize( e.xyy*mapCryptRoots( pos + e.xyy, time, bass, mid, high ) + 
                    e.yyx*mapCryptRoots( pos + e.yyx, time, bass, mid, high ) + 
                    e.yxy*mapCryptRoots( pos + e.yxy, time, bass, mid, high ) + 
                    e.xxx*mapCryptRoots( pos + e.xxx, time, bass, mid, high ) );
}

vec4 renderCryptRoots(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
  vec2 uv = (st - 0.5) / 0.5;  // Normalize to -1 to 1 range
  
  // Audio-reactive camera movement
  vec3 eye = vec3(.1 + bass * 0.1, .1 + mid * 0.05, -time * 0.1 - 4. + high * 0.2);
  vec3 at = vec3(0, 0, eye.z - 2.0 + sin(time * 0.5 + bass) * 0.5);
  vec3 ray = look(eye, at, uv);
  vec3 pos = eye;
  
  float dither = random(uv + fract(time));
  float total = dither * .2;
  float shade = 0.0;
  const float count = 60.0;
  
  for (float index = count; index > 0.0; --index) {
    pos = eye + ray * total;
    float dist = mapCryptRoots(pos, time, bass, mid, high);
    if (dist < 0.001 + total * .003) {
      shade = index / count;
      break;
    }
    dist *= 0.5 + 0.1 * dither;
    total += dist;
  }
  
  vec3 normal = getNormalCryptRoots(pos, time, bass, mid, high);
  vec3 color = vec3(0);
  
  // Audio-enhanced colors
  color += smoothstep(.3, .6, fbm(pos*100.)) * (.2 + energy * 0.3);
  color += vec3(0.839, 1, 1) * pow(clamp(dot(normal, normalize(vec3(0,2,1))), 0.0, 1.0), 4.) * (1.0 + high * 0.5);
  color += vec3(1, 0.725, 0.580) * pow(clamp(dot(normal, -normalize(pos-at)), 0.0, 1.0), 4.) * (1.0 + mid * 0.3);
  color += vec3(0.972, 1, 0.839) * pow(clamp(dot(normal, normalize(vec3(4,0,1))), 0.0, 1.0), 4.) * (1.0 + bass * 0.4);
  color += vec3(0.972, 1, 0.839) * pow(clamp(dot(normal, normalize(vec3(-5,0,1)))*.5+.5, 0.0, 1.0), 4.) * (1.0 + energy * 0.2);
  
  color = mix(vec3(0), color, clamp(dot(normal, -ray), 0.0, 1.0));
  color *= pow(shade, 1.0/1.2);
  
  // Apply palette
  vec3 palette = mix(uPrimaryColor, uSecondaryColor, uColorBlend);
  color = mix(palette, color, 0.6);
  
  // Audio-reactive brightness
  color *= 1.0 + energy * 0.3;
  
  return vec4(color, 1.0);
}

#line 1 13
// @EFFECT name="Breathing" index=34 desc="Rhythmic expansion effect with audio reactivity" author="System"
// Breathing Layer - Rhythmic expansion effect
// Based on glslify quintic-out easing function

float qinticOut(float t) {
  return 1.0 - (pow(abs(t - 1.0), 5.0));
}

vec4 renderBreathing(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st;
    uv *= 3.0;
	
    float l = length(uv);
    float t = qinticOut(fract(time * 0.5));
    
    // Enhanced audio reactivity
    float bassResponse = smoothstep(0.0, 1.0, bass) * 2.0;
    float midResponse = smoothstep(0.0, 1.0, mid) * 1.5;
    float highResponse = smoothstep(0.0, 1.0, high) * 1.0;
    float energyResponse = smoothstep(0.0, 1.0, energy) * 1.8;
    
    // Combined audio influence with different weights
    float audioReactivity = bassResponse * 0.4 + midResponse * 0.3 + highResponse * 0.2 + energyResponse * 0.5;
    audioReactivity = pow(audioReactivity, 1.5); // Enhance contrast
    
    // Base breathing pattern
    float breath = sin(t * 6.28 - l * 3.0);
    breath *= 1.0 - clamp(l, 0.0, 1.0);
    
    // Apply audio modulation to multiple aspects
    float audioModulation = 1.0 + audioReactivity * 3.0; // Stronger response
    breath *= audioModulation;
    
    // Add tempo-based pulsing
    float tempoPulse = sin(time * tempo * 0.1) * 0.2 * audioReactivity;
    breath += tempoPulse * (1.0 - l);
    
    // Enhanced color response
    vec3 baseColor = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    
    // Dynamic color shifting based on audio
    vec3 bassColor = mix(baseColor, vec3(1.0, 0.3, 0.2), bassResponse * 0.4); // Red shift for bass
    vec3 midColor = mix(baseColor, vec3(0.3, 1.0, 0.4), midResponse * 0.3);   // Green shift for mid
    vec3 highColor = mix(baseColor, vec3(0.2, 0.4, 1.0), highResponse * 0.3); // Blue shift for high
    
    vec3 finalColor = mix(mix(bassColor, midColor, 0.5), highColor, 0.3);
    finalColor *= breath;
    
    // Enhanced alpha with audio response
    float baseAlpha = clamp(0.3 + breath * 0.7, 0.0, 1.0);
    float audioAlpha = 1.0 + audioReactivity * 2.0;
    float alpha = baseAlpha * audioAlpha;
    alpha = clamp(alpha, 0.0, 1.0);
    
    return vec4(finalColor, alpha);
}

#line 1 14
// @EFFECT name="Evolution Noise" index=35 desc="Time-evolving noise pattern with audio reactivity" author="System"
// Evolution Noise - Time-evolving noise pattern
// Based on fractal noise with temporal evolution

float noise(vec2 pos, float evolve) {
    // Loop the evolution (over a very long period of time).
    float e = fract((evolve * 0.01));
    
    // Coordinates
    float cx = pos.x * e;
    float cy = pos.y * e;
    
    // Generate a "random" black or white value
    return fract(23.0 * fract(2.0 / fract(fract(cx * 2.4 / cy * 23.0) * fract(cx * evolve / pow(abs(cy), 0.050)))));
}

vec4 renderEvolutionNoise(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Audio-reactive parameters
    float audioMod = 1.0 + energy * 0.8 + bass * 0.4;
    float timeMod = time * audioMod;
    
    // Scale based on audio frequencies
    vec2 scale = vec2(1.0) + vec2(bass, mid) * 0.5;
    vec2 pos = st * scale * 2.0;
    
    // Generate multiple layers of noise for complexity
    vec3 colour = vec3(0.0);
    int layers = 3; // Reduced for performance
    
    for(int i = 0; i < layers; i++) {
        float layerTime = timeMod + float(i) * 0.1;
        float noiseValue = noise(pos * (1.0 + float(i) * 0.5), layerTime);
        
        // Different color channels respond to different frequencies
        if(i == 0) {
            colour.r = noiseValue; // Bass affects red
        } else if(i == 1) {
            colour.g = noiseValue; // Mid affects green  
        } else {
            colour.b = noiseValue; // High affects blue
        }
    }
    
    // Apply scene colors with reduced intensity for TV static effect
    vec3 baseColor = mix(uPrimaryColor, uSecondaryColor, uColorBlend);
    colour = mix(colour, baseColor * 0.3, 0.2); // Reduced base color influence
    
    // Reduce overall brightness to TV static levels (mostly gray)
    colour *= (0.3 + energy * 0.2); // Much lower brightness
    
    // Add subtle pulsing effect based on tempo
    float pulse = sin(time * tempo * 0.1) * 0.5 + 0.5;
    colour += pulse * energy * 0.1; // Reduced pulse intensity
    
    // Ensure colors stay in TV static range (darker grays)
    colour = clamp(colour, 0.1, 0.8); // Clamp to prevent white washout
    
    // Alpha based on energy and noise intensity
    float alpha = 0.6 + energy * 0.4;
    alpha = clamp(alpha, 0.0, 1.0);
    
    return vec4(colour, alpha);
}

#line 1 15
// @EFFECT name="Phi Fields" index=36 desc="Stabilized lissajous spiral field with tonemapping" author="System"
// Eye of Phi Redux: stabilized lissajous/spiral field with controlled tonemapping
const float kPhiScale = 7.5;
const float kPhiPI = 3.14159265359;
const float kPhiTAU = kPhiPI * 2.0;

vec2 phiRotate(float a) {
    return vec2(cos(a), sin(a));
}

float phiLine(vec2 p, float thickness) {
    return smoothstep(0.0, thickness, thickness - length(p));
}

vec3 phiGradient(vec3 base, float func, float phase, float width, float strength, bool reciprocal) {
    float n = max(abs(func), 1e-3);
    float g = min(n, 1.0 / n);
    float s = abs(sin(func * kPhiPI - phase));
    if (reciprocal) {
        float alt = abs(sin(kPhiPI / n + phase));
        s = min(s, alt);
    }
    float mask = 1.0 - pow(clamp(s, 0.0, 1.0), width);
    return base * mask * pow(g, strength);
}

float phiSpiralMask(vec2 uv, float exponent, float decimal, float width, float hardness, float rotation) {
    float radius = length(uv);
    float sr = pow(max(radius, 1e-3), exponent);
    float angle = round(sr) * decimal * kPhiTAU + rotation;
    vec2 target = phiRotate(angle) * radius;
    float line = phiLine(uv - target, width);
    float smoothVal = abs(fract(sr + 0.5) - 0.5);
    float falloff = pow(1.0 - clamp(smoothVal * 1.6, 0.0, 1.0), hardness);
    return line * falloff;
}

vec3 phiPalette(float l, float hueShift, float mixFactor) {
    vec3 baseA = mix(vec3(1.0, 0.75, 0.25), uPrimaryColor, 0.5);
    vec3 baseB = mix(vec3(0.35, 0.55, 0.95), uSecondaryColor, 0.5);
    float wave = sin(l + hueShift) * 0.5 + 0.5;
    return mix(baseA, baseB, mixFactor * wave + (1.0 - mixFactor) * 0.5);
}

vec3 phiToneMap(vec3 c) {
    return c / (1.0 + c);
}

vec4 renderPhiFields(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
    vec2 pixel = (st - 0.5) * aspect;

    float zoom = mix(0.8, 1.3, clamp(bass * 0.6 + energy * 0.4, 0.0, 1.0));
    float exponent = mix(0.85, 1.25, clamp(mid * 0.9 + tempo * 0.2, 0.0, 1.0));
    float spiralExponent = mix(-1.15, 1.15, clamp(high * 1.4 - 0.2, -1.15, 1.15));
    float hueShift = uTime * 0.4 + energy * 0.6;

    vec3 accum = vec3(0.0);
    const int AA = 2;
    for (int j = 0; j < AA; ++j) {
        for (int k = 0; k < AA; ++k) {
            vec2 jitter = (vec2(float(j), float(k)) + vec2(0.5)) / float(AA);
            vec2 uv = (pixel + jitter / uResolution.y) * kPhiScale * zoom;

            vec2 warped = exp(log(max(abs(uv), vec2(1e-4))) * exponent) * sign(uv);
            uv = mix(uv, warped, clamp(mid * 0.7 + high * 0.2, 0.0, 1.0));

            float px = length(fwidth(uv)) + 1e-4;
            float x = uv.x;
            float y = uv.y;
            float len = max(length(uv), 1e-3);

            float metallicCircle = (x * x + y * y - 1.0) / max(y, 1e-3);
            vec3 palette = phiPalette(len, hueShift, clamp(0.35 + energy * 0.3, 0.0, 0.7));

            float width = mix(0.08, 0.14, clamp(energy * 0.6 + bass * 0.3, 0.0, 1.0));
            float depth = mix(0.3, 0.55, clamp(energy, 0.0, 1.0));

            vec3 color = vec3(0.0);
            color += phiGradient(palette, metallicCircle, -hueShift, width, depth, false);
            color += phiGradient(palette, abs(y / max(x, 1e-3)) * sign(y), -hueShift, width, depth, false);
            color += phiGradient(palette, (x * x) / max(y * y, 1e-3) * sign(y), -hueShift, width, depth, false);
            color += phiGradient(palette, (x * x) + (y * y), hueShift, width, depth, true);

            float spiralWidth = px * 2.0 * mix(0.9, 1.3, clamp(energy, 0.0, 1.0));
            float timeSpiral = (uTime * 0.3 + energy * 0.4) / kPhiTAU;
            color += palette * phiSpiralMask(uv, spiralExponent, timeSpiral, spiralWidth, 2.0, 0.0);
            color += palette * phiSpiralMask(uv, spiralExponent, timeSpiral, spiralWidth, 2.0, kPhiPI);
            color += palette * phiSpiralMask(uv, -spiralExponent, timeSpiral, spiralWidth, 2.0, 0.0);
            color += palette * phiSpiralMask(uv, -spiralExponent, timeSpiral, spiralWidth, 2.0, kPhiPI);

            float glow = pow(max(1.0 - len, 0.0), mix(2.0, 3.5, clamp(energy, 0.0, 1.0)));
            color += glow * mix(vec3(0.25, 0.3, 0.45), palette, 0.6);

            float gridAmt = clamp(bass * 0.4 + energy * 0.25 - 0.15, 0.0, 1.0);
            if (gridAmt > 0.01) {
                vec2 grid = abs(fract(uv + 0.5) - 0.5) / px;
                float gridMask = 1.0 - clamp(min(grid.x, grid.y), 0.0, 1.0);
                color.gb += gridAmt * 0.15 * gridMask;
            }

            accum += max(color, 0.0);
        }
    }

    float sampleWeight = 1.0 / float(AA * AA);
    vec3 finalColor = accum * sampleWeight;
    finalColor *= vec3(1.0 + bass * 0.25, 1.0 + mid * 0.25, 1.0 + high * 0.3);
    finalColor = phiToneMap(finalColor);

    float alpha = clamp(0.55 + length(finalColor) * 0.35 + energy * 0.2, 0.0, 1.0);
    return vec4(finalColor, alpha);
}

#line 1 16
// Stage7 - Geometric patterns with beat synchronization
// Based on Danguafer/Silexars shader with audio-reactive modifications

// Modified beat function for audio reactivity
float mb(vec2 p1, vec2 p0, float beat) { 
    return (0.04 + beat) / (pow(p1.x - p0.x, 2.0) + pow(p1.y - p0.y, 2.0)); 
}

vec4 renderStage7(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Audio-reactive beat detection
    float audioBeat = pow(bass * 0.8 + energy * 0.4, 2.0) * 0.05;
    float beat = audioBeat; // Use audio instead of channel time
    
    // Coordinate system
    vec2 p = (2.0 * st - 1.0);
    vec2 o = vec2(pow(p.x, 2.0), pow(p.y, 2.0));
    
    // Base geometric pattern
    vec3 col = vec3(pow(2.0 * abs(o.x + o.y) + abs(o.x - o.y), 5.0));
    col = col * 0.5; // Scale down to prevent oversaturation
    
    // Time with beat influence
    float t = time + beat * 2.0;
    
    // Multiple frequency oscillators
    float t2 = t * 2.0, t3 = t * 3.0, s2 = sin(t2), s3 = sin(t3), s4 = sin(t * 4.0), c2 = cos(t2), c3 = cos(t3);
    
    // Moving blobs for each color channel
    vec2 mbr, mbg, mbb;
    mbr = mbg = mbb = vec2(0.0);
    
    // Red channel movement - responds to bass
    mbr += vec2(0.10 * s4 + 0.40 * c3, 0.40 * s2 + 0.20 * c3);
    mbr += vec2(bass * 0.2 * sin(time * 5.0), bass * 0.2 * cos(time * 3.0));
    
    // Green channel movement - responds to mid
    mbg += vec2(0.15 * s3 + 0.30 * c2, 0.10 * -s4 + 0.30 * c3);
    mbg += vec2(mid * 0.15 * sin(time * 4.0 + 1.0), mid * 0.15 * cos(time * 6.0));
    
    // Blue channel movement - responds to high
    mbb += vec2(0.10 * s3 + 0.50 * c3, 0.10 * -s4 + 0.50 * c2);
    mbb += vec2(high * 0.25 * sin(time * 7.0 + 2.0), high * 0.25 * cos(time * 5.0));
    
    // Distance-based color modulation
    col.r *= length(mbr.xy - p.xy);
    col.g *= length(mbg.xy - p.xy);
    col.b *= length(mbb.xy - p.xy);
    
    // Apply the metaball effect
    col *= pow(mb(mbr, p, beat) + mb(mbg, p, beat) + mb(mbb, p, beat), 1.75);
    
    // Apply scene colors
    vec3 baseColor = mix(uPrimaryColor, uSecondaryColor, uColorBlend);
    col = mix(col, baseColor, 0.2);
    
    // Audio-reactive brightness
    col *= (0.8 + energy * 0.4);
    
    // Add frequency-based color enhancement
    col.r *= (1.0 + bass * 0.3);
    col.g *= (1.0 + mid * 0.3);
    col.b *= (1.0 + high * 0.3);
    
    // Add glow for high energy moments
    if (energy > 0.6) {
        float glow = (energy - 0.6) * 2.5;
        col += vec3(0.2, 0.4, 0.8) * glow;
    }
    
    // Ensure colors are in valid range
    col = clamp(col, 0.0, 1.0);
    
    // Alpha based on intensity and energy
    float alpha = 0.7 + length(col) * 0.15 + energy * 0.3;
    alpha = clamp(alpha, 0.0, 1.0);
    
    return vec4(col, alpha);
}

#line 1 17
// @EFFECT name="Weird Creature" index=39 desc="Endless living creature with audio reactivity" author="Leon Denise"
// Weird Endless Living Creature
// Inspired by Inigo Quilez live stream shader deconstruction
// Leon Denise (ponk) 2019.08.28
// Licensed under hippie love conspiracy
// Audio-reactive adaptation for visualizer

// Using code from Inigo Quilez, Morgan McGuire

// tweak zone
const int count = 15;
const float speed = 1.;
const float balance = 1.5;
const float range = 1.4;
const float radius = .6;
const float blend = .3;
const float falloff = 1.2;

// increment it at your own GPU risk
const float motion_frames = 1.;

// toolbox (using unique names to avoid conflicts)
#define repeat(p,r) (mod(p,r)-r/2.)
float weirdRandom(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
mat2 weirdRot(float a) { float c=cos(a),s=sin(a); return mat2(c,-s,s,c); }
float weirdSmoothmin (float a, float b, float r) { float h = clamp(.5+.5*(b-a)/r, 0., 1.); return mix(b, a, h)-r*h*(1.-h); }
float sdSphere (vec3 p, float r) { return length(p)-r; }
vec3 weirdLook (vec3 eye, vec3 target, vec2 anchor, float fov) {
    vec3 forward = normalize(target-eye);
    vec3 right = normalize(cross(forward, vec3(0,1,0)));
    vec3 up = normalize(cross(right, forward));
    return normalize(forward * fov + right * anchor.x + up * anchor.y);
}

float weirdGeometry (vec3 pos, float time) {
    float scene = 1., a = 1.;
    float t = time * .5 + pos.x / 30.;
    t = floor(t)+smoothstep(0.0,.9,pow(fract(t),2.));
    pos.x = repeat(pos.x+time, 5.);
    for (int i = count; i > 0; --i) {
        pos.x = abs(pos.x)-range*a;
        pos.xy *= weirdRot(cos(t)*balance/a+a*2.);
        pos.zy *= weirdRot(sin(t)*balance/a+a*2.);
        scene = weirdSmoothmin(scene, sdSphere(pos,(radius*a)), blend*a);
        a /= falloff;
    }
    return scene;
}

float weirdRaymarch ( vec3 eye, vec3 ray, float time, out float total ) {
    float dither = weirdRandom(ray.xy+fract(time));
    total = 0.0;
    const int count = 20;
    for (int index = count; index > 0; --index) {
        float dist = weirdGeometry(eye+total*ray,time);
        dist *= 0.9+0.1*dither;
        total += dist;
        if (dist < 0.001 * total) {
            return float(index)/float(count);
        }
    }
    return 0.;
}

vec3 weirdCamera (vec3 eye, float audioMod) {
    // Audio-reactive camera movement
    float audioRotation = audioMod * 0.3;
    eye.yz *= weirdRot(audioRotation);
    eye.xz *= weirdRot(audioRotation * 0.7);
    return eye;
}

vec4 renderWeirdCreature(vec2 st, float uTime, float uTempo, float uEnergy, float uBass, float uMid, float uHigh) {
    vec2 uv = st * 2.0; // Convert from -1,1 to -2,2 range
    
    // Audio-reactive parameters
    float audioMod = uBass * 0.5 + uMid * 0.3 + uHigh * 0.2;
    float speedMod = speed * (1.0 + audioMod * 0.5);
    float rangeMod = range * (1.0 + uEnergy * 0.3);
    
    vec3 eye = weirdCamera(vec3(0,0,4), audioMod);
    vec3 ray = weirdLook(eye, vec3(0), uv, 1.);
    float total = 0.0;
    vec4 fragColor = vec4(0);
    
    for (float index = motion_frames; index > 0.; --index) {
        float dither = weirdRandom(ray.xy+fract(uTime+index));
        float time = uTime*speedMod+(dither+index)/10./motion_frames;
        fragColor += vec4(weirdRaymarch(eye, ray, time, total))/motion_frames;
    }
    
    // extra color with audio reactivity
    vec3 baseColor = vec3(.7,.8,.9);
    baseColor.r *= 1.0 + uBass * 0.4;
    baseColor.g *= 1.0 + uMid * 0.4;
    baseColor.b *= 1.0 + uHigh * 0.4;
    
    fragColor.rgb *= baseColor;
    float d = smoothstep(7.,0.,total);
    
    // Audio-reactive glow
    vec3 glowColor = vec3(0.8,.6,.5);
    glowColor.r *= 1.0 + uBass * 0.5;
    glowColor.g *= 1.0 + uMid * 0.3;
    glowColor *= d * (1.0 + uEnergy * 0.5);
    
    fragColor.rgb += glowColor;
    
    // Add some extra brightness based on energy
    fragColor.rgb *= 1.0 + uEnergy * 0.2;
    
    return fragColor;
}

#line 1 18
// @EFFECT name="Anaglyph Assembly" index=40 desc="Stereoscopic anaglyph with assembly/disassembly" author="Leon Denise"
// Audio-reactive stereoscopic anaglyph inspired by Leon Denise's "Anaglyph Quick Sketch"
// Adapted to the Cascade procedural pipeline with assembly/disassembly behaviour similar to the head shader.

const int kAnaglyphLayerCount = 3;
const int kAnaglyphMarchSteps = 32;
const float kAnaglyphRange = 1.0;
const float kAnaglyphRadius = 0.3;
const float kAnaglyphBlend = 1.5;
const float kAnaglyphBalance = 1.5;
const float kAnaglyphFalloff = 1.9;
const float kAnaglyphDivergence = 0.08;
const float kAnaglyphFieldOfView = 1.2;

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

float anaglyphSimpleNoise(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
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
        pos = abs(pos) - kAnaglyphRange * a * wave;
        scene = anaglyphSmoothMin(scene, length(pos) - kAnaglyphRadius * a, kAnaglyphBlend * a);
        a /= kAnaglyphFalloff;
    }

    return scene;
}

float anaglyphZoneThreshold(vec3 pos, float assemblyFactor) {
    float normalizedHeight = clamp((pos.y + 2.5) / 5.0, 0.0, 1.0);
    float threshold = 0.08 + normalizedHeight * 0.4;
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
    const float eps = 0.003;
    vec4 q = vec4(eps, -eps, -eps, 0.0);
    return normalize(vec3(
        anaglyphMap(pos + q.xzz) - anaglyphMap(pos - q.xzz),
        anaglyphMap(pos + q.zxz) - anaglyphMap(pos - q.zxz),
        anaglyphMap(pos + q.zzx) - anaglyphMap(pos - q.zzx)
    ));
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

        if (dist < 0.005) {
            vec3 normal = anaglyphCalcNormal(pos);
            vec3 lightDir = normalize(vec3(-0.6, 0.8, 0.4));
            float diff = max(dot(normal, lightDir), 0.0);

            float assemblyFactor = anaglyphAssemblyFactor();
            vec3 basePalette = mix(uPrimaryColor, uSecondaryColor, clamp(0.35 + assemblyFactor * 0.5, 0.0, 1.0));

            vec3 color = basePalette * (0.3 + diff * (0.9 + uEnergy * 0.4));

            float fog = exp(-travel * 0.5);
            vec3 ambient = mix(uPrimaryColor, uSecondaryColor, 0.5) * 0.1;
            color = mix(ambient, color, fog);

            float alpha = clamp(0.5 + diff * 0.3 + assemblyFactor * 0.3, 0.0, 1.0);
            return vec4(clamp(color, 0.0, 1.0), alpha);
        }

        travel += dist * 0.9;
        if (travel > 12.0) {
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

#line 1 19
// @EFFECT name="Message Tunnel" index=41 desc="Audio-reactive tunnel riff with trace marching" author="System"
// Little Message Redux - audio-reactive tunnel riff
const int kMessageTraceSteps = 48;
const float kMessagePi = 3.14159265359;

mat2 messageRot(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, s, -s, c);
}

float messageMap(vec3 p, float time, float energy, float bass, float mid, float high) {
    vec3 pp = p;
    pp.z = abs(pp.z) - 2.0;
    pp.z = abs(pp.z) - 2.0;

    float gate = (time * 2.0) + bass * 1.5;
    pp.x += gate;

    float d = 1000.0;
    const int n = 16;
    float freqMix = clamp(0.4 + bass * 0.3 + high * 0.2, 0.2, 1.0);
    for (int i = 1; i < n; ++i) {
        float x = float(i) / float(n);
        vec3 q = pp;
        q.x += x * kMessagePi * 1.0;
        q.z = abs(q.z) - 1.0;
        q.yz *= messageRot(x * q.x * freqMix);
        q.y = abs(q.y) - mix(1.4, 2.2, clamp(energy, 0.0, 1.0));
        q.y += x * kMessagePi * 0.5;
        float k = length(q.yz) - mix(0.08, 0.16, clamp(high, 0.0, 1.0));
        d = min(d, k);
        d = max(d, 1.6 * x + gate - pp.x);
    }
    return min(d, 4.0 + gate - pp.x);
}

float messageTrace(vec3 origin, vec3 ray, float time, float energy, float bass, float mid, float high) {
    float t = 0.0;
    for (int i = 0; i < kMessageTraceSteps; ++i) {
        vec3 pos = origin + ray * t;
        float dist = messageMap(pos, time, energy, bass, mid, high) * 0.55;
        t += dist;
        if (abs(dist) < 0.0006 || t > 18.0) {
            break;
        }
    }
    return t;
}

vec3 messageShade(vec3 o, vec3 r, float time, float energy, float bass, float mid, float high) {
    float t = messageTrace(o, r, time, energy, bass, mid, high);
    vec3 w = o + r * t;
    float fd = messageMap(w, time, energy, bass, mid, high);

    float inv = 1.0 / (1.0 + t * t * 0.1 + abs(fd) * 1200.0);

    vec3 baseColor = mix(uPrimaryColor * 0.6, uSecondaryColor * 0.8, clamp(0.4 + high * 0.6, 0.0, 1.0));
    vec3 color = baseColor * inv;

    float bloom = 1.0 / (1.0 + t * t * 0.18);
    color = mix(color, vec3(1.0), bloom * 0.35);
    color = mix(vec3(0.1, 0.11, 0.13), color, 1.0 / (1.0 + t * t * 0.1));

    float scan = sign((fract((r.y + 0.5) * 14.0) - 0.5) / 16.0) * 0.5 + 0.5;
    float vignette = smoothstep(1.2, 0.2, length(r.xy));
    color = mix(color * 0.55, color, scan * vignette);

    color *= 1.0 + vec3(bass * 0.3, mid * 0.25, high * 0.4);
    return clamp(color, 0.0, 1.0);
}

vec4 renderMessageTunnel(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Apply exact raymarched object coordinate system like Walker
    vec2 uv = st;
    uv.x *= uResolution.x / max(uResolution.y, 1.0);

    vec3 r = normalize(vec3(uv, 1.0 - dot(uv, uv) * 0.45));
    vec3 o = vec3(0.0);  // Neutral origin like raymarched object

    r.xz *= messageRot(kMessagePi * 0.5 + tempo * 0.15);

    vec3 color = messageShade(o, r, time, energy, bass, mid, high);
    float alpha = clamp(0.65 + length(color) * 0.3 + energy * 0.2, 0.0, 1.0);

    return vec4(color, alpha);
}

#line 1 20
// @EFFECT name="Pouet Grid" index=42 desc="Audio-reactive UV distortion homage" author="Danilo Guanabara"
// Pouet Grid Redux - audio-reactive UV distortion homage (Danilo Guanabara)
const float kPouetPi = 3.14159265359;
const float kPouetTau = kPouetPi * 2.0;

mat2 pouetRot(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, s, -s, c);
}

vec3 pouetPalette(float bass, float mid, float high) {
    vec3 base = mix(uPrimaryColor, uSecondaryColor, 0.5 + 0.5 * sin(uTime * 0.3));
    vec3 lift = vec3(0.6 + bass * 0.4, 0.5 + mid * 0.3, 0.7 + high * 0.5);
    return clamp(base * lift, 0.0, 1.5);
}

vec4 renderPouetGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec3 color = vec3(0.0);
    float z = time * mix(1.5, 2.5, clamp(tempo * 0.3 + energy * 0.4, 0.0, 1.0));

    vec2 fragCoord = st * uResolution.xy;

    for (int i = 0; i < 3; ++i) {
        vec2 uv;
        vec2 p = fragCoord / uResolution.xy;
        uv = p;
        
        // Apply exact raymarched object coordinate system
        p.x *= uResolution.x / max(uResolution.y, 1.0);
        // No manual offset - keep neutral coordinates like raymarched object

        float l = length(p) + 1e-4;
        float wobble = sin(z * 3.0) + 1.0;  // Changed from sin(z * 2.0) to sin(z * 3.0) for even faster wobble
        float pulse = abs(sin(l * (9.0 + high * 4.0) - z - z));
        vec2 ripple = p / l * wobble * pulse * mix(0.4, 0.8, clamp(energy, 0.0, 1.0));

        vec2 extra = vec2(0.0);
        extra.x = sin(p.y * 3.0 + time * 1.2) * 0.05 * bass;
        extra.y = cos(p.x * 4.0 - time * 0.9) * 0.04 * mid;

        uv += ripple + extra;
        uv += 0.03 * vec2(sin(z + float(i)), cos(z - float(i))) * clamp(high * 0.6, 0.0, 1.0);

        vec2 cell = mod(uv, 1.0) - 0.5;
        float dist = length(cell);
        float channel = 0.01 / max(dist, 1e-3);
        color[i] = channel;
        z += 0.07 + high * 0.02;
    }

    vec3 palette = pouetPalette(bass, mid, high);
    color *= palette;

    float vignette = smoothstep(1.4, 0.3, length(st * 2.0 - 1.0));
    color *= vignette;

    float alpha = clamp(0.5 + length(color) * 0.4 + energy * 0.25, 0.0, 1.0);
    return vec4(clamp(color, 0.0, 1.0), alpha);
}

#line 1 21
// @EFFECT name="Cylinder Repeat" index=43 desc="Volumetric raymarch with audio-reactive glow" author="System"
// Cylinder Repeat Redux - volumetric raymarch with audio-reactive glow
const float CYL_NEAR_CLIP = 2.8;
const float CYL_FAR_CLIP = 30.0;
const int   CYL_MAX_STEPS = 128;
const float CYL_STEP_MIN = 0.01;
const float CYL_STEP_MULT = 32.0 / float(CYL_MAX_STEPS);
const float CYL_REPEAT_SCALE = 10.0;
const float CYL_HALF_OFFSET = -0.05;
const float CYL_GLOW_GAIN = 10.0;
const float CYL_ASPECT_A = 2.35;
const float CYL_ASPECT_B = 16.0 / 9.0;
const vec3  CYL_CAM_EYE = vec3(7.0, 8.0, 9.0);
const vec3  CYL_CAM_TARGET = vec3(0.0, -10.0, 0.0);
const vec3  CYL_CAM_UP = vec3(0.2, 1.0, 0.0);
const float CYL_JITTER_DIST = 0.5;

float cylRand(vec2 n) {
    return fract(sin(dot(n, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 cylHash3(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

vec3 cylTruncate(vec3 p, vec3 levels) {
    return floor(p * levels) / levels;
}

vec2 cylRotate(vec2 v, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

float cylSaturateScalar(float v) {
    return clamp(v, 0.0, 1.0);
}

vec3 cylSaturateVec(vec3 v) {
    return clamp(v, vec3(0.0), vec3(1.0));
}

float cylSdSphere(vec3 p, float r) {
    return length(p) - r;
}

vec2 cylHalfCircle(vec3 p, float bass, float mid, float high) {
    vec3 ofs = cylHash3(cylTruncate(p, vec3(13.0)));
    vec3 h2 = cylHash3(cylTruncate(p, vec3(32.0) + ofs));
    vec3 h3 = cylHash3(cylTruncate(p, vec3(16.0)));
    p += 0.02 * h2 + 0.01 * h3;

    float radius = 1.0 + bass * 0.35;
    float d = cylSdSphere(p, radius);
    d = max(d, -cylSdSphere(p, radius * 0.9));
    d = max(d,  p.x - 0.1);
    d = max(d, -p.x - 0.1);

    vec2 result;
    result.x = d + CYL_HALF_OFFSET;
    result.y = step(0.75, min(max(h3.x, h2.x), max(ofs.x, h2.y)) + high * 0.1);
    return result;
}

vec2 cylMinPair(vec2 a, vec2 b) {
    return (a.x < b.x) ? a : b;
}

vec4 cylRadialRepeat(vec3 p, float time, float bass, float mid, float high) {
    vec2 best = vec2(CYL_FAR_CLIP, 0.0);
    vec3 dp = p;

    vec2 rotXZ = cylRotate(dp.xz, 0.3 * time);
    dp.x = rotXZ.x;
    dp.z = rotXZ.y;
    best = cylMinPair(best, cylHalfCircle(dp, bass, mid, high));

    vec2 rotXY = cylRotate(dp.xy, -0.7 * time);
    dp.x = rotXY.x;
    dp.y = rotXY.y;
    best = cylMinPair(best, cylHalfCircle(dp * 1.3, bass, mid, high));

    vec2 rotXZ2 = cylRotate(vec2(dp.x, dp.z), 1.1 * time);
    dp.x = rotXZ2.x;
    dp.z = rotXZ2.y;
    best = cylMinPair(best, cylHalfCircle(dp * 1.8, bass, mid, high));

    vec2 rotXZ3 = cylRotate(vec2(dp.x, dp.z), -1.3 * time);
    dp.x = rotXZ3.x;
    dp.z = rotXZ3.y;
    best = cylMinPair(best, cylHalfCircle(dp * 2.8, bass, mid, high));

    return vec4(best, 0.0, 0.0);
}

vec4 cylScene(vec3 p, float time, float bass, float mid, float high) {
    vec3 local = (p - CYL_CAM_TARGET) / CYL_REPEAT_SCALE;
    vec4 repeat = cylRadialRepeat(local, time, bass, mid, high);
    repeat.x *= CYL_REPEAT_SCALE;
    return repeat;
}

vec4 cylMarchRay(inout vec3 pos, vec3 dir, float time, float bass, float mid, float high,
                 out int stepsTaken, out float minDist) {
    vec4 sample = vec4(0.0);
    float travelled = 0.0;
    minDist = 1e6;
    stepsTaken = 0;
    for (int i = 0; i < CYL_MAX_STEPS; ++i) {
        sample = cylScene(pos, time, bass, mid, high);
        stepsTaken++;
        minDist = min(minDist, sample.x);
        if (sample.x < 0.0 || travelled > CYL_FAR_CLIP) {
            break;
        }
        float stepLen = CYL_STEP_MIN + CYL_STEP_MULT * sample.x;
        pos += dir * stepLen;
        travelled += stepLen;
    }
    return sample;
}

vec3 cylEstimateNormal(vec3 pos, float dist, float time, float bass, float mid, float high) {
    const float eps = 0.35;
    float dx = cylScene(pos + vec3(eps, 0.0, 0.0), time, bass, mid, high).x;
    float dy = cylScene(pos + vec3(0.0, eps, 0.0), time, bass, mid, high).x;
    float dz = cylScene(pos + vec3(0.0, 0.0, eps), time, bass, mid, high).x;
    return normalize(vec3(dx - dist, dy - dist, dz - dist));
}

vec3 cylHsvToRgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 cylRgbToHsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 cylHsvToRgbSafe(vec3 hsv) {
    return cylSaturateVec(cylHsvToRgb(hsv));
}

vec3 cylRgbToHsvSafe(vec3 rgb) {
    return cylRgbToHsv(cylSaturateVec(rgb));
}

vec4 renderCylinderRepeat(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Apply exact raymarched object coordinate system like Walker
    vec2 fragCoord = st * uResolution.xy;

    vec3 forward = normalize(CYL_CAM_TARGET - CYL_CAM_EYE);
    vec3 right = normalize(cross(forward, CYL_CAM_UP));
    vec3 up = normalize(cross(right, forward));

    vec2 centered = fragCoord / uResolution.xy;  // Remove -vec2(0.5) like Walker
    float aspect = mix(CYL_ASPECT_A, CYL_ASPECT_B, 1.0);

    // Apply raymarched object style centering (no Y adjustment)
    vec3 camPos = CYL_CAM_EYE;  // Neutral camera position like raymarched object
    vec3 dir = normalize(forward + right * (centered.x * aspect) + up * centered.y);  // No Y adjustment
    vec3 origin = camPos + dir * CYL_NEAR_CLIP;

    float jitter = cylRand(centered + vec2(fract(time * 0.37)));
    origin -= dir * (4.0 * CYL_JITTER_DIST * jitter);

    vec3 marchPos = origin;
    int steps = 0;
    float minDist;
    vec4 sample = cylMarchRay(marchPos, dir, time, bass, mid, high, steps, minDist);

    bool hit = sample.x < 0.0;
    if (!hit) {
        vec3 bg = mix(uPrimaryColor * 0.1, uSecondaryColor * 0.2, cylSaturateScalar(high * 0.6));
        return vec4(bg, 0.3);
    }

    vec3 normal = cylEstimateNormal(marchPos, sample.x, time, bass, mid, high);

    vec3 baseColor = mix(vec3(0.05, 0.07, 0.09), mix(uPrimaryColor, uSecondaryColor, 0.5), cylSaturateScalar(sample.y));
    float shade = cylSaturateScalar(dot(normal, -dir) * 0.5 + 0.5);
    vec3 color = mix(vec3(0.05, 0.07, 0.09), baseColor, shade);

    float iterationRatio = (float(steps) + 0.5 - jitter) / float(CYL_MAX_STEPS);
    float glow = CYL_GLOW_GAIN * pow(iterationRatio, 2.3);
    vec3 glowColor = mix(uPrimaryColor, uSecondaryColor, cylSaturateScalar(0.5 + high * 0.4));
    color += glow * glowColor * 0.18;

    float vignette = pow(1.5 - length(centered * vec2(2.35, 1.0)), 2.0);
    color *= cylSaturateScalar(vignette);

    vec3 hsv = cylRgbToHsvSafe(color);
    hsv.x = fract(hsv.x - 0.1 * pow(0.9 - st.y, 2.0) - tempo * 0.03);
    hsv.y = clamp(hsv.y + bass * 0.2, 0.0, 1.0);
    hsv.z *= 1.0 + energy * 0.3;
    color = cylHsvToRgbSafe(hsv);

    color *= 1.0 + vec3(bass * 0.25, mid * 0.2, high * 0.35);
    color = cylSaturateVec(color);

    float alpha = clamp(0.6 + length(color) * 0.35 + energy * 0.2, 0.0, 1.0);
    return vec4(color, alpha);
}

#line 1 22
// @EFFECT name="Head" index=29 desc="Hand modeled 3D head with audio-reactive assembly" author="System"
/*

Head
----

This was painstakingly hand modeled by 'tracing' a polygonal model
exported from Daz 3D.

The initial approach is to smooth blend small ellipses with a large
blend radius, a technique I took from Ink Drawing by lnae
(https://www.shadertoy.com/view/MltcDB)

The brow and jawline are formed by blending planes and spheres.
The nose is, of course, a few capsules.
The ears are various extruded and warped 2D layers, they were by
far the hardest part.

I encourage you to comment parts out and see how it all adds up.

Uses a few primitives and tools from HG_SDF and IQ.

Apologies for the boring shading, you can see some more interesting
applications in the GIF and 4K that this was created for:

* Fractal Polycephaly https://media.giphy.com/media/J2xwceb3Kk50fGXWdj/giphy.gif
* ᴇ s ᴄ ʜ ᴇ ʀ ᴡ ᴀ ᴠ ᴇ https://www.shadertoy.com/view/wtf3RM


License: Creative Commons Attribution-NonCommercial
https://creativecommons.org/licenses/by-nc/4.0/

*/

#define PI 3.14159265359

// Note: uCameraZoom, uCameraOffsetX, uCameraOffsetY are declared in procedural_main.glsl

// Optimized rotation using sin/cos only once
void pR(inout vec2 p, float a) {
    float s = sin(a), c = cos(a);
    p = vec2(c*p.x - s*p.y, s*p.x + c*p.y);
}

vec2 pRi(vec2 p, float a) {
    pR(p, a);
    return p;
}

float vmax(vec2 v) {
    return max(v.x, v.y);
}

float vmax(vec3 v) {
    return max(max(v.x, v.y), v.z);
}

float vmin(vec3 v) {
    return min(min(v.x, v.y), v.z);
}

float vmin(vec2 v) {
    return min(v.x, v.y);
}

float fBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return length(max(d, vec3(0))) + vmax(min(d, vec3(0)));
}

float fCorner2(vec2 p) {
    return length(max(p, vec2(0))) + vmax(min(p, vec2(0)));
}

float fDisc(vec3 p, float r) {
    float l = length(p.xz) - r;
    return l < 0. ? abs(p.y) : length(vec2(p.y, l));
}

// IQ https://www.shadertoy.com/view/Xds3zN
float sdRoundCone( in vec3 p, in float r1, in float r2, float h )
{
    vec2 q = vec2( length(p.xz), p.y );
    
    float b = (r1-r2)/h;
    float a = sqrt(1.0-b*b);
    float k = dot(q,vec2(-b,a));
    
    if( k < 0.0 ) return length(q) - r1;
    if( k > a*h ) return length(q-vec2(0.0,h)) - r2;
        
    return dot(q, vec2(a,b) ) - r1;
}

float smin2(float a, float b, float r) {
    vec2 u = max(vec2(r - a,r - b), vec2(0));
    return max(r, min (a, b)) - length(u);
}

float smax2(float a, float b, float r) {
    vec2 u = max(vec2(r + a,r + b), vec2(0));
    return min(-r, max (a, b)) + length(u);
}

float smin(float a, float b, float k){
    float f = clamp(0.5 + 0.5 * ((a - b) / k), 0., 1.);
    return (1. - f) * a + f  * b - f * (1. - f) * k;
}

float smax(float a, float b, float k) {
    return -smin(-a, -b, k);
}

// Removed smin3/smax3 - use single faster smin/smax instead
float smin3(float a, float b, float k){
    return smin(a, b, k);
}

float smax3(float a, float b, float k){
    return smax(a, b, k);
}


float ellip(vec3 p, vec3 s) {
    float r = vmin(s);
    p *= r / s;
    return length(p) - r;
}

float ellip(vec2 p, vec2 s) {
    float r = vmin(s);
    p *= r / s;
    return length(p) - r;
}

// Eye flag - passed by reference-like pattern using a vec2 return
vec2 mHeadWithEye(vec3 p) {
    bool isEyeLocal = false;
    
    pR(p.yz, -.1);
    p.y -= .11;

    vec3 pa = p;
    vec3 ps = p;
    ps.x = sqrt(ps.x * ps.x + .0005);
    p.x = abs(p.x);
    vec3 pp = p;

    float d = 1e12;

    // skull back
    p += vec3(0,-.135,.09);
    d = ellip(p, vec3(.395, .385, .395));

    // skull base
    p = pp;
    p += vec3(0,-.135,.09) + vec3(0,.1,.07);
    d = smin(d, ellip(p, vec3(.38, .36, .35)), .05);

    // forehead
    p = pp;
    p += vec3(0,-.145,-.175);
    d = smin(d, ellip(p, vec3(.315, .3, .33)), .18);

    p = pp;
    pR(p.yz, -.5);
    float bb = fBox(p, vec3(.5,.67,.7));
    d = smax(d, bb, .2);

    // face base
    p = pp;
    p += vec3(0,.25,-.13);
    d = smin(d, length(p) - .28, .1);

    // behind ear
    p = ps;
    p += vec3(-.15,.13,.06);
    d = smin(d, ellip(p, vec3(.15,.15,.15)), .15);

    p = ps;
    p += vec3(-.07,.18,.1);
    d = smin(d, length(p) - .2, .18);

    // cheek base
    p = pp;
    p += vec3(-.2,.12,-.14);
    d = smin(d, ellip(p, vec3(.15,.22,.2) * .8), .15);

    // jaw base
    p = pp;
    p += vec3(0,.475,-.16);
    pR(p.yz, .8);
    d = smin(d, ellip(p, vec3(.19,.1,.2)), .1);
    
    // brow
    p = pp;
    p += vec3(0,-.0,-.18);
    vec3 bp = p;
    float brow = length(p) - .36;
    p.x -= .37;
    brow = smax(brow, dot(p, normalize(vec3(1,.2,-.2))), .2);
    p = bp;
    brow = smax(brow, dot(p, normalize(vec3(0,.6,1))) - .43, .25);
    p = bp;
    pR(p.yz, -.5);
    float peak = -p.y - .165;
    peak += smoothstep(.0, .2, p.x) * .01;
    peak -= smoothstep(.12, .29, p.x) * .025;
    brow = smax(brow, peak, .07);
    p = bp;
    pR(p.yz, .5);
    brow = smax(brow, -p.y - .06, .15);
    d = smin(d, brow, .06);

    // jaw - optimized with pre-normalized vectors
    vec3 jo = vec3(-.25,.4,-.07);
    p = ps + jo;
    // Pre-computed normalized vectors for performance
    vec3 jn1 = normalize(vec3(1,-.2,-.05));
    vec3 jn2 = normalize(vec3(.5,-.25,.35));
    vec3 jn3 = normalize(vec3(-.0,-1.,-.8));
    vec3 jn4 = normalize(vec3(.98,-1.,.15));
    vec3 jn5 = normalize(vec3(.6,-.2,-.45));
    vec3 jn6 = normalize(vec3(.5,.1,-.5));
    vec3 jn7 = normalize(vec3(1,.2,-.3));
    
    float jaw = dot(p, jn1) - .069;
    jaw = smax(jaw, dot(p, jn2) - .13, .12);
    jaw = smax(jaw, dot(p, jn3) - .12, .15);
    jaw = smax(jaw, dot(p, jn4) - .13, .08);
    jaw = smax(jaw, dot(p, jn5) - .19, .15);
    jaw = smax(jaw, dot(p, jn6) - .26, .15);
    jaw = smax(jaw, dot(p, jn7) - .22, .15);

    p = pp;
    p += vec3(0,.63,-.2);
    pR(p.yz, .15);
    float cr = .5;
    jaw = smax(jaw, length(p.xy - vec2(0,cr)) - cr, .05);

    p = pp + jo;
    // Pre-compute remaining normalized vectors
    vec3 jn8 = normalize(vec3(0,-.4,1));
    vec3 jn9 = normalize(vec3(0,1.5,2));
    jaw = smax(jaw, dot(p, jn8) - .35, .1);
    jaw = smax(jaw, dot(p, jn9) - .3, .2);
    jaw = max(jaw, length(pp + vec3(0,.6,-.3)) - .7);

    p = pa;
    p += vec3(.2,.5,-.1);
    float jb = length(p);
    // Simplified smoothstep
    jb = clamp(jb / .4, 0., 1.);
    float js = mix(0., -.005, jb);
    jb = mix(.01, .04, jb);

    d = smin(d, jaw - js, jb);

    // chin
    p = pp;
    p += vec3(0,.585,-.395);
    p.x *= .7;
    d = smin(d, ellip(p, vec3(.028,.028,.028)*1.2), .15);

    // nose
    p = pp;
    p += vec3(0,.03,-.45);
    pR(p.yz, 3.);
    d = smin(d, sdRoundCone(p, .008, .05, .18), .1);

    p = pp;
    p += vec3(0,.06,-.47);
    pR(p.yz, 2.77);
    d = smin(d, sdRoundCone(p, .005, .04, .225), .05);

    // cheek

    p = pp;
    p += vec3(-.2,.2,-.28);
    pR(p.xz, .5);
    pR(p.yz, .4);
    float ch = ellip(p, vec3(.1,.1,.12)*1.05);
    d = smin(d, ch, .1);

    p = pp;
    p += vec3(-.26,.02,-.1);
    pR(p.xz, .13);
    pR(p.yz, .5);
    float temple = ellip(p, vec3(.1,.1,.15));
    temple = smax(temple, p.x - .07, .1);
    d = smin(d, temple, .1);

    p = pp;
    p += vec3(.0,.2,-.32);
    ch = ellip(p, vec3(.1,.08,.1));
    d = smin(d, ch, .1);

    p = pp;
    p += vec3(-.17,.31,-.17);
    ch = ellip(p, vec3(.1));
    d = smin(d, ch, .1);

    // mouth base
    p = pp;
    p += vec3(-.0,.29,-.29);
    pR(p.yz, -.3);
    d = smin(d, ellip(p, vec3(.13,.15,.1)), .18);

    p = pp;
    p += vec3(0,.37,-.4);
    d = smin(d, ellip(p, vec3(.03,.03,.02) * .5), .1);

    p = pp;
    p += vec3(-.09,.37,-.31);
    d = smin(d, ellip(p, vec3(.04)), .18);

    // bottom lip
    p = pp;
    p += vec3(0,.455,-.455);
    p.z += clamp(p.x / .2, 0., 1.) * .05;
    float lb = mix(.035, .03, clamp((length(p) - .05) / (.15 - .05), 0., 1.));
    vec3 ls = vec3(.055,.028,.022) * 1.25;
    float w = .192;
    vec2 pl2 = vec2(p.x, length(p.yz * vec2(.79,1)));
    float bottomlip = length(pl2 + vec2(0,w-ls.z)) - w;
    bottomlip = smax(bottomlip, length(pl2 - vec2(0,w-ls.z)) - w, .055);
    d = smin(d, bottomlip, lb);
    
    // top lip
    p = pp;
    p += vec3(0,.38,-.45);
    pR(p.xz, -.3);
    ls = vec3(.065,.03,.05);
    w = ls.x * (-log(ls.y/ls.x) + 1.);
    vec3 pl = p * vec3(.78,1,1);
    float toplip = length(pl + vec3(0,w-ls.y,0)) - w;
    toplip = smax(toplip, length(pl - vec3(0,w-ls.y,0)) - w, .065);
    p = pp;
    p += vec3(0,.33,-.45);
    pR(p.yz, .7);
    float cut;
    // Pre-compute normalized vectors for lip cut
    vec3 ln1 = normalize(vec3(.5,.25,0));
    vec3 ln2 = normalize(vec3(-.5,.5,0));
    vec3 ln3 = normalize(vec3(.5,.5,0));
    cut = dot(p, ln1) - .056;
    float dip = smin(
        dot(p, ln2) + .005,
        dot(p, ln3) + .005,
        .025
    );
    cut = smax(cut, dip, .04);
    cut = smax(cut, p.x - .1, .05);
    toplip = smax(toplip, cut, .02);

    d = smin(d, toplip, .07);

    // seam
    p = pp;
    p += vec3(0,.425,-.44);
    lb = length(p);
    float lr = mix(.04, .02, clamp((lb - .05) / (.12 - .05), 0., 1.));
    pR(p.yz, .1);
    p.y -= clamp(p.x / .03, 0., 1.) * .002;
    p.y += clamp((p.x - .03) / (.1 - .03), 0., 1.) * .007;
    p.z -= .133;
    float seam = fDisc(p, .2);
    seam = smax(seam, -d - .015, .01); // fix inside shape
    d = mix(d, smax(d, -seam, lr), .65);

    // nostrils base
    p = pp;
    p += vec3(0,.3,-.43);
    d = smin(d, length(p) - .05, .07);

    // nostrils
    p = pp;
    p += vec3(0,.27,-.52);
    pR(p.yz, .2);
    float nostrils = ellip(p, vec3(.055,.05,.06));

    p = pp;
    p += vec3(-.043,.28,-.48);
    pR(p.xy, .15);
    p.z *= .8;
    nostrils = smin(nostrils, sdRoundCone(p, .042, .0, .12), .02);

    d = smin(d, nostrils, .02);

    p = pp;
    p += vec3(-.033,.3,-.515);
    pR(p.xz, .5);
    d = smax(d, -ellip(p, vec3(.011,.03,.025)), .015);

    //return d;

    // eyelids
    p = pp;
    p += vec3(-.16,.07,-.34);
    float eyelids = ellip(p, vec3(.08,.1,.1));

    p = pp;
    p += vec3(-.16,.09,-.35);
    float eyelids2 = ellip(p, vec3(.09,.1,.07));

    // edge top
    p = pp;
    p += vec3(-.173,.148,-.43);
    p.x *= .97;
    float et = length(p.xy) - .09;

    // edge bottom - optimized with pre-normalized vectors
    p = pp;
    p += vec3(-.168,.105,-.43);
    p.x *= .9;
    vec3 en1 = normalize(vec3(-.1,-1,-.2));
    vec3 en2 = normalize(vec3(-.3,-1,0));
    vec3 en3 = normalize(vec3(.5,-1,-.5));
    float eb = dot(p, en1) + .001;
    eb = smin(eb, dot(p, en2) - .006, .01);
    eb = smax(eb, dot(p, en3) - .018, .05);

    float edge = max(max(eb, et), -d);

    d = smin(d, eyelids, .01);
    d = smin(d, eyelids2, .03);
    d = smax(d, -edge, .005);

    // eyeball
    p = pp;
    p += vec3(-.165,.0715,-.346);
    float eyeball = length(p) - .088;
    isEyeLocal = eyeball < d;
    d = min(d, eyeball);

    // tear duct
    p = pp;
    p += vec3(-.075,.1,-.37);
    d = min(d, length(p) - .05);

    
 	// ear
    p = pp;
    p += vec3(-.405,.12,.10);
    pR(p.xy, -.12);
    pR(p.xz, .35);
    pR(p.yz, -.3);
    vec3 pe = p;

    // base - simplified smoothstep
    float ear = p.s + clamp((p.y + .05) / (.1 + .05), 0., 1.) * .015 - .005;
    float earback = -ear - mix(.001, .025, clamp((.3 - p.y) / (.3 + .2), 0., 1.));

    // inner
    pR(p.xz, -.5);
    float iear = ellip(p.zy - vec2(.01,-.03), vec2(.045,.05));
    iear = smin(iear, length(p.zy - vec2(.04,-.09)) - .02, .09);
    float ridge = iear;
    iear = smin(iear, length(p.zy - vec2(.1,-.03)) - .06, .07);
    ear = smax2(ear, -iear, .04);
    earback = smin(earback, iear - .04, .02);

    // ridge
    p = pe;
    pR(p.xz, .2);
    ridge = ellip(p.zy - vec2(.01,-.03), vec2(.045,.055));
    ridge = smin3(ridge, -pRi(p.zy, .2).x - .01, .015);
    ridge = smax3(ridge, -ellip(p.zy - vec2(-.01,.1), vec2(.12,.08)), .02);

    float ridger = .01;

    ridge = max(-ridge, ridge - ridger);

    ridge = smax2(ridge, abs(p.x) - ridger/2., ridger/2.);

    ear = smin(ear, ridge, .045);

    p = pe;

    // outline
    float outline = ellip(pRi(p.yz, .2), vec2(.12,.09));
    outline = smin(outline, ellip(p.yz + vec2(.155,-.02), vec2(.035, .03)), .14);

    // edge
    float eedge = p.x + smoothstep(.2, -.4, p.y) * .06 - .03;

    float edgeo = ellip(pRi(p.yz, .1), vec2(.095,.065));
    edgeo = smin(edgeo, length(p.zy - vec2(0,-.1)) - .03, .1);
    float edgeoin = smax(abs(pRi(p.zy, .15).y + .035) - .01, -p.z-.01, .01);
    edgeo = smax(edgeo, -edgeoin, .05);

    // Simplified smoothstep chain in ear dent
    float eedent = clamp((-p.z + .05) / (.05 + .05), 0., 1.) * clamp((.06 - fCorner2(vec2(-p.z, p.y))) / .06, 0., 1.);
    eedent += clamp((.1 + p.z) / .2, 0., 1.) * .2;
    eedent += clamp((.1 - p.y) / .2, 0., 1.) * clamp((p.z + .03) / .03, 0., 1.) * .3;
    eedent = min(eedent, 1.);

    eedge += eedent * .06;

    eedge = smax(eedge, -edgeo, .01);
    ear = smin(ear, eedge, .01);
    ear = max(ear, earback);

    ear = smax2(ear, outline, .015);

    d = smin(d, ear, .015);

    // targus
    p = pp;
    p += vec3(-.34,.2,.02);
    d = smin2(d, ellip(p, vec3(.015,.025,.015)), .035);
    p = pp;
    p += vec3(-.37,.18,.03);
    pR(p.xz, .5);
    pR(p.yz, -.4);
    d = smin(d, ellip(p, vec3(.01,.03,.015)), .015);
    
    return vec2(d, isEyeLocal ? 1.0 : 0.0);
}

// Wrapper for backward compatibility - returns just distance
float mHead(vec3 p) {
    return mHeadWithEye(p).x;
}

// sstep function kept for compatibility but simplified usage
float sstep(float t) {
    float x = t * PI - PI / 2.;
    return sin(x) * .5 + .5;
}

vec2 mapWithEye(vec3 p) {
    
    float scale = 1.;
    float s = .2;
    // Simplified rotation - removed nested sstep calls for performance
    float t = mod(uTime * s, 1.);
    float ry = t * PI * 2.;  // Direct linear rotation instead of complex easing
    float rx = sin(uTime * .33) * .2;
    
    pR(p.yz, rx);
    pR(p.xz, ry);
    
    p /= scale;
    
    // Audio-reactive assembly/disassembly
    float audioEnergy = uBass * 0.5 + uMid * 0.3 + uHigh * 0.2;
    // Add base energy level so head is visible even without audio
    float baseEnergy = 0.3; // Minimum energy level
    audioEnergy = max(audioEnergy, baseEnergy);
    // Simplified smoothstep to clamp
    float assemblyFactor = clamp((audioEnergy - 0.1) / (0.8 - 0.1), 0., 1.);
    
    // Create different assembly zones based on height
    float headHeight = p.y + 0.5;  // Normalize height (head is roughly -0.5 to 0.5)
    
    // Different parts assemble at different energy levels
    float zoneThreshold;
    if (headHeight > 0.2) {
        // Top parts (forehead, top of head) - need high energy
        zoneThreshold = 0.4; // Reduced from 0.7
    } else if (headHeight > 0.0) {
        // Middle parts (eyes, nose) - need medium energy  
        zoneThreshold = 0.25; // Reduced from 0.4
    } else if (headHeight > -0.2) {
        // Lower middle (mouth, cheeks) - need low-medium energy
        zoneThreshold = 0.15; // Reduced from 0.2
    } else {
        // Bottom parts (chin, jaw) - assemble first
        zoneThreshold = 0.05; // Reduced from 0.1
    }
    
    // Simplified noise for better performance
    float noise = sin(p.x * 5.0 + uTime) * 0.5 + 0.5;  // Reduced complexity
    zoneThreshold += noise * 0.05 * (1.0 - assemblyFactor);
    
    // Apply assembly threshold
    if (assemblyFactor < zoneThreshold) {
        // Disassembled state - return large distance (invisible)
        return vec2(1000.0, 0.0);
    }
    
    vec2 headResult = mHeadWithEye(p);
    headResult.x *= scale;
    return headResult;
}

float map(vec3 p) {
    return mapWithEye(p).x;
}

// Optimized normal calculation using standard 6-point method (faster than iterative)
vec3 calcNormal(vec3 pos){
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        map(pos + e.xyy) - map(pos - e.xyy),
        map(pos + e.yxy) - map(pos - e.yxy),
        map(pos + e.yyx) - map(pos - e.yyx)
    ));
}

// Forward declaration
vec4 renderSingleHead(vec2 st, float uTime, float uTempo, float uEnergy, float uBass, float uMid, float uHigh);

vec4 renderHead(vec2 st, float uTime, float uTempo, float uEnergy, float uBass, float uMid, float uHigh) {
    return renderSingleHead(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
}

vec4 renderSingleHead(vec2 st, float uTime, float uTempo, float uEnergy, float uBass, float uMid, float uHigh) {
    // Apply global camera zoom and offset
    st *= uCameraZoom;
    st += vec2(uCameraOffsetX, uCameraOffsetY);
    
    // Dynamic resolution scaling for performance
    float pixelCount = uResolution.x * uResolution.y;
    float resolutionScale = 1.0;
    if (pixelCount > 1920.0 * 1080.0) {
        resolutionScale = 0.5;
    } else if (pixelCount > 1280.0 * 720.0) {
        resolutionScale = 0.7;
    }
    st *= resolutionScale;
    
    // Center the head by moving camera back and adjusting view
    // Adjust camera distance based on zoom (closer zoom = move camera closer)
    float camZ = 3.5 / max(uCameraZoom, 0.1);  // Prevent division by zero
    vec3 camPos = vec3(0, 0.1, camZ);
    vec3 rayDirection = normalize(vec3(st.x, st.y - 0.1, -4));
    
    vec3 rayPosition = camPos;
    float rayLength = 0.;
    float dist = 0.;
    bool bg = false;
    vec3 col = vec3(.1);
    bool hitIsEye = false;

    // Optimized raymarching: reduced iterations with adaptive step
    for (int i = 0; i < 36; i++) {
        rayLength += dist;
        rayPosition = camPos + rayDirection * rayLength;
        vec2 mapResult = mapWithEye(rayPosition);
        dist = mapResult.x;

        float eps = .001 * max(uCameraZoom, 0.1);
        if (abs(dist) < eps) {
            hitIsEye = mapResult.y > 0.5;
        	break;
        }
        
        if (rayLength > 5. * camZ) {
            bg = true;
            break;
        }
    }
    
    if ( ! bg) {
        vec3 albedo = hitIsEye ? vec3(2) : vec3(1);
        vec3 n = calcNormal(rayPosition);
        vec3 lp = vec3(-.5,.5,.5);
        float l = max(dot(lp, n), 0.);
        vec3 ld = normalize(lp - rayPosition);
        l += .02;
        l += pow(max(0., 1. + dot(n, rayDirection)), 3.) * .05;
        
        // Add audio reactivity
        float audioMod = 1.0 + uBass * 0.5 + uMid * 0.3 + uHigh * 0.2;
        l *= audioMod;
        
        col = albedo * l;
        
        // Add color variation based on audio
        col.r *= 1.0 + uBass * 0.3;
        col.g *= 1.0 + uMid * 0.3;
        col.b *= 1.0 + uHigh * 0.3;
        
        col = pow(col, vec3(1./2.2));
    }

    return vec4(col, 1.0);
}

#line 1 23
// @EFFECT name="Power Particle" index=44 desc="Audio-reactive geometric pattern shader" author="System"
// Power Particle - audio-reactive geometric pattern shader
#define PI 3.14159265359
#define TWO_PI 6.28318530718

float impulse( float k, float x )
{
    float h = k*x;
    return h*exp(1.0-h);
}

float plot(float dis, float blur, float lineSize){
   float pct = smoothstep(dis,dis+blur,0.5)-smoothstep(lineSize+dis,lineSize+dis+blur,0.5);     
   return   pct ;
}

vec3 wooper(vec2 st, float timeCheck, float bass, float mid, float high, float energy){
    
    vec3 color = vec3(0.0);
    vec2 pos = vec2(0.5)-abs(st);

    float r = length(pos)*2.0;
    float a = atan(pos.y,pos.x);
    
    // Audio-reactive parameters
    float grid = 4.3651814 + high * 2.0;
    float grid2 = 4.1270218 + mid * 1.5;
    float morph = 2.30923 + bass * 1.0;
    float size = 0.544726 + energy * 0.2;
    float lineSize = 0.174972;
    float blur = 0.227794;
    
    float gridSine = 5.+ (grid2*sin(timeCheck/5. * PI));
    
    r = fract(impulse(r,gridSine)*grid);
    
    float morphSine = 0.2 + ( 1.+sin(timeCheck/3. * PI) /2.)*morph;
    float morphSine2 = 0.2 + ( 1.+sin(timeCheck/5. * PI) /2.)*morph;
    float morphSine3 = 0.2 + ( 1.+sin(timeCheck/9. * PI) /2.)*morph;
    
    float f = ( size*cos(a*6. + timeCheck/3.) + size*cos(a*2. + timeCheck/2.))/2.;
    float p = plot(1.-smoothstep(f,f+0.9,r*morphSine), blur, lineSize);
    float f2 = ( size*cos(a*4. + timeCheck/3.) + size*cos(a*3. + timeCheck*7.))/2.;
    float p2 = plot(1.-smoothstep(f2,f2+0.9,r*morphSine2), blur, lineSize);
    float f3 = ( size*cos(a*7. + timeCheck/30.) + size*cos(a*3. + timeCheck*7.))/2.;
    float p3 = plot(1.-smoothstep(f3,f3+0.9,r*morphSine3), blur, lineSize);
    
    color.r = p;
    color.g = p2 *st.x;
    color.b = p3;
 
   return(color);
}

vec3 powerParticle(vec2 st, float time, float bass, float mid, float high, float energy){
  
    st.y += ((st.x*0.05)*sin(time/10.*PI)+(st.x*0.1)*sin(time/12.*PI))/2.;
    st.x += ((st.y*0.05)*sin(time/10.*PI) + (st.y*0.1)*sin(time/12.*PI))/2.;
    
    vec2 pos = vec2(0.25+0.25*sin(time))-abs(st);

    float r = length(pos);
    float d = distance(st,vec2(0.5))* (sin(time/8.));
    d = distance(vec2(.5),st);
   vec3 colorNew = vec3(0);
   
   float delayAmount = 0.044175148;
   float speed = 0.466905 + energy * 0.3;
   float delay = delayAmount;
   float timerChecker = time * speed ;
    for(int i=0;i<10;i++) {
      vec3 colorCheck = wooper(st, timerChecker+ float(i)*delay, bass, mid, high, energy)* (1.-(float(i)/10.0));
      colorNew+= colorCheck ;
    }
    
    return(colorNew);
}

vec3 rgb2hsb( in vec3 c ){
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz),
                 vec4(c.gb, K.xy),
                 step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r),
                 vec4(c.r, p.yzx),
                 step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)),
                d / (q.x + e),
                q.x);
}


vec4 renderPowerParticle(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Apply raymarched object style centering (neutral coordinates)
    vec2 res = vec2(0);
    res.x = uResolution.x*0.5625;
    res.y = uResolution.y;
    vec2 coord = st * uResolution.xy / res;
    coord.x -= 0.35;
    
    vec3 powerColor = powerParticle(coord, time, bass, mid, high, energy);
    vec3 hue = rgb2hsb(powerColor);
    hue.x = mod(time/10.,1.);
    hue.y = 0.5;
    float d = 1.-distance(vec2(.5),coord)*2.;
   	
    vec3 finalColor = (hsb2rgb(hue)*d )+(powerColor*d*0.5);
    float alpha = clamp(0.7 + length(finalColor) * 0.3 + energy * 0.2, 0.0, 1.0);

    return vec4(finalColor, alpha);
}

#line 1 24
// @EFFECT name="Flopine" index=45 desc="Geometric scene with audio-reactive primitives" author="Flopine"
// Code by Flopine
// Thanks to wsmind, leon, XT95, lsdlive, lamogui, 
// Coyhot, Alkama,YX, NuSan, slerpy and wwrighter for teaching me
// Thanks LJ for giving me the spark :3
// Thanks to the Cookie Collective, which build a cozy and safe environment for me 
// and other to sprout :)  https://twitter.com/CookieDemoparty

#define FLOPINE_PI acos(-1.)
#define TAU 6.283581
#define ITER 80.

#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
#define crep(p,c,l) p=p-c*clamp(round(p/c),-l,l)

#define dt(sp,off) fract((uTime+off)*sp)
#define bouncy(sp,off) sqrt(sin(dt(sp,off)*PI))

struct obj
{
  float d;
  vec3 cs; 
  vec3 cl;
};

obj minobj (obj a, obj b)
{
  if (a.d<b.d) return a;
  else return b;
}

float stmin(float a, float b, float k, float n)
{
  float st = k/n;
  float u = b-k;
  return min(min(a,b),0.5*(u+a+abs(mod(u-a+st,2.*st)-st)));
}

void mo (inout vec2 p, vec2 d)
{
  p = abs(p)-d;
  if(p.y>p.x) p = p.yx;
}

float box (vec3 p, vec3 c)
{
  vec3 q = abs(p)-c;
  return min(0.,max(q.x,max(q.y,q.z)))+length(max(q,0.));
}

float flopine_sc (vec3 p, float d)
{
  p=abs(p);
  p=max(p,p.yzx);
  return min(p.x,min(p.y,p.z))-d;
}

obj prim1 (vec3 p, float bass, float mid, float high)
{
  p.x = abs(p.x)-3.;
  float per = 0.9;
  float id = round(p.y/per);
  p.xz *= rot(sin(dt(0.8,id*1.2)*TAU) + bass * 0.5);
  crep(p.y, per,4.);
  mo(p.xz,vec2(0.3));
  p.x += bouncy(2.,0.)*0.8;
  float pd = box(p,vec3(1.5,0.2,0.2));
  return obj(pd,vec3(0.5,0.,0.),vec3(1.,0.5,0.9));
}

obj prim2 (vec3 p, float bass, float mid, float high)
{
  p.y = abs(p.y)-6.;
  p.z = abs(p.z)-4.;
  mo(p.xz, vec2(1.));
  vec3 pp = p;
  mo(p.yz, vec2(0.5));
  p.y -= 0.5;
  float p2d = max(-flopine_sc(p,0.7),box(p,vec3(1.)));
  p = pp;
  p2d = min(p2d, max(box(p,vec3(bouncy(2.,0.)*4. + mid * 2.)),flopine_sc(p,0.2)));
  return obj(p2d, vec3(0.2),vec3(1.));
}

obj prim3 (vec3 p, float bass, float mid, float high)
{
  p.z = abs(p.z)-9.;
  float per = 0.8;
  vec2 id = round(p.xy/per)-.5;
  float height = 1.*bouncy(2.,sin(length(id*0.05))) + high * 0.5;
  float p3d = box(p,vec3(2.,2.,0.2));
  crep(p.xy,per,2.);
  p3d = stmin(p3d,box(p+vec3(0.,0.,height*0.9),vec3(0.15,.15,height)),0.2,3.);
  return obj (p3d, vec3(0.1,0.7,0.),vec3(1.,0.9,0.));
}

obj prim4 (vec3 p, float bass, float mid, float high)
{
  p.y = abs(p.y)-5.;
  mo(p.xz, vec2(1.));
  float scale = 1.5;
  p *= scale;
  float per = 2.*(bouncy(0.5,0.) + bass * 0.3);
  crep(p.xz,per,2.);
  float p4d = max(box(p,vec3(0.9)),flopine_sc(p,0.25));
  return obj (p4d/scale, vec3(0.1,0.2,0.4),vec3(0.1,0.8,0.9));
}

float squared (vec3 p,float s)
{
  mo(p.zy,vec2(s));
  return box(p,vec3(0.2,10.,0.2));
}

obj prim5 (vec3 p, float bass, float mid, float high)
{
  p.x = abs(p.x)-8.;
  float id = round(p.z/7.);
  crep(p.z,7.,2.);
  float scarce = 3.;
  float p5d=1e10;
  for(int i=0;i<4; i++)
  {
    p.x += bouncy(1.,id*0.9)*0.6 + mid * 0.4;
    p5d = min(p5d,squared(p,scarce));
    p.yz *= rot(PI/4.);
    scarce -= 1.;    
  }
  return obj(p5d,vec3(0.5,0.2,0.1),vec3(1.,0.9,0.1));
}

obj SDF (vec3 p, float bass, float mid, float high)
{
  p.yz *= rot(-atan(1./sqrt(2.)));
  p.xz *= rot(PI/4.);

  obj scene = prim1(p, bass, mid, high);
  scene = minobj(scene,prim2(p, bass, mid, high));
  scene = minobj(scene,prim3(p, bass, mid, high));
  scene = minobj(scene,prim4(p, bass, mid, high));
  scene = minobj(scene, prim5(p, bass, mid, high));
  return scene;
}

vec3 getnorm (vec3 p, float bass, float mid, float high)
{
  vec2 eps = vec2(0.001,0.);
  return normalize(SDF(p, bass, mid, high).d-vec3(SDF(p-eps.xyy, bass, mid, high).d,SDF(p-eps.yxy, bass, mid, high).d,SDF(p-eps.yyx, bass, mid, high).d));
}

vec4 renderFlopine(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Apply raymarched object style centering (neutral coordinates)
    vec2 uv = (st * 2.0 - 1.0);
    uv.x *= uResolution.x / max(uResolution.y, 1.0);

    // Camera centered in the scene looking outward
    vec3 ro = vec3(0.0, 0.0, 0.0);
    
    // Look direction based on UV with slight rotation over time
    float rotTime = uTime * 0.2;
    float cx = cos(rotTime);
    float sx = sin(rotTime);
    
    // Rotate UV coordinates for camera rotation
    vec2 ruv;
    ruv.x = uv.x * cx - uv.y * sx;
    ruv.y = uv.x * sx + uv.y * cx;
    
    // Ray direction outward from center
    float fov = 1.0;
    vec3 rd = normalize(vec3(ruv * fov, 1.0));
    
    // Slight camera movement based on bass
    ro += vec3(sin(uTime * 0.5) * bass * 0.5, 
               cos(uTime * 0.3) * bass * 0.3, 
               sin(uTime * 0.4) * bass * 0.2);
    
    vec3 p = ro;
    
    // Use black background instead of white
    vec3 col = vec3(0.0);
    vec3 l = normalize(vec3(1., 1.4, -2.));

    obj O; 
    bool hit = false;

    for (float i = 0.; i < ITER; i++)
    {
        O = SDF(p, bass, mid, high);
        if (O.d < 0.001)
        {
            hit = true; 
            break;
        }
        p += O.d * rd;
    }

    if (hit)
    {
        vec3 n = getnorm(p, bass, mid, high);
        float light = max(dot(n, l), 0.);
        col = mix(O.cs, O.cl, light);
    }
    
    float alpha = clamp(0.7 + length(col) * 0.3 + energy * 0.2, 0.0, 1.0);
    return vec4(sqrt(col), alpha);
}

#line 1 25
// @EFFECT name="Eiyeron Deform" index=46 desc="DEMOS AND COLORS plane deformation by Eiyeron" author="Eiyeron" zoom=1.2

/**
DEMOS AND COLORS
By @Eiyeron
    Based on Illogical from Matrefeytontias, plane deformations on TI-83/84
    And some tunnel effects.

Use : Comment/Uncomment the defines as you wants, they'll enable/disable various effects in the shader.
**/

const float EIY_SPEED = 0.25;
const float EIY_PI = 3.141592653589793;
const float EIY_TAU = EIY_PI * 2.0;

vec3 eiyeronRainbow(vec2 p) {
    vec2 shifted = fract(abs(p) * 0.16666);
    shifted = min(shifted, 1.0 - shifted);
    float r = smoothstep(0.166666, 0.333333, shifted.x + 0.05);
    float g = smoothstep(0.166666, 0.333333, shifted.x + 0.25);
    float b = smoothstep(0.166666, 0.333333, shifted.x + 0.45);
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

float eiyeronChecker(vec2 p, float t) {
    float xpos = floor(20.0 * p.x);
    float ypos = floor(10.0 * p.y);
    float col = mod(xpos, 2.0);
    float phase = xpos * ypos + t * 5.0;
    return (mod(ypos, 2.0) > 0.0) ? cos(phase) : sin(phase);
}

vec2 eiyeronNormalize(vec2 fragCoord) {
    vec2 position = 2.0 * fragCoord / uResolution - 1.0;
    position.x *= uResolution.x / uResolution.y;
    return position;
}

vec3 eiyeronCore(vec2 fragCoord, float t, float energy, float bass, float mid, float high) {
    vec2 position = eiyeronNormalize(fragCoord);

    float r = length(position);
    float angle = atan(position.y, position.x);
    float factor = sin(t) * 0.5 + 0.5;

    float uPlane = position.x / max(abs(position.y), 0.001);
    float vPlane = 1.0 / max(abs(position.y), 0.001);
    float uTunnel = angle;
    float vTunnel = 1.0 / max(r, 0.001);

    float u = mix(uPlane, uTunnel, 1.0 - factor);
    float v = mix(vPlane, vTunnel, 1.0 - factor);

    vec2 p = vec2(u, v);
    p += vec2(EIY_SPEED * cos(t), EIY_SPEED * t);
    p += vec2(0.08 * bass, 0.05 * high);

    vec3 baseColor = vec3(cos(p.x), sin(p.y), 1.0 - 0.5 * cos(t));
    vec3 rainbow = eiyeronRainbow(p);
    float checker = eiyeronChecker(p, t);

    vec3 color = baseColor * rainbow * checker;

    float motif = 0.0;
    for (float i = 0.0; i < 5.0; i += 1.0) {
        float ang = i * (EIY_TAU / 5.0) * 61.95;
        motif += cos(EIY_TAU * (p.y * cos(ang) + p.x * sin(ang) + sin(t * 0.004) * 100.0));
    }
    color *= vec3(motif / 3.0);

    color *= clamp(1.0 / (abs(v) + 0.1), 0.0, 4.0);
    color *= clamp(2.0 - r, 0.0, 2.0);

    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + high * 0.2, 0.0, 1.0));
    color *= palette;

    vec3 bandGlow = vec3(bass * 0.2, mid * 0.15, high * 0.25);
    color = color * (0.8 + energy * 0.7 + high * 0.4) + bandGlow;

    return color;
}

vec4 renderEiyeronDeform(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 fragCoord = (st * 0.5 + 0.5) * uResolution;
    float t = time * (1.0 + tempo * 0.05) + bass * 0.2;

    vec3 color = eiyeronCore(fragCoord, t, energy, bass, mid, high);
    color = clamp(color, 0.0, 1.0);
    return vec4(color, 1.0);
}

#line 1 26
// @EFFECT name="Fractal Rotation Field" index=47 desc="Fractal rotation field with accumulated transforms" author="Shadertoy" zoom=0.8

mat2 rotate2D_fractal(float t) {
    return mat2(cos(t), sin(t), -sin(t), cos(t));
}

vec4 renderFractalRotation(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 r = uResolution.xy;
    vec2 n = vec2(0.0), N = vec2(0.0), q;
    vec2 p = st * 2.0;
    vec4 o = vec4(0.0);
    float S = 5.0, a = 0.0, j = 0.0;
    float t = time;
    
    mat2 m = rotate2D_fractal(5.0);
    for(; j < 30.0; j++, S *= 1.2) {
        p *= m;
        n *= m;
        q = p * S + j + n + t * 4.0 + sin(t * 4.0) * 0.8;
        a += dot(cos(q) / S, vec2(1.0));
        q = sin(q);
        n += q;
        N += q / (S + 20.0);
    }
    
    o += 0.1 - a * 0.1;
    o.r *= 5.0 + bass * 2.0;
    o += min(0.7, 0.001 / length(N + 0.0001));
    o -= o * dot(p, p) * 0.7;
    
    vec3 paletteMix = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + mid * 0.2, 0.0, 1.0));
    float paletteInfluence = clamp(0.45 + uColorBlend * 0.4 + high * 0.25, 0.0, 1.0);
    o.rgb = mix(o.rgb, paletteMix, paletteInfluence);
    
    float intensity = 1.0 + energy * 0.5;
    o.rgb *= intensity;
    
    o = clamp(o, 0.0, 1.0);
    return o;
}

#line 1 27
// @EFFECT name="Kaleidoscopic Flow" index=48 desc="Fractal kaleidoscopic pattern with flowing colors" author="ShaderToy"

vec4 renderKaleidoscopicFlow(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 p = st * 4.0;
    float t = time * 0.4 + length(p + vec2(sin(time), cos(time)) * 4.0) * 0.1;
    
    vec3 c = vec3(0.0);
    
    c += uPrimaryColor * fract((p.x + p.y + fract(t * 0.5)) * 5.0);
    c *= uSecondaryColor * 2.0 * fract((sin(t * 6.0) * p.x - p.y + fract(t * 0.05)) * 5.0);
    c *= (p.x * p.y);
    
    // Audio reactivity
    c *= (0.5 + energy * 0.5);
    c += uSecondaryColor * bass * 0.3;
    
    c = clamp(c, 0.0, 1.0);
    float alpha = clamp(0.4 + energy * 0.4, 0.0, 1.0);
    
    return vec4(c, alpha);
}

#line 1 28
// @EFFECT name="Reactive Twist Field" index=49 desc="Reactive twist vortex adapted from a mouse-driven spiral test" author="ShaderToy"

vec4 renderReactiveTwistField(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 v = st * (12.0 + high * 6.0);
    float t = time * 4.0;

    float factor = clamp(0.18 + bass * 0.75 + energy * 0.2, 0.05, 1.6);
    float rotation = (mid * 2.0 - 1.0) * 3.14159265 * 0.95 + high * 1.1 + tempo * 0.15;
    float c = cos(rotation);
    float s = sin(rotation);

    for (int i = 1; i <= 40; ++i) {
        float d = float(i + 3) / 40.0;
        float x = v.x;
        float y = v.y + sin(v.x * d * 7.0 + t + bass * 1.5) / d * factor
                      + cos(v.x * d + t + energy) / d * factor;

        v.x = x * c - y * s;
        v.y = x * s + y * c;
        v *= 0.985 + high * 0.002;
    }

    float col = length(v) * 0.25;
    vec3 rgb = 0.5 + 0.5 * vec3(
        cos(col),
        cos(col * 2.0 + time * 0.2),
        cos(col * 4.0 + bass)
    );

    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(0.5 + 0.5 * sin(col * 2.5 + mid * 3.0), 0.0, 1.0));
    rgb *= palette;
    rgb *= 0.55 + energy * 0.65;
    rgb += uSecondaryColor * factor * 0.15;
    rgb = clamp(rgb, 0.0, 1.0);

    return vec4(rgb, 1.0);
}

#line 1 29
// @EFFECT name="Collapsed Transit Grid" index=50 desc="Dense capsule tunnels with glow" author="ShaderToy"

#define ITE_MAX 100
#define DIST_COEFF 0.66
#define DIST_MIN 0.01
#define DIST_MAX 10000.0
#define INF 100000.0
#define UNIT_WINDOW_SIZE 50.0

float rand1(float n) { return fract(sin(n) * 43758.5453123); }
float rand2(vec2 n) {
    return fract(sin(dot(n, vec2(12.9898, 22.1414))) * 43758.5453);
}

float noise1(float p) {
    float fl = floor(p);
    float fc = fract(p);
    return mix(rand1(fl), rand1(fl + 1.0), fc);
}

float noise2(vec2 n) {
    const vec2 d = vec2(0.5, 1.0);
    vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
    return mix(mix(rand2(b), rand2(b + d.yx), f.x),
               mix(rand2(b + d.xy), rand2(b + d.yy), f.x), f.y);
}

mat3 rotM(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    return mat3(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s,
                oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s,
                oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c);
}

vec3 GenRay(vec3 dir, vec3 up, float angle, vec2 fragCoord) {
    vec2 p = (fragCoord * 2.0 - uResolution) / min(uResolution.x, uResolution.y);
    vec3 u = normalize(cross(up, dir));
    vec3 v = normalize(cross(dir, u));
    float fov = angle * PI * 0.5 / 180.0;
    return normalize(sin(fov) * u * p.x + sin(fov) * v * p.y + cos(fov) * dir);
}

float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float sdCross(vec3 p) {
    float da = sdBox(p, vec3(9100.0, 1.0, 1.0));
    float db = sdBox(p, vec3(1.0, 100.0, 1.0));
    float dc = sdBox(p, vec3(1.0, 1.0, 100.0));
    return min(da, min(db, dc));
}

float Cs(vec3 p) {
    vec3 c = vec3(4.0);
    p -= 0.5 * c;
    p = mod(p, c) - 0.5 * c;
    return sdCross(p);
}

float men(vec3 p, float d) {
    float s = 1.0 / 3.0;
    float ratio = 1.0 / (3.0 + 2.0);
    for (int i = 0; i < 3; ++i) {
        vec3 r = p / s;
        d = max(d, -Cs(r) * s);
        s *= ratio;
    }
    return d;
}

float sdCylinder(vec3 p, vec3 c) {
    return length(p.xz - c.xy) - c.z;
}

float tunnel(vec3 p) {
    float d = 999.0;
    d = sdCylinder(p, vec3(0.6, 0.0, 4.0));
    d = max(d, -sdCylinder(p, vec3(0.0, 0.0, 3.9)));
    return d;
}

float collapsed(vec3 p) {
    float d = 9999.0;
    d = length(p) - 2.4;
    d = min(tunnel(p - vec3(0.0, 0.0, sin(p.y * 0.2))), d);
    d = max(d, -Cs(p * rotM(vec3(0.0, 1.0, 0.0), p.y * 0.4)));
    return d;
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float glowAccum;

float mapCollapsed(vec3 p) {
    float d = sdBox(p, vec3(1.0));
    vec3 q = p;
    vec3 cell = vec3(5.0, 5.0, 3.0);
    vec2 index = floor(p.xy / cell.xy);
    q.xy = mod(q.xy, cell.xy) - 0.5 * cell.xy;
    d = min(d, length(q) - 0.1);

    float h = noise2(index);
    vec2 up = vec2(index.x, index.y + 1.0);
    vec2 down = vec2(index.x, index.y - 1.0);
    vec2 right = vec2(index.x + 1.0, index.y);
    vec2 left = vec2(index.x - 1.0, index.y);

    float dc = tan(uTime * rand2(index) * 4.0 - rand2(index) * 0.4);
    float dup = sin(uTime * rand2(up) * 4.0 - rand2(up) * 0.4);
    float ddown = sin(uTime * rand2(down) * 4.0 - rand2(down) * 0.4);
    float dright = sin(uTime * rand2(right) * 4.0 - rand2(right) * 0.4);
    float dleft = sin(uTime * rand2(left) * 4.0 - rand2(left) * 0.4);

    d = min(d, sdCapsule(q, vec3(0.0, 0.0, h + dc), vec3(cell.x, 0.0, noise2(right) + dright), 0.01));
    d = min(d, sdCapsule(q, vec3(2.0, 0.0, h + dc), vec3(-cell.x, 0.0, noise2(left) + dleft), 0.01));
    d = min(d, sdCapsule(q, vec3(0.0, 0.0, h + dc), vec3(0.0, cell.y, noise2(up) + dup), 0.01));
    d = min(d, sdCapsule(q, vec3(0.0, 0.0, h + dc), vec3(0.0, -cell.y, noise2(down) + ddown), 0.01));

    glowAccum = min(glowAccum, d);
    return d;
}

vec3 getNormalCollapsed(vec3 p) {
    float eps = 0.001;
    return normalize(vec3(
        mapCollapsed(p + vec3(eps, 0.0, 0.0)) - mapCollapsed(p - vec3(eps, 0.0, 0.0)),
        mapCollapsed(p + vec3(0.0, eps, 0.0)) - mapCollapsed(p - vec3(0.0, eps, 0.0)),
        mapCollapsed(p + vec3(0.0, 0.0, eps)) - mapCollapsed(p - vec3(0.0, 0.0, eps))
    ));
}

vec4 renderCollapsedTransit(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 fragCoord = (st * 0.5 + 0.5) * uResolution;
    vec3 lightDir = normalize(vec3(0.2, -0.99, 0.99));
    vec3 pos = vec3(3.0 * cos(time), 3.0 * sin(time), 4.0);
    vec3 target = vec3(0.5, 0.0, 0.0);
    vec3 dir = GenRay(normalize(target - pos), vec3(0.0, 0.0, 1.0), 120.0, fragCoord);

    glowAccum = 99990.0;
    float t = 0.0;
    float dist = 0.0;
    int iterations = 0;

    for (int i = 0; i < ITE_MAX; ++i) {
        dist = mapCollapsed(pos + dir * t);
        if (dist < DIST_MIN) { break; }
        t += dist * DIST_COEFF;
        if (t > DIST_MAX) { break; }
        iterations = i;
    }

    vec3 ip = pos + dir * t;
    vec3 color = vec3(0.0);
    if (dist < DIST_MIN) {
        vec3 n = getNormalCollapsed(ip);
        float diff = clamp(dot(lightDir, n), 0.1, 1.0);
        color = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + mid * 0.2, 0.0, 1.0)) * diff;
        color *= 0.8 + energy * 0.4;
    } else {
        color += 0.01 / (glowAccum + 0.0001);
    }

    float depthFog = clamp(1.0 - t * 0.04, 0.0, 1.0);
    color = mix(vec3(0.05, 0.08, 0.1), color, depthFog);
    color = clamp(color, 0.0, 1.0);
    return vec4(color, 1.0);
}

#line 1 30
// @EFFECT name="Celestial Ribbon Bloom" index=51 desc="Layered spherical ribbons with volumetric glow" author="tigrou"

vec4 tigrouPortalLayer(vec2 px, float depth, float timeSign) {
    float l = PI;
    float k = timeSign * sign(depth);
    float x = px.x * 320.0 * 0.0065 * depth;
    float y = px.y * 240.0 * 0.0060 * depth;
    float c = sqrt(x * x + y * y);
    if (c > 1.0) {
        return vec4(0.0);
    }

    float u = -0.4 * sign(depth) + sin(k * 0.5);
    float v = sqrt(max(1.0 - x * x - y * y, 0.0));
    float q = y * sin(u) - v * cos(u);
    y = y * cos(u) + v * sin(u);
    v = acos(clamp(y, -1.0, 1.0));
    float sv = sin(v);
    float invSv = (abs(sv) < 1e-4) ? 0.0 : (x / sv);
    invSv = clamp(invSv, -1.0, 1.0);
    u = acos(invSv) / (2.0 * l) * 120.0 * sign(q) - k;
    v = v * 60.0 / l;
    q = cos(floor(v / l));
    float wave = float(int((u + l / 2.0) / l));
    c = pow(abs(cos(u) * sin(v)), 0.2) * 0.1 /
        (q + sin(wave + k * 0.6 + cos(q * 25.0))) * pow(1.0 - c, 0.9);

    vec4 res;
    if (c < 0.0) {
        res = vec4(-c / 2.0 * abs(cos(k * 0.1)), 0.0, 0.0, 1.0);
    } else {
        res = vec4(c, c * 2.0, c * 2.0, 1.0);
    }
    return res;
}

vec4 renderCelestialRibbonBloom(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 fragCoord = (st * 0.5 + 0.5) * uResolution;
    vec2 p = -1.0 + 2.0 * fragCoord / uResolution;

    vec4 accum = vec4(0.0);
    for (int i = 8; i > 0; --i) {
        float depth = 1.0 - float(i) / 80.0;
        accum += tigrouPortalLayer(p, depth, time) * (0.008 - float(i) * 0.00005);
    }

    vec4 detail = tigrouPortalLayer(p, 1.0, time);
    vec4 layer = detail + sqrt(max(accum, 0.0));

    // Soft back layer hint from original shader
    if (detail.a == 77.0) {
        layer += tigrouPortalLayer(p, -0.2, time) * 0.02;
    }

    vec3 palette = mix(uPrimaryColor, uSecondaryColor,
                       clamp(0.45 + 0.35 * sin(mid * 2.0 + time * 0.5), 0.0, 1.0));
    vec3 ribbon = layer.rgb * palette;
    ribbon *= 0.6 + energy * 0.5;
    ribbon += uSecondaryColor * (bass * 0.15 + high * 0.1);
    ribbon = clamp(ribbon, 0.0, 1.0);

    float glowMask = clamp(length(layer.rgb) * 0.9, 0.0, 1.0);
    vec3 background = vec3(0.015, 0.02, 0.04);
    vec3 color = mix(background, ribbon, glowMask);

    return vec4(color, 1.0);
}

#line 1 31
// @EFFECT name="Mandelbulb Flux Bloom" index=52 desc="Mandelbulb raymarch matching tigrou's original look" author="tigrou"

const int kMandelbulbMaxIter = 15;
const float kMandelbulbBailout = 6.0;
const float kMandelbulbPower = 20.0;

float mandelbulbDistance(vec3 p) {
    vec3 z = p;
    vec3 c = z;
    float r = 0.0;
    float dr = 1.2;
    for (int i = 0; i <= kMandelbulbMaxIter; ++i) {
        r = length(z);
        if (r > kMandelbulbBailout) {
            break;
        }

        float theta = acos(clamp(z.z / max(r, 1e-5), -1.0, 1.0));
        float phi = atan(z.y, z.x);
        dr = pow(r, kMandelbulbPower - 1.0) * kMandelbulbPower * dr + 1.0;

        float zr = pow(r, kMandelbulbPower);
        theta *= kMandelbulbPower;
        phi *= kMandelbulbPower;

        vec3 trig = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
        z = trig * zr + c;
    }
    return 0.3 * log(r) * r / max(dr, 1e-5);
}

vec4 renderMandelbulbFlux(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 fragCoord = (st * 0.5 + 0.5) * uResolution;
    vec2 pos = (fragCoord * 2.0 - uResolution) / uResolution.y;

    vec3 camPos = vec3(cos(time * 0.3), sin(time * 0.3), 1.5);
    vec3 camTarget = vec3(0.0);
    vec3 camDir = normalize(camTarget - camPos);
    vec3 camUp = normalize(vec3(0.0, 1.0, 0.0));
    vec3 camSide = normalize(cross(camDir, camUp));
    camUp = normalize(cross(camSide, camDir));
    float focus = 1.8;

    vec3 rayDir = normalize(camSide * pos.x + camUp * pos.y + camDir * focus);
    vec3 ray = camPos;
    float accumSteps = 0.0;
    float d = 0.0;
    float totalDist = 0.0;
    const int MAX_MARCH = 50;
    const float MAX_DISTANCE = 1000.0;

    for (int i = 0; i < MAX_MARCH; ++i) {
        d = mandelbulbDistance(ray);
        totalDist += d;
        ray += rayDir * d;
        accumSteps += 1.4;
        if (d < 0.001) {
            break;
        }
        if (totalDist > MAX_DISTANCE) {
            totalDist = MAX_DISTANCE;
            break;
        }
    }

    float c = totalDist * 0.1;
    vec3 baseColor = 1.0 - vec3(c * 0.003, c * 0.5, c * 0.2)
                     - vec3(0.025, 0.019, 0.02) * accumSteps * 0.8;
    baseColor = clamp(baseColor, 0.0, 1.0);

    // Blend gently with the project palette so UI colors still affect the effect.
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend, 0.0, 1.0));
    vec3 color = mix(baseColor, baseColor * palette, 0.35 + high * 0.1);

    return vec4(color, 1.0);
}

#line 1 32
// @EFFECT name="Iridescent Eye" index=53 desc="Tiled iris eye inspired by stb" author="stb"

float hash11_eye(float p) {
    const vec3 MOD3 = vec3(443.8975, 397.2973, 491.1871);
    vec3 p3 = fract(vec3(p) * MOD3);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

float smoothHash_eye(float a) {
    float i = floor(a);
    float f = fract(a);
    return mix(hash11_eye(i), hash11_eye(i + 1.0), f * f * (3.0 - 2.0 * f));
}

vec4 permute_eye(vec4 x) { return mod(((x * 34.0) + 1.0) * x, 289.0); }
vec2 fade_eye(vec2 t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

float cnoise_eye(vec2 P, float rep) {
    P.x = mod(P.x, rep);
    vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    Pi.z = mod(Pi.z, rep);
    vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod(Pi, 289.0);
    vec4 ix = Pi.xzxz;
    vec4 iy = Pi.yyww;
    vec4 fx = Pf.xzxz;
    vec4 fy = Pf.yyww;
    vec4 i = permute_eye(permute_eye(ix) + iy);
    vec4 gx = 2.0 * fract(i * 0.0243902439) - 1.0;
    vec4 gy = abs(gx) - 0.5;
    vec4 tx = floor(gx + 0.5);
    gx = gx - tx;
    vec2 g00 = vec2(gx.x, gy.x);
    vec2 g10 = vec2(gx.y, gy.y);
    vec2 g01 = vec2(gx.z, gy.z);
    vec2 g11 = vec2(gx.w, gy.w);
    vec4 norm = 1.79284291400159 - 0.85373472095314 *
        vec4(dot(g00, g00), dot(g01, g01), dot(g10, g10), dot(g11, g11));
    g00 *= norm.x;
    g01 *= norm.y;
    g10 *= norm.z;
    g11 *= norm.w;
    float n00 = dot(g00, vec2(fx.x, fy.x));
    float n10 = dot(g10, vec2(fx.y, fy.y));
    float n01 = dot(g01, vec2(fx.z, fy.z));
    float n11 = dot(g11, vec2(fx.w, fy.w));
    vec2 fade_xy = fade_eye(Pf.xy);
    vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
    float n_xy = mix(n_x.x, n_x.y, fade_xy.y);
    return 2.3 * n_xy;
}

float renderEyePattern(vec2 p, float pupil, vec2 lpos) {
    vec2 original = p;
    p /= 1.0 - 0.1 * dot(p, p);
    p -= lpos;

    float adjustedPupil = pupil + 0.3 * (1.0 - length(lpos)) - 0.1;
    vec2 pr = vec2(atan(p.x, p.y) / PI / 2.0,
                   clamp((length(p) - 1.0) / adjustedPupil + 0.8, 0.0, 1.0));
    pr.y = smoothstep(0.0, 1.0, pr.y);

    vec2 freq = vec2(30.0, 1.5);
    float f = pow((cnoise_eye(pr * freq, freq.x) + 1.0) / 4.0, 0.65);
    f -= pow((cnoise_eye(pr * freq * vec2(2.0, 3.0) + 9.0, 2.0 * freq.x) + 1.0) / 2.0 - 0.5, 2.0);

    float shade = dot(p, lpos);
    f += 0.7 * shade;
    f *= pow(smoothstep(0.0, 0.5, pr.y), 0.15);
    f = mix(f, 0.25, smoothstep(0.5, 1.0, pr.y + 0.2));
    f = mix(f, 1.0 - 0.1 * dot(p, p) + 0.25 * shade, smoothstep(0.7, 0.85, pr.y));
    f = mix(1.0, f, clamp((length(p + lpos * 1.33) - 0.15) / 0.025, 0.0, 1.0));
    f = mix(f, 0.0, clamp((length(vec2(original.x, abs(original.y)) + vec2(0.0, 1.2)) - 2.0) / 0.04, 0.0, 1.0));

    return f;
}

vec4 renderIridescentEye(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 fragCoord = (st * 0.5 + 0.5) * uResolution;
    vec2 p = 7.0 * (fragCoord - uResolution * 0.5) / uResolution.y;

    float pupil = 0.7 + 0.2 * smoothHash_eye(1.5 * time);
    p /= (dot(p, p) / 12.0);
    p.x += time * 0.5;
    p = mod(p + vec2(floor(p.y / 2.0) * 2.0, 0.0), vec2(4.0, 2.0)) - vec2(2.0, 1.0);

    float lookSwing = sin(time * 0.4 + high * 1.6) * (0.25 + high * 0.15);
    float lookTilt = cos(time * 0.3 + mid * 1.2) * (0.2 + mid * 0.2);
    float focus = clamp(0.75 + energy * 0.3 + bass * 0.2, 0.6, 1.2);
    vec2 lightPos = vec2(1.4 * lookSwing, 0.7 * lookTilt) * focus;
    float eyeValue = renderEyePattern(p, pupil, lightPos);

    vec3 irisColor = vec3(1.0, 1.3, 1.7) * eyeValue;
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + mid * 0.1, 0.0, 1.0));
    vec3 color = mix(irisColor, irisColor * palette, 0.4 + high * 0.1);
    color *= 0.8 + energy * 0.4;

    return vec4(clamp(color, 0.0, 1.0), 1.0);
}

#line 1 33
// @EFFECT name="Voronoi Gate Stream" index=54 desc="Layered gates and flowing light stream" author="Shadertoy"

vec2 rotate2D_gate(vec2 p, float a) {
    float c = cos(a);
    float s = sin(a);
    return vec2(p.x * c - p.y * s, p.x * s + p.y * c);
}

float box_gate(vec2 p, vec2 b, float r) {
    return length(max(abs(p) - b, 0.0)) - r;
}

vec3 intersectPlane(vec3 ro, vec3 rd, vec3 c, vec3 u, vec3 v) {
    vec3 q = ro - c;
    vec3 cu = cross(u, v);
    float denom = dot(cross(v, u), rd);
    return vec3(
        dot(cu, q),
        dot(cross(q, u), rd),
        dot(cross(v, q), rd)
    ) / denom;
}

float rand11_gate(float p) {
    return fract(sin(p * 591.32) * 43758.5357);
}

float rand12_gate(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5357);
}

vec2 rand21_gate(float p) {
    return fract(vec2(sin(p * 591.32), cos(p * 391.32)));
}

vec2 rand22_gate(vec2 p) {
    return fract(vec2(
        sin(p.x * 591.32 + p.y * 154.077),
        cos(p.x * 391.32 + p.y * 49.077)
    ));
}

float noise11_gate(float p) {
    float fl = floor(p);
    return mix(rand11_gate(fl), rand11_gate(fl + 1.0), fract(p));
}

vec3 noise31_gate(float p) {
    return vec3(
        noise11_gate(p),
        noise11_gate(p + 18.952),
        noise11_gate(p - 11.372)
    ) * 2.0 - 1.0;
}

vec3 voronoi_gate(vec2 x) {
    vec2 n = floor(x);
    vec2 f = fract(x);
    vec2 mg = vec2(0.0);
    float md = 8.0;
    float md2 = 8.0;
    vec2 mr = vec2(0.0);
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = rand22_gate(n + g);
            vec2 r = g + o - f;
            float d = max(abs(r.x), abs(r.y));
            if (d < md) {
                md2 = md;
                md = d;
                mr = r;
                mg = g;
            } else if (d < md2) {
                md2 = d;
            }
        }
    }
    return vec3(n + mg, md2 - md);
}

#define A2V_GATE(a) vec2(sin((a) * 6.28318531 / 100.0), cos((a) * 6.28318531 / 100.0))

float circles_gate(vec2 p, float timeFactor) {
    float l = length(p);
    vec2 pp = rotate2D_gate(p, timeFactor * 3.0);
    float c = max(dot(pp, normalize(vec2(-0.2, 0.5))), -dot(pp, normalize(vec2(0.2, 0.5))));
    c = min(c, max(dot(pp, normalize(vec2(0.5, -0.5))), -dot(pp, normalize(vec2(0.2, -0.5)))));
    c = min(c, max(dot(pp, normalize(vec2(0.3, 0.5))), -dot(pp, normalize(vec2(0.2, 0.5)))));

    float v = abs(l - 0.5) - 0.03;
    v = max(v, -c);
    v = min(v, abs(l - 0.54) - 0.02);
    v = min(v, abs(l - 0.64) - 0.05);

    pp = rotate2D_gate(p, timeFactor * -1.333);
    c = max(dot(pp, A2V_GATE(-5.0)), -dot(pp, A2V_GATE(5.0)));
    c = min(c, max(dot(pp, A2V_GATE(20.0)), -dot(pp, A2V_GATE(30.0))));
    c = min(c, max(dot(pp, A2V_GATE(45.0)), -dot(pp, A2V_GATE(55.0))));
    c = min(c, max(dot(pp, A2V_GATE(70.0)), -dot(pp, A2V_GATE(80.0))));

    float w = abs(l - 0.83) - 0.09;
    v = min(v, max(w, c));
    return v;
}

float shade_gate(float d) {
    float v = 1.0 - smoothstep(0.0, 0.12, d);
    float g = exp(d * -20.0);
    return v + g * 0.5;
}

vec4 renderVoronoiGateStream(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 fragCoord = (st * 0.5 + 0.5) * uResolution;
    vec2 uv = fragCoord / uResolution;
    uv = uv * 2.0 - 1.0;
    uv.x *= uResolution.x / uResolution.y;

    float t = time;
    vec3 ro = 0.7 * vec3(cos(0.2), 0.0, sin(0.2));
    ro.y = cos(0.6) * 0.3 + 0.65;
    vec3 ta = vec3(0.0, 0.2, 0.0);

    float shake = 0.0;
    float stime = 0.0;
    vec3 ww = normalize(ta - ro + noise31_gate(stime) * shake * 0.01);
    vec3 uu = normalize(cross(ww, normalize(vec3(0.0, 1.0, 0.2))));
    vec3 vv = normalize(cross(uu, ww));
    vec3 rd = normalize(uv.x * uu + uv.y * vv + 1.0 * ww);

    ro += noise31_gate(-stime) * shake * 0.015;
    ro.x += t * -10.0;

    float intensity = 0.0;
    vec3 its;
    float v;

    // Voronoi floors
    for (int i = 0; i < 4; ++i) {
        float layer = float(i);
        its = intersectPlane(ro, rd, vec3(0.0, -5.0 - layer * 5.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0));
        if (its.x > 0.0) {
            vec3 vo = voronoi_gate((its.yz) * 0.05 + 8.0 * rand21_gate(float(i)));
            v = exp(-100.0 * (vo.z - 0.02));
            float fx = 0.0;
            if (i == 3) {
                float fxi = cos(vo.x * 0.2 + t * 1.5);
                fx = clamp(smoothstep(0.9, 1.0, fxi), 0.0, 0.9) * rand12_gate(vo.xy);
                fx *= exp(-3.0 * vo.z) * 2.0;
            }
            intensity += v * 0.1 + fx;
            intensity *= 64.0 / its.x;
        }
    }

    // Gates
    float gatex = floor(ro.x / 8.0 + 0.5) * 8.0 + 4.0;
    float go = -32.0;
    for (int i = 0; i < 4; ++i) {
        its = intersectPlane(ro, rd, vec3(gatex + go, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));
        if (dot(its.yz, its.yz) < 2.0 && its.x > 0.0) {
            v = circles_gate(its.yz, t);
            intensity += shade_gate(v) * 0.25;
        }
        go += 8.0;
    }

    // Stream particles
    for (int j = 0; j < 20; ++j) {
        float id = float(j);
        vec3 bp = vec3(0.0, (rand11_gate(id) * 2.0 - 1.0) * 0.25, 0.0);
        vec3 itp = intersectPlane(ro, rd, bp, vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0));
        if (itp.x > 0.0) {
            vec2 pp = itp.yz;
            float spd = (1.0 + rand11_gate(id) * 3.0) * -2.5;
            pp.y += t * spd;
            pp += (rand21_gate(id) * 2.0 - 1.0) * vec2(0.3, 1.0);
            float rep = rand11_gate(id) + 1.5;
            pp.y = mod(pp.y, rep * 2.0) - rep;
            float d = box_gate(pp, vec2(0.02, 0.3), 0.1);
            float vGlow = 1.0 - smoothstep(0.0, 0.03, abs(d) - 0.001);
            float gGlow = min(exp(d * -20.0), 2.0);
            intensity += (vGlow + gGlow * 0.7) * 0.5;
        }
    }

    intensity = max(intensity, 0.0);
    float rayMask = smoothstep(0.3, 0.75, intensity);
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + bass * 0.2, 0.0, 1.0));
    vec3 rayColor = palette * pow(vec3(clamp(intensity, 0.0, 1.0)), vec3(1.5));
    vec3 color = mix(vec3(0.0), rayColor, rayMask);
    color *= 0.7 + energy * 0.5;
    color = clamp(color, 0.0, 1.0);

    return vec4(color, 1.0);
}

#line 1 34
// @EFFECT name="Recursive Cube Bloom" index=55 desc="Pulsing cubic lattice with ray-marched glow" author="John Ao (ShaderToy)"

const float RCC_EPS = 0.001;
const float RCC_R = 0.07;
const int RCC_MAX_STEPS = 80;
const int RCC_AA = 12;

float rcc_sdf(vec3 p) {
    p = fract(p + 0.5);
    p = min(p, 1.0 - p);
    p *= p;
    return sqrt(p.x < p.y ? p.x + min(p.y, p.z) : p.y + min(p.x, p.z)) - RCC_R;
}

float rcc_sum3(vec3 x) {
    return x.x + x.y + x.z;
}

float rcc_max3(vec3 x) {
    return max(x.x, max(x.y, x.z));
}

float rcc_min3(vec3 x) {
    return min(x.x, min(x.y, x.z));
}

float rcc_cube_intersect(vec3 o, vec3 d) {
    vec3 a = 1.0 / d;
    vec3 b = -o * a;
    vec3 c = abs(a) * 0.5;
    float t1 = rcc_max3(b - c);
    float t2 = rcc_min3(b + c);
    return (0.0 < t1 && t1 < t2) ? t1 : -1.0;
}

float rcc_random2(vec2 seed) {
    return fract(1000.0 * sin(seed.x * 12345.0 + seed.y) * sin(seed.y * 1234.0 + seed.x));
}

vec4 renderRecursiveCubeBloom(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 fragCoord = (st * 0.5 + 0.5) * uResolution;
    vec2 localResolution = uResolution;
    float animTime = time * (0.8 + tempo * 0.05) + bass * 0.4;

    vec3 camDir = normalize(vec3(sin(animTime), cos(animTime), sin(animTime * 0.7 + 1.0)));
    vec3 xAxis = normalize(cross(vec3(0.0, 0.0, 1.0), camDir));
    if (length(xAxis) < 0.001) {
        xAxis = vec3(1.0, 0.0, 0.0);
    }
    vec3 yAxis = cross(camDir, xAxis);
    vec3 cam = camDir * (3.0 + energy * 0.6);

    float col = 0.0;
    float minRes = min(localResolution.x, localResolution.y);

    for (int i = 0; i < RCC_AA; ++i) {
        vec2 jitter = vec2(
            rcc_random2(vec2(float(i), animTime * 0.001 + bass)),
            rcc_random2(vec2(float(i) + 19.0, animTime * 0.002 + mid))
        ) - 0.5;
        vec2 sampleCoord = fragCoord + jitter;
        vec2 uv = 0.8 * (2.0 * sampleCoord - localResolution.xy) / minRes;

        vec3 rayDir = normalize(uv.x * xAxis + uv.y * yAxis - cam);
        float r = rcc_cube_intersect(cam, rayDir);
        if (r > 0.0) {
            float dist = rcc_sdf(cam + r * rayDir);
            if (dist > RCC_EPS) {
                r += dist;
                for (int j = 0; j < RCC_MAX_STEPS; ++j) {
                    if (dist <= RCC_EPS) break;
                    dist = rcc_sdf(cam + r * rayDir);
                    r += dist;
                }
                col += pow(0.7, rcc_sum3(abs(floor(cam + r * rayDir + 0.5))));
            }
        } else {
            col += 0.12;
        }
    }

    col /= float(RCC_AA);
    float audioBoost = 0.8 + energy * 0.6 + bass * 0.4;
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + high * 0.3, 0.0, 1.0));
    float rayMask = smoothstep(0.2, 0.8, col);
    vec3 rayColor = palette * pow(vec3(clamp(col, 0.0, 1.0)), vec3(1.2));
    vec3 color = mix(vec3(0.0), rayColor, rayMask);
    color *= audioBoost;
    color = clamp(color, 0.0, 1.0);

    return vec4(color, 1.0);
}

#line 1 35
// @EFFECT name="Hello World Grid" index=56 desc="Minimal HELLO, WORLD! marquee with palette tint" author="Lygia-inspired"

// ─── Glyph atlas ──────────────────────────────────────────────────────────────
// 10 glyphs × 7 rows, 5-bit bitmask per row (bit 4 = leftmost column)
// Order: H E L O , [space] W R D !

int glyphRow(int g, int r) {
    if (r < 0 || r >= 7) return 0;
    const int ATLAS[70] = int[](
        17, 17, 31, 17, 17, 17, 17,  // H
        31, 16, 16, 31, 16, 16, 31,  // E
        16, 16, 16, 16, 16, 16, 31,  // L
        14, 17, 17, 17, 17, 17, 14,  // O
         0,  0,  0,  0,  0, 12,  8,  // ,
         0,  0,  0,  0,  0,  0,  0,  // [space]
        17, 17, 21, 21, 21, 31, 17,  // W
        31, 17, 17, 31, 20, 18, 17,  // R
        30, 17, 17, 17, 17, 17, 30,  // D
         4,  4,  4,  4,  4,  0,  4   // !
    );
    return ATLAS[g * 7 + r];
}

// ─── Message ──────────────────────────────────────────────────────────────────
const int MESSAGE_LEN = 13;
const int MESSAGE[13] = int[](0, 1, 2, 2, 3, 4, 5, 6, 3, 7, 2, 8, 9);
// H  E  L  L  O  ,  _  W  O  R  L  D  !

// LITN marquee glyph data -----------------------------------------------------
const int LITN_MESSAGE_LEN = 5;
const int LITN_GLYPH_L = 11;
const int LITN_GLYPH_I = 8;
const int LITN_GLYPH_T = 19;
const int LITN_GLYPH_N = 13;
const int LITN_GLYPH_SPACE = 26;

// A-Z (5×7 bitmap rows) + space
const int LITN_ATLAS[189] = int[](
    14, 17, 17, 31, 17, 17, 17,  // A
    30, 17, 17, 30, 17, 17, 30,  // B
    14, 17, 16, 16, 16, 17, 14,  // C
    30, 17, 17, 17, 17, 17, 30,  // D
    31, 16, 16, 30, 16, 16, 31,  // E
    31, 16, 16, 30, 16, 16, 16,  // F
    14, 17, 16, 23, 17, 17, 15,  // G
    17, 17, 17, 31, 17, 17, 17,  // H
    31,  4,  4,  4,  4,  4, 31,  // I
     1,  1,  1,  1, 17, 17, 14,  // J
    17, 18, 20, 24, 20, 18, 17,  // K
    16, 16, 16, 16, 16, 16, 31,  // L
    17, 27, 21, 21, 17, 17, 17,  // M
    17, 25, 21, 21, 19, 17, 17,  // N
    14, 17, 17, 17, 17, 17, 14,  // O
    30, 17, 17, 30, 16, 16, 16,  // P
    14, 17, 17, 17, 21, 18, 13,  // Q
    30, 17, 17, 30, 20, 18, 17,  // R
    15, 16, 16, 14,  1,  1, 30,  // S
    31,  4,  4,  4,  4,  4,  4,  // T
    17, 17, 17, 17, 17, 17, 14,  // U
    17, 17, 17, 17, 17, 10,  4,  // V
    17, 17, 17, 21, 21, 21, 10,  // W
    17, 17, 10,  4, 10, 17, 17,  // X
    17, 17, 10,  4,  4,  4,  4,  // Y
    31,  1,  2,  4,  8, 16, 31,  // Z
     0,  0,  0,  0,  0,  0,  0   // space
);

const int LITN_MESSAGE[LITN_MESSAGE_LEN] = int[](
    LITN_GLYPH_L,
    LITN_GLYPH_I,
    LITN_GLYPH_T,
    LITN_GLYPH_N,
    LITN_GLYPH_SPACE
);

int litnGlyphRow(int g, int r) {
    if (r < 0 || r >= 7) return 0;
    return LITN_ATLAS[g * 7 + r];
}

float sampleLitnGlyph(int g, vec2 uv) {
    const float HMARGIN = 0.08;
    const float VMARGIN = 0.05;
    vec2 inner = (uv - vec2(HMARGIN, VMARGIN))
               / vec2(1.0 - 2.0 * HMARGIN, 1.0 - 2.0 * VMARGIN);
    if (any(lessThan(inner, vec2(0.0))) || any(greaterThan(inner, vec2(1.0)))) {
        return 0.0;
    }

    vec2  p    = inner * vec2(5.0, 7.0);
    ivec2 cell = ivec2(floor(p));
    if (cell.x < 0 || cell.x >= 5 || cell.y < 0 || cell.y >= 7) {
        return 0.0;
    }

    int   mask = litnGlyphRow(g, cell.y);
    int   bit  = 1 << (4 - cell.x);
    float on   = (mask & bit) != 0 ? 1.0 : 0.0;

    vec2  frc = fract(p);
    float aa  = smoothstep(0.01, 0.03, frc.x)
              * smoothstep(0.01, 0.03, frc.y)
              * smoothstep(0.01, 0.03, 1.0 - frc.x)
              * smoothstep(0.01, 0.03, 1.0 - frc.y);
    return on * aa;
}

float marqueeLitnChar(vec2 uv, float scrollX, out int col) {
    float cx = fract(uv.x - scrollX / float(LITN_MESSAGE_LEN)) * float(LITN_MESSAGE_LEN);
    col = int(floor(cx)) % LITN_MESSAGE_LEN;
    if (col < 0) col += LITN_MESSAGE_LEN;
    vec2 gUV = vec2(fract(cx), uv.y);
    return sampleLitnGlyph(LITN_MESSAGE[col], gUV);
}

// @EFFECT name="LITN Grid" index=58 desc="Multi-row white marquee that spells LITN" author="System" zoom=3.43
vec4 renderLitnGrid(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec2 uv = st * 0.5 + 0.5;
    uv.x *= uResolution.y / max(uResolution.x, 1.0);
    uv += vec2(-0.35, -0.48);

    const int ROW_COUNT = 1;
    float bandH = 0.26;
    float baseY = 0.008;

    vec3 bg = mix(vec3(0.015, 0.015, 0.025),
                  uSecondaryColor * 0.04,
                  clamp(uColorBlend, 0.0, 1.0));
    vec3 color = bg;
    float alpha = 0.0;

    for (int row = 0; row < ROW_COUNT; ++row) {
        float bandY = baseY;
        float wobble = 0.006 * sin(time * 3.2 + uv.x * 8.0);
        float band = (uv.y - bandY + wobble) / bandH;

        float edgeFade = smoothstep(0.0, 0.12, band)
                       * smoothstep(1.0, 0.88, band);
        if (edgeFade <= 0.001) {
            continue;
        }

        float scanRow = fract(band * 7.0);
        float scanLine = 1.0 - 0.05 * (1.0 - smoothstep(0.9, 1.0, scanRow));
        float gridMask = sin((uv.x + time * 0.6) * 320.0) * sin((uv.y + time * 0.6) * 180.0);
        float ledPulse = 0.5 + 0.5 * gridMask;

        const float scrollSpeed = 0.32;
        float scrollPhase = time * scrollSpeed;
        float scrollX = mod(scrollPhase, float(LITN_MESSAGE_LEN));

        int col = 0;
        float glyphX = fract((uv.x - 0.02) * 1.35 + 10.0);
        vec2 glyphUV = vec2(glyphX, 1.0 - band);
        float charVal = marqueeLitnChar(glyphUV, scrollX, col);
        if (charVal <= 0.0001) {
            continue;
        }

        // Bright white letters with high intensity
        vec3 letterColor = vec3(2.5);
        
        // Strong glow halo around letters
        float glowAmt = charVal * 0.8 * edgeFade;
        vec3 glowCol = vec3(1.5) * 0.8;
        
        // Full brightness where letters exist
        float letterMask = charVal * edgeFade;
        color = mix(color, letterColor, letterMask);
        color += glowAmt * glowCol;
        
        // Very high alpha for letter visibility
        alpha = max(alpha, charVal * edgeFade * 1.5);
    }

    return vec4(color, alpha);
}

// ─── Glyph sampler ────────────────────────────────────────────────────────────
// Pixel grid is mapped with a narrow dead-zone on each edge so adjacent
// characters have a visible gap, and the feather is kept very tight so
// pixels read as crisp on-screen even at low resolutions.
//
// SPACING trick: instead of the full [0,1] UV range we use [MARGIN, 1-MARGIN]
// for the active glyph area.  The rest is always 0 → pure black gap.

float sampleGlyph(int g, vec2 uv) {
    // Character cell margin — increases inter-character gap without changing
    // the glyph data.  0.08 = ~8% dead-zone on each horizontal side.
    const float HMARGIN = 0.08;  // horizontal gap between characters
    const float VMARGIN = 0.04;  // small top/bottom breathing room

    // Remap uv into the active area, return 0 outside it
    vec2 inner = (uv - vec2(HMARGIN, VMARGIN))
               / vec2(1.0 - 2.0 * HMARGIN, 1.0 - 2.0 * VMARGIN);
    if (any(lessThan(inner, vec2(0.0))) || any(greaterThan(inner, vec2(1.0))))
        return 0.0;

    vec2  p    = inner * vec2(5.0, 7.0);
    ivec2 cell = ivec2(floor(p));
    // Clamp to valid range (remap guarantees this, but guard anyway)
    if (cell.x < 0 || cell.x >= 5 || cell.y < 0 || cell.y >= 7) return 0.0;

    int   mask = glyphRow(g, cell.y);
    int   bit  = 1 << (4 - cell.x);
    float on   = (mask & bit) != 0 ? 1.0 : 0.0;

    // Very tight feather — keeps pixels crisp while removing the hard 1-pixel
    // aliasing step.  Use a narrower range than before (0.03–0.08 vs 0.04–0.12).
    vec2  frc = fract(p);
    float aa  = smoothstep(0.03, 0.08, frc.x)
              * smoothstep(0.03, 0.08, frc.y)
              * smoothstep(0.03, 0.08, 1.0 - frc.x)
              * smoothstep(0.03, 0.08, 1.0 - frc.y);
    return on * aa;
}

// ─── Infinite marquee ─────────────────────────────────────────────────────────

float marqueeChar(vec2 uv, float scrollX, out int col) {
    float cx = fract(uv.x - scrollX / float(MESSAGE_LEN)) * float(MESSAGE_LEN);
    col = int(floor(cx)) % MESSAGE_LEN;
    if (col < 0) col += MESSAGE_LEN;
    vec2 gUV = vec2(fract(cx), uv.y);
    return sampleGlyph(MESSAGE[col], gUV);
}

// ─── Main entry point ─────────────────────────────────────────────────────────

vec4 renderHelloWorldGrid(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    // ── Aspect-correct UV ────────────────────────────────────────────────────
    vec2 uv  = st * 0.5 + 0.5;
    uv.x    *= uResolution.y / max(uResolution.x, 1.0);

    // ── Multi-row layout ─────────────────────────────────────────────────────
    // bandH is taller so each glyph row has more pixels → clearer letterforms.
    // rowSpacing adds extra breathing room between rows.
    const int ROW_COUNT  = 6;
    float bandH          = 0.18 + energy * 0.02;   // taller band → bigger glyphs
    float rowSpacing     = bandH + 0.06;             // fixed gap, not energy-driven
    float baseY          = 0.5  + mid * 0.02;        // gentler vertical float

    vec3  bg    = mix(vec3(0.02, 0.02, 0.04),
                      uSecondaryColor * 0.05,
                      clamp(uColorBlend, 0.0, 1.0));
    vec3  color = bg;
    float alpha = 0.0;

    for (int row = 0; row < ROW_COUNT; ++row) {
        float offset = (float(row) - 0.5 * float(ROW_COUNT - 1)) * rowSpacing;
        float bandY  = baseY + offset;
        float band   = (uv.y - bandY) / bandH;

        // Wider fade ramp so the top/bottom of each band is never hard-clipped
        float edgeFade = smoothstep(0.0, 0.12, band)
                       * smoothstep(1.0, 0.88, band);
        if (edgeFade <= 0.001) continue;

        // Scan-line: very subtle — just enough to hint at a pixel-grid feel
        float scanRow  = fract(band * 7.0);
        float scanLine = 1.0 - 0.10 * (1.0 - smoothstep(0.88, 1.0, scanRow));

        // Alternating scroll direction, each row slightly different speed
        float dir         = (row % 2 == 0) ? 1.0 : -1.0;
        float scrollSpeed = 0.8 * (tempo / 120.0) * (1.0 + 0.10 * float(row)) * dir;
        float scrollPhase = time * scrollSpeed + float(row) * 0.6;
        float scrollX     = fract(scrollPhase / float(MESSAGE_LEN)) * float(MESSAGE_LEN);

        int   col     = 0;
        vec2  glyphUV = vec2(uv.x, 1.0 - band);
        float charVal = marqueeChar(glyphUV, scrollX, col);
        if (charVal <= 0.0001) continue;

        // ── Per-character pulse ──────────────────────────────────────────────
        float pulse = 0.75 + 0.25 * sin(time * 2.2
                              + float(col) * 0.45
                              + bass  * 1.5
                              + float(row) * 0.7);

        // ── Letter brightness — boosted base, no longer drifts too dark ──────
        // Using 1.15 minimum so pixels are always clearly white/bright even
        // at low energy.  energy and high add shimmer on top.
        float brightness = 1.15 + energy * 0.35 + high * 0.25;
        vec3  letterColor = vec3(brightness) * pulse;

        // ── Bloom — tighter radius, lower amplitude → less smearing ──────────
        // glowAmt drives a small halo; keeping it at ≤0.25 prevents bloom from
        // washing out the letterform edges.
        float glowAmt = charVal * clamp(0.15 + bass * 0.25 + high * 0.15, 0.0, 0.25)
                      * scanLine * edgeFade;
        vec3  glowCol = vec3(1.0) * (0.40 + high * 0.1);

        // ── Composite this row into accumulated color ────────────────────────
        float maskVal = charVal * edgeFade * scanLine;
        color  = mix(color, letterColor, maskVal);
        color += glowAmt * glowCol * 0.4;

        alpha = clamp(alpha + maskVal * (0.35 + energy * 0.15), 0.0, 1.0);
    }

    return vec4(color, alpha);
}

#line 1 36
// @EFFECT name="Daily Flow Lines" index=57 desc="Reactive ribbon drops inspired by Will Stallwood daily 031" author="Will Stallwood (port by System)"

const float DLF_PI = 3.14159265359;
const float DLF_TWO_PI = 6.28318530718;
const float DLF_SECONDS = 6.0;
const int DLF_OCTAVES = 7;
const float DLF_SEED = 43758.5453123;

float dlfEaseOutCubic(float t) {
    t -= 1.0;
    return t * t * t + 1.0;
}

float dlfEaseInCubic(float t) {
    return t * t * t;
}

float dlfLinearStep(float begin, float end, float t) {
    return clamp((t - begin) / (end - begin), 0.0, 1.0);
}

float dlfRandom(vec2 st) {
    return fract(sin(dot(st, vec2(12.9898, 78.233))) * DLF_SEED);
}

float dlfNoise(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    float a = dlfRandom(i);
    float b = dlfRandom(i + vec2(1.0, 0.0));
    float c = dlfRandom(i + vec2(0.0, 1.0));
    float d = dlfRandom(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float dlfFBM(vec2 st) {
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100.0);
    mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
    for (int i = 0; i < DLF_OCTAVES; ++i) {
        v += a * dlfNoise(st);
        st = rot * st * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

float dlfPattern(vec2 p, float m, out vec2 q, out vec2 r) {
    q.x = dlfFBM(p + vec2(0.0) + m);
    q.y = dlfFBM(p + vec2(5.2, 1.3) + m);
    r.x = dlfFBM(p + 4.0 * q + vec2(1.7, 9.2));
    r.y = dlfFBM(p + 4.0 * q + vec2(8.3, 2.8));
    return dlfFBM(p + 4.0 * r);
}

float dlfSdLine(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

vec2 dlfCenter(vec2 st) {
    float aspect = uResolution.x / max(uResolution.y, 1.0);
    st.x = st.x * aspect - aspect * 0.5 + 0.5;
    return st;
}

vec2 dlfPreparedCoords(vec2 st) {
    vec2 uv = st;
    uv.x *= uResolution.y / max(uResolution.x, 1.0);
    uv = uv * 0.5 + 0.5;
    uv = dlfCenter(uv);
    return uv * 2.0 - 1.0;
}

vec4 renderDailyFlowLines(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 plane = dlfPreparedCoords(st);

    float beatSpeed = mix(0.6, 1.8, clamp(tempo * 0.35 + energy * 0.4, 0.0, 1.0));
    float tPhase = fract(time / DLF_SECONDS);
    float period = mod(time, DLF_SECONDS);

    float motion = dlfEaseOutCubic(dlfLinearStep(0.0, 1.0, period)) * beatSpeed * (1.0 + bass * 0.8);
    vec2 q, r;
    float flowWarp = 1.0 + high * 0.35 + mid * 0.2;
    float pattern = dlfPattern(plane * flowWarp + vec2(bass, high) * 0.25, motion * (0.05 + energy * 0.1), q, r);

    vec2 pos = vec2(2.0);
    float spacing = mix(0.11, 0.18, clamp(mid * 0.7 + high * 0.3, 0.0, 1.0));
    float baseSize = mix(0.008, 0.02, clamp(high * 0.8 + energy * 0.5, 0.0, 1.0));
    float dropOffset = mix(0.9, 1.4, clamp(energy + bass * 0.5, 0.0, 1.0));
    float posTiming = mod(time + bass * 0.5, DLF_SECONDS * 0.5);

    pos.x = mix(2.0, 0.4 - bass * 0.2, dlfEaseOutCubic(dlfLinearStep(0.0, 2.0, posTiming)));
    pos.y = mix(2.2 + bass * 0.5, -0.6 - energy * 0.3, dlfEaseOutCubic(dlfLinearStep(0.0, 1.0, posTiming)));
    float size = mix(baseSize, 0.9, dlfEaseInCubic(dlfLinearStep(1.2, 3.2, posTiming)));

    float invert = step(0.5, fract(tPhase * (2.0 + bass * 1.5))) * step(0.2, high + energy * 0.3);

    vec3 sdf = vec3(1.0);
    vec2 sdfPos;

    sdfPos = mix(pos, pos, dlfLinearStep(0.0, dropOffset, posTiming));
    sdf.x = min(sdf.x, dlfSdLine(plane, vec2(sdfPos.x, -spacing * 2.0).yx, vec2(sdfPos.y, -spacing * 2.0).yx) * pattern - size);

    sdfPos = mix(pos + 1.0, pos, dlfLinearStep(0.0, dropOffset, posTiming));
    sdf.x = min(sdf.x, dlfSdLine(plane, vec2(sdfPos.x, -spacing).yx, vec2(sdfPos.y, -spacing).yx) * pattern - size);

    sdfPos = mix(pos + 2.0, pos, dlfLinearStep(0.0, dropOffset, posTiming));
    sdf.x = min(sdf.x, dlfSdLine(plane, vec2(sdfPos.x, 0.0).yx, vec2(sdfPos.y, 0.0).yx) * pattern - size);

    sdfPos = mix(pos + 3.0, pos, dlfLinearStep(0.0, dropOffset, posTiming));
    sdf.x = min(sdf.x, length(plane - vec2(spacing, sdfPos.y)) * pattern - size);

    sdfPos = mix(pos + 4.0, pos, dlfLinearStep(0.0, dropOffset, posTiming));
    sdf.x = min(sdf.x, dlfSdLine(plane, vec2(sdfPos.x, spacing * 2.0).yx, vec2(sdfPos.y, spacing * 2.0).yx) * pattern - size);

    float stroke = 1.0 - smoothstep(0.0, 0.003 + high * 0.0015 + energy * 0.001, sdf.x);
    vec3 palette = mix(uPrimaryColor, uSecondaryColor, clamp(uColorBlend + high * 0.4 + mid * 0.2, 0.0, 1.0));
    vec3 baseGlow = mix(vec3(0.05, 0.06, 0.08), palette, 0.3 + high * 0.2);
    vec3 color = mix(baseGlow, palette, stroke);

    float shimmer = 0.35 + 0.65 * stroke + bass * 0.25;
    color *= shimmer * (1.0 + vec3(bass * 0.4, mid * 0.3, high * 0.5));
    color += palette * stroke * (0.15 + energy * 0.4);

    if (invert > 0.5) {
        color = mix(vec3(0.95), color, 0.45 + high * 0.3);
    }

    float alpha = clamp(0.4 + stroke * 0.5 + energy * 0.3 + high * 0.2, 0.0, 1.0);
    return vec4(clamp(color, 0.0, 1.0), alpha);
}

#line 1 37
// @EFFECT name="HANNAH ADAMS Grid" index=59 desc="Bottom marquee that spells HANNAH ADAMS" author="System" zoom=3.43

// HANNAH ADAMS marquee glyph data -----------------------------------------------------
const int HANNAH_MESSAGE_LEN = 14;
const int HANNAH_GLYPH_H = 7;   // H is index 7 in A-Z
const int HANNAH_GLYPH_A = 0;   // A is index 0
const int HANNAH_GLYPH_N = 13;  // N is index 13
const int HANNAH_GLYPH_SPACE = 26; // space is index 26
const int HANNAH_GLYPH_D = 3;   // D is index 3
const int HANNAH_GLYPH_M = 12;  // M is index 12
const int HANNAH_GLYPH_S = 18;  // S is index 18

// A-Z (5×7 bitmap rows) + space
const int HANNAH_ATLAS[189] = int[](
    14, 17, 17, 31, 17, 17, 17,  // A (0)
    30, 17, 17, 30, 17, 17, 30,  // B (1)
    14, 17, 16, 16, 16, 17, 14,  // C (2)
    30, 17, 17, 17, 17, 17, 30,  // D (3)
    31, 16, 16, 30, 16, 16, 31,  // E (4)
    31, 16, 16, 30, 16, 16, 16,  // F (5)
    14, 17, 16, 23, 17, 17, 15,  // G (6)
    17, 17, 17, 31, 17, 17, 17,  // H (7)
    31,  4,  4,  4,  4,  4, 31,  // I (8)
     1,  1,  1,  1, 17, 17, 14,  // J (9)
    17, 18, 20, 24, 20, 18, 17,  // K (10)
    16, 16, 16, 16, 16, 16, 31,  // L (11)
    17, 27, 21, 21, 17, 17, 17,  // M (12)
    17, 25, 21, 21, 19, 17, 17,  // N (13)
    14, 17, 17, 17, 17, 17, 14,  // O (14)
    30, 17, 17, 30, 16, 16, 16,  // P (15)
    14, 17, 17, 17, 21, 18, 13,  // Q (16)
    30, 17, 17, 30, 20, 18, 17,  // R (17)
    15, 16, 16, 14,  1,  1, 30,  // S (18)
    31,  4,  4,  4,  4,  4,  4,  // T (19)
    17, 17, 17, 17, 17, 17, 14,  // U (20)
    17, 17, 17, 17, 17, 10,  4,  // V (21)
    17, 17, 17, 21, 21, 21, 10,  // W (22)
    17, 17, 10,  4, 10, 17, 17,  // X (23)
    17, 17, 10,  4,  4,  4,  4,  // Y (24)
    31,  1,  2,  4,  8, 16, 31,  // Z (25)
     0,  0,  0,  0,  0,  0,  0   // space (26)
);

const int HANNAH_MESSAGE[HANNAH_MESSAGE_LEN] = int[](
    HANNAH_GLYPH_H,
    HANNAH_GLYPH_A,
    HANNAH_GLYPH_N,
    HANNAH_GLYPH_N,
    HANNAH_GLYPH_A,
    HANNAH_GLYPH_H,
    HANNAH_GLYPH_SPACE,
    HANNAH_GLYPH_A,
    HANNAH_GLYPH_D,
    HANNAH_GLYPH_A,
    HANNAH_GLYPH_M,
    HANNAH_GLYPH_S,
    HANNAH_GLYPH_SPACE,
    HANNAH_GLYPH_SPACE
);

int hannahGlyphRow(int g, int r) {
    if (r < 0 || r >= 7) return 0;
    return HANNAH_ATLAS[g * 7 + r];
}

float sampleHannahGlyph(int g, vec2 uv) {
    const float HMARGIN = 0.08;
    const float VMARGIN = 0.05;
    vec2 inner = (uv - vec2(HMARGIN, VMARGIN))
               / vec2(1.0 - 2.0 * HMARGIN, 1.0 - 2.0 * VMARGIN);
    if (any(lessThan(inner, vec2(0.0))) || any(greaterThan(inner, vec2(1.0)))) {
        return 0.0;
    }

    vec2  p    = inner * vec2(5.0, 7.0);
    ivec2 cell = ivec2(floor(p));
    if (cell.x < 0 || cell.x >= 5 || cell.y < 0 || cell.y >= 7) {
        return 0.0;
    }

    int   mask = hannahGlyphRow(g, cell.y);
    int   bit  = 1 << (4 - cell.x);
    float on   = (mask & bit) != 0 ? 1.0 : 0.0;

    vec2  frc = fract(p);
    float aa  = smoothstep(0.01, 0.03, frc.x)
              * smoothstep(0.01, 0.03, frc.y)
              * smoothstep(0.01, 0.03, 1.0 - frc.x)
              * smoothstep(0.01, 0.03, 1.0 - frc.y);
    return on * aa;
}

float marqueeHannahChar(vec2 uv, float scrollX, out int col) {
    float cx = fract(uv.x - scrollX / float(HANNAH_MESSAGE_LEN)) * float(HANNAH_MESSAGE_LEN);
    col = int(floor(cx)) % HANNAH_MESSAGE_LEN;
    if (col < 0) col += HANNAH_MESSAGE_LEN;
    vec2 gUV = vec2(fract(cx), uv.y);
    return sampleHannahGlyph(HANNAH_MESSAGE[col], gUV);
}

vec4 renderHannahAdamsGrid(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec2 uv = st * 0.5 + 0.5;
    uv.x *= uResolution.y / max(uResolution.x, 1.0);
    uv += vec2(-0.35, -0.48);

    const int ROW_COUNT = 1;
    float bandH = 0.26;
    float baseY = 0.008;

    vec3 bg = mix(vec3(0.015, 0.015, 0.025),
                  uSecondaryColor * 0.04,
                  clamp(uColorBlend, 0.0, 1.0));
    vec3 color = bg;
    float alpha = 0.0f;

    for (int row = 0; row < ROW_COUNT; ++row) {
        float bandY = baseY;
        float wobble = 0.006 * sin(time * 3.2 + uv.x * 8.0);
        float band = (uv.y - bandY + wobble) / bandH;

        float edgeFade = smoothstep(0.0, 0.12, band)
                       * smoothstep(1.0, 0.88, band);
        if (edgeFade <= 0.001) {
            continue;
        }

        float scanRow = fract(band * 7.0);
        float scanLine = 1.0 - 0.05 * (1.0 - smoothstep(0.9, 1.0, scanRow));
        float gridMask = sin((uv.x + time * 0.6) * 320.0) * sin((uv.y + time * 0.6) * 180.0);
        float ledPulse = 0.5 + 0.5 * gridMask;

        const float scrollSpeed = 0.32;
        float scrollPhase = time * scrollSpeed;
        float scrollX = mod(scrollPhase, float(HANNAH_MESSAGE_LEN));

        int col = 0;
        float glyphX = fract((uv.x - 0.02) * 1.35 + 10.0);
        vec2 glyphUV = vec2(glyphX, 1.0 - band);
        float charVal = marqueeHannahChar(glyphUV, scrollX, col);
        if (charVal <= 0.0001) {
            continue;
        }

        // Bright white letters with high intensity
        vec3 letterColor = vec3(2.5);
        
        // Strong glow halo around letters
        float glowAmt = charVal * 0.8 * edgeFade;
        vec3 glowCol = vec3(1.5) * 0.8;
        
        // Full brightness where letters exist
        float letterMask = charVal * edgeFade;
        color = mix(color, letterColor, letterMask);
        color += glowAmt * glowCol;
        
        // Very high alpha for letter visibility
        alpha = max(alpha, charVal * edgeFade * 1.5);
    }

    return vec4(color, alpha);
}

#line 1 38
// @EFFECT name="ANOTHER CODE Grid" index=60 desc="Bottom marquee that spells ANOTHER CODE" author="System" zoom=3.43

// ANOTHER CODE marquee glyph data -----------------------------------------------------
const int ANOTHER_MESSAGE_LEN = 13;
const int ANOTHER_GLYPH_A = 0;   // A is index 0 in A-Z
const int ANOTHER_GLYPH_N = 13;  // N is index 13
const int ANOTHER_GLYPH_O = 14;  // O is index 14
const int ANOTHER_GLYPH_T = 19;  // T is index 19
const int ANOTHER_GLYPH_H = 7;   // H is index 7
const int ANOTHER_GLYPH_E = 4;   // E is index 4
const int ANOTHER_GLYPH_R = 17;  // R is index 17
const int ANOTHER_GLYPH_C = 2;   // C is index 2
const int ANOTHER_GLYPH_D = 3;   // D is index 3
const int ANOTHER_GLYPH_SPACE = 26; // space is index 26

// A-Z (5×7 bitmap rows) + space
const int ANOTHER_ATLAS[189] = int[](
    14, 17, 17, 31, 17, 17, 17,  // A (0)
    30, 17, 17, 30, 17, 17, 30,  // B (1)
    14, 17, 16, 16, 16, 17, 14,  // C (2)
    30, 17, 17, 17, 17, 17, 30,  // D (3)
    31, 16, 16, 30, 16, 16, 31,  // E (4)
    31, 16, 16, 30, 16, 16, 16,  // F (5)
    14, 17, 16, 23, 17, 17, 15,  // G (6)
    17, 17, 17, 31, 17, 17, 17,  // H (7)
    31,  4,  4,  4,  4,  4, 31,  // I (8)
     1,  1,  1,  1, 17, 17, 14,  // J (9)
    17, 18, 20, 24, 20, 18, 17,  // K (10)
    16, 16, 16, 16, 16, 16, 31,  // L (11)
    17, 27, 21, 21, 17, 17, 17,  // M (12)
    17, 25, 21, 21, 19, 17, 17,  // N (13)
    14, 17, 17, 17, 17, 17, 14,  // O (14)
    30, 17, 17, 30, 16, 16, 16,  // P (15)
    14, 17, 17, 17, 21, 18, 13,  // Q (16)
    30, 17, 17, 30, 20, 18, 17,  // R (17)
    15, 16, 16, 14,  1,  1, 30,  // S (18)
    31,  4,  4,  4,  4,  4,  4,  // T (19)
    17, 17, 17, 17, 17, 17, 14,  // U (20)
    17, 17, 17, 17, 17, 10,  4,  // V (21)
    17, 17, 17, 21, 21, 21, 10,  // W (22)
    17, 17, 10,  4, 10, 17, 17,  // X (23)
    17, 17, 10,  4,  4,  4,  4,  // Y (24)
    31,  1,  2,  4,  8, 16, 31,  // Z (25)
     0,  0,  0,  0,  0,  0,  0   // space (26)
);

const int ANOTHER_MESSAGE[ANOTHER_MESSAGE_LEN] = int[](
    ANOTHER_GLYPH_A,
    ANOTHER_GLYPH_N,
    ANOTHER_GLYPH_O,
    ANOTHER_GLYPH_T,
    ANOTHER_GLYPH_H,
    ANOTHER_GLYPH_E,
    ANOTHER_GLYPH_R,
    ANOTHER_GLYPH_SPACE,
    ANOTHER_GLYPH_C,
    ANOTHER_GLYPH_O,
    ANOTHER_GLYPH_D,
    ANOTHER_GLYPH_E,
    ANOTHER_GLYPH_SPACE
);

int anotherGlyphRow(int g, int r) {
    if (r < 0 || r >= 7) return 0;
    return ANOTHER_ATLAS[g * 7 + r];
}

float sampleAnotherGlyph(int g, vec2 uv) {
    const float HMARGIN = 0.08;
    const float VMARGIN = 0.05;
    vec2 inner = (uv - vec2(HMARGIN, VMARGIN))
               / vec2(1.0 - 2.0 * HMARGIN, 1.0 - 2.0 * VMARGIN);
    if (any(lessThan(inner, vec2(0.0))) || any(greaterThan(inner, vec2(1.0)))) {
        return 0.0;
    }

    vec2  p    = inner * vec2(5.0, 7.0);
    ivec2 cell = ivec2(floor(p));
    if (cell.x < 0 || cell.x >= 5 || cell.y < 0 || cell.y >= 7) {
        return 0.0;
    }

    int   mask = anotherGlyphRow(g, cell.y);
    int   bit  = 1 << (4 - cell.x);
    float on   = (mask & bit) != 0 ? 1.0 : 0.0;

    vec2  frc = fract(p);
    float aa  = smoothstep(0.01, 0.03, frc.x)
              * smoothstep(0.01, 0.03, frc.y)
              * smoothstep(0.01, 0.03, 1.0 - frc.x)
              * smoothstep(0.01, 0.03, 1.0 - frc.y);
    return on * aa;
}

float marqueeAnotherChar(vec2 uv, float scrollX, out int col) {
    float cx = fract(uv.x - scrollX / float(ANOTHER_MESSAGE_LEN)) * float(ANOTHER_MESSAGE_LEN);
    col = int(floor(cx)) % ANOTHER_MESSAGE_LEN;
    if (col < 0) col += ANOTHER_MESSAGE_LEN;
    vec2 gUV = vec2(fract(cx), uv.y);
    return sampleAnotherGlyph(ANOTHER_MESSAGE[col], gUV);
}

vec4 renderAnotherCodeGrid(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec2 uv = st * 0.5 + 0.5;
    uv.x *= uResolution.y / max(uResolution.x, 1.0);
    uv += vec2(-0.35, -0.48);

    const int ROW_COUNT = 1;
    float bandH = 0.26;
    float baseY = 0.008;

    vec3 bg = mix(vec3(0.015, 0.015, 0.025),
                  uSecondaryColor * 0.04,
                  clamp(uColorBlend, 0.0, 1.0));
    vec3 color = bg;
    float alpha = 0.0;

    for (int row = 0; row < ROW_COUNT; ++row) {
        float bandY = baseY;
        float wobble = 0.006 * sin(time * 3.2 + uv.x * 8.0);
        float band = (uv.y - bandY + wobble) / bandH;

        float edgeFade = smoothstep(0.0, 0.12, band)
                       * smoothstep(1.0, 0.88, band);
        if (edgeFade <= 0.001) {
            continue;
        }

        float scanRow = fract(band * 7.0);
        float scanLine = 1.0 - 0.05 * (1.0 - smoothstep(0.9, 1.0, scanRow));
        float gridMask = sin((uv.x + time * 0.6) * 320.0) * sin((uv.y + time * 0.6) * 180.0);
        float ledPulse = 0.5 + 0.5 * gridMask;

        const float scrollSpeed = 0.32;
        float scrollPhase = time * scrollSpeed;
        float scrollX = mod(scrollPhase, float(ANOTHER_MESSAGE_LEN));

        int col = 0;
        float glyphX = fract((uv.x - 0.02) * 1.35 + 10.0);
        vec2 glyphUV = vec2(glyphX, 1.0 - band);
        float charVal = marqueeAnotherChar(glyphUV, scrollX, col);
        if (charVal <= 0.0001) {
            continue;
        }

        // Bright white letters with high intensity
        vec3 letterColor = vec3(2.5);
        
        // Strong glow halo around letters
        float glowAmt = charVal * 0.8 * edgeFade;
        vec3 glowCol = vec3(1.5) * 0.8;
        
        // Full brightness where letters exist
        float letterMask = charVal * edgeFade;
        color = mix(color, letterColor, letterMask);
        color += glowAmt * glowCol;
        
        // Very high alpha for letter visibility
        alpha = max(alpha, charVal * edgeFade * 1.5);
    }

    return vec4(color, alpha);
}

#line 1 39
// @EFFECT name="LUPERFUT Grid" index=61 desc="Bottom marquee that spells LUPERFUT" author="System" zoom=3.43

// LUPERFUT marquee glyph data -----------------------------------------------------
const int LUPERFUT_MESSAGE_LEN = 9;
const int LUPERFUT_GLYPH_L = 11;  // L is index 11 in A-Z
const int LUPERFUT_GLYPH_U = 20;  // U is index 20
const int LUPERFUT_GLYPH_P = 15;  // P is index 15
const int LUPERFUT_GLYPH_E = 4;   // E is index 4
const int LUPERFUT_GLYPH_R = 17;  // R is index 17
const int LUPERFUT_GLYPH_F = 5;   // F is index 5
const int LUPERFUT_GLYPH_T = 19;  // T is index 19
const int LUPERFUT_GLYPH_SPACE = 26; // space is index 26

// A-Z (5×7 bitmap rows) + space
const int LUPERFUT_ATLAS[189] = int[](
    14, 17, 17, 31, 17, 17, 17,  // A (0)
    30, 17, 17, 30, 17, 17, 30,  // B (1)
    14, 17, 16, 16, 16, 17, 14,  // C (2)
    30, 17, 17, 17, 17, 17, 30,  // D (3)
    31, 16, 16, 30, 16, 16, 31,  // E (4)
    31, 16, 16, 30, 16, 16, 16,  // F (5)
    14, 17, 16, 23, 17, 17, 15,  // G (6)
    17, 17, 17, 31, 17, 17, 17,  // H (7)
    31,  4,  4,  4,  4,  4, 31,  // I (8)
     1,  1,  1,  1, 17, 17, 14,  // J (9)
    17, 18, 20, 24, 20, 18, 17,  // K (10)
    16, 16, 16, 16, 16, 16, 31,  // L (11)
    17, 27, 21, 21, 17, 17, 17,  // M (12)
    17, 25, 21, 21, 19, 17, 17,  // N (13)
    14, 17, 17, 17, 17, 17, 14,  // O (14)
    30, 17, 17, 30, 16, 16, 16,  // P (15)
    14, 17, 17, 17, 21, 18, 13,  // Q (16)
    30, 17, 17, 30, 20, 18, 17,  // R (17)
    15, 16, 16, 14,  1,  1, 30,  // S (18)
    31,  4,  4,  4,  4,  4,  4,  // T (19)
    17, 17, 17, 17, 17, 17, 14,  // U (20)
    17, 17, 17, 17, 17, 10,  4,  // V (21)
    17, 17, 17, 21, 21, 21, 10,  // W (22)
    17, 17, 10,  4, 10, 17, 17,  // X (23)
    17, 17, 10,  4,  4,  4,  4,  // Y (24)
    31,  1,  2,  4,  8, 16, 31,  // Z (25)
     0,  0,  0,  0,  0,  0,  0   // space (26)
);

const int LUPERFUT_MESSAGE[LUPERFUT_MESSAGE_LEN] = int[](
    LUPERFUT_GLYPH_L,
    LUPERFUT_GLYPH_U,
    LUPERFUT_GLYPH_P,
    LUPERFUT_GLYPH_E,
    LUPERFUT_GLYPH_R,
    LUPERFUT_GLYPH_F,
    LUPERFUT_GLYPH_U,
    LUPERFUT_GLYPH_T,
    LUPERFUT_GLYPH_SPACE
);

int luperfutGlyphRow(int g, int r) {
    if (r < 0 || r >= 7) return 0;
    return LUPERFUT_ATLAS[g * 7 + r];
}

float sampleLuperfutGlyph(int g, vec2 uv) {
    const float HMARGIN = 0.08;
    const float VMARGIN = 0.05;
    vec2 inner = (uv - vec2(HMARGIN, VMARGIN))
               / vec2(1.0 - 2.0 * HMARGIN, 1.0 - 2.0 * VMARGIN);
    if (any(lessThan(inner, vec2(0.0))) || any(greaterThan(inner, vec2(1.0)))) {
        return 0.0;
    }

    vec2  p    = inner * vec2(5.0, 7.0);
    ivec2 cell = ivec2(floor(p));
    if (cell.x < 0 || cell.x >= 5 || cell.y < 0 || cell.y >= 7) {
        return 0.0;
    }

    int   mask = luperfutGlyphRow(g, cell.y);
    int   bit  = 1 << (4 - cell.x);
    float on   = (mask & bit) != 0 ? 1.0 : 0.0;

    vec2  frc = fract(p);
    float aa  = smoothstep(0.01, 0.03, frc.x)
              * smoothstep(0.01, 0.03, frc.y)
              * smoothstep(0.01, 0.03, 1.0 - frc.x)
              * smoothstep(0.01, 0.03, 1.0 - frc.y);
    return on * aa;
}

float marqueeLuperfutChar(vec2 uv, float scrollX, out int col) {
    float cx = fract(uv.x - scrollX / float(LUPERFUT_MESSAGE_LEN)) * float(LUPERFUT_MESSAGE_LEN);
    col = int(floor(cx)) % LUPERFUT_MESSAGE_LEN;
    if (col < 0) col += LUPERFUT_MESSAGE_LEN;
    vec2 gUV = vec2(fract(cx), uv.y);
    return sampleLuperfutGlyph(LUPERFUT_MESSAGE[col], gUV);
}

vec4 renderLuperfutGrid(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec2 uv = st * 0.5 + 0.5;
    uv.x *= uResolution.y / max(uResolution.x, 1.0);
    uv += vec2(-0.35, -0.48);

    const int ROW_COUNT = 1;
    float bandH = 0.26;
    float baseY = 0.008;

    vec3 bg = mix(vec3(0.015, 0.015, 0.025),
                  uSecondaryColor * 0.04,
                  clamp(uColorBlend, 0.0, 1.0));
    vec3 color = bg;
    float alpha = 0.0;

    for (int row = 0; row < ROW_COUNT; ++row) {
        float bandY = baseY;
        float wobble = 0.006 * sin(time * 3.2 + uv.x * 8.0);
        float band = (uv.y - bandY + wobble) / bandH;

        float edgeFade = smoothstep(0.0, 0.12, band)
                       * smoothstep(1.0, 0.88, band);
        if (edgeFade <= 0.001) {
            continue;
        }

        float scanRow = fract(band * 7.0);
        float scanLine = 1.0 - 0.05 * (1.0 - smoothstep(0.9, 1.0, scanRow));
        float gridMask = sin((uv.x + time * 0.6) * 320.0) * sin((uv.y + time * 0.6) * 180.0);
        float ledPulse = 0.5 + 0.5 * gridMask;

        const float scrollSpeed = 0.32;
        float scrollPhase = time * scrollSpeed;
        float scrollX = mod(scrollPhase, float(LUPERFUT_MESSAGE_LEN));

        int col = 0;
        float glyphX = fract((uv.x - 0.02) * 1.35 + 10.0);
        vec2 glyphUV = vec2(glyphX, 1.0 - band);
        float charVal = marqueeLuperfutChar(glyphUV, scrollX, col);
        if (charVal <= 0.0001) {
            continue;
        }

        // Bright white letters with high intensity
        vec3 letterColor = vec3(2.5);
        
        // Strong glow halo around letters
        float glowAmt = charVal * 0.8 * edgeFade;
        vec3 glowCol = vec3(1.5) * 0.8;
        
        // Full brightness where letters exist
        float letterMask = charVal * edgeFade;
        color = mix(color, letterColor, letterMask);
        color += glowAmt * glowCol;
        
        // Very high alpha for letter visibility
        alpha = max(alpha, charVal * edgeFade * 1.5);
    }

    return vec4(color, alpha);
}

#line 1 40
// @EFFECT name="Glass Refraction Field" index=62 desc="Raymarched glass cubes with refraction and reflection effects" author="Matthias Hurrle"

vec3 hueGlass(float a) {
    return 0.5 + 0.2 * cos(10.3 * a + vec3(0.0, 23.0, 21.0));
}

mat2 rotGlass(float a) {
    return mat2(cos(a), -sin(a), sin(a), cos(a));
}

float sylGlass(vec2 p, float r) {
    return length(p) - r;
}

vec3 dflameGlass(vec2 uv, float time) {
    vec2 n = vec2(0.0);
    vec2 q = vec2(0.0);
    
    uv *= 0.875;
    
    float d = dot(uv, uv);
    float s = 9.0;
    float a = 0.02;
    float b = sin(time * 0.4 - d * 4.0) * 0.9;
    float t = time * 4.0;
    
    uv *= rotGlass(sin(6.0 + t * 0.05) * 0.8 - 0.567);
    uv.y -= t * 0.05;
    
    mat2 m = mat2(0.6, 1.2, -1.2, 0.6);
    for (float i = 0.0; i < 30.0; i++) {
        n *= m;
        q = uv * s - t + b + i + n;
        a += dot(cos(q) / s, vec2(0.2));
        n += sin(q);
        s *= 1.2;
    }
    
    vec3 col = vec3(4.0, 2.0, 1.0) * (a + 0.2) + a + a - d;
    col = exp(-col * 8.0);
    col = abs(col);
    col = sqrt(col);
    col = exp(-col * 4.0);
    
    return col;
}

float tickGlass(float t, float e) {
    return floor(t) + pow(smoothstep(0.0, 1.0, fract(t)), e);
}

float boxGlass(vec3 p, vec3 s, float r) {
    p = abs(p) - s;
    return length(max(p, 0.0)) + min(0.0, max(max(p.x, p.y), p.z)) - r;
}

float mapGlass(vec3 p, float time) {
    const float n = 5.5;
    p.yz = (fract(p.yz / n) - 0.5) * n;
    p.xz = (p.xz - n * clamp(round(p.xz / n), -10.0, 10.0));
    
    float T = mod(time, 90.0) * 0.45;
    p.yz *= rotGlass(sin(tickGlass(T, 1.0)));
    p.xz *= rotGlass(sin(tickGlass(T, 1.0)));
    
    float d = 1e5;
    float bx = boxGlass(p, vec3(0.85), 0.125);
    d = min(d, bx);
    
    return d;
}

vec3 normGlass(vec3 p, float time) {
    vec2 e = vec2(1e-2, 0.0);
    float d = mapGlass(p, time);
    vec3 n = d - vec3(
        mapGlass(p - e.xyy, time),
        mapGlass(p - e.yxy, time),
        mapGlass(p - e.yyx, time)
    );
    return normalize(n);
}

vec3 dirGlass(vec2 uv, vec3 ro, vec3 t, float z) {
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 f = normalize(t - ro);
    vec3 r = normalize(cross(up, f));
    vec3 u = cross(f, r);
    vec3 c = f * z;
    vec3 i = c + uv.x * r + uv.y * u;
    return normalize(i);
}

vec4 renderGlassRefractionField(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = st - 0.5;
    uv.x *= uResolution.x / uResolution.y;
    
    float T = mod(time + energy * 2.0, 90.0) * 0.45;
    
    vec3 col = vec3(0.0);
    vec3 tg = vec3(0.0, 0.0, T * 10.0);
    vec3 ro = vec3(0.0, 0.0, tg.z - 20.0);
    vec3 rd = dirGlass(uv, ro, tg, 1.0);
    
    // Camera rotation based on bass
    float bassRot = cos(T * 0.2) * 3.14159 * (0.5 + bass * 0.5);
    ro.xy *= rotGlass(bassRot);
    rd.xy *= rotGlass(bassRot);
    
    // Secondary rotation based on mid
    float midRot = sin(T * 0.2) * 3.14159 * mid;
    rd.xz *= rotGlass(midRot);
    
    vec3 l = normalize(ro - vec3(1.0, 2.0, 3.0));
    vec3 p = ro;
    
    const float steps = 60.0;
    const float maxd = 30.0;
    
    float i = 0.0;
    float dd = 0.0;
    float side = 1.0;
    float e = 1.0;
    
    for (; i < steps; i++) {
        float d = mapGlass(p, time) * side;
        
        if (d < 1e-3) {
            vec3 n = normGlass(p, time) * side;
            float fog = 1.0 - clamp(dd / maxd, 0.0, 1.0);
            float diff = max(0.0, dot(normalize(ro - p), n));
            float fres = clamp(dot(-rd, n), 0.0, 1.0);
            
            vec3 h = normalize(l - rd);
            vec3 pal = hueGlass(diff);
            
            col += e
                * (1.0 - max(0.0, i / 200.0))
                * diff
                * (5.0 * pow(max(0.0, dot(n, h)), 64.0) +
                   0.5 * pow(max(0.0, fres), 32.0))
                * pal;
            
            side = -side;
            vec3 rdo = refract(rd, n, 1.0 + side * 0.45);
            
            if (dot(rdo, rdo) == 0.0) {
                rdo = reflect(rd, n);
            }
            
            rd = rdo;
            d = 9e-2;
            e *= 0.925;
        }
        
        if (dd > maxd) {
            dd = maxd;
            break;
        }
        
        p += rd * d;
        dd += d;
    }
    
    p = ro + rd * maxd;
    float ends = pow(abs(rd.z), 7.0);
    col += ends * dflameGlass(abs(p.xy * 0.05), time);
    
    // Apply palette and energy
    col *= 2.0;
    col *= mix(uPrimaryColor, vec3(1.0), 0.3);
    col += uSecondaryColor * energy * 0.2;
    col = clamp(col, 0.0, 1.0);
    
    return vec4(col, 1.0);
}

#line 1 41
// @EFFECT name="Matrix Digital Rain" index=63 desc="Matrix-style falling digital characters" author="Patricio Gonzalez Vivo"

float randomMatrix(in float x) { 
    return fract(sin(x) * 43758.5453); 
}

float randomMatrix(in vec2 st) { 
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453); 
}

float randomCharMatrix(vec2 outer, vec2 inner) {
    float grid = 5.0;
    vec2 margin = vec2(0.2, 0.05);
    vec2 borders = step(margin, inner) * step(margin, 1.0 - inner);
    vec2 ipos = floor(inner * grid);
    vec2 fpos = fract(inner * grid);
    return step(0.5, randomMatrix(outer * 64.0 + ipos)) * borders.x * borders.y * step(0.01, fpos.x) * step(0.01, fpos.y);
}

vec4 renderMatrixDigitalRain(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Adjust aspect ratio like original
    st.y *= uResolution.y / uResolution.x;
    
    vec3 color = vec3(0.0);
    
    // Rows controlled by energy (1 to 24)
    float rows = mix(1.0, 24.0, energy);
    
    vec2 ipos = floor(st * rows);
    vec2 fpos = fract(st * rows);
    
    // Animation speed controlled by tempo and bass
    float speed = 20.0 * (1.0 + tempo + bass * 2.0);
    ipos += vec2(0.0, floor(time * speed * randomMatrix(ipos.x + 1.0)));
    
    float pct = 1.0;
    pct *= randomCharMatrix(ipos, fpos);
    
    color = vec3(pct);
    
    // Apply matrix green tint mixed with user palette
    vec3 matrixGreen = vec3(0.0, 1.0, 0.0);
    vec3 baseColor = mix(uPrimaryColor, matrixGreen, 0.8);
    
    color *= baseColor;
    
    return vec4(color, 1.0);
}

#line 1 42
// @EFFECT name="Ikeda Digits" index=64 desc="Falling digital numbers like Ikeda data stream" author="Patricio Gonzalez Vivo"

float randomIkeda(in float x) { 
    return fract(sin(x) * 43758.5453); 
}

float randomIkeda(vec2 p) { 
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); 
}

float binIkeda(vec2 ipos, float n) {
    float remain = mod(n, 33554432.0);
    for(float i = 0.0; i < 25.0; i++) {
        if (floor(i / 3.0) == ipos.y && mod(i, 3.0) == ipos.x) {
            return step(1.0, mod(remain, 2.0));
        }
        remain = ceil(remain / 2.0);
    }
    return 0.0;
}

float charIkeda(vec2 st, float n) {
    st.x = st.x * 2.0 - 0.5;
    st.y = st.y * 1.2 - 0.1;

    vec2 grid = vec2(3.0, 5.0);

    vec2 ipos = floor(st * grid);
    vec2 fpos = fract(st * grid);

    n = floor(mod(n, 10.0));
    float digit = 0.0;
    if (n < 1.0) { digit = 31600.0; }
    else if (n < 2.0) { digit = 9363.0; }
    else if (n < 3.0) { digit = 31184.0; }
    else if (n < 4.0) { digit = 31208.0; }
    else if (n < 5.0) { digit = 23525.0; }
    else if (n < 6.0) { digit = 29672.0; }
    else if (n < 7.0) { digit = 29680.0; }
    else if (n < 8.0) { digit = 31013.0; }
    else if (n < 9.0) { digit = 31728.0; }
    else if (n < 10.0) { digit = 31717.0; }
    float pct = binIkeda(ipos, digit);

    vec2 borders = vec2(1.0);
    borders *= step(0.0, st) * step(0.0, 1.0 - st);

    return step(0.5, 1.0 - pct) * borders.x * borders.y;
}

vec4 renderIkedaDigits(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    st.x *= uResolution.x / uResolution.y;

    // Rows controlled by energy
    float rows = mix(10.0, 40.0, energy);
    vec2 ipos = floor(st * rows);
    vec2 fpos = fract(st * rows);

    // Animation speed controlled by tempo and bass
    float speed = 20.0 * (1.0 + tempo + bass * 2.0);
    ipos += vec2(0.0, floor(time * speed * randomIkeda(ipos.x + 1.0)));
    
    float pct = randomIkeda(ipos);
    vec3 color = vec3(charIkeda(fpos, 100.0 * pct));
    
    // Highlight effect
    color = mix(color, vec3(color.r, 0.0, 0.0), step(0.99, pct));
    
    // Mix with user palette
    vec3 digitColor = mix(uPrimaryColor, vec3(0.0, 1.0, 0.0), 0.6);
    vec3 bgColor = mix(uSecondaryColor, vec3(0.0, 0.0, 0.0), 0.8);
    
    color *= digitColor;
    color += bgColor * (1.0 - length(color));
    
    return vec4(color, 1.0);
}

#line 1 43
// @EFFECT name="Ikeda Grid" index=65 desc="Data grid with crosses and animated digits" author="Patricio Gonzalez Vivo"

float randomIkedaGrid(in float x) { 
    return fract(sin(x) * 43758.5453); 
}

float randomIkedaGrid(in vec2 st) { 
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453); 
}

float gridIkeda(vec2 st, float res) {
    vec2 grid = fract(st * res);
    return 1.0 - (step(res, grid.x) * step(res, grid.y));
}

float boxIkeda(in vec2 st, in vec2 size) {
    size = vec2(0.5) - size * 0.5;
    vec2 uv = smoothstep(size, size + vec2(0.001), st);
    uv *= smoothstep(size, size + vec2(0.001), vec2(1.0) - st);
    return uv.x * uv.y;
}

float crossIkeda(in vec2 st, vec2 size) {
    return clamp(boxIkeda(st, vec2(size.x * 0.5, size.y * 0.125)) +
            boxIkeda(st, vec2(size.y * 0.125, size.x * 0.5)), 0.0, 1.0);
}

vec4 renderIkedaGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    st.x *= uResolution.x / uResolution.y;

    vec3 color = vec3(0.0);

    // Grid - intensity controlled by energy
    vec2 grid_st = st * 300.0;
    color += vec3(0.5, 0.0, 0.0) * gridIkeda(grid_st, 0.01);
    color += vec3(0.2, 0.0, 0.0) * gridIkeda(grid_st, 0.02);
    color += vec3(0.2) * gridIkeda(grid_st, 0.1) * (0.5 + energy * 0.5);

    // Crosses - react to bass
    vec2 crosses_st = st + 0.5;
    crosses_st *= 3.0 * (1.0 + bass);
    vec2 crosses_st_f = fract(crosses_st);
    color *= 1.0 - crossIkeda(crosses_st_f, vec2(0.3, 0.3));
    color += vec3(0.9) * crossIkeda(crosses_st_f, vec2(0.2, 0.2));

    // Digits - animation speed controlled by tempo
    vec2 blocks_st = floor(st * 6.0);
    float t = time * (0.8 + tempo) + randomIkedaGrid(blocks_st);
    float time_i = floor(t);
    float time_f = fract(t);
    color.rgb += step(0.9, randomIkedaGrid(blocks_st + time_i)) * (1.0 - time_f);

    // Mix with user palette
    vec3 gridColor = mix(uPrimaryColor, vec3(0.8, 0.0, 0.0), 0.5);
    vec3 accentColor = mix(uSecondaryColor, vec3(0.9, 0.9, 0.0), 0.3);
    
    color *= gridColor;
    color += accentColor * high * 0.3;

    return vec4(color, 1.0);
}

#line 1 44
// @EFFECT name="IChing Hexagrams" index=66 desc="Ancient IChing hexagram patterns with animated transitions" author="Patricio Gonzalez Vivo"

float shapeIChing(vec2 st, float N) {
    st = st * 2.0 - 1.0;
    float a = atan(st.x, st.y) + 3.14159265359;
    float r = 6.28318530718 / N;
    return abs(cos(floor(0.5 + a / r) * r - a) * length(st));
}

float boxIChing(vec2 st, vec2 size) {
    return shapeIChing(st * size, 4.0);
}

float rectIChing(vec2 _st, vec2 _size) {
    _size = vec2(0.5) - _size * 0.5;
    vec2 uv = smoothstep(_size, _size + vec2(1e-4), _st);
    uv *= smoothstep(_size, _size + vec2(1e-4), vec2(1.0) - _st);
    return uv.x * uv.y;
}

float hexIChing(vec2 st, float a, float b, float c, float d, float e, float f) {
    st = st * vec2(2.0, 6.0);

    vec2 fpos = fract(st);
    vec2 ipos = floor(st);

    if (ipos.x == 1.0) fpos.x = 1.0 - fpos.x;
    if (ipos.y < 1.0) {
        return mix(boxIChing(fpos, vec2(0.84, 1.0)), boxIChing(fpos - vec2(0.03, 0.0), vec2(1.0)), a);
    } else if (ipos.y < 2.0) {
        return mix(boxIChing(fpos, vec2(0.84, 1.0)), boxIChing(fpos - vec2(0.03, 0.0), vec2(1.0)), b);
    } else if (ipos.y < 3.0) {
        return mix(boxIChing(fpos, vec2(0.84, 1.0)), boxIChing(fpos - vec2(0.03, 0.0), vec2(1.0)), c);
    } else if (ipos.y < 4.0) {
        return mix(boxIChing(fpos, vec2(0.84, 1.0)), boxIChing(fpos - vec2(0.03, 0.0), vec2(1.0)), d);
    } else if (ipos.y < 5.0) {
        return mix(boxIChing(fpos, vec2(0.84, 1.0)), boxIChing(fpos - vec2(0.03, 0.0), vec2(1.0)), e);
    } else if (ipos.y < 6.0) {
        return mix(boxIChing(fpos, vec2(0.84, 1.0)), boxIChing(fpos - vec2(0.03, 0.0), vec2(1.0)), f);
    }
    return 0.0;
}

float hexIChing(vec2 st, float N) {
    float b[6];
    float remain = floor(mod(N, 64.0));
    for(int i = 0; i < 6; i++) {
        b[i] = 0.0;
        b[i] = step(1.0, mod(remain, 2.0));
        remain = ceil(remain / 2.0);
    }
    return hexIChing(st, b[0], b[1], b[2], b[3], b[4], b[5]);
}

float randomIChing(in vec2 _st) { 
    return fract(sin(dot(_st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

vec4 renderIChingHexagrams(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    st.y *= uResolution.y / uResolution.x;

    // Grid density controlled by energy
    float gridScale = mix(5.0, 15.0, energy);
    st *= gridScale;
    vec2 fpos = fract(st);
    vec2 ipos = floor(st);

    // Animation speed controlled by tempo and bass
    float t = time * (5.0 + tempo * 5.0 + bass * 3.0);
    float df = 1.0;
    df = hexIChing(fpos, ipos.x + ipos.y + t * randomIChing(ipos)) + (1.0 - rectIChing(fpos, vec2(0.7)));

    vec3 color = mix(vec3(0.0), uPrimaryColor, step(0.7, df));
    
    // Add secondary color accents on high frequencies
    color = mix(color, uSecondaryColor, high * step(0.8, df) * 0.5);
    
    return vec4(color, 1.0);
}

#line 1 45
// @EFFECT name="Reflected Turbulence" index=67 desc="Mirrored turbulence noise patterns" author="kyndinfo"

vec3 mod289Turb(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289Turb(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permuteTurb(vec3 x) { return mod289Turb(((x*34.0)+1.0)*x); }

float snoiseTurb(vec2 v) {
    const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                        0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                        -0.577350269189626,  // -1.0 + 2.0 * C.x
                        0.024390243902439); // 1.0 / 41.0
    vec2 i  = floor(v + dot(v, C.yy) );
    vec2 x0 = v -   i + dot(i, C.xx);
    vec2 i1;
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod289Turb(i);
    vec3 p = permuteTurb( permuteTurb( i.y + vec3(0.0, i1.y, 1.0 ))
        + i.x + vec3(0.0, i1.x, 1.0 ));

    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
    m = m*m ;
    m = m*m ;
    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
    vec3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

float turbulenceReflect(vec2 st, float octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    for (int i = 0; i < 6; i++) {
        if (float(i) >= octaves) break;
        value += amplitude * abs(snoiseTurb(st));
        st *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

vec4 renderReflectedTurbulence(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    st.x *= uResolution.x / uResolution.y;
    
    // Reflection based on energy
    float reflectThreshold = mix(0.3, 0.7, energy);
    st.x = (st.x > reflectThreshold) ? st.x : 1.0 - st.x;
    st.y = (st.y > reflectThreshold) ? st.y : 1.0 - st.y;
    
    // Turbulence displace controlled by tempo and bass
    float displacement = mix(0.2, 0.8, tempo + bass);
    st.x += turbulenceReflect(st, 4.0 + mid * 2.0) * displacement * 0.5;
    st.y += turbulenceReflect(st + vec2(1.0), 4.0 + high * 2.0) * displacement * 0.2;
    
    // Scale controlled by energy
    float scale = mix(3.0, 10.0, energy);
    float v = turbulenceReflect(st * scale, 6.0);
    
    // Color mixing with palette
    vec3 color = mix(uSecondaryColor, uPrimaryColor, v);
    color += vec3(high * 0.3, mid * 0.2, bass * 0.3) * v;
    
    return vec4(color, 1.0);
}

#line 1 46
// @EFFECT name="Ikeda Data Stream" index=68 desc="Scrolling data stream with RGB offset" author="Patricio Gonzalez Vivo"

float randomDataStream(in float x) {
    return fract(sin(x) * 1e4);
}

float randomDataStream(in vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float patternDataStream(vec2 st, vec2 v, float t) {
    vec2 p = floor(st + v);
    return step(t, randomDataStream(100.0 + p * 0.000001) + randomDataStream(p.x) * 0.5);
}

vec4 renderIkedaDataStream(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    st.x *= uResolution.x / uResolution.y;

    // Grid density controlled by energy
    vec2 grid = vec2(mix(50.0, 150.0, energy), mix(25.0, 75.0, energy));
    st *= grid;

    vec2 ipos = floor(st);  // integer
    vec2 fpos = fract(st);  // fraction

    // Speed controlled by tempo and bass
    vec2 vel = vec2(time * 2.0 * max(grid.x, grid.y) * (1.0 + tempo));
    vel *= vec2(-1.0, 0.0) * randomDataStream(1.0 + ipos.y + bass); // direction

    // Assign a random value base on the integer coord
    vec2 offset = vec2(0.1, 0.0);

    // Use mid/high for threshold variation
    float threshold = 0.5 + mid * 0.3 + high * 0.2;

    vec3 color = vec3(0.0);
    color.r = patternDataStream(st + offset, vel, threshold);
    color.g = patternDataStream(st, vel, threshold);
    color.b = patternDataStream(st - offset, vel, threshold);

    // Margins
    color *= step(0.2, fpos.y);

    // Apply white color to active pixels, black background
    color = vec3(step(0.1, color.r + color.g + color.b));
    color *= vec3(1.0); // White data stream
    
    // Add subtle palette tint on high energy
    color = mix(color, uPrimaryColor, high * 0.3);

    return vec4(color, 1.0);
}

#line 1 47
// @EFFECT name="Loop Noise SDF" index=69 desc="Looping noise on radial SDF with smooth transitions" author="Will Stallwood"

vec2 random2SDF(vec2 st) {
    st = vec2(dot(st, vec2(127.1, 311.7)),
              dot(st, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(st) * 43758.5453123);
}

float v_noiseSDF(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(dot(random2SDF(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0)),
                   dot(random2SDF(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
               mix(dot(random2SDF(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
                   dot(random2SDF(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y);
}

float v_noiseSDF(vec2 st, float edges) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(dot(random2SDF(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0)),
                   dot(random2SDF(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
               mix(dot(random2SDF(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
                   dot(random2SDF(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y);
}

float stepUpDownSDF(float begin, float end, float t) {
    return step(begin, t) - step(end, t);
}

mat2 rotateSDF(float angle) {
    return mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

vec3 timeSDF(float u_time) {
    float period = mod(u_time, 5.0);
    vec3 t = vec3(fract(u_time/5.0), period, 1.0-fract(period));
    return t;
}

float loop_noiseSDF(vec2 st, float u_time) {
    float loopLength = 5.0;
    float transitionStart = 5.0 * 0.5;
    float t = mod(u_time, loopLength);
    float v1 = v_noiseSDF(st + t);
    float v2 = v_noiseSDF(st + t - loopLength);
    float transitionProgress = (t - transitionStart) / (loopLength - transitionStart);
    float progress = clamp(transitionProgress, 0.0, 1.0);
    return mix(v1, v2, progress);
}

vec4 renderLoopNoiseSDF(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    float seconds = 5.0;
    vec3 t = timeSDF(time);

    // Center and aspect ratio
    float aspect = uResolution.x / uResolution.y;
    st.x = st.x * aspect - aspect * 0.5 + 0.5;
    st = st * 2.0 - 1.0;

    // Rotation
    st *= rotateSDF(1.57079632675);

    // SDF
    vec2 pos = st - vec2(0.0);
    float r = 0.1;
    float a = atan(pos.y, pos.x);
    r = length(pos);
    r *= abs(cos(abs(a) * 2.0));

    t.x += fract(t.x - 0.5);
    float t1 = smoothstep(0.5, 1.0, t.x);
    float t2 = smoothstep(0.0, 0.5, t.x);

    float n = 0.5 * loop_noiseSDF(vec2(abs(a * 0.50), r) * 20.0, time);
    r += n;

    float sdf = r - 0.3;
    sdf = smoothstep(0.0, 0.001, sdf);

    // Color with palette support
    vec3 color = vec3(0.07);
    color = mix(color, vec3(1.0), sdf);
    color = mix(color, vec3(0.0), abs(st.y / 8.0));
    
    // Add palette colors
    color = mix(color, uPrimaryColor, bass * 0.5);
    color = mix(color, uSecondaryColor, high * 0.3);

    return vec4(color, 1.0);
}

#line 1 48
// @EFFECT name="Loop Noise Rays" index=70 desc="Looping noise radial rays on black background" author="Will Stallwood"

vec2 random2Rays(vec2 st) {
    st = vec2(dot(st, vec2(127.1, 311.7)),
              dot(st, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(st) * 43758.5453123);
}

float v_noiseRays(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(dot(random2Rays(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0)),
                   dot(random2Rays(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
               mix(dot(random2Rays(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
                   dot(random2Rays(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y);
}

float loop_noiseRays(vec2 st, float seconds, float time) {
    float loopLength = seconds;
    float transitionStart = seconds * 0.5;
    float t = mod(time, loopLength);

    float v1 = v_noiseRays(st + t);
    float v2 = v_noiseRays(st + t - loopLength);

    float transitionProgress = (t - transitionStart) / (loopLength - transitionStart);
    float progress = clamp(transitionProgress, 0.0, 1.0);
    return mix(v1, v2, progress);
}

mat2 rotateRays(float angle) {
    return mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

vec4 renderLoopNoiseRays(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Center and aspect ratio
    float aspect = uResolution.x / uResolution.y;
    st.x = st.x * aspect - aspect * 0.5 + 0.5;

    // Space
    st = st * 2.0 - 1.0;

    // Rotation based on tempo and time
    st *= rotateRays(1.57079632675 + time * 0.2 * tempo);

    // SDF
    vec2 pos = st;

    float r = 0.1;
    float a = atan(pos.y, pos.x);
    r = length(pos);
    r *= abs(cos(abs(a) * 2.0));

    // Noise controlled by energy
    float seconds = mix(3.0, 8.0, 1.0 - energy * 0.5);
    float n = 0.5 * loop_noiseRays(vec2(abs(a * 0.50), r) * mix(10.0, 30.0, energy), seconds, time);
    r += n;

    // SDF value - inverted for black background
    float sdf = r - mix(0.2, 0.5, bass);
    sdf = 1.0 - smoothstep(0.0, 0.001, sdf); // INVERTED: white rays on black

    // Color - black background with white rays
    vec3 color = vec3(0.0); // Black background
    color = mix(color, vec3(1.0), sdf); // White where active
    
    // Add palette color accents on high frequencies
    color = mix(color, uPrimaryColor, high * 0.5 * sdf);
    
    // Edge fade
    color = mix(color, vec3(0.0), abs(st.y / 8.0) * (1.0 - sdf));

    return vec4(color, 1.0);
}

#line 1 49
// @EFFECT name="Cell Rings" index=71 desc="Animated cellular ring patterns with noise" author="Will Stallwood"

float randomCell(vec2 st) {
    return fract(sin(dot(st.xy, vec2(-30.950, -10.810))) * 43758.5453123);
}

vec2 random2Cell(vec2 p) {
    return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}

float noiseCell(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    float a = randomCell(i);
    float b = randomCell(i + vec2(1.0, 0.0));
    float c = randomCell(i + vec2(0.0, 1.0));
    float d = randomCell(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

mat2 rotateCell(float angle) {
    return mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

float cellSDF(vec2 st, float time, float energy) {
    float seconds = mix(3.0, 8.0, 1.0 - energy * 0.5);
    float t = time / seconds;
    st = st * 2.0 - 1.0;
    vec2 r = random2Cell(st);
    float angle = 3.14159265359 * r.y;
    st *= rotateCell(angle + 6.283185307 * t * 40.0 * energy);
    float ring_size = mix(0.01 * sin(6.283185307 * t), 0.01 * sin(6.283185307 * t * 2.0) + 0.3 * floor(st.x * 10.0) / 10.0, cos(6.283185307 * t));
    float d = length(st) - 0.1 - 0.5 * sin(6.283185307 * log(t + 0.001));
    d = abs(d + ring_size * -abs(sin(6.283185307 * t * angle + t))) - ring_size;
    float g = st.y;
    d *= pow(abs(g), exp(sin(6.283185307 * t)));
    return d;
}

vec4 renderCellRings(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Center and aspect ratio
    float aspect = uResolution.x / uResolution.y;
    st.x = st.x * aspect - aspect * 0.5 + 0.5;

    // Timing
    float seconds = mix(4.0, 10.0, 1.0 - energy);
    float t = fract(time / seconds);

    // SDF
    float sdf = cellSDF(st, time, energy);

    // Color - black background with colored rings
    vec3 color = vec3(0.0);
    color = mix(color, uPrimaryColor, 1.0 - smoothstep(0.0, 0.002 * (1.0 + bass), sdf));

    return vec4(color, 1.0);
}

#line 1 50
// @EFFECT name="Corona Virus" index=72 desc="Raymarched virus particles with lipid bilayer" author="Martijn Steinrucken"

mat2 RotCV(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

float sminCV(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float sdCapsuleCV(vec3 p, vec3 a, vec3 b, float r) {
    vec3 ab = b - a;
    vec3 ap = p - a;
    float t = dot(ab, ap) / dot(ab, ab);
    t = clamp(t, 0.0, 1.0);
    vec3 c = a + t * ab;
    return length(p - c) - r;
}

float N31CV(vec3 p) {
    vec3 a = fract(vec3(p) * vec3(213.897, 653.453, 253.098));
    a += dot(a, a + 79.76);
    return fract(a.x * a.y * a.z);
}

float N21CV(vec2 p) {
    vec3 a = fract(vec3(p.xyx) * vec3(213.897, 653.453, 253.098));
    a += dot(a, a.yzx + 79.76);
    return fract((a.x + a.y) * a.z);
}

vec3 WorldToCubeCV(vec3 p) {
    vec3 ap = abs(p);
    vec3 sp = sign(p);
    float m = max(ap.x, max(ap.y, ap.z));
    vec3 st;
    if (m == ap.x)
        st = vec3(p.zy, 1.0 * sp.x);
    else if (m == ap.y)
        st = vec3(p.zx, 2.0 * sp.y);
    else
        st = vec3(p.xy, 3.0 * sp.z);
    st.xy /= m;
    st.xy *= (1.45109572583 - 0.451095725826 * abs(st.xy));
    return st;
}

float LipidCV(vec3 p, float twist, float scale) {
    vec3 n = sin(p * 20.0) * 0.2;
    p *= scale;
    p.xz *= RotCV(p.y * 0.3 * twist);
    p.x = abs(p.x);
    float d = length(p + n) - 2.0;
    float s = length(p.xz - vec2(1.5, 0.0)) - 0.05 * scale + max(0.4, p.y);
    d = sminCV(d, s * 0.9, 0.4);
    return d / scale;
}

float sdTentacleCV(vec3 p) {
    float offs = sin(p.x * 50.0) * sin(p.y * 30.0) * sin(p.z * 20.0);
    p.x += sin(p.y * 10.0 + uTime) * 0.02;
    p.y *= 0.2;
    float d = sdCapsuleCV(p, vec3(0.0, 0.1, 0.0), vec3(0.0, 0.8, 0.0), 0.04);
    p.xz = abs(p.xz);
    d = min(d, sdCapsuleCV(p, vec3(0.0, 0.8, 0.0), vec3(0.1, 0.9, 0.1), 0.01));
    d += offs * 0.01;
    return d;
}

float ParticleCV(vec3 p, float scale, float amount, float time) {
    vec3 st = WorldToCubeCV(p);
    vec3 cPos = vec3(st.x, length(p), st.y);
    vec3 tPos = cPos;
    cPos.xz *= scale;
    vec2 uv = fract(cPos.xz) - 0.5;
    vec2 id = floor(cPos.xz);
    float n = N21CV(id);
    float t = (time + st.z + n * 123.32) * 1.3;
    float wobble = sin(t) + sin(1.3 * t) * 0.4;
    wobble /= 1.4;
    wobble = pow(abs(wobble), 3.0);
    wobble *= amount / scale;
    vec3 ccPos = vec3(uv.x, cPos.y, uv.y);
    vec3 sPos = vec3(0.0, 3.5 + wobble, 0.0);
    vec3 pos = ccPos - sPos;
    pos.y *= scale / 2.0;
    float d = LipidCV(pos, n, 10.0) / scale;
    d = min(d, length(p) - 0.2 * scale);
    float tent = sdTentacleCV(tPos);
    d = min(d, tent);
    return d;
}

float GetDistCV(vec3 p, float time, float energy, float bass) {
    float scale = 8.0;
    p.z += time * (0.5 + bass);
    vec3 id = floor(p / 10.0);
    p = mod(p, vec3(10.0)) - 5.0;
    float n = N21CV(id.xz);
    p.xz *= RotCV(time * 0.2 * (n - 0.5));
    p.yz *= RotCV(time * 0.2 * (N21CV(id.zx) - 0.5));
    scale = mix(4.0, 16.0, N21CV(id.xz));
    n = N31CV(id);
    float filledCells = 0.3 + energy * 0.4;
    if (n > filledCells) {
        return max(0.0, 5.0 - max(p.x, max(p.y, p.z))) + 0.1;
    }
    p += sin(p.x + time) * 0.1 + sin(p.y * p.z + time) * 0.05;
    float surf = sin(scale + time * 0.2) * 0.5 + 0.5;
    surf *= surf;
    surf *= 4.0;
    surf += 2.0;
    float d = ParticleCV(p, scale, surf, time);
    p.xz *= RotCV(0.78 + time * 0.08);
    p.zy *= RotCV(0.5);
    d = sminCV(d, ParticleCV(p, scale, surf, time), 0.02);
    return d;
}

float RayMarchCV(vec3 ro, vec3 rd, float time, float energy, float bass) {
    float dO = 0.0;
    float cone = 0.0005;
    for (int i = 0; i < 100; i++) {
        vec3 p = ro + rd * dO;
        float dS = GetDistCV(p, time, energy, bass);
        dO += dS;
        if (dO > 40.0 || abs(dS) < 0.01 + dO * cone) break;
    }
    return dO;
}

vec3 GetNormalCV(vec3 p, float time, float energy, float bass) {
    float d = GetDistCV(p, time, energy, bass);
    vec2 e = vec2(0.001, 0.0);
    vec3 n = d - vec3(
        GetDistCV(p - e.xyy, time, energy, bass),
        GetDistCV(p - e.yxy, time, energy, bass),
        GetDistCV(p - e.yyx, time, energy, bass));
    return normalize(n);
}

vec3 RCV(vec2 uv, vec3 p, vec3 l, vec3 up, float z) {
    vec3 f = normalize(l - p);
    vec3 r = normalize(cross(up, f));
    vec3 u = cross(f, r);
    vec3 c = p + f * z;
    vec3 i = c + uv.x * r + uv.y * u;
    vec3 d = normalize(i - p);
    return d;
}

vec4 renderCoronaVirus(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = (st - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    
    float t = time * (1.0 + tempo);
    
    vec3 col = vec3(0.0);
    
    vec3 ro = vec3(0.0, 0.0, -1.0);
    ro.yz *= RotCV(-0.0);
    ro.xz *= RotCV(time * 0.05 * energy);
    
    vec3 up = vec3(0.0, 1.0, 0.0);
    up.xy *= RotCV(sin(t * 0.1));
    vec3 rd = RCV(uv, ro, vec3(0.0, 0.0, 0.0), up, 0.5);
    
    ro.x += 5.0;
    ro.xy *= RotCV(t * 0.1);
    ro.xy -= 5.0;
    
    float d = RayMarchCV(ro, rd, t, energy, bass);
    
    float bg = rd.y * 0.5 + 0.3;
    float poleDist = length(rd.xz);
    float poleMask = smoothstep(0.5, 0.0, poleDist);
    bg += sign(rd.y) * poleMask;
    
    float a = atan(rd.x, rd.z);
    bg += (sin(a * 5.0 + t + rd.y * 2.0) + sin(a * 7.0 - t + rd.y * 2.0)) * 0.2;
    float rays = (sin(a * 5.0 + t * 2.0 + rd.y * 2.0) * sin(a * 37.0 - t + rd.y * 2.0)) * 0.5 + 0.5;
    bg *= mix(1.0, rays, 0.25 * poleDist * (sin(t * 0.1) * 0.5 + 0.5));
    col += bg;
    
    if (d < 40.0) {
        vec3 p = ro + rd * d;
        vec3 n = GetNormalCV(p, t, energy, bass);
        p = mod(p, vec3(10.0)) - 5.0;
        float ao = smoothstep(2.96, 3.7, length(p));
        col += (n.y * 0.5 + 0.5) * ao * 2.0;
        col *= smoothstep(-1.0, 6.0, p.y);
        
        // Add palette color on high frequencies
        col = mix(col, uPrimaryColor, high * 0.3);
    }
    
    col = mix(col, vec3(bg), smoothstep(0.0, 40.0, d));
    
    // Color grading
    col *= mix(vec3(1.0, 0.9, 0.8), uSecondaryColor, mid * 0.3);
    
    return vec4(col, 1.0);
}

#line 1 51
// @EFFECT name="Sphere Raytrace" index=73 desc="Raytraced spheres with fractal textures and reflections" author="Jordan Duty"

#define ptpi 1385.4557313670110891409199368797
#define pipi 36.462159692
#define picu 31.006276680299820175476315067101
#define pepi 23.140692632779269005729086367949
#define chpi 11.59195327552152062775175205256
#define shpi 11.548739357257748377977334315388
#define pisq 9.8696044010893586188344909998762
#define twpi 6.2831853075286766559
#define pi 3.1415926535897932384626433832795
#define sqpi 1.7724538509055160272981674833411
#define hfpi 1.5707963267948966192313216916398
#define cupi 1.4645918875615232630201425272638
#define prpi 1.4396194958475906883364908049738
#define lnpi 1.1447298858494001741434273513531
#define trpi 1.0471975511965977461542144610932
#define thpi 0.99627207622074994426469058001254
#define lgpi 0.4971498726941338543512682882909
#define rcpi 0.31830988618379067153776752674503
#define rcpipi 0.0274256931232981061195562708591

#define useFractal fractal2ColorRT

#define iterationsRT 10
#define formuparamRT 0.42
#define volstepsRT 10
#define stepsizeRT 0.120
#define zoomRT 0.1000
#define tileRT 0.1120
#define speedRT 0.00100
#define brightnessRT 0.001
#define darkmatterRT 0.500
#define distfadingRT 0.120
#define saturationRT -0.900

vec3 universeFractalRT(vec2 surfacePos, float time) {
    vec2 uv = surfacePos;
    vec3 dir = vec3(uv * zoomRT, 10.0);
    float a2 = time / 10.0 * speedRT;
    float a1 = 10.0;
    mat2 rot1 = mat2(cos(a1), sin(a1), -sin(a1), cos(a1));
    mat2 rot2 = rot1;
    dir.xz *= rot1;
    dir.xy *= rot2;
    vec3 from = vec3(0.0, 0.0, 0.0);
    from += vec3(0.001 * time, 0.001 * time, -2.0);
    from.xz *= rot1;
    from.xy *= rot2;
    float s = 0.4, fade = 0.2;
    vec3 v = vec3(0.4);
    for (int r = 0; r < volstepsRT; r++) {
        vec3 p = from + s * dir * 0.5;
        p = abs(vec3(tileRT) - mod(p, vec3(tileRT * 2.0)));
        float pa, a = pa = 0.0;
        for (int i = 0; i < iterationsRT; i++) {
            p = abs(p) / dot(p, p) - 1.1 * formuparamRT;
            a += abs(length(p) - pa);
            pa = length(p);
        }
        float dm = max(0.0, darkmatterRT - a * a * 0.001);
        a *= a * a * 2.0;
        if (r > 3) fade *= 1.0 - dm;
        v += fade;
        v += vec3(s, s * s, s * s * s * s) * a * brightnessRT * fade;
        fade *= distfadingRT;
        s += stepsizeRT;
    }
    v = mix(vec3(length(v)), v, saturationRT);
    return vec3(v * 0.01);
}

#define fractal_detailsRT 10
#define zoomoutRT 1.0

vec3 fractal2ColorRT(vec2 surfacePos, float time) {
    vec2 p = surfacePos * zoomoutRT;
    vec3 c = vec3(0.0);
    vec2 fractal;
    float deepfade = 1.0;
    for (int i = 0; i < fractal_detailsRT; i++) {
        deepfade *= 0.5;
        fractal = abs(p) / dot(p, p) - 1.0 + sin(time * 0.5) * 0.5;
        vec2 pdiff = fractal - p;
        c.rg += pdiff * deepfade;
        c.b += abs(length(pdiff)) * deepfade;
        p = fractal;
    }
    return c;
}

struct RayRT {
    vec3 Dir;
    vec3 Pos;
};

struct SphereRT {
    vec3 Pos;
    vec3 Color;
    float Rad;
    float Reflection;
};

vec3 LightPosRT = vec3(0.0, -3.0, 10.0);

vec3 IntersectsRT(SphereRT s, RayRT r) {
    vec3 l = s.Pos - r.Pos;
    float tca = dot(l, r.Dir);
    if (tca < 0.0) return vec3(0.0, 0.0, -1.0);
    float d2 = dot(l, l) - tca * tca;
    if (d2 > s.Rad * s.Rad) return vec3(0.0, 0.0, -1.0);
    float thc = sqrt((s.Rad * s.Rad) - d2);
    return vec3(tca - thc, tca + thc, 1.0);
}

vec3 Trace3RT(RayRT r, SphereRT spheres[7], float time) {
    vec3 Color = vec3(0.0, 0.0, 0.0);
    SphereRT s;
    bool col = false;
    float tnear = 1e8;
    for (int i = 0; i < 7; i++) {
        vec3 intTest = IntersectsRT(spheres[i], r);
        if (intTest.z != -1.0) {
            if (intTest.x < tnear) {
                tnear = intTest.x;
                s = spheres[i];
                col = true;
            }
        }
    }
    if (col == false) return vec3(0.0, 0.0, 0.0);
    vec3 phit = r.Pos + r.Dir * tnear;
    float spaceScale = 0.1;
    Color += (useFractal(phit.xy * spaceScale, time) + useFractal(phit.xz * spaceScale, time) + useFractal(phit.yz * spaceScale, time)) / 3.0;
    vec3 nhit = phit - s.Pos;
    nhit = normalize(nhit);
    vec3 lightDir = LightPosRT - phit;
    bool blocked = false;
    lightDir = normalize(lightDir);
    float DiffuseFactor = dot(nhit, lightDir);
    vec3 diffuseColor = vec3(0.0, 0.0, 0.0);
    vec3 ambientColor = vec3(s.Color * 0.2);
    for (int n = 0; n < 7; n++) {
        RayRT rl;
        rl.Pos = phit;
        rl.Dir = lightDir;
        vec3 intTestL = IntersectsRT(spheres[n], rl);
        if (intTestL.z != -1.0) {
            if (intTestL.x < length(LightPosRT - phit)) {
                blocked = true;
            }
        }
    }
    if (!blocked) {
        if (DiffuseFactor > 0.0) {
            diffuseColor = vec3(1.0, 1.0, 1.0) * DiffuseFactor;
            Color += s.Color * diffuseColor + ambientColor;
        } else {
            Color += ambientColor;
        }
    } else {
        Color += ambientColor;
    }
    return Color;
}

vec3 Trace2RT(RayRT r, SphereRT spheres[7], float time) {
    vec3 Color = vec3(0.0, 0.0, 0.0);
    SphereRT s;
    bool col = false;
    float tnear = 1e8;
    for (int i = 0; i < 7; i++) {
        vec3 intTest = IntersectsRT(spheres[i], r);
        if (intTest.z != -1.0) {
            if (intTest.x < tnear) {
                tnear = intTest.x;
                s = spheres[i];
                col = true;
            }
        }
    }
    if (col == false) return vec3(0.0, 0.0, 0.0);
    vec3 phit = r.Pos + r.Dir * tnear;
    vec3 nhit = phit - s.Pos;
    nhit = normalize(nhit);
    if (dot(r.Dir, nhit) > 0.0) nhit *= -1.0;
    if (s.Reflection > 0.0) {
        float facingratio = dot((r.Dir * -1.0), nhit);
        vec3 refldir = r.Dir - nhit * 2.0 * dot(r.Dir, nhit);
        refldir = normalize(refldir);
        RayRT rd;
        rd.Pos = phit;
        rd.Dir = refldir;
        vec3 refl = Trace3RT(rd, spheres, time);
        float param1 = (1.0 - s.Reflection);
        float param2 = s.Reflection;
        Color.x = (param1 * Color.x + param2 * refl.x);
        Color.y = (param1 * Color.y + param2 * refl.y);
        Color.z = (param1 * Color.z + param2 * refl.z);
    }
    vec3 lightDir = LightPosRT - phit;
    bool blocked = false;
    lightDir = normalize(lightDir);
    float DiffuseFactor = dot(nhit, lightDir);
    vec3 diffuseColor = vec3(0.0, 0.0, 0.0);
    vec3 ambientColor = vec3(s.Color * 0.2);
    for (int n = 0; n < 7; n++) {
        RayRT rl;
        rl.Pos = phit;
        rl.Dir = lightDir;
        vec3 intTestL = IntersectsRT(spheres[n], rl);
        if (intTestL.z != -1.0) {
            if (intTestL.x < length(LightPosRT - phit))
                blocked = true;
        }
    }
    if (!blocked) {
        if (DiffuseFactor > 0.0) {
            diffuseColor = vec3(1.0, 1.0, 1.0) * DiffuseFactor;
            Color += s.Color * diffuseColor + ambientColor;
        } else {
            Color += ambientColor;
        }
    } else {
        Color += ambientColor;
    }
    return Color;
}

vec3 spukeRT(vec3 pos, float time) {
    vec2 p = ((pos.z) + (sin((((length(sin((pos.xy) + pos.z * pi))) + (cos((pos.z * pi) / pi))))))) + pos.xy * pos.z;
    vec3 col = vec3(0.0, 0.0, 0.0);
    float ca = 0.0;
    for (int j = 1; j < 8; j++) {
        p *= 1.4;
        float jj = float(j);
        for (int i = 1; i < 8; i++) {
            vec2 newp = p * 0.96;
            float ii = float(i);
            newp.x += 1.2 / (ii + jj) * sin(ii * p.y + (p.x * 0.3) + cos(pos.z / pi / pi) * pi * pi + 0.003 * (jj / ii)) + 1.0;
            newp.y += 0.8 / (ii + jj) * cos(ii * p.x + (p.y * 0.3) + sin(pos.z / pi / pi) * pi * pi + 0.003 * (jj / ii)) - 1.0;
            p = newp;
        }
        p *= 0.9;
        col += vec3(0.5 * sin(pi * p.x) + 0.5, 0.5 * sin(pi * p.y) + 0.5, 0.5 * sin(pi * p.x) * cos(pi * p.y) + 0.5) * (0.5 * sin(pos.z * pi) + 0.5);
        ca += 0.7;
    }
    col /= ca;
    return vec3(col * col * col);
}

vec3 TraceRT(RayRT r, SphereRT spheres[7], float time) {
    vec3 Color = vec3(0.0, 0.0, 0.0);
    SphereRT s;
    bool col = false;
    float tnear = 1e8;
    for (int i = 0; i < 7; i++) {
        vec3 intTest = IntersectsRT(spheres[i], r);
        if (intTest.z != -1.0) {
            if (intTest.x < tnear) {
                tnear = intTest.x;
                s = spheres[i];
                col = true;
            }
        }
    }
    if (col == false) return vec3(0.0, 0.0, 0.0);
    vec3 phit = r.Pos + r.Dir * tnear;
    vec3 nhit = phit - s.Pos;
    nhit = normalize(nhit);
    if (dot(r.Dir, nhit) > 0.0) nhit *= -1.0;
    if (s.Reflection > 0.0) {
        float facingratio = dot((r.Dir * -1.0), nhit);
        vec3 refldir = r.Dir - nhit * 2.0 * dot(r.Dir, nhit);
        refldir = normalize(refldir);
        RayRT rd;
        rd.Pos = phit;
        rd.Dir = refldir;
        vec3 refl = Trace2RT(rd, spheres, time);
        float param1 = (1.0 - s.Reflection);
        float param2 = s.Reflection;
        Color.x = (param1 * Color.x + param2 * refl.x);
        Color.y = (param1 * Color.y + param2 * refl.y);
        Color.z = (param1 * Color.z + param2 * refl.z);
    }
    vec3 lightDir = LightPosRT - phit;
    bool blocked = false;
    lightDir = normalize(lightDir);
    float DiffuseFactor = dot(nhit, lightDir);
    vec3 diffuseColor = vec3(0.0, 0.0, 0.0);
    vec3 ambientColor = vec3(s.Color * 0.2);
    for (int n = 0; n < 7; n++) {
        RayRT rl;
        rl.Pos = phit;
        rl.Dir = lightDir;
        vec3 intTestL = IntersectsRT(spheres[n], rl);
        if (intTestL.z != -1.0) {
            if (intTestL.x < length(LightPosRT - phit))
                blocked = true;
        }
    }
    if (!blocked) {
        if (DiffuseFactor > 0.0) {
            diffuseColor = vec3(1.0, 1.0, 1.0) * DiffuseFactor;
            Color += s.Color * diffuseColor + ambientColor;
        } else {
            Color += ambientColor;
        }
    } else {
        Color += ambientColor;
    }
    return mix(Color, spukeRT(Color * pi, time), 0.5 + sin(time / pi) * 0.25);
}

vec4 renderSphereRaytrace(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    SphereRT spheres[7];
    spheres[0].Pos = vec3(5.0, 0.0, -0.0);
    spheres[0].Color = uPrimaryColor;
    spheres[0].Rad = 4.0;
    spheres[0].Reflection = 0.4;
    spheres[1].Pos = vec3(-5.0, 0.0, -0.0);
    spheres[1].Color = uSecondaryColor;
    spheres[1].Rad = 4.0;
    spheres[1].Reflection = 0.3;
    spheres[2].Pos = vec3(-5.0, 1004.0, -0.0);
    spheres[2].Color = vec3(0.0, 0.0, 0.0);
    spheres[2].Rad = 1000.0;
    spheres[2].Reflection = 0.5;
    spheres[3].Pos = vec3(-5.0, 0.0, -1040.0);
    spheres[3].Color = vec3(0.9, 0.0, 0.0);
    spheres[3].Rad = 1000.0;
    spheres[3].Reflection = 0.505;
    spheres[4].Pos = vec3(1020.0, 0.0, -0.0);
    spheres[4].Color = vec3(0.0, 0.9, 0.0);
    spheres[4].Rad = 1000.0;
    spheres[4].Reflection = 0.505;
    spheres[5].Pos = vec3(-1020.0, 0.0, -0.0);
    spheres[5].Color = vec3(0.0, 0.0, 0.7);
    spheres[5].Rad = 1000.0;
    spheres[5].Reflection = 0.505;
    spheres[6].Pos = vec3(-5.0, 0.0, 1040.0);
    spheres[6].Color = vec3(0.9, 0.9, 0.0);
    spheres[6].Rad = 1000.0;
    spheres[6].Reflection = 0.505;
    
    float invWidth = 1.0 / uResolution.x;
    float invHeight = 1.0 / uResolution.y;
    float fov = 60.0 + energy * 20.0;
    float aspectratio = uResolution.x / uResolution.y;
    float angle = tan(pi * 0.5 * fov / 180.0);
    
    vec2 coord = st * uResolution.xy;
    float camSpeed = 0.5 * (1.0 + tempo);
    vec3 camTrans = vec3(20.0 * cos(camSpeed * time), -5.0, 20.0 * sin(camSpeed * time));
    vec3 camDir = camTrans - vec3(0.0);
    LightPosRT.y = -10.0;
    mat3 rot;
    vec3 f = normalize(camTrans);
    vec3 u = vec3(0.0, 1.0, 0.0);
    vec3 s = normalize(cross(f, u));
    u = cross(s, f);
    rot[0][0] = s.x; rot[1][0] = s.y; rot[2][0] = s.z;
    rot[0][1] = u.x; rot[1][1] = u.y; rot[2][1] = u.z;
    rot[0][2] = f.x; rot[1][2] = f.y; rot[2][2] = f.z;
    RayRT R;
    float xx = (2.0 * ((coord.x + 0.5) * invWidth) - 1.0) * angle * aspectratio;
    float yy = (1.0 - 2.0 * ((coord.y + 0.5) * invHeight)) * angle;
    R.Pos = camTrans;
    R.Dir = vec3(xx, yy, -1.0) * rot;
    R.Dir = normalize(R.Dir);
    
    vec3 color = TraceRT(R, spheres, time);
    
    // Add audio reactivity
    color += uPrimaryColor * bass * 0.2;
    color = mix(color, uSecondaryColor, high * 0.15);
    
    return vec4(color, 1.0);
}

#line 1 52
// @EFFECT name="Tiles and Numbers" index=74 desc="Animated tiles with numbers, fractals, truchet and droste patterns" author="Shane/ikr7/FabriceNeyret2"

#define RotTiles(a) mat2(cos(a),-sin(a),sin(a),cos(a))
#define BTiles(p,s) max(abs(p).x-s.x,abs(p).y-s.y)
#define deg45Tiles .707
#define R45Tiles(p) (( p + vec2(p.y,-p.x) ) *deg45Tiles)
#define TriTiles(p,s) max(R45Tiles(p).x,max(R45Tiles(p).y,BTiles(p,s)))
#define LINE_SIZE_TILES 0.05

vec2 _uvTiles;

float randomTiles(vec2 p) {
    return fract(sin(dot(p.xy, vec2(12.9898,78.233)))* 43758.5453123);
}

float lineToTiles(vec2 p, vec2 a, vec2 b){
    return distance(p,mix(a,b,clamp(dot(p-a,b-a)/dot(b-a,b-a),0.0,1.0)));
}

float c0Tiles(vec2 p){
    vec2 prevP = p;
    p.x = abs(p.x);
    float d = lineToTiles(p,vec2(0.3,0.4),vec2(0.3,-0.4));
    p = prevP;
    p.y = abs(p.y);
    float d2 = lineToTiles(p,vec2(-0.3,0.4),vec2(0.3,0.4));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c1Tiles(vec2 p){
    float d = lineToTiles(p,vec2(-0.15,0.4),vec2(0.0,0.4));
    float d2 = lineToTiles(p,vec2(0.0,0.4),vec2(0.0,-0.4));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c2Tiles(vec2 p){
    float d = lineToTiles(p,vec2(0.3,0.4),vec2(0.3,0.0));
    float d2 = lineToTiles(p,vec2(0.3,0.0),vec2(0.1,0.0));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(-0.3,0.0),vec2(-0.1,0.0));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(-0.3,0.0),vec2(-0.3,-0.4));
    d = min(d,d2);
    p.y = abs(p.y);
    d2 = lineToTiles(p,vec2(-0.3,0.4),vec2(0.3,0.4));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c3Tiles(vec2 p){
    float d = lineToTiles(p,vec2(0.3,0.4),vec2(0.3,-0.4));
    float d2 = lineToTiles(p,vec2(0.3,0.0),vec2(0.0,0.0));
    d = min(d,d2);
    p.y = abs(p.y);
    d2 = lineToTiles(p,vec2(-0.3,0.4),vec2(0.3,0.4));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c4Tiles(vec2 p){
    float d = lineToTiles(p,vec2(0.0,0.4),vec2(-0.3,-0.25));
    float d2 = lineToTiles(p,vec2(-0.3,-0.25),vec2(0.3,-0.25));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(0.2,-0.1),vec2(0.2,-0.4));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(0.2,-0.1),vec2(0.0,-0.1));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c5Tiles(vec2 p){
    p.x *= -1.0;
    return c2Tiles(p);
}

float c6Tiles(vec2 p){
    float d = lineToTiles(p,vec2(-0.3,0.4),vec2(0.2,0.4));
    float d2 = lineToTiles(p,vec2(-0.3,0.4),vec2(-0.3,-0.4));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(-0.3,-0.4),vec2(-0.2,-0.4));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(0.0,-0.4),vec2(0.3,-0.4));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(0.3,-0.4),vec2(0.3,0.0));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(0.3,0.0),vec2(-0.2,0.0));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c7Tiles(vec2 p){
    float d = lineToTiles(p,vec2(-0.3,0.4),vec2(0.3,0.4));
    float d2 = lineToTiles(p,vec2(0.3,0.4),vec2(-0.3,-0.4));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c8Tiles(vec2 p){
    float d = lineToTiles(p,vec2(-0.3,0.4),vec2(-0.3,0.0));
    float d2 = lineToTiles(p,vec2(-0.3,0.0),vec2(0.2,0.0));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(0.3,0.3),vec2(0.3,-0.4));
    d = min(d,d2);
    d2 = lineToTiles(p,vec2(-0.3,-0.1),vec2(-0.3,-0.4));
    d = min(d,d2);
    p.y = abs(p.y);
    d2 = lineToTiles(p,vec2(-0.3,0.4),vec2(0.3,0.4));
    d = min(d,d2);
    return d - LINE_SIZE_TILES;
}

float c9Tiles(vec2 p){
    p *= -1.0;
    return c6Tiles(p);
}

float drawNumberTiles(vec2 p, int d){
    return d==0 ? c0Tiles(p)
         : d==1 ? c1Tiles(p)
         : d==2 ? c2Tiles(p)
         : d==3 ? c3Tiles(p)
         : d==4 ? c4Tiles(p)
         : d==5 ? c5Tiles(p)
         : d==6 ? c6Tiles(p)
         : d==7 ? c7Tiles(p)
         : d==8 ? c8Tiles(p)
         : d==9 ? c9Tiles(p)
         : 10.0;
}

float arrowTiles(vec2 p){
    p.y -= 0.05;
    float d = TriTiles(p,vec2(0.1));
    p.y += 0.05;
    float d2 = TriTiles(p,vec2(0.1));
    d = max(-d2,d);
    d2 = length(p-vec2(0.0,-0.03))-0.015;
    d = min(d,d2);
    return d;
}

float arrowsTiles(vec2 p, float n, float time){
    p.y -= time*clamp((0.1+n),0.5,1.0)*0.3;
    p.y = mod(p.y,0.2)-0.1;
    p.y += 0.01;
    float d = arrowTiles(p);
    return d;
}

float fractalTiles(vec2 p, float n, float time){
    vec2 prevP = p;
    float d = 10.0;
    for(float i = 1.0; i<4.0; i+=1.0){
        p *= RotTiles(radians(n+i*30.0*time));
        p = abs(p)-i*0.1;
        p *= RotTiles(radians(n+i*30.0));
        float d2 = arrowsTiles(p,n,time);
        d = min(d,d2);
    }
    p = prevP;
    d = max(BTiles(p,vec2(0.45)),d);
    return d;
}

float truchetTiles(vec2 p, float n, float time){
    vec2 prevP = p;
    p -= time*0.1+n;
    p *= 5.0;
    vec2 id = floor(p);
    vec2 gr = fract(p)-0.5;
    float n2 = randomTiles(id);
    float r = 45.0;
    if(n2>=0.25 && n2 < 0.5){
        r = -45.0;
    } else if(n2>=0.5 && n2 < 0.75){
        r = -135.0;
    } else if(n2>=0.75){
        r = 135.0;
    }
    float a = radians(r);
    float d = dot(gr,vec2(cos(a),sin(a)));
    p = prevP;
    d = max(BTiles(p,vec2(0.45)), d*0.1);
    return d;
}

vec2 clogTiles(vec2 z) {
    return vec2(log(length(z)), atan(z.y, z.x));
}

vec2 drosteUVTiles(vec2 p, float n, float time){
    float speed = 0.25+n;
    float animate = mod(time*speed,2.07);
    float rate = sin(time*0.5);
    p = clogTiles(p)*mat2(1.0,0.11,rate*0.5,1.0);
    p = exp(p.x-animate) * vec2(cos(p.y), sin(p.y));
    vec2 c = abs(p);
    vec2 duv = 0.5+p*exp2(ceil(-log2(max(c.y,c.x))-2.0));
    return duv;
}

vec2 pmodTiles(vec2 p, float s, float space){
    float modVal = s*(2.0+space);
    p = mod(p,modVal)-(modVal*0.5);
    return p;
}

float drosteCirclesTiles(vec2 p, float s, float space, float n, float time){
    vec2 prevP = p;
    p = drosteUVTiles(p,n,time);
    p = pmodTiles(p,s,space);
    p *= RotTiles(radians(time*30.0));
    float d = abs(length(p)-s)-0.02;
    d = max(-(abs(p.x)-0.02),d);
    p = prevP;
    d = max(BTiles(p,vec2(0.45)),d);
    return d;
}

vec2 mobiusLogUVTiles(vec2 uv, float time) {
    vec2 z = uv - vec2(-1.0, 0.0);
    uv.x -= 0.5;
    uv *= mat2(z, -z.y, z.x) / dot(uv, uv);
    _uvTiles = log(length(uv + 0.5)) * vec2(0.5, -0.5);
    uv = log(length(uv += 0.5)) * vec2(0.5, -0.5) + atan(uv.y, uv.x) / 6.2831853 * vec2(3.0, 1.0);
    return uv;
}

float tilesTiles(vec2 p, float time){
    p *= 4.0;
    vec2 id = floor(p);
    vec2 gr = fract(p)-0.5;
    float n = randomTiles(id);
    float n2 = randomTiles(id)*10.0;
    float d = 10.0;
    if(n<0.25){
        d = fractalTiles(gr,n,time);
    } else if(n>0.25 && n<0.5){
        d = truchetTiles(gr,n,time);
    } else if(n>0.5 && n<0.8){
        d = drawNumberTiles(gr,int(mod(time+n2,10.0)));
    } else if(n>0.8){
        d = drosteCirclesTiles(gr,0.1,0.5,n*0.3,time);
    }
    return d;
}

float renderTiles(vec2 p, float time){
    p.y += time*0.2;
    vec2 gr = fract(p)-0.5;
    float d = tilesTiles(gr,time);
    return d;
}

vec4 renderTilesNumbers(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec2 uv = (st - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    vec2 p = uv;
    uv = mobiusLogUVTiles(uv, time);
    float d = renderTiles(uv, time);
    float w = length(fwidth(_uvTiles)) * 1.5;
    float aa = smoothstep(-w, w, d);
    // Pure monochrome - white background with black lines
    vec3 col = vec3(1.0); // Pure white background
    col = mix(col, vec3(0.0), aa); // Pure black lines
    return vec4(col, 1.0);
}

#line 1 53
// @EFFECT name="Audio Spectrum EQ" index=75 desc="Fullscreen 20Hz-18kHz spectrum analyzer like Pulse Audio Easy Effects" author="Visualizer"

// Fullscreen 20Hz to 18kHz spectrum analyzer
// 64 bands covering the full audible spectrum

float eqRect(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// Log frequency to linear position (20Hz - 18kHz)
float freqToX(float freq) {
    float minLog = log2(20.0);
    float maxLog = log2(18000.0);
    return (log2(freq) - minLog) / (maxLog - minLog);
}

// Map band index to frequency
float bandToFreq(int band, int totalBands) {
    float t = float(band) / float(totalBands - 1);
    float minLog = log2(20.0);
    float maxLog = log2(18000.0);
    return pow(2.0, mix(minLog, maxLog, t));
}

vec4 renderAudioEQ(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Fullscreen - use full UV range
    vec2 uv = (st - 0.5) * 2.0; // -1 to 1 range
    
    // Dark background
    vec3 col = vec3(0.005, 0.005, 0.008);
    
    // 64 frequency bands for smooth spectrum (20Hz - 18kHz)
    const int numBands = 64;
    float totalWidth = 2.0; // Full width (-1 to 1)
    float gap = 0.003;
    float barW = (totalWidth / float(numBands)) - gap;
    float spacing = barW + gap;
    float startX = -1.0 + barW * 0.5;
    float maxHeight = 1.8; // Taller bars for fullscreen
    float baseY = -0.9;
    
    // Fallback animation
    float t = time * 2.5;
    float anim[64];
    for (int i = 0; i < 64; i++) {
        // Different frequencies for each band
        float freq = 0.3 + float(i) * 0.05;
        // Simulate spectrum: more activity in mids
        float activity = 1.0 - abs(float(i) - 32.0) / 32.0;
        anim[i] = 0.05 + activity * 0.25 + sin(t * freq) * 0.1 * activity;
    }
    
    // Audio inputs with minimums
    float b = max(bass, 0.1);
    float m = max(mid, 0.08);
    float h = max(high, 0.06);
    float e = max(energy, 0.15);
    
    // Generate 64 frequency bands (20Hz - 18kHz)
    float bands[64];
    
    // Fill bands based on frequency distribution
    for (int i = 0; i < 64; i++) {
        float f = bandToFreq(i, 64);
        float normPos = float(i) / 63.0;
        
        // Frequency ranges:
        // 20-60 Hz: Sub-bass (bands 0-5)
        // 60-250 Hz: Bass (bands 6-15)
        // 250-500 Hz: Low-mids (bands 16-23)
        // 500 Hz - 2 kHz: Mids (bands 24-39)
        // 2-6 kHz: High-mids (bands 40-51)
        // 6-18 kHz: Highs (bands 52-63)
        
        float intensity;
        if (f < 60.0) {
            // Sub-bass
            intensity = b * (1.0 - float(i) * 0.08) + e * 0.1;
        } else if (f < 250.0) {
            // Bass
            float blend = (f - 60.0) / 190.0;
            intensity = mix(b * 0.9, b * 0.5, blend) + m * blend * 0.2 + e * 0.08;
        } else if (f < 500.0) {
            // Low-mids
            float blend = (f - 250.0) / 250.0;
            intensity = mix(b * 0.4, m * 0.7, blend) + e * 0.06;
        } else if (f < 2000.0) {
            // Mids
            float blend = (f - 500.0) / 1500.0;
            intensity = mix(m * 0.75, m * 0.95, blend) + h * blend * 0.15 + e * 0.05;
        } else if (f < 6000.0) {
            // High-mids
            float blend = (f - 2000.0) / 4000.0;
            intensity = mix(m * 0.5, h * 0.85, blend) + e * 0.07;
        } else {
            // Highs
            float blend = (f - 6000.0) / 12000.0;
            intensity = h * (1.0 - blend * 0.3) + e * 0.04;
        }
        
        // Add temporal variation
        float wave = sin(time * 3.0 + float(i) * 0.2) * 0.02;
        float fallback = anim[i] * 0.5;
        bands[i] = clamp(max(intensity + wave, fallback), 0.0, 1.0);
    }
    
    // Draw frequency bars
    for (int i = 0; i < numBands; i++) {
        float xPos = startX + float(i) * spacing;
        float barHeight = bands[i] * maxHeight;
        
        if (barHeight < 0.005) continue;
        
        vec2 barCenter = vec2(xPos, baseY + barHeight * 0.5);
        vec2 barSize = vec2(barW * 0.5, barHeight * 0.5);
        
        // Background track
        float trackY = baseY + maxHeight * 0.5;
        float track = eqRect(uv - vec2(xPos, trackY), vec2(barW * 0.5, maxHeight * 0.5));
        float trackMask = smoothstep(0.01, 0.0, track);
        col = mix(col, vec3(0.02, 0.02, 0.025), trackMask * 0.5);
        
        // Active bar
        float d = eqRect(uv - barCenter, barSize);
        float barMask = smoothstep(0.002, 0.0, d);
        
        if (barMask > 0.0) {
            vec3 barColor;
            float level = bands[i];
            float normPos = float(i) / 63.0;
            
            // Full spectrum color mapping (20Hz - 18kHz)
            // Deep red (sub) -> orange -> yellow -> green -> cyan -> blue -> violet (ultra)
            if (normPos < 0.15) {
                // 20-80 Hz: Deep red to orange
                barColor = mix(vec3(0.8, 0.0, 0.0), vec3(1.0, 0.4, 0.0), normPos / 0.15);
            } else if (normPos < 0.30) {
                // 80-200 Hz: Orange to yellow
                barColor = mix(vec3(1.0, 0.4, 0.0), vec3(1.0, 0.9, 0.0), (normPos - 0.15) / 0.15);
            } else if (normPos < 0.50) {
                // 200-800 Hz: Yellow to green
                barColor = mix(vec3(1.0, 0.9, 0.0), vec3(0.2, 1.0, 0.2), (normPos - 0.30) / 0.20);
            } else if (normPos < 0.70) {
                // 800 Hz - 3 kHz: Green to cyan
                barColor = mix(vec3(0.2, 1.0, 0.2), vec3(0.0, 0.9, 1.0), (normPos - 0.50) / 0.20);
            } else if (normPos < 0.85) {
                // 3-8 kHz: Cyan to blue
                barColor = mix(vec3(0.0, 0.9, 1.0), vec3(0.2, 0.4, 1.0), (normPos - 0.70) / 0.15);
            } else {
                // 8-18 kHz: Blue to violet
                barColor = mix(vec3(0.2, 0.4, 1.0), vec3(0.6, 0.2, 1.0), (normPos - 0.85) / 0.15);
            }
            
            // Brightness based on level
            barColor *= (0.6 + level * 0.8);
            
            // Add subtle palette influence
            barColor = mix(barColor, uPrimaryColor, 0.05);
            
            // Glow at top
            float topY = barCenter.y + barSize.y;
            float topGlow = smoothstep(0.0, 0.03, topY - uv.y) * smoothstep(0.0, 0.015, uv.y - (topY - 0.03));
            barColor += vec3(0.5) * topGlow * level;
            
            col = mix(col, barColor, barMask);
        }
    }
    
    // Horizontal dB grid lines
    for (int i = 0; i <= 12; i++) {
        float y = baseY + float(i) * (maxHeight / 12.0);
        float intensity = (i == 6) ? 0.4 : 0.15;
        float line = smoothstep(0.001, 0.0, abs(uv.y - y));
        col = mix(col, vec3(0.1, 0.1, 0.12), line * intensity);
    }
    
    // Frequency markers at bottom (20Hz, 100Hz, 1kHz, 10kHz, 18kHz)
    float markers[5] = float[](0.0, 0.22, 0.5, 0.78, 1.0);
    for (int i = 0; i < 5; i++) {
        float x = -1.0 + markers[i] * 2.0;
        float marker = smoothstep(0.005, 0.0, abs(uv.x - x));
        col = mix(col, vec3(0.15, 0.15, 0.18), marker * 0.6);
        
        // Small vertical tick
        float tick = smoothstep(0.0, 0.02, uv.y - baseY) * smoothstep(0.06, 0.0, uv.y - baseY);
        tick *= marker > 0.0 ? 1.0 : 0.0;
        col = mix(col, vec3(0.2, 0.2, 0.25), tick);
    }
    
    return vec4(col, 1.0);
}

#line 1 54
// @EFFECT name="Noise Dot Grid" index=76 desc="Grid of circles with Perlin noise-controlled sizes" author="p5.js port"

vec4 renderNoiseDotGrid(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Noise scales from p5.js
    float xScale = 0.015;
    float yScale = 0.02;
    
    // Gap between dots (can be modulated by audio)
    float gap = 0.1 + bass * 0.05;
    
    // Offset for noise animation
    float offset = time * 0.5;
    
    vec3 color = vec3(0.0);
    
    // Calculate grid position
    vec2 gridPos = st * 2.0; // Scale to cover -1 to 1 range
    
    // Calculate grid cell
    vec2 cell = floor(gridPos / gap);
    vec2 cellUV = mod(gridPos, gap) / gap - 0.5;
    
    // Calculate noise value using scaled and offset coordinates (use vec2)
    vec2 noiseCoords = vec2((cell.x + offset) * xScale, (cell.y + offset) * yScale);
    float noiseValue = noise(noiseCoords);
    
    // Calculate diameter based on noise value (increased size multiplier)
    float diameter = noiseValue * gap * 2.5;
    
    // Calculate distance from cell center
    float dist = length(cellUV);
    
    // Create circle
    float circle = smoothstep(diameter, diameter * 0.8, dist);
    
    // Color based on audio and noise
    vec3 dotColor = vec3(
        0.5 + 0.5 * sin(noiseValue * 6.28 + time),
        0.5 + 0.5 * sin(noiseValue * 6.28 + time * 1.3),
        0.5 + 0.5 * sin(noiseValue * 6.28 + time * 1.7)
    );
    
    // Audio reactivity
    dotColor *= 0.8 + bass * 0.4;
    
    // Add glow
    float glow = smoothstep(diameter * 1.5, diameter * 0.5, dist);
    dotColor += glow * 0.3 * vec3(0.2, 0.4, 0.8);
    
    color = dotColor * circle;
    
    return vec4(color, 1.0);
}

#line 1 55
// @EFFECT name="Recursive Grids" index=77 desc="Recursive square grids with mouse-reactive rotation" author="#WCCChallenge p5.js port"

vec4 renderRecursiveGrids(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec3 color = vec3(0.0);
    
    // Yellow color from p5.js #FFC00020 (RGB: 1.0, 0.75, 0.0, alpha: 0.125)
    vec3 gridColor = vec3(1.0, 0.75, 0.0);
    
    // Use camera offset as mouse position equivalent
    vec2 shift = vec2(uCameraOffsetX, uCameraOffsetY) * 0.5;
    
    // Grid positions (3x3 grid at -0.3, 0, 0.3)
    for (int gy = -1; gy <= 1; gy++) {
        for (int gx = -1; gx <= 1; gx++) {
            vec2 gridPos = vec2(float(gx) * 0.3, float(gy) * 0.3);
            
            // Initial square size and position
            float d = 0.29; // 0.29 * height equivalent
            vec2 pos = gridPos;
            
            // Recursive/iterative square drawing (max iterations)
            const int maxIterations = 8;
            float s = 10.0;
            
            for (int i = 0; i < maxIterations; i++) {
                if (d < 0.016) break; // height/60 equivalent
                
                // Calculate rotation based on position relative to shift
                vec2 toShift = pos - shift;
                float reach = atan(toShift.y, toShift.x);
                
                // Draw square at current position
                vec2 rectSt = (st - pos) / d;
                float rect = smoothstep(0.5, 0.48, max(abs(rectSt.x), abs(rectSt.y)));
                color += gridColor * rect * 0.125; // Alpha from p5.js
                
                // Calculate next position and size
                pos = pos - vec2(cos(reach), sin(reach)) * s * 0.5 * d;
                d = (s - 1.0) * d / s;
            }
        }
    }
    
    // Add background
    color += vec3(1.0) * (1.0 - smoothstep(0.0, 0.1, length(st)));
    
    // Audio reactivity - modulate rotation speed
    color *= 0.8 + bass * 0.3;
    
    return vec4(color, 1.0);
}

#line 1 56
// @EFFECT name="Game of Life" index=78 desc="Conway's Game of Life cellular automaton" author="p5.js port"


// Get cell state at grid position with toroidal wrapping
float getCellState(vec2 gridPos, vec2 gridSize, float time) {
    vec2 wrapped = mod(gridPos, gridSize);
    float seed = hash21(wrapped + floor(time * 0.1));
    return step(0.5, seed);
}

// Count neighbors with toroidal wrapping
int countNeighbors(vec2 gridPos, vec2 gridSize, float time) {
    int sum = 0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            vec2 neighborPos = gridPos + vec2(float(i), float(j));
            sum += int(getCellState(neighborPos, gridSize, time));
        }
    }
    return sum;
}

// Apply Game of Life rules
float applyGameOfLifeRules(float state, int neighbors) {
    if (state < 0.5) {
        // Dead cell becomes alive if exactly 3 neighbors
        return float(neighbors == 3);
    } else {
        // Live cell stays alive if 2 or 3 neighbors
        return float(neighbors == 2 || neighbors == 3);
    }
}

vec4 renderGameOfLife(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    // Grid resolution - matches p5.js resolution of 20
    float resolution = 20.0;
    
    // Calculate grid dimensions
    vec2 aspect = uResolution.xy / min(uResolution.x, uResolution.y);
    vec2 gridSize = aspect * resolution;
    
    // Convert screen position to grid coordinates
    vec2 gridPos = st * 0.5 + 0.5;
    gridPos *= gridSize;
    
    // Audio-reactive evolution speed
    float evolutionSpeed = 0.5 + bass * 0.5;
    float timeStep = floor(time * evolutionSpeed);
    
    // Get current cell state based on hash (simulating state persistence)
    vec2 cellIndex = floor(gridPos);
    float seed = hash21(cellIndex + timeStep * 0.01);
    float currentState = step(0.5, seed);
    
    // Count neighbors
    int neighbors = countNeighbors(cellIndex, gridSize, timeStep);
    
    // Apply Game of Life rules
    float nextState = applyGameOfLifeRules(currentState, neighbors);
    
    // Blend between current and next state for smooth transitions
    float blend = fract(time * evolutionSpeed);
    float cellState = mix(currentState, nextState, blend);
    
    // Color palette from p5.js (black and white)
    vec3 deadColor = vec3(0.88, 0.88, 0.88); // Light gray background
    vec3 aliveColor = vec3(0.0, 0.0, 0.0);     // Black cells
    
    // Add audio-reactive color modulation
    vec3 color = mix(deadColor, aliveColor, cellState);
    
    // Add subtle glow to alive cells based on energy
    float glow = cellState * energy * 0.3;
    color += glow * uPrimaryColor;
    
    // Add grid lines
    vec2 gridUV = fract(gridPos);
    float gridLine = smoothstep(0.02, 0.0, min(gridUV.x, gridUV.y)) +
                     smoothstep(0.98, 1.0, max(gridUV.x, gridUV.y));
    color += gridLine * 0.1;
    
    // Alpha based on cell state
    float alpha = cellState * 0.9 + 0.1;
    
    return vec4(color, alpha);
}

#line 1 57
// @EFFECT name="3D Wave Boxes" index=79 desc="Concentric layers of boxes with wave motion" author="p5.js port"

// Draw a box at position with rotation
float drawBox(vec3 rayOrigin, vec3 rayDir, vec3 boxPos, float boxSize, float angle) {
    // Rotate ray into box local space
    vec3 localOrigin = rayOrigin - boxPos;
    localOrigin = rotateY(localOrigin, -angle);
    vec3 localDir = rotateY(rayDir, -angle);
    
    // Simple box intersection (AABB in local space)
    vec3 boxHalf = vec3(boxSize * 0.5);
    vec3 invDir = 1.0 / max(abs(localDir), 1e-6);
    vec3 tMin = (-boxHalf - localOrigin) * invDir;
    vec3 tMax = (boxHalf - localOrigin) * invDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    
    if (tNear < tFar && tFar > 0.0) {
        return tNear;
    }
    return 1e6;
}

vec4 render3DWaveBoxes(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    const int layerNum = 25;
    const float distance = 10.0;
    const int boxNum = 40;
    const float amplitude = 50.0;
    const float waveSpeed = 1.0;
    
    // Audio-reactive modifications
    float audioAmp = amplitude * (0.8 + bass * 0.4);
    float audioWaveSpeed = waveSpeed * (0.5 + energy * 0.5);
    
    // Camera setup (simplified raymarching)
    vec3 ro = vec3(0.0, -300.0, 300.0);
    vec3 lookAt = vec3(0.0, amplitude, 0.0);
    vec3 forward = normalize(lookAt - ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    
    // Apply camera zoom
    forward *= (1.0 / uCameraZoom);
    
    // Ray direction
    vec3 rd = normalize(forward + st.x * right + st.y * up);
    
    // Scene rotation around Y axis
    float sceneRotation = time * 10.0;
    ro = rotateY(ro, sceneRotation);
    rd = rotateY(rd, sceneRotation);
    
    // Find closest box intersection
    float closestT = 1e6;
    vec3 closestPos = vec3(0.0);
    float closestLayer = 0.0;
    float closestTheta = 0.0;
    
    // Iterate through layers
    for (int layer = 1; layer <= layerNum; layer++) {
        float r = float(layer) * distance;
        
        // Iterate through boxes in layer
        for (int i = 0; i < boxNum; i++) {
            float theta = float(i) * 360.0 / float(boxNum);
            
            // Spherical to rectangular
            vec3 posi = sphericalToRectangular(r, theta, 90.0);
            
            // Add wave motion
            float wavePhase = audioWaveSpeed * time + float(layer) / float(layerNum) * 360.0;
            posi.y += audioAmp * sin(radians(wavePhase));
            
            // Box size based on layer and theta
            float centralAngle = 360.0 / float(boxNum) * 0.9;
            float boxSize = centralAngle / 360.0 * 3.14159 * r;
            
            // Rotate ray into box space
            float t = drawBox(ro, rd, posi, boxSize, theta);
            
            if (t < closestT) {
                closestT = t;
                closestPos = posi;
                closestLayer = float(layer);
                closestTheta = theta;
            }
        }
    }
    
    // If we hit a box, render it
    if (closestT < 1e5) {
        // HSB color calculation
        float hue = remap(closestLayer, 0.0, float(layerNum), 0.0, float(layerNum));
        float sat = 100.0 * (0.75 + 0.25 * sin(radians(mod(time, 360.0))));
        float bri = 100.0 * (1.0 + 0.0 * cos(radians(mod(time / 2.0, 360.0))));
        
        vec3 hsbColor = vec3(hue, sat, bri) / vec3(float(layerNum), 100.0, 100.0);
        color = hsb2rgb(hsbColor);
        
        // Add audio-reactive glow
        color *= (0.8 + energy * 0.4);
        
        // Simple distance fog
        float fog = exp(-closestT * 0.001);
        color *= fog;
        alpha = fog;
        
        // Add rim lighting
        color += vec3(0.2) * energy;
    } else {
        // Background
        color = vec3(0.0);
        alpha = 0.0;
    }
    
    return vec4(color, alpha);
}

#line 1 58
// @EFFECT name="Circular Cellular Automata" index=80 desc="2D cellular automata on circular grid with multiple rule sets" author="p5.js port"


// Get cell state at grid position with toroidal wrapping
float getCellState(vec2 gridPos, vec2 gridSize, float time, int patternIndex) {
    vec2 wrapped = mod(gridPos, gridSize);
    float seed = hash21(wrapped + vec2(float(patternIndex), floor(time * 0.1)));
    return seed;
}

// Count selected neighbors based on pattern
float countSelectedNeighbors(vec2 gridPos, vec2 gridSize, float time, int patternIndex) {
    float total = 0.0;
    
    // Neighbor selection patterns (8 neighbors)
    // Pattern order: left-up, up, right-up, left, right, left-down, down, right-down
    int patterns[152]; // 19 patterns × 8 neighbors
    
    // Pattern 0
    patterns[0] = 0; patterns[1] = 1; patterns[2] = 1; patterns[3] = 1;
    patterns[4] = 1; patterns[5] = 1; patterns[6] = 1; patterns[7] = 1;
    // Pattern 1
    patterns[8] = 1; patterns[9] = 0; patterns[10] = 1; patterns[11] = 1;
    patterns[12] = 1; patterns[13] = 1; patterns[14] = 1; patterns[15] = 1;
    // Pattern 2
    patterns[16] = 1; patterns[17] = 1; patterns[18] = 0; patterns[19] = 1;
    patterns[20] = 1; patterns[21] = 1; patterns[22] = 1; patterns[23] = 1;
    // Pattern 3
    patterns[24] = 1; patterns[25] = 1; patterns[26] = 1; patterns[27] = 0;
    patterns[28] = 1; patterns[29] = 1; patterns[30] = 1; patterns[31] = 1;
    // Pattern 4
    patterns[32] = 1; patterns[33] = 1; patterns[34] = 1; patterns[35] = 1;
    patterns[36] = 0; patterns[37] = 1; patterns[38] = 1; patterns[39] = 1;
    // Pattern 5
    patterns[40] = 1; patterns[41] = 1; patterns[42] = 1; patterns[43] = 1;
    patterns[44] = 1; patterns[45] = 0; patterns[46] = 1; patterns[47] = 1;
    // Pattern 6
    patterns[48] = 1; patterns[49] = 1; patterns[50] = 1; patterns[51] = 1;
    patterns[52] = 1; patterns[53] = 1; patterns[54] = 0; patterns[55] = 1;
    // Pattern 7
    patterns[56] = 1; patterns[57] = 1; patterns[58] = 1; patterns[59] = 1;
    patterns[60] = 1; patterns[61] = 1; patterns[62] = 1; patterns[63] = 0;
    // Pattern 8
    patterns[64] = 1; patterns[65] = 1; patterns[66] = 1; patterns[67] = 1;
    patterns[68] = 1; patterns[69] = 1; patterns[70] = 1; patterns[71] = 1;
    // Pattern 9
    patterns[72] = 0; patterns[73] = 0; patterns[74] = 1; patterns[75] = 1;
    patterns[76] = 1; patterns[77] = 1; patterns[78] = 1; patterns[79] = 1;
    // Pattern 10
    patterns[80] = 1; patterns[81] = 0; patterns[82] = 0; patterns[83] = 1;
    patterns[84] = 1; patterns[85] = 1; patterns[86] = 1; patterns[87] = 1;
    // Pattern 11
    patterns[88] = 1; patterns[89] = 1; patterns[90] = 1; patterns[91] = 1;
    patterns[92] = 1; patterns[93] = 1; patterns[94] = 0; patterns[95] = 0;
    // Pattern 12
    patterns[96] = 1; patterns[97] = 1; patterns[98] = 1; patterns[99] = 1;
    patterns[100] = 1; patterns[101] = 0; patterns[102] = 0; patterns[103] = 1;
    // Pattern 13
    patterns[104] = 1; patterns[105] = 1; patterns[106] = 1; patterns[107] = 0;
    patterns[108] = 1; patterns[109] = 1; patterns[110] = 0; patterns[111] = 1;
    // Pattern 14
    patterns[112] = 0; patterns[113] = 1; patterns[114] = 0; patterns[115] = 1;
    patterns[116] = 1; patterns[117] = 1; patterns[118] = 1; patterns[119] = 1;
    // Pattern 15
    patterns[120] = 1; patterns[121] = 1; patterns[122] = 0; patterns[123] = 0;
    patterns[124] = 1; patterns[125] = 1; patterns[126] = 1; patterns[127] = 1;
    // Pattern 16
    patterns[128] = 0; patterns[129] = 1; patterns[130] = 1; patterns[131] = 0;
    patterns[132] = 1; patterns[133] = 1; patterns[134] = 1; patterns[135] = 1;
    // Pattern 17
    patterns[136] = 1; patterns[137] = 1; patterns[138] = 1; patterns[139] = 1;
    patterns[140] = 1; patterns[141] = 0; patterns[142] = 1; patterns[143] = 0;
    // Pattern 18
    patterns[144] = 0; patterns[145] = 0; patterns[146] = 0; patterns[147] = 0;
    patterns[148] = 1; patterns[149] = 0; patterns[150] = 0; patterns[151] = 0;
    
    int idx = patternIndex * 8;
    
    // Check each neighbor based on pattern
    for (int i = 0; i < 8; i++) {
        if (patterns[idx + i] == 1) {
            vec2 offset = vec2(0.0);
            if (i == 0) offset = vec2(-1.0, -1.0);      // left-up
            else if (i == 1) offset = vec2(0.0, -1.0); // up
            else if (i == 2) offset = vec2(1.0, -1.0);  // right-up
            else if (i == 3) offset = vec2(-1.0, 0.0);  // left
            else if (i == 4) offset = vec2(1.0, 0.0);   // right
            else if (i == 5) offset = vec2(-1.0, 1.0);  // left-down
            else if (i == 6) offset = vec2(0.0, 1.0);   // down
            else if (i == 7) offset = vec2(1.0, 1.0);    // right-down
            
            total += getCellState(gridPos + offset, gridSize, time, patternIndex);
        }
    }
    
    return total;
}

// Apply cellular automata rules
float applyCARules(float state, float total, float average, float previous) {
    if (average >= 254.0) {
        return 0.0;
    } else if (average <= 1.0) {
        return 255.0;
    } else {
        float nextState = state + average;
        if (previous > 0.0) nextState -= previous;
        return clamp(nextState, 0.0, 255.0);
    }
}

vec4 renderCircularCellularAutomata(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Grid parameters from p5.js
    const int columns = 140;
    const int rows = 30;
    const int numPatterns = 19;
    
    // Audio-reactive pattern selection
    float patternCycle = mod(floor(time * 0.2), float(numPatterns));
    int patternIndex = int(patternCycle);
    
    // Audio-reactive evolution
    float evolutionSpeed = 0.5 + bass * 0.5;
    float timeStep = floor(time * evolutionSpeed);
    
    // Convert screen position to polar coordinates
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    float dist = length(fromCenter);
    float angle = atan(fromCenter.y, fromCenter.x) + 3.14159; // 0 to 2PI
    
    // Map to circular grid
    float maxRadius = 0.45;
    float minRadius = 0.05;
    
    if (dist < minRadius || dist > maxRadius) {
        return vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    // Calculate ring and position on ring
    float ringFraction = (dist - minRadius) / (maxRadius - minRadius);
    int ring = int(ringFraction * float(rows));
    
    // Calculate position on ring (column)
    float angleFraction = angle / 6.28318;
    int col = int(angleFraction * float(columns));
    
    // Get cell state
    vec2 gridPos = vec2(float(col), float(ring));
    vec2 gridSize = vec2(float(columns), float(rows));
    
    float currentState = getCellState(gridPos, gridSize, timeStep, patternIndex);
    float previousState = getCellState(gridPos, gridSize, timeStep - 1.0, patternIndex);
    
    // Count selected neighbors
    float total = countSelectedNeighbors(gridPos, gridSize, timeStep, patternIndex);
    float average = total / 8.0;
    
    // Apply rules
    float cellState = applyCARules(currentState, total, average, previousState);
    
    // Smooth transition between states
    float blend = fract(time * evolutionSpeed);
    float smoothState = mix(currentState, cellState, blend);
    
    // Normalize state to 0-1
    float normalizedState = smoothState / 255.0;
    
    // HSB color calculation
    float hue = fract(time * 0.05 + float(patternIndex) * 0.1) * 255.0;
    float saturation = 255.0;
    float brightness = normalizedState * 255.0;
    
    // Convert HSB to RGB
    vec3 hsb = vec3(hue, saturation, brightness) / 255.0;
    vec3 rgb = hsb2rgb(hsb);
    
    // Audio-reactive color modulation
    rgb *= (0.8 + energy * 0.4);
    
    // Add glow based on energy
    float glow = normalizedState * energy * 0.5;
    rgb += glow * uPrimaryColor;
    
    // Alpha based on cell state
    alpha = normalizedState * 0.9 + 0.1;
    
    // Add circular grid lines (decorative)
    float gridLine = smoothstep(0.02, 0.0, fract(ringFraction * float(rows))) +
                     smoothstep(0.98, 1.0, fract(ringFraction * float(rows)));
    rgb += gridLine * 0.1 * uSecondaryColor;
    
    return vec4(rgb, alpha);
}

#line 1 59
// @EFFECT name="Recursive Subdivision" index=81 desc="Quadtree subdivision with 3D boxes" author="p5.js port"


// Gaussian random approximation
float randomGaussian(vec2 p) {
    float u1 = hash21(p);
    float u2 = hash21(p + 100.0);
    return sqrt(-2.0 * log(u1 + 0.001)) * cos(6.28318 * u2);
}



// Draw a 3D box (simplified ray intersection)
float drawBox(vec3 ro, vec3 rd, vec3 boxPos, vec3 boxSize) {
    vec3 boxMin = boxPos - boxSize * 0.5;
    vec3 boxMax = boxPos + boxSize * 0.5;
    
    vec3 invDir = 1.0 / max(abs(rd), 1e-6);
    vec3 tMin = (boxMin - ro) * invDir;
    vec3 tMax = (boxMax - ro) * invDir;
    
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    
    if (tNear < tFar && tFar > 0.0) {
        return tNear;
    }
    return 1e6;
}


vec4 renderRecursiveSubdivision(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    const float bb = 3.0;
    const int maxSquares = 1000;
    const int subdivisions = 175;
    
    // Audio-reactive parameters
    float rotationSpeed = 0.5 + bass * 0.5;
    float subdivisionSeed = floor(time * 0.1);
    
    // Camera setup
    vec3 ro = vec3(0.0, 0.0, 400.0);
    vec3 lookAt = vec3(0.0, 0.0, 0.0);
    vec3 forward = normalize(lookAt - ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    
    // Apply camera zoom
    forward *= (1.0 / uCameraZoom);
    
    // Ray direction
    vec3 rd = normalize(forward + st.x * right + st.y * up);
    
    // Scene rotation
    float rotX = 3.14159 / 2.0 - 3.14159 / 6.0 + 3.14159 / 12.0 * sin(3.14159 / 60.0 * time * rotationSpeed);
    float rotZ = 3.14159 / 60.0 * time * rotationSpeed;
    
    ro = rotateZ(rotateX(ro, rotX), rotZ);
    rd = rotateZ(rotateX(rd, rotX), rotZ);
    
    // Find closest box intersection
    float closestT = 1e6;
    vec3 closestPos = vec3(0.0);
    float closestSize = 0.0;
    int closestIndex = 0;
    
    // Initialize 4 base squares (2x2 grid)
    float baseSize = 200.0;
    vec2 basePositions[4] = vec2[](
        vec2(-100.0, -100.0),
        vec2(100.0, -100.0),
        vec2(100.0, 100.0),
        vec2(-100.0, 100.0)
    );
    
    // Procedurally generate subdivision structure
    for (int i = 0; i < maxSquares; i++) {
        // Determine which base square this belongs to
        int baseIdx = i % 4;
        vec2 basePos = basePositions[baseIdx];
        
        // Calculate subdivision level and position
        int level = 0;
        vec2 pos = basePos;
        float size = baseSize;
        
        // Simulate quadtree subdivision
        for (int s = 0; s < 8; s++) {
            float seed = hash21(vec2(float(i), float(s) + subdivisionSeed));
            if (seed < 0.4 && size > 10.0) {
                // Subdivide
                size *= 0.5;
                int quadrant = int(hash21(vec2(float(i), float(s) + subdivisionSeed + 1000.0)) * 4.0);
                if (quadrant == 0) pos += vec2(size * 0.5, size * 0.5);
                else if (quadrant == 1) pos += vec2(-size * 0.5, size * 0.5);
                else if (quadrant == 2) pos += vec2(-size * 0.5, -size * 0.5);
                else pos += vec2(size * 0.5, -size * 0.5);
                level++;
            }
        }
        
        // Calculate box depth based on type
        float type = hash21(vec2(float(i), subdivisionSeed));
        float depth = size * 0.25;
        if (type < 0.6) {
            // Stepped layers
            int layers = int(size / bb);
            depth = float(layers) * bb * 0.5;
        }
        
        // Box position in 3D
        vec3 boxPos = vec3(pos.x, pos.y, 0.0);
        
        // Draw box
        float t = drawBox(ro, rd, boxPos, vec3(size, size, depth));
        
        if (t < closestT) {
            closestT = t;
            closestPos = boxPos;
            closestSize = size;
            closestIndex = i;
        }
    }
    
    // If we hit a box, render it
    if (closestT < 1e5) {
        // HSB color calculation
        float baseHue = hash21(vec2(0.0, subdivisionSeed)) * 360.0;
        float hue = fract((baseHue + hash21(vec2(float(closestIndex), 0.0)) * 180.0 - 90.0) / 360.0);
        float saturation = 0.8 + hash21(vec2(float(closestIndex), 1.0)) * 0.1;
        float brightness = 0.8 + hash21(vec2(float(closestIndex), 2.0)) * 0.1;
        
        vec3 hsbColor = hsb2rgb(vec3(hue, saturation, brightness));
        
        // Audio-reactive color modulation
        hsbColor *= (0.8 + energy * 0.4);
        
        // Add rim lighting based on surface normal
        color = hsbColor;
        
        // Simple distance fog
        float fog = exp(-closestT * 0.002);
        color *= fog;
        alpha = fog;
        
        // Add glow based on energy
        color += vec3(0.1) * energy;
    } else {
        // Background
        color = vec3(0.08, 0.08, 0.04);
        alpha = 1.0;
    }
    
    return vec4(color, alpha);
}

#line 1 60
// @EFFECT name="Lowres Pixelation" index=82 desc="Rotating 3D box rendered as dot-matrix" author="p5.js port"




// Simple box intersection
float boxIntersect(vec3 ro, vec3 rd, vec3 boxSize) {
    vec3 boxMin = -boxSize * 0.5;
    vec3 boxMax = boxSize * 0.5;
    
    vec3 invDir = 1.0 / max(abs(rd), 1e-6);
    vec3 tMin = (boxMin - ro) * invDir;
    vec3 tMax = (boxMax - ro) * invDir;
    
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    
    if (tNear < tFar && tFar > 0.0) {
        return tNear;
    }
    return 1e6;
}

// Calculate box normal
vec3 boxNormal(vec3 p, vec3 boxSize) {
    vec3 boxMin = -boxSize * 0.5;
    vec3 boxMax = boxSize * 0.5;
    
    vec3 normal = vec3(0.0);
    float epsilon = 0.01;
    
    if (abs(p.x - boxMin.x) < epsilon) normal = vec3(-1.0, 0.0, 0.0);
    else if (abs(p.x - boxMax.x) < epsilon) normal = vec3(1.0, 0.0, 0.0);
    else if (abs(p.y - boxMin.y) < epsilon) normal = vec3(0.0, -1.0, 0.0);
    else if (abs(p.y - boxMax.y) < epsilon) normal = vec3(0.0, 1.0, 0.0);
    else if (abs(p.z - boxMin.z) < epsilon) normal = vec3(0.0, 0.0, -1.0);
    else if (abs(p.z - boxMax.z) < epsilon) normal = vec3(0.0, 0.0, 1.0);
    
    return normal;
}

vec4 renderLowresPixelation(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    const int maxTiles = 100;
    vec3 boxSize = vec3(30.0, 80.0, 20.0);
    
    // Audio-reactive resolution (10-100 tiles)
    float tiles = mix(10.0, float(maxTiles), energy);
    vec2 gridPos = floor(st * tiles);
    vec2 gridUV = fract(st * tiles);
    
    // Calculate low-res UV coordinates
    vec2 lowResUV = gridPos / tiles;
    
    // Render 3D box to low-res buffer
    vec3 ro = vec3(0.0, 0.0, 150.0);
    vec3 rd = normalize(vec3(lowResUV - 0.5, -1.0));
    
    // Apply rotations from p5.js
    float rotX = radians(time * 1.2);
    float rotY = radians(time * 0.5);
    float rotZ = radians(time * 1.5);
    
    // Audio-reactive rotation speed
    rotX *= (0.5 + bass * 0.5);
    rotY *= (0.5 + mid * 0.5);
    rotZ *= (0.5 + high * 0.5);
    
    ro = rotateZ(rotateY(rotateX(ro, rotX), rotY), rotZ);
    rd = rotateZ(rotateY(rotateX(rd, rotX), rotY), rotZ);
    
    // Intersect with box
    float t = boxIntersect(ro, rd, boxSize);
    
    float brightness = 0.0;
    if (t < 1e5) {
        // Calculate hit point
        vec3 hit = ro + rd * t;
        vec3 normal = boxNormal(hit, boxSize);
        
        // Lighting from p5.js
        vec3 ambient = vec3(0.31); // 80/255
        vec3 lightDir = normalize(vec3(-1.0, 0.0, -1.0));
        vec3 lightColor = vec3(1.0);
        
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 lighting = ambient + lightColor * diff;
        
        brightness = dot(lighting, vec3(0.299, 0.587, 0.114));
    }
    
    // Calculate dot size based on brightness
    float tileW = 1.0 / tiles;
    float dotSize = map(brightness, 0.0, 1.0, 0.0, tileW);
    
    // Draw dot
    vec2 center = gridUV - 0.5;
    float dist = length(center);
    float dotMask = smoothstep(dotSize * 0.5, dotSize * 0.4, dist);
    
    // White color from p5.js
    vec3 dotColor = vec3(1.0);
    
    // Audio-reactive brightness
    dotColor *= (0.8 + energy * 0.4);
    
    color = dotColor * dotMask;
    alpha = dotMask;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, alpha);
    
    return vec4(color, alpha);
}

#line 1 61
// @EFFECT name="Particle Cloud" index=83 desc="Rotating sphere-like cloud with mouse repulsion" author="p5.js port"


vec4 renderParticleCloud(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    const int particles = 8000;
    const float attraction = 0.01;
    const float damping = 0.9;
    const float repel_strength = 28.0;
    float radius = 250.0;
    float repel_radius = 90.0;
    
    // Audio-reactive parameters
    radius *= (0.8 + bass * 0.4);
    repel_radius *= (0.8 + energy * 0.5);
    
    // Rotation angle
    float angle = time * 0.5;
    angle *= (0.5 + tempo * 0.5);
    
    // Mouse position (use camera offset as mouse equivalent)
    vec2 mouse = vec2(uCameraOffsetX, uCameraOffsetY) * 500.0;
    
    // Sample particles procedurally
    float brightness = 0.0;
    
    // Use spatial hashing to simulate particle density
    vec2 gridPos = st * 100.0;
    vec2 gridIdx = floor(gridPos);
    vec2 gridUV = fract(gridPos);
    
    // Sample multiple particles in this region
    for (int i = 0; i < 32; i++) {
        float idx = float(i) + hash21(gridIdx) * 100.0;
        
        // Compute rotating "home" position
        float homeX = sin(idx + angle) * sin(idx * idx) * radius;
        float homeY = cos(idx * idx) * radius;
        vec2 home = vec2(homeX, homeY);
        
        // Current particle position (with some noise)
        vec2 noise = vec2(hash21(vec2(idx, time)), hash21(vec2(idx, time + 100.0))) - 0.5;
        vec2 pos = home + noise * 10.0;
        
        // Mouse repulsion
        vec2 awayFromMouse = pos - mouse;
        float distSq = dot(awayFromMouse, awayFromMouse);
        
        if (distSq > 0.1 && distSq < repel_radius * repel_radius) {
            float distance = sqrt(distSq);
            awayFromMouse = normalize(awayFromMouse);
            float repel = repel_strength * (1.0 - distance / repel_radius);
            pos += awayFromMouse * repel * 0.5;
        }
        
        // Check if this particle is close to our sample point
        vec2 toSample = st * 1000.0 - pos;
        float sampleDist = length(toSample);
        
        if (sampleDist < 15.0) {
            brightness += smoothstep(15.0, 0.0, sampleDist);
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // White color from p5.js
    vec3 particleColor = vec3(1.0);
    
    // Audio-reactive brightness
    particleColor *= (0.8 + energy * 0.4);
    
    color = particleColor * brightness;
    alpha = brightness;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, alpha);
    
    // Add subtle glow based on energy
    color += vec3(0.1) * energy * brightness;
    
    return vec4(color, alpha + 0.1);
}

#line 1 62
// @EFFECT name="Fibonacci Curl" index=84 desc="Fibonacci spiral with 3D rotation" author="p5.js port"





// Draw arc segment
float drawArc(vec2 p, float radius, float startAngle, float endAngle, float lineWidth) {
    vec2 toP = p;
    float dist = length(toP);
    float angle = atan(toP.y, toP.x);
    
    // Normalize angle to [0, 2PI]
    if (angle < 0.0) angle += 6.28318;
    
    // Normalize start/end angles
    float start = mod(startAngle, 6.28318);
    float end = mod(endAngle, 6.28318);
    
    // Check if angle is within arc
    bool inAngle = false;
    if (start < end) {
        inAngle = angle >= start && angle <= end;
    } else {
        inAngle = angle >= start || angle <= end;
    }
    
    // Distance from arc
    float distFromArc = abs(dist - radius);
    
    // Arc stroke
    if (inAngle && distFromArc < lineWidth) {
        return 1.0 - smoothstep(0.0, lineWidth, distFromArc);
    }
    
    return 0.0;
}

vec4 renderFibonacciCurl(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    const int fibCount = 15;
    const float halfPi = 1.5708;
    
    // Audio-reactive rotation
    float dim = 2.0 * 6.28318 * sin(time / 300.0);
    dim *= (0.5 + bass * 0.5);
    
    // Audio-reactive arc type cycling
    float typeCycle = mod(floor(time * 0.2), 3.0);
    int arcType = int(typeCycle);
    
    // Generate Fibonacci sequence
    float fib[15];
    fib[0] = 0.0;
    fib[1] = 1.0;
    for (int i = 2; i < fibCount; i++) {
        fib[i] = fib[i - 1] + fib[i - 2];
    }
    
    // Scale factor
    float scale = 0.005;
    scale *= (0.8 + energy * 0.4);
    
    // Calculate total arc contribution
    float arcBrightness = 0.0;
    
    // Transform screen position to 3D space
    vec3 p = vec3(st * 2.0, 0.0);
    
    // Apply 3D rotations from p5.js
    p = rotateZ(p, dim / 2.0);
    p = rotateY(p, dim / 2.0);
    p = rotateX(p, dim / 2.0);
    
    // Use 2D projection for arc drawing
    vec2 pos2D = p.xy;
    
    // Draw Fibonacci curl
    vec2 currentPos = vec2(0.0);
    float currentAngle = 0.0;
    
    for (int i = 0; i < fibCount; i++) {
        float r = fib[i] * abs(dim) * scale;
        
        // Translation offset (from p5.js)
        if (i > 2) {
            float offset = -(fib[i - 1] * abs(dim) * scale) / 2.0 + (fib[i - 3] * abs(dim) * scale) / 2.0;
            currentPos += vec2(offset, 0.0);
        }
        
        // Draw arc segment
        float arc = drawArc(pos2D - currentPos, r, currentAngle, currentAngle + halfPi, 0.02 * scale);
        arcBrightness += arc;
        
        // Rotate for next segment
        currentAngle += halfPi;
        currentPos = rotate2D(currentPos, halfPi);
    }
    
    arcBrightness = clamp(arcBrightness, 0.0, 1.0);
    
    // White color from p5.js
    vec3 arcColor = vec3(1.0);
    
    // Audio-reactive color
    arcColor *= (0.8 + energy * 0.4);
    
    // Add subtle glow
    arcColor += vec3(0.2) * energy * arcBrightness;
    
    color = arcColor * arcBrightness;
    alpha = arcBrightness * 0.5 + 0.1;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, alpha);
    
    return vec4(color, alpha + 0.1);
}

#line 1 63
// @EFFECT name="Collatz Spiral" index=85 desc="3n+1 conjecture visualization with dual spiral cycles" author="p5.js port"

// Helper function to calculate distance from point to line segment
float distToLine(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    vec2 ap = p - a;
    float t = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
    vec2 closest = a + t * ab;
    return length(p - closest);
}

vec4 renderCollatzSpiral(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    float velocity = 12.0;
    float evenAngle = 0.24;
    float oddAngle = -0.47;
    float length = 2.0;
    
    // Audio-reactive parameters
    velocity *= (0.5 + bass * 0.5);
    length *= (0.8 + energy * 0.4);
    evenAngle *= (0.8 + mid * 0.4);
    oddAngle *= (0.8 + high * 0.4);
    
    // Center position
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    float dist = length(fromCenter);
    
    // Calculate current number based on time
    float currentNumber = 3.0 + floor(time * 0.1);
    currentNumber = mod(currentNumber, 100.0) + 3.0;
    
    // Simulate Collatz sequence
    float hailstoneA = currentNumber;
    float hailstoneB = currentNumber;
    float angleA = 0.0;
    float angleB = 0.0;
    vec2 pos = vec2(0.0);
    
    // Trace the spiral
    float brightness = 0.0;
    const int maxSteps = 100;
    
    for (int i = 0; i < maxSteps; i++) {
        if (hailstoneA > 1.0 && hailstoneA < 1e6) {
            // Cycle A (white, outward)
            float parityA = mod(hailstoneA, 2.0);
            if (parityA == 0.0) {
                hailstoneA *= 0.5;
                angleA += evenAngle;
            } else {
                hailstoneA = 3.0 * hailstoneA + 1.0;
                angleA += oddAngle;
            }
            
            // Calculate line segment
            vec2 dirA = vec2(cos(angleA), -sin(angleA));
            vec2 lineStart = pos;
            vec2 lineEnd = pos + dirA * length * 0.01;
            
            // Check if our sample point is near this line
            float lineDist = distToLine(fromCenter, lineStart, lineEnd);
            if (lineDist < 0.005) {
                brightness += 1.0 - smoothstep(0.0, 0.005, lineDist);
            }
            
            pos += dirA * length * 0.01;
        }
    }
    
    // Reset for cycle B
    pos = vec2(0.0);
    for (int i = 0; i < maxSteps; i++) {
        if (hailstoneB > 1.0 && hailstoneB < 1e6) {
            // Cycle B (black, inward)
            float parityB = mod(hailstoneB, 2.0);
            if (parityB == 0.0) {
                hailstoneB *= 0.5;
                angleB -= evenAngle;
            } else {
                hailstoneB = 3.0 * hailstoneB + 1.0;
                angleB -= oddAngle;
            }
            
            // Calculate line segment (reverse direction)
            vec2 dirB = vec2(-cos(angleB), -sin(angleB));
            vec2 lineStart = pos;
            vec2 lineEnd = pos + dirB * length * 0.01;
            
            // Check if our sample point is near this line
            float lineDist = distToLine(fromCenter, lineStart, lineEnd);
            if (lineDist < 0.005) {
                brightness -= 0.5 - smoothstep(0.0, 0.005, lineDist);
            }
            
            pos += dirB * length * 0.01;
        }
    }
    
    brightness = clamp(brightness, -0.5, 1.0);
    
    // Color based on brightness (white for positive, black for negative)
    vec3 spiralColor = vec3(brightness + 0.5);
    
    // Audio-reactive color modulation
    spiralColor *= (0.8 + energy * 0.4);
    
    // Add subtle glow
    float glow = smoothstep(0.0, 0.1, abs(brightness));
    spiralColor += vec3(0.1) * energy * glow;
    
    color = spiralColor;
    
    return vec4(color, 1.0);
}

#line 1 64
// @EFFECT name="Quadtree Boxes" index=86 desc="3D quadtree subdivision with layered and grid boxes" author="p5.js port"






// Box intersection
float boxIntersect(vec3 ro, vec3 rd, vec3 boxMin, vec3 boxMax) {
    vec3 invDir = 1.0 / max(abs(rd), 1e-6);
    vec3 tMin = (boxMin - ro) * invDir;
    vec3 tMax = (boxMax - ro) * invDir;
    
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    
    if (tNear < tFar && tFar > 0.0) {
        return tNear;
    }
    return 1e6;
}

// Box normal
vec3 boxNormal(vec3 p, vec3 boxMin, vec3 boxMax) {
    vec3 normal = vec3(0.0);
    float epsilon = 0.01;
    
    if (abs(p.x - boxMin.x) < epsilon) normal = vec3(-1.0, 0.0, 0.0);
    else if (abs(p.x - boxMax.x) < epsilon) normal = vec3(1.0, 0.0, 0.0);
    else if (abs(p.y - boxMin.y) < epsilon) normal = vec3(0.0, -1.0, 0.0);
    else if (abs(p.y - boxMax.y) < epsilon) normal = vec3(0.0, 1.0, 0.0);
    else if (abs(p.z - boxMin.z) < epsilon) normal = vec3(0.0, 0.0, -1.0);
    else if (abs(p.z - boxMax.z) < epsilon) normal = vec3(0.0, 0.0, 1.0);
    
    return normal;
}


vec4 renderQuadtreeBoxes(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    float bb = 3.0;
    float cf = hash21(vec2(0.0, time)) * 360.0;
    
    // Audio-reactive parameters
    bb *= (0.8 + energy * 0.4);
    
    // 3D rotation
    float rotX = 1.5708 - 0.5236 + 0.2618 * sin(3.14159 / 60.0 * time);
    float rotZ = 3.14159 / 60.0 * time;
    
    rotX *= (0.5 + bass * 0.5);
    rotZ *= (0.5 + mid * 0.5);
    
    // Ray setup
    vec3 ro = vec3(0.0, 0.0, 400.0);
    vec3 rd = normalize(vec3(st * 800.0 - 400.0, st * 800.0 - 400.0, -400.0));
    
    // Apply rotations
    ro = rotateZ(rotateX(ro, rotX), rotZ);
    rd = rotateZ(rotateX(rd, rotX), rotZ);
    
    // Procedurally generate quadtree structure
    float brightness = 0.0;
    
    // Initial 4 boxes
    vec3 boxPositions[4];
    float boxSizes[4];
    
    boxPositions[0] = vec3(-200.0, -200.0, 0.0);
    boxSizes[0] = 400.0;
    
    boxPositions[1] = vec3(200.0, -200.0, 0.0);
    boxSizes[1] = 400.0;
    
    boxPositions[2] = vec3(200.0, 200.0, 0.0);
    boxSizes[2] = 400.0;
    
    boxPositions[3] = vec3(-200.0, 200.0, 0.0);
    boxSizes[3] = 400.0;
    
    // Render boxes
    for (int i = 0; i < 4; i++) {
        vec3 boxPos = boxPositions[i];
        float boxSize = boxSizes[i];
        
        // Random properties for this box
        float ran = hash31(vec3(float(i), 0.0, 0.0));
        int o = int(hash31(vec3(float(i), 1.0, 0.0)) * 3.0) + 2;
        
        // HSB color
        float hue = mod(cf + hash31(vec3(float(i), 2.0, 0.0)) * 180.0 - 90.0, 360.0) / 360.0;
        float sat = 0.8 + hash31(vec3(float(i), 3.0, 0.0)) * 0.1;
        float bri = 0.8 + hash31(vec3(float(i), 4.0, 0.0)) * 0.1;
        vec3 boxColor = hsb2rgb(vec3(hue, sat, bri));
        
        // Audio-reactive color
        boxColor *= (0.8 + energy * 0.4);
        
        vec3 boxMin = boxPos - boxSize * 0.5;
        vec3 boxMax = boxPos + boxSize * 0.5;
        
        float t = boxIntersect(ro, rd, boxMin, boxMax);
        
        if (t < 1e5) {
            vec3 hit = ro + rd * t;
            vec3 normal = boxNormal(hit, boxMin, boxMax);
            
            // Lighting
            vec3 lightDir = normalize(vec3(-1.0, 0.0, -1.0));
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 lighting = vec3(0.2) + boxColor * diff;
            
            brightness += 0.5 * diff;
            
            // Add grid pattern if ran >= 0.6
            if (ran >= 0.6) {
                vec2 localUV = (hit.xy - boxPos) / boxSize + 0.5;
                float gridX = step(0.5, mod(localUV.x * float(o), 1.0));
                float gridY = step(0.5, mod(localUV.y * float(o), 1.0));
                if (gridX > 0.5 || gridY > 0.5) {
                    lighting *= 0.5;
                }
            }
            
            color += lighting * 0.3;
            alpha += 0.3;
        }
    }
    
    // Procedural subdivision (simplified)
    for (int k = 0; k < 50; k++) {
        float x = (hash31(vec3(float(k), 0.0, 0.0)) - 0.5) * 240.0;
        float y = (hash31(vec3(float(k), 1.0, 0.0)) - 0.5) * 240.0;
        
        // Find containing box
        int h = -1;
        for (int i = 0; i < 4; i++) {
            vec3 boxPos = boxPositions[i];
            float boxSize = boxSizes[i];
            
            if (x > boxPos.x - boxSize * 0.5 && x < boxPos.x + boxSize * 0.5 &&
                y > boxPos.y - boxSize * 0.5 && y < boxPos.y + boxSize * 0.5) {
                h = i;
                break;
            }
        }
        
        if (h >= 0) {
            float newSize = boxSizes[h] * 0.5;
            vec3 subBoxPos = boxPositions[h] + vec3(newSize * 0.5, newSize * 0.5, 0.0);
            
            vec3 subBoxMin = subBoxPos - newSize * 0.5;
            vec3 subBoxMax = subBoxPos + newSize * 0.5;
            
            float t = boxIntersect(ro, rd, subBoxMin, subBoxMax);
            
            if (t < 1e5) {
                vec3 hit = ro + rd * t;
                vec3 normal = boxNormal(hit, subBoxMin, subBoxMax);
                
                float hue = mod(cf + hash31(vec3(float(k), 0.0, 0.0)) * 180.0 - 90.0, 360.0) / 360.0;
                vec3 subColor = hsb2rgb(vec3(hue, 0.85, 0.85));
                
                vec3 lightDir = normalize(vec3(-1.0, 0.0, -1.0));
                float diff = max(dot(normal, lightDir), 0.0);
                vec3 lighting = vec3(0.2) + subColor * diff;
                
                color += lighting * 0.2;
                brightness += 0.2 * diff;
                alpha += 0.2;
            }
        }
    }
    
    color = clamp(color, 0.0, 1.0);
    alpha = clamp(alpha, 0.0, 1.0);
    
    // Background
    vec3 bgColor = vec3(0.08, 0.08, 0.04); // 20,20,10 in HSB converted
    color = mix(bgColor, color, alpha);
    
    return vec4(color, alpha + 0.1);
}

#line 1 65
// @EFFECT name="Star Ring" index=87 desc="Star ring with particle system and gradient layers" author="p5.js port"



vec4 renderStarRing(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    float innerRadiusi = 75.0;
    float innerRadius = 130.0;
    float innerRadiussa = 320.0;
    float innerRadiussb = 198.0;
    float outerRadius = 550.0;
    float noiseAmp = 20.0;
    float Noisefactor = 33.0;
    float innerNoisefactor = 10.0;
    float innerAlpha = 255.0;
    float outerAlpha = 15.0;
    float thick = 10.0;
    float pointStep = 0.9;
    
    // Audio-reactive parameters
    noiseAmp *= (0.8 + bass * 0.4);
    Noisefactor *= (0.8 + mid * 0.4);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    float dist = length(fromCenter);
    float angle = atan(fromCenter.y, fromCenter.x);
    
    // Rotation angles
    float d = time * 0.8 * 1.5;
    float e = time * 0.8 * 0.65;
    float g = time * 0.8 * 0.2;
    
    d *= (0.5 + bass * 0.5);
    e *= (0.5 + mid * 0.5);
    g *= (0.5 + high * 0.5);
    
    // Calculate ring contributions
    float ringBrightness = 0.0;
    
    // Inner ring (white to orange gradient)
    if (d < 360.0) {
        vec2 rotatedUV = rotate2D(fromCenter, d);
        float ringDist = length(rotatedUV);
        
        if (ringDist > innerRadiusi && ringDist < outerRadius) {
            float noiseOffset = sin(time / innerNoisefactor / 10.0) * cos(time / innerNoisefactor / 10.0) * 15.0;
            float noiseVal = noise(vec2(ringDist / Noisefactor / 3.0, time)) * noiseAmp;
            
            float alpha = mix(innerAlpha / 255.0, outerAlpha / 255.0, (ringDist - innerRadiusi) / (outerRadius - innerRadiusi));
            
            // Color gradient from white to orange
            vec3 c3 = vec3(247.0, 127.0, 0.0) / 255.0;
            vec3 c4 = vec3(1.0);
            float mixVal = (ringDist - innerRadiusi) / (outerRadius - innerRadiusi);
            vec3 ringColor = mix(c4, c3, mixVal);
            
            float pointSize = 1.0;
            float pointDist = abs(ringDist - (floor(ringDist / pointStep) * pointStep + noiseOffset));
            
            if (pointDist < 0.01) {
                ringBrightness += alpha * ringColor.r * 0.3;
            }
        }
    }
    
    // Middle ring (white to blue gradient)
    if (e < 360.0) {
        vec2 rotatedUV = rotate2D(fromCenter, e);
        float ringDist = length(rotatedUV);
        
        if (ringDist > innerRadius && ringDist < outerRadius) {
            float noiseOffset = sin(20.0 * time) * 10.0 + noise(vec2(time / innerNoisefactor, 1.0)) * 50.0;
            float noiseVal = noise(vec2(ringDist / Noisefactor, time)) * noiseAmp;
            
            float alpha = mix(innerAlpha / 255.0, outerAlpha / 255.0, (ringDist - innerRadius) / (outerRadius - innerRadius));
            
            // Color gradient from white to blue
            vec3 c1 = vec3(0.0, 167.0, 225.0) / 255.0;
            vec3 c2 = vec3(1.0);
            float mixVal = (ringDist - innerRadius) / (outerRadius - innerRadius);
            vec3 ringColor = mix(c2, c1, mixVal);
            
            float thick1 = noise(vec2(time * 10.0, 0.0)) * thick + 20.0 * (sin(10.0 * time) + 1.0);
            float irisStroke = 1.0 + 0.6 * step(thick1, 0.0);
            
            float pointDist = abs(ringDist - (floor(ringDist / pointStep) * pointStep + noiseOffset));
            float waveOffset = noiseVal;
            
            if (pointDist < 0.02) {
                ringBrightness += alpha * ringColor.g * 0.4;
            }
        }
    }
    
    // Outer ring (blue to teal gradient)
    if (g < 360.0) {
        vec2 rotatedUV = rotate2D(fromCenter, g);
        float ringDist = length(rotatedUV);
        
        if (ringDist > innerRadiussa && ringDist < outerRadius) {
            float noiseOffset = sin(2.5 * time) * 10.0 + noise(vec2(time / innerNoisefactor / 1.1, 2.0)) * 40.0;
            
            float alpha = mix(innerAlpha / 255.0, (outerAlpha - 15.0) / 255.0, (ringDist - innerRadius) / (outerRadius - innerRadius));
            
            // Color gradient from blue to teal
            vec3 c5 = vec3(3.0, 71.0, 72.0) / 255.0;
            vec3 c6 = vec3(20.0, 129.0, 186.0) / 255.0;
            float mixVal = (ringDist - innerRadius) / (outerRadius - innerRadius);
            vec3 ringColor = mix(c6, c5, mixVal);
            
            float pointDist = abs(ringDist - (floor(ringDist / pointStep) * pointStep + noiseOffset));
            
            if (pointDist < 0.01) {
                ringBrightness += alpha * ringColor.b * 0.3;
            }
        }
    }
    
    // Inner solid ring (cyan to dark blue gradient)
    if (g < 360.0) {
        vec2 rotatedUV = rotate2D(fromCenter, g);
        float ringDist = length(rotatedUV);
        
        if (ringDist > innerRadiussb && ringDist < outerRadius) {
            float noiseOffset = cos(2.5 * time) * 10.0 + noise(vec2(time / innerNoisefactor / 1.2, 3.0)) * 40.0;
            
            float alpha = mix((innerAlpha - 90.0) / 255.0, (outerAlpha - 15.0) / 255.0, (ringDist - innerRadius) / (outerRadius - innerRadius));
            
            // Color gradient from cyan to dark blue
            vec3 c7 = vec3(0.0, 150.0, 199.0) / 255.0;
            vec3 c8 = vec3(10.0, 36.0, 99.0) / 255.0;
            float mixVal = (ringDist - innerRadius) / (outerRadius - innerRadius);
            vec3 ringColor = mix(c7, c8, mixVal);
            
            float pointDist = abs(ringDist - (floor(ringDist / pointStep) * pointStep + noiseOffset));
            
            if (pointDist < 0.01) {
                ringBrightness += alpha * ringColor.b * 0.2;
            }
        }
    }
    
    // Particle system
    float particleBrightness = 0.0;
    const int numParticles = 100;
    
    for (int i = 0; i < numParticles; i++) {
        float idx = float(i);
        float particleAngle = hash21(vec2(idx, 0.0)) * 6.28318;
        float particleRadius = mix(innerRadius, outerRadius * 0.5, hash21(vec2(idx, 1.0)));
        
        vec2 particlePos = vec2(cos(particleAngle), sin(particleAngle)) * particleRadius;
        
        // Noise-based movement
        float noiseFactorx = 35.0;
        float noiseFactory = 35.0;
        vec2 vel = vec2(
            map(noise(vec2(particlePos.x / noiseFactorx, particlePos.y / noiseFactory)), 0.0, 1.0, -1.0, 1.0) * 2.5,
            map(noise(vec2(particlePos.x / noiseFactorx + 10.0, particlePos.y / noiseFactory + 100.0)), 0.0, 1.0, -1.0, 1.0) * 2.5
        );
        
        particlePos += vel * time * 0.01;
        
        float particleDist = length(fromCenter - particlePos);
        if (particleDist < 2.5) {
            float alpha = mix(100.0 / 255.0, 0.0, sqrt(dot(particlePos, particlePos)) / outerRadius);
            particleBrightness += alpha * 0.5;
        }
    }
    
    // Star system (radial lines from center)
    float starBrightness = 0.0;
    const int numStars = 50;
    
    for (int i = 0; i < numStars; i++) {
        float idx = float(i);
        float starAngle = hash21(vec2(idx, 0.0)) * 6.28318;
        float starDist = hash21(vec2(idx, 1.0)) * outerRadius;
        
        vec2 starPos = vec2(cos(starAngle), sin(starAngle)) * starDist;
        
        // Radial movement
        float acc = mix(0.005, 0.2, energy);
        vec2 vel = vec2(cos(starAngle), sin(starAngle)) * acc * time * 10.0;
        
        starPos += vel;
        
        // Check if star is near our sample point
        float lineDist = abs(dot(normalize(fromCenter), normalize(starPos)));
        float distToLine = length(fromCenter - starPos * dot(fromCenter, starPos) / dot(starPos, starPos));
        
        if (distToLine < 0.02 && lineDist > 0.9) {
            float alpha = mix(0.0, 1.0, length(vel) / 3.0);
            starBrightness += alpha * 0.3;
        }
    }
    
    // Combine all layers
    ringBrightness = clamp(ringBrightness, 0.0, 1.0);
    particleBrightness = clamp(particleBrightness, 0.0, 1.0);
    starBrightness = clamp(starBrightness, 0.0, 1.0);
    
    // Composite colors
    vec3 ringColor = vec3(ringBrightness * 0.8, ringBrightness * 0.6, ringBrightness * 0.4);
    vec3 particleColor = vec3(particleBrightness);
    vec3 starColor = vec3(starBrightness);
    
    color = ringColor + particleColor + starColor;
    alpha = ringBrightness + particleBrightness * 0.5 + starBrightness * 0.3;
    
    // Audio-reactive color modulation
    color *= (0.8 + energy * 0.4);
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, clamp(alpha, 0.0, 1.0));
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}

#line 1 66
// @EFFECT name="Rotating Circle" index=88 desc="Rotating circular pattern with noise-based line connections" author="p5.js port"



vec4 renderRotatingCircle(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    float radius = 250.0;
    float numPoints = 63.0; // TWO_PI / 0.1
    float step = 0.1;
    float rotationSpeed = 0.04;
    
    // Audio-reactive parameters
    radius *= (0.8 + energy * 0.4);
    rotationSpeed *= (0.5 + bass * 0.5);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    
    // Periodic reset (every 750 frames ~ 12.5 seconds)
    float cycleTime = 12.5;
    float cycle = floor(time / cycleTime);
    float localTime = mod(time, cycleTime);
    
    // Random rotation angle for this cycle
    float a = 3.14159 / floor(hash21(vec2(cycle, 0.0)) * 4.0 + 2.0);
    
    // Overall rotation
    float overallRotation = rotationSpeed * time * 60.0;
    
    // Initialize points on circle
    float brightness = 0.0;
    
    for (float i = 0.0; i < 6.28318; i += step) {
        int idx = int(i / step);
        
        // Initial position on circle
        vec2 pos = vec2(cos(i), sin(i)) * radius;
        
        // Apply individual rotation
        pos = rotate2D(pos, a * idx);
        
        // Apply noise-based movement
        float t = localTime * 0.01 * 60.0;
        pos.x += noise(vec2(t, float(idx) * 0.1)) * 2.0;
        pos.y += noise(vec2(t + 10.0, float(idx) * 0.1)) * 2.0;
        
        // Apply overall rotation
        pos = rotate2D(pos, overallRotation);
        
        // Find mapped index (reverse order)
        float j = mix(numPoints - 1.0, 0.0, i / 6.28318);
        int jIdx = int(j);
        
        // Get mapped position
        vec2 mappedPos = vec2(cos(j * step), sin(j * step)) * radius;
        mappedPos = rotate2D(mappedPos, a * jIdx);
        mappedPos.x += noise(vec2(t, float(jIdx) * 0.1)) * 2.0;
        mappedPos.y += noise(vec2(t + 10.0, float(jIdx) * 0.1)) * 2.0;
        mappedPos = rotate2D(mappedPos, overallRotation);
        
        // Draw line between points
        vec2 lineStart = pos * 0.001; // Scale to UV space
        vec2 lineEnd = mappedPos * 0.001;
        
        // Calculate distance from sample point to line
        vec2 lineDir = lineEnd - lineStart;
        float lineLen = length(lineDir);
        if (lineLen > 0.001) {
            vec2 lineNorm = lineDir / lineLen;
            vec2 toSample = fromCenter - lineStart;
            float proj = clamp(dot(toSample, lineNorm), 0.0, lineLen);
            vec2 closest = lineStart + lineNorm * proj;
            float dist = length(fromCenter - closest);
            
            if (dist < 0.003) {
                // Add curve effect with random offset
                float curveOffset = (noise(vec2(i, time)) - 0.5) * 0.006;
                dist += abs(curveOffset);
                
                if (dist < 0.003) {
                    brightness += (1.0 - smoothstep(0.0, 0.003, dist)) * 0.8;
                }
            }
        }
        
        // Draw points
        float pointDist = length(fromCenter - pos * 0.001);
        if (pointDist < 0.005) {
            brightness += (1.0 - smoothstep(0.0, 0.005, pointDist)) * 0.5;
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // Color (white with subtle tint)
    color = vec3(brightness);
    
    // Audio-reactive color modulation
    color *= (0.8 + energy * 0.4);
    
    // Add subtle hue shift based on bass
    color = mix(color, vec3(brightness * 0.5, brightness * 0.3, brightness * 0.8), bass * 0.3);
    
    alpha = brightness + 0.1;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, clamp(alpha, 0.0, 1.0));
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}

#line 1 67
// @EFFECT name="Curved Lines" index=89 desc="Circular pattern with rotating ellipses based on tangent angles" author="p5.js port"



vec4 renderCurvedLines(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    float radius = 200.0;
    float step = 0.1;
    float ellipseWidth = 400.0;
    float ellipseHeight = 1.0;
    
    // Audio-reactive parameters
    radius *= (0.8 + energy * 0.4);
    ellipseWidth *= (0.8 + bass * 0.4);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    
    // Time variable
    float t = time * 0.01;
    
    // Sine wave modulation (0.9 to 2)
    float s = mix(0.9, 2.0, (sin(t) + 1.0) * 0.5);
    s *= (0.5 + mid * 0.5);
    
    // Initialize points on circle
    float brightness = 0.0;
    const int numPoints = 63; // TWO_PI / 0.1
    
    for (int i = 0; i < numPoints; i++) {
        float angle = float(i) * step;
        
        // Current point on circle
        vec2 pos = vec2(cos(angle), sin(angle)) * radius;
        
        // Next point on circle
        float nextAngle = float(i + 1) * step;
        vec2 nextPos = vec2(cos(nextAngle), sin(nextAngle)) * radius;
        
        // Calculate angle to next point (atan2)
        float a = atan(nextPos.y - pos.y, nextPos.x - pos.x);
        
        // Add sine modulation
        a += s;
        
        // Draw ellipse (400x1, essentially a line)
        // The ellipse is centered at pos, rotated by angle a
        vec2 ellipseCenter = pos * 0.001; // Scale to UV space
        vec2 ellipseSize = vec2(ellipseWidth, ellipseHeight) * 0.001;
        
        // Rotate the sample point into ellipse local space
        vec2 localPos = rotate2D(fromCenter - ellipseCenter, -a);
        
        // Check if point is within ellipse
        vec2 normalizedPos = localPos / ellipseSize;
        float ellipseDist = length(normalizedPos);
        
        if (ellipseDist < 1.0) {
            // Add soft edge
            float edge = 1.0 - smoothstep(0.8, 1.0, ellipseDist);
            brightness += edge * 0.5;
        }
        
        // Also draw line representation for thin ellipses
        vec2 lineDir = vec2(cos(a), sin(a));
        float lineLen = ellipseWidth * 0.001;
        vec2 lineStart = ellipseCenter - lineDir * lineLen * 0.5;
        vec2 lineEnd = ellipseCenter + lineDir * lineLen * 0.5;
        
        vec2 lineVec = lineEnd - lineStart;
        float lineLenActual = length(lineVec);
        if (lineLenActual > 0.001) {
            vec2 lineNorm = lineVec / lineLenActual;
            vec2 toSample = fromCenter - lineStart;
            float proj = clamp(dot(toSample, lineNorm), 0.0, lineLenActual);
            vec2 closest = lineStart + lineNorm * proj;
            float lineDist = length(fromCenter - closest);
            
            if (lineDist < 0.002) {
                brightness += (1.0 - smoothstep(0.0, 0.002, lineDist)) * 0.8;
            }
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // Color (white)
    color = vec3(brightness);
    
    // Audio-reactive color modulation
    color *= (0.8 + energy * 0.4);
    
    // Add subtle hue shift based on high frequencies
    color = mix(color, vec3(brightness * 0.8, brightness * 0.5, brightness * 1.0), high * 0.3);
    
    alpha = brightness + 0.1;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, clamp(alpha, 0.0, 1.0));
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}

#line 1 68
// @EFFECT name="Triangle Particles" index=90 desc="Triangle particle system with growing rotating triangles" author="p5.js port"



// Draw a triangle
float drawTriangle(vec2 p, vec2 center, float size, float angle) {
    vec2 localPos = rotate2D(p - center, -angle);
    
    // Triangle vertices (equilateral triangle)
    vec2 v0 = vec2(0.0, size);
    vec2 v1 = vec2(size * cos(2.094), size * sin(2.094));
    vec2 v2 = vec2(size * cos(4.189), size * sin(4.189));
    
    // Edge function for triangle
    vec2 e0 = v1 - v0;
    vec2 e1 = v2 - v1;
    vec2 e2 = v0 - v2;
    
    vec2 p0 = localPos - v0;
    vec2 p1 = localPos - v1;
    vec2 p2 = localPos - v2;
    
    float s0 = sign(e0.x * p0.y - e0.y * p0.x);
    float s1 = sign(e1.x * p1.y - e1.y * p1.x);
    float s2 = sign(e2.x * p2.y - e2.y * p2.x);
    
    // Inside triangle if all signs are same
    float inside = step(0.0, s0 * s1 * s2);
    
    // Add edge glow
    float edge0 = abs(e0.x * p0.y - e0.y * p0.x) / length(e0);
    float edge1 = abs(e1.x * p1.y - e1.y * p1.x) / length(e1);
    float edge2 = abs(e2.x * p2.y - e2.y * p2.x) / length(e2);
    float edge = min(min(edge0, edge1), edge2);
    float edgeGlow = (1.0 - smoothstep(0.0, 0.02, edge)) * 0.5;
    
    return inside + edgeGlow;
}

vec4 renderTriangleParticles(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Background color (light gray from p5.js)
    vec3 bgColor = vec3(240.0 / 255.0);
    
    // Audio-reactive parameters
    float particleCount = 50.0 + energy * 50.0;
    float maxParticleSize = 50.0 + bass * 30.0;
    float growthRate = 1.0 + mid * 0.5;
    
    // Generate particles
    float brightness = 0.0;
    const int maxParticles = 100;
    
    for (int i = 0; i < maxParticles; i++) {
        if (float(i) >= particleCount) break;
        
        // Seed for this particle
        vec2 seed = vec2(float(i), 0.0);
        
        // Initial position (center of screen, or random)
        vec2 initPos = vec2(0.5) + (vec2(hash21(seed), hash21(seed + 1.0)) - 0.5) * 0.1;
        
        // Velocity (random direction)
        float angle = hash21(seed + 2.0) * 6.28318;
        float speed = hash21(seed + 3.0) * 10.0;
        vec2 vel = vec2(cos(angle), sin(angle)) * speed * 0.001;
        
        // Acceleration
        float mx = mix(0.3, 0.5, hash21(seed + 4.0));
        float my = mix(0.3, 0.5, hash21(seed + 5.0));
        if (hash21(seed + 6.0) < 0.5) mx = -mx;
        if (hash21(seed + 7.0) < 0.5) my = -my;
        vec2 acc = vec2(mx, my) * 0.0001;
        
        // Particle lifetime
        float lifetime = hash21(seed + 8.0) * 5.0;
        float particleTime = mod(time + lifetime, 10.0);
        
        // Size grows over time
        float size = 1.0 + particleTime * growthRate * 10.0;
        float maxSize = mix(5.0, maxParticleSize, hash21(seed + 9.0));
        
        // Kill particle when size exceeds max
        if (size > maxSize) continue;
        
        // Position updates
        vec2 pos = initPos + vel * particleTime * 60.0 + acc * particleTime * particleTime * 3600.0;
        
        // Rotation
        float rotAngle = 0.0;
        float incr = 5.0;
        if (hash21(seed + 10.0) < 0.5) incr = -5.0;
        rotAngle = incr * particleTime * 60.0 * 0.01;
        
        // Draw triangle
        float tri = drawTriangle(st, pos, size * 0.002, rotAngle);
        
        if (tri > 0.0) {
            // Color (black from p5.js)
            brightness += tri * 0.3;
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // Color (black triangles on light background)
    color = mix(bgColor, vec3(0.0), brightness);
    
    // Add shadow effect (simulated)
    float shadow = brightness * 0.2;
    color = mix(color, vec3(0.0), shadow);
    
    alpha = brightness + 0.1;
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}

#line 1 69
// @EFFECT name="Bezier Petals" index=91 desc="Rotating petal pattern with bezier curves" author="p5.js port"



// Cubic bezier curve distance estimation
float bezierDistance(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3) {
    // Simple approach: sample along bezier curve and find closest point
    float minDist = 1000.0;
    const int samples = 20;
    
    for (int i = 0; i <= samples; i++) {
        float t = float(i) / float(samples);
        
        // Cubic bezier formula
        float mt = 1.0 - t;
        vec2 bp = mt * mt * mt * p0 + 
                  3.0 * mt * mt * t * p1 + 
                  3.0 * mt * t * t * p2 + 
                  t * t * t * p3;
        
        float d = length(p - bp);
        minDist = min(minDist, d);
    }
    
    return minDist;
}

vec4 renderBezierPetals(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    int petals = 8;
    float maxJ = 200.0;
    float jStep = 25.0;
    
    // Audio-reactive parameters
    petals = int(8.0 + bass * 4.0);
    maxJ *= (0.8 + energy * 0.4);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    
    // Frame count equivalent (60 fps)
    float frameCount = time * 60.0;
    
    // Calculate bezier control points
    float x = sin(radians(frameCount)) * 0.5;
    float y = cos(radians(frameCount * 0.7)) * 0.5;
    float x2 = sin(radians(frameCount * 0.3)) * 0.5;
    float y2 = cos(radians(frameCount * 1.1)) * 0.5;
    float x3 = sin(radians(frameCount * 0.2)) * 0.5;
    
    // Audio-reactive modulation
    x *= (0.5 + mid * 0.5);
    y *= (0.5 + mid * 0.5);
    x2 *= (0.5 + high * 0.5);
    y2 *= (0.5 + high * 0.5);
    x3 *= (0.5 + bass * 0.5);
    
    float brightness = 0.0;
    
    // Iterate through petals
    for (int d = 0; d < 360; d += (360 / 8)) {
        float angle = radians(float(d));
        
        // Rotate sample point into petal space
        vec2 rotatedPos = rotate2D(fromCenter, -angle);
        
        // Iterate through bezier curves
        for (float j = 0.0; j < maxJ; j += jStep) {
            // Bezier control points in UV space
            vec2 p0 = vec2(0.0);
            vec2 p1 = vec2(x, y);
            vec2 p2 = vec2(x3, j * 0.001);
            vec2 p3 = vec2(x2, y2);
            
            // Calculate distance to bezier curve
            float dist = bezierDistance(rotatedPos, p0, p1, p2, p3);
            
            // Add glow based on distance
            if (dist < 0.005) {
                float glow = (1.0 - smoothstep(0.0, 0.005, dist)) * 0.3;
                brightness += glow;
            }
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // Color (white from p5.js)
    color = vec3(brightness);
    
    // Audio-reactive color modulation
    color *= (0.8 + energy * 0.4);
    
    // Add subtle hue shift based on audio
    color = mix(color, vec3(brightness, brightness * 0.8, brightness * 0.6), bass * 0.3);
    
    alpha = brightness + 0.1;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, clamp(alpha, 0.0, 1.0));
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}

#line 1 70
// @EFFECT name="Koch Snowflake" index=92 desc="Koch snowflake fractal with recursive subdivision" author="p5.js port"



// Line distance with glow
float lineGlow(vec2 p, vec2 a, vec2 b, float width) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float d = length(pa - ba * h);
    return smoothstep(width, width * 0.5, d);
}

// Circle distance with glow
float circleGlow(vec2 p, vec2 center, float radius) {
    float d = length(p - center);
    return smoothstep(radius, radius * 0.5, d);
}

// Koch subdivision - subdivide line into 4 segments with triangular bump
vec2 kochSubdivide(vec2 p0, vec2 p1, float t, float rotAngle) {
    vec2 v = p1 - p0;
    vec2 p = p0 + v * t;
    
    // Add triangular bump at t = 0.5
    if (t >= 0.33 && t <= 0.67) {
        vec2 mid = p0 + v * 0.5;
        vec2 perp = vec2(-v.y, v.x);
        perp = normalize(perp) * length(v) * 0.2887; // tan(30°) / 3
        perp = rotate2D(perp, rotAngle);
        p = mid + perp * (t - 0.5) * 3.0;
    }
    
    return p;
}

vec4 renderKochSnowflake(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    int angle = 3; // Number of sides (triangle by default)
    int layer = 3; // Recursion depth
    
    // Audio-reactive parameters
    angle = int(3.0 + bass * 3.0);
    layer = int(3.0 + mid * 2.0);
    if (layer > 4) layer = 4; // Limit for performance
    
    float radius = 0.35;
    radius *= (0.8 + energy * 0.4);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 fromCenter = st - center;
    
    // Rotation for the peak point (simulating mouse interaction)
    float rotAngle = time * 0.5 + energy * 1.0;
    
    float brightness = 0.0;
    
    // Generate initial polygon vertices
    const int maxVertices = 15;
    vec2 vertices[maxVertices];
    int vertexCount = angle;
    
    for (int i = 0; i < maxVertices; i++) {
        if (i >= angle) break;
        float a = float(i) * (6.28318 / float(angle)) + 1.5708; // Start at top
        vertices[i] = vec2(cos(a), sin(a)) * radius;
    }
    
    // Draw Koch snowflake by sampling along lines
    const int samplesPerLine = 50;
    const int maxSegments = 256; // 4^4 = 256 max segments
    
    for (int i = 0; i < maxVertices; i++) {
        if (i >= vertexCount) break;
        
        vec2 p0 = vertices[i];
        vec2 p1 = vertices[(i + 1) % vertexCount];
        
        // Sample along line
        for (int s = 0; s < samplesPerLine; s++) {
            float t = float(s) / float(samplesPerLine - 1);
            
            // Apply Koch subdivision iteratively
            vec2 pt = p0 + (p1 - p0) * t;
            
            for (int l = 0; l < 4; l++) {
                if (l >= layer) break;
                
                // Determine which segment we're in
                float segment = t * 4.0;
                int seg = int(segment);
                float localT = fract(segment);
                
                // Recalculate points for this level
                vec2 v0, v1;
                if (l == 0) {
                    v0 = p0;
                    v1 = p1;
                } else {
                    // For simplicity, just use linear interpolation with noise
                    // Real Koch subdivision would require storing all segments
                    vec2 mid = p0 + (p1 - p0) * 0.5;
                    vec2 perp = vec2(-(p1.y - p0.y), p1.x - p0.x);
                    perp = normalize(perp) * length(p1 - p0) * 0.2887 * (1.0 - float(l) * 0.2);
                    perp = rotate2D(perp, rotAngle * (float(l) + 1.0));
                    
                    if (seg == 1 || seg == 2) {
                        pt = mid + perp * (localT - 0.5) * 2.0;
                    } else {
                        pt = p0 + (p1 - p0) * t;
                    }
                }
                
                t = localT;
            }
            
            // Draw point glow
            float pointDist = length(fromCenter - pt);
            if (pointDist < 0.01) {
                brightness += (1.0 - smoothstep(0.0, 0.01, pointDist)) * 0.2;
            }
        }
        
        // Draw the main line
        float line = lineGlow(fromCenter, p0, p1, 0.003);
        brightness += line * 0.5;
        
        // Draw circles at endpoints
        float circle0 = circleGlow(fromCenter, p0, 0.005);
        float circle1 = circleGlow(fromCenter, p1, 0.005);
        brightness += (circle0 + circle1) * 0.3;
    }
    
    // Add more detail for higher iterations
    for (int l = 0; l < layer; l++) {
        float scale = pow(0.33, float(l + 1));
        float brightnessScale = 1.0 - float(l) * 0.2;
        
        for (int i = 0; i < maxVertices; i++) {
            if (i >= vertexCount) break;
            
            vec2 p0 = vertices[i];
            vec2 p1 = vertices[(i + 1) % vertexCount];
            vec2 mid = p0 + (p1 - p0) * 0.5;
            
            // Add triangular bump
            vec2 perp = vec2(-(p1.y - p0.y), p1.x - p0.x);
            perp = normalize(perp) * length(p1 - p0) * 0.2887 * scale;
            perp = rotate2D(perp, rotAngle);
            vec2 peak = mid + perp;
            
            // Draw bump
            float bumpLine1 = lineGlow(fromCenter, p0 + (mid - p0) * 0.33, peak, 0.002);
            float bumpLine2 = lineGlow(fromCenter, peak, p1 - (p1 - mid) * 0.33, 0.002);
            brightness += (bumpLine1 + bumpLine2) * brightnessScale * 0.3;
            
            // Draw circle at peak
            float peakCircle = circleGlow(fromCenter, peak, 0.003 * scale);
            brightness += peakCircle * brightnessScale * 0.2;
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // Color (white to gray gradient based on distance)
    float dist = length(fromCenter);
    vec3 startCol = vec3(1.0);
    vec3 endCol = vec3(0.33, 0.33, 0.33);
    vec3 col = mix(startCol, endCol, smoothstep(0.0, 0.35, dist));
    
    color = col * brightness;
    
    // Audio-reactive color modulation
    color *= (0.8 + energy * 0.4);
    
    // Add subtle hue shift based on audio
    color = mix(color, vec3(color.r * 0.8, color.g, color.b * 1.2), bass * 0.3);
    
    alpha = brightness + 0.1;
    
    // Background
    vec3 bgColor = vec3(0.0);
    color = mix(bgColor, color, clamp(alpha, 0.0, 1.0));
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}

#line 1 71
// @EFFECT name="Flow Field" index=93 desc="Perlin noise flow field with triangular particles" author="p5.js port"

// Draw triangle
float drawTriangle(vec2 p, vec2 center, float angle, float base, float height) {
    // Triangle vertices in local space
    vec2 v0 = vec2(base, 0.0);
    vec2 v1 = vec2(-base, 0.0);
    vec2 v2 = vec2(0.0, height);
    
    // Rotate vertices
    v0 = rotate2D(v0, angle);
    v1 = rotate2D(v1, angle);
    v2 = rotate2D(v2, angle);
    
    // Transform to world space
    v0 += center;
    v1 += center;
    v2 += center;
    
    // Edge function for triangle
    vec2 e0 = v1 - v0;
    vec2 e1 = v2 - v1;
    vec2 e2 = v0 - v2;
    
    vec2 p0 = p - v0;
    vec2 p1 = p - v1;
    vec2 p2 = p - v2;
    
    float s0 = sign(e0.x * p0.y - e0.y * p0.x);
    float s1 = sign(e1.x * p1.y - e1.y * p1.x);
    float s2 = sign(e2.x * p2.y - e2.y * p2.x);
    
    // Inside triangle if all signs are same
    float inside = step(0.0, s0 * s1 * s2);
    
    return inside;
}

vec4 renderFlowField(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from p5.js
    float inc = 0.02;
    float scl = 30.0;
    
    // Audio-reactive parameters
    inc *= (0.5 + energy * 0.5);
    scl *= (0.8 + bass * 0.4);
    
    // Calculate grid
    float cols = floor(1.0 / scl);
    float rows = floor(1.0 / scl);
    
    // Background color (dark blue from p5.js)
    vec3 bgColor = vec3(0.0, 0.0, 0.196);
    
    // Time offset for noise evolution
    float zoff = time * 0.5;
    zoff *= (0.5 + mid * 0.5);
    
    float brightness = 0.0;
    
    // Iterate through grid
    for (float y = 0.0; y < rows; y++) {
        for (float x = 0.0; x < cols; x++) {
            float xoff = x * inc;
            float yoff = y * inc;
            
            // Get noise angle
            float angle = snoise(vec2(xoff, yoff + zoff)) * 6.28318;
            
            // Grid cell center
            vec2 center = vec2(x * scl + scl * 0.5, y * scl + scl * 0.5);
            
            // Draw triangle
            float tri = drawTriangle(st, center, angle, 0.01, scl * 0.04);
            
            if (tri > 0.0) {
                // Color (white with transparency from p5.js)
                brightness += tri * 0.8;
            }
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    
    // Color (white from p5.js)
    color = vec3(brightness);
    
    // Audio-reactive color modulation
    color *= (0.8 + energy * 0.4);
    
    // Add subtle hue shift based on audio
    color = mix(color, vec3(color.r, color.g * 0.8, color.b * 1.2), bass * 0.3);
    
    alpha = brightness + 0.1;
    
    // Background
    color = mix(bgColor, color, clamp(alpha, 0.0, 1.0));
    
    return vec4(color, clamp(alpha + 0.1, 0.0, 1.0));
}

#line 1 72
// @EFFECT name="Isometric Cubes" index=94 desc="Isometric 3D cube grid with ray tracing" author="p5.js port"


// Isometric projection
vec3 isometricProject(vec3 p, float alpha, float beta) {
    // Convert degrees to radians
    float a = radians(alpha);
    float b = radians(beta);
    
    // Isometric rotation matrix
    float x = p.x * cos(a) - p.y * sin(a);
    float y = (p.x * sin(a) + p.y * cos(a)) * cos(b) - p.z * sin(b);
    float z = (p.x * sin(a) + p.y * cos(a)) * sin(b) + p.z * cos(b);
    
    return vec3(x, y, z);
}

// AABB ray-box intersection
bool intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
    vec3 invDir = 1.0 / rayDir;
    
    float tmin = (boxMin.x - rayOrigin.x) * invDir.x;
    float tmax = (boxMax.x - rayOrigin.x) * invDir.x;
    
    if (tmin > tmax) {
        float temp = tmin;
        tmin = tmax;
        tmax = temp;
    }
    
    float tymin = (boxMin.y - rayOrigin.y) * invDir.y;
    float tymax = (boxMax.y - rayOrigin.y) * invDir.y;
    
    if (tymin > tymax) {
        float temp = tymin;
        tymin = tymax;
        tymax = temp;
    }
    
    if (tmin > tymax || tymin > tmax) {
        return false;
    }
    
    if (tymin > tmin) {
        tmin = tymin;
    }
    
    if (tymax < tmax) {
        tmax = tymax;
    }
    
    float tzmin = (boxMin.z - rayOrigin.z) * invDir.z;
    float tzmax = (boxMax.z - rayOrigin.z) * invDir.z;
    
    if (tzmin > tzmax) {
        float temp = tzmin;
        tzmin = tzmax;
        tzmax = temp;
    }
    
    if (tmin > tzmax || tzmin > tmax) {
        return false;
    }
    
    return true;
}

// Draw cube with isometric projection
float drawCube(vec2 uv, vec3 cubePos, float cubeSize, float alpha, float beta) {
    // Calculate cube bounds
    vec3 boxMin = cubePos - cubeSize * 0.5;
    vec3 boxMax = cubePos + cubeSize * 0.5;
    
    // Project cube vertices to 2D isometric
    vec3 corners[8];
    corners[0] = isometricProject(vec3(boxMin.x, boxMin.y, boxMin.z), alpha, beta);
    corners[1] = isometricProject(vec3(boxMax.x, boxMin.y, boxMin.z), alpha, beta);
    corners[2] = isometricProject(vec3(boxMax.x, boxMax.y, boxMin.z), alpha, beta);
    corners[3] = isometricProject(vec3(boxMin.x, boxMax.y, boxMin.z), alpha, beta);
    corners[4] = isometricProject(vec3(boxMin.x, boxMin.y, boxMax.z), alpha, beta);
    corners[5] = isometricProject(vec3(boxMax.x, boxMin.y, boxMax.z), alpha, beta);
    corners[6] = isometricProject(vec3(boxMax.x, boxMax.y, boxMax.z), alpha, beta);
    corners[7] = isometricProject(vec3(boxMin.x, boxMax.y, boxMax.z), alpha, beta);
    
    // Check if point is inside projected cube (simplified)
    float minProjX = 1000.0, maxProjX = -1000.0;
    float minProjY = 1000.0, maxProjY = -1000.0;
    
    for (int i = 0; i < 8; i++) {
        minProjX = min(minProjX, corners[i].x);
        maxProjX = max(maxProjX, corners[i].x);
        minProjY = min(minProjY, corners[i].y);
        maxProjY = max(maxProjY, corners[i].y);
    }
    
    // Simple bounding box check
    if (uv.x >= minProjX && uv.x <= maxProjX && uv.y >= minProjY && uv.y <= maxProjY) {
        return 1.0;
    }
    
    return 0.0;
}

vec4 renderIsometricCubes(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from original code
    float alphaAngle = 45.0; // yaw
    float betaAngle = 35.264; // pitch
    float cubeSize = 0.1;
    
    // Audio-reactive parameters
    alphaAngle += bass * 10.0;
    betaAngle += mid * 5.0;
    cubeSize *= (0.8 + energy * 0.4);
    
    // Grid size
    int gridSize = 8;
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 uv = (st - center) * 2.0;
    
    float brightness = 0.0;
    
    // Create cube grid
    for (int x = -4; x < 4; x++) {
        for (int y = -4; y < 4; y++) {
            for (int z = -2; z < 2; z++) {
                // Cube position
                vec3 cubePos = vec3(float(x) * cubeSize, float(y) * cubeSize, float(z) * cubeSize);
                
                // Apply audio-reactive offset
                cubePos.x += sin(time + float(x) * 0.5) * bass * 0.1;
                cubePos.y += cos(time + float(y) * 0.5) * mid * 0.1;
                cubePos.z += sin(time + float(z) * 0.5) * high * 0.1;
                
                // Project cube
                float cubeHit = drawCube(uv, cubePos, cubeSize, alphaAngle, betaAngle);
                
                if (cubeHit > 0.0) {
                    // Color from original code: #7259ff (purple)
                    vec3 cubeColor = vec3(0.45, 0.35, 1.0);
                    
                    // Audio-reactive color modulation
                    cubeColor *= (0.8 + energy * 0.4);
                    
                    brightness += cubeHit * 0.8;
                    color += cubeColor * cubeHit;
                }
            }
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    color = clamp(color, 0.0, 1.0);
    
    // Background
    vec3 bgColor = vec3(0.05, 0.05, 0.1);
    color = mix(bgColor, color, brightness);
    
    alpha = brightness + 0.1;
    
    return vec4(color, clamp(alpha, 0.0, 1.0));
}

#line 1 73
// @EFFECT name="Wave Circles" index=95 desc="Concentric wave circles with boxes" author="p5.js port"

// Project 3D point to 2D with rotation
vec2 project3D(vec3 p, float rotation) {
    // Rotate around Y axis
    float c = cos(rotation);
    float s = sin(rotation);
    
    float x = p.x * c - p.z * s;
    float z = p.x * s + p.z * c;
    float y = p.y;
    
    // Simple perspective projection
    float fov = 2.0;
    float dist = 3.0;
    float scale = fov / (dist + z);
    
    return vec2(x * scale, y * scale);
}

// HSB to RGB conversion
vec3 hsbToRgb(vec3 hsb) {
    vec3 rgb = clamp(abs(mod(hsb.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    return hsb.z * mix(vec3(1.0), rgb, hsb.y);
}

vec4 renderWaveCircles(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from original code
    int layerNum = 25;
    float distance = 0.02;
    int boxNum = 40;
    float amplitude = 0.15;
    float waveSpeed = 1.0;
    
    // Audio-reactive parameters
    layerNum = int(float(layerNum) * (0.8 + energy * 0.4));
    distance *= (0.8 + bass * 0.4);
    amplitude *= (0.8 + mid * 0.4);
    waveSpeed *= (0.5 + high * 0.5);
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 uv = (st - center) * 2.0;
    
    float brightness = 0.0;
    
    // Scene rotation
    float rotation = time * waveSpeed * 10.0;
    rotation += bass * 5.0;
    
    // Draw boxes in concentric layers
    for (int layer = 1; layer <= 25; layer++) {
        if (layer > layerNum) continue;
        
        // Layer radius
        float r = float(layer) * distance;
        
        // Draw boxes on this layer
        for (int i = 0; i < 40; i++) {
            float theta = float(i) * (360.0 / float(boxNum));
            
            // Convert spherical to rectangular
            vec3 posi = sphericalToRectangular(r, theta, 90.0);
            
            // Add wave motion
            float wavePhase = waveSpeed * time * 50.0 + float(layer) / float(layerNum) * 360.0;
            posi.y += amplitude * sin(radians(wavePhase));
            
            // Apply audio-reactive offset
            posi.x += sin(time + float(layer) * 0.5) * bass * 0.05;
            posi.z += cos(time + float(i) * 0.5) * mid * 0.05;
            
            // Project to 2D
            vec2 projected = project3D(posi, rotation);
            
            // Box size based on layer
            float boxSize = (360.0 / float(boxNum) * 0.9 / 360.0 * 3.14159 * r) * 0.5;
            
            // Draw box (simple circle/box representation)
            float distToBox = length(uv - projected);
            float boxHit = smoothstep(boxSize, boxSize * 0.5, distToBox);
            
            if (boxHit > 0.0) {
                // HSB color based on layer
                float hue = map(float(layer), 0.0, float(layerNum), 0.0, 1.0);
                float saturation = 0.75 + (1.0 - 0.75) * sin(radians(mod(time * 50.0, 360.0)));
                saturation *= (0.8 + energy * 0.4);
                float brightnessVal = 1.0 + (1.0 - 1.0) * cos(radians(mod(time * 25.0, 360.0)));
                brightnessVal *= (0.8 + high * 0.4);
                
                vec3 hsb = vec3(hue, saturation, brightnessVal);
                vec3 boxColor = hsbToRgb(hsb);
                
                brightness += boxHit * 0.6;
                color += boxColor * boxHit;
            }
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    color = clamp(color, 0.0, 1.0);
    
    // Background
    vec3 bgColor = vec3(0.0, 0.0, 0.05);
    color = mix(bgColor, color, brightness);
    
    alpha = brightness + 0.1;
    
    return vec4(color, clamp(alpha, 0.0, 1.0));
}

#line 1 74
// @EFFECT name="Noise Particles" index=96 desc="Noise-based particle system" author="p5.js port"

// 3D simplex noise
vec3 permute3(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }

vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise3(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
    vec3 i  = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min( g.xyz, l.zxy );
    vec3 i2 = max( g.xyz, l.zxy );
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy;
    vec3 x3 = x0 - D.yyy;
    i = mod(i, 289.0);
    vec4 p = permute3( permute3( permute3(
                i.z + vec3(0.0, i1.z, i2.z))
                + i.y + vec3(0.0, i1.y, i2.y))
                + i.x + vec3(0.0, i1.x, i2.x));
    float n_ = 0.142857142857;
    vec3  ns = n_ * D.wyz - D.xzx;
    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);
    vec4 x = x_ *ns.x + ns.yyyy;
    vec4 y = y_ *ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);
    vec4 b0 = vec4( x.xy, y.xy );
    vec4 b1 = vec4( x.zw, y.zw );
    vec4 s0 = floor(b0)*2.0 + 1.0;
    vec4 s1 = floor(b1)*2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));
    vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
    vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;
    vec3 p0 = vec3(a0.xy,h.x);
    vec3 p1 = vec3(a0.zw,h.y);
    vec3 p2 = vec3(a1.xy,h.z);
    vec3 p3 = vec3(a1.zw,h.w);
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3) ) );
}

vec4 renderNoiseParticles(
    vec2  st,
    float time,
    float tempo,
    float energy,
    float bass,
    float mid,
    float high)
{
    vec3 color = vec3(0.0);
    float alpha = 0.0;
    
    // Parameters from original code
    int num = 1000;
    float noiseScale = 0.01 / 2.0;
    
    // Audio-reactive parameters
    num = int(float(num) * (0.8 + energy * 0.4));
    noiseScale *= (0.5 + bass * 0.5);
    
    float brightness = 0.0;
    
    // Center coordinates
    vec2 center = vec2(0.5);
    vec2 uv = (st - center) * 2.0;
    
    // Simulate particle system
    for (int i = 0; i < 1000; i++) {
        if (i >= num) break;
        
        // Pseudo-random particle position based on index
        float fi = float(i);
        float seed = fi * 0.001;
        
        vec2 p = vec2(
            fract(sin(seed) * 43758.5453),
            fract(cos(seed) * 23421.6432)
        ) * 2.0 - 1.0;
        
        // Animate particle position
        float zoff = time * 0.5;
        zoff *= (0.5 + mid * 0.5);
        
        float n = snoise3(vec3(p * noiseScale * 100.0, zoff));
        float angle = 6.28318 * n;
        
        // Move particle
        p += vec2(cos(angle), sin(angle)) * 0.002;
        
        // Wrap around screen
        p = mod(p + 1.0, 2.0) - 1.0;
        
        // Draw particle (point)
        float distToParticle = length(uv - p);
        float particleHit = smoothstep(0.01, 0.005, distToParticle);
        
        if (particleHit > 0.0) {
            // White color from original code
            vec3 particleColor = vec3(1.0);
            
            // Audio-reactive color modulation
            particleColor *= (0.8 + energy * 0.4);
            
            brightness += particleHit * 0.5;
            color += particleColor * particleHit;
        }
    }
    
    brightness = clamp(brightness, 0.0, 1.0);
    color = clamp(color, 0.0, 1.0);
    
    // Background with trail effect
    vec3 bgColor = vec3(0.0, 0.0, 0.0);
    color = mix(bgColor, color, brightness);
    
    // Trail effect (simulate background(0, 10))
    alpha = brightness * 0.9 + 0.1;
    
    return vec4(color, clamp(alpha, 0.0, 1.0));
}

#line 1 75
void main() {
    vec2 st = (vUV - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    
    // Apply global camera zoom and offset
    st *= uCameraZoom;
    st += vec2(uCameraOffsetX, uCameraOffsetY);

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
    } else if (uMode == 23) {
        color = renderEtiennePulse(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 24) {
        color = renderFractalRunway(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 25) {
        color = renderVolumetricTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 26) {
        color = renderChromaticSwirl(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 27) {
        color = renderHyperPulse(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 28) {
        color = renderGyroidReflections(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 29) {
        color = renderHead(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 30) {
        color = renderMetalGyroidHall(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 31) {
        color = renderHexKaleidoscope(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 32) {
        color = renderHSVColorShift(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 33) {
        color = renderCryptRoots(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 34) {
        color = renderBreathing(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 35) {
        color = renderEvolutionNoise(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 36) {
        color = renderPhiFields(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 37) {
        color = renderFractalInfinity(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 38) {
        color = renderWalker(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 39) {
        color = renderWeirdCreature(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 40) {
        color = renderAnaglyphAssembly(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 41) {
        color = renderMessageTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 42) {
        color = renderPouetGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 43) {
        color = renderCylinderRepeat(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 44) {
        color = renderPowerParticle(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 45) {
        color = renderFlopine(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 46) {
        color = renderEiyeronDeform(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 47) {
        color = renderFractalRotation(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 48) {
        color = renderKaleidoscopicFlow(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 49) {
        color = renderReactiveTwistField(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 50) {
        color = renderCollapsedTransit(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 51) {
        color = renderCelestialRibbonBloom(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 52) {
        color = renderMandelbulbFlux(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 53) {
        color = renderIridescentEye(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 54) {
        color = renderVoronoiGateStream(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 55) {
        color = renderRecursiveCubeBloom(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 56) {
        color = renderHelloWorldGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 57) {
        color = renderDailyFlowLines(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 58) {
        color = renderLitnGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 59) {
        color = renderHannahAdamsGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 60) {
        color = renderAnotherCodeGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 61) {
        color = renderLuperfutGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 62) {
        color = renderGlassRefractionField(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 63) {
        color = renderMatrixDigitalRain(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 64) {
        color = renderIkedaDigits(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 65) {
        color = renderIkedaGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 66) {
        color = renderIChingHexagrams(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 67) {
        color = renderReflectedTurbulence(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 68) {
        color = renderIkedaDataStream(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 69) {
        color = renderLoopNoiseSDF(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 70) {
        color = renderLoopNoiseRays(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 71) {
        color = renderCellRings(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 72) {
        color = renderCoronaVirus(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 73) {
        color = renderSphereRaytrace(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 74) {
        color = renderTilesNumbers(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 75) {
        color = renderAudioEQ(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 76) {
        color = renderNoiseDotGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 77) {
        color = renderRecursiveGrids(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 78) {
        color = renderGameOfLife(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 79) {
        color = render3DWaveBoxes(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 80) {
        color = renderCircularCellularAutomata(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 81) {
        color = renderRecursiveSubdivision(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 82) {
        color = renderLowresPixelation(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 83) {
        color = renderParticleCloud(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 84) {
        color = renderFibonacciCurl(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 85) {
        color = renderCollatzSpiral(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 86) {
        color = renderQuadtreeBoxes(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 87) {
        color = renderStarRing(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 88) {
        color = renderRotatingCircle(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 89) {
        color = renderCurvedLines(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 90) {
        color = renderTriangleParticles(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 91) {
        color = renderBezierPetals(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 92) {
        color = renderKochSnowflake(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 93) {
        color = renderFlowField(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 94) {
        color = renderIsometricCubes(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 95) {
        color = renderWaveCircles(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 96) {
        color = renderNoiseParticles(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    }
    
    // Invalid modes will show black/pink error color

    FragColor = color;
}
