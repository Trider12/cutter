#ifndef UTILS_H
#define UTILS_H

#define PI              3.14159265359
#define TWO_PI          6.28318530718
#define PI_OVER_TWO     1.57079632679
#define ONE_OVER_PI     0.31830988618
#define ONE_OVER_TWO_PI 0.15915494309
#define EPSILON         0.00001

bool isTextureValid(uint textureIndex)
{
    return bool(~textureIndex);
}

vec3 unpackSnorm8(ivec3 snorm)
{
    const float scale = float((1 << 7) - 1);
    return snorm / scale;
}

float unpackUnorm16(uint unorm)
{
    const float scale = float((1 << 16) - 1);
    return unorm / scale;
}

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 reinhardTonemap(vec3 color)
{
    return color / (1.f + luminance(color));
}

float radicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley2d(uint i, uint N)
{
    return vec2(float(i) / float(N), radicalInverseVdC(i));
}

#endif // !UTILS_H