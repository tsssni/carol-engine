#include "include/ssao.hlsli"
#include "include/root_signature.hlsli"

static const int gBlurRadius = 5;

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexId)
{
    VertexOut vout;
    vout.TexC = gTexCoords[vid];
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
    
    return vout;
}

float NdcDepthToViewDepth(float z_ndc)
{
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 PS(VertexOut pin) : SV_Target
{
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
    
    float4 color = blurWeights[gBlurRadius] * gTex2D[gSsaoAmbientMap0Idx+gBlurDirection].SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
    float totalWeight = blurWeights[gBlurRadius];
    
    float3 centerNormal = gTex2D[gSsaoNormalMapIdx].SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz;
    float centerDepth = NdcDepthToViewDepth(gTex2D[gSsaoDepthStencilMapIdx].SampleLevel(gsamPointClamp, pin.TexC, 0.0f).r);
    
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        if (i == 0)
        {
            continue;
        }
        
        float2 offsetTexC = pin.TexC + i * texOffset;
        float3 neighborNormal = gTex2D[gSsaoNormalMapIdx].SampleLevel(gsamPointClamp, offsetTexC, 0.0f).xyz;
        float neighborDepth = NdcDepthToViewDepth(gTex2D[gSsaoDepthStencilMapIdx].SampleLevel(gsamPointClamp, offsetTexC, 0.0f).r);
 
        if (dot(centerNormal, neighborNormal) >= 0.8f && abs(centerDepth - neighborDepth) <= 0.2f)
        {
            color += blurWeights[i + gBlurRadius] * gTex2D[gSsaoAmbientMap0Idx+gBlurDirection].SampleLevel(gsamPointClamp, offsetTexC, 0.0f);
            totalWeight += blurWeights[i + gBlurRadius];
        }
    }
    
    return color / totalWeight;
}