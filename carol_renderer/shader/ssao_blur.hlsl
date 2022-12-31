#include "include/root_signature.hlsli"
#include "include/screen_tex.hlsli"

static const int gBlurRadius = 5;

cbuffer BlurDirection : register(b3)
{
    uint gBlurDirection;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

[numthreads(6, 1, 1)]
[OutputTopology("triangle")]
void MS(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[2],
    out vertices VertexOut verts[6])
{     
    SetMeshOutputCounts(6, 2);
    
    if (gtid == 0)
    {
        tris[gtid] = uint3(0, 1, 2);
    }
    else if (gtid == 1)
    {
        tris[gtid] = uint3(3, 4, 5);
    }
    
    if (gtid < 6)
    {
        VertexOut vout;
    
        vout.TexC = gTexCoords[gtid];
        vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

        verts[gtid] = vout;
    }
}

float NdcDepthToViewDepth(float z_ndc)
{
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D gDepthMap = ResourceDescriptorHeap[gDepthStencilIdx];
    Texture2D gNormalMap = ResourceDescriptorHeap[gNormalIdx];
    Texture2D gAmbientMap = ResourceDescriptorHeap[gAmbientIdx + gBlurDirection];
    
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
    
    float4 color = blurWeights[gBlurRadius] * gAmbientMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
    float totalWeight = blurWeights[gBlurRadius];
        
    float3 centerNormal = gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz;
    float centerDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).r);
    
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        if (i == 0)
        {
            continue;
        }
        
        float2 offsetTexC = pin.TexC + i * texOffset;
        float3 neighborNormal = gNormalMap.SampleLevel(gsamPointClamp, offsetTexC, 0.0f).xyz;
        float neighborDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamPointClamp, offsetTexC, 0.0f).r);
 
        if (dot(centerNormal, neighborNormal) >= 0.8f && abs(centerDepth - neighborDepth) <= 0.2f)
        {
            color += blurWeights[i + gBlurRadius] * gAmbientMap.SampleLevel(gsamPointClamp, offsetTexC, 0.0f);
            totalWeight += blurWeights[i + gBlurRadius];
        }
    }
    
    return color / totalWeight;
}