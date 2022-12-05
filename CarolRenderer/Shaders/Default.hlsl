#include "Common.hlsl"
#include "Light.hlsl"

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gShadowMap : register(t2);

#ifdef SSAO
Texture2D gSsaoMap : register(t3);
#endif

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION0;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD; 
#ifdef SSAO
    float4 SsaoPosH : POSITION1;
#endif

};

VertexOut VS(VertexIn vin)
{   
    VertexOut vout;
    
#ifdef SKINNED
    vin = SkinnedTransform(vin);
#endif

    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    vout.NormalW = normalize(mul(vin.NormalL, (float3x3) gWorld));
    vout.TangentW = normalize(mul(vin.TangentL, (float3x3) gWorld));
    vout.TexC = vin.TexC;
   
#ifdef TAA
    vout.PosH = mul(float4(vout.PosW, 1.0f), gJitteredViewProj);
#else
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
#endif

#ifdef SSAO
    vout.SsaoPosH = mul(float4(vout.PosW, 1.0f), gViewProjTex);
#endif
    
    return vout;
}

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    
    [unroll]
    for(int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return pow(percentLit / 9.0f, 10.0f);
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 texDiffuse = gDiffuseMap.SampleLevel(gsamAnisotropicWrap, pin.TexC, pow(pin.PosH.z, 15.0f) * 8.0f);
   
    LightMaterialData lightMat;
    lightMat.fresnelR0 = gFresnelR0;
    lightMat.diffuseAlbedo = texDiffuse.rgb;
    lightMat.roughness = gRoughness;

    float3 texNormal = gNormalMap.SampleLevel(gsamAnisotropicWrap, pin.TexC, pow(pin.PosH.z, 15.0f) * 8.0f).rgb;
    texNormal = TexNormalToWorldSpace(texNormal, pin.NormalW, pin.TangentW);
    
    float3 ambient = gLights[0].Ambient * texDiffuse.rgb;
    
#ifdef SSAO
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = gSsaoMap.SampleLevel(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;
    //ambient *= ambientAccess;
#endif

    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(mul(float4(pin.PosW, 1.0f), gLights[0].LightViewProjTex));
    float4 litColor = ComputeLighting(gLights, lightMat, pin.PosW, texNormal, normalize(gEyePosW - pin.PosW), shadowFactor);
    
    return float4(ambient + litColor.rgb, 1.0f);
}