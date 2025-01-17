#include "globalDescriptorSet.h"
#include "utils.h"

layout(location = 0) out vec3 outPos;

void main()
{
    Position position = positions[gl_VertexIndex];
    vec4 pos = vec4(position.x, position.y, position.z, 1.f);
    vec2 uv = unpackSnorm2x16(normalUvs[gl_VertexIndex].uv) * MAX_UV;
    uv.y = 1.f - uv.y;
    TransformData td = transforms[position.transformIndex];
    mat4 modelMat = sceneData.sceneMat * td.toWorldMat;
    mat4 mvp = sceneData.projMat * sceneData.viewMat * modelMat;
    outPos = (modelMat * pos).xyz;
    gl_Position = vec4(uv * 2.f - 1.f, 0.f, 1.f);
}