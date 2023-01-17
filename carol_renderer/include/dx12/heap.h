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
		Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
		byte* MappedData = nullptr;

		Heap* Heap;
		uint64_t Bytes = 0;
		uint64_t Addr = 0;
	};

	class Heap
	{
	public:
		Heap(D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag);

		virtual std::unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC* desc) = 0;
		virtual void Deallocate(std::unique_ptr<HeapAllocInfo> info);

		virtual void DeleteResourceImmediate(std::unique_ptr<HeapAllocInfo> info);
		virtual void DelayedDelete();

		virtual ID3D12Heap* GetHeap(const HeapAllocInfo* info)const = 0;
		virtual uint32_t GetOffset(const HeapAllocInfo* info)const = 0;

	protected:
		virtual void Delete(std::unique_ptr<HeapAllocInfo> info) = 0;

		D3D12_HEAP_TYPE mType;
		D3D12_HEAP_FLAGS mFlag;

		std::vector<std::vector<std::unique_ptr<HeapAllocInfo>>> mDeletedResources;
	};

	class BuddyHeap : public Heap
	{
	public:
		BuddyHeap(
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t heapSize = 1 << 26);

		virtual std::unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC* desc)override;
		virtual ID3D12Heap* GetHeap(const HeapAllocInfo* info)const override;
		virtual uint32_t GetOffset(const HeapAllocInfo* info)const override;

	protected:
		virtual void Delete(std::unique_ptr<HeapAllocInfo> info);

		void Align();
		void AddHeap();

		std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>> mHeaps;
		std::vector<std::unique_ptr<Buddy>> mBuddies;

		uint32_t mHeapSize;
		uint32_t mPageSize = 65536;
		uint32_t mNumPages;
	};

	class SegListHeap : public Heap
	{
	public:
		SegListHeap(
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t maxPageSize = 1 << 26);

		virtual std::unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC* desc)override;
		virtual ID3D12Heap* GetHeap(const HeapAllocInfo* info)const override;
		virtual uint32_t GetOffset(const HeapAllocInfo* info)const override;

	protected:
		virtual void Delete(std::unique_ptr<HeapAllocInfo> info);

		uint32_t GetOrder(uint32_t size)const;
		void AddHeap(uint32_t order);

		std::vector<std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>>> mSegLists;
		std::vector<std::vector<std::unique_ptr<Bitset>>> mBitsets;

		uint32_t mMaxNumPages;
		uint32_t mPageSize = 65536;
		uint32_t mOrder;
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

		void DelayedDelete();

	protected:
		std::unique_ptr<Heap> mDefaultBuffersHeap;
		std::unique_ptr<Heap> mUploadBuffersHeap;
		std::unique_ptr<Heap> mReadbackBuffersHeap;
		std::unique_ptr<Heap> mTexturesHeap;
	};
}

