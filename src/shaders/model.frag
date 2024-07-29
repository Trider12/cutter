#include "fragmentUtils.h"
#include "modelDescriptorSet.h"
#include "pbr.h"

struct VsOut
{
    vec3 pos;
    vec3 norm;
    vec3 bary;
    vec2 uv;
    uint transformIndex;
};

layout(location = 0) in FsInBlock
{
    VsOut fsIn;
};

layout(location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform texture2D textures[MAX_MODEL_TEXTURES];

layout(push_constant) uniform ConstantBlock
{
    uint materialIndex;
    uint skyboxIndex;
#ifdef DEBUG
    uint debugFlags;
#endif // DEBUG
};

vec3 perturbNormal(vec3 N, vec3 V, vec2 texcoord, uint normalMapIndex)
{
    vec3 map = texture(sampler2D(textures[normalMapIndex], linearRepeatSampler), texcoord).rgb;
    map.xy = map.xy * 2.f - 1.f;
    map.z = sqrt(1.f - dot(map.xy, map.xy));
    map.x = -map.x;
    mat3 TBN = cotangentFrame(N, -V, texcoord);
    return normalize(TBN * map);
}

void main()
{
    MaterialData md = materials[materialIndex];
    vec3 cameraPos = cameraData.invViewMat[3].xyz;
    vec3 viewVec = cameraPos - fsIn.pos;
    vec3 N = normalize(fsIn.norm);
    vec3 V = normalize(viewVec);

    if(bool(cameraData.sceneConfig & SCENE_USE_NORMAL_MAP) && isTextureValid(md.normalTexIndex))
    {
        N = perturbNormal(N, viewVec, fsIn.uv, md.normalTexIndex);
    }

    vec3 albedo = isTextureValid(md.colorTexIndex) ? texture(sampler2D(textures[md.colorTexIndex], linearRepeatSampler), fsIn.uv).rgb : vec3(1.f);
    vec3 aoRoughMetal = isTextureValid(md.aoRoughMetalTexIndex) ? texture(sampler2D(textures[md.aoRoughMetalTexIndex], linearRepeatSampler), fsIn.uv).rgb : vec3(1.f, 0.f, 0.f);
    float ao = bool(md.mask & MATERIAL_HAS_AO_TEX) ? aoRoughMetal.r : 1.f;
    float roughness = max(aoRoughMetal.g, 0.01f);
    float metallic = aoRoughMetal.b;

    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.f);

    vec3 irradiance = texture(samplerCube(irradianceMaps[skyboxIndex], linearRepeatSampler), N).rgb;
    vec3 prefiltered = textureLod(samplerCube(prefilteredMaps[skyboxIndex], linearRepeatSampler), R, roughness * MAX_PREFILTERED_MAP_LOD).rgb;
    vec2 brdf = texture(sampler2D(brdfLut, linearClampSampler), vec2(NdotV, roughness)).rg;

    vec3 fDiff, fSpec;
    vec3 LoDiff = vec3(0.f), LoSpec = vec3(0.f);

    if(bool(cameraData.sceneConfig & SCENE_USE_LIGHTS))
    {
        for(uint i = 0; i < uint(lightData.dirLightCount); i++)
        {
            vec3 L = -normalize(vec3(lightData.lights[i].x, lightData.lights[i].y, lightData.lights[i].z));
            vec3 H = normalize(V + L);
            float NdotL = max(dot(N, L), 0.f);
            float NdotH = max(dot(N, H), 0.f);
            float HdotV = max(dot(H, V), 0.f);

            vec3 lightColor = vec3(lightData.lights[i].r, lightData.lights[i].g, lightData.lights[i].b);
            float intensity = float(lightData.lights[i].intensity);
            vec3 Li = lightColor * intensity;

            directBRDF(NdotV, NdotL, NdotH, HdotV, albedo, roughness, metallic, fDiff, fSpec);
            LoDiff += fDiff * Li * NdotL;
            LoSpec += fSpec * Li * NdotL;
        }

//        for(uint i = 0; i < uint(lightData.pointLightCount); i++)
//        {
//            uint offset = uint(lightData.dirLightCount);
//            vec3 lightPos = vec3(lightData.lights[offset + i].x, lightData.lights[offset + i].y, lightData.lights[offset + i].z);
//            vec3 L = normalize(lightPos - fsIn.pos);
//        }
    }

    if(bool(cameraData.sceneConfig & SCENE_USE_IBL))
    {
        envBRDF(albedo, roughness, metallic, irradiance, prefiltered, brdf, fDiff, fSpec);
        LoDiff += fDiff * ao;
        LoSpec += fSpec * ao;
    }

    vec3 outColor = reinhardTonemap(LoDiff + LoSpec);

#ifdef DEBUG
    switch(debugFlags)
    {
        case DEBUG_SHOW_COLOR:
            outColor = albedo;
            break;
        case DEBUG_SHOW_NORMAL:
            outColor = N;
            break;
        case DEBUG_SHOW_AO:
            outColor = vec3(ao);
            break;
        case DEBUG_SHOW_ROUGHNESS:
            outColor = vec3(roughness);
            break;
        case DEBUG_SHOW_METALLIC:
            outColor = vec3(metallic);
            break;
        case DEBUG_SHOW_IRRADIANCE:
            outColor = irradiance;
            break;
        case DEBUG_SHOW_PREFILTERED:
            outColor = prefiltered;
            break;
        case DEBUG_SHOW_DIFFUSE:
            outColor = LoDiff;
            break;
        case DEBUG_SHOW_SPECULAR:
            outColor = LoSpec;
            break;
        default:
            break;
    }
#endif // DEBUG

    const vec3 wireframeColor = vec3(0.f);
    if(bool(cameraData.sceneConfig & SCENE_SHOW_WIREFRAME))
        outColor = mix(outColor, wireframeColor, edgeFactor(fsIn.bary));

    outFragColor = vec4(outColor, 1.f);
}