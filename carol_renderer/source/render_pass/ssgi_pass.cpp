#include <render_pass/ssgi_pass.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <global.h>

namespace Carol
{
    using std::make_unique;
}

Carol::SsgiPass::SsgiPass(DXGI_FORMAT ssgiFormat, DXGI_FORMAT ssgiHiZFormat)
    :mSsgiFormat(ssgiFormat),
    mSsgiHiZFormat(ssgiHiZFormat)
{
    InitPSOs();
    
	mSceneColorConstants = make_unique<SceneColorConstants>();
	mSceneColorCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(SceneColorConstants), gHeapManager->GetUploadBuffersHeap());
}

void Carol::SsgiPass::Draw()
{
    Generate();
    DrawSsgi();
}

uint32_t Carol::SsgiPass::GetSceneColorSrvIdx()
{
    return mSceneColorMap->GetGpuSrvIdx();
}

uint32_t Carol::SsgiPass::GetSceneColorUavIdx()
{
    return mSceneColorMap->GetGpuUavIdx();
}

uint32_t Carol::SsgiPass::GetSsgiHiZSrvIdx()
{
    return mSsgiHiZMap->GetGpuSrvIdx();
}

uint32_t Carol::SsgiPass::GetSsgiHiZUavIdx()
{
    return mSsgiHiZMap->GetGpuUavIdx();
}

uint32_t Carol::SsgiPass::GetSsgiSrvIdx()
{
    return mSsgiMap->GetGpuSrvIdx();
}

uint32_t Carol::SsgiPass::GetSsgiUavIdx()
{
    return mSsgiMap->GetGpuUavIdx();
}

void Carol::SsgiPass::SetSampleCount(uint32_t sampleCount)
{
    mSampleCount = sampleCount;
}

void Carol::SsgiPass::SetNumSteps(uint32_t numSteps)
{
    mNumSteps = numSteps;
}

void Carol::SsgiPass::InitPSOs()
{ 
    mSsgiGenerateComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mSsgiGenerateComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/ssgi_generate_cs.dxil"));
	mSsgiGenerateComputePSO->Finalize();

    mSsgiComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mSsgiComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/ssgi_cs.dxil"));
	mSsgiComputePSO->Finalize();
}

void Carol::SsgiPass::InitBuffers()
{
    mSceneColorMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mSsgiFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        nullptr,
        mMipLevel);

    mSsgiHiZMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mSsgiHiZFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        nullptr,
        mMipLevel);

    mSsgiMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mSsgiFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::SsgiPass::Generate()
{
    gGraphicsCommandList->SetPipelineState(mSsgiGenerateComputePSO->Get());

	for (int i = 0; i < mMipLevel - 1; i += 5)
	{
		mSceneColorConstants->SrcMipLevel = i;
		mSceneColorConstants->NumMipLevel= i + 5 >= mMipLevel ? mMipLevel - i - 1 : 5;
		gGraphicsCommandList->SetComputeRootConstantBufferView(PASS_CB, mSceneColorCBAllocator->Allocate(mSceneColorConstants.get()));
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		gGraphicsCommandList->Dispatch(width, height, 1);
	}
}

void Carol::SsgiPass::DrawSsgi()
{
    uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);
    uint32_t ssgiConstants[] = {mSampleCount, mNumSteps};

	mSsgiMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gGraphicsCommandList->SetPipelineState(mSsgiComputePSO->Get());
    gGraphicsCommandList->SetComputeRoot32BitConstants(ROOT_CONSTANTS, _countof(ssgiConstants), ssgiConstants, 0);
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
	mSsgiMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}
