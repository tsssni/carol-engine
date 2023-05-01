#include <render_pass/tone_mapping_pass.h>
#include <dx12.h>
#include <string_view>
#include <vector>

namespace Carol
{
	using std::vector;
	using std::wstring_view;
	using std::make_unique;
}

Carol::ToneMappingPass::ToneMappingPass(ID3D12Device* device)
{
	InitPSOs(device);
}

void Carol::ToneMappingPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	uint32_t groupWidth = ceilf(mWidth / 32.f); 
    uint32_t groupHeight = ceilf(mHeight / 32.f);

	mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmdList->SetPipelineState(mLDRToneMappingComputePSO->Get());
	cmdList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::ToneMappingPass::SetFrameMap(ColorBuffer* frameMap)
{
	mFrameMap = frameMap;
}

void Carol::ToneMappingPass::InitPSOs(ID3D12Device* device)
{
	mLDRToneMappingComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mLDRToneMappingComputePSO->SetRootSignature(sRootSignature.get());
	mLDRToneMappingComputePSO->SetCS(&gLDRToneMappingCS);
	mLDRToneMappingComputePSO->Finalize(device);
}

void Carol::ToneMappingPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
}
