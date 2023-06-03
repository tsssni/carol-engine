#include <render_pass/cull_pass.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/indirect_command.h>
#include <dx12/pipeline_state.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <scene/model.h>
#include <global.h>

namespace Carol
{
	using std::vector;
	using std::make_unique;
	using std::wstring_view;
	using namespace DirectX;
}

Carol::CullPass::CullPass(
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT depthFormat,
	DXGI_FORMAT hiZFormat)
	:mCullConstants(MESH_TYPE_COUNT),
	mCullCBAddr(MESH_TYPE_COUNT),
	mCulledCommandBuffer(MESH_TYPE_COUNT),
	mDepthBias(depthBias),
	mDepthBiasClamp(depthBiasClamp),
	mSlopeScaledDepthBias(slopeScaledDepthBias),
	mDepthFormat(depthFormat),
	mHiZFormat(hiZFormat)
{
	InitPSOs();

	mCulledCommandBufferAllocator = make_unique<FrameBufferAllocator>(
		1024,
		sizeof(IndirectCommand),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mHiZConstants = make_unique<HiZConstants>();
	mHiZCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(CullConstants), gHeapManager->GetUploadBuffersHeap());

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		mCullConstants[i] = make_unique<CullConstants>();
	}
	mCullCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(CullConstants), gHeapManager->GetUploadBuffersHeap());
}

void Carol::CullPass::Draw()
{
	CullReset();
	gGraphicsCommandList->RSSetViewports(1, &mViewport);
	gGraphicsCommandList->RSSetScissorRects(1, &mScissorRect);

	CullInstances();
	CullMeshes();
	GenerateHiZ();

	OcclusionCullInstancesRecheck();
	OcclusionCullMeshesRecheck();
	GenerateHiZ();
}

void Carol::CullPass::Update(XMMATRIX viewProj, XMMATRIX histViewProj, XMVECTOR eyePos)
{
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);
		mCulledCommandBufferAllocator->DiscardBuffer(mCulledCommandBuffer[type].release(), gCpuFenceValue);
		mCulledCommandBuffer[type] = mCulledCommandBufferAllocator->RequestBuffer(gGpuFenceValue, gModelManager->GetMeshesCount(type));

		XMStoreFloat4x4(&mCullConstants[i]->ViewProj, viewProj);
		XMStoreFloat4x4(&mCullConstants[i]->HistViewProj, histViewProj);
		XMStoreFloat3(&mCullConstants[i]->EyePos, eyePos);
		mCullConstants[i]->MeshCount = gModelManager->GetMeshesCount(type);
		mCullConstants[i]->CulledCommandBufferIdx = mCulledCommandBuffer[type]->GetGpuUavIdx();
		mCullConstants[i]->CommandBufferIdx = gModelManager->GetCommandBufferIdx(type);
		mCullConstants[i]->MeshBufferIdx = gModelManager->GetMeshBufferIdx(type);
		mCullConstants[i]->InstanceFrustumCulledMarkBufferIdx = gModelManager->GetInstanceFrustumCulledMarkBufferIdx(type);
		mCullConstants[i]->InstanceOcclusionCulledMarkBufferIdx = gModelManager->GetInstanceOcclusionCulledMarkBufferIdx(type);
		mCullConstants[i]->InstanceCulledMarkBufferIdx = gModelManager->GetInstanceCulledMarkBufferIdx(type);
		mCullConstants[i]->HiZMapIdx = mHiZMap->GetGpuSrvIdx();

		mCullCBAddr[i] = mCullCBAllocator->Allocate(mCullConstants[i].get());
	}

	mHiZConstants->DepthMapIdx = mDepthMap->GetGpuSrvIdx();
	mHiZConstants->RWHiZMapIdx = mHiZMap->GetGpuUavIdx();
	mHiZConstants->HiZMapIdx = mHiZMap->GetGpuSrvIdx();
}

uint32_t Carol::CullPass::GetHiZMapSrvIdx()
{
	return mHiZMap->GetGpuSrvIdx();
}

Carol::StructuredBuffer* Carol::CullPass::GetIndirectCommandBuffer(MeshType type)
{
	return mCulledCommandBuffer[type].get();
}

void Carol::CullPass::SetDepthMap(ColorBuffer* depthMap)
{
	mDepthMap = depthMap;
}

void Carol::CullPass::InitPSOs()
{
	mOpaqueStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueStaticCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/opaque_cull_as.dxil"));
	mOpaqueStaticCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/static_cull_ms.dxil"));
	mOpaqueStaticCullMeshPSO->Finalize();

	mOpaqueSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueSkinnedCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/opaque_cull_as.dxil"));
	mOpaqueSkinnedCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/skinned_cull_ms.dxil"));
	mOpaqueSkinnedCullMeshPSO->Finalize();

	mTransparentStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentStaticCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/transparent_cull_as.dxil"));
	mTransparentStaticCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/static_cull_ms.dxil"));
	mTransparentStaticCullMeshPSO->Finalize();

	mTransparentSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentSkinnedCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/transparent_cull_as.dxil"));
	mTransparentSkinnedCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/skinned_cull_ms.dxil"));
	mTransparentSkinnedCullMeshPSO->Finalize();

	mOpaqueHistHiZStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueHistHiZStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueHistHiZStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueHistHiZStaticCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/opaque_hist_hiz_cull_as.dxil"));
	mOpaqueHistHiZStaticCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/static_cull_ms.dxil"));
	mOpaqueHistHiZStaticCullMeshPSO->Finalize();

	mOpaqueHistHiZSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueHistHiZSkinnedCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/opaque_hist_hiz_cull_as.dxil"));
	mOpaqueHistHiZSkinnedCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/skinned_cull_ms.dxil"));
	mOpaqueHistHiZSkinnedCullMeshPSO->Finalize();

	mTransparentHistHiZStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentHistHiZStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentHistHiZStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentHistHiZStaticCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/transparent_hist_hiz_cull_as.dxil"));
	mTransparentHistHiZStaticCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/static_cull_ms.dxil"));
	mTransparentHistHiZStaticCullMeshPSO->Finalize();
	
	mTransparentHistHiZSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentHistHiZSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentHistHiZSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentHistHiZSkinnedCullMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/transparent_hist_hiz_cull_as.dxil"));
	mTransparentHistHiZSkinnedCullMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/skinned_cull_ms.dxil"));
	mTransparentHistHiZSkinnedCullMeshPSO->Finalize();

	mCullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mCullInstanceComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/cull_cs.dxil"));
	mCullInstanceComputePSO->Finalize();

	mHistHiZCullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mHistHiZCullInstanceComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/hist_hiz_cull_cs.dxil"));
	mHistHiZCullInstanceComputePSO->Finalize();

	mHiZGenerateComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mHiZGenerateComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/hiz_generate_cs.dxil"));
	mHiZGenerateComputePSO->Finalize();
}

void Carol::CullPass::InitBuffers()
{
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
		mMipLevel);
}

void Carol::CullPass::CullReset()
{
	gModelManager->ClearCullMark();

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		mCulledCommandBuffer[type]->Transition(D3D12_RESOURCE_STATE_COPY_DEST);
		mCulledCommandBuffer[type]->ResetCounter();
	}
}

void Carol::CullPass::CullInstances()
{
	gGraphicsCommandList->SetPipelineState(mCullInstanceComputePSO->Get());
	
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}
		
		uint32_t count = ceilf(mCullConstants[type]->MeshCount / 32.f);

		gGraphicsCommandList->SetComputeRootConstantBufferView(PASS_CB, mCullCBAddr[i]);
		mCulledCommandBuffer[type]->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		gGraphicsCommandList->Dispatch(count, 1, 1);
		mCulledCommandBuffer[type]->UavBarrier();
	}
}

void Carol::CullPass::OcclusionCullInstancesRecheck()
{
	gGraphicsCommandList->SetPipelineState(mHistHiZCullInstanceComputePSO->Get());
	
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}
		
		uint32_t count = ceilf(mCullConstants[type]->MeshCount / 32.f);

		gGraphicsCommandList->SetComputeRootConstantBufferView(PASS_CB, mCullCBAddr[i]);
		mCulledCommandBuffer[type]->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		gGraphicsCommandList->Dispatch(count, 1, 1);
		mCulledCommandBuffer[type]->UavBarrier();
	}
}

void Carol::CullPass::CullMeshes()
{
	auto depthStencilDsv = mDepthMap->GetDsv();
	gGraphicsCommandList->ClearDepthStencilView(depthStencilDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	gGraphicsCommandList->OMSetRenderTargets(0, nullptr, true, &depthStencilDsv);

	ID3D12PipelineState* pso[] = {mOpaqueStaticCullMeshPSO->Get(), mOpaqueSkinnedCullMeshPSO->Get(), mTransparentStaticCullMeshPSO->Get(), mTransparentSkinnedCullMeshPSO->Get()};

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);
		mCulledCommandBuffer[type]->Transition(D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}

		gGraphicsCommandList->SetPipelineState(pso[i]);
		gGraphicsCommandList->SetGraphicsRootConstantBufferView(PASS_CB, mCullCBAddr[i]);
		ExecuteIndirect(mCulledCommandBuffer[type].get());
	}
}

void Carol::CullPass::OcclusionCullMeshesRecheck()
{
	ID3D12PipelineState* pso[] = {mOpaqueHistHiZStaticCullMeshPSO->Get(), mOpaqueHistHiZSkinnedCullMeshPSO->Get(), mTransparentHistHiZStaticCullMeshPSO->Get(), mTransparentHistHiZSkinnedCullMeshPSO->Get()};

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);
		mCulledCommandBuffer[type]->Transition(D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}

		gGraphicsCommandList->SetPipelineState(pso[i]);
		gGraphicsCommandList->SetGraphicsRootConstantBufferView(PASS_CB, mCullCBAddr[i]);
		ExecuteIndirect(mCulledCommandBuffer[type].get());
	}
}

void Carol::CullPass::GenerateHiZ()
{
	gGraphicsCommandList->SetPipelineState(mHiZGenerateComputePSO->Get());

	for (int i = 0; i < mMipLevel - 1; i += 5)
	{
		mHiZConstants->SrcMipLevel = i;
		mHiZConstants->NumMipLevel= i + 5 >= mMipLevel ? mMipLevel - i - 1 : 5;
		gGraphicsCommandList->SetComputeRootConstantBufferView(PASS_CB, mHiZCBAllocator->Allocate(mHiZConstants.get()));
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		gGraphicsCommandList->Dispatch(width, height, 1);
		mHiZMap->UavBarrier();
	}
}
