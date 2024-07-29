#include "common.h"

layout(location = 0) out vec3 outPos;

layout(std430, set = 1, binding = 0) uniform CameraBlock
{
    CameraData cameraData;
};

void main()
{
    vec4 pos = vec4(((gl_VertexIndex << 2) & 4) - 1, ((gl_VertexIndex << 1) & 4) - 1, 0.f, 1.f);
    mat4 invProjMat = inverse(cameraData.projMat);
    mat3 invViewMat = transpose(mat3(cameraData.viewMat));
    outPos = invViewMat * (invProjMat * pos).xyz;
    gl_Position = pos;
}