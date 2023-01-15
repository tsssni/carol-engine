#include <render_pass/frame.h>
#include <render_pass/scene.h>
#include <render_pass/oitppll.h>
#include <dx12.h>
#include <global.h>
#include <DirectXColors.h>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::unique_ptr;
    using std::make_unique;
	using namespace DirectX;
}

Carol::FramePass::FramePass(
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT hiZFormat)
	:mCulledCommandBuffer(gNumFrame),
	mCullIdx(OPAQUE_MESH_TYPE_COUNT),
	mHiZIdx(HIZ_IDX_COUNT),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat),
	mHiZFormat(hiZFormat)
{
    InitShaders();
    InitPSOs();
	
	for (int i = 0; i < gNumFrame; ++i)
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

void Carol::FramePass::Draw(OitppllPass* oitppllPass)
{
	gCommandList->RSSetViewports(1, &mViewport);
	gCommandList->RSSetScissorRects(1, &mScissorRect);

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	gCommandList->ClearRenderTargetView(GetFrameRtv(), Colors::Gray, 0, nullptr);
	gCommandList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
	gCommandList->OMSetRenderTargets(1, GetRvaluePtr(GetFrameRtv()), true, GetRvaluePtr(GetFrameDsv()));
	
	gCommandList->SetPipelineState(gPSOs[L"OpaqueStatic"]->Get());
	gScene->ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_STATIC));

	gCommandList->SetPipelineState(gPSOs[L"OpaqueSkinned"]->Get());
	gScene->ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_SKINNED));
	
	gScene->DrawSkyBox(gPSOs[L"SkyBox"]->Get());
	oitppllPass->Draw();

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
}

void Carol::FramePass::Update()
{
	uint32_t currFrame = gCurrFrame;

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		TestCommandBufferSize(mCulledCommandBuffer[currFrame][i], gScene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[currFrame][i]->GetGpuUavIdx();
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

	DrawHiZ();
	CullMeshes(true);
	DrawDepth(true);

	DrawHiZ();
	CullMeshes(false);
	DrawDepth(false);
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
	return mCulledCommandBuffer[gCurrFrame][type - OPAQUE_MESH_START].get();
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

void Carol::FramePass::Draw()
{
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

	vector<wstring> cullWriteDefines =
	{
		L"OCCLUSION=1",
		L"WRITE=1"
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
}

void Carol::FramePass::InitPSOs()
{
	auto cullStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	cullStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthStencilFormat));
	cullStaticMeshPSO->SetAS(gShaders[L"CullWriteAS"].get());
	cullStaticMeshPSO->SetMS(gShaders[L"DepthStaticMS"].get());
	cullStaticMeshPSO->Finalize();
	gPSOs[L"FrameCullStaticMeshes"] = std::move(cullStaticMeshPSO);

	auto cullSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	cullSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthStencilFormat));
	cullSkinnedMeshPSO->SetAS(gShaders[L"CullWriteAS"].get());
	cullSkinnedMeshPSO->SetMS(gShaders[L"DepthSkinnedMS"].get());
	cullSkinnedMeshPSO->Finalize();
	gPSOs[L"FrameCullSkinnedMeshes"] = std::move(cullSkinnedMeshPSO);

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

void Carol::FramePass::Clear()
{
	gScene->ClearCullMark();

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
		mCulledCommandBuffer[gCurrFrame][i]->ResetCounter();
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
	}
}

void Carol::FramePass::CullMeshes(bool hist)
{
	gCommandList->SetPipelineState(gPSOs[L"FrameCullInstances"]->Get());
	
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
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

void Carol::FramePass::DrawDepth(bool hist)
{
	gCommandList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	gCommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(GetFrameDsv()));
	ID3D12PipelineState* pso[] = {gPSOs[L"FrameCullStaticMeshes"]->Get(), gPSOs[L"FrameCullSkinnedMeshes"]->Get()};

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
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

void Carol::FramePass::DrawHiZ()
{
	gCommandList->SetPipelineState(gPSOs[L"HiZGenerate"]->Get());
	mHiZMipLevels = 10;
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