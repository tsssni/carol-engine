#include <render_pass/ssao.h>
#include <render_pass/global_resources.h>
#include <render_pass/normal.h>
#include <render_pass/frame.h>
#include <render_pass/mesh.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/descriptor.h>
#include <dx12/root_signature.h>
#include <scene/camera.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

namespace Carol
{
    using namespace DirectX;
    using namespace DirectX::PackedVector;
    using std::vector;
    using std::wstring;
    using std::make_unique;
}

Carol::SsaoPass::SsaoPass(GlobalResources* globalResources, uint32_t blurCount, DXGI_FORMAT ambientMapFormat)
    :RenderPass(globalResources), 
     mBlurCount(blurCount), 
     mAmbientMapFormat(ambientMapFormat)
{
    InitShaders();
    InitPSOs();
    InitRandomVectors();
    InitRandomVectorMap();
}

void Carol::SsaoPass::SetBlurCount(uint32_t blurCout)
{
    mBlurCount = blurCout;
}

uint32_t Carol::SsaoPass::GetRandVecSrvIdx()
{
    return mRandomVecMap->GetGpuSrvIdx();
}

uint32_t Carol::SsaoPass::GetSsaoSrvIdx()
{
    return mAmbientMap0->GetGpuSrvIdx();
}

void Carol::SsaoPass::OnResize()
{
    static uint32_t width = 0;
    static uint32_t height = 0;

    if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
    {
        width = *mGlobalResources->ClientWidth;
        height = *mGlobalResources->ClientHeight;

        InitBuffers();
    }
}

void Carol::SsaoPass::ReleaseIntermediateBuffers()
{
    mRandomVecMap->ReleaseIntermediateBuffer();
}

void Carol::SsaoPass::Draw()
{
    DrawSsao();
    DrawAmbientMap();
}

void Carol::SsaoPass::Update()
{
}

void Carol::SsaoPass::DrawSsao()
{
    mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
    mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(mAmbientMap0->GetRtv(), DirectX::Colors::Red, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mAmbientMap0->GetRtv()), true, nullptr);
    
    mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"Ssao"].Get());
    mGlobalResources->CommandList->DispatchMesh(1, 1, 1);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

Carol::vector<float> Carol::SsaoPass::CalcGaussWeights(float sigma)
{
    static int maxGaussRadius = 5;

    int blurRadius = (int)ceil(2.0f * sigma);
    assert(blurRadius <= maxGaussRadius);

    vector<float> weights;
    float weightsSum = 0.0f;

    weights.resize(2 * blurRadius + 1);

    for (int i = -blurRadius; i <= blurRadius; ++i)
    {
        weights[blurRadius + i] = expf(-i * i / (2.0f * sigma * sigma));
        weightsSum += weights[blurRadius + i];
    }

    for (int i = 0; i < blurRadius * 2 + 1; ++i)
    {
        weights[i] /= weightsSum;
    }
    
    return weights;
}

void Carol::SsaoPass::InitBuffers()
{
    uint32_t width = *mGlobalResources->ClientWidth;
    uint32_t height = *mGlobalResources->ClientHeight;
    D3D12_CLEAR_VALUE optClearValue = CD3DX12_CLEAR_VALUE(mAmbientMapFormat, DirectX::Colors::Red);

    mAmbientMap0 = make_unique<ColorBuffer>(
        width,
        height,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mAmbientMapFormat,
        mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
        mGlobalResources->DescriptorManager,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &optClearValue);

	mAmbientMap1 = make_unique<ColorBuffer>(
        width,
        height,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mAmbientMapFormat,
        mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
        mGlobalResources->DescriptorManager,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &optClearValue);
}

void Carol::SsaoPass::InitRandomVectors()
{
    mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

    mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

    // 6 centers of cube faces
    mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
    mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

    mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
    mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

    mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
    mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

    for (int i = 0; i < 14; ++i)
    {
        float s = 0.25f + rand() * 1.0f / RAND_MAX * (1.0f - 0.25f);
        XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

        XMStoreFloat4(&mOffsets[i], v);
    }
}

void Carol::SsaoPass::InitRandomVectorMap()
{ 
    mRandomVecMap = make_unique<ColorBuffer>(
        256,
        256,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
        mGlobalResources->DescriptorManager,
        D3D12_RESOURCE_STATE_GENERIC_READ);

    XMCOLOR initData[256 * 256];
    for (int i = 0; i < 256; ++i)
    {
        for (int j = 0; j < 256; ++j)
        {
            XMCOLOR randVec = { rand() * 1.0f / RAND_MAX,rand() * 1.0f / RAND_MAX,rand() * 1.0f / RAND_MAX ,0.0f };
            initData[i * 256 + j] = randVec;
        }
    }

    D3D12_SUBRESOURCE_DATA subresource;
    subresource.pData = initData;
    subresource.RowPitch = 256 * sizeof(XMCOLOR);
    subresource.SlicePitch = subresource.RowPitch * 256;
    mRandomVecMap->CopySubresources(mGlobalResources->CommandList, mGlobalResources->HeapManager->GetUploadBuffersHeap(), &subresource, 0, 1);
}

void Carol::SsaoPass::InitShaders()
{
    vector<wstring> nullDefines{};

    vector<wstring> skinnedDefines =
    {
        L"SKINNED=1"
    };

    (*mGlobalResources->Shaders)[L"SsaoMS"] = make_unique<Shader>(L"shader\\ssao_ms.hlsl", nullDefines, L"main", L"ms_6_6");
    (*mGlobalResources->Shaders)[L"SsaoPS"] = make_unique<Shader>(L"shader\\ssao_ps.hlsl", nullDefines, L"main", L"ps_6_6");
    (*mGlobalResources->Shaders)[L"SsaoBlurPS"] = make_unique<Shader>(L"shader\\ssao_blur_ps.hlsl", nullDefines, L"main", L"ps_6_6");
}

void Carol::SsaoPass::InitPSOs()
{
    auto ssaoPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
    auto ssaoMS = (*mGlobalResources->Shaders)[L"SsaoMS"].get();
    auto ssaoPS = (*mGlobalResources->Shaders)[L"SsaoPS"].get();
    ssaoPsoDesc.MS = { reinterpret_cast<byte*>(ssaoMS->GetBufferPointer()),ssaoMS->GetBufferSize() };
    ssaoPsoDesc.PS = { reinterpret_cast<byte*>(ssaoPS->GetBufferPointer()),ssaoPS->GetBufferSize() };
    ssaoPsoDesc.RTVFormats[0] = mAmbientMapFormat;
    ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    ssaoPsoDesc.DepthStencilState.DepthEnable = false;
    auto ssaoPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(ssaoPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC ssaoStreamDesc;
    ssaoStreamDesc.pPipelineStateSubobjectStream = &ssaoPsoStream;
    ssaoStreamDesc.SizeInBytes = sizeof(ssaoPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&ssaoStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"Ssao"].GetAddressOf())));

    auto blurPsoDesc = ssaoPsoDesc;
    auto blurMS = (*mGlobalResources->Shaders)[L"ScreenMS"].get();
    auto blurPS = (*mGlobalResources->Shaders)[L"SsaoBlurPS"].get();
    blurPsoDesc.MS = { reinterpret_cast<byte*>(blurMS->GetBufferPointer()),blurMS->GetBufferSize() };
    blurPsoDesc.PS = { reinterpret_cast<byte*>(blurPS->GetBufferPointer()),blurPS->GetBufferSize() };
    auto blurPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(blurPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC blurStreamDesc;
    blurStreamDesc.pPipelineStateSubobjectStream = &blurPsoStream;
    blurStreamDesc.SizeInBytes = sizeof(blurPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&blurStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"SsaoBlur"].GetAddressOf())));
}

void Carol::SsaoPass::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
    std::copy(&mOffsets[0], &mOffsets[14], offsets);
}

void Carol::SsaoPass::DrawAmbientMap()
{
    mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"SsaoBlur"].Get());

    for (int i = 0; i < mBlurCount; ++i)
    {
        DrawAmbientMap(false);
        DrawAmbientMap(true);
    }
}

void Carol::SsaoPass::DrawAmbientMap(bool vertBlur)
{
    ID3D12Resource* output;
    CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;
    uint32_t ssaoConstants[2] = { vertBlur };

    if (!vertBlur)
    {
        output = mAmbientMap1->Get();
        outputRtv = mAmbientMap1->GetRtv();
        ssaoConstants[1] = mAmbientMap0->GetGpuSrvIdx();
    }
    else
    {
        output = mAmbientMap0->Get();
        outputRtv = mAmbientMap0->GetRtv();
        ssaoConstants[1] = mAmbientMap1->GetGpuSrvIdx();
    }

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(outputRtv, DirectX::Colors::Red, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, &outputRtv, true, nullptr);
    mGlobalResources->CommandList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, 2, ssaoConstants, 0);

    mGlobalResources->CommandList->DispatchMesh(1, 1, 1);
    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}
