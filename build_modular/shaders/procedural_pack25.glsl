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
