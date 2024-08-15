#include "modelDescriptorSet.h"
#include "utils.h"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec2 outBurn; // time and alpha

layout(push_constant) uniform ConstantBlock
{
    layout(offset = 8) float time;
    layout(offset = 48) CuttingData cuttingData;
};

const float burnWidth = 0.025f; // meters

void main()
{
    float absDistToPlane = abs(dot(vec4(inPos, 1.f), cuttingData.normalAndD));
    float fbm = fbm(gl_FragCoord.xy * 0.1f);
    float base = cuttingData.width * 0.5f;
    float offset = burnWidth * mix(1.f, fbm, 0.75f);
    float threshold = base + offset;

    if(absDistToPlane > threshold)
        discard;

    float burnAlpha = 1.f - smoothstep(base, threshold, absDistToPlane);
    outBurn = vec2(time, burnAlpha);
}