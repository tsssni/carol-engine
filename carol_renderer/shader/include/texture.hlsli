#ifndef TEXTURE_HEADER
#define TEXTURE_HEADER

float LOD(float depth)
{
    return clamp(pow(depth, 15.f) * 8.f, 0.f, 7.f);
}

bool CheckOutOfBounds(float3 pos)
{
    return 
    pos.x < 0.01f || pos.x >= 0.99f 
    || pos.y < 0.01f || pos.y >= 0.99f
    || pos.z < 0.01f || pos.z >= 0.99f;
}

#endif
