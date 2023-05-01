#include <dx12/root_signature.h>
#include <dx12/descriptor.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>

namespace Carol {
    using std::make_unique;
}

Carol::RootSignature::RootSignature(ID3D12Device* device)
{
    Shader rootSignatureShader;
    rootSignatureShader.SetFileName(L"shader\\root_signature.hlsl");
    rootSignatureShader.SetDefines({});
    rootSignatureShader.SetEntryPoint(L"main");
    rootSignatureShader.SetTarget(L"ms_6_6");
    rootSignatureShader.Finalize();

    ThrowIfFailed(device->CreateRootSignature(0, rootSignatureShader.GetBufferPointer(), rootSignatureShader.GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

ID3D12RootSignature* Carol::RootSignature::Get()const
{
    return mRootSignature.Get();
}

