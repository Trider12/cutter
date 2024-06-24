#include "globalDescriptorSet.h"
#include "pbr.h"
#include "utils.h"

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

layout(set = 1, binding = 0) uniform texture2D textures[MAX_TEXTURES];

layout(push_constant) uniform ConstantBlock
{
    uint materialIndex;
};

mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv)
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    // solve the linear system
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    // construct a scale-invariant frame
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

vec3 perturbNormal(vec3 N, vec3 V, vec2 texcoord, uint normalMapIndex)
{
    vec3 map = texture(sampler2D(textures[normalMapIndex], linearRepeatSampler), texcoord).rgb;
    map = map * 2.f - 1.f;
    map.y = -map.y;
    mat3 TBN = cotangentFrame(N, -V, texcoord);
    return normalize(TBN * map);
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

void main()
{
    MaterialData md = materials[materialIndex];
    vec3 cameraPos = cameraData.invViewMat[3].xyz;
    vec3 viewVec = cameraPos - fsIn.pos;
    vec3 N = normalize(fsIn.norm);
    vec3 V = normalize(viewVec);

    if(bool(cameraData.sceneConfig & CONFIG_USE_NORMAL_MAP) && isTextureValid(md.normalTexIndex))
    {
        N = perturbNormal(N, viewVec, fsIn.uv, md.normalTexIndex);
    }

    vec3 albedo = isTextureValid(md.colorTexIndex) ? texture(sampler2D(textures[md.colorTexIndex], linearRepeatSampler), fsIn.uv).rgb : vec3(1.f);
    vec3 aoRoughMetal = isTextureValid(md.aoRoughMetalTexIndex) ? texture(sampler2D(textures[md.aoRoughMetalTexIndex], linearRepeatSampler), fsIn.uv).rgb : vec3(0.f);
    vec3 Lo = vec3(0.f);

    for(uint i = 0; i < uint(lightData.dirLightCount); i++)
    {
        vec3 L = -normalize(vec3(lightData.lights[i].x, lightData.lights[i].y, lightData.lights[i].z));
        vec3 H = normalize(V + L);
        float NdotV = max(dot(N, V), 0.f);
        float NdotL = max(dot(N, L), 0.f);
        float NdotH = max(dot(N, H), 0.f);
        float HdotV = max(dot(H, V), 0.f);

        vec3 lightColor = vec3(lightData.lights[i].r, lightData.lights[i].g, lightData.lights[i].b);
        float intensity = float(lightData.lights[i].intensity);
        vec3 Li = lightColor * intensity;

        Lo += BRDF(NdotV, NdotL, NdotH, HdotV, albedo, aoRoughMetal.g, aoRoughMetal.b) * Li * NdotL;
    }

//    for(uint i = 0; i < uint(lightData.pointLightCount); i++)
//    {
//        uint offset = uint(lightData.dirLightCount);
//        vec3 lightPos = vec3(lightData.lights[offset + i].x, lightData.lights[offset + i].y, lightData.lights[offset + i].z);
//        vec3 L = normalize(lightPos - fsIn.pos);
//    }

    vec3 outColor = reinhardTonemap(Lo);
    vec3 wireframeColor = vec3(0.f);
    outColor = mix(outColor, wireframeColor, edgeFactor(fsIn.bary) * uint(bool(cameraData.sceneConfig & CONFIG_SHOW_WIREFRAME)));
    outFragColor = vec4(outColor, 1.f);
}