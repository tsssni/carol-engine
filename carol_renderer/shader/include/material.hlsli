#ifndef MATERIAL_HEADER
#define MATERIAL_HEADER

struct Material
{
    float3 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
};

float3 CalcFresnelR0(float3 diffuseColor, float metallic)
{
    static float3 fresenlR0 = float3(0.04f, 0.04f, 0.04f);
    return lerp(fresenlR0, diffuseColor, metallic);
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

#endif
