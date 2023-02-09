export module carol.renderer.dx12.descriptor;
import carol.renderer.dx12.command;
import carol.renderer.utils;
import <wrl/client.h>;
import <vector>;
import <queue>;
import <memory>;
import <unordered_map>;
import <mutex>;

namespace Carol
{
	using Microsoft::WRL::ComPtr;
	using std::lock_guard;
	using std::make_pair;
	using std::make_unique;
	using std::mutex;
	using std::pair;
	using std::queue;
	using std::unique_ptr;
	using std::unordered_map;
	using std::vector;

	class DescriptorManager;

	export class DescriptorAllocInfo
	{
	public:
		DescriptorManager *Manager = nullptr;
		uint32_t StartOffset = 0;
		uint32_t NumDescriptors = 0;
	};

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator(
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			ID3D12Device* device,
			uint32_t initNumCpuDescriptors = 2048,
			uint32_t numGpuDescriptors = 65536)
			:mType(type),
			mNumCpuDescriptorsPerHeap(initNumCpuDescriptors),
			mNumGpuDescriptors(numGpuDescriptors)
		{
			mDescriptorSize = device->GetDescriptorHandleIncrementSize(type);
			AddCpuDescriptorHeap(device);
			InitGpuDescriptorHeap(device);
		}

		~DescriptorAllocator()
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

		unique_ptr<DescriptorAllocInfo> CpuAllocate(uint32_t numDescriptors)
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

			ComPtr<ID3D12Device> device;
			mCpuDescriptorHeaps.back()->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));
			AddCpuDescriptorHeap(device.Get());

			if (mCpuBuddies.back()->Allocate(numDescriptors, buddyInfo))
			{
				descInfo = make_unique<DescriptorAllocInfo>();
				descInfo->StartOffset = mNumCpuDescriptorsPerHeap * (mCpuBuddies.size() - 1) + buddyInfo.PageId;
				descInfo->NumDescriptors = numDescriptors;
			}

			return descInfo;
		}

		void CpuDeallocate(DescriptorAllocInfo* info)
		{
			mCpuDeletedAllocInfo.emplace_back(info);
		}

		unique_ptr<DescriptorAllocInfo> GpuAllocate(uint32_t numDescriptors)
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

		void GpuDeallocate(DescriptorAllocInfo* info)
		{
			mGpuDeletedAllocInfo.emplace_back(info);
		}

		void DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue)
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

			while (!mGpuDeletedAllocInfo.empty() && mGpuDeletedAllocInfoQueue.front().first <= completedFenceValue)
			{
				for (auto& info : mGpuDeletedAllocInfoQueue.front().second)
				{
					GpuDelete(info.get());
				}

				mGpuDeletedAllocInfoQueue.pop();
			}
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
		void AddCpuDescriptorHeap(ID3D12Device* device)
		{
			mCpuDescriptorHeaps.emplace_back();
			mCpuBuddies.emplace_back(make_unique<Buddy>(mNumCpuDescriptorsPerHeap, 1u));

			D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc;
			descHeapDesc.Type = mType;
			descHeapDesc.NumDescriptors = mNumCpuDescriptorsPerHeap;
			descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			descHeapDesc.NodeMask = 0;

			ThrowIfFailed(device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mCpuDescriptorHeaps.back().GetAddressOf())));
		}

		void InitGpuDescriptorHeap(ID3D12Device* device)
		{
			if (mType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
			{
				mGpuBuddy = make_unique<Buddy>(mNumGpuDescriptors, 1u);

				D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc;
				descHeapDesc.Type = mType;
				descHeapDesc.NumDescriptors = mNumGpuDescriptors;
				descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				descHeapDesc.NodeMask = 0;

				ThrowIfFailed(device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mGpuDescriptorHeap.GetAddressOf())));
			}
		}

		void CpuDelete(const DescriptorAllocInfo* info)
		{
			lock_guard<mutex> lock(mCpuAllocatorMutex);

			uint32_t buddyIdx = info->StartOffset / mNumCpuDescriptorsPerHeap;
			uint32_t blockIdx = info->StartOffset % mNumCpuDescriptorsPerHeap;
			BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

			mCpuBuddies[buddyIdx]->Deallocate(buddyInfo);
		}

		void GpuDelete(const DescriptorAllocInfo* info)
		{
			lock_guard<mutex> lock(mGpuAllocatorMutex);

			BuddyAllocInfo buddyInfo(info->StartOffset, info->NumDescriptors);
			mGpuBuddy->Deallocate(buddyInfo);
		}

		vector<ComPtr<ID3D12DescriptorHeap>> mCpuDescriptorHeaps;
		vector<unique_ptr<Buddy>> mCpuBuddies;
		uint32_t mNumCpuDescriptorsPerHeap = 0;

		ComPtr<ID3D12DescriptorHeap> mGpuDescriptorHeap;
		unique_ptr<Buddy> mGpuBuddy;
		uint32_t mNumGpuDescriptors = 0;

		vector<unique_ptr<DescriptorAllocInfo>> mCpuDeletedAllocInfo;
		vector<unique_ptr<DescriptorAllocInfo>> mGpuDeletedAllocInfo;

		queue<pair<uint64_t, vector<unique_ptr<DescriptorAllocInfo>>>> mCpuDeletedAllocInfoQueue;
		queue<pair<uint64_t, vector<unique_ptr<DescriptorAllocInfo>>>> mGpuDeletedAllocInfoQueue;

		D3D12_DESCRIPTOR_HEAP_TYPE mType;
		uint32_t mDescriptorSize = 0;

		mutex mCpuAllocatorMutex;
		mutex mGpuAllocatorMutex;
	};

	export class DescriptorManager
	{
	public:
		DescriptorManager(
			ID3D12Device* device,
			uint32_t initCpuCbvSrvUavHeapSize = 2048,
			uint32_t initGpuCbvSrvUavHeapSize = 65536,
			uint32_t initRtvHeapSize = 2048,
			uint32_t initDsvHeapSize = 2048)
		{
			mCbvSrvUavAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, device, initCpuCbvSrvUavHeapSize, initGpuCbvSrvUavHeapSize);
			mRtvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, device, initRtvHeapSize, 0);
			mDsvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, device, initDsvHeapSize, 0);
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
			auto info = mCbvSrvUavAllocator->CpuAllocate(numDescriptors);
			info->Manager = this;

			return info;
		}

		unique_ptr<DescriptorAllocInfo> GpuCbvSrvUavAllocate(uint32_t numDescriptors)
		{
			auto info = mCbvSrvUavAllocator->GpuAllocate(numDescriptors);
			info->Manager = this;

			return info;
		}

		unique_ptr<DescriptorAllocInfo> RtvAllocate(uint32_t numDescriptors)
		{
			auto info = mRtvAllocator->CpuAllocate(numDescriptors);
			info->Manager = this;

			return info;
		}

		unique_ptr<DescriptorAllocInfo> DsvAllocate(uint32_t numDescriptors)
		{
			auto info = mDsvAllocator->CpuAllocate(numDescriptors);
			info->Manager = this;

			return info;
		}

		void CpuCbvSrvUavDeallocate(DescriptorAllocInfo* info)
		{
			if(info && info->Manager == this)
			{
				mCbvSrvUavAllocator->CpuDeallocate(info);
			}
		}

		void GpuCbvSrvUavDeallocate(DescriptorAllocInfo* info)
		{
			if(info && info->Manager == this)
			{
				mCbvSrvUavAllocator->GpuDeallocate(info);
			}
		}

		void RtvDeallocate(DescriptorAllocInfo* info)
		{
			if(info && info->Manager == this)
			{
				mRtvAllocator->CpuDeallocate(info);
			}
		}

		void DsvDeallocate(DescriptorAllocInfo* info)
		{
			if(info && info->Manager == this)
			{
				mDsvAllocator->CpuDeallocate(info);
			}
		}

		void DelayedDelete(uint64_t cpuFenceValue, uint64_t completedFenceValue)
		{
			mCbvSrvUavAllocator->DelayedDelete(cpuFenceValue, completedFenceValue);
			mRtvAllocator->DelayedDelete(cpuFenceValue, completedFenceValue);
			mDsvAllocator->DelayedDelete(cpuFenceValue, completedFenceValue);
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
}