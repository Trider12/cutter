#include "modelDescriptorSet.h"
#include "utils.h"

struct VsOut
{
    vec3 pos;
    vec3 norm;
    vec3 bary;
    vec2 uv;
    uint transformIndex;
};

layout(location = 0) out VsOutBlock
{
    VsOut vsOut;
};

void main()
{
    uint index = indices[gl_VertexIndex];
    vec4 pos = vec4(positions[index].x, positions[index].y, positions[index].z, 1.f);
    vec4 norm = vec4(unpackSnorm8(ivec3(normalUvs[index].x, normalUvs[index].y, normalUvs[index].z)), 0.f);
    vec2 uv = vec2(normalUvs[index].u, normalUvs[index].v);
    uint transformIndex = uint(positions[index].transformIndex);
    TransformData td = transforms[transformIndex];
    mat4 modelMat = cameraData.sceneMat * td.toWorldMat;
    mat4 mvp = cameraData.projMat * cameraData.viewMat * modelMat;
    uint mod = gl_VertexIndex % 3;

    gl_Position = mvp * pos;
    vsOut.pos = (modelMat * pos).xyz;
    vsOut.norm = (modelMat * norm).xyz;
    vsOut.uv = uv;
    vsOut.bary = vec3(mod == 0, mod == 1, mod == 2);
    vsOut.transformIndex = transformIndex;
}