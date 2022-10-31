cbuffer SsaoCB : register(b0)
{
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gProjTex;
    float4 gOffsetVectors[14];
    
    float4 gBlurWeights[3];
    float2 gInvRenderTargetSize;
    
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEplison;
}

cbuffer RootConstant : register(b1)
{
    bool gHorizontalBlur;
}

Texture2D gNormalMap : register(t0);
Texture2D gDepthMap : register(t1);
Texture2D gRandomVectorMap : register(t2);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

static const int gSampleCount = 14;

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    vout.TexC = gTexCoords[vid];
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
    
    float4 posVH = mul(vout.PosH, gInvProj);
    vout.PosV = posVH.xyz / posVH.w;
    
    return vout;
}

float NdcDepthToViewDepth(float depth)
{
    return gProj[3][2] / (depth - gProj[2][2]);
}

float OcclusionFunction(float distZ)
{
    float occlusion = 0.0f;
    if (distZ > gSurfaceEplison)
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
    }
    
    return occlusion;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 normal = normalize(gNormalMap.SampleLevel(gsamLinearClamp, pin.TexC, 0.0f).xyz);
    float viewDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamLinearClamp, pin.TexC, 0.0f).r);
    float3 viewPos = (viewDepth / pin.PosV.z) * pin.PosV.xyz;
    
    float3 randVec = 2.0f * gRandomVectorMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz - 1.0f;
    float occlusionSum = 0.0f;
    
    for (int i = 0; i < gSampleCount;++i)
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
        float flip = sign(dot(offset, normal));
        float3 offsetPos = viewPos + flip * gOcclusionRadius * offset;
        
        float4 screenOffsetPos = mul(float4(offsetPos, 1.0f), gProjTex);
        screenOffsetPos /= screenOffsetPos.w;
        float offsetDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamLinearClamp, screenOffsetPos.xy, 0.0f).r);
        
        float3 screenPos = (offsetDepth / offsetPos.z) * offsetPos;
        float distZ = viewPos.z - screenPos.z;
        float dp = max(dot(normalize(screenPos - viewPos), normal), 0.0f);
        
        float occlusion = dp * OcclusionFunction(distZ);
        occlusionSum += occlusion;
    }

    
    occlusionSum /= gSampleCount;
    float access = 1.0f - occlusionSum;
    
    return saturate(pow(access, 6.0f));
}
