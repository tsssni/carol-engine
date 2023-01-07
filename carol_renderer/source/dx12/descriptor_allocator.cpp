#include <dx12/descriptor_allocator.h>
#include <dx12/resource.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>

namespace Carol {
	using std::vector;
	using std::make_unique;	
}

Carol::DescriptorAllocator::DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t initNumCpuDescriptors, uint32_t initNumGpuDescriptors)
	:mDevice(device), mType(type), mNumCpuDescriptorsPerHeap(initNumCpuDescriptors), mNumGpuDescriptorsPerSection(initNumGpuDescriptors)
{
	mDescriptorSize = device->GetDescriptorHandleIncrementSize(type);
	AddCpuDescriptorHeap();

	if (mType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		ExpandGpuDescriptorHeap();
	}
}

Carol::DescriptorAllocator::~DescriptorAllocator()
{
}

bool Carol::DescriptorAllocator::CpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	if (numDescriptors == 0)
	{
		return true;
	}

	BuddyAllocInfo buddyInfo;

	for (int i = 0; i < mCpuBuddies.size(); ++i)
	{
		if (mCpuBuddies[i]->Allocate(numDescriptors, buddyInfo))
		{
			info->Allocator = this;
			info->StartOffset = mNumCpuDescriptorsPerHeap * i + buddyInfo.PageId;
			info->NumDescriptors = numDescriptors;

			return true;
		}
	}

	AddCpuDescriptorHeap();

	if (mCpuBuddies.back()->Allocate(numDescriptors, buddyInfo))
	{
		info->Allocator = this;
		info->StartOffset = mNumCpuDescriptorsPerHeap * (mCpuBuddies.size() - 1) + buddyInfo.PageId;
		info->NumDescriptors = numDescriptors;

		return true;
	}

	return false;
}

bool Carol::DescriptorAllocator::CpuDeallocate(DescriptorAllocInfo* info)
{
	if (info->NumDescriptors == 0) 
	{
		return true;
	}

	uint32_t buddyIdx = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t blockIdx = info->StartOffset % mNumCpuDescriptorsPerHeap;
	BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

	if (mCpuBuddies[buddyIdx]->Deallocate(buddyInfo))
	{
		info->Allocator = nullptr;
		info->NumDescriptors = 0;
		info->StartOffset = 0;

		return true;
	}
	
	return false;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetCpuHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	uint32_t heapId = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t startOffset = info->StartOffset % mNumCpuDescriptorsPerHeap;
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mCpuDescriptorHeaps[heapId]->GetCPUDescriptorHandleForHeapStart(), startOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

bool Carol::DescriptorAllocator::GpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	if (numDescriptors == 0)
	{
		return true;
	}

	if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		return false;
	}

	BuddyAllocInfo buddyInfo;

	for (int i = 0; i < mGpuBuddies.size(); ++i)
	{
		if (mGpuBuddies[i]->Allocate(numDescriptors, buddyInfo))
		{
			info->Allocator = this;
			info->StartOffset = mNumGpuDescriptorsPerSection * i + buddyInfo.PageId;
			info->NumDescriptors = numDescriptors;

			return true;
		}
	}
	
	ExpandGpuDescriptorHeap();
	return false;
}

bool Carol::DescriptorAllocator::GpuDeallocate(DescriptorAllocInfo* info)
{
	if (info->NumDescriptors == 0)
	{
		return true;
	}

	uint32_t buddyIdx = info->StartOffset / mNumGpuDescriptorsPerSection;
	uint32_t blockIdx = info->StartOffset % mNumGpuDescriptorsPerSection;
	BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

	if (mGpuBuddies[buddyIdx]->Deallocate(buddyInfo))
	{
		info->Allocator = nullptr;
		info->NumDescriptors = 0;
		info->StartOffset = 0;

		return true;
	}
	
	return false;
}

void Carol::DescriptorAllocator::CreateSrv(D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	mDevice->CreateShaderResourceView(resource->Get(), srvDesc, GetCpuHandle(info, offset));
}

void Carol::DescriptorAllocator::CreateUav(D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, Resource* resource, Resource* counterResource, DescriptorAllocInfo* info, uint32_t offset)
{
	mDevice->CreateUnorderedAccessView(resource->Get(), counterResource ? counterResource->Get() : nullptr, uavDesc, GetCpuHandle(info, offset));
}

void Carol::DescriptorAllocator::CreateRtv(D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	mDevice->CreateRenderTargetView(resource->Get(), rtvDesc, GetCpuHandle(info, offset));
}

void Carol::DescriptorAllocator::CreateDsv(D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	mDevice->CreateDepthStencilView(resource->Get(), dsvDesc, GetCpuHandle(info, offset));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetShaderCpuHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetGpuHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

void Carol::DescriptorAllocator::CopyDescriptors(DescriptorAllocInfo* cpuInfo, DescriptorAllocInfo* shaderCpuInfo)
{
	if (cpuInfo->Allocator == this && shaderCpuInfo->Allocator == this)
	{
		mDevice->CopyDescriptorsSimple(cpuInfo->NumDescriptors, GetShaderCpuHandle(shaderCpuInfo), GetCpuHandle(cpuInfo), mType);
	}
}

uint32_t Carol::DescriptorAllocator::GetDescriptorSize()
{
	return mDescriptorSize;
}

ID3D12DescriptorHeap* Carol::DescriptorAllocator::GetGpuDescriptorHeap()
{
	return mGpuDescriptorHeap.Get();
}

void Carol::DescriptorAllocator::AddCpuDescriptorHeap()
{
	mCpuDescriptorHeaps.emplace_back();
	mCpuBuddies.emplace_back(make_unique<Buddy>(mNumCpuDescriptorsPerHeap, 1u));

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Type = mType;
	descHeapDesc.NumDescriptors = mNumCpuDescriptorsPerHeap;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descHeapDesc.NodeMask = 0;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mCpuDescriptorHeaps.back().GetAddressOf())));
}

void Carol::DescriptorAllocator::ExpandGpuDescriptorHeap()
{
	mNumGpuDescriptors = (mNumGpuDescriptors == 0) ? mNumGpuDescriptorsPerSection : mNumGpuDescriptors * 2;
	mGpuBuddies.resize(0);

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Type = mType;
	descHeapDesc.NumDescriptors = mNumGpuDescriptors;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mGpuDescriptorHeap.GetAddressOf())));

	for (int i = 0; i < mNumGpuDescriptors / mNumGpuDescriptorsPerSection; ++i)
	{
		mGpuBuddies.emplace_back(make_unique<Buddy>(mNumGpuDescriptorsPerSection, 1u));
	}
}