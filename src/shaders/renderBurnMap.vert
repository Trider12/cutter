#include "modelDescriptorSet.h"
#include "utils.h"

layout(location = 0) out vec3 outPos;

void main()
{
    uint index = readIndices[gl_VertexIndex];
    vec4 pos = vec4(positions[index].x, positions[index].y, positions[index].z, 1.f);
    vec2 uv = vec2(normalUvs[index].u, 1.f - normalUvs[index].v);
    TransformData td = transforms[positions[index].transformIndex];
    mat4 modelMat = sceneData.sceneMat * td.toWorldMat;
    mat4 mvp = sceneData.projMat * sceneData.viewMat * modelMat;
    outPos = (modelMat * pos).xyz;
    gl_Position = vec4(uv * 2.f - 1.f, 0.f, 1.f);
}