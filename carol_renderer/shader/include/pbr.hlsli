#ifndef PBR_REFLECTION_HEADER
#define PBR_REFLECTION_HEADER

#include "material.hlsli"

float3 SpecularBRDF(float3 toLight, float3 toEye, float3 normal, float3 reflectance, Material mat)
{
    float3 halfVec = normalize(toLight + toEye);
    float geometry = 0.f;
    float normalProb = 0.f;

#ifdef SMITH
#ifdef GGX
#ifdef DIRECTION_CORRELATED_G2
    geometry = DirectionCorrelatedGGXSmithG2(toLight, toEye, halfVec, mat.Roughness);
#elif HEIGHT_CORRELATED_G2
    geometry = HeightCorrelatedGGXSmithG2(toLight, toEye, halfVec, mat.Roughness);
#endif
#endif
#endif

#ifdef GGX
    normalProb = GGXNormalDistribution(halfVec, normal, mat.Roughness);
#endif

    float3 brdf = float3(0.f, 0.f, 0.f);
    brdf = reflectance * geometry * normalProb / (4 * abs(dot(toLight, normal)) * abs(dot(toEye, normal)));
    return brdf;
}

float3 SubsurfaceScatteringBRDF(float3 toLight, float3 toEye, float3 normal, float3 reflectance, Material mat)
{
    float3 subsurfaceAlbedo = CalcSubsurfaceAlbedo(mat.SubsurfaceAlbedo, reflectance, mat.Metallic);

    float3 brdf = float3(0.f, 0.f, 0.f);
#ifdef LAMBERTIAN
    brdf = Lambertian(subsurfaceAlbedo);
#endif
    return brdf;
}

float3 PBR(float3 lightStrength, float3 toLight, float3 toEye, float3 normal, Material mat)
{
    if (dot(toLight, normal) <= 0.f || dot(toEye, normal) <= 0.f)
    {
        return 0.f;
    }
    
    float3 reflectance = SchlickFresnel(CalcFresnelR0(mat.SubsurfaceAlbedo, mat.Metallic), normalize(toLight + toEye), toLight);
    float3 specularBRDF = SpecularBRDF(toLight, toEye, normal, reflectance, mat);
    float3 subsurfaceScatteringBRDF = SubsurfaceScatteringBRDF(toLight, toEye, normal, reflectance, mat);

    return (specularBRDF + subsurfaceScatteringBRDF) * lightStrength * max(0.f, dot(normal, toLight));

}
#endif