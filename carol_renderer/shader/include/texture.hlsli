#ifndef TEXTURE_HEADER
#define TEXTURE_HEADER

float LOD(float depth)
{
    return clamp(pow(depth, 15.0f) * 8.0f, 0.f, 7.f);
}

bool CheckOutOfBounds(float2 pos)
{
    return pos.x < 0.f || pos.x > 1.f || pos.y < 0.f || pos.y > 1.f;
}

#endif
