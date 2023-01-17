#include <dx12/root_signature.h>
#include <dx12/descriptor.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>
#include <global.h>

namespace Carol {
    using std::make_unique;
}

Carol::RootSignature::RootSignature()
{
    Shader rootSignatureShader(L"shader\\root_signature.hlsl", {}, L"main", L"ms_6_6");
    ThrowIfFailed(gDevice->CreateRootSignature(0, rootSignatureShader.GetBufferPointer(), rootSignatureShader.GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

ID3D12RootSignature* Carol::RootSignature::Get()const
{
    return mRootSignature.Get();
}

