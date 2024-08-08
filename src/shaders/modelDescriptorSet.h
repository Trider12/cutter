#include "common.h"

#ifdef COMPUTE
#define QUALIFIER
#else
#define QUALIFIER readonly
#endif // COMPUTE

layout(std430, set = 0, binding = 0) restrict QUALIFIER buffer DrawIndirectBlock
{
    DrawIndirectData drawData[2]; // one for reading, one for writing
};

layout(std430, set = 0, binding = 1) restrict readonly buffer ReadIndicesBlock
{
    uint readIndices[];
};

layout(std430, set = 0, binding = 2) restrict QUALIFIER buffer WriteIndicesBlock
{
    uint writeIndices[];
};

layout(std430, set = 0, binding = 3) restrict QUALIFIER buffer PositionsBlock
{
    Position positions[];
};

layout(std430, set = 0, binding = 4) restrict QUALIFIER buffer NormalUvsBlock
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
    CameraData cameraData;
};

layout(set = 0, binding = 9) uniform texture2D brdfLut;
layout(set = 0, binding = 10) uniform textureCube irradianceMaps[];
layout(set = 0, binding = 11) uniform textureCube prefilteredMaps[];
layout(set = 0, binding = 12) uniform sampler linearClampSampler;
layout(set = 0, binding = 13) uniform sampler linearRepeatSampler;