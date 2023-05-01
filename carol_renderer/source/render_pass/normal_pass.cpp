#include <global.h>
#include <DirectXColors.h>
#include <string_view>

namespace Carol
{
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using std::make_unique;
    using namespace DirectX;
}

Carol::NormalPass::NormalPass(
    DXGI_FORMAT normalMapFormat,
    DXGI_FORMAT normalDsvFormat)
	:
    mNormalMapFormat(normalMapFormat),
    mFrameDsvFormat(normalDsvFormat),
    mIndirectCommandBuffer(MESH_TYPE_COUNT)
{
    InitPSOs();
}

void Carol::NormalPass::Draw()
{
    gGraphicsCommandList->RSSetViewports(1, &mViewport);
    gGraphicsCommandList->RSSetScissorRects(1, &mScissorRect);

    mNormalMap->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    gGraphicsCommandList->ClearRenderTargetView(mNormalMap->GetRtv(), DirectX::Colors::Blue, 0, nullptr);
    gGraphicsCommandList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
    gGraphicsCommandList->OMSetRenderTargets(1, GetRvaluePtr(mNormalMap->GetRtv()), true, GetRvaluePtr(mDepthStencilMap->GetDsv()));

    gGraphicsCommandList->SetPipelineState(mNormalsStaticMeshPSO->Get());
    ExecuteIndirect(mIndirectCommandBuffer[OPAQUE_STATIC]);
    
    gGraphicsCommandList->SetPipelineState(mNormalsSkinnedMeshPSO->Get());
    ExecuteIndirect(mIndirectCommandBuffer[OPAQUE_SKINNED]);

    mNormalMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Carol::NormalPass::SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
{
    mIndirectCommandBuffer[type] = indirectCommandBuffer;
}

void Carol::NormalPass::SetDepthStencilMap(ColorBuffer* depthStencilMap)
{
    mDepthStencilMap = depthStencilMap;
}

uint32_t Carol::NormalPass::GetNormalSrvIdx()const
{
    return mNormalMap->GetGpuSrvIdx();
}

void Carol::NormalPass::InitPSOs()
{
	mNormalsStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mNormalsStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mNormalsStaticMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mFrameDsvFormat);
	mNormalsStaticMeshPSO->SetAS(&gCullAS);
	mNormalsStaticMeshPSO->SetMS(&gNormalsStaticMS);
	mNormalsStaticMeshPSO->SetPS(&gNormalsPS);
	mNormalsStaticMeshPSO->Finalize();

	mNormalsSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mNormalsSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mNormalsSkinnedMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mFrameDsvFormat);
	mNormalsSkinnedMeshPSO->SetAS(&gCullAS);
	mNormalsSkinnedMeshPSO->SetMS(&gNormalsSkinnedMS);
	mNormalsSkinnedMeshPSO->SetPS(&gNormalsPS);
	mNormalsSkinnedMeshPSO->Finalize();
}

void Carol::NormalPass::InitBuffers()
{
    D3D12_CLEAR_VALUE optClearValue  = CD3DX12_CLEAR_VALUE(mNormalMapFormat, DirectX::Colors::Blue);
    mNormalMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mNormalMapFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &optClearValue);
}
