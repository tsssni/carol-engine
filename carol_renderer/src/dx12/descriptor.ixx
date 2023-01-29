export module carol.renderer.dx12.descriptor;
import carol.renderer.dx12.command;
import carol.renderer.utils;
import <wrl/client.h>;
import <vector>;
import <memory>;
import <unordered_map>;

namespace Carol
{
	using Microsoft::WRL::ComPtr;
	using std::make_unique;
	using std::unique_ptr;
	using std::unordered_map;
	using std::vector;

	class DescriptorAllocator;

	export class DescriptorAllocInfo
	{
	public:
		uint32_t StartOffset = 0;
		uint32_t NumDescriptors = 0;
	};

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator(
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			uint32_t initNumCpuDescriptors = 2048,
			uint32_t numGpuDescriptors = 65536)
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

		unique_ptr<DescriptorAllocInfo> CpuAllocate(uint32_t numDescriptors)
		{
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

			AddCpuDescriptorHeap();

			if (mCpuBuddies.back()->Allocate(numDescriptors, buddyInfo))
			{
				descInfo = make_unique<DescriptorAllocInfo>();
				descInfo->StartOffset = mNumCpuDescriptorsPerHeap * (mCpuBuddies.size() - 1) + buddyInfo.PageId;
				descInfo->NumDescriptors = numDescriptors;
			}

			return descInfo;
		}

		void CpuDeallocate(unique_ptr<DescriptorAllocInfo> info)
		{
			mCpuDeletedAllocInfo[gCurrFrame].emplace_back(std::move(info));
		}

		unique_ptr<DescriptorAllocInfo> GpuAllocate(uint32_t numDescriptors)
		{
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

		void GpuDeallocate(unique_ptr<DescriptorAllocInfo> info)
		{
			mGpuDeletedAllocInfo[gCurrFrame].emplace_back(std::move(info));
		}

		void DelayedDelete()
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

		ID3D12DescriptorHeap *GetGpuDescriptorHeap() const
		{
			return mGpuDescriptorHeap.Get();
		}

		uint32_t GetDescriptorSize() const
		{
			return mDescriptorSize;
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			uint32_t heapId = info->StartOffset / mNumCpuDescriptorsPerHeap;
			uint32_t startOffset = info->StartOffset % mNumCpuDescriptorsPerHeap;
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(mCpuDescriptorHeaps[heapId]->GetCPUDescriptorHandleForHeapStart(), startOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
		}

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			return CD3DX12_GPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
		}

	protected:
		void AddCpuDescriptorHeap()
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

		void InitGpuDescriptorHeap()
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

		void CpuDelete(unique_ptr<DescriptorAllocInfo> info)
		{
			if (info)
			{
				uint32_t buddyIdx = info->StartOffset / mNumCpuDescriptorsPerHeap;
				uint32_t blockIdx = info->StartOffset % mNumCpuDescriptorsPerHeap;
				BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

				mCpuBuddies[buddyIdx]->Deallocate(buddyInfo);
			}
		}

		void GpuDelete(unique_ptr<DescriptorAllocInfo> info)
		{
			if (info)
			{
				BuddyAllocInfo buddyInfo(info->StartOffset, info->NumDescriptors);
				mGpuBuddy->Deallocate(buddyInfo);
			}
		}

		vector<ComPtr<ID3D12DescriptorHeap>> mCpuDescriptorHeaps;
		vector<unique_ptr<Buddy>> mCpuBuddies;
		uint32_t mNumCpuDescriptorsPerHeap = 0;

		ComPtr<ID3D12DescriptorHeap> mGpuDescriptorHeap;
		unique_ptr<Buddy> mGpuBuddy;
		uint32_t mNumGpuDescriptors = 0;

		vector<vector<unique_ptr<DescriptorAllocInfo>>> mCpuDeletedAllocInfo;
		vector<vector<unique_ptr<DescriptorAllocInfo>>> mGpuDeletedAllocInfo;

		D3D12_DESCRIPTOR_HEAP_TYPE mType;
		uint32_t mDescriptorSize = 0;
	};

	export class DescriptorManager
	{
	public:
		DescriptorManager(
			uint32_t initCpuCbvSrvUavHeapSize = 2048,
			uint32_t initGpuCbvSrvUavHeapSize = 65536,
			uint32_t initRtvHeapSize = 2048,
			uint32_t initDsvHeapSize = 2048)
		{
			mCbvSrvUavAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initCpuCbvSrvUavHeapSize, initGpuCbvSrvUavHeapSize);
			mRtvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initRtvHeapSize, 0);
			mDsvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initDsvHeapSize, 0);
		}

		DescriptorManager(DescriptorManager &&manager)
		{
			mCbvSrvUavAllocator = std::move(manager.mCbvSrvUavAllocator);
			mRtvAllocator = std::move(manager.mRtvAllocator);
			mDsvAllocator = std::move(manager.mDsvAllocator);
		}

		DescriptorManager(const DescriptorManager &) = delete;

		DescriptorManager &operator=(const DescriptorManager &) = delete;

		unique_ptr<DescriptorAllocInfo> CpuCbvSrvUavAllocate(uint32_t numDescriptors)
		{
			return mCbvSrvUavAllocator->CpuAllocate(numDescriptors);
		}

		unique_ptr<DescriptorAllocInfo> GpuCbvSrvUavAllocate(uint32_t numDescriptors)
		{
			return mCbvSrvUavAllocator->GpuAllocate(numDescriptors);
		}

		unique_ptr<DescriptorAllocInfo> RtvAllocate(uint32_t numDescriptors)
		{
			return mRtvAllocator->CpuAllocate(numDescriptors);
		}

		unique_ptr<DescriptorAllocInfo> DsvAllocate(uint32_t numDescriptors)
		{
			return mDsvAllocator->CpuAllocate(numDescriptors);
		}

		void CpuCbvSrvUavDeallocate(unique_ptr<DescriptorAllocInfo> info)
		{
			mCbvSrvUavAllocator->CpuDeallocate(std::move(info));
		}

		void GpuCbvSrvUavDeallocate(unique_ptr<DescriptorAllocInfo> info)
		{
			mCbvSrvUavAllocator->GpuDeallocate(std::move(info));
		}

		void RtvDeallocate(unique_ptr<DescriptorAllocInfo> info)
		{
			mRtvAllocator->CpuDeallocate(std::move(info));
		}

		void DsvDeallocate(unique_ptr<DescriptorAllocInfo> info)
		{
			mDsvAllocator->CpuDeallocate(std::move(info));
		}

		void DelayedDelete()
		{
			mCbvSrvUavAllocator->DelayedDelete();
			mRtvAllocator->DelayedDelete();
			mDsvAllocator->DelayedDelete();
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuCbvSrvUavHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			return mCbvSrvUavAllocator->GetCpuHandle(info, offset);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuCbvSrvUavHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			return mCbvSrvUavAllocator->GetShaderCpuHandle(info, offset);
		}

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuCbvSrvUavHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			return mCbvSrvUavAllocator->GetGpuHandle(info, offset);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			return mRtvAllocator->GetCpuHandle(info, offset);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(const DescriptorAllocInfo *info, uint32_t offset = 0) const
		{
			return mDsvAllocator->GetCpuHandle(info, offset);
		}

		uint32_t GetCbvSrvUavSize() const
		{
			return mCbvSrvUavAllocator->GetDescriptorSize();
		}

		uint32_t GetRtvSize() const
		{
			return mRtvAllocator->GetDescriptorSize();
		}

		uint32_t GetDsvSize() const
		{
			return mDsvAllocator->GetDescriptorSize();
		}

		ID3D12DescriptorHeap *GetResourceDescriptorHeap() const
		{
			return mCbvSrvUavAllocator->GetGpuDescriptorHeap();
		}

	protected:
		unique_ptr<DescriptorAllocator> mCbvSrvUavAllocator;
		unique_ptr<DescriptorAllocator> mRtvAllocator;
		unique_ptr<DescriptorAllocator> mDsvAllocator;
	};

	export unique_ptr<DescriptorManager> gDescriptorManager;
}