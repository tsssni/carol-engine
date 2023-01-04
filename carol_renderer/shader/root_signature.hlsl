#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED),"\
"CBV(b0, space = 0),"\
"RootConstants(num32BitConstants = 10, b1, space = 0),"\
"CBV(b2, space = 0),"\
"RootConstants(num32BitConstants = 4, b3, space = 0),"\
"CBV(b4, space = 0),"\
"RootConstants(num32BitConstants = 15, b5, space = 0),"\
"StaticSampler(s0,"\
"filter = FILTER_MIN_MAG_MIP_POINT,"\
"addressU = TEXTURE_ADDRESS_WRAP,"\
"addressV = TEXTURE_ADDRESS_WRAP,"\
"addressW = TEXTURE_ADDRESS_WRAP),"\
"StaticSampler(s1,"\
"filter = FILTER_MIN_MAG_MIP_POINT,"\
"addressU = TEXTURE_ADDRESS_CLAMP,"\
"addressV = TEXTURE_ADDRESS_CLAMP,"\
"addressW = TEXTURE_ADDRESS_CLAMP),"\
"StaticSampler(s2,"\
"filter = FILTER_MIN_MAG_MIP_LINEAR,"\
"addressU = TEXTURE_ADDRESS_WRAP,"\
"addressV = TEXTURE_ADDRESS_WRAP,"\
"addressW = TEXTURE_ADDRESS_WRAP),"\
"StaticSampler(s3,"\
"filter = FILTER_MIN_MAG_MIP_LINEAR,"\
"addressU = TEXTURE_ADDRESS_CLAMP,"\
"addressV = TEXTURE_ADDRESS_CLAMP,"\
"addressW = TEXTURE_ADDRESS_CLAMP),"\
"StaticSampler(s4,"\
"filter = FILTER_ANISOTROPIC,"\
"addressU = TEXTURE_ADDRESS_WRAP,"\
"addressV = TEXTURE_ADDRESS_WRAP,"\
"addressW = TEXTURE_ADDRESS_WRAP,"\
"mipLODBias = 0.0f,"\
"maxAnisotropy = 8),"\
"StaticSampler(s5,"\
"filter = FILTER_MIN_MAG_MIP_POINT,"\
"addressU = TEXTURE_ADDRESS_CLAMP,"\
"addressV = TEXTURE_ADDRESS_CLAMP,"\
"addressW = TEXTURE_ADDRESS_CLAMP,"\
"mipLODBias = 0.0f,"\
"maxAnisotropy = 8),"\
"StaticSampler(s6,"\
"filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,"\
"addressU = TEXTURE_ADDRESS_BORDER,"\
"addressV = TEXTURE_ADDRESS_BORDER,"\
"addressW = TEXTURE_ADDRESS_BORDER,"\
"mipLODBias = 0.0f,"\
"maxAnisotropy = 16,"\
"comparisonFunc = COMPARISON_LESS_EQUAL,"\
"borderColor = STATIC_BORDER_COLOR_OPAQUE_BLACK)"

[RootSignature(RS)]
[numthreads(1, 1, 1)]
[OutputTopology("triangle")]
void main()
{
}
