#include <render_pass/taa_pass.h>
#include <global.h>
#include <render_pass/display_pass.h>
#include <render_pass/frame_pass.h>
#include <scene/scene.h>
#include <dx12.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <cmath>
#include <string_view>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::make_unique;
	using Microsoft::WRL::ComPtr;
}

Carol::TaaPass::TaaPass(
	DXGI_FORMAT velocityMapFormat,
	DXGI_FORMAT frameFormat)
	 :mVelocityMapFormat(velocityMapFormat),
	 mFrameFormat(frameFormat)
{
	InitHalton();
	InitShaders();
	InitPSOs();
}

void Carol::TaaPass::Update()
{
}

void Carol::TaaPass::ReleaseIntermediateBuffers()
{
}

void Carol::TaaPass::InitShaders()
{
	vector<wstring_view> nullDefines{};

	vector<wstring_view> skinnedDefines =
	{
		L"SKINNED=1"
	};

	gShaders[L"TaaVelocityStaticMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	gShaders[L"TaaVelocityPS"] = make_unique<Shader>(L"shader\\velocity_ps.hlsl", nullDefines, L"main", L"ps_6_6");
	gShaders[L"TaaVelocitySkinnedMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	gShaders[L"TaaOutputPS"] = make_unique<Shader>(L"shader\\taa_ps.hlsl", nullDefines, L"main", L"ps_6_6");
}

void Carol::TaaPass::InitPSOs()
{
	auto velocityStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	velocityStaticMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, gFramePass->GetFrameDsvFormat());
	velocityStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
	velocityStaticMeshPSO->SetMS(gShaders[L"TaaVelocityStaticMS"].get());
	velocityStaticMeshPSO->SetPS(gShaders[L"TaaVelocityPS"].get());
	velocityStaticMeshPSO->Finalize();
	gPSOs[L"VelocityStatic"] = std::move(velocityStaticMeshPSO);

	auto velocitySkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	velocitySkinnedMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, gFramePass->GetFrameDsvFormat());
	velocitySkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
	velocitySkinnedMeshPSO->SetMS(gShaders[L"TaaVelocitySkinnedMS"].get());
	velocitySkinnedMeshPSO->SetPS(gShaders[L"TaaVelocityPS"].get());
	velocitySkinnedMeshPSO->Finalize();
	gPSOs[L"VelocitySkinned"] = std::move(velocitySkinnedMeshPSO);

	auto outputMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	outputMeshPSO->SetDepthStencilState(gDepthDisabledState);
	outputMeshPSO->SetRenderTargetFormat(gFramePass->GetFrameRtvFormat());
	outputMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
	outputMeshPSO->SetPS(gShaders[L"TaaOutputPS"].get());
	outputMeshPSO->Finalize();
	gPSOs[L"TaaOutput"] = std::move(outputMeshPSO);
}

void Carol::TaaPass::Draw()
{
	DrawVelocityMap();
	DrawOutput();
}

void Carol::TaaPass::DrawVelocityMap()
{
	gCommandList->RSSetViewports(1, &mViewport);
	gCommandList->RSSetScissorRects(1, &mScissorRect);

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	gCommandList->ClearRenderTargetView(mVelocityMap->GetRtv(), DirectX::Colors::Black, 0, nullptr);
	gCommandList->ClearDepthStencilView(gFramePass->GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
	gCommandList->OMSetRenderTargets(1, GetRvaluePtr(mVelocityMap->GetRtv()), true, GetRvaluePtr(gFramePass->GetFrameDsv()));

	gCommandList->SetPipelineState(gPSOs[L"VelocityStatic"]->Get());
	gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_STATIC));

	gCommandList->SetPipelineState(gPSOs[L"VelocitySkinned"]->Get());
	gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_SKINNED));

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
}

void Carol::TaaPass::DrawOutput()
{
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(gDisplayPass->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	gCommandList->ClearRenderTargetView(gDisplayPass->GetCurrBackBufferRtv(), DirectX::Colors::Gray, 0, nullptr);
	gCommandList->OMSetRenderTargets(1, GetRvaluePtr(gDisplayPass->GetCurrBackBufferRtv()), true, nullptr);
	gCommandList->SetPipelineState(gPSOs[L"TaaOutput"]->Get());
	static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)));
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(gDisplayPass->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)));
	gCommandList->CopyResource(mHistFrameMap->Get(), gDisplayPass->GetCurrBackBuffer()->Get());
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(gDisplayPass->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT)));
}

void Carol::TaaPass::GetHalton(float& proj0, float& proj1)
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / mWidth;
	proj1 = (2 * mHalton[i].y - 1) / mHeight;

	i = (i + 1) % 8;
}

void Carol::TaaPass::SetHistViewProj(DirectX::XMMATRIX& histViewProj)
{
	mHistViewProj = histViewProj;
}

DirectX::XMMATRIX Carol::TaaPass::GetHistViewProj()
{
	return mHistViewProj;
}

uint32_t Carol::TaaPass::GetVeloctiySrvIdx()
{
	return mVelocityMap->GetGpuSrvIdx();
}

uint32_t Carol::TaaPass::GetHistFrameSrvIdx()
{
	return mHistFrameMap->GetGpuSrvIdx();
}

void Carol::TaaPass::InitBuffers()
{
	mHistFrameMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	
	D3D12_CLEAR_VALUE optClearValue = CD3DX12_CLEAR_VALUE(mVelocityMapFormat, DirectX::Colors::Black);
	mVelocityMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mVelocityMapFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&optClearValue);
}

void Carol::TaaPass::InitHalton()
{
	for (int i = 0; i < 8; ++i)
	{
		mHalton[i].x = RadicalInversion(2, i + 1);
		mHalton[i].y = RadicalInversion(3, i + 1);
	}
}

float Carol::TaaPass::RadicalInversion(int base, int num)
{
	int temp[4];
	int i = 0;

	while (num != 0)
	{
		temp[i++] = num % base;
		num /= base;
	}

	double convertionResult = 0;
	for (int j = 0; j < i; ++j)
	{
		convertionResult += temp[j] * pow(base, -j - 1);
	}

	return convertionResult;
}
