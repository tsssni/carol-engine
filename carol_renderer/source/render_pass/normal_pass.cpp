#include <render_pass/normal_pass.h>
#include <global.h>
#include <render_pass/frame_pass.h>
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

Carol::NormalPass::NormalPass(DXGI_FORMAT normalMapFormat)
	:mNormalMapFormat(normalMapFormat)
{
    InitShaders();
    InitPSOs();
}

void Carol::NormalPass::Draw()
{
    gCommandList->RSSetViewports(1, &mViewport);
    gCommandList->RSSetScissorRects(1, &mScissorRect);

    gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    gCommandList->ClearRenderTargetView(mNormalMap->GetRtv(), DirectX::Colors::Blue, 0, nullptr);
    gCommandList->ClearDepthStencilView(gFramePass->GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
    gCommandList->OMSetRenderTargets(1, GetRvaluePtr(mNormalMap->GetRtv()), true, GetRvaluePtr(gFramePass->GetFrameDsv()));

    gCommandList->SetPipelineState(gPSOs[L"NormalsStatic"]->Get());
    gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_STATIC));
    
    gCommandList->SetPipelineState(gPSOs[L"NormalsSkinned"]->Get());
    gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_SKINNED));

    gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
}

void Carol::NormalPass::Update()
{
}

uint32_t Carol::NormalPass::GetNormalSrvIdx()
{
    return mNormalMap->GetGpuSrvIdx();
}

void Carol::NormalPass::InitShaders()
{
    vector<wstring_view> nullDefines{};

    vector<wstring_view> skinnedDefines =
    {
        L"SKINNED=1"
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

void Carol::NormalPass::InitPSOs()
{
    if (gPSOs.count(L"NormalsStatic") == 0)
    {
		auto normalsStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		normalsStaticMeshPSO->SetRenderTargetFormat(mNormalMapFormat, gFramePass->GetFrameDsvFormat());
		normalsStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
		normalsStaticMeshPSO->SetMS(gShaders[L"NormalsStaticMS"].get());
		normalsStaticMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
		normalsStaticMeshPSO->Finalize();

		gPSOs[L"NormalsStatic"] = std::move(normalsStaticMeshPSO);
    }

	if (gPSOs.count(L"NormalsSkinned") == 0)
    {
		auto normalsSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		normalsSkinnedMeshPSO->SetRenderTargetFormat(mNormalMapFormat, gFramePass->GetFrameDsvFormat());
		normalsSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
		normalsSkinnedMeshPSO->SetMS(gShaders[L"NormalsSkinnedMS"].get());
		normalsSkinnedMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
		normalsSkinnedMeshPSO->Finalize();
		
		gPSOs[L"NormalsSkinned"] = std::move(normalsSkinnedMeshPSO);
    }
}

void Carol::NormalPass::InitBuffers()
{
    D3D12_CLEAR_VALUE optClearValue  = CD3DX12_CLEAR_VALUE(mNormalMapFormat, DirectX::Colors::Blue);
    mNormalMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mNormalMapFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &optClearValue);
}
