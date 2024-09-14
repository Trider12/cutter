#include "globalDescriptorSet.h"

struct VsOut
{
    vec3 pos;
    vec3 norm;
    vec2 uv;
};

layout(location = 0) out VsOutBlock
{
    VsOut vsOut;
};

void main()
{
    Position position = positions[gl_VertexIndex];
    vec4 pos = vec4(position.x, position.y, position.z, 1.f);
    vec4 norm = unpackSnorm4x8(normalUvs[gl_VertexIndex].xyzw);
    vec2 uv = unpackSnorm2x16(normalUvs[gl_VertexIndex].uv) * MAX_UV;
    TransformData td = transforms[position.transformIndex];
    mat4 modelMat = sceneData.sceneMat * td.toWorldMat;
    mat4 mvp = sceneData.projMat * sceneData.viewMat * modelMat;

    gl_Position = mvp * pos;
    vsOut.pos = (modelMat * pos).xyz;
    vsOut.norm = (modelMat * norm).xyz;
    vsOut.uv = uv;
}