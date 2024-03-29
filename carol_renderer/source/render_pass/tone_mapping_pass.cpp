#include <render_pass/tone_mapping_pass.h>
#include <dx12/pipeline_state.h>
#include <dx12/shader.h>
#include <global.h>
#include <string_view>
#include <vector>

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
	mLDRToneMappingComputePSO = std::make_unique<ComputePSO>(PSO_DEFAULT);
	mLDRToneMappingComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/tone_mapping_cs.dxil"));
	mLDRToneMappingComputePSO->Finalize();
}

void Carol::ToneMappingPass::InitBuffers()
{
}
