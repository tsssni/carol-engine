#include <render_pass/frame.h>
#include <render_pass/global_resources.h>
#include <render_pass/shadow.h>
#include <render_pass/ssao.h>
#include <render_pass/mesh.h>
#include <render_pass/oitppll.h>
#include <render_pass/taa.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/root_signature.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/shader.h>
#include <scene/scene.h>
#include <scene/camera.h>
#include <DirectXColors.h>

namespace Carol
{
	using std::vector;
	using std::wstring;
    using std::make_unique;
	using namespace DirectX;
}

Carol::FramePass::FramePass(GlobalResources* globalResources, DXGI_FORMAT frameFormat, DXGI_FORMAT depthStencilFormat, DXGI_FORMAT hiZFormat)
	:RenderPass(globalResources),
	mOcclusionCommandBuffer(OPAQUE_MESH_TYPE_COUNT),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat),
	mHiZFormat(hiZFormat)
{
    InitShaders();
    InitPSOs();
}

void Carol::FramePass::Draw()
{
	DrawFrame();
	DrawHiZ();
}

void Carol::FramePass::Update()
{
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		uint32_t numElements = mGlobalResources->Scene->GetMeshesCount((MeshType)(OPAQUE_MESH_START + i));

		if (numElements > 0)
		{
			uint32_t stride = sizeof(IndirectCommand);
			mOcclusionCommandBuffer[i].reset();
			mOcclusionCommandBuffer[i] = make_unique<StructuredBuffer>(
				numElements,
				stride,
				mGlobalResources->DefaultBuffersHeap,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				mGlobalResources->CbvSrvUavAllocator,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}
	}
}

void Carol::FramePass::OnResize()
{
	static uint32_t width = 0;
	static uint32_t height = 0;

	if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
	{
		width = *mGlobalResources->ClientWidth;
		height = *mGlobalResources->ClientHeight;
		mHiZMipLevels = std::max(floor(log2(width)), floor(log2(height)));

		InitBuffers();
	}
}

void Carol::FramePass::ReleaseIntermediateBuffers()
{
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetFrameRtv()
{
	return mFrameMap->GetRtv();
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetFrameDsv()
{
	return mDepthStencilMap->GetDsv();
}

uint32_t Carol::FramePass::GetFrameSrvIdx()
{
	return mFrameMap->GetGpuSrvIdx();
}

uint32_t Carol::FramePass::GetDepthStencilSrvIdx()
{
	return mDepthStencilMap->GetGpuSrvIdx();
}

uint32_t Carol::FramePass::GetHiZSrvIdx()
{
	return mHiZMap->GetGpuSrvIdx();
}

uint32_t Carol::FramePass::GetHiZUavIdx()
{
	return mHiZMap->GetGpuUavIdx();
}

void Carol::FramePass::InitShaders()
{
	vector<wstring> nullDefines = {};

	vector<wstring> staticDefines =
	{
		L"TAA=1",L"SSAO=1"
	};

	vector<wstring> skinnedDefines =
	{
		L"TAA=1",L"SSAO=1",L"SKINNED=1"
	};
	
	(*mGlobalResources->Shaders)[L"CullAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", nullDefines, L"main", L"as_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueStaticMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueStaticPS"] = make_unique<Shader>(L"shader\\default_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueSkinnedMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueSkinnedPS"] = make_unique<Shader>(L"shader\\default_ps.hlsl", skinnedDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"ScreenMS"] = make_unique<Shader>(L"shader\\screen_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxMS"] = make_unique<Shader>(L"shader\\skybox_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxPS"] = make_unique<Shader>(L"shader\\skybox_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"HiZGenerateCS"] = make_unique<Shader>(L"shader\\hiz_generate_cs.hlsl", nullDefines, L"main", L"cs_6_6");
	(*mGlobalResources->Shaders)[L"FrameCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", nullDefines, L"main", L"cs_6_6");
}

void Carol::FramePass::InitPSOs()
{
	auto opaqueStaticPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto opaqueStaticAS = (*mGlobalResources->Shaders)[L"CullAS"].get();
	auto opaqueStaticMS = (*mGlobalResources->Shaders)[L"OpaqueStaticMS"].get();
	auto opaqueStaticPS = (*mGlobalResources->Shaders)[L"OpaqueStaticPS"].get();
	opaqueStaticPsoDesc.AS = { reinterpret_cast<byte*>(opaqueStaticAS->GetBufferPointer()),opaqueStaticAS->GetBufferSize() };
	opaqueStaticPsoDesc.MS = { reinterpret_cast<byte*>(opaqueStaticMS->GetBufferPointer()),opaqueStaticMS->GetBufferSize() };
	opaqueStaticPsoDesc.PS = { reinterpret_cast<byte*>(opaqueStaticPS->GetBufferPointer()),opaqueStaticPS->GetBufferSize() };
	auto opaqueStaticPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(opaqueStaticPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC opaqueStaticStreamDesc;
    opaqueStaticStreamDesc.pPipelineStateSubobjectStream = &opaqueStaticPsoStream;
    opaqueStaticStreamDesc.SizeInBytes = sizeof(opaqueStaticPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&opaqueStaticStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"OpaqueStatic"].GetAddressOf())));

	auto opaqueSkinnedPsoDesc = opaqueStaticPsoDesc;
	auto opaqueSkinnedMS = (*mGlobalResources->Shaders)[L"OpaqueSkinnedMS"].get();
	auto opaqueSkinnedPS = (*mGlobalResources->Shaders)[L"OpaqueSkinnedPS"].get();
	opaqueSkinnedPsoDesc.MS = { reinterpret_cast<byte*>(opaqueSkinnedMS->GetBufferPointer()),opaqueSkinnedMS->GetBufferSize() };
	opaqueSkinnedPsoDesc.PS = { reinterpret_cast<byte*>(opaqueSkinnedPS->GetBufferPointer()),opaqueSkinnedPS->GetBufferSize() };
	auto opaqueSkinnedPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(opaqueSkinnedPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC opaqueSkinnedStreamDesc;
    opaqueSkinnedStreamDesc.pPipelineStateSubobjectStream = &opaqueSkinnedPsoStream;
    opaqueSkinnedStreamDesc.SizeInBytes = sizeof(opaqueSkinnedPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&opaqueSkinnedStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"OpaqueSkinned"].GetAddressOf())));

	auto skyBoxPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto skyBoxMS = (*mGlobalResources->Shaders)[L"SkyBoxMS"].get();
	auto skyBoxPS = (*mGlobalResources->Shaders)[L"SkyBoxPS"].get();
	skyBoxPsoDesc.MS = { reinterpret_cast<byte*>(skyBoxMS->GetBufferPointer()),skyBoxMS->GetBufferSize() };
	skyBoxPsoDesc.PS = { reinterpret_cast<byte*>(skyBoxPS->GetBufferPointer()),skyBoxPS->GetBufferSize() };
	skyBoxPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	auto skyBoxPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(skyBoxPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC skyBoxStreamDesc;
    skyBoxStreamDesc.pPipelineStateSubobjectStream = &skyBoxPsoStream;
    skyBoxStreamDesc.SizeInBytes = sizeof(skyBoxPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&skyBoxStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"SkyBox"].GetAddressOf())));

	auto hiZGeneratePsoDesc = *mGlobalResources->BaseComputePsoDesc;
	auto hiZGenerateCS = (*mGlobalResources->Shaders)[L"HiZGenerateCS"].get();
	hiZGeneratePsoDesc.CS = { reinterpret_cast<byte*>(hiZGenerateCS->GetBufferPointer()), hiZGenerateCS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateComputePipelineState(&hiZGeneratePsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"HiZGenerate"].GetAddressOf())));

	auto frameCullPsoDesc = *mGlobalResources->BaseComputePsoDesc;
	auto frameCullCS = (*mGlobalResources->Shaders)[L"FrameCullCS"].get();
	frameCullPsoDesc.CS = { reinterpret_cast<byte*>(frameCullCS->GetBufferPointer()), frameCullCS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateComputePipelineState(&frameCullPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"FrameCull"].GetAddressOf())));
}

void Carol::FramePass::InitBuffers()
{
	uint32_t width = *mGlobalResources->ClientWidth;
	uint32_t height = *mGlobalResources->ClientHeight;

	D3D12_CLEAR_VALUE frameOptClearValue = CD3DX12_CLEAR_VALUE(mFrameFormat, DirectX::Colors::Gray);
	mFrameMap = make_unique<ColorBuffer>(
		width,
		height,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameFormat,
		mGlobalResources->TexturesHeap,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		mGlobalResources->CbvSrvUavAllocator,
		mGlobalResources->RtvAllocator,
		nullptr,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&frameOptClearValue);

	D3D12_CLEAR_VALUE depthStencilOptClearValue = CD3DX12_CLEAR_VALUE(GetDsvFormat(mDepthStencilFormat), 1.f, 0);
	mDepthStencilMap = make_unique<ColorBuffer>(
		width,
		height,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mDepthStencilFormat,
		mGlobalResources->TexturesHeap,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		mGlobalResources->CbvSrvUavAllocator,
		nullptr,
		mGlobalResources->DsvAllocator,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&depthStencilOptClearValue);

	mHiZMap = make_unique<ColorBuffer>(
		width,
		height,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		mGlobalResources->DefaultBuffersHeap,
		D3D12_RESOURCE_STATE_COMMON,
		mGlobalResources->CbvSrvUavAllocator,
		nullptr,
		nullptr,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mHiZMipLevels);
}

void Carol::FramePass::DrawFrame()
{
	mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(GetFrameRtv(), Colors::Gray, 0, nullptr);
	mGlobalResources->CommandList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetFrameRtv()), true, GetRvaluePtr(GetFrameDsv()));
	
	mGlobalResources->Meshes->DrawMeshes({
		(*mGlobalResources->PSOs)[L"OpaqueStatic"].Get(),
		(*mGlobalResources->PSOs)[L"OpaqueSkinned"].Get(),
		nullptr,
		nullptr });
	mGlobalResources->Meshes->DrawSkyBox((*mGlobalResources->PSOs)[L"SkyBox"].Get());
	mGlobalResources->Oitppll->Draw();

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::FramePass::DrawHiZ()
{
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"HiZGenerate"].Get());

	for (int i = 0; i < mHiZMipLevels; i += 5)
	{
		uint32_t hiZConstants[7] = { 
			GetDepthStencilSrvIdx(),
			mHiZMap->GetGpuSrvIdx(),
			mHiZMap->GetGpuUavIdx(),
			i,
			i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5,
			*mGlobalResources->ClientWidth,
			*mGlobalResources->ClientHeight};
		mGlobalResources->CommandList->SetComputeRoot32BitConstants(RootSignature::PASS_CONSTANTS, _countof(hiZConstants), hiZConstants, 0);
		
		uint32_t width = ceilf(((*mGlobalResources->ClientWidth) >> i) / 32.f);
		uint32_t height = ceilf(((*mGlobalResources->ClientHeight) >> i) / 32.f);
		mGlobalResources->CommandList->Dispatch(width, height, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
	}
}