#include <carol.h>
#include <string_view>
#include <vector>

namespace Carol
{
	using std::vector;
	using std::wstring_view;
	using std::make_unique;
}

Carol::ToneMappingPass::ToneMappingPass()
{
	InitPSOs();
}

void Carol::ToneMappingPass::Draw()
{
	uint32_t groupWidth = ceilf(mWidth / 32.f); 
    uint32_t groupHeight = ceilf(mHeight / 32.f);

	gGraphicsCommandList->SetPipelineState(mLDRToneMappingComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::ToneMappingPass::InitPSOs()
{
	mLDRToneMappingComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mLDRToneMappingComputePSO->SetRootSignature(sRootSignature.get());
	mLDRToneMappingComputePSO->SetCS(gLDRToneMappingCS.get());
	mLDRToneMappingComputePSO->Finalize();
}

void Carol::ToneMappingPass::InitBuffers()
{
}
