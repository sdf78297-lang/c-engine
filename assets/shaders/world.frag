#version 330 core

in vec2 vUV;
in vec3 vNormal;
in vec3 vFragPos;

uniform sampler2D uAlbedo;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform vec3 uViewPos;
uniform vec3 uTint;
uniform float uUVScale;

uniform float uVignette;
uniform float uAberration;
uniform float uInstability;
uniform float uFlicker;
uniform vec2  uResolution;
uniform int   uDebugGeometry;

// Point lights
struct PLight {
    vec3  pos;
    vec3  color;
    float radius;
};
#define MAX_PLIGHTS 32
uniform PLight uPLights[MAX_PLIGHTS];
uniform int    uPointLightCount;

out vec4 FragColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

vec3 filmic(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float grid_line(vec2 p, float scale, float width) {
    vec2 g = abs(fract(p * scale) - 0.5);
    float d = min(g.x, g.y);
    return 1.0 - smoothstep(width, width + 0.018, d);
}

void main() {
    float unstable = clamp(uInstability, 0.0, 1.0);
    float uvScale = uUVScale > 0.001 ? uUVScale : 1.0;
    vec4 albedo = texture(uAlbedo, vUV * uvScale) * vec4(uTint, 1.0);

    vec3 N = normalize(vNormal);
    float floorMask = smoothstep(0.62, 0.96, N.y);
    float ceilingMask = smoothstep(0.62, 0.96, -N.y);
    float wallMask = 1.0 - clamp(floorMask + ceilingMask, 0.0, 1.0);
    float floorGrid = grid_line(vFragPos.xz, 1.14, 0.030) * floorMask;
    float ceilingGrid = grid_line(vFragPos.xz + vec2(0.17, 0.11), 0.84, 0.026) * ceilingMask;
    float wallAlongX = step(abs(N.z), abs(N.x));
    vec2 wallUV = mix(vec2(vFragPos.x, vFragPos.y),
                      vec2(vFragPos.z, vFragPos.y),
                      wallAlongX);
    float wallPanel = grid_line(wallUV, 0.58, 0.025) * wallMask * 0.55;
    float dirtCells = hash21(floor(vFragPos.xz * 1.55) + floor(vFragPos.yy * 0.9));
    float lowGrime = smoothstep(1.35, 0.12, vFragPos.y) * wallMask;
    float cornerDirt = (floorGrid * 0.16 + ceilingGrid * 0.08 + wallPanel * 0.12 + lowGrime * 0.18);
    cornerDirt += smoothstep(0.66, 1.0, dirtCells) * 0.08 * (floorMask + lowGrime);
    albedo.rgb *= 1.0 - clamp(cornerDirt, 0.0, 0.34);

    if (uDebugGeometry != 0) {
        vec3 floorColor = vec3(0.18, 0.45, 0.62);
        vec3 wallColor = vec3(0.70, 0.74, 0.70);
        vec3 ceilColor = vec3(0.84, 0.80, 0.58);
        vec3 classColor = floorColor * floorMask + ceilColor * ceilingMask + wallColor * wallMask;
        vec3 debugColor = mix(classColor, albedo.rgb, 0.22);
        debugColor += vec3(0.06, 0.08, 0.09) * floorGrid;
        debugColor += vec3(0.10, 0.08, 0.03) * ceilingGrid;
        debugColor += vec3(0.08, 0.10, 0.10) * wallPanel;
        FragColor = vec4(clamp(debugColor, 0.0, 1.0), 1.0);
        return;
    }

    vec3 L = normalize(uLightPos - vFragPos);

    float dist = length(uLightPos - vFragPos);
    float atten = 1.0 / (1.0 + 0.020 * dist + 0.0018 * dist * dist);

    float time = uFlicker;
    float flicker = 0.08 + 0.07 * sin(time * 8.7) + 0.045 * sin(time * 19.1 + 1.4);
    flicker += unstable * (0.05 * sin(time * 29.0 + vFragPos.z) + 0.035 * hash21(gl_FragCoord.xy * 0.013 + time));
    flicker = clamp(flicker, 0.0, 0.34);
    float lambert = max(dot(N, L), 0.0);
    float twoSidedWall = abs(dot(N, L));
    float wallFill = mix(lambert, twoSidedWall, wallMask * 0.72);
    float diff = wallFill * atten * (1.0 - flicker);

    vec3 V = normalize(uViewPos - vFragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 42.0) * atten * 0.16;
    spec += floorMask * pow(max(dot(reflect(-L, N), V), 0.0), 20.0) * atten * 0.10;

    vec3 readableBase = vec3(0.052, 0.062, 0.066) + wallMask * vec3(0.030, 0.034, 0.032);
    vec3 color = (readableBase + uAmbient + uLightColor * (diff + spec) * 1.16) * albedo.rgb;

    // Accumulate point lights
    for (int i = 0; i < MAX_PLIGHTS; i++) {
        if (i >= uPointLightCount) break;
        vec3 pDir = uPLights[i].pos - vFragPos;
        float pDist = length(pDir);
        float pR = uPLights[i].radius;
        if (pDist > pR * 2.0) continue;
        vec3 pL = normalize(pDir);
        float pAtten = 1.0 / (1.0 + 0.35 * pDist + 0.44 * (pDist * pDist) / (pR * pR));
        // Smooth falloff at radius edge
        pAtten *= 1.0 - smoothstep(pR * 0.7, pR * 1.5, pDist);
        float pLambert = max(dot(N, pL), 0.0);
        float pDiff = mix(pLambert, abs(dot(N, pL)), wallMask * 0.62) * pAtten;
        vec3 pH = normalize(pL + V);
        float pSpec = pow(max(dot(N, pH), 0.0), 32.0) * pAtten * 0.12;
        color += uPLights[i].color * (pDiff + pSpec) * albedo.rgb;
    }

    float viewDist = length(uViewPos - vFragPos);
    float fog = smoothstep(6.0, 23.0, viewDist);
    fog += smoothstep(0.15, 1.25, 1.45 - vFragPos.y) * 0.10;
    fog = clamp(fog, 0.0, 0.78);
    vec3 fogColor = vec3(0.022, 0.031, 0.034);

    float grime = smoothstep(1.25, 0.10, vFragPos.y) * 0.16;
    color *= 1.0 - grime;
    color *= vec3(0.82, 0.91, 0.98);
    color += floorMask * vec3(0.012, 0.018, 0.020);
    color = mix(color, fogColor, fog);

    // Use actual screen resolution for vignette
    vec2 res = uResolution.x > 1.0 ? uResolution : vec2(1280.0, 720.0);
    vec2 screen = gl_FragCoord.xy / res;
    float vigShape = smoothstep(0.82, 0.22, distance(screen, vec2(0.5)));
    float vig = mix(0.52, 1.0, vigShape);
    color *= mix(1.0, vig, 0.56 + clamp(uVignette, 0.0, 1.0) * 0.32);

    // Chromatic aberration — offset R and B channels outward from center
    float abr = clamp(uAberration, 0.0, 1.0);
    if (abr > 0.01) {
        vec2 dir = screen - vec2(0.5);
        float edgeDist = length(dir);
        float shift = abr * edgeDist * 0.012;
        // We can only do screen-space CA on the final color as a post approximation:
        // shift the red channel outward and blue inward
        color.r *= 1.0 + shift * 2.5;
        color.b *= 1.0 - shift * 1.8;
        // Slight green desaturation at edges
        color.g *= 1.0 - shift * 0.4;
    }

    float edge = smoothstep(0.42, 0.74, distance(screen, vec2(0.5)));
    color = mix(color, color * vec3(1.07, 0.90, 0.86) + vec3(0.010, 0.0, 0.0),
                unstable * edge * 0.22);

    float scan = sin(gl_FragCoord.y * 2.2 + time * (16.0 + unstable * 28.0)) * 0.5 + 0.5;
    color *= 1.0 - scan * mix(0.018, 0.060, unstable);
    float grain = hash21(gl_FragCoord.xy + time * 31.0) - 0.5;
    color += grain * mix(0.016, 0.050, unstable);

    float fracture = step(0.985, hash21(floor(gl_FragCoord.xy / vec2(18.0, 4.0)) + floor(time * 5.0)));
    color += vec3(0.16, 0.02, 0.01) * fracture * unstable * 0.045;
    color = mix(color, vec3(dot(color, vec3(0.299, 0.587, 0.114))), 0.08 + unstable * 0.10);

    color = filmic(max(color, vec3(0.0)) * 1.24);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, albedo.a);
}
