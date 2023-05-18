#include "include/common.hlsli"
#include "include/texture.hlsli"
#include "include/shadow.hlsli"

float3 GetPosW(float2 uv, float depth)
{
    float2 pos = float2(uv.x / gRenderTargetSize.x, uv.y / gRenderTargetSize.y);

    return ViewPosToWorldPos(TexPosToViewPos(pos, NdcDepthToViewDepth(depth, gProj), gInvProj), gInvView);
}

[numthreads(32, 32, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    RWTexture2D<float4> frameMap = ResourceDescriptorHeap[gRWFrameMapIdx];
    Texture2D diffuseRoughnessMap = ResourceDescriptorHeap[gDiffuseRoughnessMapIdx];
    Texture2D emissiveMetallicMap = ResourceDescriptorHeap[gEmissiveMetallicMapIdx];
    Texture2D normalMap = ResourceDescriptorHeap[gNormalMapIdx];
    Texture2D depthStencilMap = ResourceDescriptorHeap[gDepthStencilMapIdx];
    Texture2D ambientMap = ResourceDescriptorHeap[gAmbientMapIdx];

    if (TextureBorderTest(dtid, gRenderTargetSize))
    {
        float2 uv = (dtid + .5f) * gInvRenderTargetSize;

        float depth = depthStencilMap[dtid].r;
        float3 posW = GetPosW(dtid + .5f, depth);

        if(depth < 1.f)
        {

            float3 diffuse = diffuseRoughnessMap[dtid].rgb;
            float3 emissive = emissiveMetallicMap[dtid].rgb;
            float roughness = diffuseRoughnessMap[dtid].a;
            float metallic = emissiveMetallicMap[dtid].a;
            float3 normal = normalMap[dtid].rgb;

            float viewDepth;
            viewDepth = NdcDepthToViewDepth(depth, gProj);

            Material lightMat;
            lightMat.SubsurfaceAlbedo = diffuse;
            lightMat.Metallic = metallic;
            lightMat.Roughness = max(1e-6f, roughness);

            float3 ambientColor = gAmbientColor * diffuse.rgb;
            float ambientAccess = ambientMap.SampleLevel(gsamLinearClamp, uv, 0.0f).r;
            ambientColor *= ambientAccess;

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
        else
        {
            TextureCube skyBox = ResourceDescriptorHeap[gSkyBoxIdx];
            frameMap[dtid] = skyBox.Sample(gsamLinearWrap, normalize(posW - gEyePosW));
        }
    }
}