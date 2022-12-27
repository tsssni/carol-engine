#include <render_pass/normal.h>
#include <render_pass/global_resources.h>
#include <render_pass/frame.h>
#include <render_pass/mesh.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/shader.h>
#include <DirectXColors.h>

namespace Carol
{
    using std::vector;
    using std::wstring;
    using std::make_unique;
    using namespace DirectX;
}

Carol::NormalPass::NormalPass(GlobalResources* globalResources, DXGI_FORMAT normalMapFormat)
	:RenderPass(globalResources), mNormalMapFormat(normalMapFormat)
{
    InitShaders();
    InitPSOs();
    OnResize();
}

void Carol::NormalPass::Draw()
{
    mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
    mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(NORMAL_RTV), DirectX::Colors::Blue, 0, nullptr);
    mGlobalResources->CommandList->ClearDepthStencilView(mGlobalResources->Frame->GetDepthStencilDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(NORMAL_RTV)), true, GetRvaluePtr(mGlobalResources->Frame->GetDepthStencilDsv()));

    mGlobalResources->Meshes->DrawMeshes({
        (*mGlobalResources->PSOs)[L"NormalsStatic"].Get(),
        (*mGlobalResources->PSOs)[L"NormalsSkinned"].Get(),
        (*mGlobalResources->PSOs)[L"NormalsStatic"].Get(),
        (*mGlobalResources->PSOs)[L"NormalsSkinned"].Get()
        });

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::NormalPass::Update()
{
}

void Carol::NormalPass::OnResize()
{
    static uint32_t width = 0;
    static uint32_t height = 0;

    if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
    {
        RenderPass::OnResize();

        width = *mGlobalResources->ClientWidth;
        height = *mGlobalResources->ClientHeight;

        InitResources();
    }
}

void Carol::NormalPass::ReleaseIntermediateBuffers()
{
}

uint32_t Carol::NormalPass::GetNormalSrvIdx()
{
    return mGpuCbvSrvUavAllocInfo->StartOffset + NORMAL_SRV;
}

void Carol::NormalPass::InitShaders()
{
    vector<wstring> nullDefines{};

    vector<wstring> skinnedDefines =
    {
        L"SKINNED=1"
    };

    (*mGlobalResources->Shaders)[L"NormalsStaticVS"] = make_unique<Shader>(L"shader\\normals.hlsl", nullDefines, L"VS", L"vs_6_6");
    (*mGlobalResources->Shaders)[L"NormalsStaticPS"] = make_unique<Shader>(L"shader\\normals.hlsl", nullDefines, L"PS", L"ps_6_6");
    (*mGlobalResources->Shaders)[L"NormalsSkinnedVS"] = make_unique<Shader>(L"shader\\normals.hlsl", skinnedDefines, L"VS", L"vs_6_6");
    (*mGlobalResources->Shaders)[L"NormalsSkinnedPS"] = make_unique<Shader>(L"shader\\normals.hlsl", skinnedDefines, L"PS", L"ps_6_6");
}

void Carol::NormalPass::InitPSOs()
{
    vector<D3D12_INPUT_ELEMENT_DESC> nullInputLayout(0);

    auto normalsStaticPsoDesc = *mGlobalResources->BasePsoDesc;
    auto normalsStaticVS = (*mGlobalResources->Shaders)[L"NormalsStaticVS"].get();
    auto normalsStaticPS = (*mGlobalResources->Shaders)[L"NormalsStaticPS"].get();
    normalsStaticPsoDesc.VS = { reinterpret_cast<byte*>(normalsStaticVS->GetBufferPointer()),normalsStaticVS->GetBufferSize() };
    normalsStaticPsoDesc.PS = { reinterpret_cast<byte*>(normalsStaticPS->GetBufferPointer()),normalsStaticPS->GetBufferSize() };
    normalsStaticPsoDesc.RTVFormats[0] = mNormalMapFormat;
    ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&normalsStaticPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"NormalsStatic"].GetAddressOf())));

    auto normalsSkinnedPsoDesc = normalsStaticPsoDesc;
    auto normalsSkinnedVS = (*mGlobalResources->Shaders)[L"NormalsSkinnedVS"].get();
    auto normalsSkinnedPS = (*mGlobalResources->Shaders)[L"NormalsSkinnedPS"].get();
    normalsSkinnedPsoDesc.VS = { reinterpret_cast<byte*>(normalsSkinnedVS->GetBufferPointer()),normalsSkinnedVS->GetBufferSize() };
    normalsSkinnedPsoDesc.PS = { reinterpret_cast<byte*>(normalsSkinnedPS->GetBufferPointer()),normalsSkinnedPS->GetBufferSize() };
    ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&normalsSkinnedPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"NormalsSkinned"].GetAddressOf())));
}

void Carol::NormalPass::InitResources()
{
	D3D12_RESOURCE_DESC normalDesc = {};
    normalDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    normalDesc.Alignment = 0;
    normalDesc.Width = *mGlobalResources->ClientWidth;
    normalDesc.Height = *mGlobalResources->ClientHeight;
    normalDesc.DepthOrArraySize = 1;
    normalDesc.MipLevels = 1;
    normalDesc.Format = mNormalMapFormat;
    normalDesc.SampleDesc.Count = 1;
    normalDesc.SampleDesc.Quality = 0;
    normalDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    normalDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_CLEAR_VALUE normalMapClearValue(mNormalMapFormat, DirectX::Colors::Blue);
    mNormalMap = make_unique<DefaultResource>(&normalDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &normalMapClearValue);
    
    InitDescriptors();
}

void Carol::NormalPass::InitDescriptors()
{
    mGlobalResources->CbvSrvUavAllocator->CpuAllocate(NORMAL_CBV_SRV_UAV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(NORMAL_RTV_COUNT, mRtvAllocInfo.get());
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = mNormalMapFormat;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    mGlobalResources->Device->CreateShaderResourceView(mNormalMap->Get(), &srvDesc, GetCpuCbvSrvUav(NORMAL_SRV));
    
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mNormalMapFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    mGlobalResources->Device->CreateRenderTargetView(mNormalMap->Get(), &rtvDesc, GetRtv(NORMAL_RTV));

    CopyDescriptors();
}
