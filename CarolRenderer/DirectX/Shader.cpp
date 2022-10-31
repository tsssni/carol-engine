#include "Shader.h"
#include "../Utils/Common.h"
#include "Buffer.h"
#include "d3dx12.h"
#include <d3dcompiler.h>

using std::make_unique;

Carol::Blob* Carol::Shader::GetBlob()
{
    return mShader.get();
}

void Carol::Shader::CompileShader(const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entryPoint, const std::string& target)
{
    uint32_t compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    mShader = make_unique<Blob>();
    Blob errorBlob;

    ThrowIfFailed(D3DCompileFromFile(fileName.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), target.c_str(), compileFlags, 0, mShader->GetAddressOf(), errorBlob.GetAddressOf()));

    if (errorBlob.Get())
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
}
