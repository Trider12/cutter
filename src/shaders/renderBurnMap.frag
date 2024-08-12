#include "modelDescriptorSet.h"

layout(location = 0) in vec3 inPos;
layout(location = 0) out uint outFragColor;

layout(push_constant) uniform ConstantBlock
{
    CuttingData cuttingData;
};

void main()
{
    float distToPlane = dot(vec4(inPos, 1.f), cuttingData.normalAndD);
    if(abs(distToPlane) > cuttingData.width)
        discard;

    outFragColor = 1;
}