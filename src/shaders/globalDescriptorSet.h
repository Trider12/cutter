#include "common.h"

layout(std430, set = 0, binding = 0) restrict readonly buffer IndicesBlock
{
    uint indices[];
};

layout(std430, set = 0, binding = 1) restrict readonly buffer PositionsBlock
{
    Position positions[];
};

layout(std430, set = 0, binding = 2) restrict readonly buffer NormalUvsBlock
{
    NormalUv normalUvs[];
};

layout(std430, set = 0, binding = 3) restrict readonly buffer TransformsBlock
{
    TransformData transforms[];
};

layout(std430, set = 0, binding = 4) restrict readonly buffer MaterialsBlock
{
    MaterialData materials[];
};

layout(std430, set = 0, binding = 5) uniform LightsBlock
{
    LightingData lightData;
};

layout(std430, set = 0, binding = 6) uniform CameraBlock
{
    CameraData cameraData;
};

layout(set = 0, binding = 7) uniform texture2D brdfLut;
layout(set = 0, binding = 8) uniform textureCube irradianceMaps[];
layout(set = 0, binding = 9) uniform textureCube prefilteredMaps[];
layout(set = 0, binding = 10) uniform sampler linearRepeatSampler;