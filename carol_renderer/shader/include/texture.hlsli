#ifndef TEXTURE_HEADER
#define TEXTURE_HEADER

float LOD(float depth)
{
    return 0.f;
}

bool CheckOutOfBounds(float3 pos)
{
    return 
    pos.x < 0.01f || pos.x >= 0.99f 
    || pos.y < 0.01f || pos.y >= 0.99f
    || pos.z < 0.01f || pos.z >= 0.99f;
}

float4 GetTexCoord(float4 pos)
{
    pos.xyz /= pos.w;
    pos.x = 0.5f * pos.x + 0.5f;
    pos.y = -0.5f * pos.y + 0.5f;

    return pos;
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
