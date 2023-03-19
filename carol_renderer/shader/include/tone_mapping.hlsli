#define GAMMA_FACTOR 2.2f

float3 Reinhard(float3 color)
{
    return color / (1.f + color);
}

float3 Aces(float3 color)
{
    const float A = 2.51f;
	const float B = 0.03f;
	const float C = 2.43f;
	const float D = 0.59f;
	const float E = 0.14f;

    return .6f * saturate(color * (A * color + B) / (color * (C * color + D) + E));
}

float3 ToneMapping(float3 color)
{
    float3 toneMappedColor = color;
#ifdef LDR
    toneMappedColor=saturate(toneMappedColor);
#else
    #ifdef REINHARD
    toneMappedColor = Reinhard(color);
    #elif ACES
    toneMappedColor = Aces(color);
    #endif
#endif
#ifdef GAMMA_CORRECTION
    toneMappedColor = pow(toneMappedColor, GAMMA_FACTOR);
#endif
    return toneMappedColor;
}