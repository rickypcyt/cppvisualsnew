// Leon 2018-01-11
// using code from IQ, LJ, Mercury, Duke, Koltes
// Adapted for audio visualizer system

#ifndef LEON_PI
#define LEON_PI 3.14159
#endif

#ifndef LEON_TAU
#define LEON_TAU (LEON_PI*2.)
#endif

#ifndef LEON_REPEAT
#define LEON_REPEAT(v,r) (mod(v+r/2.,r)-r/2.)
#endif

float leonRng (vec2 seed) { return fract(sin(dot(seed*.1,vec2(324.654,156.546)))*46556.24); }
mat2 leonRot (float a) { float c=cos(a),s=sin(a); return mat2(c,s,-s,c); }
float leonSphere (vec3 p, float r) { return length(p)-r; }
float leonCylinder (vec2 p, float r) { return length(p)-r; }
float leonDisk (vec3 p, float r, float h) { return max(length(p.xy)-r, abs(p.z)-h); }

float leonAmod (inout vec2 p, float c) {
    float ca = (2.*3.14159)/c;
    float a = atan(p.y,p.x)+ca*.5;
    float index = floor(a/ca);
    a = mod(a,ca)-ca*.5;
    p = vec2(cos(a),sin(a))*length(p);
    return index;
}

vec3 leonLookAt (vec3 eye, vec3 target, vec2 uv) {
    vec3 forward = normalize(target-eye);
    vec3 right = normalize(cross(vec3(0,1,0), forward));
    vec3 up = normalize(cross(forward, right));
    return normalize(forward * .5 + uv.x * right + uv.y * up);
}

vec3 leonOrbit (vec3 eye, float time) {
    // Use audio-reactive mouse simulation
    float mouseX = sin(time * 0.1) * 0.5;
    float mouseY = cos(time * 0.15) * 0.5;
    eye.xz *= leonRot(mouseX*2.-1.);
    eye.zy *= leonRot(-(mouseY*2.-1.));
    return eye;
}

struct LeonShape {
    float dist, density, friction;
    vec3 color;
};

LeonShape leonMap (vec3 pos, float time) {
    LeonShape scene;
    scene.dist = 1000.;
    scene.color = vec3(1);
    scene.friction = .2;
    scene.density = .01;
    vec3 p, pp;
    float shape = 1000.;
    float interval = .6;
    float polar = 5.;
    float radius = 2.;
    float thin = .005;
    float diskRadius = .4;
    float diskThin = .01;
    scene.dist = leonSphere(pos, radius);

    p = pos;
    leonAmod(p.xz, polar);
    p = abs(p);
    p.yz *= leonRot(time*.1);
    p.xz *= leonRot(time*.2);
    p.yx *= leonRot(time*.3);
    p = LEON_REPEAT(p-time*.5, interval);
    shape = min(shape, leonCylinder(p.xz, thin));
    shape = min(shape, leonCylinder(p.yz, thin));
    shape = min(shape, leonCylinder(p.yx, thin));
    pp = p;
    pp.x = LEON_REPEAT(pp.x+time*.2, .1);
    shape = min(shape, leonDisk(pp.zyx, .02, thin*.5));
    pp = p;
    pp.y = LEON_REPEAT(pp.y+time*.2, .1);
    shape = min(shape, leonDisk(pp.xzy, .02, thin*.5));
    pp = p;
    pp.z = LEON_REPEAT(pp.z+time*.2, .1);
    shape = min(shape, leonDisk(pp, .02, thin*.5));
    p.yz *= leonRot(time*.9);
    p.xz *= leonRot(time*.6);
    p.yx *= leonRot(time*.3);
    diskRadius *= 1.-clamp(length(pos)*.5, 0., 1.);
    shape = min(shape, max(leonDisk(p, diskRadius, thin), leonDisk(p, diskRadius-diskThin, thin*2.)*-1.));
    shape = min(shape, max(leonDisk(p.xzy, diskRadius, thin), leonDisk(p.xzy, diskRadius-diskThin, thin*2.)*-1.));
    shape = min(shape, max(leonDisk(p.zyx, diskRadius, thin), leonDisk(p.zyx, diskRadius-diskThin, thin*2.)*-1.));

    scene.dist = max(scene.dist, shape);

    return scene;
}

vec3 leonRaymarch (vec2 coord, float time, vec2 resolution) {
    vec2 viewport = (coord.xy-.5*resolution.xy)/resolution.y;
    vec3 origin = leonOrbit(vec3(0,1,-1), time);
    vec3 eye = origin;
    vec3 ray = leonLookAt(origin, vec3(0), viewport);
    float dither = leonRng(viewport+fract(time));
    vec3 color = vec3(0.);
    float volume = 0.;
    for (float i = 0.; i <= 1.; i += 1./100.) {
        LeonShape shape = leonMap(eye, time);
        if (shape.dist < 0.001) {
            color += shape.color * shape.friction * mix(1., 1.-i, shape.friction);
            volume += shape.friction;
            if (volume >= 1.) {
                break;
            }
        }
        shape.dist = max(shape.dist, shape.density);
        shape.dist *= .9 + .1 * dither;
        eye += ray * shape.dist;
    }
    return color;
}

vec4 renderLeon2018(vec2 st, float uTime, float uTempo, float uEnergy, float uBass, float uMid, float uHigh) {
    // Convert to shader toy coordinates
    vec2 fragCoord = (st + 0.5) * uResolution.xy;
    
    // Map audio parameters to shader parameters with audio reactivity
    float audioTime = uTime * (1.0 + uEnergy * 0.5 + uBass * 0.2);
    
    // Add audio-reactive rotation and movement
    float bassMod = 1.0 + uBass * 0.3;
    float midMod = 1.0 + uMid * 0.2;
    float highMod = 1.0 + uHigh * 0.1;
    
    // Get base raymarched scene
    vec3 baseColor = leonRaymarch(fragCoord, audioTime, uResolution.xy);
    
    // Modulate colors based on audio frequencies
    baseColor.r *= bassMod;
    baseColor.g *= midMod;
    baseColor.b *= highMod;
    
    // Add energy-based brightness and saturation
    float brightness = 1.0 + uEnergy * 0.8;
    baseColor *= brightness;
    
    // Add frequency-based color shifting
    if (uBass > 0.5) {
        baseColor.r = min(1.0, baseColor.r * 1.5);
    }
    if (uMid > 0.5) {
        baseColor.g = min(1.0, baseColor.g * 1.3);
    }
    if (uHigh > 0.5) {
        baseColor.b = min(1.0, baseColor.b * 1.4);
    }
    
    // Ensure colors are in valid range
    baseColor = clamp(baseColor, 0.0, 1.0);
    
    // Add subtle pulse based on tempo
    float pulse = 1.0 + sin(uTime * uTempo * 0.1) * 0.1 * uEnergy;
    baseColor *= pulse;
    
    return vec4(baseColor, 1.0);
}
