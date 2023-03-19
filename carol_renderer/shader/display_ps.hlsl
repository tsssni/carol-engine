#include "include/common.hlsli"

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float4 main(PixelIn pin) : SV_Target
{
    RWTexture2D<float4> frameMap = ResourceDescriptorHeap[gRWFrameMapIdx];
    return frameMap[uint2(pin.PosH.x, pin.PosH.y)];
}