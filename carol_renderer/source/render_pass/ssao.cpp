#include <render_pass/ssao.h>
#include <render_pass/global_resources.h>
#include <render_pass/normal.h>
#include <render_pass/frame.h>
#include <render_pass/mesh.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/descriptor_allocator.h>
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
     mAmbientMapFormat(ambientMapFormat), 
     mSsaoConstants(make_unique<SsaoConstants>()),
     mSsaoCBAllocInfo(make_unique<HeapAllocInfo>())
{
    InitConstants();
    InitShaders();
    InitPSOs();
    InitRandomVectors();
    InitRandomVectorMap();
    OnResize();
}

void Carol::SsaoPass::SetBlurCount(uint32_t blurCout)
{
    mBlurCount = blurCout;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::SsaoPass::GetSsaoSrv()
{
    return GetShaderGpuSrv(AMBIENT0_SRV);
}

void Carol::SsaoPass::OnResize()
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

void Carol::SsaoPass::ReleaseIntermediateBuffers()
{
    mRandomVecMap->ReleaseIntermediateBuffer();
}

void Carol::SsaoPass::InitSsaoCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    if (!SsaoCBHeap)
	{
		SsaoCBHeap = make_unique<CircularHeap>(device, cmdList, true, 32, sizeof(SsaoConstants));
	}
}

void Carol::SsaoPass::Draw()
{
    DrawSsao();
    DrawAmbientMap();
}

void Carol::SsaoPass::Update()
{
    GetOffsetVectors(mSsaoConstants->OffsetVectors);
    auto blurWeights = CalcGaussWeights(2.5f);
    mSsaoConstants->BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
    mSsaoConstants->BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
    mSsaoConstants->BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

    SsaoCBHeap->DeleteResource(mSsaoCBAllocInfo.get());
    SsaoCBHeap->CreateResource(mSsaoCBAllocInfo.get());
    SsaoCBHeap->CopyData(mSsaoCBAllocInfo.get(), mSsaoConstants.get());
}

void Carol::SsaoPass::DrawSsao()
{
    mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
    mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(AMBIENT0_RTV), DirectX::Colors::Red, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(AMBIENT0_RTV)), true, nullptr);
    
    mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::ROOT_SIGNATURE_SSAO_CB, SsaoCBHeap->GetGPUVirtualAddress(mSsaoCBAllocInfo.get()));
    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_SRV_0, mGlobalResources->Frame->GetDepthStencilSrv());
    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_SRV_1, mGlobalResources->Normal->GetNormalSrv());
    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_SRV_2, GetShaderGpuSrv(RAND_VEC_SRV));
    
    mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"Ssao"].Get());
    mGlobalResources->CommandList->IASetVertexBuffers(0, 0, nullptr);
    mGlobalResources->CommandList->IASetIndexBuffer(nullptr);
    mGlobalResources->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mGlobalResources->CommandList->DrawInstanced(6, 1, 0, 0);

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

void Carol::SsaoPass::InitResources()
{
    D3D12_RESOURCE_DESC ambientMapDesc = {};
    ambientMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    ambientMapDesc.Alignment = 0;
    ambientMapDesc.Width = *mGlobalResources->ClientWidth;
    ambientMapDesc.Height = *mGlobalResources->ClientHeight;
    ambientMapDesc.DepthOrArraySize = 1;
    ambientMapDesc.MipLevels = 1;
    ambientMapDesc.Format = mAmbientMapFormat;
    ambientMapDesc.SampleDesc.Count = 1;
    ambientMapDesc.SampleDesc.Quality = 0;
    ambientMapDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    ambientMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;	
    CD3DX12_CLEAR_VALUE ambientMapClearValue(mAmbientMapFormat, DirectX::Colors::Red);

    mAmbientMap0 = make_unique<DefaultResource>(&ambientMapDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &ambientMapClearValue);
    mAmbientMap1 = make_unique<DefaultResource>(&ambientMapDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &ambientMapClearValue);

    InitDescriptors();
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
    mRandomVecMap->CopySubresources(mGlobalResources->CommandList, mGlobalResources->UploadBuffersHeap, &subresource);
}

void Carol::SsaoPass::InitShaders()
{
    vector<wstring> nullDefines{};

    vector<wstring> skinnedDefines =
    {
        L"SKINNED=1"
    };

    (*mGlobalResources->Shaders)[L"SsaoVS"] = make_unique<Shader>(L"shader\\ssao.hlsl", nullDefines, L"VS", L"vs_6_5");
    (*mGlobalResources->Shaders)[L"SsaoPS"] = make_unique<Shader>(L"shader\\ssao.hlsl", nullDefines, L"PS", L"ps_6_5");
    (*mGlobalResources->Shaders)[L"SsaoBlurVS"] = make_unique<Shader>(L"shader\\ssao_blur.hlsl", nullDefines, L"VS", L"vs_6_5");
    (*mGlobalResources->Shaders)[L"SsaoBlurPS"] = make_unique<Shader>(L"shader\\ssao_blur.hlsl", nullDefines, L"PS", L"ps_6_5");
}

void Carol::SsaoPass::InitPSOs()
{
    vector<D3D12_INPUT_ELEMENT_DESC> nullInputLayout(0);

    auto ssaoPsoDesc = *mGlobalResources->BasePsoDesc;
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

void Carol::SsaoPass::InitConstants()
{
    mSsaoConstants = make_unique<SsaoConstants>();
    mSsaoCBAllocInfo = make_unique<HeapAllocInfo>();
}

void Carol::SsaoPass::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
    std::copy(&mOffsets[0], &mOffsets[14], offsets);
}

void Carol::SsaoPass::InitDescriptors()
{
    mGlobalResources->CbvSrvUavAllocator->CpuAllocate(SSAO_SRV_COUNT, mCpuSrvAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(SSAO_RTV_COUNT, mRtvAllocInfo.get());
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    mGlobalResources->Device->CreateShaderResourceView(mRandomVecMap->Get(), &srvDesc, GetCpuSrv(RAND_VEC_SRV));

    srvDesc.Format = mAmbientMapFormat;
    mGlobalResources->Device->CreateShaderResourceView(mAmbientMap0->Get(), &srvDesc, GetCpuSrv(AMBIENT0_SRV));
    mGlobalResources->Device->CreateShaderResourceView(mAmbientMap1->Get(), &srvDesc, GetCpuSrv(AMBIENT1_SRV));
    
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mAmbientMapFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    mGlobalResources->Device->CreateRenderTargetView(mAmbientMap0->Get(), &rtvDesc, GetRtv(AMBIENT0_RTV));
    mGlobalResources->Device->CreateRenderTargetView(mAmbientMap1->Get(), &rtvDesc, GetRtv(AMBIENT1_RTV));
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
    Resource* output;
    CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;

    if (!vertBlur)
    {
        output = mAmbientMap1.get();
        outputRtv = GetRtv(AMBIENT1_RTV);
        inputSrv = GetShaderGpuSrv(AMBIENT0_SRV);
    }
    else
    {
        output = mAmbientMap0.get();
        outputRtv = GetRtv(AMBIENT0_RTV);
        inputSrv = GetShaderGpuSrv(AMBIENT1_SRV);
    }

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
    mGlobalResources->CommandList->ClearRenderTargetView(outputRtv, DirectX::Colors::Red, 0, nullptr);
    mGlobalResources->CommandList->OMSetRenderTargets(1, &outputRtv, true, nullptr);
    
    mGlobalResources->CommandList->SetGraphicsRoot32BitConstant(RootSignature::ROOT_SIGNATURE_CONSTANT, vertBlur, 0);
    mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_SRV_2, inputSrv);

    mGlobalResources->CommandList->IASetVertexBuffers(0, 0, nullptr);
    mGlobalResources->CommandList->IASetIndexBuffer(nullptr);
    mGlobalResources->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mGlobalResources->CommandList->DrawInstanced(6, 1, 0, 0);

    mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(output->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}
