#include "common.h"

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform ConstantBlock
{
    layout(offset = 16) LineData lineData;
};

float lineSdfNormalized(vec2 p, vec2 a, vec2 b, float r)
{
    vec2 ba = b - a, pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.f, 1.f);
    vec2 q = pa - h * ba;
    float d = length(q);
    return 1.f - d / r;
}

const vec3 coreColor = vec3(1.f);
const vec3 glowColor = vec3(1.f, 0.f, 0.f);
const float coreAlpha = 1.f;
const float coreEdge = 0.15f;

void main()
{
    float radius = lineData.width * 0.5f;
    float sdf = lineSdfNormalized(gl_FragCoord.xy, lineData.p1, lineData.p2, radius);
    float dist = 1.f - sdf;
    float colorBlend = smoothstep(coreEdge * 0.5f, coreEdge, dist);
    float alphaBlend = step(coreEdge, dist);
    vec3 color = mix(coreColor, glowColor, colorBlend);
    float glowAlpha = pow(clamp(sdf / (1.f - coreEdge), 0.f, 1.f), 3);
    float alpha = mix(coreAlpha, glowAlpha, alphaBlend);
    outFragColor = vec4(color, alpha);
}