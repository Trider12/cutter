#include "globalDescriptorSet.h"

layout(std430, set = 1, binding = 0) restrict readonly buffer SkyboxPositionsBlock
{
    Position skyboxPositions[];
};

layout(location = 0) out vec3 outPos;

void main()
{
    outPos = vec3(skyboxPositions[gl_VertexIndex].x, skyboxPositions[gl_VertexIndex].y, skyboxPositions[gl_VertexIndex].z);
    mat4 viewProj = cameraData.projMat * mat4(mat3(cameraData.viewMat));
    vec4 pos = viewProj * vec4(outPos, 1.f);
    pos.z = 0.f;
    gl_Position = pos;
}