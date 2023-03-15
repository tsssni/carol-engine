#ifndef BLINN_PHONG_HEADER
#define BLINN_PHONG_HEADER

#include "material.hlsli"

float3 BlinnPhong(float3 lightStrength, float3 toLight, float3 toEye, float3 normal, Material mat)
{
    const float m = (1.0f - mat.Roughness) * 256.0f;
    float3 halfVec = normalize(toEye + toLight);
    float3 fresnelR0 = CalcFresnelR0(mat.SubsurfaceAlbedo, mat.Metallic);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(fresnelR0, halfVec, toLight);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.SubsurfaceAlbedo.rgb + specAlbedo) * lightStrength * dot(normal, toLight);
}
#endif
