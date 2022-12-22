#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT),"\
"CBV(b0, space = 0),"\
"CBV(b1, space = 0),"\
"CBV(b2, space = 0),"\
"CBV(b3, space = 0),"\
"DescriptorTable(SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE), visibility = SHADER_VISIBILITY_PIXEL),"\
"DescriptorTable(SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE), visibility = SHADER_VISIBILITY_PIXEL),"\
"DescriptorTable(SRV(t0, space = 2, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE), visibility = SHADER_VISIBILITY_PIXEL),"\
"DescriptorTable(SRV(t0, space = 3, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE), visibility = SHADER_VISIBILITY_PIXEL),"\
"DescriptorTable(UAV(u0, space = 0, numDescriptors = 3), visibility = SHADER_VISIBILITY_PIXEL),"\
"DescriptorTable(SRV(t0, space = 4, numDescriptors = 2), visibility = SHADER_VISIBILITY_PIXEL),"\
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
void main()
{
}
