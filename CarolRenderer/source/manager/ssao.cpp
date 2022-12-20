#include <manager/ssao.h>
#include <global_resources.h>
#include <manager/display.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>
#include <dx12/descriptor_allocator.h>
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

Carol::SsaoManager::SsaoManager(GlobalResources* globalResources, uint32_t blurCount, DXGI_FORMAT normalMapFormat, DXGI_FORMAT ambientMapFormat)
    :Manager(globalResources), mBlurCount(blurCount), mNormalMapFormat(normalMapFormat), mAmbientMapFormat(ambientMapFormat), mSsaoConstants(make_unique<SsaoConstants>())
{
    InitConstants();
    InitRootSignature();
    InitShaders();
    InitPSOs();
    InitRandomVectors();
    InitRandomVectorMap();
    OnResize();
}

void Carol::SsaoManager::SetBlurCount(uint32_t blurCout)
{
    mBlurCount = blurCout;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::SsaoManager::GetSsaoSrv()
{
    return GetShaderGpuSrv(AMBIENT0_SRV);
}

void Carol::SsaoManager::OnResize()
{
    static uint32_t width = 0;
    static uint32_t height = 0;

    if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
    {
        Manager::OnResize();

        width = *mGlobalResources->ClientWidth;
        height = *mGlobalResources->ClientHeight;

        InitResources();
    }
}

void Carol::SsaoManager::ReleaseIntermediateBuffers()
{
    mRandomVecMap->ReleaseIntermediateBuffer();
}

void Carol::SsaoManager::InitSsaoCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    if (!SsaoCBHeap)
	{
		SsaoCBHeap = make_unique<CircularHeap>(device, cmdList, true, 32, sizeof(SsaoConstants));
	}
}

void Carol::SsaoManager::Draw()
{
    DrawNormalsAndDepth();
    DrawSsao();
    DrawAmbientMap();
}

void Carol::SsaoManager::Update()
{
    XMMATRIX proj = mGlobalResources->Camera->GetProj();
    static XMMATRIX tex(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);

    XMStoreFloat4x4(&mSsaoConstants->Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mSsaoConstants->InvProj, XMMatrixTranspose(XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(proj)), proj)));
    XMStoreFloat4x4(&mSsaoConstants->ProjTex, XMMatrixTranspose(XMMatrixMultiply(proj, tex)));

    GetOffsetVectors(mSsaoConstants->OffsetVectors);
    auto blurWeights = CalcGaussWeights(2.5f);
    mSsaoConstants->BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
    mSsaoConstants->BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
    mSsaoConstants->BlurWeights[2] = XMFLOAT4(&blurWeights[8]);
    mSsaoConstants->InvRenderTargetSize = { 1.0f / *mGlobalResources->ClientWidth,1.0f / *mGlobalResources->ClientHeight };

    SsaoCBHeap->DeleteResource(mSsaoCBAllocInfo.get());
    SsaoCBHeap->CreateResource(nullptr, nullptr, mSsaoCBAllocInfo.get());
    SsaoCBHeap->CopyData(mSsaoCBAllocInfo.get(), mSsaoConstants.get());
}

void Carol::SsaoManager::DrawNormalsAndDepth()
{
    mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
    mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(NORMAL_RTV), DirectX::Colors::Blue, 0, nullptr);
    mGlobalResources->CommandList->ClearDepthStencilView(mGlobalResources->Display->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(NORMAL_RTV)), true, GetRvaluePtr(mGlobalResources->Display->GetDepthStencilView()));

    mGlobalResources->CommandList->SetGraphicsRootSignature(mGlobalResources->RootSignature);
    mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(0, mGlobalResources->PassCBHeap->GetGPUVirtualAddress(mGlobalResources->PassCBAllocInfo));

    mGlobalResources->DrawMeshes(
        (*mGlobalResources->PSOs)[L"DrawNormalsStatic"].Get(),
        (*mGlobalResources->PSOs)[L"DrawNormalsSkinned"].Get(),
        (*mGlobalResources->PSOs)[L"DrawNormalsStatic"].Get(),
		(*mGlobalResources->PSOs)[L"DrawNormalsSkinned"].Get(),
        false
    );
    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::SsaoManager::DrawSsao()
{
    mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
    mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(AMBIENT0_RTV), DirectX::Colors::Red, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(AMBIENT0_RTV)), true, nullptr);

    mGlobalResources->CommandList->SetGraphicsRootSignature(mRootSignature.Get());
    mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(0, SsaoCBHeap->GetGPUVirtualAddress(mSsaoCBAllocInfo.get()));
    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(2, mGlobalResources->Display->GetDepthStencilSrv());
    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(3, GetShaderGpuSrv(NORMAL_SRV));
    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(4, GetShaderGpuSrv(RAND_VEC_SRV));

    mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"Ssao"].Get());
    mGlobalResources->CommandList->IASetVertexBuffers(0, 0, nullptr);
    mGlobalResources->CommandList->IASetIndexBuffer(nullptr);
    mGlobalResources->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mGlobalResources->CommandList->DrawInstanced(6, 1, 0, 0);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

Carol::vector<float> Carol::SsaoManager::CalcGaussWeights(float sigma)
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

void Carol::SsaoManager::InitResources()
{
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = *mGlobalResources->ClientWidth;
    texDesc.Height = *mGlobalResources->ClientHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mNormalMapFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_CLEAR_VALUE normalMapClearValue(mNormalMapFormat, DirectX::Colors::Blue);
    mNormalMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &normalMapClearValue);
	
    texDesc.Width = *mGlobalResources->ClientWidth;
    texDesc.Height = *mGlobalResources->ClientHeight;
    texDesc.Format = mAmbientMapFormat;

    CD3DX12_CLEAR_VALUE ambientMapClearValue(mAmbientMapFormat, DirectX::Colors::Red);
    mAmbientMap0 = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &ambientMapClearValue);
    mAmbientMap1 = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &ambientMapClearValue);

    InitDescriptors();
}

void Carol::SsaoManager::InitRandomVectors()
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

void Carol::SsaoManager::InitRandomVectorMap()
{
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = 256;
    texDesc.Height = 256;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    mRandomVecMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap);

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
    mRandomVecMap->CopySubresources(mGlobalResources, &subresource);
}

void Carol::SsaoManager::InitRootSignature()
{
    CD3DX12_ROOT_PARAMETER rootPara[5];
    rootPara[0].InitAsConstantBufferView(0);
    rootPara[1].InitAsConstants(1, 1);

    CD3DX12_DESCRIPTOR_RANGE range[3];
    range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

    rootPara[2].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootPara[3].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[4].InitAsDescriptorTable(1, &range[2], D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = StaticSampler::GetDefaultStaticSamplers();
    CD3DX12_ROOT_SIGNATURE_DESC slotRootSigDesc(5, rootPara, staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Blob serializedRootSigBlob;
    Blob errorBlob;

    auto hr = D3D12SerializeRootSignature(&slotRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob.Get())
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    ThrowIfFailed(hr);
    ThrowIfFailed(mGlobalResources->Device->CreateRootSignature(0, serializedRootSigBlob->GetBufferPointer(), serializedRootSigBlob->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Carol::SsaoManager::InitShaders()
{
    vector<wstring> nullDefines{};

    vector<wstring> skinnedDefines =
    {
        L"SKINNED=1"
    };

    (*mGlobalResources->Shaders)[L"DrawNormalsStaticVS"] = make_unique<Shader>(L"shader\\normals.hlsl", nullDefines, L"VS", L"vs_6_5");
    (*mGlobalResources->Shaders)[L"DrawNormalsStaticPS"] = make_unique<Shader>(L"shader\\normals.hlsl", nullDefines, L"PS", L"ps_6_5");
    (*mGlobalResources->Shaders)[L"DrawNormalsSkinnedVS"] = make_unique<Shader>(L"shader\\normals.hlsl", skinnedDefines, L"VS", L"vs_6_5");
    (*mGlobalResources->Shaders)[L"DrawNormalsSkinnedPS"] = make_unique<Shader>(L"shader\\normals.hlsl", skinnedDefines, L"PS", L"ps_6_5");
    (*mGlobalResources->Shaders)[L"SsaoVS"] = make_unique<Shader>(L"shader\\ssao.hlsl", nullDefines, L"VS", L"vs_6_5");
    (*mGlobalResources->Shaders)[L"SsaoPS"] = make_unique<Shader>(L"shader\\ssao.hlsl", nullDefines, L"PS", L"ps_6_5");
    (*mGlobalResources->Shaders)[L"SsaoBlurVS"] = make_unique<Shader>(L"shader\\ssao_blur.hlsl", nullDefines, L"VS", L"vs_6_5");
    (*mGlobalResources->Shaders)[L"SsaoBlurPS"] = make_unique<Shader>(L"shader\\ssao_blur.hlsl", nullDefines, L"PS", L"ps_6_5");
}

void Carol::SsaoManager::InitPSOs()
{
    vector<D3D12_INPUT_ELEMENT_DESC> nullInputLayout(0);

    auto drawNormalsStaticPsoDesc = *mGlobalResources->BasePsoDesc;
    auto drawNormalsStaticVS = (*mGlobalResources->Shaders)[L"DrawNormalsStaticVS"].get();
    auto drawNormalsStaticPS = (*mGlobalResources->Shaders)[L"DrawNormalsStaticPS"].get();
    drawNormalsStaticPsoDesc.VS = { reinterpret_cast<byte*>(drawNormalsStaticVS->GetBufferPointer()),drawNormalsStaticVS->GetBufferSize() };
    drawNormalsStaticPsoDesc.PS = { reinterpret_cast<byte*>(drawNormalsStaticPS->GetBufferPointer()),drawNormalsStaticPS->GetBufferSize() };
    drawNormalsStaticPsoDesc.RTVFormats[0] = mNormalMapFormat;
    ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&drawNormalsStaticPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"DrawNormalsStatic"].GetAddressOf())));

    auto drawNormalsSkinnedPsoDesc = drawNormalsStaticPsoDesc;
    auto drawNormalsSkinnedVS = (*mGlobalResources->Shaders)[L"DrawNormalsSkinnedVS"].get();
    auto drawNormalsSkinnedPS = (*mGlobalResources->Shaders)[L"DrawNormalsSkinnedPS"].get();
    drawNormalsSkinnedPsoDesc.VS = { reinterpret_cast<byte*>(drawNormalsSkinnedVS->GetBufferPointer()),drawNormalsSkinnedVS->GetBufferSize() };
    drawNormalsSkinnedPsoDesc.PS = { reinterpret_cast<byte*>(drawNormalsSkinnedPS->GetBufferPointer()),drawNormalsSkinnedPS->GetBufferSize() };
    ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&drawNormalsSkinnedPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"DrawNormalsSkinned"].GetAddressOf())));

    auto ssaoPsoDesc = *mGlobalResources->BasePsoDesc;
    ssaoPsoDesc.pRootSignature = mRootSignature.Get();
    auto ssaoVS = (*mGlobalResources->Shaders)[L"SsaoVS"].get();
    auto ssaoPS = (*mGlobalResources->Shaders)[L"SsaoPS"].get();
    ssaoPsoDesc.VS = { reinterpret_cast<byte*>(ssaoVS->GetBufferPointer()),ssaoVS->GetBufferSize() };
    ssaoPsoDesc.PS = { reinterpret_cast<byte*>(ssaoPS->GetBufferPointer()),ssaoPS->GetBufferSize() };
    ssaoPsoDesc.InputLayout = { nullInputLayout.data(),(uint32_t)nullInputLayout.size() };
    ssaoPsoDesc.RTVFormats[0] = mAmbientMapFormat;
    ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    ssaoPsoDesc.DepthStencilState.DepthEnable = false;
    ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"Ssao"].GetAddressOf())));

    auto blurPsoDesc = ssaoPsoDesc;
    auto blurVS = (*mGlobalResources->Shaders)[L"SsaoBlurVS"].get();
    auto blurPS = (*mGlobalResources->Shaders)[L"SsaoBlurPS"].get();
    blurPsoDesc.VS = { reinterpret_cast<byte*>(blurVS->GetBufferPointer()),blurVS->GetBufferSize() };
    blurPsoDesc.PS = { reinterpret_cast<byte*>(blurPS->GetBufferPointer()),blurPS->GetBufferSize() };
    ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&blurPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"SsaoBlur"].GetAddressOf())));
}

void Carol::SsaoManager::InitConstants()
{
    mSsaoConstants = make_unique<SsaoConstants>();
    mSsaoCBAllocInfo = make_unique<HeapAllocInfo>();
}

void Carol::SsaoManager::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
    std::copy(&mOffsets[0], &mOffsets[14], offsets);
}

void Carol::SsaoManager::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(SSAO_SRV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(SSAO_RTV_COUNT, mRtvAllocInfo.get());

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = mNormalMapFormat;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    mGlobalResources->Device->CreateShaderResourceView(mNormalMap->Get(), &srvDesc, GetCpuSrv(NORMAL_SRV));

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    mGlobalResources->Device->CreateShaderResourceView(mRandomVecMap->Get(), &srvDesc, GetCpuSrv(RAND_VEC_SRV));

    srvDesc.Format = mAmbientMapFormat;
    mGlobalResources->Device->CreateShaderResourceView(mAmbientMap0->Get(), &srvDesc, GetCpuSrv(AMBIENT0_SRV));
    mGlobalResources->Device->CreateShaderResourceView(mAmbientMap1->Get(), &srvDesc, GetCpuSrv(AMBIENT1_SRV));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mNormalMapFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    mGlobalResources->Device->CreateRenderTargetView(mNormalMap->Get(), &rtvDesc, GetRtv(NORMAL_RTV));

    rtvDesc.Format = mAmbientMapFormat;
    mGlobalResources->Device->CreateRenderTargetView(mAmbientMap0->Get(), &rtvDesc, GetRtv(AMBIENT0_RTV));
    mGlobalResources->Device->CreateRenderTargetView(mAmbientMap1->Get(), &rtvDesc, GetRtv(AMBIENT1_RTV));
}

void Carol::SsaoManager::DrawAmbientMap()
{
    mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"SsaoBlur"].Get());

    for (int i = 0; i < mBlurCount; ++i)
    {
        DrawAmbientMap(true);
        DrawAmbientMap(false);
    }
}

void Carol::SsaoManager::DrawAmbientMap(bool horzBlur)
{
    Resource* output;
    CD3DX12_GPU_DESCRIPTOR_HANDLE inputGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

    if (horzBlur)
    {
        output = mAmbientMap1.get();
        inputGpuSrv = GetShaderGpuSrv(AMBIENT0_SRV);
        outputRtv = GetRtv(AMBIENT1_RTV);

        mGlobalResources->CommandList->SetGraphicsRoot32BitConstant(1, 1, 0);
    }
    else
    {
        output = mAmbientMap0.get();
        inputGpuSrv = GetShaderGpuSrv(AMBIENT1_SRV);
        outputRtv = GetRtv(AMBIENT0_RTV);

        mGlobalResources->CommandList->SetGraphicsRoot32BitConstant(1, 0, 0);
    }

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(outputRtv, DirectX::Colors::Red, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(4, inputGpuSrv);
	
    mGlobalResources->CommandList->IASetVertexBuffers(0, 0, nullptr);
    mGlobalResources->CommandList->IASetIndexBuffer(nullptr);
    mGlobalResources->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mGlobalResources->CommandList->DrawInstanced(6, 1, 0, 0);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}
