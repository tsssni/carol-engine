#include "include/common.hlsli"
#include "include/oitppll.hlsli"

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

float4 main(PixelIn pin) : SV_Target
{
    StructuredBuffer<OitNode> oitNodeBuffer = ResourceDescriptorHeap[gOitBufferRIdx];
    Buffer<uint> startOffsetBuffer = ResourceDescriptorHeap[gOitOffsetBufferRIdx];
    
    uint2 pixelPos = uint2(pin.PosH.x - 0.5f, pin.PosH.y - 0.5f);
    uint startOffsetAddr = pixelPos.y * gRenderTargetSize.x + pixelPos.x;
    uint offset = startOffsetBuffer[startOffsetAddr];

    OitNode sortedPixels[MAX_SORTED_PIXELS];

    if (offset == 0xffffffff)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
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

    return color;
}