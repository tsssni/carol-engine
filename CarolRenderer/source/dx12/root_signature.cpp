#include <dx12/root_signature.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>

namespace Carol {
    using std::make_unique;
}

Carol::RootSignature::RootSignature(ID3D12Device* device, DescriptorAllocator* allocator)
    :mDevice(device),
     mAllocator(allocator), 
     mTex1DAllocInfo(make_unique<DescriptorAllocInfo>()),
     mTex2DAllocInfo(make_unique<DescriptorAllocInfo>()),
     mTex3DAllocInfo(make_unique<DescriptorAllocInfo>()),
     mTexCubeAllocInfo(make_unique<DescriptorAllocInfo>()),
     mTex1DCount(0),
     mTex2DCount(0),
     mTex3DCount(0),
     mTexCubeCount(0)
{
    Shader rootSignatureShader(L"shader\\include\\root_signature_compile.hlsli", {}, L"main", L"vs_6_5");
    ThrowIfFailed(mDevice->CreateRootSignature(0, rootSignatureShader.GetBufferPointer(), rootSignatureShader.GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

Carol::RootSignature::~RootSignature()
{
	 if (mTex1DAllocInfo->Allocator)
    {
        mTex1DAllocInfo->Allocator->GpuDeallocate(mTex1DAllocInfo.get());
    }

	if (mTex2DAllocInfo->Allocator)
    {
        mTex2DAllocInfo->Allocator->GpuDeallocate(mTex2DAllocInfo.get());
    }

	if (mTex3DAllocInfo->Allocator)
    {
        mTex3DAllocInfo->Allocator->GpuDeallocate(mTex3DAllocInfo.get());
    }

    if (mTexCubeAllocInfo->Allocator)
    {
        mTexCubeAllocInfo->Allocator->GpuDeallocate(mTexCubeAllocInfo.get());
    }
}

void Carol::RootSignature::ClearAllocInfo()
{
    mTex1DInfos.clear();
    mTex2DInfos.clear();
    mTex3DInfos.clear();
    mTexCubeInfos.clear();

    if (mTex1DAllocInfo->Allocator)
    {
        mTex1DAllocInfo->Allocator->GpuDeallocate(mTex1DAllocInfo.get());
    }

	if (mTex2DAllocInfo->Allocator)
    {
        mTex2DAllocInfo->Allocator->GpuDeallocate(mTex2DAllocInfo.get());
    }

	if (mTex3DAllocInfo->Allocator)
    {
        mTex3DAllocInfo->Allocator->GpuDeallocate(mTex3DAllocInfo.get());
    }

	if (mTexCubeAllocInfo->Allocator)
    {
        mTexCubeAllocInfo->Allocator->GpuDeallocate(mTexCubeAllocInfo.get());
    }

    mTex1DCount = 0;
    mTex2DCount = 0;
    mTex3DCount = 0;
    mTexCubeCount = 0;
}

ID3D12RootSignature* Carol::RootSignature::Get()
{
    return mRootSignature.Get();
}

uint32_t Carol::RootSignature::AllocateTex1D(DescriptorAllocInfo* info)
{
    if (info->NumDescriptors == 0)
    {
        return -1;
    }

    mTex1DInfos.push_back(info);
    int temp = mTex1DCount;

    mTex1DCount += info->NumDescriptors;
    return temp;
}

uint32_t Carol::RootSignature::AllocateTex2D(DescriptorAllocInfo* info)
{
    if (info->NumDescriptors == 0)
    {
        return -1;
    }

    mTex2DInfos.push_back(info);
    int temp = mTex2DCount;

    mTex2DCount += info->NumDescriptors;
    return temp;
}

uint32_t Carol::RootSignature::AllocateTex3D(DescriptorAllocInfo* info)
{
    if (info->NumDescriptors == 0)
    {
        return -1;
    }

	mTex3DInfos.push_back(info);
    int temp = mTex3DCount;

    mTex3DCount += info->NumDescriptors;
    return temp;
}

uint32_t Carol::RootSignature::AllocateTexCube(DescriptorAllocInfo* info)
{
	mTexCubeInfos.push_back(info);
    int temp = mTexCubeCount;

    mTexCubeCount += info->NumDescriptors;
    return temp;
}

void Carol::RootSignature::AllocateTex1DBundle()
{
    if (mTex1DAllocInfo)
    {
        mAllocator->GpuDeallocate(mTex1DAllocInfo.get());
    }

    int total = CountNumDescriptors(mTex1DInfos);
    mAllocator->GpuAllocate(total, mTex1DAllocInfo.get());
    CopyDescriptors(mTex1DAllocInfo.get(), mTex1DInfos);
}

void Carol::RootSignature::AllocateTex2DBundle()
{
    if (mTex2DAllocInfo)
    {
        mAllocator->GpuDeallocate(mTex2DAllocInfo.get());
    }

    int total = CountNumDescriptors(mTex2DInfos);
    mAllocator->GpuAllocate(total, mTex2DAllocInfo.get());
    CopyDescriptors(mTex2DAllocInfo.get(), mTex2DInfos);
}

void Carol::RootSignature::AllocateTex3DBundle()
{
    if (mTex3DAllocInfo)
    {
        mAllocator->GpuDeallocate(mTex3DAllocInfo.get());
    }

    int total = CountNumDescriptors(mTex3DInfos);
    mAllocator->GpuAllocate(total, mTex3DAllocInfo.get());
    CopyDescriptors(mTex3DAllocInfo.get(), mTex3DInfos);
}

void Carol::RootSignature::AllocateTexCubeBundle()
{
    if (mTexCubeAllocInfo)
    {
        mAllocator->GpuDeallocate(mTexCubeAllocInfo.get());
    }

    int total = CountNumDescriptors(mTexCubeInfos);
    mAllocator->GpuAllocate(total, mTexCubeAllocInfo.get());
    CopyDescriptors(mTexCubeAllocInfo.get(), mTexCubeInfos);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RootSignature::GetTex1DBundleHandle()
{
    return mAllocator->GetShaderGpuHandle(mTex1DAllocInfo.get());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RootSignature::GetTex2DBundleHandle()
{
    return mAllocator->GetShaderGpuHandle(mTex2DAllocInfo.get());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RootSignature::GetTex3DBundleHandle()
{
    return mAllocator->GetShaderGpuHandle(mTex3DAllocInfo.get());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RootSignature::GetTexCubeBundleHandle()
{
    return mAllocator->GetShaderGpuHandle(mTexCubeAllocInfo.get());
}

uint32_t Carol::RootSignature::CountNumDescriptors(std::vector<DescriptorAllocInfo*>& infos)
{
    uint32_t total = 0;

    for (auto& info : infos)
    {
        total += info->NumDescriptors;
    }

    return total;
}

void Carol::RootSignature::CopyDescriptors(DescriptorAllocInfo* texInfo, std::vector<DescriptorAllocInfo*>& infos)
{
    auto gpuHandle = mAllocator->GetShaderCpuHandle(texInfo);

    for (auto& info : infos)
    {
        auto cpuHandle = mAllocator->GetCpuHandle(info);
        mDevice->CopyDescriptorsSimple(info->NumDescriptors, gpuHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        gpuHandle.Offset(info->NumDescriptors * mAllocator->GetDescriptorSize());
    }
}

