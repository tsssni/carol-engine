#include <render_pass/epf_pass.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <global.h>

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

void Carol::EpfPass::SetBlurRadius(uint32_t blurRadius)
{
	mBlurRadius = blurRadius;
}

void Carol::EpfPass::SetBlurCount(uint32_t blurCount)
{
	mBlurCount = blurCount;
}

void Carol::EpfPass::Draw()
{
	uint32_t groupWidth = ceilf(mColorMap->GetWidth() * 1.f / (32 - 2 * mBlurRadius)); 
    uint32_t groupHeight = ceilf(mColorMap->GetHeight() * 1.f / (32 - 2 * mBlurRadius));

	uint32_t epfConstants[] = { mColorMap->GetGpuUavIdx(), mBlurRadius, mBlurCount };

	mColorMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gGraphicsCommandList->SetPipelineState(mEpfComputePSO->Get());
	gGraphicsCommandList->SetComputeRoot32BitConstants(ROOT_CONSTANTS, _countof(epfConstants), epfConstants, 0);
    gGraphicsCommandList->Dispatch(groupWidth, groupHeight , 1);
    mColorMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Carol::EpfPass::InitPSOs()
{
	mEpfComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mEpfComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/epf_cs.dxil"));
	mEpfComputePSO->Finalize();
}

void Carol::EpfPass::InitBuffers()
{
}
