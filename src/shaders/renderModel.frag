#include "globalDescriptorSet.h"
#include "pbr.h"

struct VsOut
{
    vec3 pos;
    vec3 norm;
    vec3 bary;
    vec2 uv;
};

layout(location = 0) in FsInBlock
{
    VsOut fsIn;
};

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform ConstantBlock
{
    uint skyboxIndex;
    uint materialIndex;
    float time;
    uint debugFlags;
};

vec3 getNormal(vec3 N, vec3 p, vec2 uv, uint normalMapIndex)
{
    vec3 normal = texture(sampler2D(materialTextures[normalMapIndex], linearRepeatSampler), uv).rgb;
    normal.xy = normal.xy * 2.f - 1.f;
    normal.z = sqrt(max(1.f - dot(normal.xy, normal.xy), 0.f));

    vec3 pdx = dFdx(p);
    vec3 pdy = dFdy(p);
    vec2 uvdx = dFdx(uv);
    vec2 uvdy = dFdy(uv);

    float det = determinant(mat2(uvdx, uvdy));
    vec3 T = normalize(uvdy.y * pdx - uvdx.y * pdy);
    vec3 B = normalize(cross(N, T));

    return normalize(mat3(T, sign(det) * B, N) * normal);
}

float edgeFactor(vec3 bary)
{
    const float thickness = 1.f;
    vec3 a3 = smoothstep(vec3(0.f), fwidth(bary) * thickness, bary);
    return 1.f - min(min(a3.x, a3.y), a3.z);
}

bool isTextureValid(uint textureIndex)
{
    return bool(~textureIndex);
}

vec3 getBurnColor(float value)
{
    const vec3 colors[] = vec3[](
        vec3(0.02, 0.02, 0.02),
        vec3(0.86, 0.27, 0.33),  // 219,69,83
        vec3(1.00, 0.77, 0.35),  // 255,196,88
        vec3(1.00, 0.99, 0.74)); // 254,253,189
    vec3 color = mix4(colors, clamp(value, 0.f, 1.f));
    return color * smoothstep(0.f, 1.f, luminance(color)) * 15.f; // 15 here is rather arbitrary
}

const float maxBurnDuration = 5.f; // seconds

void main()
{
    MaterialData md = materials[materialIndex];
    vec3 cameraPos = sceneData.invViewMat[3].xyz;
    vec3 viewVec = cameraPos - fsIn.pos;
    vec3 N = normalize(fsIn.norm);
    vec3 V = normalize(viewVec);

    if(!gl_FrontFacing)
        N = -N;

    if(isTextureValid(md.normalTexIndex))
        N = getNormal(N, fsIn.pos, fsIn.uv, md.normalTexIndex);

    vec3 albedo = isTextureValid(md.colorTexIndex) ? texture(sampler2D(materialTextures[md.colorTexIndex], linearRepeatSampler), fsIn.uv).rgb : vec3(1.f);
    vec3 aoRoughMetal = isTextureValid(md.aoRoughMetalTexIndex) ? texture(sampler2D(materialTextures[md.aoRoughMetalTexIndex], linearRepeatSampler), fsIn.uv).rgb : vec3(1.f, 0.f, 0.f);
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

    if(bool(sceneData.sceneConfig & SCENE_USE_LIGHTS))
    {
        for(uint i = 0; i < lightData.dirLightCount; i++)
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

//        for(uint i = 0; i < lightData.pointLightCount; i++)
//        {
//            uint offset = lightData.dirLightCount;
//            vec3 lightPos = vec3(lightData.lights[offset + i].x, lightData.lights[offset + i].y, lightData.lights[offset + i].z);
//            vec3 L = normalize(lightPos - fsIn.pos);
//        }
    }

    if(bool(sceneData.sceneConfig & SCENE_USE_IBL))
    {
        envBRDF(albedo, roughness, metallic, irradiance, prefiltered, brdf, fDiff, fSpec);
        LoDiff += fDiff * ao;
        LoSpec += fSpec * ao;
    }

    vec3 outColor = LoDiff + LoSpec;

    vec4 burn = texture(sampler2D(burnMapTexture, linearRepeatSampler), fsIn.uv);
    float burnTime = burn.x;
    float burnAlpha = burn.a;
    float burnLevel = 1.f - smoothstep(burnTime, burnTime + maxBurnDuration * burnAlpha, time);
    vec3 burnColor = getBurnColor(burnLevel);
    outColor = mix(outColor, burnColor, burnAlpha);

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
        case DEBUG_SHOW_BURN:
            outColor = burnColor;
        default:
            break;
    }
#endif // DEBUG

    const vec3 wireframeColor = vec3(0.f);
    if(bool(sceneData.sceneConfig & SCENE_SHOW_WIREFRAME))
        outColor = mix(outColor, wireframeColor, edgeFactor(fsIn.bary));

    outFragColor = vec4(outColor, 1.f);
}