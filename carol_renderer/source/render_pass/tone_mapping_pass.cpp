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

Carol::ToneMappingPass::ToneMappingPass(ID3D12Device* device, DXGI_FORMAT adaptedLuminanceFormat)
	:mAdaptedLuminanceFormat(adaptedLuminanceFormat),
	mAdaptedLuminanceIdx(ADAPTED_LUMINANCE_IDX_COUNT)
{
	InitShaders();
	InitPSOs(device);
}

void Carol::ToneMappingPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	uint32_t groupWidth = ceilf(mWidth / 32.f); 
    uint32_t groupHeight = ceilf(mHeight / 32.f);

	mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmdList->SetPipelineState(gPSOs[L"LDRToneMapping"]->Get());
	cmdList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::ToneMappingPass::SetFrameMap(ColorBuffer* frameMap)
{
	mFrameMap = frameMap;
}

void Carol::ToneMappingPass::InitShaders()
{
	vector<wstring_view> nullDefines = {};

	if (gShaders.count(L"LDRToneMappingCS") == 0)
	{
		gShaders[L"LDRToneMappingCS"] = make_unique<Shader>(L"shader\\tone_mapping_cs.hlsl", nullDefines, L"main", L"cs_6_6");
	}
}

void Carol::ToneMappingPass::InitPSOs(ID3D12Device* device)
{
	if (gPSOs.count(L"LDRToneMapping") == 0)
	{
		auto ldrToneMappingComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
		ldrToneMappingComputePSO->SetRootSignature(sRootSignature.get());
		ldrToneMappingComputePSO->SetCS(gShaders[L"LDRToneMappingCS"].get());
		ldrToneMappingComputePSO->Finalize(device);

		gPSOs[L"LDRToneMapping"] = std::move(ldrToneMappingComputePSO);
	}
}

void Carol::ToneMappingPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
}
