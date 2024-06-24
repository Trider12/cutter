#include "utils.h"

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(float NdotH, float roughness)
{
    float a2 = roughness * roughness * roughness * roughness;
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float maskingSchlickKarisGGX(float NdotS, float roughness)
{
    float k = 0.25f * (roughness + 1.f) * (roughness + 1.f);
    return 2.f * NdotS / (NdotS * (2.f - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return maskingSchlickKarisGGX(NdotV, roughness) * maskingSchlickKarisGGX(NdotL, roughness);
}

vec3 BRDF(float NdotV, float NdotL, float NdotH, float HdotV, vec3 albedo, float roughness, float metallic)
{
    vec3 F0 = mix(vec3(0.04f), albedo, metallic);
    vec3 F = fresnelSchlick(HdotV, F0);
    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);

    vec3 fSpec = F * G * D / (4.f * NdotV * NdotL + 0.0001f);
    vec3 kDiff = mix(vec3(1.f) - F, vec3(0.f), metallic);
    vec3 fDiff = kDiff * albedo / PI;

    return fDiff + fSpec;
}