#include <render_pass/ssao.h>
#include <global.h>
#include <render_pass/frame.h>
#include <dx12.h>
#include <scene/camera.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <string_view>

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

Carol::SsaoPass::SsaoPass(
    uint32_t blurCount,
    DXGI_FORMAT ambientMapFormat)
     :mBlurCount(blurCount), 
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
    gCommandList->RSSetViewports(1, &mViewport);
    gCommandList->RSSetScissorRects(1, &mScissorRect);

    gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    gCommandList->ClearRenderTargetView(mAmbientMap0->GetRtv(), DirectX::Colors::Red, 0, nullptr);
    gCommandList->OMSetRenderTargets(1, GetRvaluePtr(mAmbientMap0->GetRtv()), true, nullptr);
    
    gCommandList->SetPipelineState(gPSOs[L"Ssao"]->Get());
    static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);

    gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
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
    D3D12_CLEAR_VALUE optClearValue = CD3DX12_CLEAR_VALUE(mAmbientMapFormat, DirectX::Colors::Red);

    mAmbientMap0 = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mAmbientMapFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &optClearValue);

	mAmbientMap1 = make_unique<ColorBuffer>(
        mWidth,
        mHeight,
        1,
        COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
        mAmbientMapFormat,
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
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
        gHeapManager->GetDefaultBuffersHeap(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
    mRandomVecMap->CopySubresources(gHeapManager->GetUploadBuffersHeap(), &subresource, 0, 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Carol::SsaoPass::InitShaders()
{
    vector<wstring_view> nullDefines{};

    vector<wstring_view> skinnedDefines =
    {
        L"SKINNED=1"
    };

    gShaders[L"SsaoMS"] = make_unique<Shader>(L"shader\\ssao_ms.hlsl", nullDefines, L"main", L"ms_6_6");
    gShaders[L"SsaoPS"] = make_unique<Shader>(L"shader\\ssao_ps.hlsl", nullDefines, L"main", L"ps_6_6");
    gShaders[L"SsaoBlurPS"] = make_unique<Shader>(L"shader\\ssao_blur_ps.hlsl", nullDefines, L"main", L"ps_6_6");
}

void Carol::SsaoPass::InitPSOs()
{
    auto ssaoMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
    ssaoMeshPSO->SetDepthStencilState(gDepthDisabledState);
    ssaoMeshPSO->SetRenderTargetFormat(mAmbientMapFormat);
    ssaoMeshPSO->SetMS(gShaders[L"SsaoMS"].get());
    ssaoMeshPSO->SetPS(gShaders[L"SsaoPS"].get());
    ssaoMeshPSO->Finalize();
    gPSOs[L"Ssao"] = std::move(ssaoMeshPSO);

	auto blurMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
    blurMeshPSO->SetDepthStencilState(gDepthDisabledState);
    blurMeshPSO->SetRenderTargetFormat(mAmbientMapFormat);
    blurMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
    blurMeshPSO->SetPS(gShaders[L"SsaoBlurPS"].get());
    blurMeshPSO->Finalize();
    gPSOs[L"SsaoBlur"] = std::move(blurMeshPSO);
}

void Carol::SsaoPass::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
    std::copy(&mOffsets[0], &mOffsets[14], offsets);
}

void Carol::SsaoPass::DrawAmbientMap()
{
    gCommandList->SetPipelineState(gPSOs[L"SsaoBlur"]->Get());

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

    gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    gCommandList->ClearRenderTargetView(outputRtv, DirectX::Colors::Red, 0, nullptr);
    gCommandList->OMSetRenderTargets(1, &outputRtv, true, nullptr);
    gCommandList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, 2, ssaoConstants, 0);

    static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);
    gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
}
