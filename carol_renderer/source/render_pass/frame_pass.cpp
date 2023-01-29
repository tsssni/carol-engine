#include <render_pass/frame_pass.h>
#include <scene/scene.h>
#include <dx12.h>
#include <global.h>
#include <DirectXColors.h>
#include <string_view>
#include <span>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::unique_ptr;
    using std::make_unique;
	using std::span;
	using namespace DirectX;
}

Carol::FramePass::FramePass(
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT hiZFormat)
	:mCulledCommandBuffer(gNumFrame),
	mCullIdx(MESH_TYPE_COUNT),
	mHiZIdx(HIZ_IDX_COUNT),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat),
	mHiZFormat(hiZFormat)
{
    InitShaders();
    InitPSOs();
	
	for (int i = 0; i < gNumFrame; ++i)
	{
		mCulledCommandBuffer[i].resize(MESH_TYPE_COUNT);

		for (int j = 0; j < MESH_TYPE_COUNT; ++j)
		{
			ResizeCommandBuffer(mCulledCommandBuffer[i][j], 1024, sizeof(IndirectCommand));
		}
	}

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		mCullIdx[i].resize(CULL_IDX_COUNT);
	}
}

void Carol::FramePass::Draw()
{
	gCommandList->RSSetViewports(1, &mViewport);
	gCommandList->RSSetScissorRects(1, &mScissorRect);

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	gCommandList->ClearRenderTargetView(GetFrameRtv(), Colors::Gray, 0, nullptr);
	gCommandList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
	gCommandList->OMSetRenderTargets(1, GetRvaluePtr(GetFrameRtv()), true, GetRvaluePtr(GetFrameDsv()));

	DrawOpaque();
	DrawTransparent();
	
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
}

void Carol::FramePass::Update()
{
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);
		TestCommandBufferSize(mCulledCommandBuffer[gCurrFrame][i], gScene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[gCurrFrame][i]->GetGpuUavIdx();
		mCullIdx[i][CULL_MESH_COUNT] = gScene->GetMeshesCount(type);
		mCullIdx[i][CULL_MESH_OFFSET] = gScene->GetMeshCBStartOffet(type);
		mCullIdx[i][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
	}

	mHiZIdx[HIZ_DEPTH_IDX] = mDepthStencilMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
}

void Carol::FramePass::ReleaseIntermediateBuffers()
{
}

void Carol::FramePass::Cull()
{
	Clear();
	gCommandList->RSSetViewports(1, &mViewport);
	gCommandList->RSSetScissorRects(1, &mScissorRect);

	GenerateHiZ();
	CullInstances(true, true);
	CullMeshlets(true, true);

	GenerateHiZ();
	CullInstances(false, true);
	CullMeshlets(false, true);

	CullInstances(false, false);
	CullMeshlets(false, false);
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetFrameRtv()
{
	return mFrameMap->GetRtv();
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetFrameDsv()
{
	return mDepthStencilMap->GetDsv();
}

DXGI_FORMAT Carol::FramePass::GetFrameRtvFormat()
{
	return mFrameFormat;
}

Carol::StructuredBuffer* Carol::FramePass::GetIndirectCommandBuffer(MeshType type)
{
	return mCulledCommandBuffer[gCurrFrame][type].get();
}

DXGI_FORMAT Carol::FramePass::GetFrameDsvFormat()
{
	return GetDsvFormat(mDepthStencilFormat);
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

uint32_t Carol::FramePass::GetPpllUavIdx()
{
	return mOitppllBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetOffsetUavIdx()
{
	return mStartOffsetBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetCounterUavIdx()
{
	return mCounterBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetPpllSrvIdx()
{
	return mOitppllBuffer->GetGpuSrvIdx();
}

uint32_t Carol::FramePass::GetOffsetSrvIdx()
{
	return mStartOffsetBuffer->GetGpuSrvIdx();
}

void Carol::FramePass::InitShaders()
{
	vector<wstring_view> nullDefines = {};

	vector<wstring_view> staticDefines = 
	{
		L"TAA=1",L"SSAO=1"
	};

	vector<wstring_view> skinnedDefines =
	{
		L"TAA=1",L"SSAO=1",L"SKINNED=1"
	};

	vector<wstring_view> cullDefines =
	{
		L"OCCLUSION=1"
	};

	vector<wstring_view> cullWriteDefines =
	{
		L"OCCLUSION=1",
		L"WRITE=1"
	};

	vector<wstring_view> meshShaderDisabledCullWriteDefines =
	{
		L"OCCLUSION=1",
		L"WRITE=1",
		L"MESH_SHADER_DISABLED=1"
	};
	
	gShaders[L"OpaqueStaticMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	gShaders[L"OpaquePS"] = make_unique<Shader>(L"shader\\default_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	gShaders[L"OpaqueSkinnedMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");

	gShaders[L"ScreenMS"] = make_unique<Shader>(L"shader\\screen_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	gShaders[L"SkyBoxMS"] = make_unique<Shader>(L"shader\\skybox_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	gShaders[L"SkyBoxPS"] = make_unique<Shader>(L"shader\\skybox_ps.hlsl", staticDefines, L"main", L"ps_6_6");

	gShaders[L"HiZGenerateCS"] = make_unique<Shader>(L"shader\\hiz_generate_cs.hlsl", nullDefines, L"main", L"cs_6_6");
	gShaders[L"FrameCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", cullDefines, L"main", L"cs_6_6");
	gShaders[L"CullAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", cullDefines, L"main", L"as_6_6");
	gShaders[L"CullWriteAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", cullWriteDefines, L"main", L"as_6_6");
	gShaders[L"DepthStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	gShaders[L"DepthSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");

	gShaders[L"BuildOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	gShaders[L"DrawOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_ps.hlsl", nullDefines, L"main", L"ps_6_6");
	gShaders[L"TransparentCullWriteAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", meshShaderDisabledCullWriteDefines, L"main", L"as_6_6");
}

void Carol::FramePass::InitPSOs()
{
	auto cullStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	cullStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthStencilFormat));
	cullStaticMeshPSO->SetAS(gShaders[L"CullWriteAS"].get());
	cullStaticMeshPSO->SetMS(gShaders[L"DepthStaticMS"].get());
	cullStaticMeshPSO->Finalize();
	gPSOs[L"OpaqueStaticMeshletCull"] = std::move(cullStaticMeshPSO);

	auto cullSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	cullSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthStencilFormat));
	cullSkinnedMeshPSO->SetAS(gShaders[L"CullWriteAS"].get());
	cullSkinnedMeshPSO->SetMS(gShaders[L"DepthSkinnedMS"].get());
	cullSkinnedMeshPSO->Finalize();
	gPSOs[L"OpaqueSkinnedMeshletCull"] = std::move(cullSkinnedMeshPSO);

	auto opaqueStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	opaqueStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	opaqueStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
	opaqueStaticMeshPSO->SetMS(gShaders[L"OpaqueStaticMS"].get());
	opaqueStaticMeshPSO->SetPS(gShaders[L"OpaquePS"].get());
	opaqueStaticMeshPSO->Finalize();
	gPSOs[L"OpaqueStatic"] = std::move(opaqueStaticMeshPSO);

	auto opaqueSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	opaqueSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	opaqueSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
	opaqueSkinnedMeshPSO->SetMS(gShaders[L"OpaqueSkinnedMS"].get());
	opaqueSkinnedMeshPSO->SetPS(gShaders[L"OpaquePS"].get());
	opaqueSkinnedMeshPSO->Finalize();
	gPSOs[L"OpaqueSkinned"] = std::move(opaqueSkinnedMeshPSO);

	auto skyBoxMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	skyBoxMeshPSO->SetDepthStencilState(gDepthLessEqualState);
	skyBoxMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	skyBoxMeshPSO->SetMS(gShaders[L"SkyBoxMS"].get());
	skyBoxMeshPSO->SetPS(gShaders[L"SkyBoxPS"].get());
	skyBoxMeshPSO->Finalize();
	gPSOs[L"SkyBox"] = std::move(skyBoxMeshPSO);

	auto hiZGenerateComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	hiZGenerateComputePSO->SetCS(gShaders[L"HiZGenerateCS"].get());
	hiZGenerateComputePSO->Finalize();
	gPSOs[L"HiZGenerate"] = std::move(hiZGenerateComputePSO);

	auto cullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	cullInstanceComputePSO->SetCS(gShaders[L"FrameCullCS"].get());
	cullInstanceComputePSO->Finalize();
	gPSOs[L"FrameCullInstances"] = std::move(cullInstanceComputePSO);

	auto buildStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	buildStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	buildStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	buildStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	buildStaticOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
	buildStaticOitppllMeshPSO->SetMS(gShaders[L"OpaqueStaticMS"].get());
	buildStaticOitppllMeshPSO->SetPS(gShaders[L"BuildOitppllPS"].get());
	buildStaticOitppllMeshPSO->Finalize();
	gPSOs[L"BuildStaticOitppll"] = std::move(buildStaticOitppllMeshPSO);

	auto buildSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	buildSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	buildSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	buildSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	buildSkinnedOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
	buildSkinnedOitppllMeshPSO->SetMS(gShaders[L"OpaqueSkinnedMS"].get());
	buildSkinnedOitppllMeshPSO->SetPS(gShaders[L"BuildOitppllPS"].get());
	buildSkinnedOitppllMeshPSO->Finalize();
	gPSOs[L"BuildSkinnedOitppll"] = std::move(buildSkinnedOitppllMeshPSO);

	auto drawOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	drawOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	drawOitppllMeshPSO->SetBlendState(gAlphaBlendState);
	drawOitppllMeshPSO->SetRenderTargetFormat(mFrameFormat);
	drawOitppllMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
	drawOitppllMeshPSO->SetPS(gShaders[L"DrawOitppllPS"].get());
	drawOitppllMeshPSO->Finalize();
	gPSOs[L"DrawOitppll"] = std::move(drawOitppllMeshPSO);

	auto oitppllMeshletCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	oitppllMeshletCullMeshPSO->SetRasterizerState(gCullDisabledState);
	oitppllMeshletCullMeshPSO->SetDepthStencilState(gDepthDisabledState);
	oitppllMeshletCullMeshPSO->SetAS(gShaders[L"TransparentCullWriteAS"].get());
	oitppllMeshletCullMeshPSO->SetMS(gShaders[L"DepthStaticMS"].get());
	oitppllMeshletCullMeshPSO->Finalize();
	gPSOs[L"TransparentMeshletCull"] = std::move(oitppllMeshletCullMeshPSO);
}

void Carol::FramePass::InitBuffers()
{
	mHiZMipLevels = std::max(ceilf(log2f(mWidth)), ceilf(log2f(mHeight)));

	D3D12_CLEAR_VALUE frameOptClearValue = CD3DX12_CLEAR_VALUE(mFrameFormat, DirectX::Colors::Gray);
	mFrameMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&frameOptClearValue);

	D3D12_CLEAR_VALUE depthStencilOptClearValue = CD3DX12_CLEAR_VALUE(GetDsvFormat(mDepthStencilFormat), 1.f, 0);
	mDepthStencilMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mDepthStencilFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&depthStencilOptClearValue);

	mHiZMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mHiZMipLevels);

	mOitppllBuffer = make_unique<StructuredBuffer>(
		mWidth * mHeight * 4,
		sizeof(OitppllNode),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mStartOffsetBuffer = make_unique<RawBuffer>(
		mWidth * mHeight * sizeof(uint32_t),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	mCounterBuffer = make_unique<RawBuffer>(
		sizeof(uint32_t),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
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
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::FramePass::DrawOpaque()
{
	gCommandList->SetPipelineState(gPSOs[L"OpaqueStatic"]->Get());
	gScene->ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_STATIC));

	gCommandList->SetPipelineState(gPSOs[L"OpaqueSkinned"]->Get());
	gScene->ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_SKINNED));
	gScene->DrawSkyBox(gPSOs[L"SkyBox"]->Get());
}

void Carol::FramePass::DrawTransparent()
{
	if (!gScene->IsAnyTransparentMeshes())
	{
		return;
	}

	constexpr uint32_t initOffsetValue = UINT32_MAX;
	constexpr uint32_t initCounterValue = 0;

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mOitppllBuffer->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mStartOffsetBuffer->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));

	gCommandList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	gCommandList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	
	gCommandList->SetPipelineState(gPSOs[L"BuildStaticOitppll"]->Get());
	gScene->ExecuteIndirect(GetIndirectCommandBuffer(TRANSPARENT_STATIC));
	
	gCommandList->SetPipelineState(gPSOs[L"BuildSkinnedOitppll"]->Get());
	gScene->ExecuteIndirect(GetIndirectCommandBuffer(TRANSPARENT_SKINNED));

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mOitppllBuffer->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mStartOffsetBuffer->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));

	gCommandList->OMSetRenderTargets(1, GetRvaluePtr(GetFrameRtv()), true, nullptr);
	gCommandList->SetPipelineState(gPSOs[L"DrawOitppll"]->Get());
	static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);
}

void Carol::FramePass::Clear()
{
	gScene->ClearCullMark();

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
		mCulledCommandBuffer[gCurrFrame][i]->ResetCounter();
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
	}
}

void Carol::FramePass::CullInstances(bool hist, bool opaque)
{
	gCommandList->SetPipelineState(gPSOs[L"FrameCullInstances"]->Get());
	
	uint32_t start = opaque ? OPAQUE_MESH_START : TRANSPARENT_MESH_START;
	uint32_t end = start + (opaque ? OPAQUE_MESH_TYPE_COUNT : TRANSPARENT_MESH_TYPE_COUNT);

	for (int i = start; i < end; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}
		
		mCullIdx[i][CULL_HIST] = hist;
		uint32_t count = ceilf(mCullIdx[i][CULL_MESH_COUNT] / 32.f);

		mCullIdx[i][CULL_HIST] = hist;
		gCommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
		gCommandList->Dispatch(count, 1, 1);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mCulledCommandBuffer[gCurrFrame][i]->Get())));
	}
}

void Carol::FramePass::CullMeshlets(bool hist, bool opaque)
{
	ID3D12PipelineState* pso[] = {
		gPSOs[L"OpaqueStaticMeshletCull"]->Get(),
		gPSOs[L"OpaqueSkinnedMeshletCull"]->Get(),
		gPSOs[L"TransparentMeshletCull"]->Get(),
		gPSOs[L"TransparentMeshletCull"]->Get() };

	uint32_t start = opaque ? OPAQUE_MESH_START : TRANSPARENT_MESH_START;
	uint32_t end = start + (opaque ? OPAQUE_MESH_TYPE_COUNT : TRANSPARENT_MESH_TYPE_COUNT);

	if (opaque)
	{
		gCommandList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		gCommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(GetFrameDsv()));
	}

	for (int i = start; i < end; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}

		mCullIdx[i][CULL_HIST] = hist;

		gCommandList->SetPipelineState(pso[i]);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
		gCommandList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		gScene->ExecuteIndirect(mCulledCommandBuffer[gCurrFrame][i].get());
	}
}

void Carol::FramePass::GenerateHiZ()
{
	gCommandList->SetPipelineState(gPSOs[L"HiZGenerate"]->Get());

	for (int i = 0; i < mHiZMipLevels - 1; i += 5)
	{
		mHiZIdx[HIZ_SRC_MIP] = i;
		mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5;
		gCommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, mHiZIdx.size(), mHiZIdx.data(), 0);
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		gCommandList->Dispatch(width, height, 1);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
	}
}