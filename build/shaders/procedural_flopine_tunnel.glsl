// Code by Flopine
// Thanks to wsmind, leon, lsdlive, lamogui and XT95 for teaching me! :) <3

#define ITER 100.

vec2 moda (vec2 p, float per)
{
    float a = atan(p.y,p.x);
    float l = length(p);
    a = mod(a-per/2.,per)-per/2.;
    return vec2(cos(a),sin(a))*l;
}

vec2 mo (vec2 p, vec2 d)
{
    p.x = abs(p.x)-d.x;
    p.y = abs(p.y)-d.y;
    if (p.y>p.x) p.xy = p.yx;
    return p;
}

mat2 rot (float a)
{
    float c = cos(a);
    float s = sin(a);
    return mat2(c,s,-s,c);    
}

float smin(float a, float b, float k) {
    float h = clamp(.5 + .5*(b - a) / k, 0., 1.);
    return mix(b, a, h) - k * h * (1. - h);
}

float stmin(float a, float b, float k, float n) {
    float s = k / n;
    float u = b - k;
    return min(min(a, b), .5 * (u + a + abs((mod(u - a + s, 2. * s)) - s)));
}

vec2 path(float t) 
{
    float a = sin(t*.2 + 1.5), b = sin(t*.2);
    return vec2(a, a*b);
}

// iq's palette
vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 2.*3.141592*(c*t+d) );
}

float cyl (vec2 p, float r)
{
    return length(p)-r;
}

float od(vec3 p, float s) {
    return dot((p), normalize(sign(p))) - s;
}

float adn (vec3 p, float time)
{
    p.xz *= rot(p.y*0.5+time);
    p.xz = moda(p.xz, 2.*3.141592/5.);

    p.x -= 2.;
        
    return cyl(p.xz,.3);
}

float prim1 (vec3 p, float per, float time)
{
    float ad = adn(p, time);
    //p.y += iTime;
    p.y = mod(p.y-per/2.,per)-per/2.;
    return stmin(ad,od(p, 1.),0.7,4.);
}

float tunnel (vec3 p, float time)
{
    p.yz *= rot(3.141592/2.);
    p.xz *= rot(p.y*0.3);
    p.xz  = moda(p.xz, 2.*3.141592/5.);
    p.x -= 6.;
    return prim1(p,2.5, time);
}

float map (vec3 p, float time)
{
    p.xy += path(p.z);
    return tunnel(p, time);
}

vec3 camera(vec3 ro, vec2 uv, vec3 ta) {
    vec3 fwd = normalize(ta - ro);
    vec3 left = cross(vec3(0, 1, 0), fwd);
    vec3 up = cross(fwd, left);
    return normalize(fwd + uv.x*left + up*uv.y);
}

vec4 renderFlopineTunnel(vec2 st, float uTime, float uTempo, float uEnergy, float uBass, float uMid, float uHigh) {
    // Convert to shader's expected coordinate system
    vec2 uv = 2.0 * st - 1.0;
    uv.x *= uResolution.x / uResolution.y;
    
    // Time with audio modulation
    float time = uTime + uTempo * 0.1;
    
    // Camera movement with audio influence
    float dt = time * 3. + uBass * 0.5;
    vec3 ro = vec3(0.001, 0.5, -9. + dt);
    vec3 ta = vec3(0, 0, dt);
    vec3 rd;
    
    ro.xy += path(ro.z);
    ta.xy += path(ta.z);
    rd = camera(ro, uv, ta);
    
    vec3 p;
    float t;
    float shad = 0.;
    
    // Raymarching with audio-reactive step size
    float stepSize = 0.12 * (1.0 - uEnergy * 0.05);
    
    for (float i=0.; i<ITER; i++)
    {
        p = ro+rd*t;
        float d = map(p, time);
        
        // Audio-reactive threshold
        float threshold = (2.0/uResolution.y) * (1.0/3.0) * t * (1.0 + uMid * 0.2);
        
        if (d < threshold)
        {
            shad = i/ITER;
            break;
        }
        t += d * stepSize;
    }
    
    // Time varying pixel color with audio reactivity
    vec3 col = vec3(1.0 - shad);
    
    // Enhanced palette with audio modulation
    float paletteMod = uv.y * 0.6 + uBass * 0.1;
    vec3 palette = pal(paletteMod,
                      vec3(0.5),
                      vec3(0.5),
                      vec3(1.0 + uHigh * 0.1),
                      vec3(0.3, 0.2, 0.2));
    
    // Distance fog with audio influence
    float fogFactor = 0.003 * (1.0 + uEnergy * 0.001);
    col = mix(col, palette * 0.8, 1.0 - exp(-fogFactor * t * t));
    
    // Add audio-reactive color boost
    col.r *= 1.0 + uBass * 0.2;
    col.g *= 1.0 + uMid * 0.15;
    col.b *= 1.0 + uHigh * 0.25;
    
    // Gamma correction
    col = pow(col, vec3(2.2));
    
    return vec4(col, 1.0);
}
