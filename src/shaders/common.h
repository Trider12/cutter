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
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#endif // __cplusplus

#define MAX_LIGHTS 16
#define MAX_MODEL_TEXTURES 64
#define MAX_PREFILTERED_MAP_LOD 4

#define SCENE_SHOW_WIREFRAME (1u << 0)
#define SCENE_USE_NORMAL_MAP (1u << 1)
#define SCENE_USE_LIGHTS     (1u << 2)
#define SCENE_USE_IBL        (1u << 3)

#define DEBUG_SHOW_COLOR       (1u << 0)
#define DEBUG_SHOW_NORMAL      (1u << 1)
#define DEBUG_SHOW_AO          (1u << 2)
#define DEBUG_SHOW_ROUGHNESS   (1u << 3)
#define DEBUG_SHOW_METALLIC    (1u << 4)
#define DEBUG_SHOW_EMISSIVE    (1u << 5)
#define DEBUG_SHOW_IRRADIANCE  (1u << 6)
#define DEBUG_SHOW_PREFILTERED (1u << 7)
#define DEBUG_SHOW_DIFFUSE     (1u << 8)
#define DEBUG_SHOW_SPECULAR    (1u << 9)

#define MATERIAL_HAS_AO_TEX (1u << 0)

struct Position
{
    float16_t x, y, z;
    uint16_t transformIndex;
};

struct NormalUv
{
    int8_t x, y, z; // snorm
    uint8_t effectMask;
    float16_t u, v;
};

struct MaterialData
{
    uint colorTexIndex;
    uint normalTexIndex;
    uint aoRoughMetalTexIndex;
    uint mask;
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
static_assert(sizeof(MaterialData) == 16, "");
static_assert(sizeof(TransformData) == 128, "");
static_assert(sizeof(LightData) == 24, "");
#endif // __cplusplus

#endif // !COMMON_H