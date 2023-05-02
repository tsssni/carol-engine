#include <carol.h>

namespace Carol
{
	using std::make_unique;
}

Carol::EpfPass::EpfPass()
{
	InitPSOs();
}

void Carol::EpfPass::SetColorMap(ColorBuffer* colorMap)
{
	mColorMap = colorMap;
}

void Carol::EpfPass::Draw()
{
	constexpr uint32_t borderRadius = 5;
	uint32_t groupWidth = ceilf(mColorMap->GetWidth() * 1.f / (32 - 2 * borderRadius)); 
    uint32_t groupHeight = ceilf(mColorMap->GetHeight() * 1.f / (32 - 2 * borderRadius));

	mColorMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gGraphicsCommandList->SetPipelineState(mEpfComputePSO->Get());
	gGraphicsCommandList->SetComputeRoot32BitConstant(ROOT_CONSTANTS, mColorMap->GetGpuUavIdx(), 0);
    gGraphicsCommandList->Dispatch(groupWidth, groupHeight , 1);
    mColorMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Carol::EpfPass::InitPSOs()
{
	mEpfComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mEpfComputePSO->SetRootSignature(sRootSignature.get());
	mEpfComputePSO->SetCS(gEpfCS.get());
	mEpfComputePSO->Finalize();
}

void Carol::EpfPass::InitBuffers()
{
}
