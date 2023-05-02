#include <carol.h>
#include <utils/common.h>
#include <fstream>

namespace Carol {
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using std::span;
    using std::ifstream;
    using std::ofstream;
    using std::make_unique;
    using Microsoft::WRL::ComPtr;
}


void Carol::Shader::SetFileName(wstring_view fileName)
{
    mFileName = fileName.data();
}

void Carol::Shader::SetDefines(const vector<wstring_view>& defines)
{
    for (auto& define : defines)
    {
        mDefines.emplace_back(define.data());
    }
}

void Carol::Shader::SetEntryPoint(std::wstring_view entryPoint)
{
    mEntryPoint = entryPoint.data();
}

void Carol::Shader::SetTarget(std::wstring_view target)
{
    mTarget = target.data();
}

void Carol::Shader::Finalize()
{
    auto args = SetArgs();
    auto result = Compile(args);
    CheckError(result);

#if defined _DEBUG
    OutputPdb(result);
#endif

    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(mShader.GetAddressOf()), nullptr);
}


LPVOID Carol::Shader::GetBufferPointer()const
{
    return mShader->GetBufferPointer();
}

size_t Carol::Shader::GetBufferSize()const
{
    return mShader->GetBufferSize();
}

void Carol::Shader::InitCompiler()
{
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(gDXCUtils.GetAddressOf())));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(gDXCCompiler.GetAddressOf())));
    ThrowIfFailed(gDXCUtils->CreateDefaultIncludeHandler(gDXCIncludeHandler.GetAddressOf()));
}

void Carol::Shader::InitShaders()
{
	vector<wstring_view> nullDefines = {};

    vector<wstring_view> skinnedDefines =
	{
		L"SKINNED"
	};

	vector<wstring_view> cullDefines =
	{
		L"FRUSTUM",
		L"NORMAL_CONE"
		L"HIZ_OCCLUSION",
		L"WRITE"
	};

	vector<wstring_view> transparentCullDefines =
	{
		L"FRUSTUM",
		L"NORMAL_CONE"
		L"HIZ_OCCLUSION",
		L"WRITE",
		L"TRANSPARENT"
	};

    gScreenMS = make_unique<Shader>();
    gScreenMS->SetFileName(L"shader\\screen_ms.hlsl");
    gScreenMS->SetDefines({});
    gScreenMS->SetEntryPoint(L"main");
    gScreenMS->SetTarget(L"ms_6_6");
    gScreenMS->Finalize();

    gDisplayPS = make_unique<Shader>();
    gDisplayPS->SetFileName(L"shader\\display_ps.hlsl");
    gDisplayPS->SetDefines({});
    gDisplayPS->SetEntryPoint(L"main");
    gDisplayPS->SetTarget(L"ps_6_6");
    gDisplayPS->Finalize();

    gCullCS = make_unique<Shader>();
    gCullCS->SetFileName(L"shader\\cull_cs.hlsl");
    gCullCS->SetDefines({L"FRUSTUM", L"HIZ_OCCLUSION"});
    gCullCS->SetEntryPoint(L"main");
    gCullCS->SetTarget(L"cs_6_6");
    gCullCS->Finalize();

    gCullAS = make_unique<Shader>();
    gCullAS->SetFileName(L"shader\\cull_as.hlsl");
    gCullAS->SetDefines({});
    gCullAS->SetEntryPoint(L"main");
    gCullAS->SetTarget(L"as_6_6");
    gCullAS->Finalize();

    gOpaqueCullAS = make_unique<Shader>();
    gOpaqueCullAS->SetFileName(L"shader\\cull_as.hlsl");
    gOpaqueCullAS->SetDefines({L"FRUSTUM", L"NORMAL_CONE" L"HIZ_OCCLUSION", L"WRITE"});
    gOpaqueCullAS->SetEntryPoint(L"main");
    gOpaqueCullAS->SetTarget(L"as_6_6");
    gOpaqueCullAS->Finalize();

    gTransparentCullAS = make_unique<Shader>();
	gTransparentCullAS->SetFileName(L"shader\\cull_as.hlsl");
    gTransparentCullAS->SetDefines({L"FRUSTUM", L"NORMAL_CONE" L"HIZ_OCCLUSION", L"TRANSPARENT", L"WRITE"});
    gTransparentCullAS->SetEntryPoint(L"main");
    gTransparentCullAS->SetTarget(L"as_6_6");
    gTransparentCullAS->Finalize();

    gHistHiZCullCS = make_unique<Shader>();
    gHistHiZCullCS->SetFileName(L"shader\\hist_hiz_cull_cs.hlsl");
    gHistHiZCullCS->SetDefines({L"FRUSTUM", L"NORMAL_CONE" L"HIZ_OCCLUSION", L"WRITE"});
    gHistHiZCullCS->SetEntryPoint(L"main");
    gHistHiZCullCS->SetTarget(L"cs_6_6");
    gHistHiZCullCS->Finalize();

    gOpaqueHistHiZCullAS = make_unique<Shader>();
	gOpaqueHistHiZCullAS->SetFileName(L"shader\\hist_hiz_cull_as.hlsl");
    gOpaqueHistHiZCullAS->SetDefines({L"FRUSTUM", L"NORMAL_CONE" L"HIZ_OCCLUSION", L"WRITE"});
    gOpaqueHistHiZCullAS->SetEntryPoint(L"main");
    gOpaqueHistHiZCullAS->SetTarget(L"as_6_6");
    gOpaqueHistHiZCullAS->Finalize();

    gTransparentHistHiZCullAS = make_unique<Shader>();
	gTransparentHistHiZCullAS->SetFileName(L"shader\\hist_hiz_cull_as.hlsl");
    gTransparentHistHiZCullAS->SetDefines({L"FRUSTUM", L"NORMAL_CONE" L"HIZ_OCCLUSION", L"TRANSPARENT", L"WRITE"});
    gTransparentHistHiZCullAS->SetEntryPoint(L"main");
    gTransparentHistHiZCullAS->SetTarget(L"as_6_6");
    gTransparentHistHiZCullAS->Finalize();
	
    gHiZGenerateCS = make_unique<Shader>();
    gHiZGenerateCS->SetFileName(L"shader\\hiz_generate_cs.hlsl");
    gHiZGenerateCS->SetDefines({});
    gHiZGenerateCS->SetEntryPoint(L"main");
    gHiZGenerateCS->SetTarget(L"cs_6_6");
    gHiZGenerateCS->Finalize();

    gDepthStaticCullMS = make_unique<Shader>();
    gDepthStaticCullMS->SetFileName(L"shader\\depth_ms.hlsl");
    gDepthStaticCullMS->SetDefines({L"CULL"});
    gDepthStaticCullMS->SetEntryPoint(L"main");
    gDepthStaticCullMS->SetTarget(L"ms_6_6");
    gDepthStaticCullMS->Finalize();

    gDepthSkinnedCullMS = make_unique<Shader>();
    gDepthSkinnedCullMS->SetFileName(L"shader\\depth_ms.hlsl");
    gDepthSkinnedCullMS->SetDefines({L"CULL", L"SKINNED"});
    gDepthSkinnedCullMS->SetEntryPoint(L"main");
    gDepthSkinnedCullMS->SetTarget(L"ms_6_6");
    gDepthSkinnedCullMS->Finalize();

    gMeshStaticMS = make_unique<Shader>();
    gMeshStaticMS->SetFileName(L"shader\\mesh_ms.hlsl");
    gMeshStaticMS->SetDefines({ L"TAA" });
    gMeshStaticMS->SetEntryPoint(L"main");
    gMeshStaticMS->SetTarget(L"ms_6_6");
    gMeshStaticMS->Finalize();

    gMeshSkinnedMS = make_unique<Shader>();
    gMeshSkinnedMS->SetFileName(L"shader\\mesh_ms.hlsl");
    gMeshSkinnedMS->SetDefines({ L"TAA", L"SKINNED"});
    gMeshSkinnedMS->SetEntryPoint(L"main");
    gMeshSkinnedMS->SetTarget(L"ms_6_6");
    gMeshSkinnedMS->Finalize();

    gBlinnPhongPS = make_unique<Shader>();
    gBlinnPhongPS->SetFileName(L"shader\\opaque_ps.hlsl");
    gBlinnPhongPS->SetDefines({ L"SSAO", L"BLINN_PHONG" });
    gBlinnPhongPS->SetEntryPoint(L"main");
    gBlinnPhongPS->SetTarget(L"ps_6_6");
    gBlinnPhongPS->Finalize();

    gPBRPS = make_unique<Shader>();
    gPBRPS->SetFileName(L"shader\\opaque_ps.hlsl");
    gPBRPS->SetDefines({ L"SSAO",L"GGX",L"SMITH",L"HEIGHT_CORRELATED",L"LAMBERTIAN" });
    gPBRPS->SetEntryPoint(L"main");
    gPBRPS->SetTarget(L"ps_6_6");
    gPBRPS->Finalize();

    gSkyBoxMS = make_unique<Shader>();
    gSkyBoxMS->SetFileName(L"shader\\skybox_ms.hlsl");
    gSkyBoxMS->SetDefines({ L"TAA" });
    gSkyBoxMS->SetEntryPoint(L"main");
    gSkyBoxMS->SetTarget(L"ms_6_6");
    gSkyBoxMS->Finalize();

    gSkyBoxPS = make_unique<Shader>();
    gSkyBoxPS->SetFileName(L"shader\\skybox_ps.hlsl");
    gSkyBoxPS->SetDefines({ L"TAA" });
    gSkyBoxPS->SetEntryPoint(L"main");
    gSkyBoxPS->SetTarget(L"ps_6_6");
    gSkyBoxPS->Finalize();

    gBlinnPhongOitppllPS = make_unique<Shader>();
    gBlinnPhongOitppllPS->SetFileName(L"shader\\oitppll_build_ps.hlsl");
    gBlinnPhongOitppllPS->SetDefines({ L"SSAO", L"BLINN_PHONG" });
    gBlinnPhongOitppllPS->SetEntryPoint(L"main");
    gBlinnPhongOitppllPS->SetTarget(L"ps_6_6");
    gBlinnPhongOitppllPS->Finalize();

    gPBROitppllPS = make_unique<Shader>();
    gPBROitppllPS->SetFileName(L"shader\\oitppll_build_ps.hlsl");
    gPBROitppllPS->SetDefines({ L"SSAO",L"GGX",L"SMITH",L"HEIGHT_CORRELATED",L"LAMBERTIAN" });
    gPBROitppllPS->SetEntryPoint(L"main");
    gPBROitppllPS->SetTarget(L"ps_6_6");
    gPBROitppllPS->Finalize();

    gDrawOitppllPS = make_unique<Shader>();
    gDrawOitppllPS->SetFileName(L"shader\\oitppll_ps.hlsl");
    gDrawOitppllPS->SetDefines({});
    gDrawOitppllPS->SetEntryPoint(L"main");
    gDrawOitppllPS->SetTarget(L"ps_6_6");
    gDrawOitppllPS->Finalize();

    gNormalsStaticMS = make_unique<Shader>();
    gNormalsStaticMS->SetFileName(L"shader\\normals_ms.hlsl");
    gNormalsStaticMS->SetDefines({});
    gNormalsStaticMS->SetEntryPoint(L"main");
    gNormalsStaticMS->SetTarget(L"ms_6_6");
    gNormalsStaticMS->Finalize();
    
    gNormalsSkinnedMS = make_unique<Shader>();
    gNormalsSkinnedMS->SetFileName(L"shader\\normals_ms.hlsl");
    gNormalsSkinnedMS->SetDefines({L"SKINNED"});
    gNormalsSkinnedMS->SetEntryPoint(L"main");
    gNormalsSkinnedMS->SetTarget(L"ms_6_6");
    gNormalsSkinnedMS->Finalize();

    gNormalsPS = make_unique<Shader>();
    gNormalsPS->SetFileName(L"shader\\normals_ps.hlsl");
    gNormalsPS->SetDefines({});
    gNormalsPS->SetEntryPoint(L"main");
    gNormalsPS->SetTarget(L"ps_6_6");
    gNormalsPS->Finalize();

    gSsaoCS = make_unique<Shader>();
    gSsaoCS->SetFileName(L"shader\\ssao_cs.hlsl");
    gSsaoCS->SetDefines({});
    gSsaoCS->SetEntryPoint(L"main");
    gSsaoCS->SetTarget(L"cs_6_6");
    gSsaoCS->Finalize();

    gEpfCS = make_unique<Shader>();
    gEpfCS->SetFileName(L"shader\\epf_cs.hlsl");
    gEpfCS->SetDefines({ L"BORDER_RADIUS=5",L"BLUR_COUNT=3" });
    gEpfCS->SetEntryPoint(L"main");
    gEpfCS->SetTarget(L"cs_6_6");
    gEpfCS->Finalize();

    gVelocityStaticMS = make_unique<Shader>();
    gVelocityStaticMS->SetFileName(L"shader\\velocity_ms.hlsl");
    gVelocityStaticMS->SetDefines({});
    gVelocityStaticMS->SetEntryPoint(L"main");
    gVelocityStaticMS->SetTarget(L"ms_6_6");
    gVelocityStaticMS->Finalize();

    gVelocitySkinnedMS = make_unique<Shader>();
    gVelocitySkinnedMS->SetFileName(L"shader\\velocity_ms.hlsl");
    gVelocitySkinnedMS->SetDefines({L"SKINNED"});
    gVelocitySkinnedMS->SetEntryPoint(L"main");
    gVelocitySkinnedMS->SetTarget(L"ms_6_6");
    gVelocitySkinnedMS->Finalize();

    gVelocityPS = make_unique<Shader>();
    gVelocityPS->SetFileName(L"shader\\velocity_ps.hlsl");
    gVelocityPS->SetDefines({});
    gVelocityPS->SetEntryPoint(L"main");
    gVelocityPS->SetTarget(L"ps_6_6");
    gVelocityPS->Finalize();

    gTaaCS = make_unique<Shader>();
    gTaaCS->SetFileName(L"shader\\taa_cs.hlsl");
    gTaaCS->SetDefines({});
    gTaaCS->SetEntryPoint(L"main");
    gTaaCS->SetTarget(L"cs_6_6");
    gTaaCS->Finalize();
    
    gLDRToneMappingCS = make_unique<Shader>();
    gLDRToneMappingCS->SetFileName(L"shader\\tone_mapping_cs.hlsl");
    gLDRToneMappingCS->SetDefines({});
    gLDRToneMappingCS->SetEntryPoint(L"main");
    gLDRToneMappingCS->SetTarget(L"cs_6_6");
    gLDRToneMappingCS->Finalize();
}

Carol::vector<LPCWSTR> Carol::Shader::SetArgs()
{
    vector<LPCWSTR> args;

#if defined _DEBUG
    args.push_back(DXC_ARG_DEBUG);
    args.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
#else
    args.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif

    args.push_back(L"-E");
    args.push_back(mEntryPoint.c_str());

    args.push_back(L"-T");
    args.push_back(mTarget.c_str());

    args.push_back(L"-I");
    args.push_back(L"shader");

    for (auto& def : mDefines)
    {
        args.push_back(L"-D");
        args.push_back(def.c_str());
    }

    args.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
    args.push_back(L"-Qstrip_reflect");
    args.push_back(L"-Qstrip_debug");

    return args;
}

Carol::ComPtr<IDxcResult> Carol::Shader::Compile(span<LPCWSTR> args)
{
    ComPtr<IDxcBlobEncoding> sourceBlob;
    ThrowIfFailed(gDXCUtils->LoadFile(mFileName.c_str(), nullptr, sourceBlob.GetAddressOf()));

    DxcBuffer sourceBuffer;
    sourceBuffer.Encoding = DXC_CP_ACP;
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();

    ComPtr<IDxcResult> result;
    ThrowIfFailed(gDXCCompiler->Compile(&sourceBuffer, args.data(), args.size(), gDXCIncludeHandler.Get(), IID_PPV_ARGS(result.GetAddressOf())));

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