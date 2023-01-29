export module carol.renderer.dx12.shader;
import carol.renderer.utils;
import <dxcapi.h>;
import <wrl/client.h>;
import <vector>;
import <string>;
import <string_view>;
import <span>;
import <memory>;
import <fstream>;
import <unordered_map>;

namespace Carol
{
    using Microsoft::WRL::ComPtr;
    using std::ifstream;
    using std::ofstream;
    using std::span;
    using std::unique_ptr;
    using std::unordered_map;
    using std::vector;
    using std::wstring;
    using std::wstring_view;

    export class Shader
    {
    public:
        Shader(
            wstring_view fileName,
            span<const wstring_view> defines,
            wstring_view entryPoint,
            wstring_view target)
        {
            auto args = SetArgs(defines, entryPoint, target);
            auto result = Compile(fileName, args);
            CheckError(result);

#if defined _DEBUG
            OutputPdb(result);
#endif

            result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(mShader.GetAddressOf()), nullptr);
        }

        LPVOID GetBufferPointer() const
        {
            return mShader->GetBufferPointer();
        }

        size_t GetBufferSize() const
        {
            return mShader->GetBufferSize();
        }

        static void InitCompiler()
        {
            ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(sUtils.GetAddressOf())));
            ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(sCompiler.GetAddressOf())));
            ThrowIfFailed(sUtils->CreateDefaultIncludeHandler(sIncludeHandler.GetAddressOf()));
        }

    private:
        vector<LPCWSTR> SetArgs(span<const wstring_view> defines, wstring_view entryPoint, wstring_view target)
        {
            vector<LPCWSTR> args;

#if defined _DEBUG
            args.push_back(DXC_ARG_DEBUG);
            args.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
#else
            args.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif

            args.push_back(L"-E");
            args.push_back(entryPoint.data());

            args.push_back(L"-T");
            args.push_back(target.data());

            args.push_back(L"-I");
            args.push_back(L"shader");

            for (auto& def : defines)
            {
                args.push_back(L"-D");
                args.push_back(def.data());
            }

            args.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
            args.push_back(L"-Qstrip_reflect");
            args.push_back(L"-Qstrip_debug");

            return args;
        }

        ComPtr<IDxcResult> Compile(wstring_view fileName, span<LPCWSTR> args)
        {
            ComPtr<IDxcBlobEncoding> sourceBlob;
            ThrowIfFailed(sUtils->LoadFile(fileName.data(), nullptr, sourceBlob.GetAddressOf()));

            DxcBuffer sourceBuffer;
            sourceBuffer.Encoding = DXC_CP_ACP;
            sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
            sourceBuffer.Size = sourceBlob->GetBufferSize();

            ComPtr<IDxcResult> result;
            ThrowIfFailed(sCompiler->Compile(&sourceBuffer, args.data(), args.size(), sIncludeHandler.Get(), IID_PPV_ARGS(result.GetAddressOf())));

            return result;
        }

        void CheckError(ComPtr<IDxcResult> &result)
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
        
        void OutputPdb(ComPtr<IDxcResult> &result)
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

        static ComPtr<IDxcCompiler3> sCompiler;
        static ComPtr<IDxcUtils> sUtils;
        static ComPtr<IDxcIncludeHandler> sIncludeHandler;
        ComPtr<IDxcBlob> mShader;
    };

    ComPtr<IDxcCompiler3> Shader::sCompiler = nullptr;
    ComPtr<IDxcUtils> Shader::sUtils = nullptr;
    ComPtr<IDxcIncludeHandler> Shader::sIncludeHandler = nullptr;
    
    export unordered_map<wstring, unique_ptr<Shader>> gShaders;
}