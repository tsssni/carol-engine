#include <dx12/heap.h>
#include <utils/d3dx12.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <global.h>
#include <assert.h>
#include <cmath>

namespace Carol {
    using std::unique_ptr;
    using std::make_unique;
    using std::make_pair;
    using std::mutex;
    using std::lock_guard;
    using Microsoft::WRL::ComPtr;
}

Carol::Heap::Heap(D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag)
    :mType(type), mFlag(flag)
{
  
}

void Carol::Heap::Deallocate(HeapAllocInfo* info)
{
    if (info && info->Heap == this)
    {
        mDeletedResources.emplace_back(info);
    }
}

void Carol::Heap::DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
    mDeletedResourcesQueue.emplace(make_pair(cpuFenceValue, std::move(mDeletedResources)));
	
	while (!mDeletedResourcesQueue.empty() && mDeletedResourcesQueue.front().first <= completedFenceValue)
	{
		for (auto& info : mDeletedResourcesQueue.front().second)
		{
			Delete(info.get());
		}

		mDeletedResourcesQueue.pop();
	}
}


Carol::BuddyHeap::BuddyHeap(
    D3D12_HEAP_TYPE type,
    D3D12_HEAP_FLAGS flag,
    ID3D12Device* device,
    uint32_t heapSize)
    :Heap(type,flag), mHeapSize(heapSize)
{
    Align();
    AddHeap(device);
}

Carol::BuddyHeap::~BuddyHeap()
{
    mDeletedResourcesQueue.emplace(make_pair(0, std::move(mDeletedResources)));

	while (!mDeletedResourcesQueue.empty())
	{
		for (auto& info : mDeletedResourcesQueue.front().second)
		{
			Delete(info.get());
		}

		mDeletedResourcesQueue.pop();
	}
}

Carol::unique_ptr<Carol::HeapAllocInfo> Carol::BuddyHeap::Allocate(const D3D12_RESOURCE_DESC* desc)
{
    lock_guard<mutex> lock(mAllocatorMutex);

    ComPtr<ID3D12Device> device;
    mHeaps.back()->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));
    uint32_t size = device->GetResourceAllocationInfo(0, 1, desc).SizeInBytes;

    BuddyAllocInfo buddyInfo;
    unique_ptr<HeapAllocInfo> heapInfo;

    for (int i = 0; i < mBuddies.size(); ++i)
    {
        if (mBuddies[i]->Allocate(size, buddyInfo))
        {
            heapInfo = make_unique<HeapAllocInfo>();
            heapInfo->Heap = this;
            heapInfo->Bytes = buddyInfo.NumPages * mPageSize;
            heapInfo->Addr = i * mHeapSize + buddyInfo.PageId * mPageSize;
            
            return heapInfo;
        }
    }

	AddHeap(device.Get());

	if (mBuddies.back()->Allocate(size, buddyInfo))
	{
		heapInfo = make_unique<HeapAllocInfo>();
        heapInfo->Heap = this;
		heapInfo->Bytes = buddyInfo.NumPages * mPageSize;
		heapInfo->Addr = (mBuddies.size() - 1) * mHeapSize + buddyInfo.PageId * mPageSize;
	}

    return heapInfo;
}

ID3D12Heap* Carol::BuddyHeap::GetHeap(const HeapAllocInfo* info)const
{
    return mHeaps[info->Addr / mHeapSize].Get();
}

uint32_t Carol::BuddyHeap::GetOffset(const HeapAllocInfo* info)const
{
    return info->Addr % mHeapSize;
}

void Carol::BuddyHeap::Delete(const HeapAllocInfo* info)
{
    lock_guard<mutex> lock(mAllocatorMutex);

	uint32_t buddyIdx = info->Addr / mHeapSize;
	uint32_t blockIdx = (info->Addr % mHeapSize) / mPageSize;
	uint32_t numBlocks = info->Bytes / mPageSize;
	BuddyAllocInfo buddyInfo(blockIdx, numBlocks);

	mBuddies[buddyIdx]->Deallocate(buddyInfo);
}

void Carol::BuddyHeap::Align()
{
    mPageSize = (~65535) & (mPageSize + 65535);

    if (mHeapSize % mPageSize)
    {
        mHeapSize = (mHeapSize / mPageSize + 1) * mPageSize;
    }

    mNumPages = mHeapSize / mPageSize;
}

void Carol::BuddyHeap::AddHeap(ID3D12Device* device)
{
    mHeaps.emplace_back();
    mBuddies.emplace_back(make_unique<Buddy>(mHeapSize,mPageSize));

    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = mHeapSize;
    heapDesc.Properties = CD3DX12_HEAP_PROPERTIES(mType);
    heapDesc.Alignment = 0;
    heapDesc.Flags = mFlag;

    device->CreateHeap(&heapDesc, IID_PPV_ARGS(mHeaps.back().GetAddressOf()));
}

Carol::SegListHeap::SegListHeap(
    D3D12_HEAP_TYPE type,
    D3D12_HEAP_FLAGS flag,
    ID3D12Device* device,
    uint32_t maxPageSize)
    :Heap(type, flag),mOrder(GetOrder(maxPageSize))
{
    mSegLists.resize(mOrder + 1);
    mBitsets.resize(mOrder + 1);

    for (int i = 0; i <= mOrder; ++i)
    {
        AddHeap(i, device);
    }
}

Carol::SegListHeap::~SegListHeap()
{
    mDeletedResourcesQueue.emplace(make_pair(0, std::move(mDeletedResources)));

	while (!mDeletedResourcesQueue.empty())
	{
		for (auto& info : mDeletedResourcesQueue.front().second)
		{
			Delete(info.get());
		}

		mDeletedResourcesQueue.pop();
	}
}

Carol::unique_ptr<Carol::HeapAllocInfo> Carol::SegListHeap::Allocate(const D3D12_RESOURCE_DESC* desc)
{
    lock_guard<mutex> lock(mAllocatorMutex);

    ComPtr<ID3D12Device> device;
    mSegLists.back().back()->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

    uint32_t size = device->GetResourceAllocationInfo(0, 1, desc).SizeInBytes;
    unique_ptr<HeapAllocInfo> heapInfo;

    if (size > mPageSize << mOrder)
    {
        return heapInfo;
    }

    auto order = GetOrder(size);
    auto orderNumPages = 1 << (mOrder - order);

    for (int i = 0; i < mBitsets[order].size(); ++i)
    {
        for (int j = 0; j < orderNumPages; ++j)
        {
            if (mBitsets[order][i]->IsPageIdle(j))
            {
                heapInfo = make_unique<HeapAllocInfo>();
                heapInfo->Heap = this;
                heapInfo->Addr = (i * orderNumPages + j) * (mPageSize << order);
                heapInfo->Bytes = mPageSize << order;
                mBitsets[order][i]->Set(j);

                return heapInfo;
            }
        }
    }

    AddHeap(order, device.Get());
    heapInfo = make_unique<HeapAllocInfo>();
    heapInfo->Heap = this;
    heapInfo->Addr = (mSegLists[order].size() - 1) * orderNumPages * (mPageSize << order);
    heapInfo->Bytes = mPageSize << order;
    mBitsets[order].back()->Set(0);

    return heapInfo;
}

ID3D12Heap* Carol::SegListHeap::GetHeap(const HeapAllocInfo* info)const
{
    uint32_t order = GetOrder(info->Bytes); 
    uint32_t orderNumPages = 1 << (mOrder - order);
    uint32_t heapIdx = info->Addr / (orderNumPages * (mPageSize << order));

    return mSegLists[order][heapIdx].Get();
}

uint32_t Carol::SegListHeap::GetOffset(const HeapAllocInfo* info)const
{
    uint32_t order = GetOrder(info->Bytes); 
    uint32_t orderNumPages = 1 << (mOrder - order);

    return info->Addr % (orderNumPages * (mPageSize << order));
}

void Carol::SegListHeap::Delete(const HeapAllocInfo* info)
{
    lock_guard<mutex> lock(mAllocatorMutex);

	auto order = GetOrder(info->Bytes);
	auto orderNumPages = 1 << (mOrder - order);

	auto heapIdx = info->Addr / (orderNumPages * mPageSize * (1u << order));
	auto pageIdx = ((info->Addr) % (orderNumPages * mPageSize * (1u << order))) / (1u << order);
	mBitsets[order][heapIdx]->Reset(pageIdx);
}

uint32_t Carol::SegListHeap::GetOrder(uint32_t size)const
{
    size = std::ceil(size * 1.0f / mPageSize);
	return std::ceil(std::log2(size));
}

void Carol::SegListHeap::AddHeap(uint32_t order, ID3D12Device* device)
{
    mSegLists[order].emplace_back();
    mBitsets[order].emplace_back(make_unique<Bitset>(1 << (mOrder - order)));

    D3D12_HEAP_DESC desc = {};
    desc.SizeInBytes = (1 << (mOrder - order)) * (mPageSize << order);
    desc.Properties = CD3DX12_HEAP_PROPERTIES(mType);
    desc.Alignment = 0;
    desc.Flags = mFlag;

    ThrowIfFailed(
        device->CreateHeap(
        &desc,
        IID_PPV_ARGS(mSegLists[order].back().GetAddressOf())
    ));
}

Carol::HeapManager::HeapManager(
    ID3D12Device* device,
    uint32_t initDefaultBuffersHeapSize,
    uint32_t initUploadBuffersHeapSize,
    uint32_t initReadbackBuffersHeapSize,
    uint32_t texturesMaxPageSize)
{
    mDefaultBuffersHeap = make_unique<BuddyHeap>(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, device, initDefaultBuffersHeapSize);
    mUploadBuffersHeap = make_unique<BuddyHeap>(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, device, initDefaultBuffersHeapSize);
    mReadbackBuffersHeap = make_unique<BuddyHeap>(D3D12_HEAP_TYPE_READBACK, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, device, initDefaultBuffersHeapSize);
    mTexturesHeap = make_unique<SegListHeap>(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, device, texturesMaxPageSize);
}

Carol::Heap* Carol::HeapManager::GetDefaultBuffersHeap()
{
    return mDefaultBuffersHeap.get();
}

Carol::Heap* Carol::HeapManager::GetUploadBuffersHeap()
{
    return mUploadBuffersHeap.get();
}

Carol::Heap* Carol::HeapManager::GetReadbackBuffersHeap()
{
    return mReadbackBuffersHeap.get();
}

Carol::Heap* Carol::HeapManager::GetTexturesHeap()
{
    return mTexturesHeap.get();
}

void Carol::HeapManager::DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
    mDefaultBuffersHeap->DelayedDelete(cpuFenceValue, completedFenceValue);
    mUploadBuffersHeap->DelayedDelete(cpuFenceValue, completedFenceValue);
    mReadbackBuffersHeap->DelayedDelete(cpuFenceValue, completedFenceValue);
    mTexturesHeap->DelayedDelete(cpuFenceValue, completedFenceValue);
}
