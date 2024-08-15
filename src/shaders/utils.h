#ifndef UTILS_H
#define UTILS_H

#define PI              3.14159265359
#define TWO_PI          6.28318530718
#define PI_OVER_TWO     1.57079632679
#define ONE_OVER_PI     0.31830988618
#define ONE_OVER_TWO_PI 0.15915494309
#define EPSILON         0.00001

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

vec3 mix3(vec3 colors[3], float value)
{
    vec3 m1 = mix(colors[0], colors[1], 2.f * value);
    vec3 m2 = mix(colors[1], colors[2], 2.f * value - 1.f);
    return mix(m1, m2, step(0.5f, value));
}

vec3 mix4(vec3 colors[4], float value)
{
    vec3 m1 = mix(colors[0], colors[1], 3.f * value);
    vec3 m2 = mix(colors[1], colors[2], 3.f * value - 1.f);
    vec3 m3 = mix(colors[2], colors[3], 3.f * value - 2.f);
    vec3 mixes[] = vec3[](m1, m2, m3);
    return mix3(mixes, value);
}

// https://thebookofshaders.com/13/
float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 st)
{
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
        (c - a) * u.y * (1.0 - u.x) +
        (d - b) * u.x * u.y;
}

#define OCTAVES 6
float fbm(vec2 st)
{
    // Initial values
    float value = 0.0;
    float amplitude = 0.5;
    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++)
    {
        value += amplitude * noise(st);
        st *= 2.;
        amplitude *= 0.5;
    }
    return value;
}

#endif // !UTILS_H