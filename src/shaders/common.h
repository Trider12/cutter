#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
#include "Glm.hpp"
#define float16_t uint16_t
#define uint uint32_t
#define vec3 glm::vec3
#define mat4 glm::mat4
#else
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_scalar_block_layout : require // TODO: remove this
#endif // __cplusplus

#define MAX_LIGHTS 16
#define MAX_MODEL_TEXTURES 64
#define CONFIG_SHOW_WIREFRAME 1 << 0
#define CONFIG_USE_NORMAL_MAP 1 << 1

struct Position
{
    float16_t x, y, z;
    uint16_t transformIndex;
};

struct NormalUv
{
    int8_t x, y, z; // snorm
    uint8_t effectMask;
    float16_t u, v; // unorm
};

struct MaterialData
{
    uint colorTexIndex;
    uint normalTexIndex;
    uint aoRoughMetalTexIndex;
};

struct TransformData
{
    mat4 toWorldMat;
    mat4 toLocalMat;
};

struct LightData
{
    float r, g, b, radius;
    float16_t x, y, z;
    float16_t intensity;
};

struct LightingData
{
    uint16_t dirLightCount;
    uint16_t pointLightCount;
    LightData lights[MAX_LIGHTS]; // dir lights then point lights
};

struct CameraData
{
    mat4 sceneMat;
    mat4 viewMat;
    mat4 invViewMat;
    mat4 projMat;
    uint sceneConfig;
};

#ifdef __cplusplus
#undef float16_t
#undef uint
#undef vec3
#undef mat4
static_assert(sizeof(Position) == 8, "");
static_assert(sizeof(NormalUv) == 8, "");
static_assert(sizeof(MaterialData) == 12, "");
static_assert(sizeof(TransformData) == 128, "");
static_assert(sizeof(LightData) == 24, "");
#endif // __cplusplus

#endif // !COMMON_H