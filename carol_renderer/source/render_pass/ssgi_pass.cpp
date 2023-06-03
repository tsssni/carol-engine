#include <render_pass/ssgi_pass.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
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
}

void Carol::SsgiPass::Draw()
{
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
    mSsgiComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mSsgiComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/ssgi_cs.dxil"));
	mSsgiComputePSO->Finalize();
}

void Carol::SsgiPass::InitBuffers()
{
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
