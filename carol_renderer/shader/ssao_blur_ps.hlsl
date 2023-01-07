#include "include/common.hlsli"

static const int gBlurRadius = 5;

cbuffer BlurDirection : register(b3)
{
    uint gBlurDirection;
    uint gAmbientInputIdx;
}

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float NdcDepthToViewDepth(float z_ndc)
{
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 main(PixelIn pin) : SV_Target
{
    Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilIdx];
    Texture2D normalMap = ResourceDescriptorHeap[gNormalIdx];
    Texture2D ambientMap = ResourceDescriptorHeap[gAmbientInputIdx];
    
    float blurWeights[12] =
    {
        gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
        gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
        gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
    };
        
    float2 texOffset;
    if (gBlurDirection == 0)
    {
        texOffset = float2(gInvRenderTargetSize.x, 0.0f);
    }
    else
    {
        texOffset = float2(0.0f, gInvRenderTargetSize.y); 
    }
    
    float4 color = blurWeights[gBlurRadius] * ambientMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
    float totalWeight = blurWeights[gBlurRadius];
        
    float3 centerNormal = normalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz;
    float centerDepth = NdcDepthToViewDepth(depthMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).r);
    
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        if (i == 0)
        {
            continue;
        }
        
        float2 offsetTexC = pin.TexC + i * texOffset;
        float3 neighborNormal = normalMap.SampleLevel(gsamPointClamp, offsetTexC, 0.0f).xyz;
        float neighborDepth = NdcDepthToViewDepth(depthMap.SampleLevel(gsamPointClamp, offsetTexC, 0.0f).r);
 
        if (dot(centerNormal, neighborNormal) >= 0.8f && abs(centerDepth - neighborDepth) <= 0.2f)
        {
            color += blurWeights[i + gBlurRadius] * ambientMap.SampleLevel(gsamPointClamp, offsetTexC, 0.0f);
            totalWeight += blurWeights[i + gBlurRadius];
        }
    }
    
    return color / totalWeight;
}
