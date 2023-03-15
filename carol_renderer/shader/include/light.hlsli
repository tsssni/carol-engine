#ifndef LIGHT_HEADER
#define LIGHT_HEADER

#include "blinn_phong.hlsli"
#include "pbr.hlsli"

struct Light
{
    float3 Strength;
    float LightPad0;
    float3 Direction;
    float LightPad1;
    float3 Position; 
    float LightPad2;

    float AttenuationConstant;
    float AttenuationLinear;
    float AttenuationQuadric;
    float LightPad3;

    float SpotInnerAngle;
    float SpotOuterAngle;
    float2 AreaSize;
    
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
};

float3 Render(float3 lightStrength, float3 toLight, float3 toEye, float3 normal, Material mat)
{
    float3 litColor = float3(0.f, 0.f, 0.f);

#ifdef BLINN_PHONG
    litColor = BlinnPhong(lightStrength, toLight, toEye, normal, mat);
#elif (defined GGX) || \
      (defined LAMBERTIAN) || \
      (defined SMITH)
    litColor = PBR(lightStrength, toLight, toEye, normal, mat);
#endif

    return litColor;
}

float CalcAttenuation(float d, float attConstant, float attLinear, float attQuadric)
{
    return 1.f / (attConstant + attLinear * d + attQuadric * d * d);
}

float CalcSpotFactor(Light light, float3 toLight)
{
    return 1.f - max(0.f, (dot(-light.Direction, toLight) - cos(light.SpotInnerAngle)) / (cos(light.SpotOuterAngle) - cos(light.SpotInnerAngle)));
}

float3 ComputeDirectionalLight(Light light, Material mat, float3 normal, float3 toEye)
{
    float3 toLight = -light.Direction;

    return Render(light.Strength, toLight, toEye, normal, mat);
}

float3 ComputePointLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 toLight = light.Position - pos;
    float d = length(toLight);
    toLight /= d;

    float att = CalcAttenuation(d, light.AttenuationConstant, light.AttenuationLinear, light.AttenuationQuadric);
    light.Strength *= att;

    return Render(light.Strength, toLight, toEye, normal, mat);
}

float3 ComputeSpotLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 toLight = light.Position - pos;
    float d = length(toLight);
    toLight /= d;

    float att = CalcAttenuation(d, light.AttenuationConstant, light.AttenuationLinear, light.AttenuationQuadric);
    light.Strength *= att;

    float spotFactor = CalcSpotFactor(light, toLight);
    light.Strength *= spotFactor;
 
    return Render(light.Strength, toLight, toEye, normal, mat);
}

#endif