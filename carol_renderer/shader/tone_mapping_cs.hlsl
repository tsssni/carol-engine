#include "include/common.hlsli"
#include "include/compute.hlsli"
#include "include/texture.hlsli"

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

    return saturate(.6f * saturate(color * (A * color + B) / (color * (C * color + D) + E)));
}

[numthreads(32, 32, 1)]
void main( uint2 dtid : SV_DispatchThreadID )
{
    if (TextureBorderTest(dtid, gRenderTargetSize))
    {
        RWTexture2D<float4> frameMap = ResourceDescriptorHeap[gRWFrameMapIdx];
        float3 color = frameMap[dtid].rgb;
#ifdef ACES
        color = Aces(color);
#elif REINHARD
        color = Reinhard(color);
#else
        color = saturate(color);
#endif
#ifdef GAMMA_CORRECTION
        color = pow(color, 1.f / GAMMA_FACTOR);
#endif
        frameMap[dtid] = float4(color, 1.f);
    }
}