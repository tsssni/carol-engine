#include <dx12/descriptor.h>
#include <dx12/resource.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <global.h>

namespace Carol {
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;	
	using Microsoft::WRL::ComPtr;
}

Carol::DescriptorAllocator::DescriptorAllocator(
	D3D12_DESCRIPTOR_HEAP_TYPE type,
	uint32_t initNumCpuDescriptors,
	uint32_t numGpuDescriptors)
	:mType(type),
	mNumCpuDescriptorsPerHeap(initNumCpuDescriptors),
	mNumGpuDescriptors(numGpuDescriptors),
	mCpuDeletedAllocInfo(gNumFrame),
	mGpuDeletedAllocInfo(gNumFrame)
{
	mDescriptorSize = gDevice->GetDescriptorHandleIncrementSize(type);
	AddCpuDescriptorHeap();
	InitGpuDescriptorHeap();
}

Carol::DescriptorAllocator::~DescriptorAllocator()
{
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorAllocator::CpuAllocate(uint32_t numDescriptors)
{
	BuddyAllocInfo buddyInfo;
	unique_ptr<DescriptorAllocInfo> descInfo;

	for (int i = 0; i < mCpuBuddies.size(); ++i)
	{
		if (mCpuBuddies[i]->Allocate(numDescriptors, buddyInfo))
		{
			descInfo = make_unique<DescriptorAllocInfo>();
			descInfo->Allocator = this;
			descInfo->StartOffset = mNumCpuDescriptorsPerHeap * i + buddyInfo.PageId;
			descInfo->NumDescriptors = numDescriptors;

			return descInfo;
		}
	}

	AddCpuDescriptorHeap();

	if (mCpuBuddies.back()->Allocate(numDescriptors, buddyInfo))
	{
		descInfo = make_unique<DescriptorAllocInfo>();
		descInfo->Allocator = this;
		descInfo->StartOffset = mNumCpuDescriptorsPerHeap * (mCpuBuddies.size() - 1) + buddyInfo.PageId;
		descInfo->NumDescriptors = numDescriptors;
	}

	return descInfo;
}

void Carol::DescriptorAllocator::CpuDeallocate(unique_ptr<DescriptorAllocInfo> info)
{
	mCpuDeletedAllocInfo[mCurrFrame].emplace_back(std::move(info));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetCpuHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	uint32_t heapId = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t startOffset = info->StartOffset % mNumCpuDescriptorsPerHeap;
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mCpuDescriptorHeaps[heapId]->GetCPUDescriptorHandleForHeapStart(), startOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorAllocator::GpuAllocate(uint32_t numDescriptors)
{
	BuddyAllocInfo buddyInfo;
	unique_ptr<DescriptorAllocInfo> descInfo;
	
	if (mType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		if (mGpuBuddy->Allocate(numDescriptors, buddyInfo))
		{
			descInfo = make_unique<DescriptorAllocInfo>();
			descInfo->Allocator = this;
			descInfo->StartOffset = buddyInfo.PageId;
			descInfo->NumDescriptors = numDescriptors;

			return descInfo;
		}
	}

	return descInfo;
}

void Carol::DescriptorAllocator::GpuDeallocate(std::unique_ptr<DescriptorAllocInfo> info)
{
	mGpuDeletedAllocInfo[mCurrFrame].emplace_back(std::move(info));
}

void Carol::DescriptorAllocator::DelayedDelete()
{
	for (auto& info : mCpuDeletedAllocInfo[gCurrFrame])
	{
		CpuDelete(std::move(info));
	}

	for (auto& info : mGpuDeletedAllocInfo[gCurrFrame])
	{
		GpuDelete(std::move(info));
	}

	mCpuDeletedAllocInfo[gCurrFrame].clear();
	mGpuDeletedAllocInfo[gCurrFrame].clear();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetShaderCpuHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetGpuHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

uint32_t Carol::DescriptorAllocator::GetDescriptorSize()const
{
	return mDescriptorSize;
}

ID3D12DescriptorHeap* Carol::DescriptorAllocator::GetGpuDescriptorHeap()const
{
	return mGpuDescriptorHeap.Get();
}

void Carol::DescriptorAllocator::AddCpuDescriptorHeap()
{
	mCpuDescriptorHeaps.emplace_back();
	mCpuBuddies.emplace_back(make_unique<Buddy>(mNumCpuDescriptorsPerHeap, 1u));

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc;
	descHeapDesc.Type = mType;
	descHeapDesc.NumDescriptors = mNumCpuDescriptorsPerHeap;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descHeapDesc.NodeMask = 0;

	ThrowIfFailed(gDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mCpuDescriptorHeaps.back().GetAddressOf())));
}

void Carol::DescriptorAllocator::InitGpuDescriptorHeap()
{
	if (mType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		mGpuBuddy = make_unique<Buddy>(mNumGpuDescriptors, 1u);

		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc;
		descHeapDesc.Type = mType;
		descHeapDesc.NumDescriptors = mNumGpuDescriptors;
		descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descHeapDesc.NodeMask = 0;

		ThrowIfFailed(gDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mGpuDescriptorHeap.GetAddressOf())));
	}
}

void Carol::DescriptorAllocator::CpuDelete(unique_ptr<DescriptorAllocInfo> info)
{
	if (info)
	{
		uint32_t buddyIdx = info->StartOffset / mNumCpuDescriptorsPerHeap;
		uint32_t blockIdx = info->StartOffset % mNumCpuDescriptorsPerHeap;
		BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

		mCpuBuddies[buddyIdx]->Deallocate(buddyInfo);
	}
}

void Carol::DescriptorAllocator::GpuDelete(std::unique_ptr<DescriptorAllocInfo> info)
{
	if (info)
	{
		BuddyAllocInfo buddyInfo(info->StartOffset, info->NumDescriptors);
		mGpuBuddy->Deallocate(buddyInfo);
	}
}

Carol::DescriptorManager::DescriptorManager(uint32_t initCpuCbvSrvUavHeapSize, uint32_t initGpuCbvSrvUavHeapSize, uint32_t initRtvHeapSize, uint32_t initDsvHeapSize)
{
	mCbvSrvUavAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initCpuCbvSrvUavHeapSize, initGpuCbvSrvUavHeapSize);
	mRtvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initRtvHeapSize, 0);
	mDsvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initDsvHeapSize, 0);
}

void Carol::DescriptorManager::DelayedDelete()
{
	mCbvSrvUavAllocator->DelayedDelete();
	mRtvAllocator->DelayedDelete();
	mDsvAllocator->DelayedDelete();
}

Carol::DescriptorManager::DescriptorManager(Carol::DescriptorManager&& manager)
{
	mCbvSrvUavAllocator = std::move(manager.mCbvSrvUavAllocator);
	mRtvAllocator = std::move(manager.mRtvAllocator);
	mDsvAllocator = std::move(manager.mDsvAllocator);
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::CpuCbvSrvUavAllocate(uint32_t numDescriptors)
{
	return mCbvSrvUavAllocator->CpuAllocate(numDescriptors);
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::GpuCbvSrvUavAllocate(uint32_t numDescriptors)
{
	return mCbvSrvUavAllocator->GpuAllocate(numDescriptors);
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::RtvAllocate(uint32_t numDescriptors)
{
	return mRtvAllocator->CpuAllocate(numDescriptors);
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::DsvAllocate(uint32_t numDescriptors)
{
	return mDsvAllocator->CpuAllocate(numDescriptors);
}

void Carol::DescriptorManager::CpuCbvSrvUavDeallocate(std::unique_ptr<DescriptorAllocInfo> info)
{
	mCbvSrvUavAllocator->CpuDeallocate(std::move(info));
}

void Carol::DescriptorManager::GpuCbvSrvUavDeallocate(std::unique_ptr<DescriptorAllocInfo> info)
{
	mCbvSrvUavAllocator->GpuDeallocate(std::move(info));
}

void Carol::DescriptorManager::RtvDeallocate(std::unique_ptr<DescriptorAllocInfo> info)
{
	mRtvAllocator->CpuDeallocate(std::move(info));
}

void Carol::DescriptorManager::DsvDeallocate(std::unique_ptr<DescriptorAllocInfo> info)
{
	mDsvAllocator->CpuDeallocate(std::move(info));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetCpuCbvSrvUavHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	return mCbvSrvUavAllocator->GetCpuHandle(info, offset);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetShaderCpuCbvSrvUavHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	return mCbvSrvUavAllocator->GetShaderCpuHandle(info, offset);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetGpuCbvSrvUavHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	return mCbvSrvUavAllocator->GetGpuHandle(info, offset);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetRtvHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	return mRtvAllocator->GetCpuHandle(info, offset);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetDsvHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	return mDsvAllocator->GetCpuHandle(info, offset);
}

uint32_t Carol::DescriptorManager::GetCbvSrvUavSize()const
{
	return mCbvSrvUavAllocator->GetDescriptorSize();
}

uint32_t Carol::DescriptorManager::GetRtvSize()const
{
	return mRtvAllocator->GetDescriptorSize();
}

uint32_t Carol::DescriptorManager::GetDsvSize()const
{
	return mDsvAllocator->GetDescriptorSize();
}

ID3D12DescriptorHeap* Carol::DescriptorManager::GetResourceDescriptorHeap()const
{
	return mCbvSrvUavAllocator->GetGpuDescriptorHeap();
}