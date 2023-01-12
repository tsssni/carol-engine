#include "include/common.hlsli"
#include "include/oitppll.hlsli"

#define MAX_SORTED_PIXELS 16

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

void SortPixels(inout OitNode sortedPixels[MAX_SORTED_PIXELS])
{
    [unroll]
    for (int i = 0; i < MAX_SORTED_PIXELS - 1; ++i)
    {
        [unroll]
        for (int j = 0; j < MAX_SORTED_PIXELS - i - 1; ++j)
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

    static OitNode sortedPixels[MAX_SORTED_PIXELS];

    if (offset == 0xffffffff)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    int i = 0;

    // In order to unroll the circulation to accelerate the program
    // some extra pixels with full transparent colors are added
    [unroll]
    for (i = 0; i < MAX_SORTED_PIXELS; ++i)
    {
        [flatten]
        if (offset != 0xffffffff)
        {
            sortedPixels[i] = oitNodeBuffer[offset];
            offset = sortedPixels[i].NextU;
        }
        else
        {
            sortedPixels[i].ColorU = float4(0.0f, 0.0f, 0.0f, 0.0f);
            sortedPixels[i].DepthU = 0xffffffff;
        }
    }

    SortPixels(sortedPixels);
    float4 color = sortedPixels[0].ColorU;

    [unroll]
    for (i = 1; i < MAX_SORTED_PIXELS; ++i)
    {
        float4 underColor = sortedPixels[i].ColorU;
        
        // Full transparent color will make the output dark
        // because color will be multiplied by its alpha value
        // multiple times, so we need to recify full transparent
        // color here
        float isFullTransparent = 1.0f - ceil(underColor.a);
        float4 src = float4(underColor.rgb + isFullTransparent * color.rgb, underColor.a + (1 - underColor.a) * isFullTransparent);
        
        color.rgb = color.rgb * color.a + (1 - color.a) * src.rgb * src.a;
        color.a = color.a + underColor.a - color.a * underColor.a;
    }

    return color;
}