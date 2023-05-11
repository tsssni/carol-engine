#ifndef TEXTURE_HEADER
#define TEXTURE_HEADER

float LOD(float depth)
{
    return 0.f;
}

bool TextureBorderTest(int2 pos, uint2 size)
{
    return pos.x >= 0 && pos.x < size.x && pos.y >= 0 && pos.y < size.y;
}

bool TextureBorderTest(float2 pos)
{
    return
    pos.x >= 0.0f || pos.x < 1.f
    || pos.y >= 0.0f || pos.y < 1.f;
}

bool TextureBorderTest(float3 pos)
{
    return 
    pos.x >= 0.0f || pos.x < 1.f 
    || pos.y >= 0.0f || pos.y < 1.f
    || pos.z >= 0.0f || pos.z < 1.f;
}

float3 NormalToWorldSpace(float3 normal, float3 normalW, float3 tangentW)
{
    normal = 2.0f * normal - 1.0f;
    
    float3 N = normalW;
    float3 T = normalize(tangentW - dot(normalW, tangentW) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    return mul(normal, TBN);
}
#endif
