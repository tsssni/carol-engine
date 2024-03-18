#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <utils/exception.h>
#include <global.h>

Carol::RootSignature::RootSignature()
{
    Shader rootSignatureShader("shader/dxil/root_signature.dxil");
    ThrowIfFailed(gDevice->CreateRootSignature(0, rootSignatureShader.GetBufferPointer(), rootSignatureShader.GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

ID3D12RootSignature* Carol::RootSignature::Get()const
{
    return mRootSignature.Get();
}

