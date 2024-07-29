#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outFragColor;

layout(set = 1, binding = 1) uniform sampler linearRepeatSampler;
layout(set = 1, binding = 2) uniform textureCube skyboxTextures[];

layout(push_constant) uniform ConstantBlock
{
    uint skyboxIndex;
};

void main()
{
    outFragColor = texture(samplerCube(skyboxTextures[skyboxIndex], linearRepeatSampler), inPos).rgb;
}