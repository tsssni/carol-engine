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

#endif
