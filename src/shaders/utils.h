#ifndef UTILS_H
#define UTILS_H

#define PI              3.14159265359
#define TWO_PI          6.28318530718
#define ONE_OVER_PI     0.31830988618
#define ONE_OVER_TWO_PI 0.15915494309

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

#endif // !UTILS_H