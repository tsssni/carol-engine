#include <carol.h>

namespace Carol
{
	using std::make_unique;
}

Carol::OitppllPass::OitppllPass()
	:mIndirectCommandBuffer(TRANSPARENT_MESH_TYPE_COUNT)
{
	InitPSOs();
}

void Carol::OitppllPass::Draw()
{
	constexpr uint32_t initOffsetValue = UINT32_MAX;
	constexpr uint32_t initCounterValue = 0;

	mPpllBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mStartOffsetBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	gGraphicsCommandList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	gGraphicsCommandList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	gGraphicsCommandList->OMSetRenderTargets(0, nullptr, true, nullptr);

	gGraphicsCommandList->SetPipelineState(mBuildOitppllStaticMeshPSO->Get());
	ExecuteIndirect( mIndirectCommandBuffer[TRANSPARENT_STATIC - TRANSPARENT_MESH_START]);

	gGraphicsCommandList->SetPipelineState(mBuildOitppllSkinnedMeshPSO->Get());
	ExecuteIndirect( mIndirectCommandBuffer[TRANSPARENT_SKINNED - TRANSPARENT_MESH_START]);

	mPpllBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mStartOffsetBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);

	gGraphicsCommandList->SetPipelineState(mOitppllComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::OitppllPass::SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
{
    mIndirectCommandBuffer[type - TRANSPARENT_MESH_START] = indirectCommandBuffer;
}

uint32_t Carol::OitppllPass::GetPpllUavIdx() const
{
	return mPpllBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetStartOffsetUavIdx() const
{
	return mStartOffsetBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetCounterUavIdx() const
{
	return mCounterBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetPpllSrvIdx() const
{
	return mPpllBuffer->GetGpuSrvIdx();
}

uint32_t Carol::OitppllPass::GetStartOffsetSrvIdx() const
{
	return mStartOffsetBuffer->GetGpuSrvIdx();
}

void Carol::OitppllPass::InitPSOs()
{
	mBuildOitppllStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBuildOitppllStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mBuildOitppllStaticMeshPSO->SetRasterizerState(gCullDisabledState.get());
	mBuildOitppllStaticMeshPSO->SetDepthStencilState(gDepthDisabledState.get());
	mBuildOitppllStaticMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBuildOitppllStaticMeshPSO->SetAS(gCullAS.get());
	mBuildOitppllStaticMeshPSO->SetMS(gOitStaticMS.get());
	mBuildOitppllStaticMeshPSO->SetPS(gBuildOitppllPS.get());
	mBuildOitppllStaticMeshPSO->Finalize();

	mBuildOitppllSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBuildOitppllSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mBuildOitppllSkinnedMeshPSO->SetRasterizerState(gCullDisabledState.get());
	mBuildOitppllSkinnedMeshPSO->SetDepthStencilState(gDepthDisabledState.get());
	mBuildOitppllSkinnedMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBuildOitppllSkinnedMeshPSO->SetAS(gCullAS.get());
	mBuildOitppllSkinnedMeshPSO->SetMS(gOitSkinnedMS.get());
	mBuildOitppllSkinnedMeshPSO->SetPS(gBuildOitppllPS.get());
	mBuildOitppllSkinnedMeshPSO->Finalize();

	mOitppllComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mOitppllComputePSO->SetRootSignature(sRootSignature.get());
	mOitppllComputePSO->SetCS(gOitppllCS.get());
	mOitppllComputePSO->Finalize();
}

void Carol::OitppllPass::InitBuffers()
{
	mPpllBuffer = make_unique<StructuredBuffer>(
		mWidth * mHeight * 16,
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

