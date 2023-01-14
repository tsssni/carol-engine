#include <render_pass/oitppll.h>
#include <global.h>
#include <render_pass/display.h>
#include <render_pass/frame.h>
#include <render_pass/taa.h>
#include <render_pass/scene.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <dx12/pipeline_state.h>
#include <dx12/indirect_command.h>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::make_unique;
	using Microsoft::WRL::ComPtr;
}

Carol::OitppllPass::OitppllPass(DXGI_FORMAT outputFormat)
	:mOutputFormat(outputFormat),
	mCulledCommandBuffer(gNumFrame),
	mCullIdx(TRANSPARENT_MESH_TYPE_COUNT)
{
	InitShaders();
	InitPSOs();

	for (int i = 0; i < gNumFrame; ++i)
	{
		mCulledCommandBuffer[i].resize(TRANSPARENT_MESH_TYPE_COUNT);

		for (int j = 0; j < TRANSPARENT_MESH_TYPE_COUNT; ++j)
		{
			ResizeCommandBuffer(mCulledCommandBuffer[i][j], 1024, sizeof(IndirectCommand));
		}
	}

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		mCullIdx[i].resize(CULL_IDX_COUNT);
	}
}

void Carol::OitppllPass::Cull()
{
	if (gScene->IsAnyTransparentMeshes())
	{
		gCommandList->SetPipelineState(gPSOs[L"OitppllInstanceCull"]->Get());

		for (int i = 0; i < TRANSPARENT_MESH_TYPE_COUNT; ++i)
		{
			if (mCullIdx[i][CULL_MESH_COUNT] == 0)
			{
				continue;
			}

			gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
			mCulledCommandBuffer[gCurrFrame][i]->ResetCounter();
			gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
		
			uint32_t count = ceilf(mCullIdx[i][CULL_MESH_COUNT] / 32.f);

			gCommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
			gCommandList->Dispatch(count, 1, 1);
			gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
		}

		gCommandList->SetPipelineState(gPSOs[L"OitppllMeshletCull"]->Get());
		gCommandList->RSSetViewports(1, &mViewport);
		gCommandList->RSSetScissorRects(1, &mScissorRect);

		for (int i = 0; i < TRANSPARENT_MESH_TYPE_COUNT; ++i)
		{
			gScene->ExecuteIndirect(mCulledCommandBuffer[gCurrFrame][i].get());
		}
	}
}

void Carol::OitppllPass::Draw()
{
	if (gScene->IsAnyTransparentMeshes())
	{
		DrawPpll();
		DrawOit();
	}
}

void Carol::OitppllPass::Update()
{
	for (int i = 0; i < TRANSPARENT_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(TRANSPARENT_MESH_START + i);
		TestCommandBufferSize(mCulledCommandBuffer[gCurrFrame][i], gScene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[gCurrFrame][i]->GetGpuUavIdx();
		mCullIdx[i][CULL_MESH_COUNT] = gScene->GetMeshesCount(type);
		mCullIdx[i][CULL_MESH_OFFSET] = gScene->GetMeshCBStartOffet(type);
		mCullIdx[i][CULL_HIST] = 0;
	}
}

void Carol::OitppllPass::ReleaseIntermediateBuffers()
{
}

uint32_t Carol::OitppllPass::GetPpllUavIdx()
{
	return mOitppllBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetOffsetUavIdx()
{
	return mStartOffsetBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetCounterUavIdx()
{
	return mCounterBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetPpllSrvIdx()
{
	return mOitppllBuffer->GetGpuSrvIdx();
}

uint32_t Carol::OitppllPass::GetOffsetSrvIdx()
{
	return mStartOffsetBuffer->GetGpuSrvIdx();
}

void Carol::OitppllPass::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> staticDefines =
	{
		L"TAA=1",
		L"SSAO=1"
	};

	vector<wstring> cullDefines =
	{
		L"FRUSTUM_ONLY=1"
	};

	vector<wstring> cullWriteDefines =
	{
		L"WRITE",
		L"TRANSPARENT"
	};

	gShaders[L"BuildOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	gShaders[L"DrawOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_ps.hlsl", nullDefines, L"main", L"ps_6_6");
	gShaders[L"TransparentCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", nullDefines, L"main", L"cs_6_6");
	gShaders[L"TransparentAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", cullDefines, L"main", L"as_6_6");
	gShaders[L"TransparentCullAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", cullWriteDefines, L"main", L"as_6_6");
}

void Carol::OitppllPass::InitPSOs()
{
	auto buildStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	buildStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	buildStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	buildStaticOitppllMeshPSO->SetAS(gShaders[L"TransparentAS"].get());
	buildStaticOitppllMeshPSO->SetMS(gShaders[L"OpaqueStaticMS"].get());
	buildStaticOitppllMeshPSO->SetPS(gShaders[L"BuildOitppllPS"].get());
	buildStaticOitppllMeshPSO->Finalize();
	gPSOs[L"BuildStaticOitppll"] = std::move(buildStaticOitppllMeshPSO);

	auto buildSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	buildSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	buildSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	buildSkinnedOitppllMeshPSO->SetAS(gShaders[L"TransparentAS"].get());
	buildSkinnedOitppllMeshPSO->SetMS(gShaders[L"OpaqueSkinnedMS"].get());
	buildSkinnedOitppllMeshPSO->SetPS(gShaders[L"BuildOitppllPS"].get());
	buildSkinnedOitppllMeshPSO->Finalize();
	gPSOs[L"BuildSkinnedOitppll"] = std::move(buildSkinnedOitppllMeshPSO);

	auto drawOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	drawOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	drawOitppllMeshPSO->SetBlendState(gAlphaBlendState);
	drawOitppllMeshPSO->SetRenderTargetFormat(gFramePass->GetFrameRtvFormat());
	drawOitppllMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
	drawOitppllMeshPSO->SetPS(gShaders[L"DrawOitppllPS"].get());
	drawOitppllMeshPSO->Finalize();
	gPSOs[L"DrawOitppll"] = std::move(drawOitppllMeshPSO);

	auto oitppllMeshletCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	oitppllMeshletCullMeshPSO->SetRasterizerState(gCullDisabledState);
	oitppllMeshletCullMeshPSO->SetDepthStencilState(gDepthDisabledState);
	oitppllMeshletCullMeshPSO->SetAS(gShaders[L"TransparentCullAS"].get());
	oitppllMeshletCullMeshPSO->SetMS(gShaders[L"DepthStaticMS"].get());
	oitppllMeshletCullMeshPSO->Finalize();
	gPSOs[L"OitppllMeshletCull"] = std::move(oitppllMeshletCullMeshPSO);

	auto oitppllInstanceCullComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	oitppllInstanceCullComputePSO->SetCS(gShaders[L"TransparentCS"].get());
	oitppllInstanceCullComputePSO->Finalize();
	gPSOs[L"OitppllInstanceCull"] = std::move(oitppllInstanceCullComputePSO);
}

void Carol::OitppllPass::InitBuffers()
{
	mOitppllBuffer = make_unique<StructuredBuffer>(
		mWidth * mHeight * 2,
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

void Carol::OitppllPass::TestCommandBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements)
{
	if (buffer->GetNumElements() < numElements)
	{
		ResizeCommandBuffer(buffer, numElements, buffer->GetElementSize());
	}
}

void Carol::OitppllPass::ResizeCommandBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize)
{
	buffer = make_unique<StructuredBuffer>(
		numElements,
		elementSize,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::OitppllPass::DrawPpll()
{
	static const uint32_t initOffsetValue = UINT32_MAX;
	static const uint32_t initCounterValue = 0;

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mOitppllBuffer->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCounterBuffer->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));

	gCommandList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	gCommandList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	
	gCommandList->SetPipelineState(gPSOs[L"BuildStaticOitppll"]->Get());
	gScene->ExecuteIndirect(mCulledCommandBuffer[gCurrFrame][TRANSPARENT_STATIC - TRANSPARENT_MESH_START].get());
	
	gCommandList->SetPipelineState(gPSOs[L"BuildSkinnedOitppll"]->Get());
	gScene->ExecuteIndirect(mCulledCommandBuffer[gCurrFrame][TRANSPARENT_SKINNED - TRANSPARENT_MESH_START].get());
}

void Carol::OitppllPass::DrawOit()
{
	ComPtr<ID3D12GraphicsCommandList6> cmdList = static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get());

	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mOitppllBuffer->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCounterBuffer->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
	
	gCommandList->RSSetViewports(1, &mViewport);
	gCommandList->RSSetScissorRects(1, &mScissorRect);

	gCommandList->OMSetRenderTargets(1, GetRvaluePtr(gFramePass->GetFrameRtv()), true, nullptr);
	gCommandList->SetPipelineState(gPSOs[L"DrawOitppll"]->Get());
	static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);
}
