#include <render_pass/taa.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/frame.h>
#include <render_pass/shadow.h>
#include <render_pass/oitppll.h>
#include <render_pass/ssao.h>
#include <render_pass/mesh.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <cmath>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::make_unique;
}

Carol::TaaPass::TaaPass(
	GlobalResources* globalResources,
	DXGI_FORMAT velocityMapFormat,
	DXGI_FORMAT frameFormat)
	:RenderPass(globalResources),
	 mVelocityMapFormat(velocityMapFormat),
	 mFrameFormat(frameFormat)
{
	InitHalton();
	InitShaders();
	InitPSOs();
}

void Carol::TaaPass::Update()
{
}

void Carol::TaaPass::OnResize()
{
	static uint32_t width = 0;
	static uint32_t height = 0;

	if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
	{
		width = *mGlobalResources->ClientWidth;
		height = *mGlobalResources->ClientHeight;

		InitBuffers();
	}
}

void Carol::TaaPass::ReleaseIntermediateBuffers()
{
}

void Carol::TaaPass::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> skinnedDefines =
	{
		L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"TaaVelocityStaticMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"TaaVelocityStaticPS"] = make_unique<Shader>(L"shader\\velocity_ps.hlsl", nullDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"TaaVelocitySkinnedMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"TaaVelocitySkinnedPS"] = make_unique<Shader>(L"shader\\velocity_ps.hlsl", skinnedDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"TaaOutputPS"] = make_unique<Shader>(L"shader\\taa_ps.hlsl", nullDefines, L"main", L"ps_6_6");
}

void Carol::TaaPass::InitPSOs()
{
	auto velocityStaticPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto velocityStaticAS = (*mGlobalResources->Shaders)[L"CullAS"].get();
	auto velocityStaticMS = (*mGlobalResources->Shaders)[L"TaaVelocityStaticMS"].get();
	auto velocityStaticPS = (*mGlobalResources->Shaders)[L"TaaVelocityStaticPS"].get();
	velocityStaticPsoDesc.AS = { reinterpret_cast<byte*>(velocityStaticAS->GetBufferPointer()),velocityStaticAS->GetBufferSize() };
	velocityStaticPsoDesc.MS = { reinterpret_cast<byte*>(velocityStaticMS->GetBufferPointer()),velocityStaticMS->GetBufferSize() };
	velocityStaticPsoDesc.PS = { reinterpret_cast<byte*>(velocityStaticPS->GetBufferPointer()),velocityStaticPS->GetBufferSize() };
	velocityStaticPsoDesc.RTVFormats[0] = mVelocityMapFormat;
	auto velocityStaticPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(velocityStaticPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC velocityStaticStreamDesc;
    velocityStaticStreamDesc.pPipelineStateSubobjectStream = &velocityStaticPsoStream;
    velocityStaticStreamDesc.SizeInBytes = sizeof(velocityStaticPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&velocityStaticStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"VelocityStatic"].GetAddressOf())));

	auto velocitySkinnedPsoDesc = velocityStaticPsoDesc;
	auto velocitySkinnedMS = (*mGlobalResources->Shaders)[L"TaaVelocitySkinnedMS"].get();
	auto velocitySkinnedPS = (*mGlobalResources->Shaders)[L"TaaVelocitySkinnedPS"].get();
	velocitySkinnedPsoDesc.MS = { reinterpret_cast<byte*>(velocitySkinnedMS->GetBufferPointer()),velocitySkinnedMS->GetBufferSize() };
	velocitySkinnedPsoDesc.PS = { reinterpret_cast<byte*>(velocitySkinnedPS->GetBufferPointer()),velocitySkinnedPS->GetBufferSize() };
	auto velocitySkinnedPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(velocitySkinnedPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC velocitySkinnedStreamDesc;
    velocitySkinnedStreamDesc.pPipelineStateSubobjectStream = &velocitySkinnedPsoStream;
    velocitySkinnedStreamDesc.SizeInBytes = sizeof(velocitySkinnedPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&velocitySkinnedStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"VelocitySkinned"].GetAddressOf())));

	auto outputPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto outputMS = (*mGlobalResources->Shaders)[L"ScreenMS"].get();
	auto outputPS = (*mGlobalResources->Shaders)[L"TaaOutputPS"].get();
	outputPsoDesc.MS = { reinterpret_cast<byte*>(outputMS->GetBufferPointer()),outputMS->GetBufferSize() };
	outputPsoDesc.PS = { reinterpret_cast<byte*>(outputPS->GetBufferPointer()),outputPS->GetBufferSize() };
	outputPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	outputPsoDesc.DepthStencilState.DepthEnable = false;
	auto outputPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(outputPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC outputStreamDesc;
    outputStreamDesc.pPipelineStateSubobjectStream = &outputPsoStream;
    outputStreamDesc.SizeInBytes = sizeof(outputPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&outputStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"TaaOutput"].GetAddressOf())));
}

void Carol::TaaPass::Draw()
{
	DrawVelocityMap();
	DrawOutput();
}

void Carol::TaaPass::DrawVelocityMap()
{
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(mVelocityMap->GetRtv(), DirectX::Colors::Black, 0, nullptr);
	mGlobalResources->CommandList->ClearDepthStencilView(mGlobalResources->Frame->GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mVelocityMap->GetRtv()), true, GetRvaluePtr(mGlobalResources->Frame->GetFrameDsv()));

	mGlobalResources->Meshes->DrawMeshes({
		(*mGlobalResources->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocitySkinned"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocitySkinned"].Get()
		});
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::TaaPass::DrawOutput()
{
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mGlobalResources->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(mGlobalResources->Display->GetCurrBackBufferRtv(), DirectX::Colors::Gray, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mGlobalResources->Display->GetCurrBackBufferRtv()), true, nullptr);
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"TaaOutput"].Get());
	mGlobalResources->CommandList->DispatchMesh(1, 1, 1);

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST)));
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mGlobalResources->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)));
	mGlobalResources->CommandList->CopyResource(mHistFrameMap->Get(), mGlobalResources->Display->GetCurrBackBuffer()->Get());
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)));
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mGlobalResources->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT)));
}

void Carol::TaaPass::GetHalton(float& proj0, float& proj1)
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / (*mGlobalResources->ClientWidth);
	proj1 = (2 * mHalton[i].y - 1) / (*mGlobalResources->ClientHeight);

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
	uint32_t width = *mGlobalResources->ClientWidth;
	uint32_t height = *mGlobalResources->ClientHeight;

	mHistFrameMap = make_unique<ColorBuffer>(
		width,
		height,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameFormat,
		mGlobalResources->DefaultBuffersHeap,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		mGlobalResources->CbvSrvUavAllocator);
	
	D3D12_CLEAR_VALUE optClearValue = CD3DX12_CLEAR_VALUE(mVelocityMapFormat, DirectX::Colors::Black);
	mVelocityMap = make_unique<ColorBuffer>(
		width,
		height,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mVelocityMapFormat,
		mGlobalResources->DefaultBuffersHeap,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		mGlobalResources->CbvSrvUavAllocator,
		mGlobalResources->RtvAllocator,
		nullptr,
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
