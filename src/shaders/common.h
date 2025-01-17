#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
#include "Glm.hpp"
#define float16_t uint16_t
#define vec2 glm::vec2
#define vec3 glm::vec3
#define vec4 glm::vec4
#define mat4 glm::mat4
#else
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#endif // __cplusplus

#define MAX_LIGHTS 16
#define MAX_MODEL_TEXTURES 64
#define MAX_PREFILTERED_MAP_LOD 4
#define MAX_UV 2.f // valid UV coords should in the [-MAX_UV, +MAX_UV] range

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
#define DEBUG_SHOW_BURN        (1u << 10)

#define MATERIAL_HAS_AO_TEX (1u << 0)

struct Position
{
    float16_t x, y, z;
    uint16_t transformIndex;
};

struct NormalUv
{
    uint32_t xyzw; // snorm 4x8
    uint32_t uv;   // snorm 2x16
};

struct MaterialData
{
    uint32_t colorTexIndex;
    uint32_t normalTexIndex;
    uint32_t aoRoughMetalTexIndex;
    uint32_t mask;
};

struct TransformData
{
    mat4 toWorldMat;
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

struct SceneData
{
    mat4 viewMat;
    mat4 projMat;
    mat4 invViewMat;
    mat4 invProjMat;
    mat4 sceneMat;
    uint32_t sceneConfig;
    uint32_t pad[3];
};

struct LineData
{
    vec2 p1;
    vec2 p2;
    vec2 windowRes;
    float width; // in pixels
    float pad;
};

struct CuttingData
{
    vec4 normalAndD;
    float width; // in meters
    uint32_t drawDataReadIndex;
    float pad[2];
};

struct PushData // yes, this can be optimized
{
    uint32_t skyboxIndex;
    uint32_t materialIndex;
    float time;
    uint32_t debugFlags;
    LineData lineData;
    CuttingData cuttingData;
};

struct DrawIndirectData
{
    // start of VkDrawIndexedIndirectCommand
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
    // end of VkDrawIndexedIndirectCommand
    uint32_t vertexCount;
};

#ifdef __cplusplus
#undef float16_t
#undef vec2
#undef vec3
#undef vec4
#undef mat4
static_assert(sizeof(Position) == 8, "");
static_assert(sizeof(NormalUv) == 8, "");
static_assert(sizeof(MaterialData) == 16, "");
static_assert(sizeof(TransformData) == 64, "");
static_assert(sizeof(LightData) == 24, "");
static_assert(sizeof(LightingData) == sizeof(LightData) * MAX_LIGHTS + 4, "");
static_assert(sizeof(SceneData) == sizeof(float[4]) * (4 * 5 + 1), "");
static_assert(sizeof(LineData) == sizeof(float[2]) * 4, "");
static_assert(sizeof(CuttingData) == sizeof(float[4]) * 2, "");
#endif // __cplusplus

#endif // !COMMON_H