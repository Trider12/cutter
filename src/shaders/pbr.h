#ifndef PBR_H
#define PBR_H

#include "utils.h"

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.f - F0) * pow(clamp(1.f - cosTheta, 0.f, 1.f), 5.f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.f - roughness), F0) - F0) * pow(clamp(1.f - cosTheta, 0.f, 1.f), 5.f);
}

float distributionGGX(float NdotH, float roughness)
{
    float a2 = roughness * roughness * roughness * roughness;
    float k = 1.f + NdotH * NdotH * (a2 - 1.f);
    return a2 / (PI * k * k);
}

float geometrySchlickKarisGGX(float NdotS, float roughness)
{
    float a = roughness * roughness;
    return 2.f * NdotS / max(NdotS * (2.f - a) + a, EPSILON);
}

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return geometrySchlickKarisGGX(NdotV, roughness) * geometrySchlickKarisGGX(NdotL, roughness);
}

vec3 importanceSampleGGX(vec2 h2d, vec3 T, vec3 B, vec3 N, float a)
{
    float cosTheta2 = (1.f - h2d.x) / (1.f + (a * a - 1.f) * h2d.x);
    float cosTheta = sqrt(cosTheta2);
    float sinTheta = sqrt(1.f - cosTheta2);
    float phi = TWO_PI * h2d.y;
    vec3 tangentSampleVec = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    vec3 worldSampleVec = tangentSampleVec.x * T + tangentSampleVec.y * B + tangentSampleVec.z * N;

    return worldSampleVec;
}

void directBRDF(float NdotV, float NdotL, float NdotH, float HdotV, vec3 albedo, float roughness, float metallic, out vec3 fDiff, out vec3 fSpec)
{
    vec3 F0 = mix(vec3(0.04f), albedo, metallic);
    vec3 F = fresnelSchlick(HdotV, F0);
    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);

    vec3 kDiff = mix(vec3(1.f) - F, vec3(0.f), metallic);
    fDiff = kDiff * albedo / PI;
    fSpec = F * G * D / max(4.f * NdotV * NdotL, EPSILON);
}

void envBRDF(vec3 albedo, float roughness, float metallic, vec3 irradiance, vec3 prefiltered, vec2 brdf, out vec3 fDiff, out vec3 fSpec)
{
    vec3 F0 = mix(vec3(0.04f), albedo, metallic);
    vec3 kDiff = mix(vec3(1.f) - F0, vec3(0.f), metallic);
    fDiff = kDiff * irradiance * albedo;
    fSpec = prefiltered * (F0 * brdf.r + brdf.g);
}

#endif // !PBR_H