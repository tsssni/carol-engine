#include "Common.hlsl"
#include "Light.hlsl"

Texture2D gTextureMaps[256] : register(t0);
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);
Texture2D gSsaoMap : register(t1, space1);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC : TEXCOORD;  
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
#endif
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION0;
    float4 SsaoPosH : POSITION1;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{   
    VertexOut vout;
    
#ifdef SKINNED
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
    
    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);

    for(int i = 0; i < 4; ++i)
    {
        posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
        normalL += weights[i] * mul(vin.NormalL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
        tangentL += weights[i] * mul(vin.TangentL.xyz, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
    }
    
    vin.PosL = posL;
    vin.NormalL = normalL;
    vin.TangentL.xyz = tangentL;
#endif

    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
    vout.SsaoPosH = mul(float4(vout.PosW, 1.0f), gViewProjTex);
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
	vout.TangentW = mul(vin.TangentL, (float3x3)gWorld);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 0.0f), gTexTransform).xy;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData matData = gMaterialData[gMatTBIndex];
    float4 texColor = gTextureMaps[matData.diffuseSrvHeapIndex].SampleLevel(gsamLinearClamp, pin.TexC, 0.0f);
   
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = gSsaoMap.SampleLevel(gsamPointClamp, pin.SsaoPosH.xy, 0.0f).r;
    
    matData.diffuseAlbedo = texColor;
    float4 litColor = ComputeLighting(gLights, matData, pin.PosW, pin.NormalW, normalize(gEyePosW - pin.PosW), float3(1.0f, 1.0f, 1.0f));
    
    return 0.5 * ambientAccess * texColor + litColor;
}