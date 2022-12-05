#include "Heap.h"
#include "../Utils/d3dx12.h"
#include "../Utils/Bitset.h"
#include "../Utils/Common.h"
#include <assert.h>
#include <cmath>

using std::make_unique;

Carol::Heap::Heap(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag)
    :mDevice(device), mType(type), mFlag(flag)
{
  
}


Carol::BuddyHeap::BuddyHeap(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag, uint32_t heapSize, uint32_t pageSize)
    :Heap(device,type,flag), mHeapSize(heapSize), mPageSize(pageSize)
{
    Align();
    AddHeap();
}

bool Carol::BuddyHeap::Allocate(uint32_t size, HeapAllocInfo* info)
{
    if (size <= 0 || size > mHeapSize)
    {
        return false;
    }

    BuddyAllocInfo buddyInfo;

    for (int i = 0; i < mBuddies.size(); ++i)
    {
        if (mBuddies[i]->Allocate(size, buddyInfo))
        {
            info->Heap = this;
            info->Bytes = buddyInfo.NumPages * mPageSize;
            info->Addr = i * mHeapSize + buddyInfo.PageId * mPageSize;

            return true;
        }
    }

	AddHeap();

	if (mBuddies.back()->Allocate(size, buddyInfo))
	{
        info->Heap = this;
		info->Bytes = buddyInfo.NumPages * mPageSize;
		info->Addr = (mBuddies.size() - 1) * mHeapSize + buddyInfo.PageId * mPageSize;

		return true;
	}


    return false;
}

bool Carol::BuddyHeap::Deallocate(HeapAllocInfo* info)
{
    if (info->Bytes <= 0 || info->Bytes > mHeapSize)
    {
        return false;
    }

    uint32_t buddyIdx = info->Addr / mHeapSize;
    uint32_t blockIdx = (info->Addr % mHeapSize) / mPageSize;
    uint32_t numBlocks = info->Bytes / mPageSize;
    BuddyAllocInfo buddyInfo(blockIdx, numBlocks);

    if (mBuddies[buddyIdx]->Deallocate(buddyInfo))
    {
        info->Heap = nullptr;
        info->Bytes = 0;
		info->Addr = 0;

        return true;
    }
    
    return false;
}

void Carol::BuddyHeap::CreateResource(ComPtr<ID3D12Resource>* resource, D3D12_RESOURCE_DESC* desc, HeapAllocInfo* info, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
{
    Allocate(mDevice->GetResourceAllocationInfo(0, 1, desc).SizeInBytes, info);

    ThrowIfFailed(mDevice->CreatePlacedResource(
        mHeaps[info->Addr / mHeapSize].Get(),
        info->Addr % mHeapSize,
        desc,
        initState,
        optimizedClearValue,
        IID_PPV_ARGS(resource->GetAddressOf())
    ));
}

void Carol::BuddyHeap::DeleteResource(HeapAllocInfo* info)
{
    Deallocate(info);
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

void Carol::BuddyHeap::AddHeap()
{
    mHeaps.emplace_back();
    mBuddies.emplace_back(make_unique<Buddy>(mHeapSize,mPageSize));

    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = mHeapSize;
    heapDesc.Properties = CD3DX12_HEAP_PROPERTIES(mType);
    heapDesc.Alignment = 0;
    heapDesc.Flags = mFlag;

    mDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(mHeaps.back().GetAddressOf()));
}

Carol::CircularHeap::CircularHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, bool isConstant, uint32_t elementCount, uint32_t elementSize)
    :Heap(device,D3D12_HEAP_TYPE_UPLOAD,D3D12_HEAP_FLAG_NONE),mCommandList(cmdList),mElementCount(elementCount),mElementSize(elementSize),mBufferSize(elementSize)
{
    if (isConstant)
    {
        Align();
    }

    ExpandHeap();
}

Carol::CircularHeap::~CircularHeap()
{
    for (int i = 0; i < mHeaps.size(); ++i)
    {
        mHeaps[i]->Unmap(0, nullptr);
        mMappedData[i] = nullptr;
    }
}

void Carol::CircularHeap::CreateResource(ComPtr<ID3D12Resource>* resource, D3D12_RESOURCE_DESC* desc, HeapAllocInfo* info, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
{
    Allocate(1, info);
}

void Carol::CircularHeap::DeleteResource(HeapAllocInfo* info)
{
    Deallocate(info);
}

void Carol::CircularHeap::CopyData(HeapAllocInfo* info, const void* data)
{
    memcpy(&mMappedData[info->Addr / mHeapSize][info->Addr % mHeapSize], data, mBufferSize);
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::CircularHeap::GetGPUVirtualAddress(HeapAllocInfo* info)
{
    return mHeaps[info->Addr / mHeapSize]->GetGPUVirtualAddress() + (info->Addr % mHeapSize);
}

bool Carol::CircularHeap::Allocate(uint32_t size, HeapAllocInfo* info)
{
    if (mQueueSize > 0 && mBegin == mEnd)
    {
        ExpandHeap();
    }

    info->Heap = this;
    info->Bytes = mElementSize;
    info->Addr = mEnd * mElementSize;

    mEnd = (mEnd + 1) % mElementCount;  
    ++mQueueSize;
    return true;
}

bool Carol::CircularHeap::Deallocate(HeapAllocInfo* info)
{
    if (mQueueSize == 0)
    {
        return false;
    }

    info->Bytes = 0;
    info->Addr = 0;

    mBegin = (mBegin + 1) % mElementCount;
    --mQueueSize;
    return true;
}

void Carol::CircularHeap::Align()
{
    mElementSize = (~255) & (mElementSize + 255);
}

void Carol::CircularHeap::ExpandHeap()
{
    ComPtr<ID3D12Resource> expiringHeap;

    if (!mHeapSize)
    {
        mHeapSize = mElementCount * mElementSize;
    }
    else
    {
        mElementCount <<= 1;
    }

    mHeaps.emplace_back();
    mMappedData.emplace_back();

    ThrowIfFailed(mDevice->CreateCommittedResource(
        GetRvaluePtr(D3D12_HEAP_PROPERTIES(CD3DX12_HEAP_PROPERTIES(mType))),
        mFlag,
        GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(mHeapSize)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(mHeaps.back().GetAddressOf())
    ));

    ThrowIfFailed(mHeaps.back()->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData.back())));

    if (mHeaps.size() > 1)
    {
        mEnd = (mElementCount >> 1) + 1;
        mQueueSize = mEnd - mBegin;
    }
    else
    {
        mBegin = 0;
        mEnd = 0;
    }
}

Carol::SegListHeap::SegListHeap(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag, uint32_t maxPageSize)
    :Heap(device, type, flag),mOrder(GetOrder(maxPageSize))
{
    mSegLists.resize(mOrder + 1);
    mBitsets.resize(mOrder + 1);

    for (int i = 0; i <= mOrder; ++i)
    {
        AddHeap(i);
    }
}

void Carol::SegListHeap::CreateResource(ComPtr<ID3D12Resource>* resource, D3D12_RESOURCE_DESC* desc, HeapAllocInfo* info, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
{
    Allocate(mDevice->GetResourceAllocationInfo(0, 1, desc).SizeInBytes, info);
    
    auto order = GetOrder(info->Bytes);
    auto orderNumPages = 1 << (mOrder - order);
    auto heapIdx = info->Addr / (orderNumPages * (mMinPageSize << order));

    ThrowIfFailed(mDevice->CreatePlacedResource(
        mSegLists[order][heapIdx].Get(),
        info->Addr % (orderNumPages * (mMinPageSize << order)),
        desc,
        initState,
        optimizedClearValue,
        IID_PPV_ARGS(resource->GetAddressOf())
    ));
}

void Carol::SegListHeap::DeleteResource(HeapAllocInfo* info)
{
    Deallocate(info);
}

bool Carol::SegListHeap::Allocate(uint32_t size, HeapAllocInfo* info)
{
    if (size <= 0 || size > mMinPageSize << mOrder)
    {
        return false;
    }

    auto order = GetOrder(size);
    auto orderNumPages = 1 << (mOrder - order);

    for (uint32_t i = 0; i < mBitsets[order].size(); ++i)
    {
        for (uint32_t j = 0; j < orderNumPages; ++j)
        {
            if (mBitsets[order][i]->IsPageIdle(j))
            {
                info->Heap = this;
                info->Addr = (i * orderNumPages + j) * (mMinPageSize << order);
                info->Bytes = mMinPageSize << order;
                mBitsets[order][i]->Set(j);

                return true;
            }
        }
    }

    AddHeap(order);
    info->Heap = this;
    info->Addr = (mSegLists[order].size() - 1) * orderNumPages * (mMinPageSize << order);
    info->Bytes = mMinPageSize << order;
    mBitsets[order].back()->Set(0);

    return true;
}

bool Carol::SegListHeap::Deallocate(HeapAllocInfo* info)
{
    if (info->Bytes <= 0 || info->Bytes >= 1 << mOrder)
    {
        return false;
    }

    auto order = GetOrder(info->Bytes);
    auto orderNumPages = 1 << (mOrder - order);

    auto heapIdx = info->Addr / (orderNumPages * (1 << order));
    auto pageIdx = ((info->Addr) % (orderNumPages * (1 << order))) / (1 << order);
    mBitsets[order][heapIdx]->Reset(pageIdx);

    return true;
}

uint32_t Carol::SegListHeap::GetOrder(uint32_t size)
{
    size = std::ceil(size * 1.0f / mMinPageSize);
	return std::ceil(std::log2(size));
}

void Carol::SegListHeap::AddHeap(uint32_t order)
{
    mSegLists[order].emplace_back();
    mBitsets[order].emplace_back(make_unique<Bitset>(1 << (mOrder - order)));

    D3D12_HEAP_DESC desc = {};
    desc.SizeInBytes = (1 << (mOrder - order)) * (mMinPageSize << order);
    desc.Properties = CD3DX12_HEAP_PROPERTIES(mType);
    desc.Alignment = 0;
    desc.Flags = mFlag;

    ThrowIfFailed(mDevice->CreateHeap(
        &desc,
        IID_PPV_ARGS(mSegLists[order].back().GetAddressOf())
    ));
}
