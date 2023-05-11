#include <render_pass/geometry_pass.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/pipeline_state.h>
#include <global.h>

namespace Carol
{
    using std::make_unique;
}

Carol::GeometryPass::GeometryPass(
    DXGI_FORMAT diffuseRoughnessFormat,
    DXGI_FORMAT emissiveMetallicFormat,
    DXGI_FORMAT normalFormat,
    DXGI_FORMAT velocityFormat,
    DXGI_FORMAT depthStencilFormat)
    :mDiffuseRoughnessFormat(diffuseRoughnessFormat),
    mEmissiveMetallicFormat(emissiveMetallicFormat),
    mNormalFormat(normalFormat),
    mVelocityFormat(velocityFormat),
    mDepthStencilFormat(depthStencilFormat),
    mIndirectCommandBuffer(OPAQUE_MESH_TYPE_COUNT)
{
    InitPSOs();
}

void Carol::GeometryPass::Draw()
{
    gGraphicsCommandList->RSSetScissorRects(1, &mScissorRect);
    gGraphicsCommandList->RSSetViewports(1, &mViewport);

    D3D12_CPU_DESCRIPTOR_HANDLE gbufferRtvs[4] =
    {
        mDiffuseRoughnessMap->GetRtv(),
        mEmissiveMetallicMap->GetRtv(),
        mNormalMap->GetRtv(),
        mVelocityMap->GetRtv()
    };

    mDiffuseRoughnessMap->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    mEmissiveMetallicMap->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    mNormalMap->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    mVelocityMap->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    float diffuseRoughnessColor[4] = {0.f,0.f,0.f,1.f};
    float emissiveMetallicColor[4] = {0.f,0.f,0.f,0.f};
    float normalColor[4] = { 0.f,0.f,0.f,1000.f };
    float velocityColor[4] = { 0.f,0.f,0.f,0.f };

    gGraphicsCommandList->ClearRenderTargetView(mDiffuseRoughnessMap->GetRtv(), diffuseRoughnessColor, 0, nullptr);
    gGraphicsCommandList->ClearRenderTargetView(mEmissiveMetallicMap->GetRtv(), emissiveMetallicColor, 0, nullptr);
    gGraphicsCommandList->ClearRenderTargetView(mNormalMap->GetRtv(), normalColor, 0, nullptr);
    gGraphicsCommandList->ClearRenderTargetView(mVelocityMap->GetRtv(), velocityColor, 0, nullptr);
    gGraphicsCommandList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
    gGraphicsCommandList->OMSetRenderTargets(_countof(gbufferRtvs), gbufferRtvs, false, GetRvaluePtr(mDepthStencilMap->GetDsv()));

    gGraphicsCommandList->SetPipelineState(mGeometryStaticMeshPSO->Get());
    ExecuteIndirect(mIndirectCommandBuffer[OPAQUE_STATIC - OPAQUE_MESH_START]);
    
    gGraphicsCommandList->SetPipelineState(mGeometrySkinnedMeshPSO->Get());
    ExecuteIndirect(mIndirectCommandBuffer[OPAQUE_SKINNED - OPAQUE_MESH_START]);

    mDiffuseRoughnessMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mEmissiveMetallicMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mNormalMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mVelocityMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Carol::GeometryPass::SetDepthStencilMap(ColorBuffer* depthStencilMap)
{
    mDepthStencilMap = depthStencilMap;
}

void Carol::GeometryPass::SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
{
    mIndirectCommandBuffer[type - OPAQUE_MESH_START] = indirectCommandBuffer;
}

uint32_t Carol::GeometryPass::GetDiffuseRoughnessMapSrvIdx()
{
    return mDiffuseRoughnessMap->GetGpuSrvIdx();
}

uint32_t Carol::GeometryPass::GetEmissiveMetallicMapSrvIdx()
{
    return mEmissiveMetallicMap->GetGpuSrvIdx();
}

uint32_t Carol::GeometryPass::GetNormalMapSrvIdx()
{
    return mNormalMap->GetGpuSrvIdx();
}

uint32_t Carol::GeometryPass::GetVelocityMapSrvIdx()
{
    return mVelocityMap->GetGpuSrvIdx();
}

void Carol::GeometryPass::InitPSOs()
{
    DXGI_FORMAT gbufferFormats[4] = { mDiffuseRoughnessFormat,mEmissiveMetallicFormat,mNormalFormat,mVelocityFormat };

    mGeometryStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mGeometryStaticMeshPSO->SetRenderTargetFormat(_countof(gbufferFormats), gbufferFormats, GetDsvFormat(mDepthStencilFormat));
	mGeometryStaticMeshPSO->SetAS(gCullAS.get());
	mGeometryStaticMeshPSO->SetMS(gMeshStaticMS.get());
	mGeometryStaticMeshPSO->SetPS(gGeometryPS.get());
	mGeometryStaticMeshPSO->Finalize();

    mGeometrySkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mGeometrySkinnedMeshPSO->SetRenderTargetFormat(_countof(gbufferFormats), gbufferFormats, GetDsvFormat(mDepthStencilFormat));
	mGeometrySkinnedMeshPSO->SetAS(gCullAS.get());
	mGeometrySkinnedMeshPSO->SetMS(gMeshSkinnedMS.get());
	mGeometrySkinnedMeshPSO->SetPS(gGeometryPS.get());
	mGeometrySkinnedMeshPSO->Finalize();
}

void Carol::GeometryPass::InitBuffers()
{
    float diffuseRoughnessColor[4] = {0.f,0.f,0.f,1.f};
    D3D12_CLEAR_VALUE diffuseRoughnessOptClearValue  = CD3DX12_CLEAR_VALUE(mDiffuseRoughnessFormat, diffuseRoughnessColor);

	mDiffuseRoughnessMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mDiffuseRoughnessFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &diffuseRoughnessOptClearValue);

    float emissiveMetallicColor[4] = {0.f,0.f,0.f,0.f};
    D3D12_CLEAR_VALUE emissiveMetallicOptClearValue  = CD3DX12_CLEAR_VALUE(mDiffuseRoughnessFormat, emissiveMetallicColor);

	mEmissiveMetallicMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mEmissiveMetallicFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &emissiveMetallicOptClearValue);

    float normalColor[4] = { 0.f,0.f,0.f,1000.f };
    D3D12_CLEAR_VALUE normalOptClearValue  = CD3DX12_CLEAR_VALUE(mNormalFormat, normalColor);

	mNormalMap = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mNormalFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &normalOptClearValue);

    float velocityColor[4] = { 0.f,0.f,0.f,0.f };
	D3D12_CLEAR_VALUE velocityOptClearValue = CD3DX12_CLEAR_VALUE(mVelocityFormat, velocityColor);
	mVelocityMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mVelocityFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&velocityOptClearValue);
}
