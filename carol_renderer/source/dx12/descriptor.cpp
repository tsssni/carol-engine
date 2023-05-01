#include <global.h>

namespace Carol {
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;	
	using std::make_pair;
	using std::lock_guard;
	using std::mutex;
	using Microsoft::WRL::ComPtr;
}

Carol::DescriptorAllocator::DescriptorAllocator(
	D3D12_DESCRIPTOR_HEAP_TYPE type,
	uint32_t initNumCpuDescriptors,
	uint32_t numGpuDescriptors)
	:mType(type),
	mNumCpuDescriptorsPerHeap(initNumCpuDescriptors),
	mNumGpuDescriptors(numGpuDescriptors)
{
	mDescriptorSize = gDevice->GetDescriptorHandleIncrementSize(type);
	AddCpuDescriptorAllocator();
	InitGpuDescriptorAllocator();
}

Carol::DescriptorAllocator::~DescriptorAllocator()
{
	mCpuDeletedAllocInfoQueue.emplace(make_pair(0, std::move(mCpuDeletedAllocInfo)));
	mGpuDeletedAllocInfoQueue.emplace(make_pair(0, std::move(mGpuDeletedAllocInfo)));

	while (!mCpuDeletedAllocInfoQueue.empty())
	{
		for (auto& info : mCpuDeletedAllocInfoQueue.front().second)
		{
			CpuDelete(info.get());
		}

		mCpuDeletedAllocInfoQueue.pop();
	}

	while (!mGpuDeletedAllocInfoQueue.empty())
	{
		for (auto& info : mGpuDeletedAllocInfoQueue.front().second)
		{
			CpuDelete(info.get());
		}

		mGpuDeletedAllocInfoQueue.pop();
	}
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorAllocator::CpuAllocate(uint32_t numDescriptors)
{
	lock_guard<mutex> lock(mCpuAllocatorMutex);

	BuddyAllocInfo buddyInfo;
	unique_ptr<DescriptorAllocInfo> descInfo;

	for (int i = 0; i < mCpuBuddies.size(); ++i)
	{
		if (mCpuBuddies[i]->Allocate(numDescriptors, buddyInfo))
		{
			descInfo = make_unique<DescriptorAllocInfo>();
			descInfo->StartOffset = mNumCpuDescriptorsPerHeap * i + buddyInfo.PageId;
			descInfo->NumDescriptors = numDescriptors;

			return descInfo;
		}
	}

	AddCpuDescriptorAllocator();

	if (mCpuBuddies.back()->Allocate(numDescriptors, buddyInfo))
	{
		descInfo = make_unique<DescriptorAllocInfo>();
		descInfo->StartOffset = mNumCpuDescriptorsPerHeap * (mCpuBuddies.size() - 1) + buddyInfo.PageId;
		descInfo->NumDescriptors = numDescriptors;
	}

	return descInfo;
}

void Carol::DescriptorAllocator::CpuDeallocate(DescriptorAllocInfo* info)
{
	mCpuDeletedAllocInfo.emplace_back(info);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetCpuHandle(const DescriptorAllocInfo* info, uint32_t offset)const
{
	uint32_t heapId = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t startOffset = info->StartOffset % mNumCpuDescriptorsPerHeap;
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mCpuDescriptorHeaps[heapId]->GetCPUDescriptorHandleForHeapStart(), startOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorAllocator::GpuAllocate(uint32_t numDescriptors)
{
	lock_guard<mutex> lock(mGpuAllocatorMutex);

	BuddyAllocInfo buddyInfo;
	unique_ptr<DescriptorAllocInfo> descInfo;
	
	if (mType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		if (mGpuBuddy->Allocate(numDescriptors, buddyInfo))
		{
			descInfo = make_unique<DescriptorAllocInfo>();
			descInfo->StartOffset = buddyInfo.PageId;
			descInfo->NumDescriptors = numDescriptors;

			return descInfo;
		}
	}

	return descInfo;
}

void Carol::DescriptorAllocator::GpuDeallocate(DescriptorAllocInfo* info)
{
	mGpuDeletedAllocInfo.emplace_back(info);
}

void Carol::DescriptorAllocator::DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	mCpuDeletedAllocInfoQueue.emplace(make_pair(cpuFenceValue, std::move(mCpuDeletedAllocInfo)));
	mGpuDeletedAllocInfoQueue.emplace(make_pair(cpuFenceValue, std::move(mGpuDeletedAllocInfo)));
	
	while (!mCpuDeletedAllocInfoQueue.empty() && mCpuDeletedAllocInfoQueue.front().first <= completedFenceValue)
	{
		for (auto& info : mCpuDeletedAllocInfoQueue.front().second)
		{
			CpuDelete(info.get());
		}

		mCpuDeletedAllocInfoQueue.pop();
	}

	while (!mGpuDeletedAllocInfoQueue.empty() && mGpuDeletedAllocInfoQueue.front().first <= completedFenceValue)
	{
		for (auto& info : mGpuDeletedAllocInfoQueue.front().second)
		{
			GpuDelete(info.get());
		}

		mGpuDeletedAllocInfoQueue.pop();
	}
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

void Carol::DescriptorAllocator::AddCpuDescriptorAllocator()
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

void Carol::DescriptorAllocator::InitGpuDescriptorAllocator()
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

void Carol::DescriptorAllocator::CpuDelete(const DescriptorAllocInfo* info)
{
	lock_guard<mutex> lock(mCpuAllocatorMutex);

	uint32_t buddyIdx = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t blockIdx = info->StartOffset % mNumCpuDescriptorsPerHeap;
	BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

	mCpuBuddies[buddyIdx]->Deallocate(buddyInfo);
}

void Carol::DescriptorAllocator::GpuDelete(const DescriptorAllocInfo* info)
{
	lock_guard<mutex> lock(mGpuAllocatorMutex);

	BuddyAllocInfo buddyInfo(info->StartOffset, info->NumDescriptors);
	mGpuBuddy->Deallocate(buddyInfo);
}

Carol::DescriptorManager::DescriptorManager(uint32_t initCpuCbvSrvUavHeapSize, uint32_t initGpuCbvSrvUavHeapSize, uint32_t initRtvHeapSize, uint32_t initDsvHeapSize)
{
	mCbvSrvUavAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initCpuCbvSrvUavHeapSize, initGpuCbvSrvUavHeapSize);
	mRtvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initRtvHeapSize, 0);
	mDsvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initDsvHeapSize, 0);
}

void Carol::DescriptorManager::DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	mCbvSrvUavAllocator->DelayedDelete(cpuFenceValue, completedFenceValue);
	mRtvAllocator->DelayedDelete(cpuFenceValue, completedFenceValue);
	mDsvAllocator->DelayedDelete(cpuFenceValue, completedFenceValue);
}

Carol::DescriptorManager::DescriptorManager(Carol::DescriptorManager&& manager)
{
	mCbvSrvUavAllocator = std::move(manager.mCbvSrvUavAllocator);
	mRtvAllocator = std::move(manager.mRtvAllocator);
	mDsvAllocator = std::move(manager.mDsvAllocator);
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::CpuCbvSrvUavAllocate(uint32_t numDescriptors)
{
	auto info = mCbvSrvUavAllocator->CpuAllocate(numDescriptors);
	info->Manager = this;

	return info;
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::GpuCbvSrvUavAllocate(uint32_t numDescriptors)
{
	auto info = mCbvSrvUavAllocator->GpuAllocate(numDescriptors);
	info->Manager = this;

	return info;
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::RtvAllocate(uint32_t numDescriptors)
{
	auto info = mRtvAllocator->CpuAllocate(numDescriptors);
	info->Manager = this;

	return info;
}

Carol::unique_ptr<Carol::DescriptorAllocInfo> Carol::DescriptorManager::DsvAllocate(uint32_t numDescriptors)
{
	auto info = mDsvAllocator->CpuAllocate(numDescriptors);
	info->Manager = this;

	return info;
}

void Carol::DescriptorManager::CpuCbvSrvUavDeallocate(DescriptorAllocInfo* info)
{
	if (info && info->Manager == this)
	{
		mCbvSrvUavAllocator->CpuDeallocate(info);
	}
}

void Carol::DescriptorManager::GpuCbvSrvUavDeallocate(DescriptorAllocInfo* info)
{
	if (info && info->Manager == this)
	{
		mCbvSrvUavAllocator->GpuDeallocate(info);
	}
}

void Carol::DescriptorManager::RtvDeallocate(DescriptorAllocInfo* info)
{
	if (info && info->Manager == this)
	{
		mRtvAllocator->CpuDeallocate(info);
	}
}

void Carol::DescriptorManager::DsvDeallocate(DescriptorAllocInfo* info)
{
	if (info && info->Manager == this)
	{
		mDsvAllocator->CpuDeallocate(info);
	}
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