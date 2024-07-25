#include "globalDescriptorSet.h"
#include "utils.h"

layout(set = 1, binding = 1) uniform textureCube skyboxTextures[];

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outFragColor;

layout(push_constant) uniform ConstantBlock
{
    uint skyboxIndex;
};

void main()
{
    outFragColor = texture(samplerCube(skyboxTextures[skyboxIndex], linearRepeatSampler), inPos).rgb;
}