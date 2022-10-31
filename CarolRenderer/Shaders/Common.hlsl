#ifndef COMMON
#define COMMON

#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#define MAX_LIGHTS 16

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position; 
    float SpotPower;
};

cbuffer PassCB : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gHistViewProj;
    float4x4 gInvViewProj;
    float4x4 gViewProjTex;
    
    float3 gEyePosW;
    float passPad0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float passPad1;
    float passPad2;

    Light gLights[MAX_LIGHTS];
}

cbuffer ObjCB : register(b1)
{
    float4x4 gWorld;
    float4x4 gHistWorld;
    float4x4 gTexTransform;
    uint gMatTBIndex;
    uint objPad0;
    uint objPad1;
    uint objPad2;
}

cbuffer SkinnedCB : register(b2)
{
    float4x4 gBoneTransforms[256];
    float4x4 gHistBoneTransforms[256];
}

struct MaterialData
{
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
    float4x4 matTransform;
    
    int diffuseSrvHeapIndex;
    int normalSrvHeapIndex;
    uint matPad0;
    uint matPad1;
};

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

#endif