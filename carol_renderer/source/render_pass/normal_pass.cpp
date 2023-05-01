#include <render_pass/normal_pass.h>
#include <render_pass/display_pass.h>
#include <scene/scene.h>
#include <dx12.h>
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
    ID3D12Device* device,
    DXGI_FORMAT normalMapFormat,
    DXGI_FORMAT normalDsvFormat)
	:
    mNormalMapFormat(normalMapFormat),
    mFrameDsvFormat(normalDsvFormat),
    mIndirectCommandBuffer(MESH_TYPE_COUNT)
{
    InitPSOs(device);
}

void Carol::NormalPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->RSSetViewports(1, &mViewport);
    cmdList->RSSetScissorRects(1, &mScissorRect);

    mNormalMap->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ClearRenderTargetView(mNormalMap->GetRtv(), DirectX::Colors::Blue, 0, nullptr);
    cmdList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(1, GetRvaluePtr(mNormalMap->GetRtv()), true, GetRvaluePtr(mDepthStencilMap->GetDsv()));

    cmdList->SetPipelineState(mNormalsStaticMeshPSO->Get());
    ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_STATIC]);
    
    cmdList->SetPipelineState(mNormalsSkinnedMeshPSO->Get());
    ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_SKINNED]);

    mNormalMap->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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

void Carol::NormalPass::InitPSOs(ID3D12Device* device)
{
	mNormalsStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mNormalsStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mNormalsStaticMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mFrameDsvFormat);
	mNormalsStaticMeshPSO->SetAS(&gCullAS);
	mNormalsStaticMeshPSO->SetMS(&gNormalsStaticMS);
	mNormalsStaticMeshPSO->SetPS(&gNormalsPS);
	mNormalsStaticMeshPSO->Finalize(device);

	mNormalsSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mNormalsSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mNormalsSkinnedMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mFrameDsvFormat);
	mNormalsSkinnedMeshPSO->SetAS(&gCullAS);
	mNormalsSkinnedMeshPSO->SetMS(&gNormalsSkinnedMS);
	mNormalsSkinnedMeshPSO->SetPS(&gNormalsPS);
	mNormalsSkinnedMeshPSO->Finalize(device);
}

void Carol::NormalPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
    D3D12_CLEAR_VALUE optClearValue  = CD3DX12_CLEAR_VALUE(mNormalMapFormat, DirectX::Colors::Blue);
    mNormalMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mNormalMapFormat,
        device,
        heap,
        descriptorManager,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &optClearValue);
}
