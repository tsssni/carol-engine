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

Carol::SsgiPass::SsgiPass(DXGI_FORMAT ssgiFormat)
    :mSsgiFormat(ssgiFormat)
{
    InitPSOs();
    
	mSceneColorConstants = make_unique<SceneColorConstants>();
	mSceneColorCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(SceneColorConstants), gHeapManager->GetUploadBuffersHeap());
}

void Carol::SsgiPass::Draw()
{
    GenerateSceneColor();
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

uint32_t Carol::SsgiPass::GetSsgiSrvIdx()
{
    return mSsgiMap->GetGpuSrvIdx();
}

uint32_t Carol::SsgiPass::GetSsgiUavIdx()
{
    return mSsgiMap->GetGpuUavIdx();
}

void Carol::SsgiPass::InitPSOs()
{ 
    mSceneColorComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mSceneColorComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/scene_color_generate_cs.dxil"));
	mSceneColorComputePSO->Finalize();

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

void Carol::SsgiPass::GenerateSceneColor()
{
    gGraphicsCommandList->SetPipelineState(mSceneColorComputePSO->Get());

	for (int i = 0; i < mMipLevel - 1; i += 5)
	{
		mSceneColorConstants->SrcMipLevel = i;
		mSceneColorConstants->NumMipLevel= i + 5 >= mMipLevel ? mMipLevel - i - 1 : 5;
		gGraphicsCommandList->SetComputeRootConstantBufferView(PASS_CB, mSceneColorCBAllocator->Allocate(mSceneColorConstants.get()));
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		gGraphicsCommandList->Dispatch(width, height, 1);
		mSceneColorMap->UavBarrier();
	}
}

void Carol::SsgiPass::DrawSsgi()
{
    uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);

	mSsgiMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gGraphicsCommandList->SetPipelineState(mSsgiComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
	mSsgiMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}
