#include "common.h"

layout(push_constant) uniform ConstantBlock
{
    LineData lineData;
};

void main()
{
    vec2 vec = normalize(lineData.p2 - lineData.p1);
    vec2 norm = vec2(-vec.y, vec.x);
    float offset = lineData.width * 0.5f;

    uint indices[] = uint[](0, 1, 2, 3, 2, 1);
    uint index = indices[gl_VertexIndex];
//    vec2 vertices[4] = vec2[](
//        vec2(lineData.p1 + (-vec - norm) * offset),
//        vec2(lineData.p1 + (-vec + norm) * offset),
//        vec2(lineData.p2 + (vec - norm) * offset),
//        vec2(lineData.p2 + (vec + norm) * offset)
//    );
//    vec2 vertex = vertices[index];

    uint blend = (index >> 1) & 1; // 0 or 1
    int vecSign = int(index & 2) - 1; // -1 or 1
    int normSign = int((index << 1) & 2) - 1; // -1 or 1
    vec2 vertex = mix(lineData.p1, lineData.p2, blend) + (vecSign * vec + normSign * norm) * offset;
    gl_Position = vec4(vertex / lineData.windowRes * 2.f - 1.f, 0.f, 1.f);
}