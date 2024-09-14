#include "globalDescriptorSet.h"

void main()
{
    Position position = positions[gl_VertexIndex];
    vec4 pos = vec4(position.x, position.y, position.z, 1.f);
    TransformData td = transforms[position.transformIndex];
    mat4 modelMat = sceneData.sceneMat * td.toWorldMat;
    mat4 mvp = sceneData.projMat * sceneData.viewMat * modelMat;

    gl_Position = mvp * pos;
}