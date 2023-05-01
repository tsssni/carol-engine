#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>

namespace Carol
{
	class Heap;
	class Bitset;
	class Buddy;

	class HeapAllocInfo
	{
	public:
		Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		byte* MappedData = nullptr;

		Heap* Heap = nullptr;
		uint64_t Bytes = 0;
		uint64_t Addr = 0;
	};

	class Heap
	{
	public:
		Heap(D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag);

		virtual std::unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC* desc) = 0;
		virtual void Deallocate(HeapAllocInfo* info);
		virtual void DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue);

		virtual ID3D12Heap* GetHeap(const HeapAllocInfo* info)const = 0;
		virtual uint32_t GetOffset(const HeapAllocInfo* info)const = 0;

	protected:
		virtual void Delete(const HeapAllocInfo* info) = 0;

		D3D12_HEAP_TYPE mType;
		D3D12_HEAP_FLAGS mFlag;

		std::vector<std::unique_ptr<HeapAllocInfo>> mDeletedResources;
		std::queue<std::pair<uint64_t, std::vector<std::unique_ptr<HeapAllocInfo>>>> mDeletedResourcesQueue;
		
		std::mutex mAllocatorMutex;
	};

	class BuddyHeap : public Heap
	{
	public:
		BuddyHeap(
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t heapSize = 1 << 26);
		~BuddyHeap();

		virtual std::unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC* desc)override;
		virtual ID3D12Heap* GetHeap(const HeapAllocInfo* info)const override;
		virtual uint32_t GetOffset(const HeapAllocInfo* info)const override;

	protected:
		virtual void Delete(const HeapAllocInfo* info)override;

		void Align();
		void AddHeap();

		std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>> mHeaps;
		std::vector<std::unique_ptr<Buddy>> mBuddies;

		uint32_t mHeapSize = 0;
		uint32_t mPageSize = 65536;
		uint32_t mNumPages = 0;
	};

	class SegListHeap : public Heap
	{
	public:
		SegListHeap(
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t maxPageSize = 1 << 26);
		~SegListHeap();

		virtual std::unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC* desc)override;
		virtual ID3D12Heap* GetHeap(const HeapAllocInfo* info)const override;
		virtual uint32_t GetOffset(const HeapAllocInfo* info)const override;

	protected:
		virtual void Delete(const HeapAllocInfo* info)override;

		uint32_t GetOrder(uint32_t size)const;
		void AddHeap(uint32_t order);

		std::vector<std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>>> mSegLists;
		std::vector<std::vector<std::unique_ptr<Bitset>>> mBitsets;

		uint32_t mMaxNumPages = 0;
		uint32_t mPageSize = 65536;
		uint32_t mOrder = 0;
	};

	class HeapManager
	{
	public:
		HeapManager(
			uint32_t initDefaultBuffersHeapSize = 1 << 26,
			uint32_t initUploadBuffersHeapSize = 1 << 26,
			uint32_t initReadbackBuffersHeapSize = 1 << 26,
			uint32_t texturesMaxPageSize = 1 << 26);
		
		Heap* GetDefaultBuffersHeap();
		Heap* GetUploadBuffersHeap();
		Heap* GetReadbackBuffersHeap();
		Heap* GetTexturesHeap();

		void DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue);

	protected:
		std::unique_ptr<Heap> mDefaultBuffersHeap;
		std::unique_ptr<Heap> mUploadBuffersHeap;
		std::unique_ptr<Heap> mReadbackBuffersHeap;
		std::unique_ptr<Heap> mTexturesHeap;
	};
}

