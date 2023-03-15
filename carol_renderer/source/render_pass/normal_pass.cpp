#include <render_pass/normal_pass.h>
#include <global.h>
#include <render_pass/display_pass.h>
#include <scene/scene.h>
#include <dx12.h>
#include <DirectXColors.h>
#include <string_view>

namespace Carol
{
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using std::make_unique;
    using namespace DirectX;
}

Carol::NormalPass::NormalPass(
    ID3D12Device* device,
    DXGI_FORMAT normalMapFormat,
    DXGI_FORMAT normalDsvFormat)
	:mNormalMapFormat(normalMapFormat),
    mNormalDsvFormat(normalDsvFormat),
    mIndirectCommandBuffer(MESH_TYPE_COUNT)
{
    InitShaders();
    InitPSOs(device);
}

void Carol::NormalPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->RSSetViewports(1, &mViewport);
    cmdList->RSSetScissorRects(1, &mScissorRect);

    mNormalMap->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ClearRenderTargetView(mNormalMap->GetRtv(), DirectX::Colors::Blue, 0, nullptr);
    cmdList->ClearDepthStencilView(mNormalDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(1, GetRvaluePtr(mNormalMap->GetRtv()), true, GetRvaluePtr(mNormalDepthStencilMap->GetDsv()));

    cmdList->SetPipelineState(gPSOs[L"NormalsStatic"]->Get());
    ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_STATIC]);
    
    cmdList->SetPipelineState(gPSOs[L"NormalsSkinned"]->Get());
    ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_SKINNED]);

    mNormalMap->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Carol::NormalPass::SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
{
    mIndirectCommandBuffer[type] = indirectCommandBuffer;
}

uint32_t Carol::NormalPass::GetNormalSrvIdx()const
{
    return mNormalMap->GetGpuSrvIdx();
}

void Carol::NormalPass::InitShaders()
{
    vector<wstring_view> nullDefines{};

    vector<wstring_view> skinnedDefines =
    {
        L"SKINNED"
    };

    if (gShaders.count(L"NormalsStaticMS") == 0)
    {
        gShaders[L"NormalsStaticMS"] = make_unique<Shader>(L"shader\\normals_ms.hlsl", nullDefines, L"main", L"ms_6_6");
    }

    if (gShaders.count(L"NormalsPS") == 0)
    {
        gShaders[L"NormalsPS"] = make_unique<Shader>(L"shader\\normals_ps.hlsl", nullDefines, L"main", L"ps_6_6");
    }

    if (gShaders.count(L"NormalsSkinnedMS") == 0)
    {
        gShaders[L"NormalsSkinnedMS"] = make_unique<Shader>(L"shader\\normals_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
    }
}

void Carol::NormalPass::InitPSOs(ID3D12Device* device)
{
    if (gPSOs.count(L"NormalsStatic") == 0)
    {
		auto normalsStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
        normalsStaticMeshPSO->SetRootSignature(sRootSignature.get());
		normalsStaticMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mNormalDsvFormat);
		normalsStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
		normalsStaticMeshPSO->SetMS(gShaders[L"NormalsStaticMS"].get());
		normalsStaticMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
		normalsStaticMeshPSO->Finalize(device);

		gPSOs[L"NormalsStatic"] = std::move(normalsStaticMeshPSO);
    }

	if (gPSOs.count(L"NormalsSkinned") == 0)
    {
		auto normalsSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
        normalsSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
		normalsSkinnedMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mNormalDsvFormat);
		normalsSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
		normalsSkinnedMeshPSO->SetMS(gShaders[L"NormalsSkinnedMS"].get());
		normalsSkinnedMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
		normalsSkinnedMeshPSO->Finalize(device);
		
		gPSOs[L"NormalsSkinned"] = std::move(normalsSkinnedMeshPSO);
    }
}

void Carol::NormalPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
    D3D12_CLEAR_VALUE optClearValue  = CD3DX12_CLEAR_VALUE(mNormalMapFormat, DirectX::Colors::Blue);
    mNormalMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mNormalMapFormat,
        device,
        heap,
        descriptorManager,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &optClearValue);

    D3D12_CLEAR_VALUE depthStencilOptClearValue = CD3DX12_CLEAR_VALUE(GetDsvFormat(mNormalDsvFormat), 1.f, 0);
	mNormalDepthStencilMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mNormalDsvFormat,
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&depthStencilOptClearValue);
}
