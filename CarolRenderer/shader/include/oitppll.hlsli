#ifndef OITPPLL_HEADER
#define OITPPLL_HEADER

struct OitNode
{
    float4 ColorU;
    uint DepthU;
    uint NextU;
};

cbuffer OitppllCB : register(b3)
{
    uint gOitppllDepthMapIdx;
    uint3 gOitppllPad0;
}

#endif