#ifndef TEXTURE_HEADER
#define TEXTURE_HEADER

float LOD(float depth)
{
    return clamp(pow(depth, 15.f) * 8.f, 0.f, 7.f);
}

bool CheckOutOfBounds(float3 pos)
{
    return 
    pos.x < 0.f || pos.x >= 1.f 
    || pos.y < 0.f || pos.y >= 1.f
    || pos.z < 0.f || pos.z >= 1.f;
}

#endif
