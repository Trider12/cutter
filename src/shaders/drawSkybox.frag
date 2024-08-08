#include "modelDescriptorSet.h"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform ConstantBlock
{
    uint skyboxIndex;
};

void main()
{
    outFragColor = vec4(texture(samplerCube(skyboxTextures[skyboxIndex], linearRepeatSampler), inPos).rgb, 1.f);
}