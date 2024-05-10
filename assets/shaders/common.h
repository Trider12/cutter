#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
#include <stdint.h>
typedef uint16_t float16_t;
#else
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#endif // __cplusplus

struct Position
{
    float16_t x, y, z;
    uint16_t matrixIndex;
};

struct NormalUv
{
    uint8_t nx, ny, nz;
    uint8_t config;
    uint16_t u, v;
};

#ifdef __cplusplus
static_assert(sizeof(Position) == 8, "");
static_assert(sizeof(NormalUv) == 8, "");
#endif // __cplusplus

#endif // !COMMON_H