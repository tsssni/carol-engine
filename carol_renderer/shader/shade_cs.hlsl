#include "include/common.hlsli"
#include "include/texture.hlsli"
#include "include/shadow.hlsli"

float3 GetPosW(float2 uv, float projDepth, float viewDepth)
{
    float2 pos = float2(uv.x / gRenderTargetSize.x, uv.y / gRenderTargetSize.y);
    pos.x = pos.x * 2.f - 1.f;
    pos.y = 1.f - pos.y * 2.f;

    float4 posH = float4(pos, projDepth, 1.f) * viewDepth;
    
    float4 posW;
#ifdef TAA
    posW = mul(posH, gInvJitteredViewProj);
#else
    posW = mul(posH, gInvViewProj);
#endif

    return posW.xyz / posW.w;
}

[numthreads(32, 32, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    RWTexture2D<float4> frameMap = ResourceDescriptorHeap[gRWFrameMapIdx];
    Texture2D diffuseRoughnessMap = ResourceDescriptorHeap[gDiffuseRoughnessMapIdx];
    Texture2D emissiveMetallicMap = ResourceDescriptorHeap[gEmissiveMetallicMapIdx];
    Texture2D normalDepthMap = ResourceDescriptorHeap[gNormalDepthMapIdx];
    Texture2D depthStencilMap = ResourceDescriptorHeap[gDepthStencilMapIdx];
#ifdef SSAO
    Texture2D ssaoMap = ResourceDescriptorHeap[gAmbientMapIdx];
#endif

    if (TextureBorderTest(dtid, gRenderTargetSize))
    {
        float2 uv = (dtid + .5f) * gInvRenderTargetSize;
        uv.x = uv.x * 2.f - 1.f;
        uv.y = 1.f - uv.y * 2.f;

        float4 diffuseRoughness = diffuseRoughnessMap[dtid];
        float4 emissiveMetallic = emissiveMetallicMap[dtid];
        float4 normalDepth = normalDepthMap[dtid];
        float projDepth = depthStencilMap[dtid].r;
        float viewDepth = normalDepth.a;
        
        float3 posW = GetPosW(dtid + .5f, projDepth, viewDepth);
        float3 diffuse = diffuseRoughness.rgb;
        float3 emissive = emissiveMetallic.rgb;
        float3 normal = normalDepth.rgb;
        float roughness = diffuseRoughness.a;
        float metallic = emissiveMetallic.a;

        Material lightMat;
        lightMat.SubsurfaceAlbedo = diffuse;
        lightMat.Metallic = metallic;
        lightMat.Roughness = max(1e-6f, roughness);

        float3 ambientColor = gAmbientColor * diffuse.rgb;
    #ifdef SSAO
        float ambientAccess = ssaoMap.SampleLevel(gsamLinearClamp, uv, 0.0f).r;
        ambientColor *= ambientAccess;
    #endif

        float3 toEye = normalize(gEyePosW - posW);
        float3 emissiveColor = emissive * dot(toEye, normal);
        float3 litColor = float3(0.f, 0.f, 0.f);

        uint mainLightIdx;
        float shadowFactor = GetCSMShadowFactor(posW, viewDepth, mainLightIdx);
        litColor += shadowFactor * ComputeDirectionalLight(gMainLights[mainLightIdx], lightMat, normal, toEye);

        for (int pointLightIdx = 0; pointLightIdx < gNumPointLights; ++pointLightIdx)
        {
            litColor += ComputePointLight(gPointLights[pointLightIdx], lightMat, posW, normal, toEye);
        }

        for (int spotLightIdx = 0; spotLightIdx < gNumPointLights; ++spotLightIdx)
        {
            litColor += ComputePointLight(gPointLights[spotLightIdx], lightMat, posW, normal, toEye);
        }

        frameMap[dtid] = float4(ambientColor + emissiveColor + litColor, 1.f);
    }
}