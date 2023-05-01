#include <render_pass/cull_pass.h>
#include <dx12.h>
#include <scene.h>

namespace Carol
{
	using std::vector;
	using std::make_unique;
	using std::wstring_view;
	using namespace DirectX;
}

Carol::CullPass::CullPass(
	ID3D12Device* device,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager,
	Scene* scene,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT depthFormat,
	DXGI_FORMAT hiZFormat)
	:mScene(scene),
	mCullConstants(MESH_TYPE_COUNT),
	mCullCBAddr(MESH_TYPE_COUNT),
	mCulledCommandBuffer(MESH_TYPE_COUNT),
	mDepthBias(depthBias),
	mDepthBiasClamp(depthBiasClamp),
	mSlopeScaledDepthBias(slopeScaledDepthBias),
	mDepthFormat(depthFormat),
	mHiZFormat(hiZFormat)
{
	InitPSOs(device);

	mCulledCommandBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(IndirectCommand),
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mHiZConstants = make_unique<HiZConstants>();
	mHiZCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(CullConstants), device, uploadBuffersHeap, descriptorManager);

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		mCullConstants[i] = make_unique<CullConstants>();
	}
	mCullCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(CullConstants), device, uploadBuffersHeap, descriptorManager);
}

void Carol::CullPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	CullReset(cmdList);
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	CullInstances(cmdList);
	CullMeshes(cmdList);
	GenerateHiZ(cmdList);

	OcclusionCullInstancesRecheck(cmdList);
	OcclusionCullMeshesRecheck(cmdList);
	GenerateHiZ(cmdList);
}

void Carol::CullPass::Update(XMMATRIX viewProj, XMMATRIX histViewProj, XMVECTOR eyePos, uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);
		mCulledCommandBufferPool->DiscardBuffer(mCulledCommandBuffer[type].release(), cpuFenceValue);
		mCulledCommandBuffer[type] = mCulledCommandBufferPool->RequestBuffer(completedFenceValue, mScene->GetMeshesCount(type));

		XMStoreFloat4x4(&mCullConstants[i]->ViewProj, viewProj);
		XMStoreFloat4x4(&mCullConstants[i]->HistViewProj, histViewProj);
		XMStoreFloat3(&mCullConstants[i]->EyePos, eyePos);
		mCullConstants[i]->CulledCommandBufferIdx = mCulledCommandBuffer[type]->GetGpuUavIdx();
		mCullConstants[i]->MeshCount = mScene->GetMeshesCount(type);
		mCullConstants[i]->MeshOffset = mScene->GetMeshCBStartOffet(type);
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

void Carol::CullPass::InitPSOs(ID3D12Device* device)
{
	mOpaqueStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueStaticCullMeshPSO->SetAS(&gOpaqueCullAS);
	mOpaqueStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mOpaqueStaticCullMeshPSO->Finalize(device);
	

	mOpaqueSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueSkinnedCullMeshPSO->SetAS(&gOpaqueCullAS);
	mOpaqueSkinnedCullMeshPSO->SetMS(&gDepthSkinnedCullMS);
	mOpaqueSkinnedCullMeshPSO->Finalize(device);

	mTransparentStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentStaticCullMeshPSO->SetAS(&gTransparentCullAS);
	mTransparentStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mTransparentStaticCullMeshPSO->Finalize(device);

	mTransparentSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentSkinnedCullMeshPSO->SetAS(&gTransparentCullAS);
	mTransparentSkinnedCullMeshPSO->SetMS(&gDepthSkinnedCullMS);
	mTransparentSkinnedCullMeshPSO->Finalize(device);

	mOpaqueHistHiZStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueHistHiZStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueHistHiZStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueHistHiZStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueHistHiZStaticCullMeshPSO->SetAS(&gOpaqueHistHiZCullAS);
	mOpaqueHistHiZStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mOpaqueHistHiZStaticCullMeshPSO->Finalize(device);

	mOpaqueHistHiZSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mOpaqueHistHiZSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mOpaqueHistHiZSkinnedCullMeshPSO->SetAS(&gOpaqueHistHiZCullAS);
	mOpaqueHistHiZSkinnedCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mOpaqueHistHiZSkinnedCullMeshPSO->Finalize(device);

	mTransparentHistHiZStaticCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentHistHiZStaticCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentHistHiZStaticCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentHistHiZStaticCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentHistHiZStaticCullMeshPSO->SetAS(&gTransparentHistHiZCullAS);
	mTransparentHistHiZStaticCullMeshPSO->SetMS(&gDepthStaticCullMS);
	mTransparentHistHiZStaticCullMeshPSO->Finalize(device);
	
	mTransparentHistHiZSkinnedCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mTransparentHistHiZSkinnedCullMeshPSO->SetRootSignature(sRootSignature.get());
	mTransparentHistHiZSkinnedCullMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	mTransparentHistHiZSkinnedCullMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthFormat));
	mTransparentHistHiZSkinnedCullMeshPSO->SetAS(&gTransparentHistHiZCullAS);
	mTransparentHistHiZSkinnedCullMeshPSO->SetMS(&gDepthSkinnedCullMS);
	mTransparentHistHiZSkinnedCullMeshPSO->Finalize(device);

	mCullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mCullInstanceComputePSO->SetRootSignature(sRootSignature.get());
	mCullInstanceComputePSO->SetCS(&gCullCS);
	mCullInstanceComputePSO->Finalize(device);

	mHistHiZCullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mHistHiZCullInstanceComputePSO->SetRootSignature(sRootSignature.get());
	mHistHiZCullInstanceComputePSO->SetCS(&gHistHiZCullCS);
	mHistHiZCullInstanceComputePSO->Finalize(device);

	mHiZGenerateComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mHiZGenerateComputePSO->SetRootSignature(sRootSignature.get());
	mHiZGenerateComputePSO->SetCS(&gHiZGenerateCS);
	mHiZGenerateComputePSO->Finalize(device);
}

void Carol::CullPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
	mHiZMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mMipLevel);
}

void Carol::CullPass::CullReset(ID3D12GraphicsCommandList* cmdList)
{
	mScene->ClearCullMark(cmdList);

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);

		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
		mCulledCommandBuffer[type]->ResetCounter(cmdList);
	}
}

void Carol::CullPass::CullInstances(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(mCullInstanceComputePSO->Get());
	
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}
		
		uint32_t count = ceilf(mCullConstants[type]->MeshCount / 32.f);

		cmdList->SetComputeRootConstantBufferView(PASS_CONSTANTS, mCullCBAddr[i]);
		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->Dispatch(count, 1, 1);
		mCulledCommandBuffer[type]->UavBarrier(cmdList);
	}
}

void Carol::CullPass::OcclusionCullInstancesRecheck(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(mHistHiZCullInstanceComputePSO->Get());
	
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}
		
		uint32_t count = ceilf(mCullConstants[type]->MeshCount / 32.f);

		cmdList->SetComputeRootConstantBufferView(PASS_CONSTANTS, mCullCBAddr[i]);
		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->Dispatch(count, 1, 1);
		mCulledCommandBuffer[type]->UavBarrier(cmdList);
	}
}

void Carol::CullPass::CullMeshes(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->ClearDepthStencilView(mDepthMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mDepthMap->GetDsv()));

	ID3D12PipelineState* pso[] = {mOpaqueStaticCullMeshPSO->Get(), mOpaqueSkinnedCullMeshPSO->Get(), mTransparentStaticCullMeshPSO->Get(), mTransparentSkinnedCullMeshPSO->Get()};

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}

		cmdList->SetPipelineState(pso[i]);
		cmdList->SetGraphicsRootConstantBufferView(PASS_CONSTANTS, mCullCBAddr[i]);
		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		ExecuteIndirect(cmdList, mCulledCommandBuffer[type].get());
	}
}

void Carol::CullPass::OcclusionCullMeshesRecheck(ID3D12GraphicsCommandList* cmdList)
{
	ID3D12PipelineState* pso[] = {mOpaqueHistHiZStaticCullMeshPSO->Get(), mOpaqueHistHiZSkinnedCullMeshPSO->Get(), mTransparentHistHiZStaticCullMeshPSO->Get(), mTransparentHistHiZSkinnedCullMeshPSO->Get()};

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		if (mCullConstants[type]->MeshCount == 0)
		{
			continue;
		}

		cmdList->SetPipelineState(pso[i]);
		cmdList->SetGraphicsRootConstantBufferView(PASS_CONSTANTS, mCullCBAddr[i]);
		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		ExecuteIndirect(cmdList, mCulledCommandBuffer[type].get());
	}
}

void Carol::CullPass::GenerateHiZ(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(mHiZGenerateComputePSO->Get());

	for (int i = 0; i < mMipLevel - 1; i += 5)
	{
		mHiZConstants->SrcMipLevel = i;
		mHiZConstants->NumMipLevel= i + 5 >= mMipLevel ? mMipLevel - i - 1 : 5;
		cmdList->SetComputeRootConstantBufferView(PASS_CONSTANTS, mHiZCBAllocator->Allocate(mHiZConstants.get()));
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		cmdList->Dispatch(width, height, 1);
		mHiZMap->UavBarrier(cmdList);
	}
}
