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

	mCulledCommandBufferPool = make_unique<StructuredBufferPool>(
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
		mCulledCommandBufferPool->DiscardBuffer(mCulledCommandBuffer[type].release(), gCpuFenceValue);
		mCulledCommandBuffer[type] = mCulledCommandBufferPool->RequestBuffer(gGpuFenceValue, gScene->GetMeshesCount(type));

		XMStoreFloat4x4(&mCullConstants[i]->ViewProj, viewProj);
		XMStoreFloat4x4(&mCullConstants[i]->HistViewProj, histViewProj);
		XMStoreFloat3(&mCullConstants[i]->EyePos, eyePos);
		mCullConstants[i]->CulledCommandBufferIdx = mCulledCommandBuffer[type]->GetGpuUavIdx();
		mCullConstants[i]->MeshCount = gScene->GetMeshesCount(type);
		mCullConstants[i]->MeshOffset = gScene->GetMeshCBStartOffet(type);
		mCullConstants[i]->HiZMapIdx = mHiZMap->GetGpuSrvIdx();

		mCullCBAddr[i] = mCullCBAllocator->Allocate(mCullConstants[i].get());
	}

	mHiZConstants->DepthMapIdx = mDepthMap->GetGpuSrvIdx();
	mHiZConstants->RWHiZMapIdx = mHiZMap->GetGpuUavIdx();
	mHiZConstants->HiZMapIdx = mHiZMap->GetGpuSrvIdx();
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
	mOpaqueStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueStaticCullMeshPSO->SetAS(&gOpaqueCullAS);
	mOpaqueStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mOpaqueStaticCullMeshPSO->Finalize();
	

	mOpaqueSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueSkinnedCullMeshPSO->SetAS(&gOpaqueCullAS);
	mOpaqueSkinnedCullMeshPSO->SetMS(&gDepthSkinnedCullMS);
	mOpaqueSkinnedCullMeshPSO->Finalize();

	mTransparentStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentStaticCullMeshPSO->SetAS(&gTransparentCullAS);
	mTransparentStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mTransparentStaticCullMeshPSO->Finalize();

	mTransparentSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentSkinnedCullMeshPSO->SetAS(&gTransparentCullAS);
	mTransparentSkinnedCullMeshPSO->SetMS(&gDepthSkinnedCullMS);
	mTransparentSkinnedCullMeshPSO->Finalize();

	mOpaqueHistHiZStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueHistHiZStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueHistHiZStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueHistHiZStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueHistHiZStaticCullMeshPSO->SetAS(&gOpaqueHistHiZCullAS);
	mOpaqueHistHiZStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mOpaqueHistHiZStaticCullMeshPSO->Finalize();

	mOpaqueHistHiZSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueHistHiZSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueHistHiZSkinnedCullMeshPSO->SetAS(&gOpaqueHistHiZCullAS);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mOpaqueHistHiZSkinnedCullMeshPSO->Finalize();

	mTransparentHistHiZStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentHistHiZStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentHistHiZStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentHistHiZStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentHistHiZStaticCullMeshPSO->SetAS(&gTransparentHistHiZCullAS);
	mTransparentHistHiZStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mTransparentHistHiZStaticCullMeshPSO->Finalize();
	
	mTransparentHistHiZSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentHistHiZSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentHistHiZSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentHistHiZSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentHistHiZSkinnedCullMeshPSO->SetAS(&gTransparentHistHiZCullAS);
	mTransparentHistHiZSkinnedCullMeshPSO->SetMS(&gDepthSkinnedCullMS);
	mTransparentHistHiZSkinnedCullMeshPSO->Finalize();

	mCullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mCullInstanceComputePSO->SetRootSignature(sRootSignature.get());
	mCullInstanceComputePSO->SetCS(&gCullCS);
	mCullInstanceComputePSO->Finalize();

	mHistHiZCullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mHistHiZCullInstanceComputePSO->SetRootSignature(sRootSignature.get());
	mHistHiZCullInstanceComputePSO->SetCS(&gHistHiZCullCS);
	mHistHiZCullInstanceComputePSO->Finalize();

	mHiZGenerateComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mHiZGenerateComputePSO->SetRootSignature(sRootSignature.get());
	mHiZGenerateComputePSO->SetCS(&gHiZGenerateCS);
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
	gScene->ClearCullMark();

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);

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
	gGraphicsCommandList->ClearDepthStencilView(mDepthMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	gGraphicsCommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mDepthMap->GetDsv()));

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
