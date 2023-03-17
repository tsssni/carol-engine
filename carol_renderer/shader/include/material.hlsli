#ifndef MATERIAL_HEADER
#define MATERIAL_HEADER

#define PI 3.14159265f

struct Material
{
    float3 SubsurfaceAlbedo;
    float Metallic;
    float Roughness;
};

float3 SchlickFresnel(float3 R0, float3 normal, float3 toLight)
{
    float cosIncidentAngle = saturate(dot(normal, toLight));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * pow(f0, 5.f);

    return reflectPercent;
}

float3 CalcFresnelR0(float3 subsurfaceAlbedo, float metallic)
{
    const static float3 fDieletric = float3(0.04f, 0.04f, 0.04f);
    return lerp(fDieletric, subsurfaceAlbedo, metallic);
}

float GGXSmithLambda(float3 view, float3 dir, float roughness)
{
    float cos2 = pow(dot(view, dir), 2.f);
    float cot2 = cos2 / (1.f - cos2);
    float a2 = cot2 / pow(roughness, 2.f);

    return 0.5f * (sqrt(1.f / a2 + 1.f) - 1.f);
}

float GGXSmithMasking(float3 view, float3 dir, float roughness)
{
    if(dot(view, dir) <= 0.f)
    {
        return 0.f;
    }

    return 1.f / (1.f + GGXSmithLambda(view, dir, roughness));
}

float DirectionCorrelatedGGXSmithMaskingShadowing(float3 toLight, float3 toEye, float3 dir, float roughness)
{
    float toLightAzimuth = atan(toLight.y / toLight.x);
    float toEyeAzimuth = atan(toEye.y / toEye.x);
    float absAzimuth = abs(toLightAzimuth - toEyeAzimuth);

    float lambda = 4.41f * absAzimuth / (4.41f * absAzimuth + 1.f);
    float toLightG1 = GGXSmithMasking(toLight, dir, roughness);
    float toEyeG1 = GGXSmithMasking(toEye, dir, roughness);

    return lambda * toLightG1 * toEyeG1 + (1 - lambda) * min(toLightG1, toEyeG1);
}

float HeightCorrelatedGGXSmithMaskingShadowing(float3 toLight, float3 toEye, float3 dir, float roughness)
{
    if (dot(toLight, dir) <= 0.f || dot(toEye, dir) <= 0.f)
    {
        return 0.f;
    }

    float toLightLambda = GGXSmithLambda(toLight, dir, roughness);
    float toEyeLambda = GGXSmithLambda(toEye, dir, roughness);

    return 1.f / (1.f + toLightLambda + toEyeLambda);
}

float GGXNormalDistribution(float3 normal, float3 surfaceNormal, float roughness)
{
    if (dot(normal, surfaceNormal) <= 0.f)
    {
        return 0.f;
    }
    
    float ns2 = pow(dot(normal, surfaceNormal), 2.f);
    float r2 = pow(roughness, 2.f);
    
    // See the comments in float3 Lambertian(float3 subsurfaceAlbedo).
    // With the same reason PI is canceled out.
    return r2 / pow((1 + (r2 - 1) * ns2), 2.f);
}

float3 Lambertian(float3 subsurfaceAlbedo, float3 reflectance, float metallic)
{
    // Light strength is defined by the radiance of
    // the light reflected by a white Lambertian surface.
    // In this case light_strength = outgoing_radiance = light_radiance / pi,
    // so PI is canceled out.
    return lerp(1.f - reflectance, 0.f, metallic) * subsurfaceAlbedo;
}

#endif
