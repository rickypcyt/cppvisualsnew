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
