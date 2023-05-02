#include "include/common.hlsli"
#include "include/oitppll.hlsli"
#include "include/texture.hlsli"

#define MAX_SORTED_PIXELS 16

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

void SortPixels(inout OitNode sortedPixels[MAX_SORTED_PIXELS], uint count)
{
    for (int i = 0; i < count - 1; ++i)
    {
        for (int j = 0; j < count - i - 1; ++j)
        {
            if (sortedPixels[j].DepthU > sortedPixels[j + 1].DepthU)
            {
                OitNode temp = sortedPixels[j];
                sortedPixels[j] = sortedPixels[j + 1];
                sortedPixels[j + 1] = temp;
            }
        }
    }

}

[numthreads(32, 32, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    RWTexture2D<float4> frameMap = ResourceDescriptorHeap[gRWFrameMapIdx];
    StructuredBuffer<OitNode> oitNodeBuffer = ResourceDescriptorHeap[gOitppllBufferIdx];
    Buffer<uint> startOffsetBuffer = ResourceDescriptorHeap[gOitppllStartOffsetBufferIdx];
    
    if (TextureBorderTest(dtid, gRenderTargetSize))
    {
        uint startOffsetAddr = dtid.y * gRenderTargetSize.x + dtid.x;
        uint offset = startOffsetBuffer[startOffsetAddr];

        OitNode sortedPixels[MAX_SORTED_PIXELS];

        if (offset == 0xffffffff)
        {
            return;
        }

        int count = 0;

        for (; count < MAX_SORTED_PIXELS && offset != 0xffffffff; ++count)
        {
            sortedPixels[count] = oitNodeBuffer[offset];
            offset = sortedPixels[count].NextU;
        }

        SortPixels(sortedPixels, count);
        float4 color = sortedPixels[0].ColorU;

        for (int i = 1; i < count; ++i)
        {
            float4 underColor = sortedPixels[i].ColorU;
            color.rgb = color.rgb * color.a + (1 - color.a) * underColor.rgb * underColor.a;
            color.a = color.a + underColor.a - color.a * underColor.a;
        }

        frameMap[dtid] = color * color.a + frameMap[dtid] * (1.f - color.a);
    }
}