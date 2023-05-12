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
