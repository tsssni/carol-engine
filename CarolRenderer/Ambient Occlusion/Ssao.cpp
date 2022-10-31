#include "SSAO.h"
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

using namespace DirectX;
using namespace DirectX::PackedVector;
using std::make_unique;

void Carol::SsaoManager::InitSsaoManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint32_t width, uint32_t height)
{
    mNormalMap = make_unique<DefaultBuffer>();
    mAmbientMap0 = make_unique<DefaultBuffer>();
    mAmbientMap1 = make_unique<DefaultBuffer>();

    mDevice = device;
    OnResize(width, height);

    InitRandomVectors();
    InitRandomVectorMap(cmdList);
}

void Carol::SsaoManager::OnResize(uint32_t width, uint32_t height)
{
    if (width != mRenderTargetWidth || height != mRenderTargetHeight)
    {
        mRenderTargetWidth = width;
        mRenderTargetHeight = height;

        mViewport.TopLeftX = 0.0f;
        mViewport.TopLeftY = 0.0f;
        mViewport.Width = width >> 1;
        mViewport.Height = height >> 1;
        mViewport.MinDepth = 0.0f;
        mViewport.MaxDepth = 1.0f;
        mScissorRect = { 0,0,(int)width >> 1,(int)height >> 1 };

        InitResources();
    }
}

void Carol::SsaoManager::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
{
    mSsaoPso = ssaoPso;
    mSsaoBlurPso = ssaoBlurPso;
}

void Carol::SsaoManager::ComputeSsao(
    ID3D12GraphicsCommandList* cmdList,
    ID3D12RootSignature* rootSignature,
    FrameResource* frameResource,
    uint32_t blurCount)
{
    cmdList->RSSetViewports(1, &mViewport);
    cmdList->RSSetScissorRects(1, &mScissorRect);

    mAmbientMap0->TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ClearRenderTargetView(mAmbientMap0CpuRtv, DirectX::Colors::Red, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &mAmbientMap0CpuRtv, true, nullptr);

    cmdList->SetGraphicsRootSignature(rootSignature);
    cmdList->SetGraphicsRootConstantBufferView(0, frameResource->SsaoCB.GetVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(2, mNormalMapGpuSrv);
    cmdList->SetGraphicsRootDescriptorTable(3, mRandomVecGpuSrv);

    cmdList->SetPipelineState(mSsaoPso);
    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(6, 1, 0, 0);
    mAmbientMap0->TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);

    BlurAmbientMap(cmdList, frameResource, blurCount);
}

vector<float> Carol::SsaoManager::CalcGaussWeights(float sigma)
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
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mRenderTargetWidth;
    texDesc.Height = mRenderTargetHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mNormalMapFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_CLEAR_VALUE normalMapClearValue(mNormalMapFormat, DirectX::Colors::Blue);
    mNormalMap->InitDefaultBuffer(mDevice, D3D12_HEAP_FLAG_NONE, &texDesc, &normalMapClearValue);
	
    texDesc.Width = mRenderTargetWidth >> 1;
    texDesc.Height = mRenderTargetHeight >> 1;
    texDesc.Format = mAmbientMapFormat;

    CD3DX12_CLEAR_VALUE ambientMapClearValue(mAmbientMapFormat, DirectX::Colors::Red);
    mAmbientMap0->InitDefaultBuffer(mDevice, D3D12_HEAP_FLAG_NONE, &texDesc, &ambientMapClearValue);
    mAmbientMap1->InitDefaultBuffer(mDevice, D3D12_HEAP_FLAG_NONE, &texDesc, &ambientMapClearValue);
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

void Carol::SsaoManager::InitRandomVectorMap(ID3D12GraphicsCommandList* cmdList)
{
    mRandomVecMap = make_unique<DefaultBuffer>();
    
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
    mRandomVecMap->InitDefaultBuffer(mDevice, D3D12_HEAP_FLAG_NONE, &texDesc);

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
    mRandomVecMap->CopySubresources(cmdList, &subresource);
}

void Carol::SsaoManager::InitDescriptors(Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE cpuRtv, uint32_t cbvSrvUavDescriptorSize, uint32_t rtvDescriptorSize)
{
    mNormalMapCpuSrv = cpuSrv;
    mDepthMapCpuSrv = cpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mRandomVecCpuSrv = cpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mAmbientMap0CpuSrv = cpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mAmbientMap1CpuSrv = cpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mNormalMapGpuSrv = gpuSrv;
    mDepthMapGpuSrv = gpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mRandomVecGpuSrv = gpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mAmbientMap0GpuSrv = gpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mAmbientMap1GpuSrv = gpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mNormalMapCpuRtv = cpuRtv;
    mAmbientMap0CpuRtv = cpuRtv.Offset(1, rtvDescriptorSize);
    mAmbientMap1CpuRtv = cpuRtv.Offset(1, rtvDescriptorSize);

    RebuildDescriptors(depthStencilBuffer);
}

Carol::DefaultBuffer* Carol::SsaoManager::GetNormalMap()
{
    return mNormalMap.get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::SsaoManager::GetNormalMapRtv()
{
    return mNormalMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::SsaoManager::GetSsaoMapSrv()
{
    return mAmbientMap0GpuSrv;
}

uint32_t Carol::SsaoManager::GetNumSrvs()
{
    return mNumSrvs;
}

uint32_t Carol::SsaoManager::GetNumRtvs()
{
    return mNumRtvs;
}

DXGI_FORMAT Carol::SsaoManager::GetNormalMapFormat()
{
    return mNormalMapFormat;
}

DXGI_FORMAT Carol::SsaoManager::GetAmbientMapFormat()
{
    return mAmbientMapFormat;
}

void Carol::SsaoManager::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
    std::copy(&mOffsets[0], &mOffsets[14], offsets);
}

void Carol::SsaoManager::RebuildDescriptors(Resource* depthStencilBuffer)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = mNormalMapFormat;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    mDevice->CreateShaderResourceView(mNormalMap->Get(), &srvDesc, mNormalMapCpuSrv);

    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    mDevice->CreateShaderResourceView(depthStencilBuffer->Get(), &srvDesc, mDepthMapCpuSrv);

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    mDevice->CreateShaderResourceView(mRandomVecMap->Get(), &srvDesc, mRandomVecCpuSrv);

    srvDesc.Format = mAmbientMapFormat;
    mDevice->CreateShaderResourceView(mAmbientMap0->Get(), &srvDesc, mAmbientMap0CpuSrv);
    mDevice->CreateShaderResourceView(mAmbientMap1->Get(), &srvDesc, mAmbientMap1CpuSrv);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mNormalMapFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
    mDevice->CreateRenderTargetView(mNormalMap->Get(), &rtvDesc, mNormalMapCpuRtv);

    rtvDesc.Format = mAmbientMapFormat;
    mDevice->CreateRenderTargetView(mAmbientMap0->Get(), &rtvDesc, mAmbientMap0CpuRtv);
    mDevice->CreateRenderTargetView(mAmbientMap1->Get(), &rtvDesc, mAmbientMap1CpuRtv);
}

void Carol::SsaoManager::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* frameResource, uint32_t blurCount)
{
    cmdList->SetPipelineState(mSsaoBlurPso);

    for (int i = 0; i < blurCount; ++i)
    {
        BlurAmbientMap(cmdList, true);
        BlurAmbientMap(cmdList, false);
    }
}

void Carol::SsaoManager::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur)
{
    Resource* output;
    CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

    if (horzBlur)
    {
        output = mAmbientMap1.get();
        inputSrv = mAmbientMap0GpuSrv;
        outputRtv = mAmbientMap1CpuRtv;
        cmdList->SetGraphicsRoot32BitConstant(1, 1, 0);
    }
    else
    {
        output = mAmbientMap0.get();
        inputSrv = mAmbientMap1GpuSrv;
        outputRtv = mAmbientMap0CpuRtv;
        cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
    }

    output->TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ClearRenderTargetView(outputRtv, DirectX::Colors::Red, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

    cmdList->SetGraphicsRootDescriptorTable(3, inputSrv);
	
    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(6, 1, 0, 0);

    output->TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
}
