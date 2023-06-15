#include <render_pass/ssao_pass.h>
#include <render_pass/epf_pass.h>
#include <dx12/root_signature.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <dx12/shader.h>
#include <global.h>
#include <DirectXColors.h>
#include <DirectXPackedVector.h>
#include <string_view>

#define BORDER_RADIUS 5

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::make_unique;
	using namespace DirectX;
	using namespace DirectX::PackedVector;
	using Microsoft::WRL::ComPtr;
}

Carol::SsaoPass::SsaoPass(DXGI_FORMAT ambientMapFormat)
	:mAmbientMapFormat(ambientMapFormat),
	mEpfPass(make_unique<EpfPass>())
{
	InitPSOs();
}

uint32_t Carol::SsaoPass::GetSsaoSrvIdx()const
{
	return mAmbientMap->GetGpuSrvIdx();
}

uint32_t Carol::SsaoPass::GetSsaoUavIdx()const
{
	return mAmbientMap->GetGpuUavIdx();
}

void Carol::SsaoPass::OnResize(
	uint32_t width,
	uint32_t height)
{
	if (mWidth != width >> 1 || mHeight != height >> 1)
	{
		mWidth = width >> 1;
		mHeight = height >> 1;
		mViewport = { 0.f,0.f,mWidth * 1.f,mHeight * 1.f,0.f,1.f };
		mScissorRect = { 0,0,(long)mWidth,(long)mHeight };

		InitBuffers();
	}
}

void Carol::SsaoPass::SetSampleCount(uint32_t sampleCount)
{
	mSampleCount = 14;
}

void Carol::SsaoPass::SetBlurRadius(uint32_t blurRadius)
{
	mEpfPass->SetBlurRadius(blurRadius);
}

void Carol::SsaoPass::SetBlurCount(uint32_t blurCount)
{
	mEpfPass->SetBlurCount(blurCount);
}

void Carol::SsaoPass::Draw()
{
	uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);

	mAmbientMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gGraphicsCommandList->SetPipelineState(mSsaoComputePSO->Get());
	gGraphicsCommandList->SetComputeRoot32BitConstant(ROOT_CONSTANTS, mSampleCount, 0);
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
	mAmbientMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mEpfPass->SetColorMap(mAmbientMap.get());
	mEpfPass->Draw();
}

void Carol::SsaoPass::InitBuffers()
{
	mAmbientMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mAmbientMapFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::SsaoPass::InitPSOs()
{
	mSsaoComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mSsaoComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/ssao_cs.dxil"));
	mSsaoComputePSO->Finalize();
}