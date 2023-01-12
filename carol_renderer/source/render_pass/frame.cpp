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
#include <dx12/descriptor.h>
#include <dx12/shader.h>
#include <scene/scene.h>
#include <scene/camera.h>
#include <DirectXColors.h>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::unique_ptr;
    using std::make_unique;
	using namespace DirectX;
}

Carol::FramePass::FramePass(GlobalResources* globalResources, DXGI_FORMAT frameFormat, DXGI_FORMAT depthStencilFormat, DXGI_FORMAT hiZFormat)
	:RenderPass(globalResources),
	mCulledCommandBuffer(globalResources->NumFrame),
	mCullIdx(OPAQUE_MESH_TYPE_COUNT),
	mHiZIdx(HIZ_IDX_COUNT),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat),
	mHiZFormat(hiZFormat)
{
    InitShaders();
    InitPSOs();
	
	for (int i = 0; i < mGlobalResources->NumFrame; ++i)
	{
		mCulledCommandBuffer[i].resize(OPAQUE_MESH_TYPE_COUNT);

		for (int j = 0; j < OPAQUE_MESH_TYPE_COUNT; ++j)
		{
			ResizeCommandBuffer(mCulledCommandBuffer[i][j], 1024, sizeof(IndirectCommand));
		}
	}

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		mCullIdx[i].resize(CULL_IDX_COUNT);
	}
}

void Carol::FramePass::Draw()
{
	mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(GetFrameRtv(), Colors::Gray, 0, nullptr);
	mGlobalResources->CommandList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetFrameRtv()), true, GetRvaluePtr(GetFrameDsv()));
	
	ID3D12PipelineState* pso[] = { (*mGlobalResources->PSOs)[L"OpaqueStatic"].Get(), (*mGlobalResources->PSOs)[L"OpaqueSkinned"].Get() };

	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"OpaqueStatic"].Get());
	mGlobalResources->Meshes->ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_STATIC));

	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"OpaqueSkinned"].Get());
	mGlobalResources->Meshes->ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_SKINNED));
	
	mGlobalResources->Meshes->DrawSkyBox((*mGlobalResources->PSOs)[L"SkyBox"].Get());
	mGlobalResources->Oitppll->Draw();

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
}

void Carol::FramePass::Update()
{
	uint32_t currFrame = *mGlobalResources->CurrFrame;

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		TestCommandBufferSize(mCulledCommandBuffer[currFrame][i], mGlobalResources->Scene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[currFrame][i]->GetGpuUavIdx();
		mCullIdx[i][CULL_MESH_COUNT] = mGlobalResources->Scene->GetMeshesCount(type);
		mCullIdx[i][CULL_MESH_OFFSET] = mGlobalResources->Meshes->GetMeshCBStartOffet(type);
		mCullIdx[i][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
	}

	mHiZIdx[HIZ_DEPTH_IDX] = mDepthStencilMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();

	mGlobalResources->Oitppll->Update();
}

void Carol::FramePass::OnResize()
{
	static uint32_t width = 0;
	static uint32_t height = 0;

	if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
	{
		width = *mGlobalResources->ClientWidth;
		height = *mGlobalResources->ClientHeight;
		mHiZMipLevels = std::max(ceilf(log2f(width)), ceilf(log2f(height)));

		InitBuffers();
	}
}

void Carol::FramePass::ReleaseIntermediateBuffers()
{
}

void Carol::FramePass::Cull()
{
	Clear();
	mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

	DrawHiZ();
	CullMeshes(true);
	DrawDepth(true);

	DrawHiZ();
	CullMeshes(false);
	DrawDepth(true);

	mGlobalResources->Oitppll->Cull();
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetFrameRtv()
{
	return mFrameMap->GetRtv();
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetFrameDsv()
{
	return mDepthStencilMap->GetDsv();
}

Carol::StructuredBuffer* Carol::FramePass::GetIndirectCommandBuffer(MeshType type)
{
	return mCulledCommandBuffer[*mGlobalResources->CurrFrame][type - OPAQUE_MESH_START].get();
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

	vector<wstring> cullDefines =
	{
		L"OCCLUSION=1"
	};
	
	(*mGlobalResources->Shaders)[L"OpaqueStaticMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueStaticPS"] = make_unique<Shader>(L"shader\\default_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueSkinnedMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueSkinnedPS"] = make_unique<Shader>(L"shader\\default_ps.hlsl", skinnedDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"ScreenMS"] = make_unique<Shader>(L"shader\\screen_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxMS"] = make_unique<Shader>(L"shader\\skybox_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxPS"] = make_unique<Shader>(L"shader\\skybox_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"HiZGenerateCS"] = make_unique<Shader>(L"shader\\hiz_generate_cs.hlsl", nullDefines, L"main", L"cs_6_6");
	(*mGlobalResources->Shaders)[L"FrameCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", cullDefines, L"main", L"cs_6_6");
	(*mGlobalResources->Shaders)[L"CullAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", cullDefines, L"main", L"as_6_6");
	(*mGlobalResources->Shaders)[L"DepthStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"DepthSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
}

void Carol::FramePass::InitPSOs()
{
	auto depthStaticPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto depthStaticAS = (*mGlobalResources->Shaders)[L"CullAS"].get();
	auto depthStaticMS = (*mGlobalResources->Shaders)[L"DepthStaticMS"].get();
	depthStaticPsoDesc.AS = { reinterpret_cast<byte*>(depthStaticAS->GetBufferPointer()), depthStaticAS->GetBufferSize() };
	depthStaticPsoDesc.MS = { reinterpret_cast<byte*>(depthStaticMS->GetBufferPointer()), depthStaticMS->GetBufferSize() };
	auto DepthStaticPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(depthStaticPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC depthStaticStreamDesc;
    depthStaticStreamDesc.pPipelineStateSubobjectStream = &DepthStaticPsoStream;
    depthStaticStreamDesc.SizeInBytes = sizeof(DepthStaticPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&depthStaticStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"DepthStatic"].GetAddressOf())));

	auto depthSkinnedPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto depthSkinnedAS = (*mGlobalResources->Shaders)[L"CullAS"].get();
	auto depthSkinnedMS = (*mGlobalResources->Shaders)[L"DepthSkinnedMS"].get();
	depthSkinnedPsoDesc.AS = { reinterpret_cast<byte*>(depthSkinnedAS->GetBufferPointer()), depthSkinnedAS->GetBufferSize() };
	depthSkinnedPsoDesc.MS = { reinterpret_cast<byte*>(depthSkinnedMS->GetBufferPointer()), depthSkinnedMS->GetBufferSize() };
	auto DepthSkinnedPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(depthSkinnedPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC depthSkinnedStreamDesc;
    depthSkinnedStreamDesc.pPipelineStateSubobjectStream = &DepthSkinnedPsoStream;
    depthSkinnedStreamDesc.SizeInBytes = sizeof(DepthSkinnedPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&depthSkinnedStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"DepthSkinned"].GetAddressOf())));

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
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&frameOptClearValue);

	D3D12_CLEAR_VALUE depthStencilOptClearValue = CD3DX12_CLEAR_VALUE(GetDsvFormat(mDepthStencilFormat), 1.f, 0);
	mDepthStencilMap = make_unique<ColorBuffer>(
		width,
		height,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mDepthStencilFormat,
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&depthStencilOptClearValue);

	mHiZMap = make_unique<ColorBuffer>(
		width,
		height,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mHiZMipLevels);
}

void Carol::FramePass::TestCommandBufferSize(unique_ptr<StructuredBuffer>& buffer, uint32_t numElements)
{
	if (buffer->GetNumElements() < numElements)
	{
		ResizeCommandBuffer(buffer, numElements, buffer->GetElementSize());
	}
}

void Carol::FramePass::ResizeCommandBuffer(unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize)
{
	buffer = make_unique<StructuredBuffer>(
		numElements,
		elementSize,
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::FramePass::Clear()
{
	mGlobalResources->Meshes->ClearCullMark();
	uint32_t currFrame = *mGlobalResources->CurrFrame;

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
		mCulledCommandBuffer[currFrame][i]->ResetCounter(mGlobalResources->CommandList);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
	}
}

void Carol::FramePass::CullMeshes(bool hist)
{
	uint32_t currFrame = *mGlobalResources->CurrFrame;
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"FrameCull"].Get());
	
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}
		
		mCullIdx[i][CULL_HIST] = hist;
		uint32_t count = ceilf(mCullIdx[i][CULL_MESH_COUNT] / 32.f);

		mCullIdx[i][CULL_HIST] = hist;
		mGlobalResources->CommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
		mGlobalResources->CommandList->Dispatch(count, 1, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mCulledCommandBuffer[currFrame][i]->Get())));
	}
}

void Carol::FramePass::DrawDepth(bool hist)
{
	mGlobalResources->CommandList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(GetFrameDsv()));

	uint32_t currFrame = *mGlobalResources->CurrFrame;
	ID3D12PipelineState* pso[] = {(*mGlobalResources->PSOs)[L"DepthStatic"].Get(), (*mGlobalResources->PSOs)[L"DepthSkinned"].Get()};

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}

		mCullIdx[i][CULL_HIST] = hist;

		mGlobalResources->CommandList->SetPipelineState(pso[i]);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
		mGlobalResources->CommandList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		mGlobalResources->Meshes->ExecuteIndirect(mCulledCommandBuffer[currFrame][i].get());
	}
}

void Carol::FramePass::DrawHiZ()
{
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"HiZGenerate"].Get());
	mHiZMipLevels = 10;
	for (int i = 0; i < mHiZMipLevels - 1; i += 5)
	{
		mHiZIdx[HIZ_SRC_MIP] = i;
		mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5;
		mGlobalResources->CommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, mHiZIdx.size(), mHiZIdx.data(), 0);
		
		uint32_t width = ceilf(((*mGlobalResources->ClientWidth) >> i) / 32.f);
		uint32_t height = ceilf(((*mGlobalResources->ClientHeight) >> i) / 32.f);
		mGlobalResources->CommandList->Dispatch(width, height, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
	}
}