#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

namespace Carol
{
	class Heap;
	class Bitset;
	class Buddy;

	class HeapAllocInfo
	{
	public:
		Heap* Heap;
		uint64_t Bytes = 0;
		uint64_t Addr = 0;
	};

	class Heap
	{
	public:
		Heap(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag);
		virtual void CreateResource(Microsoft::WRL::ComPtr<ID3D12Resource>* resource, D3D12_RESOURCE_DESC* desc, HeapAllocInfo* info, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue = nullptr) = 0;
		virtual void SetCurrFrame(uint32_t currFrame);
		virtual void DeleteResource(HeapAllocInfo* info);
		virtual void DeleteResourceImmediate(HeapAllocInfo* info);
		virtual void DelayedDelete();

	protected:
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info) = 0;
		virtual bool Deallocate(HeapAllocInfo* info) = 0;

		ID3D12Device* mDevice;
		D3D12_HEAP_TYPE mType;
		D3D12_HEAP_FLAGS mFlag;

		uint32_t mCurrFrame;
		std::vector<std::vector<HeapAllocInfo*>> mDeletedResources;
	};

	class BuddyHeap : public Heap
	{
	public:
		BuddyHeap(
			ID3D12Device* device,
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t heapSize = 1 << 26,
			uint32_t pageSize = 1 << 16);

		virtual void CreateResource(
			Microsoft::WRL::ComPtr<ID3D12Resource>* resource,
			D3D12_RESOURCE_DESC* desc,
			HeapAllocInfo* info,
			D3D12_RESOURCE_STATES initState,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr)override;

	protected:
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info)override;
		virtual bool Deallocate(HeapAllocInfo* info)override;

		void Align();
		void AddHeap();

		std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>> mHeaps;
		std::vector<std::unique_ptr<Buddy>> mBuddies;

		uint32_t mHeapSize;
		uint32_t mPageSize;
		uint32_t mNumPages;
	};

	class CircularHeap : public Heap
	{
	public:
		CircularHeap(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			bool isConstant,
			uint32_t elementCount = 32,
			uint32_t elementSize = 256);
		~CircularHeap();

		virtual void CreateResource(HeapAllocInfo* info);
		virtual void DeleteResource(HeapAllocInfo* info);

		void CopyData(HeapAllocInfo* info, const void* data);
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(HeapAllocInfo* info);

	protected:
		virtual void CreateResource(
			Microsoft::WRL::ComPtr<ID3D12Resource>* resource,
			D3D12_RESOURCE_DESC* desc,
			HeapAllocInfo* info,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr)override;
		
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info)override;
		virtual bool Deallocate(HeapAllocInfo* info)override;

		void Align();
		void ExpandHeap();

		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mHeaps;
		std::vector<byte*> mMappedData;

		ID3D12GraphicsCommandList* mCommandList;

		uint32_t mElementCount = 0;
		uint32_t mElementSize = 0;
		uint32_t mBufferSize = 0;
		
		uint32_t mHeapSize = 0;

		uint32_t mBegin;
		uint32_t mEnd;
		uint32_t mQueueSize = 0;
	};

	class SegListHeap : public Heap
	{
	public:
		SegListHeap(
			ID3D12Device* device,
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t maxPageSize = 1 << 26);
		
		virtual void CreateResource(
			Microsoft::WRL::ComPtr<ID3D12Resource>* resource, 
			D3D12_RESOURCE_DESC* desc, HeapAllocInfo* info, 
			D3D12_RESOURCE_STATES initState, 
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr)override;

	protected:
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info);
		virtual bool Deallocate(HeapAllocInfo* info);

		uint32_t GetOrder(uint32_t size);
		void AddHeap(uint32_t order);

		std::vector<std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>>> mSegLists;
		std::vector<std::vector<std::unique_ptr<Bitset>>> mBitsets;

		uint32_t mMaxNumPages;
		uint32_t mPageSize = 65536;
		uint32_t mOrder;
	};
}

