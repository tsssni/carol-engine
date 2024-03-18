#include <render_pass/taa_pass.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <dx12/shader.h>
#include <global.h>
#include <DirectXColors.h>
#include <cmath>
#include <string_view>

#define BORDER_RADIUS 1

Carol::TaaPass::TaaPass(
	DXGI_FORMAT frameMapFormat)
	:mFrameMapFormat(frameMapFormat)
{
	InitHalton();
	InitPSOs();
}

void Carol::TaaPass::InitPSOs()
{
	mTaaComputePSO = std::make_unique<ComputePSO>(PSO_DEFAULT);
	mTaaComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/taa_cs.dxil"));
	mTaaComputePSO->Finalize();
}

void Carol::TaaPass::Draw()
{
	uint32_t groupWidth = ceilf(mWidth * 1.f / (32 - 2 * BORDER_RADIUS)); 
    uint32_t groupHeight = ceilf(mHeight * 1.f / (32 - 2 * BORDER_RADIUS));

	gGraphicsCommandList->SetPipelineState(mTaaComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::TaaPass::GetHalton(float& proj0, float& proj1)const
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / mWidth;
	proj1 = (2 * mHalton[i].y - 1) / mHeight;

	i = (i + 1) % 8;
}

void Carol::TaaPass::InitBuffers()
{
}

void Carol::TaaPass::InitHalton()
{
	for (int i = 0; i < 8; ++i)
	{
		mHalton[i].x = RadicalInversion(2, i + 1);
		mHalton[i].y = RadicalInversion(3, i + 1);
	}
}

float Carol::TaaPass::RadicalInversion(int base, int num)
{
	int temp[4];
	int i = 0;

	while (num != 0)
	{
		temp[i++] = num % base;
		num /= base;
	}

	double convertionResult = 0;
	for (int j = 0; j < i; ++j)
	{
		convertionResult += temp[j] * pow(base, -j - 1);
	}

	return convertionResult;
}
