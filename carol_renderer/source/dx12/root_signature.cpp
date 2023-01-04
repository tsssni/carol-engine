#include <dx12/root_signature.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>

namespace Carol {
    using std::make_unique;
}

Carol::RootSignature::RootSignature(ID3D12Device* device, DescriptorAllocator* allocator)
{
    Shader rootSignatureShader(L"shader\\root_signature.hlsl", {}, L"main", L"ms_6_6");
    ThrowIfFailed(device->CreateRootSignature(0, rootSignatureShader.GetBufferPointer(), rootSignatureShader.GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

ID3D12RootSignature* Carol::RootSignature::Get()
{
    return mRootSignature.Get();
}

