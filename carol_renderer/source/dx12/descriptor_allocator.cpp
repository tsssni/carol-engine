#include <dx12/descriptor_allocator.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>

namespace Carol {
	using std::vector;
	using std::make_unique;	
}

Carol::DescriptorAllocator::DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t initNumCpuDescriptors)
	:mDevice(device), mType(type), mNumCpuDescriptorsPerHeap(initNumCpuDescriptors)
{
	AddCpuDescriptorHeap();
	mDescriptorSize = device->GetDescriptorHandleIncrementSize(type);
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

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetCpuHandle(DescriptorAllocInfo* info)
{
	uint32_t heapId = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t startOffset = info->StartOffset % mNumCpuDescriptorsPerHeap;
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mCpuDescriptorHeaps[heapId]->GetCPUDescriptorHandleForHeapStart(), startOffset, mDescriptorSize);
}

bool Carol::CbvSrvUavDescriptorAllocator::GpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
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

bool Carol::CbvSrvUavDescriptorAllocator::GpuDeallocate(DescriptorAllocInfo* info)
{
	mGpuDeletionInfo[mCurrFrame].push_back(info);
	return true;
}

bool Carol::CbvSrvUavDescriptorAllocator::GpuDelete(DescriptorAllocInfo* info)
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

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::CbvSrvUavDescriptorAllocator::GetShaderCpuHandle(DescriptorAllocInfo* info)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::CbvSrvUavDescriptorAllocator::GetShaderGpuHandle(DescriptorAllocInfo* info)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize);
}

Carol::RtvDescriptorAllocator::RtvDescriptorAllocator(ID3D12Device* device, uint32_t initNumCpuDescriptors)
	:DescriptorAllocator(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initNumCpuDescriptors)
{
}

Carol::DsvDescriptorAllocator::DsvDescriptorAllocator(ID3D12Device* device, uint32_t initNumCpuDescriptors)
	:DescriptorAllocator(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initNumCpuDescriptors)
{
}

Carol::CbvSrvUavDescriptorAllocator::CbvSrvUavDescriptorAllocator(ID3D12Device* device, uint32_t initNumCpuDescriptors, uint32_t initNumGpuDescriptors)
	:DescriptorAllocator(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initNumCpuDescriptors), 
	mNumGpuDescriptorsPerSection(initNumGpuDescriptors)
{
	ExpandGpuDescriptorHeap();
}

Carol::CbvSrvUavDescriptorAllocator::~CbvSrvUavDescriptorAllocator()
{
	for (auto& vec : mGpuDeletionInfo)
	{
		for (auto& info : vec)
		{
			if (info->Allocator == this)
			{
				GpuDeallocate(info);
			}
		}
	}
}

void Carol::CbvSrvUavDescriptorAllocator::SetCurrFrame(uint32_t currFrame)
{
	mCurrFrame = currFrame;

	if (mCurrFrame >= mGpuDeletionInfo.size())
	{
		mGpuDeletionInfo.emplace_back();
	}
}

void Carol::CbvSrvUavDescriptorAllocator::DelayedDelete()
{
	for (auto& info : mGpuDeletionInfo[mCurrFrame])
	{
		if (info->Allocator == this)
		{
			GpuDelete(info);
		}
	}

	mGpuDeletionInfo[mCurrFrame].clear();
}

uint32_t Carol::DescriptorAllocator::GetDescriptorSize()
{
	return mDescriptorSize;
}

ID3D12DescriptorHeap* Carol::CbvSrvUavDescriptorAllocator::GetGpuDescriptorHeap()
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

void Carol::CbvSrvUavDescriptorAllocator::ExpandGpuDescriptorHeap()
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