#include <dx12/shader.h>
#include <utils/common.h>
#include <fstream>

namespace Carol {
    using std::vector;
    using std::wstring;
    using std::ifstream;
    using std::ofstream;
    using Microsoft::WRL::ComPtr;
}

Carol::ComPtr<IDxcCompiler3> Carol::Shader::Compiler = nullptr;
Carol::ComPtr<IDxcUtils> Carol::Shader::Utils = nullptr;
Carol::ComPtr<IDxcIncludeHandler> Carol::Shader::IncludeHandler = nullptr;

Carol::Shader::Shader(const wstring& fileName, const vector<wstring>& defines, const wstring& entryPoint, const wstring& target)
{
    auto args = SetArgs(defines, entryPoint, target);
    auto result = Compile(fileName, args);
    CheckError(result);

#if _DEBUG
    OutputPdb(result);
#endif

    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(mShader.GetAddressOf()), nullptr);
}


LPVOID Carol::Shader::GetBufferPointer()
{
    return mShader->GetBufferPointer();
}

size_t Carol::Shader::GetBufferSize()
{
    return mShader->GetBufferSize();
}

void Carol::Shader::InitCompiler()
{
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(Utils.GetAddressOf())));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(Compiler.GetAddressOf())));
    ThrowIfFailed(Utils->CreateDefaultIncludeHandler(IncludeHandler.GetAddressOf()));
}

Carol::vector<LPCWSTR> Carol::Shader::SetArgs(const vector<wstring>& defines, const wstring& entryPoint, const wstring& target)
{
    vector<LPCWSTR> args;

#if defined(DEBUG) || defined(_DEBUG)
    args.push_back(DXC_ARG_DEBUG);
    args.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
#else
    args.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif

    args.push_back(L"-E");
    args.push_back(entryPoint.c_str());

    args.push_back(L"-T");
    args.push_back(target.c_str());

    args.push_back(L"-I");
    args.push_back(L"shader");

    for (auto& def : defines)
    {
        args.push_back(L"-D");
        args.push_back(def.c_str());
    }

    args.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
    args.push_back(L"-Qstrip_reflect");
    args.push_back(L"-Qstrip_debug");

    return args;
}

Carol::ComPtr<IDxcResult> Carol::Shader::Compile(const wstring& fileName, vector<LPCWSTR>& args)
{
    ComPtr<IDxcBlobEncoding> sourceBlob;
    ThrowIfFailed(Utils->LoadFile(fileName.c_str(), nullptr, sourceBlob.GetAddressOf()));
    DxcBuffer sourceBuffer;
    sourceBuffer.Encoding = DXC_CP_ACP;
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();

    ComPtr<IDxcResult> result;
    ThrowIfFailed(Compiler->Compile(&sourceBuffer, args.data(), args.size(), IncludeHandler.Get(), IID_PPV_ARGS(result.GetAddressOf())));

    return result;
}

void Carol::Shader::CheckError(ComPtr<IDxcResult>& result)
{
    ComPtr<IDxcBlobUtf8> errors = nullptr;
    ThrowIfFailed(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr));
    if (errors != nullptr && errors->GetStringLength() != 0)
    {
        OutputDebugStringA(errors->GetStringPointer());
    }

    HRESULT statusHR;
    result->GetStatus(&statusHR);
    ThrowIfFailed(statusHR);
}

void Carol::Shader::OutputPdb(ComPtr<IDxcResult>& result)
{
    IDxcBlob* debugData = nullptr;;
    IDxcBlobUtf16* debugDataPath = nullptr;
    ThrowIfFailed(result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&debugData), &debugDataPath));

    wstring pdbFileName = debugDataPath->GetStringPointer();
    pdbFileName = L"shader\\pdb\\" + pdbFileName;

    ofstream ofs(pdbFileName, ifstream::binary);
    if (ofs.is_open())
    {
        ofs.write((const char*)debugData->GetBufferPointer(), debugData->GetBufferSize());
    }
    ofs.close();
}