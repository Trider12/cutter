#include "common.h"

#ifdef COMPUTE
#define graphicsReadonly
#else
#define graphicsReadonly readonly
#endif // COMPUTE

layout(std430, set = 0, binding = 0) restrict graphicsReadonly buffer DrawIndirectBlock
{
    DrawIndirectData drawData[2]; // one for reading, one for writing
};

layout(std430, set = 0, binding = 1) restrict readonly buffer ReadIndicesBlock
{
    uint readIndices[];
};

layout(std430, set = 0, binding = 2) restrict graphicsReadonly buffer WriteIndicesBlock
{
    uint writeIndices[];
};

layout(std430, set = 0, binding = 3) restrict graphicsReadonly buffer PositionsBlock
{
    Position positions[];
};

layout(std430, set = 0, binding = 4) restrict graphicsReadonly buffer NormalUvsBlock
{
    NormalUv normalUvs[];
};

layout(std430, set = 0, binding = 5) restrict readonly buffer TransformsBlock
{
    TransformData transforms[];
};

layout(std430, set = 0, binding = 6) restrict readonly buffer MaterialsBlock
{
    MaterialData materials[];
};

layout(std430, set = 0, binding = 7) uniform LightsBlock
{
    LightingData lightData;
};

layout(std430, set = 0, binding = 8) uniform CameraBlock
{
    SceneData sceneData;
};

layout(set = 0, binding = 10) uniform sampler linearClampSampler;
layout(set = 0, binding = 11) uniform sampler linearRepeatSampler;
layout(set = 0, binding = 12) uniform sampler nearestClampSampler;
layout(set = 0, binding = 13) uniform sampler nearestRepeatSampler;

layout(set = 0, binding = 20) uniform texture2D brdfLut;
layout(set = 0, binding = 21) uniform textureCube skyboxTextures[];
layout(set = 0, binding = 22) uniform textureCube irradianceMaps[];
layout(set = 0, binding = 23) uniform textureCube prefilteredMaps[];
layout(set = 0, binding = 24) uniform texture2D burnMapTexture;
layout(set = 0, binding = 25) uniform texture2D framebufferTextureMips[];
layout(set = 0, binding = 26, rgba16f) uniform image2D framebufferImageMips[];

layout(set = 0, binding = 30) uniform texture2D materialTextures[MAX_MODEL_TEXTURES];
