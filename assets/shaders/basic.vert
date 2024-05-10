#version 450
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(std430, set = 0, binding = 0) readonly buffer Positions
{
    Position positions[];
};

layout(std430, set = 0, binding = 1) readonly buffer NormalUvs
{
    NormalUv normalUvs[];
};

layout(std430, set = 0, binding = 2) readonly buffer Matrices
{
    mat4 matrices[];
};

layout(location = 0) out vec3 outColor;

void main() 
{
    vec3 pos = vec3(positions[gl_VertexIndex].x, positions[gl_VertexIndex].y, positions[gl_VertexIndex].z);
    mat4 mat = matrices[int(positions[gl_VertexIndex].matrixIndex)];
    gl_Position = mat * vec4(pos, 1.f);
    outColor = vec3(1.f, 0.f, 0.f);
}
