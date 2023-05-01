#include <global.h>

namespace Carol {
    using std::make_unique;
}

Carol::RootSignature::RootSignature()
{
    Shader rootSignatureShader;
    rootSignatureShader.SetFileName(L"shader\\root_signature.hlsl");
    rootSignatureShader.SetDefines({});
    rootSignatureShader.SetEntryPoint(L"main");
    rootSignatureShader.SetTarget(L"ms_6_6");
    rootSignatureShader.Finalize();

    ThrowIfFailed(gDevice->CreateRootSignature(0, rootSignatureShader.GetBufferPointer(), rootSignatureShader.GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

ID3D12RootSignature* Carol::RootSignature::Get()const
{
    return mRootSignature.Get();
}

