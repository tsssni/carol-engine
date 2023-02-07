export module carol.renderer.dx12.heap;
import carol.renderer.dx12.command;
import carol.renderer.utils;
import <wrl/client.h>;
import <vector>;
import <memory>;
import <cmath>;

namespace Carol
{
	using Microsoft::WRL::ComPtr;
	using std::make_unique;
	using std::unique_ptr;
	using std::vector;

	class Heap;

	export class HeapAllocInfo
	{
	public:
		ComPtr<ID3D12Resource> Resource;
		byte *MappedData = nullptr;

		Heap *Heap = nullptr;
		uint64_t Bytes = 0;
		uint64_t Addr = 0;
	};

	export class Heap
	{
	public:
		Heap(D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag)
			: mType(type), mFlag(flag), mDeletedResources(gNumFrame)
		{
		}

		virtual unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC *desc) = 0;

		virtual void Deallocate(unique_ptr<HeapAllocInfo> info)
		{
			mDeletedResources[gCurrFrame].push_back(std::move(info));
		}

		virtual void DeleteResourceImmediate(unique_ptr<HeapAllocInfo> info)
		{
			Delete(std::move(info));
		}

		virtual void DelayedDelete()
		{
			for (auto &info : mDeletedResources[gCurrFrame])
			{
				if (info->MappedData)
				{
					info->Resource->Unmap(0, nullptr);
					info->MappedData = nullptr;
				}

				Delete(std::move(info));
			}

			mDeletedResources[gCurrFrame].clear();
		}

		virtual ID3D12Heap *GetHeap(const HeapAllocInfo *info) const = 0;

		virtual uint32_t GetOffset(const HeapAllocInfo *info) const = 0;

	protected:
		virtual void Delete(unique_ptr<HeapAllocInfo> info) = 0;

		D3D12_HEAP_TYPE mType;
		D3D12_HEAP_FLAGS mFlag;

		vector<vector<unique_ptr<HeapAllocInfo>>> mDeletedResources;
	};

	export class BuddyHeap : public Heap
	{
	public:
		BuddyHeap(
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t heapSize = 1 << 26)
			: Heap(type, flag), mHeapSize(heapSize)
		{
			Align();
			AddHeap();
		}

		virtual unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC *desc) override
		{
			uint32_t size = gDevice->GetResourceAllocationInfo(0, 1, desc).SizeInBytes;

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

			AddHeap();

			if (mBuddies.back()->Allocate(size, buddyInfo))
			{
				heapInfo = make_unique<HeapAllocInfo>();
				heapInfo->Heap = this;
				heapInfo->Bytes = buddyInfo.NumPages * mPageSize;
				heapInfo->Addr = (mBuddies.size() - 1) * mHeapSize + buddyInfo.PageId * mPageSize;
			}

			return heapInfo;
		}

		virtual ID3D12Heap *GetHeap(const HeapAllocInfo *info) const override
		{
			return mHeaps[info->Addr / mHeapSize].Get();
		}

		virtual uint32_t GetOffset(const HeapAllocInfo *info) const override
		{
			return info->Addr % mHeapSize;
		}

	protected:
		virtual void Delete(unique_ptr<HeapAllocInfo> info)
		{
			uint32_t buddyIdx = info->Addr / mHeapSize;
			uint32_t blockIdx = (info->Addr % mHeapSize) / mPageSize;
			uint32_t numBlocks = info->Bytes / mPageSize;
			BuddyAllocInfo buddyInfo(blockIdx, numBlocks);

			mBuddies[buddyIdx]->Deallocate(buddyInfo);
		}

		void Align()
		{
			mPageSize = (~65535) & (mPageSize + 65535);

			if (mHeapSize % mPageSize)
			{
				mHeapSize = (mHeapSize / mPageSize + 1) * mPageSize;
			}

			mNumPages = mHeapSize / mPageSize;
		}

		void AddHeap()
		{
			mHeaps.emplace_back();
			mBuddies.emplace_back(make_unique<Buddy>(mHeapSize, mPageSize));

			D3D12_HEAP_DESC heapDesc = {};
			heapDesc.SizeInBytes = mHeapSize;
			heapDesc.Properties = CD3DX12_HEAP_PROPERTIES(mType);
			heapDesc.Alignment = 0;
			heapDesc.Flags = mFlag;

			gDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(mHeaps.back().GetAddressOf()));
		}

		vector<ComPtr<ID3D12Heap>> mHeaps;
		vector<unique_ptr<Buddy>> mBuddies;

		uint32_t mHeapSize = 0;
		uint32_t mPageSize = 65536;
		uint32_t mNumPages = 0;
	};

	export class SegListHeap : public Heap
	{
	public:
		SegListHeap(
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t maxPageSize = 1 << 26)
			: Heap(type, flag), mOrder(GetOrder(maxPageSize))
		{
			mSegLists.resize(mOrder + 1);
			mBitsets.resize(mOrder + 1);

			for (int i = 0; i <= mOrder; ++i)
			{
				AddHeap(i);
			}
		}

		virtual unique_ptr<HeapAllocInfo> Allocate(const D3D12_RESOURCE_DESC *desc) override
		{
			uint32_t size = gDevice->GetResourceAllocationInfo(0, 1, desc).SizeInBytes;
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

			AddHeap(order);
			heapInfo = make_unique<HeapAllocInfo>();
			heapInfo->Heap = this;
			heapInfo->Addr = (mSegLists[order].size() - 1) * orderNumPages * (mPageSize << order);
			heapInfo->Bytes = mPageSize << order;
			mBitsets[order].back()->Set(0);

			return heapInfo;
		}

		virtual ID3D12Heap *GetHeap(const HeapAllocInfo *info) const override
		{
			uint32_t order = GetOrder(info->Bytes);
			uint32_t orderNumPages = 1 << (mOrder - order);
			uint32_t heapIdx = info->Addr / (orderNumPages * (mPageSize << order));

			return mSegLists[order][heapIdx].Get();
		}

		virtual uint32_t GetOffset(const HeapAllocInfo *info) const override
		{
			uint32_t order = GetOrder(info->Bytes);
			uint32_t orderNumPages = 1 << (mOrder - order);

			return info->Addr % (orderNumPages * (mPageSize << order));
		}

	protected:
		virtual void Delete(unique_ptr<HeapAllocInfo> info)
		{
			auto order = GetOrder(info->Bytes);
			auto orderNumPages = 1 << (mOrder - order);

			auto heapIdx = info->Addr / (orderNumPages * mPageSize * (1u << order));
			auto pageIdx = ((info->Addr) % (orderNumPages * mPageSize * (1u << order))) / (1u << order);
			mBitsets[order][heapIdx]->Reset(pageIdx);
		}

		uint32_t GetOrder(uint32_t size) const
		{
			size = ceil(size * 1.0f / mPageSize);
			return ceil(log2(size));
		}

		void AddHeap(uint32_t order)
		{
			mSegLists[order].emplace_back();
			mBitsets[order].emplace_back(make_unique<Bitset>(1 << (mOrder - order)));

			D3D12_HEAP_DESC desc = {};
			desc.SizeInBytes = (1 << (mOrder - order)) * (mPageSize << order);
			desc.Properties = CD3DX12_HEAP_PROPERTIES(mType);
			desc.Alignment = 0;
			desc.Flags = mFlag;

			ThrowIfFailed(gDevice->CreateHeap(
				&desc,
				IID_PPV_ARGS(mSegLists[order].back().GetAddressOf())));
		}

		vector<vector<ComPtr<ID3D12Heap>>> mSegLists;
		vector<vector<unique_ptr<Bitset>>> mBitsets;

		uint32_t mMaxNumPages = 0;
		uint32_t mPageSize = 65536;
		uint32_t mOrder = 0;
	};

	export class HeapManager
	{
	public:
		HeapManager(
			uint32_t initDefaultBuffersHeapSize = 1 << 26,
			uint32_t initUploadBuffersHeapSize = 1 << 26,
			uint32_t initReadbackBuffersHeapSize = 1 << 26,
			uint32_t texturesMaxPageSize = 1 << 26)
		{
			mDefaultBuffersHeap = make_unique<BuddyHeap>(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, initDefaultBuffersHeapSize);
			mUploadBuffersHeap = make_unique<BuddyHeap>(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, initDefaultBuffersHeapSize);
			mReadbackBuffersHeap = make_unique<BuddyHeap>(D3D12_HEAP_TYPE_READBACK, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, initDefaultBuffersHeapSize);
			mTexturesHeap = make_unique<SegListHeap>(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, texturesMaxPageSize);
		}

		Heap *GetDefaultBuffersHeap()
		{
			return mDefaultBuffersHeap.get();
		}

		Heap *GetUploadBuffersHeap()
		{
			return mUploadBuffersHeap.get();
		}

		Heap *GetReadbackBuffersHeap()
		{
			return mReadbackBuffersHeap.get();
		}

		Heap *GetTexturesHeap()
		{
			return mTexturesHeap.get();
		}

		void DelayedDelete()
		{
			mDefaultBuffersHeap->DelayedDelete();
			mUploadBuffersHeap->DelayedDelete();
			mReadbackBuffersHeap->DelayedDelete();
			mTexturesHeap->DelayedDelete();
		}

	protected:
		unique_ptr<Heap> mDefaultBuffersHeap;
		unique_ptr<Heap> mUploadBuffersHeap;
		unique_ptr<Heap> mReadbackBuffersHeap;
		unique_ptr<Heap> mTexturesHeap;
	};

	export unique_ptr<HeapManager> gHeapManager;
}